/* 
 * request.c: 
 *
 * Copyright (C) 2003 Deng Pan <deng.pan@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>

#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <hb_api_core.h>
#include <hb_api.h>
#include <ha_msg.h>
#include <heartbeat.h>

#include <saf/ais.h>
#include <checkpointd/clientrequest.h>
#include "checkpointd.h"
#include "client.h"
#include "replica.h"
#include "message.h"
#include "request.h"
#include "response.h"
#include "operation.h"
#include "utils.h"

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

extern SaCkptServiceT* saCkptService;

/* Process client request */
gboolean
SaCkptRequestProcess(IPC_Channel* clientChannel)
{
	SaCkptRequestT 		*ckptReq = NULL;

	(void)_ha_msg_h_Id;
	(void)_heartbeat_h_Id;

	ckptReq = SaCkptRequestReceive(clientChannel);
	if (ckptReq != NULL) {
		SaCkptRequestStart(ckptReq);

		 return TRUE;
	} else {
		return FALSE;
	}
}

int 
SaCkptRequestStart(SaCkptRequestT* ckptReq) 
{
	SaCkptMessageT		*ckptMsg = NULL;
	SaCkptResponseT 	*ckptResp = NULL;
	
	SaCkptClientT		*client = NULL;
	SaCkptReplicaT		*replica = NULL;
	SaCkptOpenCheckpointT	*openCkpt = NULL;

	SaCkptReqInitParamT*	initParam = NULL;
	SaCkptReqOpenParamT*	openParam = NULL;
	SaCkptReqCloseParamT*	closeParam = NULL;

	int	clientHandle;
	int	checkpointHandle;
	void*	reqParam;

	client = ckptReq->client;
	clientHandle = ckptReq->clientRequest->clientHandle;
	reqParam = ckptReq->clientRequest->reqParam;

	ckptResp = SaCkptResponseCreate(ckptReq);

	switch (ckptReq->clientRequest->req) {
	case REQ_SERVICE_INIT:
		initParam = reqParam;
	
		/* FIXME: different version should work together */
		if (SaCkptVersionCompare(initParam->ver,
			saCkptService->version) != 0) {
			ckptResp->resp->retVal = SA_ERR_VERSION;
			SaCkptResponseSend(&ckptResp);
			break;
		}

		if (clientHandle <= 0) {
			/* create new client */
			client = SaCkptClientCreate(initParam);
			client->channel[0] = ckptReq->clientChannel;

			ckptReq->client = client;
			ckptReq->clientRequest->clientHandle = 
				client->clientHandle;

			/* 
			 * initialize is a special operation
			 * the handle will be returned in the response header
			 *
			 * should be sync with the client library
			 */
			ckptResp->resp->clientHandle = client->clientHandle;
		} else {
			/* add channel for async operation */
			client = g_hash_table_lookup(
				saCkptService->clientHash,
				(gpointer)&clientHandle);
			if (client == NULL) {
				ckptResp->resp->retVal = 
					SA_ERR_FAILED_OPERATION;
			} else {
				client->channel[1] = 
					ckptReq->clientChannel;
			}
		}
		SaCkptResponseSend(&ckptResp);
		
		break;

	case REQ_SERVICE_FINL:
		if ((g_list_length(client->pendingRequestList) != 0) ||
			(g_hash_table_size(client->requestHash) != 0)){
			ckptResp->resp->retVal = SA_ERR_BUSY;
			SaCkptResponseSend(&ckptResp);
			break;
		}

		/* send response first */
		SaCkptResponseSend(&ckptResp);
		
		SaCkptClientDelete(&client);

		break;
		
	case REQ_CKPT_OPEN:
		ckptReq->operation = OP_CKPT_OPEN;
		
		openParam = reqParam;

		replica = g_hash_table_lookup(saCkptService->replicaHash, 
			(gconstpointer)(openParam->ckptName.value));
		
		/* if local replica exist */
		if (replica != NULL) { 
			/* if unlinked, return error */
			if (replica->flagUnlink == TRUE) {
				ckptResp->resp->retVal = 
					SA_ERR_FAILED_OPERATION;
				SaCkptResponseSend(&ckptResp);
				break;
			}

			/* if the retention timer started, stop it */
			if (replica->retentionTimeoutTag > 0) {
				SaCkptReplicaStopTimer(replica);
			}
			
			openCkpt = SaCkptCheckpointOpen(client, 
				replica, openParam);
			/* update the request */
			ckptReq->openCkpt = openCkpt;

			/* send response */
			ckptResp->resp->dataLength = 
				sizeof(SaCkptCheckpointHandleT);
			ckptResp->resp->data = SaCkptMalloc( 
				ckptResp->resp->dataLength);
			SACKPTASSERT(ckptResp->resp->data != NULL);
			memcpy(ckptResp->resp->data, 
				&(openCkpt->checkpointHandle), 
				ckptResp->resp->dataLength);
			
			SaCkptResponseSend(&ckptResp);
			break;

		} else { 
			/* 
			 * no local replica.
			 * add request into queue and broadcast message 
			 */
			g_hash_table_insert(client->requestHash,
				(gpointer)&(ckptReq->clientRequest->requestNO),
				(gpointer)ckptReq);

			ckptMsg = SaCkptMessageCreateReq(ckptReq, 
				M_CKPT_OPEN_BCAST);
			strcpy(ckptMsg->checkpointName, openParam->ckptName.value);
			SaCkptMessageBroadcast(ckptMsg);
			SaCkptFree((void**)&ckptMsg);
			
			SaCkptRequestStartTimer(ckptReq);
		}

		break;

	case REQ_CKPT_CLOSE:
		closeParam = reqParam;
		
		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(closeParam->checkpointHandle));
		if (openCkpt == NULL) {
			ckptResp->resp->retVal = SA_ERR_BAD_HANDLE;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		ckptReq->openCkpt = openCkpt;
		
		/* no local replica, send message to active replica */
		if (openCkpt->flagLocalReplica != TRUE) {
			strcpy(ckptReq->toNodeName, openCkpt->activeNodeName);
			
			/* add it to request queue first */
			g_hash_table_insert(client->requestHash,
				(gpointer)&(ckptReq->clientRequest->requestNO),
				(gpointer)ckptReq);

			ckptMsg = SaCkptMessageCreateReq(ckptReq, 
				M_CKPT_CLOSE_REMOTE);
			SaCkptMessageSend(ckptMsg, openCkpt->activeNodeName);
			SaCkptFree((void**)&ckptMsg);
			
			SaCkptRequestStartTimer(ckptReq);
			 
			break;
		}

		// FIEXME: before the close, check whether there are still
		// pending operaitons

		/* local replica exist, close it */
		ckptResp->resp->retVal = SaCkptCheckpointClose(&openCkpt);
		SaCkptResponseSend(&ckptResp);
		
		break;
	
	case REQ_SEC_CRT:
	case REQ_SEC_DEL:
	case REQ_SEC_RD:
	case REQ_SEC_WRT:
	case REQ_SEC_OWRT:
		if (ckptReq->clientRequest->req == REQ_SEC_RD) {
			ckptReq->operation = OP_CKPT_READ;
		} else {
			ckptReq->operation = OP_CKPT_UPD;
		}

		/* the first field of reqParam is the handle */
		checkpointHandle= *(int*)reqParam;

		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(checkpointHandle));
		if (openCkpt == NULL) {
			ckptResp->resp->retVal = SA_ERR_BAD_HANDLE;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		ckptReq->openCkpt = openCkpt;

		strcpy(ckptReq->toNodeName, openCkpt->activeNodeName);
		g_hash_table_insert(client->requestHash,
			(gpointer)&(ckptReq->clientRequest->requestNO),
			(gpointer)ckptReq);

		if (ckptReq->clientRequest->req == REQ_SEC_RD) {
			ckptMsg = SaCkptMessageCreateReq(ckptReq, M_CKPT_READ);
		} else {
			ckptMsg = SaCkptMessageCreateReq(ckptReq, M_CKPT_UPD);
		}
		SaCkptMessageSend(ckptMsg, openCkpt->activeNodeName);
//		SaCkptMessageBroadcast(ckptMsg);
		SaCkptFree((void**)&ckptMsg);
		ckptMsg = NULL;
		
		SaCkptRequestStartTimer(ckptReq);
		
		break;
		
	default:
		cl_log(LOG_INFO, "Not implemented request");
		break;
	}

	if (ckptResp != NULL) {
		if (ckptResp->resp != NULL) {
			if (ckptResp->resp->data != NULL) {
				SaCkptFree((void**)&(ckptResp->resp->data));
			}
			SaCkptFree((void**)&(ckptResp->resp));
		}
		SaCkptFree((void**)&ckptResp);
	}
	
	return HA_OK;
}


/* request timeout process, send back timeout response to client */
gboolean
SaCkptRequestTimeout(gpointer timeout_data)
{
	SaCkptRequestT* ckptReq = (SaCkptRequestT*)timeout_data;
	SaCkptResponseT* ckptResp = NULL;
	
	SaCkptOpenCheckpointT* openCkpt = ckptReq->openCkpt;
	SaCkptClientT* client = ckptReq->client;
	SaCkptReplicaT* replica = NULL;

	SaCkptReqOpenParamT* openParam = NULL;

	char* strReq = NULL;

	strReq = SaCkptReq2String(ckptReq->clientRequest->req);
	cl_log(LOG_INFO, 
		"Request timeout, client %d, request %lu (%s)", 
		ckptReq->clientRequest->clientHandle,
		ckptReq->clientRequest->requestNO, strReq);
	SaCkptFree((void**)&strReq);

	ckptResp = SaCkptResponseCreate(ckptReq);
	
	switch (ckptReq->clientRequest->req) {
	case REQ_CKPT_OPEN:
		openParam = ckptReq->clientRequest->reqParam;

		if (openParam->openFlag & 
			SA_CKPT_CHECKPOINT_COLOCATED) {

			/* create local replica */
			replica = SaCkptReplicaCreate(openParam);
			SACKPTASSERT(replica != NULL);
			replica->replicaState= STATE_CREATE_COMMITTED;

			/* open the local replica */
			openCkpt = SaCkptCheckpointOpen(client, 
				replica, openParam);
			SACKPTASSERT(openCkpt != NULL);
			ckptReq->openCkpt = openCkpt;

			/* create and send client response */
			ckptResp->resp->dataLength = 
				sizeof(SaCkptCheckpointHandleT);
			ckptResp->resp->data = SaCkptMalloc(
				sizeof(SaCkptCheckpointHandleT));
			memcpy(ckptResp->resp->data, 
				&(openCkpt->checkpointHandle),
				sizeof(openCkpt->checkpointHandle));

			ckptResp->resp->retVal = SA_OK;
		} else {
			ckptResp->resp->retVal = SA_ERR_TIMEOUT;
		}
		
		break;

	case REQ_CKPT_CLOSE:
	case REQ_SEC_CRT:
	case REQ_SEC_DEL:
	case REQ_SEC_RD:
	case REQ_SEC_WRT:
	case REQ_SEC_OWRT:
		ckptResp->resp->retVal = SA_ERR_TIMEOUT;

		break;
		
	default:
		break;
		
	}
	
	SaCkptResponseSend(&ckptResp);

	/* alway return FALSE since it was one-time timer*/
	return FALSE;
}


SaCkptRequestT*
SaCkptRequestReceive(IPC_Channel* clientChannel)
{
	IPC_Message		*ipcMsg = NULL;

	SaCkptRequestT		*ckptReq = NULL;
	SaCkptResponseT		*ckptResp = NULL;
	SaCkptClientRequestT	*clientRequest = NULL;
	
	char	*p = NULL;
	int	rc = IPC_OK;
	
	char	*strReq = NULL;

	/* receive ipc message */
	rc = clientChannel->ops->recv(clientChannel, &ipcMsg);
	if (rc != IPC_OK) {
//		cl_log(LOG_ERR, "Receive error request");
		return NULL;
	}

	if (ipcMsg->msg_len <
		sizeof(SaCkptClientRequestT) - 2*sizeof(void*)) {
		cl_log(LOG_ERR, "Error request");
		return NULL;
	}

	p = ipcMsg->msg_body;

	clientRequest = SaCkptMalloc(sizeof(SaCkptClientRequestT));
	SACKPTASSERT(clientRequest != NULL);
	memcpy(clientRequest, p, 
		sizeof(SaCkptClientRequestT) - 2*sizeof(void*));
	p += (sizeof(SaCkptClientRequestT) - 2*sizeof(void*));

	/* the param and data length should not be negative */
	if (clientRequest->reqParamLength > 0) {
		clientRequest->reqParam = SaCkptMalloc(
			clientRequest->reqParamLength);
		SACKPTASSERT(clientRequest->reqParam != NULL);
		memcpy(clientRequest->reqParam, p, 
			clientRequest->reqParamLength);
		p += clientRequest->reqParamLength;
	} else {
		clientRequest->reqParam = NULL;
	}

	if (clientRequest->dataLength > 0) {
		clientRequest->data = SaCkptMalloc(
			clientRequest->dataLength);
		SACKPTASSERT(clientRequest->data != NULL);
		memcpy(clientRequest->data, p, 
			clientRequest->dataLength);
		p += clientRequest->dataLength;
	} else {
		clientRequest->data = NULL;
	}

	/* free ipc message */
	if (ipcMsg->msg_body != NULL) {
		SaCkptFree((void**)&(ipcMsg->msg_body));
	}
	SaCkptFree((void**)&ipcMsg);

	if (saCkptService->flagVerbose) {
		strReq = SaCkptReq2String(clientRequest->req);
		cl_log(LOG_INFO, 
			"<<<---");
		cl_log(LOG_INFO,
			"Receive request %lu (%s), client %d",
			clientRequest->requestNO, strReq,
			clientRequest->clientHandle);
		SaCkptFree((void**)&strReq);
	}

	ckptReq = SaCkptMalloc(sizeof(SaCkptRequestT));
	SACKPTASSERT(ckptReq != NULL);
	
	ckptReq->clientRequest= clientRequest;
	ckptReq->clientChannel = clientChannel;
	ckptReq->client = NULL;
	ckptReq->openCkpt = NULL;
	ckptReq->operation = 0;
	ckptReq->timeoutTag = 0;

	/* by default, the request send to itself */
	strcpy(ckptReq->toNodeName, saCkptService->nodeName);

	/* check the clienthanle */
	if (clientRequest->clientHandle < 0) {
		ckptResp = SaCkptResponseCreate(ckptReq);
		ckptResp->resp->retVal = SA_ERR_BAD_HANDLE;
		SaCkptResponseSend(&ckptResp);
		
		return NULL;
	} else {
		ckptReq->client = (SaCkptClientT*)g_hash_table_lookup(
			saCkptService->clientHash, 
			(gpointer)&(clientRequest->clientHandle));

		if ((ckptReq->client == NULL) &&
			(ckptReq->clientRequest->req != REQ_SERVICE_INIT)) {
			ckptResp = SaCkptResponseCreate(ckptReq);
			ckptResp->resp->retVal = SA_ERR_BAD_HANDLE;
			SaCkptResponseSend(&ckptResp);
			
			return NULL;
		}
	}

	return ckptReq;

}

/* remove the request and free its memory */
int 
SaCkptRequestRemove(SaCkptRequestT** pCkptReq)
{
	SaCkptRequestT* ckptReq = *pCkptReq;
	SaCkptClientT* client = ckptReq->client;
	unsigned int requestNO = ckptReq->clientRequest->requestNO;

	if (client != NULL) {
		/* remove ckptReq from request queue */
		g_hash_table_remove(client->requestHash, 
			(gconstpointer)&(requestNO));
	}

	SaCkptRequestStopTimer(ckptReq);

	/* free ckptReq*/
	if (ckptReq->clientRequest->data != NULL) {
		SaCkptFree((void**)&(ckptReq->clientRequest->data));
	}
	
	if (ckptReq->clientRequest->reqParam != NULL) {
		SaCkptFree((void**)&(ckptReq->clientRequest->reqParam));
	}
	SaCkptFree((void**)&(ckptReq->clientRequest));
	SaCkptFree((void**)&ckptReq);

#if 0
	/* start pending request */
	if (g_hash_table_size(client->requestHash) == 0) {
		GList* list = NULL;
		
		list = client->pendingRequestList;
		if (list != NULL) {
			ckptReq = (SaCkptRequestT*)list->data;

			if (ckptReq != NULL) {
				client->pendingRequestList = 
					g_list_remove(
					client->pendingRequestList, 
					(gpointer)ckptReq);

				SaCkptRequestStart(ckptReq);
			}
		}
	}
#endif

	*pCkptReq = NULL;

	return HA_OK;
}

void 
SaCkptRequestStartTimer(SaCkptRequestT* ckptReq)
{
	char* strReq = NULL;

	/* to avoid start more than one timer */
	if (ckptReq->timeoutTag <= 0) {
		ckptReq->timeoutTag = Gmain_timeout_add(
			REQUEST_TIMEOUT * 1000, 
			SaCkptRequestTimeout, 
			(gpointer)ckptReq);

		if (saCkptService->flagVerbose) {
			strReq = SaCkptReq2String(
				ckptReq->clientRequest->req);
			cl_log(LOG_INFO, 
			"Start timer %u for request %lu (%s), client %d",
			ckptReq->timeoutTag,
			ckptReq->clientRequest->requestNO, strReq,
			ckptReq->client->clientHandle);
			SaCkptFree((void**)&strReq);
		}
	}

	return;
}

void
SaCkptRequestStopTimer(SaCkptRequestT* ckptReq)
{
	char* strReq = NULL;

	if (ckptReq->timeoutTag > 0) {
		if (saCkptService->flagVerbose) {
			strReq = SaCkptReq2String(
				ckptReq->clientRequest->req);
			cl_log(LOG_INFO, 
			"delete timer %u for request %lu (%s), client %d",
			ckptReq->timeoutTag,
			ckptReq->clientRequest->requestNO, strReq,
			ckptReq->client->clientHandle);
			SaCkptFree((void**)&strReq);
		}
		
		g_source_remove(ckptReq->timeoutTag);
		ckptReq->timeoutTag = 0;
	}

	return;
}

char* 
SaCkptReq2String(SaCkptReqT req)
{
	char* strTemp = NULL;
	char* strReq = NULL;

	strTemp = (char*)SaCkptMalloc(64);
	SACKPTASSERT(strTemp != NULL);

	switch (req) {
	case REQ_SERVICE_INIT:
		strcpy(strTemp, "REQ_SERVICE_INIT");
		break;
	case REQ_SERVICE_FINL:
		strcpy(strTemp, "REQ_SERVICE_FINL");
		break;
	case REQ_CKPT_OPEN:
		strcpy(strTemp, "REQ_CKPT_OPEN");
		break;
	case REQ_CKPT_CLOSE:
		strcpy(strTemp, "REQ_CKPT_CLOSE");
		break;
	case REQ_CKPT_ULNK:
		strcpy(strTemp, "REQ_CKPT_ULNK");
		break;
	case REQ_CKPT_RTN_SET:
		strcpy(strTemp, "REQ_CKPT_RTN_SET");
		break;
	case REQ_CKPT_ACT_SET:
		strcpy(strTemp, "REQ_CKPT_ACT_SET");
		break;
	case REQ_CKPT_STAT_GET:
		strcpy(strTemp, "REQ_CKPT_STAT_GET");
		break;
	case REQ_SEC_CRT:
		strcpy(strTemp, "REQ_SEC_CRT");
		break;
	case REQ_SEC_DEL:
		strcpy(strTemp, "REQ_SEC_DEL");
		break;
	case REQ_SEC_EXP_SET:
		strcpy(strTemp, "REQ_SEC_EXP_SET");
		break;
	case REQ_SEC_QUERY:
		strcpy(strTemp, "REQ_SEC_QUERY");
		break;
	case REQ_SEC_WRT:
		strcpy(strTemp, "REQ_SEC_WRT");
		break;
	case REQ_SEC_OWRT:
		strcpy(strTemp, "REQ_SEC_OWRT");
		break;
	case REQ_SEC_RD:
		strcpy(strTemp, "REQ_SEC_RD");
		break;
	case REQ_CKPT_SYNC:
		strcpy(strTemp, "REQ_CKPT_SYNC");
		break;
	}

	strReq = (char*)SaCkptMalloc(strlen(strTemp)+1);
	if (strReq == NULL) {
		return NULL;
	}
	memcpy(strReq, strTemp, strlen(strTemp)+1);

	SaCkptFree((void**)&strTemp);

	return strReq;
}

/* 
 * after the node failure, 
 * the request will be resent to the new active replica 
 */
void 
SaCkptRequestNodeFailure(gpointer key, 
	gpointer value,
	gpointer userdata)
{
	SaCkptRequestT* ckptReq = value;
	char* strNodeName = userdata;

	/* restart the sent but not finished requests */
	if (!strcmp(ckptReq->toNodeName, strNodeName)) {
		SaCkptRequestStart(ckptReq);
	}

	return;
}
