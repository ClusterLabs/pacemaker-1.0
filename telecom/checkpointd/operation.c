/* $Id: operation.c,v 1.12 2005/03/16 17:11:15 lars Exp $ */
/* 
 * operation.c: 
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

/* 
 * operation timeout process 
 * usually, it will send out rollback message 
 */
gboolean
SaCkptOperationTimeout(gpointer timeout_data)
{
	SaCkptOperationT* ckptOp = (SaCkptOperationT*)timeout_data;
	SaCkptReplicaT* replica = ckptOp->replica;

	SaCkptMessageT* ckptMsg = NULL;
	
	char* strOp = NULL;

	strOp = SaCkptOp2String(ckptOp->operation);
	cl_log(LOG_INFO, "Replica %s operation %d (%s) timeout",
		replica->checkpointName,
		ckptOp->operationNO, strOp);
	SaCkptFree((void*)&strOp);

	if (ckptOp->state == OP_STATE_PENDING) {
		/* 
		 * if the operation is still not started
		 * remove it from the pending queue
		 */
		replica->pendingOperationList = 
			g_list_remove(
			replica->pendingOperationList,
			(gpointer)ckptOp);
		
		if (saCkptService->flagVerbose) {
			cl_log(LOG_INFO, 
				"Remove operation from pending list");
		}
	} else {
		/* if started, send rollback message */
		switch (ckptOp->operation) {
		case OP_RPLC_ADD:
			ckptMsg = SaCkptMessageCreateOp(ckptOp, 
				M_RPLC_ADD_ROLLBACK_BCAST);
			break;
			
		case OP_CKPT_UPD:
			ckptMsg = SaCkptMessageCreateOp(ckptOp, 
				M_CKPT_UPD_ROLLBACK_BCAST);
			break;
			
		default:
			break;
		}
		SaCkptMessageMulticast(ckptMsg, 
			ckptOp->stateList);
		SaCkptMessageDelete(&ckptMsg);
	}

	/* always return FALSE */
	return FALSE;
}

/* start the operation */
void 
SaCkptOperationStart(SaCkptOperationT* ckptOp)
{
	SaCkptReplicaT* replica = ckptOp->replica;
	SaCkptMessageT* ckptMsg = NULL;

	SaCkptStateT*	state = NULL;

	GList*	nodeList = NULL;
	GList*	list = NULL;
	int 	retVal = SA_OK;

	ckptMsg = SaCkptMessageCreateOp(ckptOp, M_CKPT_UPD);
	
	g_hash_table_insert(replica->operationHash,
		(gpointer)&(ckptOp->operationNO),
		(gpointer)ckptOp);

	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO, 
			"Remove operation from pending list");
	}
	
	ckptOp->state = OP_STATE_STARTED;
	
	replica->flagReplicaLock = TRUE;
	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO,
			"Replica %s locked",
			replica->checkpointName);
	}
	
	switch (ckptOp->operation) {
	case OP_RPLC_CRT:
		SaCkptReplicaPack(&(ckptMsg->data),
			&(ckptMsg->dataLength), replica);
		ckptMsg->msgSubtype = M_RPLC_CRT_REPLY;
		SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);

		break;

	case OP_CKPT_UPD:
		if (replica->createFlag & 
			SA_CKPT_WR_ACTIVE_REPLICA) {
			/* update active replica */
			retVal = SaCkptReplicaUpdate(replica, 
				ckptMsg->clientRequest,
				ckptMsg->dataLength,
				ckptMsg->data,
				ckptMsg->paramLength,
				ckptMsg->param);
		
			ckptMsg->msgSubtype = M_CKPT_UPD_REPLY;
			ckptMsg->retVal = retVal;
			SaCkptMessageSend(ckptMsg, 
				ckptMsg->clientHostName);
		
			if (retVal != SA_OK) {
				break;
			}
		
			/* send msg to all the nodes except itself */
			ckptMsg->msgSubtype = M_CKPT_UPD_BCAST;
		
			nodeList = g_list_copy(replica->nodeList);
			list = nodeList;
			while (list != NULL) {
				state = (SaCkptStateT*)list->data;
				if (!strcmp(state->nodeName, 
					saCkptService->nodeName)) {
					nodeList = g_list_remove(
						nodeList,
						(gpointer)state);
					break;
				}
				
				list = list->next;
			}
			cl_log(LOG_INFO,"-------Before we send out multicast");
			
			SaCkptMessageMulticast(ckptMsg, nodeList);
			cl_log(LOG_INFO,"-------we send out multicast");
			
			g_list_free(nodeList);
			cl_log(LOG_INFO,"-------we free nodelist");
		
			replica->flagReplicaLock = FALSE;
			if (saCkptService->flagVerbose) {
				cl_log(LOG_INFO,
					"Replica %s unlocked",
					replica->checkpointName);
			}

			SaCkptOperationRemove(&ckptOp);
		
		}else {
			ckptMsg->msgSubtype = M_CKPT_UPD_PREPARE_BCAST;
			SaCkptMessageMulticast(ckptMsg, 
				ckptOp->stateList);
		}
		break;

	case OP_CKPT_READ:
		retVal = SaCkptReplicaRead(replica, 
			&(ckptMsg->dataLength),
			&(ckptMsg->data),
			ckptMsg->paramLength,
			ckptMsg->param);
		
		ckptMsg->retVal = retVal;
		ckptMsg->msgSubtype = M_CKPT_READ_REPLY;
		SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);

		replica->flagReplicaLock = FALSE;
		if (saCkptService->flagVerbose) {
			cl_log(LOG_INFO,
				"Replica %s unlocked",
				replica->checkpointName);
		}

		SaCkptOperationRemove(&ckptOp);
			
		break;

	case OP_CKPT_SYNC:
		ckptMsg->retVal = SA_OK;
		ckptMsg->msgSubtype = M_CKPT_SYNC_REPLY;
		SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);
		
		SaCkptOperationRemove(&ckptOp);
		
		break;

	case OP_CKPT_ACT_SET:
		replica->flagReplicaPending = TRUE;
		cl_log(LOG_INFO,
			"Replica %s stop sending requests",
			replica->checkpointName);

		ckptMsg->retVal = SA_OK;
		ckptMsg->msgSubtype = M_CKPT_ACT_SET_BCAST_REPLY;
		SaCkptMessageSend(ckptMsg, 
			ckptMsg->clientHostName);

		SaCkptOperationRemove(&ckptOp);

		break;
	default:
		break;
		
	}

	return;
}

void 
SaCkptOperationContinue(SaCkptOperationT* ckptOp)
{
	cl_log(LOG_INFO, "OpContinue: not implemented");	
	return;
}

/* remove the operation and free its memory */
int 
SaCkptOperationRemove(SaCkptOperationT** pCkptOp)
{
	SaCkptOperationT* ckptOp = *pCkptOp;
	SaCkptReplicaT* replica = ckptOp->replica;
	
	GList* list = NULL;

	SaCkptOperationStopTimer(ckptOp);
		
	/* remove op from operation queue */
	g_hash_table_remove(replica->operationHash, 
		(gpointer)&(ckptOp->operationNO));

	if (replica->flagReplicaLock == FALSE) {
		/* start pending operation */
		list = replica->pendingOperationList;
		if (list != NULL) {
			SaCkptOperationT* op = NULL;

			op = (SaCkptOperationT*)list->data;
			if (op != NULL) {
				replica->pendingOperationList = 
					g_list_remove(
					replica->pendingOperationList, 
					(gpointer)op);
				
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO, 
					"Remove operation from pending list");
				}
				
				SaCkptOperationStart(op);
			}
		}
	}

	/* free the operation */
	if (ckptOp->paramLength > 0) {
		SaCkptFree((void**)&(ckptOp->param));
	}

	if (ckptOp->dataLength > 0) {
		SaCkptFree((void**)&(ckptOp->data));
	}

	list = ckptOp->stateList;
	while (list != NULL) {
		SaCkptFree((void**)&(list->data));
		list = list->next;
	}
	g_list_free(ckptOp->stateList);
	
	SaCkptFree((void*)&ckptOp);

	*pCkptOp = NULL;

	/*
	 * if the replica reference count is zero and the timeout flag
	 * is true, delete the replica
	 */
	if ((replica->referenceCount == 0) &&
		(replica->flagRetentionTimeout == TRUE)) {
		SaCkptReplicaRemove(&replica);
	}

	return HA_OK;
}

/* start timer for the operation */
void 
SaCkptOperationStartTimer(SaCkptOperationT* ckptOp)
{
	char* strOp = NULL;
	
	if (ckptOp->timeoutTag <= 0) {
		ckptOp->timeoutTag = Gmain_timeout_add(
			OPERATION_TIMEOUT * 1000000, 
			SaCkptOperationTimeout, 
			(gpointer)ckptOp);

		if (saCkptService->flagVerbose) {
			strOp = SaCkptOp2String(ckptOp->operation);
			cl_log(LOG_INFO, 
				"Start timer %u for op %d (%s)",
				ckptOp->timeoutTag, 
				ckptOp->operationNO, strOp);
			SaCkptFree((void*)&strOp);
		}
	}

	return;
}

/* stop the timer for the operation */
void 
SaCkptOperationStopTimer(SaCkptOperationT* ckptOp)
{
	char* strOp = NULL;
	if (ckptOp->timeoutTag > 0) {
		if (saCkptService->flagVerbose) {
			strOp = SaCkptOp2String(ckptOp->operation);
			cl_log(LOG_INFO, 
				"delete timer %u for op %d (%s)",
				ckptOp->timeoutTag,
				ckptOp->operationNO, strOp);
			SaCkptFree((void*)&strOp);
		}
		
		g_source_remove(ckptOp->timeoutTag);
		ckptOp->timeoutTag = 0;
	}

	return;
}

/* whether the operation finished or not */
int
SaCkptOperationFinished(char* fromNodeName, SaCkptOpStateT opState, GList* list)
{
	int finished = TRUE;
	SaCkptStateT* state = NULL;

	while (list != NULL) {
		state = (SaCkptStateT*)list->data;
		if (!strcmp(state->nodeName, fromNodeName)) {
			state->state = opState;
		} else {
			/*
			 * Shouldn't state->state be the same type
			 * as opState?  FIXME or comment me
			 */
			if ((SaCkptOpStateT)state->state != opState) {
				finished = FALSE;
			}
		}

		list = list->next;
	}

	return finished;
}


char* 
SaCkptOp2String(SaCkptOpT op)
{
	char* strOp = NULL;
	char*  strTemp = NULL;

	strTemp = (char*)SaCkptMalloc(64);
	SACKPTASSERT(strTemp != NULL);

	switch (op) {
	case OP_NULL:
		strcpy(strTemp, "OP_NULL");
		break;
	case OP_CKPT_OPEN:
		strcpy(strTemp, "OP_CKPT_OPEN");
		break;
	case OP_RPLC_CRT:
		strcpy(strTemp, "OP_RPLC_CRT");
		break;
	case OP_RPLC_ADD:
		strcpy(strTemp, "OP_RPLC_ADD");
		break;
	case OP_CKPT_UPD:
		strcpy(strTemp, "OP_CKPT_UPD");
		break;
	case OP_CKPT_READ:
		strcpy(strTemp, "OP_CKPT_READ");
		break;
	case OP_CKPT_ULNK:
		strcpy(strTemp, "OP_CKPT_ULNK");
		break;
	case OP_CKPT_SYNC:
		strcpy(strTemp, "OP_CKPT_SYNC");
		break;
	case OP_CKPT_ACT_SET:
		strcpy(strTemp, "OP_CKPT_ACT_SET");
		break;
	}

	strOp = (char*)SaCkptMalloc(strlen(strTemp)+1);
	if (strOp == NULL) {
		return NULL;
	}
	memcpy(strOp, strTemp, strlen(strTemp)+1);

	SaCkptFree((void*)&strTemp);

	return strOp;
}

/* Continue the operation after a node failure*/
void 
SaCkptOperationNodeFailure(gpointer key, 
	gpointer value, 
	gpointer userdata)
{
	SaCkptOperationT* ckptOp = NULL;
	SaCkptMessageT* ckptMsg = NULL;
	SaCkptReplicaT* replica = NULL;
	SaCkptStateT* state = NULL;
	char* strNodeName = NULL;
	GList* list = NULL;
	int finished = TRUE;

	int opState = -1;

	ckptOp = (SaCkptOperationT*)value;
	replica = ckptOp->replica;
	strNodeName = (char*)userdata;

	/* 
	 * if the operation is started by the failed node
	 * remove the operation directly
	 */
	if (!strcmp(ckptOp->clientHostName, strNodeName)) {
		SaCkptOperationRemove(&ckptOp);

		return;
	}

	/* only OP_RPLC_ADD and OP_CKPT_UPD need to be processed */
	if ((ckptOp->operation == OP_RPLC_ADD) ||
		(ckptOp->operation == OP_CKPT_UPD)) {

		list = ckptOp->stateList;
		finished = TRUE;
		while (list != NULL) {
			state = (SaCkptStateT*)list->data;
			if (state == NULL) {
				list = list->next;
				continue;
			}
			if (!strcmp(state->nodeName, strNodeName)) {
				ckptOp->stateList = g_list_remove(
					ckptOp->stateList,
					(gpointer)state);
				list = ckptOp->stateList;
				continue;
			} else {
				if (opState == -1) {
					opState = state->state;
				} else {
					if (opState != state->state) {
						finished = FALSE;
					}
				}
			}
			
			list = list->next;
		}

		if (finished) {
			switch(opState) {
			case OP_STATE_PREPARED:
				ckptOp->state = OP_STATE_PREPARED;
				
				/* broadcast commit message */
				if (ckptOp->operation == OP_RPLC_ADD) {
					ckptMsg = SaCkptMessageCreateOp(ckptOp,
						M_RPLC_ADD_COMMIT_BCAST);
				} else {
					ckptMsg = SaCkptMessageCreateOp(ckptOp,
						M_CKPT_UPD_COMMIT_BCAST);
				}
				SaCkptMessageMulticast(ckptMsg, 
					ckptOp->stateList);
				SaCkptFree((void*)&ckptMsg);
				
				break;
				
			case OP_STATE_COMMITTED:
				/* send back response */
				ckptOp->state = OP_STATE_COMMITTED;

				if (ckptOp->operation == OP_RPLC_ADD) {
					ckptMsg = SaCkptMessageCreateOp(ckptOp,
						M_RPLC_ADD_REPLY);
				} else {
					ckptMsg = SaCkptMessageCreateOp(ckptOp,
						M_CKPT_UPD_REPLY);
				}
				SaCkptMessageSend(ckptMsg, 
					ckptOp->clientHostName);
				SaCkptFree((void*)&ckptMsg);

				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}

				SaCkptOperationRemove(&ckptOp);
				break;

			case OP_STATE_ROLLBACKED:
				/* send back response */
				ckptOp->state = OP_STATE_ROLLBACKED;

				if (ckptOp->operation == OP_RPLC_ADD) {
					ckptMsg = SaCkptMessageCreateOp(ckptOp,
						M_RPLC_ADD_REPLY);
				} else {
					ckptMsg = SaCkptMessageCreateOp(ckptOp,
						M_CKPT_UPD_REPLY);
				}
				ckptMsg->retVal = SA_ERR_FAILED_OPERATION;
				SaCkptMessageSend(ckptMsg, 
					ckptOp->clientHostName);
				SaCkptFree((void*)&ckptMsg);

				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}

				SaCkptOperationRemove(&ckptOp);

				break;
				
			default:
				break;
			}
		}
	}

	return;
}

SaCkptResponseT* 
createLocalReplical(SaCkptRequestT* ckptReq){
	
	SaCkptResponseT* ckptResp = NULL;
	SaCkptOpenCheckpointT* openCkpt = ckptReq->openCkpt;
	SaCkptClientT* client = ckptReq->client;
	SaCkptReplicaT* replica = NULL;


	SaCkptReqOpenParamT* openParam = NULL;

	char* strReq = NULL;
	
	if(ckptReq == NULL) {
		cl_log(LOG_ERR,"NULL ckptReq in createLocalReplica");
		return NULL;
	}
	
	if(ckptReq->clientRequest->req != REQ_CKPT_OPEN &&
			ckptReq->clientRequest->req != REQ_CKPT_OPEN_ASYNC){
		cl_log(LOG_ERR,"Not open req in createLocalReplica");
		return NULL;
	}

	strReq = SaCkptReq2String(ckptReq->clientRequest->req);
	cl_log(LOG_INFO, 
		"client %d, request %lu (%s), create a local replica", 
		ckptReq->clientRequest->clientHandle,
		ckptReq->clientRequest->requestNO, strReq);
	SaCkptFree((void*)&strReq);

	ckptResp = SaCkptResponseCreate(ckptReq);
	
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
	return ckptResp;
}

void
updateReplicaPendingOption(SaCkptReplicaT *replica, const char *hostName){
	GList * list = NULL;
	GList * opStateList = NULL;
	SaCkptOperationT* ckptOp = NULL;
	SaCkptStateT* state = NULL;
	gboolean nodeExisted = FALSE;
	list = replica->pendingOperationList;
	
	while(list != NULL){
		ckptOp = (SaCkptOperationT*)list->data;
		opStateList = ckptOp->stateList;
		nodeExisted = FALSE;
		/* we should check if this node name exist already, maybe just for debug*/
		while(opStateList != NULL){
			state = (SaCkptStateT* )opStateList->data;
			if(!strncmp(state->nodeName, hostName,SA_MAX_NAME_LENGTH )){
				nodeExisted = TRUE;
				cl_log(LOG_ERR, "hostName %s exist already in updateReplicaPendingOption",hostName);
				break;
			}
			opStateList = opStateList->next;
		}
		
		if(!nodeExisted){
			opStateList = ckptOp->stateList;
			state = (SaCkptStateT*)ha_malloc(sizeof(SaCkptStateT));
			strncpy(state->nodeName,hostName,SA_MAX_NAME_LENGTH);
			state->nodeName[SA_MAX_NAME_LENGTH-1]='\0';
			state->state = OP_STATE_STARTED;
			ckptOp->stateList = g_list_append(ckptOp->stateList,
				(gpointer)state);
		}
		list = list->next;
	}
}
