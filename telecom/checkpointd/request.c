/* $Id: request.c,v 1.13 2005/03/16 17:11:15 lars Exp $ */
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>

#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/realtime.h>
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

	while (clientChannel->ops->is_message_pending(clientChannel) 
		== TRUE) {
		ckptReq = SaCkptRequestReceive(clientChannel);
		if (ckptReq != NULL) {
			SaCkptRequestStart(ckptReq);

			return TRUE;
		} else {
			return FALSE;
		}
	}

	return TRUE;

}

int 
SaCkptRequestStart(SaCkptRequestT* ckptReq) 
{
	SaCkptMessageT		*ckptMsg = NULL;
	SaCkptResponseT 	*ckptResp = NULL;
	
	SaCkptClientT		*client = NULL;
	SaCkptReplicaT		*replica = NULL;
	SaCkptOpenCheckpointT	*openCkpt = NULL;

	SaCkptReqInitParamT	*initParam = NULL;
	SaCkptReqOpenParamT	*openParam = NULL;
	SaCkptReqCloseParamT	*closeParam = NULL;
	SaCkptReqSecExpSetParamT	*secExpSetParam = NULL;
	SaCkptReqRtnParamT	*rtnParam = NULL;
/*	SaCkptReqUlnkParamT	*unlinkParam = NULL; */
	SaCkptReqSecQueryParamT	*secQueryParam = NULL;
	
	SaCkptSectionT		*section = NULL;
	SaCkptCheckpointStatusT *checkpointStatus = NULL;
	SaCkptCheckpointCreationAttributesT	*attr = NULL;
	const char *sectionName 	= NULL;
	char* strReq = NULL;
	SaCkptHandleT		clientHandle = 0;
	SaCkptCheckpointHandleT	checkpointHandle = 0;
	void			*reqParam = NULL;
	SaNameT			*unlinkName = NULL;
	

	SaTimeT			timeout = 0;
	
	SaCkptSectionDescriptorT	*sectionDescriptor = NULL;
	int 				secListPass = 0;
	unsigned int 				secListTotalSize = 0;
	int			sectNumber = 0;
	int			descNumber = 0;
	
	char			*p = NULL;
	int			sectSelected = 0;
	
	GList			*list = NULL;

	timeout = REQUEST_TIMEOUT * 1000LL * 1000LL * 1000LL;

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
	case REQ_CKPT_OPEN_ASYNC:
		ckptReq->operation = OP_CKPT_OPEN;

		openParam = reqParam;

		unlinkName = g_hash_table_lookup(
			saCkptService->unlinkedCheckpointHash,
			(gconstpointer)(openParam->ckptName.value));
		if (unlinkName != NULL) {
			cl_log(LOG_ERR, 
				"checkpoint has already been unlinked");
			/* FIXME  No return value defined on SPEC A*/
			ckptResp->resp->retVal = 
				SA_ERR_INVALID_PARAM;
			SaCkptResponseSend(&ckptResp);
			break;
		}

		replica = g_hash_table_lookup(saCkptService->replicaHash, 
			(gconstpointer)(openParam->ckptName.value));
		
		/* if local replica exist */
		if (replica != NULL) { 
			/* if unlinked, return error */
			if (replica->flagUnlink == TRUE) {
				cl_log(LOG_ERR, 
				"checkpoint has already been unlinked");
				
				ckptResp->resp->retVal = 
					SA_ERR_INVALID_PARAM;
				SaCkptResponseSend(&ckptResp);
				break;
			}

			/* 
			 * if the createattribute is not null and is 
			 * different from the replica, return error
			 */
			attr = &(openParam->attr);
			if ((attr->checkpointSize != 
				replica->maxCheckpointSize) ||
			    (attr->creationFlags != 
				replica->createFlag) || 
			    (attr->maxSectionIdSize !=
				replica->maxSectionIDSize) || 
			    (attr->maxSections != 
				replica->maxSectionNumber) ||
			    (attr->maxSectionSize !=
				replica->maxSectionSize)) {
				cl_log(LOG_ERR, 
					"Attribute is different");
				
				ckptResp->resp->retVal = SA_ERR_EXIST;
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
				
			initOpenReqNodeStatus(ckptReq->clientRequest);

			ckptMsg = SaCkptMessageCreateReq(ckptReq, 
				M_CKPT_OPEN_BCAST);
			strcpy(ckptMsg->checkpointName, openParam->ckptName.value);
			SaCkptMessageBroadcast(ckptMsg);
			SaCkptFree((void*)&ckptMsg);

			 if (openParam->timetout < timeout) {
			 	timeout = openParam->timetout;
			 }
			SaCkptRequestStartTimer(ckptReq, timeout);
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
			ckptMsg->data = SaCkptMalloc(
				sizeof(openCkpt->checkpointRemoteHandle));
			ckptMsg->dataLength = 
				sizeof(openCkpt->checkpointRemoteHandle);
			memcpy(ckptMsg->data, 
				&(openCkpt->checkpointRemoteHandle),
				sizeof(openCkpt->checkpointRemoteHandle));
			SaCkptMessageSend(ckptMsg, openCkpt->activeNodeName);
			SaCkptFree((void*)&ckptMsg);
			
			SaCkptRequestStartTimer(ckptReq, timeout);
			 
			break;
		}

		/* local replica exist, close it */
		ckptResp->resp->retVal = SaCkptCheckpointClose(&openCkpt);
		SaCkptResponseSend(&ckptResp);
		
		break;

	case REQ_CKPT_ACT_SET:
		ckptReq->operation = OP_CKPT_ACT_SET;

		/* the first field of reqParam is the handle */
		checkpointHandle= *(SaCkptCheckpointHandleT*)reqParam;

		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(checkpointHandle));
		if (openCkpt == NULL) {
			ckptResp->resp->retVal = SA_ERR_BAD_HANDLE;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		ckptReq->openCkpt = openCkpt;

		replica = openCkpt->replica;
		if (replica == NULL) {
			/* no local replica */
			ckptResp->resp->retVal = SA_ERR_FAILED_OPERATION;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		
		if (replica->flagIsActive == TRUE) {
			/* is already active replica */
			SaCkptResponseSend(&ckptResp);
			break;
		}

		strcpy(ckptReq->toNodeName, openCkpt->activeNodeName);
		if (replica->flagReplicaPending == TRUE) {
			client->pendingRequestList = g_list_append(
				client->pendingRequestList,
				ckptReq);
			break;
		}

		g_hash_table_insert(client->requestHash,
			(gpointer)&(ckptReq->clientRequest->requestNO),
			(gpointer)ckptReq);

		ckptMsg = SaCkptMessageCreateReq(ckptReq, M_CKPT_ACT_SET_BCAST);
		SaCkptMessageBroadcast(ckptMsg);
		SaCkptFree((void*)&ckptMsg);
		ckptMsg = NULL;
		
		SaCkptRequestStartTimer(ckptReq, timeout);
		
		break;	
		
	case REQ_SEC_CRT:
		sectionName = ((SaCkptReqSecCrtParamT *)(ckptReq->clientRequest->reqParam))->sectionID.id;
		goto begin;
	case REQ_SEC_DEL:
		sectionName = ((SaCkptReqSecDelParamT *)(ckptReq->clientRequest->reqParam))->sectionID.id;
		goto begin;
	case REQ_SEC_WRT:
		sectionName = ((SaCkptReqSecWrtParamT *)(ckptReq->clientRequest->reqParam))->sectionID.id;
		goto begin;
	case REQ_SEC_OWRT:
		sectionName = ((SaCkptReqSecOwrtParamT *)(ckptReq->clientRequest->reqParam))->sectionID.id;
		goto begin;
	case REQ_SEC_RD:
		sectionName = ((SaCkptReqSecReadParamT *)(ckptReq->clientRequest->reqParam))->sectionID.id;
		goto begin;
	case REQ_CKPT_SYNC:
	case REQ_CKPT_SYNC_ASYNC:
begin:		strReq = SaCkptReq2String(ckptReq->clientRequest->req);
		if ((ckptReq->clientRequest->req != REQ_CKPT_SYNC) &&
			(ckptReq->clientRequest->req != REQ_CKPT_SYNC_ASYNC)){
			if(saCkptService->flagVerbose){
				cl_log(LOG_INFO,"Request %lu(%s), section is %s",	\
				ckptReq->clientRequest->requestNO, strReq,sectionName );
			}
		}
		if (ckptReq->clientRequest->req == REQ_SEC_RD) {
			ckptReq->operation = OP_CKPT_READ;
		} else if ((ckptReq->clientRequest->req == REQ_CKPT_SYNC) ||
			(ckptReq->clientRequest->req == REQ_CKPT_SYNC_ASYNC)){
			ckptReq->operation = OP_CKPT_SYNC;
		} else {
			ckptReq->operation = OP_CKPT_UPD;
		}

		/* the first field of reqParam is the handle */
		checkpointHandle= *(SaCkptCheckpointHandleT*)reqParam;

		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(checkpointHandle));
		if (openCkpt == NULL) {
			ckptResp->resp->retVal = SA_ERR_BAD_HANDLE;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		replica = openCkpt->replica;
		ckptReq->openCkpt = openCkpt;

		strcpy(ckptReq->toNodeName, openCkpt->activeNodeName);
		
		if ((replica != NULL) && 
			(replica->flagReplicaPending == TRUE)) {
			client->pendingRequestList = g_list_append(
				client->pendingRequestList,
				ckptReq);
			break;
		}
		/* FIXME */
		/* How about replica is NULL ? */

		/* add request to hash table */
		g_hash_table_insert(client->requestHash,
			&(ckptReq->clientRequest->requestNO),
			ckptReq);

		if (ckptReq->clientRequest->req == REQ_SEC_RD) {
			ckptMsg = SaCkptMessageCreateReq(ckptReq, M_CKPT_READ);
		} else if ((ckptReq->clientRequest->req == REQ_CKPT_SYNC) ||
			(ckptReq->clientRequest->req == REQ_CKPT_SYNC_ASYNC)){
			SaCkptReqSyncParamT* syncParam = NULL;

			syncParam = (SaCkptReqSyncParamT*)reqParam;
			if (syncParam->timeout < timeout) {
				timeout = syncParam->timeout;
			}
			ckptMsg = SaCkptMessageCreateReq(ckptReq, M_CKPT_SYNC);
		} else {
			ckptMsg = SaCkptMessageCreateReq(ckptReq, M_CKPT_UPD);
		}
		SaCkptMessageSend(ckptMsg, openCkpt->activeNodeName);
		SaCkptFree((void*)&ckptMsg);
		ckptMsg = NULL;
		
		SaCkptRequestStartTimer(ckptReq, timeout);
		
		break;

	case REQ_SEC_EXP_SET:
		secExpSetParam = reqParam;
		checkpointHandle= secExpSetParam->checkpointHandle;

		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(checkpointHandle));
		if ((openCkpt == NULL) || 
			(openCkpt->replica == NULL)) {
			ckptResp->resp->retVal = SA_ERR_LIBRARY;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		ckptReq->openCkpt = openCkpt;
		strcpy(ckptReq->toNodeName, openCkpt->activeNodeName);
		
		replica = openCkpt->replica;
		section = SaCkptSectionFind(replica, 
					&secExpSetParam->sectionID);
		if (section == NULL) {
			ckptResp->resp->retVal = SA_ERR_LIBRARY;
			SaCkptResponseSend(&ckptResp);
			break;
		}

		section->expirationTime = secExpSetParam->expireTime;
		/* FIXME: start expiration timer */
		
		ckptResp->resp->retVal = SA_OK;
		SaCkptResponseSend(&ckptResp);
		
		break;
		
	case REQ_CKPT_STAT_GET:
		/* the first field of reqParam is the handle */
		checkpointHandle= *(SaCkptCheckpointHandleT*)reqParam;

		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(checkpointHandle));
		if ((openCkpt == NULL) || 
			(openCkpt->replica == NULL)) {
			ckptResp->resp->retVal = SA_ERR_LIBRARY;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		ckptReq->openCkpt = openCkpt;
		strcpy(ckptReq->toNodeName, openCkpt->activeNodeName);
		
		replica = openCkpt->replica;

		checkpointStatus = SaCkptMalloc(
			sizeof(SaCkptCheckpointStatusT));
		if (checkpointStatus == NULL) {
			ckptResp->resp->retVal = SA_ERR_NO_MEMORY;
			SaCkptResponseSend(&ckptResp);
			break;
		}

		attr = &(checkpointStatus->checkpointCreationAttributes);
		
		attr->checkpointSize = replica->maxCheckpointSize;
		attr->creationFlags = replica->createFlag;
		attr->maxSectionIdSize = replica->maxSectionIDSize;
		attr->maxSections = replica->maxSectionNumber;
		attr->maxSectionSize = replica->maxSectionSize;
		attr->retentionDuration = replica->retentionDuration;
		checkpointStatus->numberOfSections = replica->sectionNumber;
		checkpointStatus->memoryUsed = replica->checkpointSize; 
					
		ckptResp->resp->retVal = SA_OK;
		ckptResp->resp->data = checkpointStatus;
		ckptResp->resp->dataLength = sizeof(SaCkptCheckpointStatusT);
		SaCkptResponseSend(&ckptResp);
		
		break;
				
	case REQ_CKPT_RTN_SET:
		rtnParam = reqParam;
		checkpointHandle= rtnParam->checkpointHandle;

		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(checkpointHandle));
		if ((openCkpt == NULL) || 
			(openCkpt->replica == NULL)) {
			ckptResp->resp->retVal = SA_ERR_LIBRARY;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		ckptReq->openCkpt = openCkpt;
		strcpy(ckptReq->toNodeName, openCkpt->activeNodeName);
		
		replica = openCkpt->replica;
		replica->retentionDuration = rtnParam->retention;

		ckptResp->resp->retVal = SA_OK;
		SaCkptResponseSend(&ckptResp);
		
		break;	

	case REQ_SEC_QUERY:
		secQueryParam = reqParam;
		checkpointHandle= secQueryParam->checkpointHandle;

		openCkpt = g_hash_table_lookup(
			saCkptService->openCheckpointHash,
			(gpointer)&(checkpointHandle));
		if ((openCkpt == NULL) || 
			(openCkpt->replica == NULL)) {
			ckptResp->resp->retVal = SA_ERR_LIBRARY;
			SaCkptResponseSend(&ckptResp);
			break;
		}
		sectionDescriptor =(SaCkptSectionDescriptorT *)SaCkptMalloc(sizeof(SaCkptSectionDescriptorT));
		if(sectionDescriptor == NULL){
			ckptResp->resp->retVal = SA_ERR_NO_MEMORY;
			SaCkptResponseSend(&ckptResp);
		}
		replica = openCkpt->replica;
		descNumber = 0;
		/*
		 * here a two-pass search used
		 * in first search, we get how much to alloc
		 * in second search, we copy all data we needed
		 *  maybe some improvement here needed
		 */
		for(secListPass = 0; secListPass <2; secListPass ++){
			list = replica->sectionList;
			if(secListPass ==1){
				/*
				 * the response begin with section number
				 * then followed by the sections, with ID at the end of each one
				 */
				ckptResp->resp->data = SaCkptMalloc(
					secListTotalSize + sizeof(int));
				
				if (ckptResp->resp->data == NULL) {
					ckptResp->resp->retVal = SA_ERR_NO_MEMORY;
					SaCkptFree((void**)&sectionDescriptor);
					SaCkptResponseSend(&ckptResp);
					break;
				}
				
				memset(ckptResp->resp->data, 0, sectNumber * sizeof(SaCkptSectionDescriptorT));
				p = ckptResp->resp->data;
				*(int *)p = descNumber;
				p += sizeof(int);
			}else{
				secListTotalSize = 0;
			}

			while (list != NULL) {
				sectSelected = 0;
				
				section = (SaCkptSectionT*)list->data;
				switch (secQueryParam->chosenFlag) {
				case SA_CKPT_SECTIONS_FOREVER:
					if (section->expirationTime == SA_TIME_END) {
						sectSelected = 1;
					} 
					break;
					
				case SA_CKPT_SECTIONS_LEQ_EXPIRATION_TIME:
					if ((section->expirationTime <= 
						secQueryParam->expireTime) &&
						(section->expirationTime != 
						SA_TIME_END)){
						sectSelected = 1;
					}
					break;
					
				case SA_CKPT_SECTIONS_GEQ_EXPIRATION_TIME:
					if ((section->expirationTime >= 
						secQueryParam->expireTime) &&
						(section->expirationTime != 
						SA_TIME_END)){
						sectSelected = 1;
					}
					break;
					
				case SA_CKPT_SECTIONS_CORRUPTED:
					if (section->dataState == 
						SA_CKPT_SECTION_CORRUPTED) {
						sectSelected = 1;
					}
					break;
					
				case SA_CKPT_SECTIONS_ANY:
					sectSelected = 1;
					break;
					
				default:
					cl_log(LOG_ERR, "Unknown section chosenFlag");
					
					SaCkptFree((void**)&sectionDescriptor);
					ckptResp->resp->retVal = SA_ERR_LIBRARY;
					SaCkptResponseSend(&ckptResp);
					break;
				}
	
				if (sectSelected == 1) {
					if(secListPass == 0){
						descNumber ++;
						secListTotalSize += (section->sectionID.idLen + sizeof(SaCkptSectionDescriptorT));
					}else{
						sectionDescriptor->sectionId.idLen= 
						section->sectionID.idLen;
						sectionDescriptor->expirationTime = 
							section->expirationTime;
						sectionDescriptor->lastUpdate = 
							section->lastUpdateTime;
						sectionDescriptor->sectionSize = 
							section->dataLength[section->dataIndex];
						sectionDescriptor->sectionState = 
							section->dataState;
						memcpy(p,sectionDescriptor,sizeof(SaCkptSectionDescriptorT));
						p += sizeof(SaCkptSectionDescriptorT);
						if(section->sectionID.idLen>0){
							memcpy(p,section->sectionID.id,section->sectionID.idLen);
							p+=  section->sectionID.idLen;
						}
					}/*if(secListPass == 0) */
				}
				
				list = list->next;
			}/* while (list != NULL) */
		}/* for(secListPass  */
		ckptResp->resp->dataLength = secListTotalSize + sizeof(int);
		ckptResp->resp->retVal = SA_OK;
		SaCkptResponseSend(&ckptResp);
		break;

	case REQ_CKPT_ULNK:
		ckptReq->operation = OP_CKPT_ULNK;
		ckptMsg = SaCkptMessageCreateReq(ckptReq, M_CKPT_UNLINK_BCAST);
		SaCkptMessageBroadcast(ckptMsg);
		SaCkptFree((void**)&ckptMsg);
		
		ckptResp->resp->retVal = SA_OK;
		SaCkptResponseSend(&ckptResp);

		break;

	default:
		cl_log(LOG_INFO, "Not implemented request");
		
		ckptResp->resp->retVal = SA_ERR_FAILED_OPERATION;
		SaCkptResponseSend(&ckptResp);
		break;
	}

	if (ckptResp != NULL) {
		if (ckptResp->resp != NULL) {
			if (ckptResp->resp->data != NULL) {
				SaCkptFree((void**)&(ckptResp->resp->data));
			}
			SaCkptFree((void**)&(ckptResp->resp));
		}
		SaCkptFree((void*)&ckptResp);
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
	SaCkptFree((void*)&strReq);

	ckptResp = SaCkptResponseCreate(ckptReq);
	
	switch (ckptReq->clientRequest->req) {
	case REQ_CKPT_OPEN:
	case REQ_CKPT_OPEN_ASYNC:
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
			notifyLowPrioNode(openParam);
			removeOpenPendingQueue(ckptReq->clientRequest->reqParam);

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

	while (clientChannel->ops->is_message_pending(clientChannel) 
		!= TRUE) {
		cl_shortsleep();
	}

	/* receive ipc message */
	rc = clientChannel->ops->recv(clientChannel, &ipcMsg);
	if (rc != IPC_OK) {
/*		cl_log(LOG_ERR, "Receive error request"); */
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
		free(ipcMsg->msg_body);
	}
	free(ipcMsg);

	if (saCkptService->flagVerbose) {
		strReq = SaCkptReq2String(clientRequest->req);
		cl_log(LOG_INFO, 
			"<<<---");
		cl_log(LOG_INFO,
			"Receive request %lu (%s), client %d",
			clientRequest->requestNO, strReq,
			clientRequest->clientHandle);
		SaCkptFree((void*)&strReq);
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

	GList* list = NULL;

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

	if (client == NULL) {
		*pCkptReq = NULL;

		return HA_OK;
	}
	
	/* start pending request */
	list = client->pendingRequestList;
	if (list != NULL) {
		ckptReq = (SaCkptRequestT*)list->data;

		if (ckptReq != NULL) {
			SaCkptReplicaT* replica = NULL;
			replica = ckptReq->openCkpt->replica;

			if ((replica != NULL) &&
				(replica->flagReplicaPending == FALSE)) {
				client->pendingRequestList = 
					g_list_remove(
						client->pendingRequestList, 
						(gpointer)ckptReq);

				SaCkptRequestStart(ckptReq);
			}

			/* FIXME */
			/* How about the replica is NULL? */
		}
	}

	*pCkptReq = NULL;

	return HA_OK;
}

void 
SaCkptRequestStartTimer(SaCkptRequestT* ckptReq, SaTimeT timeout)
{
	char* strReq = NULL;

	/* to avoid start more than one timer */
	if (ckptReq->timeoutTag <= 0) {
		ckptReq->timeoutTag = Gmain_timeout_add(
			timeout / 1000000, 
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
			SaCkptFree((void*)&strReq);
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
			SaCkptFree((void*)&strReq);
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
	case REQ_CKPT_OPEN_ASYNC:
		strcpy(strTemp, "REQ_CKPT_OPEN_ASYNC");
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
	case REQ_CKPT_SYNC_ASYNC:
		strcpy(strTemp, "REQ_CKPT_SYNC_ASYNC");
		break;
	}

	strReq = (char*)SaCkptMalloc(strlen(strTemp)+1);
	if (strReq == NULL) {
		return NULL;
	}
	memcpy(strReq, strTemp, strlen(strTemp)+1);

	SaCkptFree((void*)&strTemp);

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
