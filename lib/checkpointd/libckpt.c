/* $Id: libckpt.c,v 1.5 2004/02/17 22:11:58 lars Exp $ */
/* 
 * ckptlib.c: data checkpoint API library
 *
 * Copyright (C) 2003 Jerry Yu <jerry.yu@intel.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* This library is an implementation of the Application Interface 
 * Specification on Service Availability Forum. Refer to: www.saforum.org/
 * specification
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>

#include <glib.h>

#include <saf/ais.h>
#include "checkpointd/clientrequest.h"

#ifndef AF_LOCAL
#	define AF_LOCAL	AF_UNIX
#endif

/* TODO list: 
 * 1. make all APIs thread safe
 */

/* -------------- data structure ----------------------------------- */
typedef struct {
	SaCkptHandleT initHandle; /* return by ckpt service */
	SaSelectionObjectT syncfd; /* used by synchronous api */
	SaSelectionObjectT asyncfd; /* used by asynchronous api */
	SaCkptCallbacksT callbacks;
} InitListDataT;

typedef struct {
	InitListDataT* pInit;
	SaCkptCheckpointHandleT checkpointHandle; /* return by ckpt service */
	SaNameT ckptName; /* checkpoint name */
} CkptListDataT;

typedef struct {
	InitListDataT* pInit;
	SaUint32T reqno;
	SaUint32T req;
	SaInvocationT invocation;
} ReqListDataT;

typedef struct {
	char *pData;
	SaUint32T bufsize;
	SaUint32T readlen;
} SaCkptMsgDataT;

#define TMPBUF_SIZE  256
const struct timeval TIMEOUT_VAL= {10,0}; /* i/o time out value: 10s */ 
SaUint32T g_reqNo = 0;
GList* g_initList = NULL;
GList* g_ckptList = NULL;
GList* g_reqList = NULL;
GList* g_secIteratorList = NULL;
char g_tmpbuf[TMPBUF_SIZE]; /* used to remove the data left by timeout p */


/* generate a request Num */
static SaUint32T
genReqNo(void)
{
	return g_reqNo++;
}

/* get init list data by init handle */
static InitListDataT*
getInitDataByHandle(const SaCkptHandleT *pHandle)
{
	GList* pList;
	InitListDataT* pInitData;
	
	pList = g_initList;
	while( NULL != pList ) {
		pInitData = (InitListDataT*)(pList->data);
		if( *pHandle == pInitData->initHandle ) {
			return pInitData;
		}
		pList = g_list_next(pList);
	}
	return NULL;
}

/* get checkpoint list data by checkpoint handle */
static CkptListDataT*
getCkptDataByHandle(const SaCkptCheckpointHandleT *pHandle)
{
	GList* pList;
	CkptListDataT* pCkptData;
	
	pList = g_ckptList;
	while( NULL != pList ) {
		pCkptData = (CkptListDataT*)(pList->data);
		if( *pHandle == pCkptData->checkpointHandle ) {
			return pCkptData;
		}
		pList = g_list_next(pList);
	}
	return NULL;
}

/* get asynchronous request list data by request no.*/
static ReqListDataT*
getReqDataByReqno(SaUint32T reqno)
{
	GList* pList;
	ReqListDataT* pReqData;

	pList = g_list_next(g_reqList);
	while( NULL != pList ) {
		pReqData = (ReqListDataT*)(pList->data);
		if(reqno == pReqData->reqno) {
			return pReqData;
		}
		pList = g_list_next(pList);
	}
	return NULL;
}

/* guarantee read the required data size */
static SaErrorT
reliableRead(SaInt32T fd, char* buffer, size_t size, \
		struct timeval *tv/*[in/out]*/)
{
	SaInt32T iret;
	fd_set rfds;
	size_t len = 0;
	SaErrorT retcode = SA_OK;

	/* set fd */
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	/* read data */
	while (len < size && retcode == SA_OK) {
		/* wait input data ready. it's not necessary to reset 
		 * the rfds since it still contains the fd when return
		 * on success
		 */
		iret = select(fd+1, &rfds, NULL, NULL, tv);
		if (0 == iret) {
			retcode = SA_ERR_TIMEOUT;
			/*
			perror("reliabeRead: select timeout");
			*/
			break;
		}
		if (iret < 0) {
			perror("reliabeRead: select");
			retcode = SA_ERR_LIBRARY;
			break;
		}

		/* receive data */
		iret = recv(fd, (buffer+len), (size-len), 0);
		if (iret == 0) {
			/* We don't think this should happen */
			retcode = SA_ERR_LIBRARY;
			break;
		}
		
		if (iret < 0) {
			/* this should not happen */
			perror("reliabeRead: recv");
			retcode = SA_ERR_LIBRARY;
			break;
    		}
		
		len = len + iret;
		if (len > size) {
			/* should not happen */
			retcode = SA_ERR_LIBRARY;
		}
	} /* end while (len < size ...*/

	return retcode;
}

/* send the required data size */
static SaErrorT
reliableWrite(SaInt32T fd, const char* buffer, size_t size, \
		struct timeval *tv /*[in/out]*/)
{
	SaInt32T iret;
	size_t len = 0;
	SaErrorT retcode = SA_OK;
	fd_set wfds;

	/* set fd */
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);

	while (len < size && retcode == SA_OK) {
		/* wait write buffer ready */
		iret = select(fd+1, NULL, &wfds, NULL, tv);
		if (0 == iret) {
			retcode = SA_ERR_TIMEOUT;
			break;
		}
		if (iret < 0) {
			perror("reliabeWrite: select");
			retcode = SA_ERR_LIBRARY;
			break;
		}
		iret = send(fd, (buffer+len), (size-len), 0);
		if (iret == 0) {
			/* We don't think this should happen */
			retcode = SA_ERR_LIBRARY;
		}
		if (iret < 0) {
			perror("reliabeWrite: send");
			retcode = SA_ERR_LIBRARY;
			break;
		}	
		len = len + iret;
	} /* end while */

	return retcode;
}

/* remove the data with this response head from the input buffer 
 */
static SaErrorT
removeData(SaInt32T fd, SaCkptResponseHeadT *pResphead,\
		struct timeval *tv /*[in/out]*/)
{
	int len;
	SaErrorT retcode = SA_OK;

	len = pResphead->dataLen;

	while (len > 0) {
		if (len <= TMPBUF_SIZE) {
			retcode = reliableRead(fd, g_tmpbuf, len, tv);
			if (SA_OK != retcode) {
				perror("removeData: reliabeRead-1");
			}
			break;
		}
		retcode = reliableRead(fd, g_tmpbuf, TMPBUF_SIZE, tv);
		if (SA_OK != retcode) {
			perror("removeData: reliabeRead-2");
			break;
		}
		len = len - TMPBUF_SIZE;
	}/* end while */

	return retcode;
}

/* read the response head with this request number
 */
static SaErrorT
readRespheadByReqno(SaInt32T fd, SaCkptResponseHeadT *pResphead /*[out]*/,\
		SaUint32T reqno, struct timeval *tv /*[in/out]*/)
{
	SaErrorT retcode;

	while (1) {
		retcode = reliableRead(fd, (char*)pResphead, \
			sizeof(SaCkptResponseHeadT), tv);
		if (SA_OK != retcode) {
			perror("readMsgByReqno: reliableRead-1");
			break;
		}
		
#if 0		
		printf("Read response : ");
		printf("msglen %lu, initHandle %d, reqno %lu, retval %d, \
			dataLen %lu\n", 
			pResphead->msglen, 
			pResphead->initHandle,
			pResphead->reqno,
			pResphead->retval,
			pResphead->dataLen);
#endif

		if (reqno == pResphead->reqno) {
			break;
		}
		
		printf("readMsgByReqno: request number error\n");
		
		/* remove the data that is not what we expected */
		retcode = removeData(fd, pResphead, tv);
		if (SA_OK != retcode) {
			break;
		}
	}
	return retcode;
}

/* invoke callback according to the pending request data
 */
static SaErrorT
doDispatch(SaInt32T fd, struct timeval *tv/*[in/out]*/)
{
	SaErrorT retcode = SA_OK;
	ReqListDataT* pReqData = NULL;
	SaCkptResponseHeadT resphead;
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptCallbacksT *callbacks = &(pReqData->pInit->callbacks);

	/* read response head */
	retcode = reliableRead(fd, (char*)&resphead, \
			sizeof(resphead), tv);
	if (SA_OK != retcode) {
		perror("saCkptDispatch: reliableRead-1");
		return retcode;
	}
	
	/* get the request data by reqno */
	pReqData = getReqDataByReqno(resphead.reqno);
	if (NULL == pReqData) {
		/* this response is not related to the pending requests. 
		 * should not happen. 
		 */
		retcode = removeData(fd, &resphead, tv);
		if (SA_OK != retcode) {
			/* */
		}
		/* !!: Here, we won't try to read next response
		 */
		return SA_ERR_LIBRARY;
	}
	
	/* invoke the callbacks */		
	switch (pReqData->req) {
	case REQ_CKPT_OPEN:
		if (SA_OK == resphead.retval) {
			retcode = reliableRead(fd, (char*)&checkpointHandle, \
				sizeof(checkpointHandle), tv);
			if (SA_OK != retcode) {
				perror("doDispatch: reliableRead failure");
				break;
			}

			/* if the callbask is not NULL, invoke the callback
			 */
			if (NULL == callbacks->saCkptCheckpointOpenCallback) {
				break;
			}
			callbacks->saCkptCheckpointOpenCallback(\
			pReqData->invocation, &checkpointHandle, SA_OK);
		}
		else {
			/* if the callbask is not NULL, invoke the callback
			 */
			if (NULL == callbacks->saCkptCheckpointOpenCallback) {
				break;
			}
			callbacks->saCkptCheckpointOpenCallback(\
				pReqData->invocation, NULL, resphead.retval);
		}
		break;
	case REQ_CKPT_SYNC:
		/* if the callbask is not NULL, invoke the callback
		 */
		if (NULL != callbacks->saCkptCheckpointSynchronizeCallback) {
			callbacks->saCkptCheckpointSynchronizeCallback(\
				pReqData->invocation, resphead.retval);
		}
		break;
	}
	
	/* remove the request from reqList and free the memory 
	 */
	g_reqList = g_list_remove(g_reqList, pReqData);
	free(pReqData);

	return retcode;
}

/* send request head and param, then wait until get the response.
 * Most requests are not required to send application data, "hello" fits all
 * these requests.
 */
static SaErrorT
hello(SaInt32T fd, SaCkptRequestHeadT *pReqHead, const void *pParam,\
	SaCkptResponseHeadT *pResphead /*[out]*/, \
	struct timeval *tv /*[in/out]*/)
{
	SaErrorT retcode;
	size_t len;
	
	/* send the request head */
	len = sizeof(SaCkptRequestHeadT);
	retcode = reliableWrite(fd, (char*)pReqHead, len, tv);
	if (SA_OK != retcode) {
		perror("hello: reliableWrite-1 failure");
		return retcode;
	}
	
	/* send the request param */
	if (0 != pReqHead->paramLen && NULL != pParam) {
		retcode = reliableWrite(fd, /*(char*)*/pParam, \
				pReqHead->paramLen, tv);
		if (SA_OK != retcode) {
			perror("hello: reliableWrite-2 failure");
			return retcode;
		}
	}
	
	/* get the response head */
	retcode = readRespheadByReqno(fd, pResphead, pReqHead->reqno, tv);
	return retcode;
}

/* ------------------------- exported API ------------------------------- */

/* Initialize the checkpoint service for the invoking process and register
 * the related callback functions. 
 */
SaErrorT
saCkptInitialize(
	SaCkptHandleT *ckptHandle/*[out]*/,
	const SaCkptCallbacksT *callbacks,
	const SaVersionT *version)
{	
	SaInt32T iret;
	SaErrorT retcode;
	size_t len;
	SaInt32T syncfd;
	SaInt32T asyncfd;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqInitParamT initParam;
	InitListDataT* pInitData;
	struct sockaddr_un srv_addr;
	struct timeval tv = TIMEOUT_VAL;
	
	/* check the parameters */
	if (NULL == ckptHandle || NULL == version) {
		return SA_ERR_INVALID_PARAM;
	}
	
	/* allocate memory for init data*/
	pInitData = (InitListDataT*)malloc(sizeof(InitListDataT));
	if ( NULL == pInitData ) {
		perror("saCkptInitialize: malloc() failure");
		return SA_ERR_NO_MEMORY;
	}
	
	/* prepare ckpt service address */
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sun_family = AF_LOCAL;
	strncpy(srv_addr.sun_path, CKPTIPC, sizeof(srv_addr.sun_path));
	
	/* prepare the sockets */
	syncfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (syncfd<0) {
		perror("saCkptInitialize: socket-1 failure");
		free(pInitData);
		return SA_ERR_LIBRARY;
	}		
	asyncfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (asyncfd<0) {
		perror("saCkptInitialize: socket-2 failure");
		free(pInitData);
		close(syncfd);
		return SA_ERR_LIBRARY;
	}
	
	/* first connect */
	iret = connect(syncfd, (struct sockaddr *)&srv_addr,
			sizeof(struct sockaddr_un));
	if (iret == -1) {
	    perror("saCkptInitialize: connect-1 failure");
		free(pInitData);
		close(syncfd);
		close(asyncfd);
    	return SA_ERR_LIBRARY;
  	}
  	
  	/* second connect */
	iret = connect(asyncfd, (struct sockaddr *)&srv_addr,
			sizeof(struct sockaddr_un));
	if (iret == -1) {
	    perror("saCkptInitialize: connect-2 failure");
		free(pInitData);
		close(syncfd);
		close(asyncfd);
    	return SA_ERR_LIBRARY;
  	}

	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqInitParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = 0;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_SERVICE_INIT;
	reqhead.paramLen = sizeof(SaCkptReqInitParamT);
	reqhead.dataLen = 0;

	/* prepare the request head */
	initParam.pid = getpid();
	initParam.tid = 0;
	memcpy( &(initParam.ver), version, sizeof(SaVersionT));
	
	/* hello to ckpt service */
	retcode = hello(syncfd, &reqhead, &initParam, &resphead, &tv);
	if ( SA_OK != retcode || SA_OK != resphead.retval){
		close(syncfd);
		close(asyncfd);
		free(pInitData);
		if (SA_OK != retcode) {
			return retcode;
		}
		return resphead.retval;
	}
	
	/* The created service handle is returned in the response head 
	 * since it is the member of the response head.
	 * this should conform to the service daemon
	 */
	*ckptHandle = resphead.initHandle;
#if 1	
	/* send the second request through the second fd, to notify the service 
	 * that this two connects are combined
	 */
	reqhead.initHandle = *ckptHandle;
	reqhead.reqno = genReqNo();
	retcode = hello(asyncfd, &reqhead, &initParam, &resphead, &tv);
	if ( SA_OK != retcode || SA_OK != resphead.retval){
		close(syncfd);
		close(asyncfd);
		free(pInitData);
		if (SA_OK != retcode) {
			return retcode;
		}
		return resphead.retval;
	}
#endif
	/* add new item to service handle list */
	pInitData->initHandle = *ckptHandle;
	pInitData->syncfd = syncfd;
	pInitData->asyncfd = asyncfd;
	if (callbacks != NULL) {
		pInitData->callbacks = *callbacks;
	}
	else {
		memset(&(pInitData->callbacks), 0, sizeof(SaCkptCallbacksT));
	}
	g_initList = g_list_append(g_initList, pInitData);
	
	return SA_OK;
}

/* Returns the fd for asynchronously operation
 */
SaErrorT
saCkptSelectionObjectGet(
	const SaCkptHandleT *ckptHandle,
	SaSelectionObjectT *selectionObject/*[out]*/)
{
	InitListDataT* pInitData; 

	/* check the parameters */
	if (NULL == ckptHandle || NULL == selectionObject) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the handle is valid */
	pInitData = getInitDataByHandle(ckptHandle);
	if (NULL == pInitData) {
		return SA_ERR_BAD_HANDLE;
	}
	*selectionObject = pInitData->asyncfd;
	return SA_OK;
}

/* This function invokes, in the context of the calling thread,one or all 
 * of the pending callbacks for the handle ckptHandle.
 */
SaErrorT
saCkptDispatch(
	const SaCkptHandleT *ckptHandle,
	SaDispatchFlagsT dispatchFlags)
{
	SaErrorT retcode;
	InitListDataT* pInitData;
	SaInt32T sockfd;
	fd_set rfds;
	struct timeval tv = TIMEOUT_VAL;
	struct timeval zero_tv = {0, 0};

	/* check the init handle is valid */
	pInitData = getInitDataByHandle(ckptHandle);
	if (NULL == pInitData) {
		return SA_ERR_BAD_HANDLE;
	}

	/* check the dispatch flag */
	if ( !(SA_DISPATCH_ONE == dispatchFlags || \
		SA_DISPATCH_ALL == dispatchFlags || \
		SA_DISPATCH_BLOCKING == dispatchFlags)) {
		return SA_ERR_BAD_FLAGS;
	}
	sockfd = pInitData->asyncfd;

	/* dispatch one pending request */
	if (SA_DISPATCH_ONE == dispatchFlags) {
		retcode = doDispatch(sockfd, &tv);
		return retcode;
	}

	/* dispatch all pending request */
	if (SA_DISPATCH_ALL == dispatchFlags) {
		while (1) {
			int iret;
			FD_ZERO(&rfds);
			FD_SET(sockfd, &rfds);
			
			/* polling the fd first */
			iret = select(sockfd+1, &rfds, NULL, NULL, &zero_tv);
			if (0 == iret) {
				return SA_OK;
			}
			if (iret < 0) {
				/* ?*/
				return SA_OK;
			}
			retcode = doDispatch(sockfd, &tv);
			if( SA_OK != retcode) {
				return retcode;
			}
		}/* end while(1) */
	}

	/* dispatch as request pending until service finalizing */
	while (1) {
		int iret;
		FD_ZERO(&rfds);
		FD_SET(sockfd, &rfds);
		
		/* blocking until request pending */
		iret = select(sockfd+1, &rfds, NULL, NULL, NULL);
		if (iret <= 0) {
			/* check the init handle is still valid */
			pInitData = getInitDataByHandle(ckptHandle);
			if (NULL == pInitData) {
				return SA_OK;
			}
		}
		tv = TIMEOUT_VAL;
		retcode = doDispatch(sockfd, &tv);
		if( SA_OK != retcode) {
			perror("saCkptDispatch: doDispatch fail");
		}
	}/* end while(1) */
}

/* closes the association, represented by ckptHandle, between the process and
 * the Checkpoint Service. It frees up resources. If any checkpoint is still
 * open with this particular handle, the invocation of this function fails.
 */
SaErrorT
saCkptFinalize(const SaCkptHandleT *ckptHandle)
{
	SaErrorT retcode;
	size_t len;
	GList* pList;
	InitListDataT* pInitData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	struct timeval tv = TIMEOUT_VAL;

	/* check the handle is valid */
	pInitData = getInitDataByHandle(ckptHandle);
	if (NULL == pInitData) {
		return SA_ERR_BAD_HANDLE;
	}

	/* check whether any open ckeckpoints exists with this handle */
	pList = g_ckptList;
	while (NULL != pList) {
		if (pInitData == ((CkptListDataT*)(pList->data))->pInit) {
			return SA_ERR_BUSY;
		}
		pList = g_list_next(pList);
	}

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = *ckptHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_SERVICE_FINL;
	reqhead.paramLen = 0;
	reqhead.dataLen = 0;

	/* hello to ckpt service */
	/* !!even some error occured, still need to release the resource */
	retcode = hello(pInitData->syncfd, &reqhead, NULL, &resphead, &tv);
	if (SA_OK != retcode) {
		return retcode;
	}
	if (SA_OK != resphead.retval) {
		return resphead.retval;
	}

	/* close the fd */
	close(pInitData->syncfd);
	close(pInitData->asyncfd);

	/* remove the item from initList and free memory */
	g_initList = g_list_remove(g_initList, pInitData);
	free(pInitData);
	return SA_OK;
}

/* the invocation of this function is blocking. A new checkpoint handle is
 * returned upon completion.
 */
SaErrorT
saCkptCheckpointOpen(
	const SaCkptHandleT *ckptHandle,
	const SaNameT *checkpointName,
	const SaCkptCheckpointCreationAttributesT *checkpointCreationAttributes,
	SaCkptCheckpointOpenFlagsT checkpointOpenFlags,
	SaTimeT timeout,
	SaCkptCheckpointHandleT *checkpointHandle/*[out]*/)
{
	SaErrorT retcode;
	size_t len;
	InitListDataT* pInitData;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqOpenParamT openParam;
	long secs = timeout/1000000000L;
	long usecs = (timeout%1000000000L)*1000000L;
	struct timeval tv = {secs, usecs};

	/* check the parameters */
	if (NULL == ckptHandle || NULL == checkpointName \
			|| NULL == checkpointHandle) {
		return SA_ERR_INVALID_PARAM;
	}

	/* check the handle is valid */
	pInitData = getInitDataByHandle(ckptHandle);
	if (NULL == pInitData) {
		return SA_ERR_BAD_HANDLE;
	}

	/* new ckptList item */
	pCkptData = (CkptListDataT*)malloc(sizeof(CkptListDataT));
	if (NULL == pCkptData ) {
		perror("saCkptCheckpointOpen: malloc() failure");
		return SA_ERR_NO_MEMORY;
	}

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqOpenParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = *ckptHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_OPEN;
	reqhead.paramLen = sizeof(SaCkptReqOpenParamT);
	reqhead.dataLen = 0;
	
	/* prepare the request param */
	memcpy(&(openParam.ckptName), checkpointName, sizeof(SaNameT));
	openParam.openFlag = checkpointOpenFlags;
	if (NULL != checkpointCreationAttributes) {
		memcpy(&(openParam.attr), checkpointCreationAttributes, \
					sizeof(openParam.attr));
	}
	else {
		memset(&(openParam.attr), 0, sizeof(openParam.attr));
	}
	
	/* hello to ckpt service */
	retcode = hello(pInitData->syncfd, &reqhead, &openParam, &resphead, &tv);
	if (SA_OK != retcode) {
		free(pCkptData); 
		return retcode;
	}
	if (SA_OK != resphead.retval) {
		free(pCkptData); 
		return resphead.retval;
	}
	
	/* receive created checkpont handle */
	retcode = reliableRead(pInitData->syncfd, (char*)checkpointHandle, \
					sizeof(checkpointHandle), &tv);
	if (SA_OK != retcode) {
		/* ?service has processed your request */
		perror("saCkptCheckpointOpen: reliableRead failure");
		free(pCkptData); 
		return retcode;
	}
	
	/* add item to ckptList */
	pCkptData->pInit = pInitData;
	memcpy(&(pCkptData->ckptName), checkpointName, sizeof(SaNameT));
	pCkptData->checkpointHandle = *checkpointHandle;
	g_ckptList = g_list_append(g_ckptList, pCkptData);
	
	return SA_OK;
}

/* open a checkpoint asynchronously
 */
SaErrorT
saCkptCheckpointOpenAsync(
	const SaCkptHandleT *ckptHandle,
	SaInvocationT invocation,
	const SaNameT *checkpointName,
	const SaCkptCheckpointCreationAttributesT *checkpointCreationAttributes,
	SaCkptCheckpointOpenFlagsT checkpointOpenFlags)
{
	SaErrorT retcode;
	size_t len;
	InitListDataT* pInitData;
	ReqListDataT* pReqData;
	SaCkptRequestHeadT reqhead;
	SaCkptReqOpenParamT openParam;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == ckptHandle || NULL == checkpointName) {
		return SA_ERR_INVALID_PARAM;
	}

	/* check the handle is valid */
	pInitData = getInitDataByHandle(ckptHandle);
	if (NULL == pInitData) {
		return SA_ERR_BAD_HANDLE;
	}

	/* new reqList item */
	pReqData = (ReqListDataT*)malloc(sizeof(ReqListDataT));
	if (NULL == pReqData ) {
		perror("saCkptCheckpointOpenAsync: malloc() failure");
		return SA_ERR_NO_MEMORY;
	}
	
  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqOpenParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = *ckptHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_OPEN;
	reqhead.paramLen = sizeof(SaCkptReqOpenParamT);
	reqhead.dataLen = 0;

	/* prepare the request param */
	memcpy(&(openParam.ckptName), checkpointName, sizeof(SaNameT));
	openParam.openFlag = checkpointOpenFlags;
	if (NULL != checkpointCreationAttributes) {
		memcpy(&(openParam.attr), checkpointCreationAttributes, \
					sizeof(openParam.attr));
	}
	else {
		memset(&(openParam.attr), 0, sizeof(openParam.attr));
	}
	
	/* send request head */
	retcode = reliableWrite(pInitData->asyncfd, (char*)&reqhead, \
		sizeof(reqhead), &tv);
	if (SA_OK != retcode ) {
		perror("saCkptCheckpointOpenAsync: reliableWrite-1 failure");
		free(pReqData);
		return retcode;
	}

	/* send request param */
	retcode = reliableWrite(pInitData->asyncfd, (char*)&openParam, \
		sizeof(openParam), &tv);
	if (SA_OK != retcode ) {
		perror("saCkptCheckpointOpenAsync: reliableWrite-1 failure");
		free(pReqData);
		return retcode;
	}

	/* add item to reqList */
	pReqData->pInit = pInitData;
	pReqData->reqno = reqhead.reqno;
	pReqData->req = reqhead.req;
	pReqData->invocation = invocation;
	g_reqList = g_list_append(g_reqList, pReqData);

	return SA_OK;
}

/* free the resources allocated for checkpoint handle
 */
SaErrorT
saCkptCheckpointClose(const SaCkptCheckpointHandleT *checkpointHandle)
{
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	struct timeval tv = TIMEOUT_VAL;

	/* check the checkpoint handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptCheckpointHandleT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_CLOSE;
	reqhead.paramLen = sizeof(SaCkptCheckpointHandleT);
	reqhead.dataLen = 0;

	/* hello to ckpt service */
	retcode = hello(pCkptData->pInit->syncfd, &reqhead, \
		checkpointHandle, &resphead, &tv);
	if (SA_OK != retcode) {
		return retcode;
	}
	if (SA_OK != resphead.retval) {
		return resphead.retval;
	}

	/* remove item from ckptList and free the memory */
	g_ckptList = g_list_remove(g_ckptList, pCkptData);
	free(pCkptData);

	/* ?check reqList and remove the async request with this handle */
	return SA_OK;
}

/* remove this checkpoint.
 */
SaErrorT
saCkptCheckpointUnlink(
	const SaCkptHandleT *ckptHandle,
	const SaNameT *checkpointName)
{
	SaErrorT retcode;
	size_t len;
	InitListDataT* pInitData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == ckptHandle || NULL == checkpointName) {
		return SA_ERR_INVALID_PARAM;
	}

	/* check the handle is valid */
	pInitData = getInitDataByHandle(ckptHandle);
	if (NULL == pInitData) {
		return SA_ERR_BAD_HANDLE;
	}

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaNameT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = *ckptHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_ULNK;
	reqhead.paramLen = sizeof(SaNameT);
	reqhead.dataLen = 0;

	/* hello to ckpt service */
	retcode = hello(pInitData->syncfd, &reqhead, checkpointName,\
		&resphead, &tv);
	if (SA_OK != retcode) {
		return retcode;
	}
	if (SA_OK != resphead.retval) {
		return resphead.retval;
	}

	/* ?no local data struct update */
	return SA_OK;
}

/* set the checkpoint duration.
 */
SaErrorT
saCkptCheckpointRetentionDurationSet(
	const SaCkptCheckpointHandleT *checkpointHandle,
	SaTimeT retentionDuration)
{
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptReqRtnParamT rtnParam;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	struct timeval tv = TIMEOUT_VAL;

	/* check the checkpoint handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqRtnParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_RTN_SET;
	reqhead.paramLen = sizeof(SaCkptReqRtnParamT);
	reqhead.dataLen = 0;

	/* prepare the request param */
	rtnParam.checkpointHandle = *checkpointHandle;
	rtnParam.retention = retentionDuration;
	
	/* hello to ckpt service */
	retcode = hello(pCkptData->pInit->syncfd, &reqhead, &rtnParam,\
		&resphead, &tv);
	if (SA_OK != retcode) {
		return retcode;
	}
	if (SA_OK != resphead.retval) {
		return resphead.retval;
	}

	return SA_OK;
}

/* set the local replica as the active replica.
 */
SaErrorT
saCkptActiveCheckpointSet(const SaCkptCheckpointHandleT *checkpointHandle)
{
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	struct timeval tv = TIMEOUT_VAL;

	/* check the checkpoint handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	
  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptCheckpointHandleT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_ACT_SET;
	reqhead.paramLen = sizeof(SaCkptCheckpointHandleT);
	reqhead.dataLen = 0;
	
	/* hello to ckpt service */
	retcode = hello(pCkptData->pInit->syncfd, &reqhead, \
		checkpointHandle, &resphead, &tv);
	if (SA_OK != retcode) {
		return retcode;
	}
	if (SA_OK != resphead.retval) {
		return resphead.retval;
	}
	
	return SA_OK;
}

/* get the checkpoint status
 */
SaErrorT
saCkptCheckpointStatusGet(
	const SaCkptCheckpointHandleT *checkpointHandle,
	SaCkptCheckpointStatusT *checkpointStatus/*[out]*/)
{
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == checkpointStatus) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the checkpoint handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	
  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptCheckpointHandleT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_STAT_GET;
	reqhead.paramLen = sizeof(SaCkptCheckpointHandleT);
	reqhead.dataLen = 0;
	
	/* hello to ckpt service */
	retcode = hello(pCkptData->pInit->syncfd, &reqhead, \
		checkpointHandle, &resphead, &tv);
	if ( SA_OK != retcode ) {
		return retcode; /* should conform to AIS */
	}
	if (SA_OK != resphead.retval) {
		return resphead.retval;
	}
	
	/* get the response data */
	retcode = reliableRead(pCkptData->pInit->syncfd, \
		(char*)checkpointStatus, \
		sizeof(SaCkptCheckpointStatusT), &tv);
	if ( SA_OK != retcode ) {
		perror("saCkptCheckpointStatusGet: reliableRead failure");
		return retcode;
	}
	
	return SA_OK;
}

/* create a new section within the checkpoint and fill with the initial data.
 */
SaErrorT
saCkptSectionCreate(
	const SaCkptCheckpointHandleT *checkpointHandle,
	SaCkptSectionCreationAttributesT *sectionCreationAttributes,
	const void *initialData,
	SaUint32T initialDataSize)
{
	SaErrorT retcode;
	SaInt32T sockfd;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqSecCrtParamT secCrtParam;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == sectionCreationAttributes \
			|| (NULL == initialData && initialDataSize>0)) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	
  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqSecCrtParamT)\
		+ initialDataSize;
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_SEC_CRT;
	reqhead.paramLen = sizeof(SaCkptReqSecCrtParamT);
	reqhead.dataLen = initialDataSize;
	
	/* prepare the request param */
	secCrtParam.checkpointHandle = *checkpointHandle;
	secCrtParam.expireTime = sectionCreationAttributes->expirationTime;
	memcpy(&(secCrtParam.sectionID), sectionCreationAttributes->sectionId, \
		sizeof(SaCkptSectionIdT));

	if ( NULL == sectionCreationAttributes->sectionId
		|| sectionCreationAttributes->sectionId->idLen > SA_MAX_ID_LENGTH
		|| sectionCreationAttributes->sectionId->idLen < 0) {
		return SA_ERR_INVALID_PARAM;
	} else {
		/* 
		 * if section id is SA_CKPT_DEFAULT_SECTION_ID
		 * ckpt sevice daemon generates the section id and return it back
 		 */
		if ( 0 == sectionCreationAttributes->sectionId->idLen) {
			secCrtParam.sectionID.idLen = 0;
			secCrtParam.sectionID.id[0] = 0;
		} else {
			secCrtParam.sectionID.idLen = 
				sectionCreationAttributes->sectionId->idLen;
			memset(secCrtParam.sectionID.id, 0, SA_MAX_ID_LENGTH);
			memcpy(secCrtParam.sectionID.id, 
				sectionCreationAttributes->sectionId->id,
				secCrtParam.sectionID.idLen);
		}
	}

	/* send the request head */
	sockfd = pCkptData->pInit->syncfd;
	retcode = reliableWrite(sockfd, (char*)&reqhead, sizeof(reqhead), &tv);
	if ( SA_OK != retcode ) {
		perror("saCkptSectionCreate: reliableWrite-1 failure");
		return retcode;
	}	
	
	/* send the request param */
	retcode = reliableWrite(sockfd, (char*)&secCrtParam, \
		reqhead.paramLen, &tv);
	if ( SA_OK != retcode ) {
		perror("saCkptSectionCreate: reliableWrite-2 failure");
		return retcode;
	}	
	
	/* send the initial data */
	if ( NULL != initialData ) {
		retcode = reliableWrite(sockfd, initialData, \
				initialDataSize, &tv);
		if ( SA_OK != retcode ) {
			perror("saCkptSectionCreate: reliableWrite-3 failure");
			return retcode;
		}
	}

	/* get the response */
	retcode = readRespheadByReqno(sockfd, &resphead, reqhead.reqno, &tv);
	if ( SA_OK != retcode ) {
		perror("saCkptSectionCreate: readRespheadByReqno failure");
		return retcode;
	}	
	if( SA_OK != resphead.retval ) {
		perror("saCkptSectionCreate failure!");
	} else {
		if (0 == secCrtParam.sectionID.idLen) {
			// TODO: read the generated section id
		} else {
			/* discard the unexpected data */
			if (resphead.dataLen > 0) {
				removeData(sockfd, &resphead, &tv);
			}
		}
	}
	
	return resphead.retval;
}

/* 
 * delete the section within the checkpoint.
 */
SaErrorT
saCkptSectionDelete(
	const SaCkptCheckpointHandleT *checkpointHandle,
	const SaCkptSectionIdT *sectionId)
{
	SaInt32T sockfd;
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqSecDelParamT secDelParam;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == sectionId) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqSecDelParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_SEC_DEL;
	reqhead.paramLen = sizeof(SaCkptReqSecDelParamT);
	reqhead.dataLen = 0;

	/* prepare the request param */
	secDelParam.checkpointHandle = *checkpointHandle;
	if ( sectionId->idLen < 0 || sectionId->idLen > SA_MAX_ID_LENGTH ) {
		return SA_ERR_INVALID_PARAM;
	} else {
		secDelParam.sectionID.idLen = sectionId->idLen;
		memset(secDelParam.sectionID.id, 0, SA_MAX_ID_LENGTH);
		memcpy(secDelParam.sectionID.id, sectionId->id,
			secDelParam.sectionID.idLen);
	}

	/* hello to ckpt service */
	sockfd = pCkptData->pInit->syncfd;
	retcode = hello(sockfd, &reqhead, &secDelParam, &resphead, &tv);
	if ( SA_OK != retcode ) {
		return retcode; /* should conform to AIS */
	}
	
	return resphead.retval;
}

/* set the section expiration time
 */
SaErrorT
saCkptSectionExpirationTimeSet(
	const SaCkptCheckpointHandleT *checkpointHandle,
	const SaCkptSectionIdT* sectionId,
	SaTimeT expirationTime)
{
	SaInt32T sockfd;
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqSecExpSetParamT secExpParam;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == sectionId) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	
  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqSecExpSetParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_SEC_EXP_SET;
	reqhead.paramLen = sizeof(SaCkptReqSecExpSetParamT);
	reqhead.dataLen = 0;

	/* prepare the request param */
	secExpParam.checkpointHandle = *checkpointHandle;
	secExpParam.expireTime = expirationTime;
	if ( sectionId->idLen < 0 || sectionId->idLen > SA_MAX_ID_LENGTH ) {
		return SA_ERR_INVALID_PARAM;
	} else {
		secExpParam.sectionID.idLen = sectionId->idLen;
		memset(secExpParam.sectionID.id, 0, SA_MAX_ID_LENGTH);
		memcpy(secExpParam.sectionID.id, sectionId->id,
			secExpParam.sectionID.idLen);
	}

	
	/* hello to ckpt service */
	sockfd = pCkptData->pInit->syncfd;
	retcode = hello(sockfd, &reqhead, &secExpParam, &resphead, &tv);
	if ( SA_OK != retcode ) {
		return retcode; /* should conform to AIS */
	}
	
	return resphead.retval;
}

/* initialize the section interator
 */
SaErrorT
saCkptSectionIteratorInitialize(
	const SaCkptCheckpointHandleT *checkpointHandle,
	SaCkptSectionsChosenT sectionsChosen,
	SaTimeT expirationTime,
	SaCkptSectionIteratorT *sectionIterator/*[out]*/)
{
	SaInt32T sockfd;
	SaInt32T i, num;
	SaErrorT retcode;
	GList *pIterator = NULL;
	size_t len;
	CkptListDataT *pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqSecQueryParamT secQueryParam;
	SaCkptSectionDescriptorT *pSecDes;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == sectionIterator) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	
  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqSecQueryParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_SEC_QUERY;
	reqhead.paramLen = sizeof(SaCkptReqSecQueryParamT);
	reqhead.dataLen = 0;

	/* prepare the request param */
	secQueryParam.checkpointHandle = *checkpointHandle;
	secQueryParam.chosenFlag = sectionsChosen;
	secQueryParam.expireTime = expirationTime;
	
	/* hello to ckpt service */
	sockfd = pCkptData->pInit->syncfd;
	retcode = hello(sockfd, &reqhead, &secQueryParam, &resphead, &tv);
	if ( SA_OK != retcode ) {
		return retcode; /* should conform to AIS */
	}

	/* check data length */
	if (0 == resphead.dataLen) {
		/* no section match the chosen conditions */
		*sectionIterator = GPOINTER_TO_INT(NULL);
		return SA_OK;
	}
	
	/* dataLen should be the multiple times the size of sec descriptor */	
	if (0 != (resphead.dataLen%sizeof(SaCkptSectionDescriptorT)) ) {
		/* should not happen */
		perror("saCkptSectionIteratorInitialize: invalid data length");
		return SA_ERR_LIBRARY;
	}
	
	/* alloc memory */
	num = resphead.dataLen/sizeof(SaCkptSectionDescriptorT);
	pSecDes = (SaCkptSectionDescriptorT*)malloc(resphead.dataLen);
	if (NULL == pSecDes) {
		perror("saCkptSectionIteratorInitialize: malloc-1");
		/* remove the data left in input buffer, or it will 
		 * cause the data incomplete (because without header)
		 */
		removeData(sockfd, &resphead, &tv);
		return SA_ERR_NO_MEMORY; 
	}
	
	/* get the descriptor data */
	retcode = reliableRead(sockfd, (char*)pSecDes, resphead.dataLen, &tv);
	if (SA_OK != retcode ) {
		perror("saCkptSectionIteratorInitialize: reliableRead failure");
		free(pSecDes);
		return SA_ERR_LIBRARY;
	}
	
	/* add descriptor to the iterator one by one */
	for (i=0; i<num; i++) {
		pIterator = g_list_append(pIterator, &(pSecDes[i]));
	}
	
	/* add to global list */
	g_secIteratorList = g_list_append(g_secIteratorList, pIterator);
	
	/* set iterator  */
	*sectionIterator = GPOINTER_TO_INT(pIterator);
	
	return SA_OK;
}

/* get the next section in the iterator
 */
SaErrorT
saCkptSectionIteratorNext(
	SaCkptSectionIteratorT *sectionIterator/*[in/out]*/,
	SaCkptSectionDescriptorT *sectionDescriptor/*[out]*/)
{
	GList* pIterator = GINT_TO_POINTER(*sectionIterator);

	/* check the parameters */
	if (NULL == sectionDescriptor) {
		return SA_ERR_INVALID_PARAM;
	}
	
	/* check iterator */
	if (NULL == pIterator) {
		return SA_ERR_NOT_EXIST;
	}
	
	memcpy(sectionDescriptor, pIterator->data, sizeof(sectionDescriptor));
	pIterator = g_list_next(pIterator);
	*sectionIterator = GPOINTER_TO_INT(pIterator);
	
	return SA_OK;
}

/* release the resource allocated to this iterator, sectionIterator should be
 * that returned by saCkptSectionIteratorInitialize
 */
SaErrorT
saCkptSectionIteratorFinalize(
	SaCkptSectionIteratorT *sectionIterator)
{
	GList *pIterator, *pList;
	SaCkptSectionDescriptorT *pSecDes;

	pIterator = GINT_TO_POINTER(*sectionIterator);
	/* check this iterator does exist */
	pList = g_secIteratorList;
	while (NULL != pList) {
		if ( pIterator == pList->data ) {
			break;
		}
		pList = g_list_next(pList);
	}
	if (NULL == pList) {
		return SA_ERR_INVALID_PARAM;
	}
	
	/* remove this iterator from iterator list */
	g_secIteratorList = g_list_remove(g_secIteratorList, pIterator);
	
	/* free the allocated memory in this iterator */
	pSecDes = (SaCkptSectionDescriptorT*)(pIterator->data);
	free(pSecDes);

	/* free the iterator */
	g_list_free(pIterator);
	
	return SA_OK;
}

/* write multiple section data to checkpoint.If the invocation does not 
 * complete or returns with an error, nothing has been written at all.
 */
SaErrorT
saCkptCheckpointWrite(
	const SaCkptCheckpointHandleT *checkpointHandle,
	const SaCkptIOVectorElementT *ioVector,
	SaUint32T numberOfElements,
	SaUint32T *erroneousVectorIndex/*[out]*/)
{
	SaInt32T sockfd;
	SaErrorT retcode = SA_OK;
	SaUint32T i;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqSecWrtParamT secWrtParam;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == ioVector) {
		return SA_ERR_INVALID_PARAM;
	}

	/* check the handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	sockfd = pCkptData->pInit->syncfd;

  	/* prepare the request head */
	reqhead.initHandle = pCkptData->pInit->initHandle;
	/* reqhead.reqno = genReqNo(); */
	reqhead.req = REQ_SEC_WRT;
	reqhead.paramLen = sizeof(SaCkptReqSecWrtParamT);

	secWrtParam.checkpointHandle = *checkpointHandle;
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqSecWrtParamT);
	
	/* write iovector one by one */
	for (i=0; i<numberOfElements; i++) {
		/* reset the time out value */
		tv = TIMEOUT_VAL;
		
		/* prepare the request head */
		reqhead.reqno = genReqNo(); 
		reqhead.msglen = len + ioVector[i].dataSize - \
			sizeof(reqhead.msglen);
		reqhead.dataLen = ioVector[i].dataSize;
		
		/* prepare the request param */
		secWrtParam.offset = ioVector[i].dataOffset;
		if ( ioVector[i].sectionId.idLen < 0
			||ioVector[i].sectionId.idLen > SA_MAX_ID_LENGTH ) {
			return SA_ERR_INVALID_PARAM;
		} else {
			secWrtParam.sectionID.idLen = ioVector[i].sectionId.idLen;
			memset(secWrtParam.sectionID.id, 0, SA_MAX_ID_LENGTH);
			memcpy(secWrtParam.sectionID.id, ioVector[i].sectionId.id,
				secWrtParam.sectionID.idLen);
		}
		
		/* send the request head */
		retcode = reliableWrite(sockfd, (char*)&reqhead, \
				sizeof(reqhead), &tv);
		if (SA_OK != retcode) {
			perror("saCkptCheckpointWrite: reliableWrite-1");
			break;
		}	
		
		/* send the request param */
		retcode = reliableWrite(sockfd, (char*)&secWrtParam, \
			reqhead.paramLen, &tv);
		if (SA_OK != retcode) {
			perror("saCkptCheckpointWrite: reliableWrite-2");
			break;
		}	
		
		/* send the section data */
		retcode = reliableWrite(sockfd,\
				(char*)(ioVector[i].dataBuffer),\
				ioVector[i].dataSize, &tv);
		if (SA_OK != retcode) {
			perror("saCkptCheckpointWrite: reliableWrite-3");
			break;
		}
		
		/* get the response */
		retcode = readRespheadByReqno(sockfd, &resphead, \
				reqhead.reqno, &tv);
		if ( SA_OK != retcode ) {
			perror("saCkptCheckpointWrite: readRespheadByReqno");
			break;
		}
		
		/* discard the unexpedted data */
		if (resphead.dataLen > 0) {
			retcode = removeData(sockfd, &resphead, &tv);
		}
		
		if( SA_OK != resphead.retval ) {
			retcode = resphead.retval;
			break;
		}
	}/* end for */	

	if (erroneousVectorIndex != NULL) {
		*erroneousVectorIndex = i;
	}
	return retcode;
}

/* overwrite the section
 */
SaErrorT
saCkptSectionOverwrite(
	const SaCkptCheckpointHandleT *checkpointHandle,
	const SaCkptSectionIdT *sectionId,
	SaUint8T *dataBuffer,
	SaSizeT dataSize)
{
	SaInt32T sockfd;
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqSecOwrtParamT secOwrtParam;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == sectionId \
			|| NULL == dataBuffer) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}

	sockfd = pCkptData->pInit->syncfd;

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqSecOwrtParamT) \
		+ dataSize;
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_SEC_OWRT;
	reqhead.paramLen = sizeof(SaCkptReqSecOwrtParamT);
	reqhead.dataLen = dataSize;

  	/* prepare the request param */
	secOwrtParam.checkpointHandle = *checkpointHandle;
	if ( sectionId->idLen < 0 || sectionId->idLen > SA_MAX_ID_LENGTH ) {
		return SA_ERR_INVALID_PARAM;
	} else {
		secOwrtParam.sectionID.idLen = sectionId->idLen;
		memset(secOwrtParam.sectionID.id, 0, SA_MAX_ID_LENGTH);
		memcpy(secOwrtParam.sectionID.id, sectionId->id,
			secOwrtParam.sectionID.idLen);
	}


	/* send the request head */
	retcode = reliableWrite(sockfd, (char*)&reqhead, sizeof(reqhead), &tv);
	if (SA_OK != retcode) {
		perror("saCkptSectionOverwrite: reliableWrite-1");
		return retcode;
	}
	
	/* send the request param */
	retcode = reliableWrite(sockfd, (char*)&secOwrtParam, \
		reqhead.paramLen, &tv);
	if (SA_OK != retcode) {
		perror("saCkptSectionOverwrite: reliableWrite-2");
		return retcode;
	}	
	
	/* send the section data */
	retcode = reliableWrite(sockfd, (char*)(dataBuffer), dataSize, &tv);
	if (SA_OK != retcode) {
		perror("saCkptSectionOverwrite: reliableWrite-3");
		return retcode;
	}	
	
	/* get the response */
	retcode = readRespheadByReqno(sockfd, &resphead, 
			reqhead.reqno, &tv);
	if (SA_OK != retcode) {
		perror("saCkptSectionOverwrite: readRespheadByReqno");
		return retcode;
	}	

	if (resphead.dataLen > 0) { 
		retcode = removeData(sockfd, &resphead, &tv);
	}

	return resphead.retval;
}

/* read multiple section data from checkpoint.
 */
SaErrorT
saCkptCheckpointRead(
	const SaCkptCheckpointHandleT *checkpointHandle,
	SaCkptIOVectorElementT *ioVector,
	SaUint32T numberOfElements,
	SaUint32T *erroneousVectorIndex/*[out]*/)
{
	SaInt32T sockfd;
	SaErrorT retcode = SA_OK;
	SaUint32T i;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	SaCkptReqSecReadParamT secRdParam;
	struct timeval tv = TIMEOUT_VAL;

	/* check the parameters */
	if (NULL == checkpointHandle || NULL == ioVector) {
		return SA_ERR_INVALID_PARAM;
	}
	/* check the handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	sockfd = pCkptData->pInit->syncfd;

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptReqSecReadParamT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.req = REQ_SEC_RD;
	reqhead.paramLen = sizeof(SaCkptReqSecReadParamT);
	reqhead.dataLen = 0;

  	/* prepare the request param */
	secRdParam.checkpointHandle = *checkpointHandle;

	/* read section one by one */
	for (i=0; i<numberOfElements; i++) {
		size_t readsize;
		
		/* reset the timout */
		tv = TIMEOUT_VAL;
		
		/* set different request num for each section */
		reqhead.reqno = genReqNo();
		
		/* prepare the request param */
		if ( ioVector[i].sectionId.idLen < 0
			||ioVector[i].sectionId.idLen > SA_MAX_ID_LENGTH ) {
			return SA_ERR_INVALID_PARAM;
		} else {
			secRdParam.sectionID.idLen = ioVector[i].sectionId.idLen;
			memset(secRdParam.sectionID.id, 0, SA_MAX_ID_LENGTH);
			memcpy(secRdParam.sectionID.id, ioVector[i].sectionId.id,
				secRdParam.sectionID.idLen);
		}

		secRdParam.dataSize = ioVector[i].dataSize;
		secRdParam.offset = ioVector[i].dataOffset;

		/* hello to ckpt service */
		retcode = hello(sockfd, &reqhead, &secRdParam, &resphead, &tv);
		if ( SA_OK != retcode ){
			perror("saCkptCheckpointRead: hello-1");
			break;
		}
		if (SA_OK != resphead.retval) {
			retcode = resphead.retval;
			break;
		}
		
		/* read the section data */
		readsize = resphead.dataLen;
		retcode = reliableRead(sockfd, \
				(char*)(ioVector[i].dataBuffer), \
				readsize, &tv);
		if (SA_OK != retcode) {
			perror("saCkptCheckpointRead: reliableRead-1");
			break;
		}
		ioVector[i].readSize = readsize; /* the actural read size */
	}

	if (erroneousVectorIndex != NULL) {
		*erroneousVectorIndex = i;
	}
	
	return retcode;
}

/* synchronize the checkpoint to all replicas
 */
SaErrorT
saCkptCheckpointSynchronize(
	const SaCkptCheckpointHandleT *checkpointHandle,
	SaTimeT timeout)
{
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	SaCkptResponseHeadT resphead;
	long secs = timeout/((SaTimeT)1000000000L);
	long usecs = (timeout%((SaTimeT)1000000000L))*1000000L;
	struct timeval tv = {secs, usecs};

	/* check the checkpoint handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}

  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptCheckpointHandleT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_SYNC;
	reqhead.paramLen = sizeof(SaCkptCheckpointHandleT);
	reqhead.dataLen = 0;
	
	/* hello to ckpt service */
	retcode = hello(pCkptData->pInit->syncfd, &reqhead, \
		checkpointHandle, &resphead, &tv);
	if ( SA_OK != retcode ){
		return retcode;
	}

	return resphead.retval;
}

/* sychronize the checkpoint replica on all nodes
 */
SaErrorT
saCkptCheckpointSynchronizeAsync(
	const SaCkptHandleT *ckptHandle,
	SaInvocationT invocation,
	const SaCkptCheckpointHandleT *checkpointHandle)
{
	SaErrorT retcode;
	size_t len;
	CkptListDataT* pCkptData;
	SaCkptRequestHeadT reqhead;
	ReqListDataT* pReqData;
	struct timeval tv = TIMEOUT_VAL;

	/* check the checkpoint handle is valid */
	pCkptData = getCkptDataByHandle(checkpointHandle);
	if (NULL == pCkptData) {
		return SA_ERR_BAD_HANDLE;
	}
	/* new reqList item */
	pReqData = (ReqListDataT*)malloc(sizeof(ReqListDataT));
	if (NULL == pReqData ) {
		perror("saCkptCheckpointSynchronizeAsync: malloc-1 failure");
		return SA_ERR_NO_MEMORY;
	}
	
  	/* prepare the request head */
	len = sizeof(SaCkptRequestHeadT) + sizeof(SaCkptCheckpointHandleT);
	reqhead.msglen = len - sizeof(reqhead.msglen);
	reqhead.initHandle = pCkptData->pInit->initHandle;
	reqhead.reqno = genReqNo();
	reqhead.req = REQ_CKPT_SYNC;
	reqhead.paramLen = sizeof(SaCkptCheckpointHandleT);
	reqhead.dataLen = 0;

	/* send request head */
	retcode = reliableWrite(pCkptData->pInit->asyncfd, (char*)&reqhead,\
			sizeof(reqhead), &tv);
	if (SA_OK != retcode) {
		perror("saCkptCheckpointOpenAsync: reliableWrite-1 failure");
		free(pReqData);
		return retcode;
	}

	/* send request param */
	retcode = reliableWrite(pCkptData->pInit->asyncfd, \
		(const char*)checkpointHandle, \
		sizeof(SaCkptCheckpointHandleT), &tv);
	if (SA_OK != retcode) {
		perror("saCkptCheckpointOpenAsync: reliableWrite-2 failure");
		free(pReqData);
		return retcode;
	}
	
	/* add item to reqList */
	pReqData->pInit = pCkptData->pInit;
	pReqData->reqno = reqhead.reqno;
	pReqData->req = REQ_CKPT_SYNC;
	pReqData->invocation = invocation;
	g_reqList = g_list_append(g_reqList, pReqData);

	return SA_OK;
}
