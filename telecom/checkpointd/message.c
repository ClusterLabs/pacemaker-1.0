/* $Id: message.c,v 1.15 2005/03/16 17:11:15 lars Exp $ */
/* 
 * message.c
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <glib.h>

#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/base64.h>
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

#undef CKPTDEBUG 

extern SaCkptServiceT* saCkptService;

#define SACKPTMESSAGEVALIDATEREQ(ckptMsg) 		\
{							\
	client = g_hash_table_lookup(			\
		saCkptService->clientHash,		\
		(gpointer)&(ckptMsg->clientHandle));	\
	if (client == NULL) {				\
		cl_log(LOG_ERR, 			\
			"No client, ignore message");	\
		break;					\
	}						\
							\
	ckptReq= g_hash_table_lookup(			\
		client->requestHash,			\
		(gpointer)&(ckptMsg->clientRequestNO));	\
	if (ckptReq == NULL) {				\
		cl_log(LOG_ERR, 			\
			"No request, ignore message");	\
		break;					\
	}						\
}

#define SACKPTMESSAGEVALIDATEOP(ckptMsg)		\
{							\
	if (replica == NULL) {				\
		cl_log(LOG_ERR,			\
			"No replica, ignore message");	\
	}						\
							\
	if (!(replica->flagIsActive)) {			\
		cl_log(LOG_ERR, 			\
			"Replica is not active, ignore message");	\
		break;					\
	}						\
							\
	ckptOp = g_hash_table_lookup(			\
		replica->operationHash,			\
		(gconstpointer)&(ckptMsg->operationNO));	\
	if (ckptOp == NULL) {				\
		cl_log(LOG_ERR, 			\
			"No operation, ignore message");	\
		break;					\
	}						\
}

/* checkpoint message process routine */
gboolean
SaCkptClusterMsgProcess()
{
	SaCkptMessageT* ckptMsg = NULL;

	SaCkptReplicaT* replica = NULL;
	SaCkptClientT* client = NULL;
	SaCkptOpenCheckpointT* openCkpt = NULL;
	SaCkptRequestT* ckptReq = NULL;
	SaCkptResponseT* ckptResp = NULL;
	SaCkptOperationT* ckptOp = NULL;

	SaCkptReqOpenParamT* openParam = NULL;
	SaCkptReqCloseParamT* closeParam = NULL;
	SaCkptCheckpointCreationAttributesT*	attr = NULL;
	SaCkptReqUlnkParamT* unlinkParam = NULL;
	SaNameT*	unlinkName = NULL;

	SaCkptStateT*	state = NULL;
	saOpenResponseTypeT openRespType ;
	int	checkpointHandle;

	GList*	list = NULL;
	GList* 	nodeList = NULL;
	int	finished = TRUE;
	saOpenResponseTypeT	updateOpenProcessRes = -1;
	SaErrorT	retVal;

	ckptMsg = SaCkptMessageReceive();
	if (ckptMsg == NULL) {
		return FALSE;
	}

	if (!strcmp(ckptMsg->msgType, T_CKPT)) {
		/* FIXME: different version should be work together  */
		if ( SaCkptVersionCompare(ckptMsg->msgVersion, 
			saCkptService->version) != 0) {
			cl_log(LOG_ERR, 
				"Bad message version\n");
			return FALSE;
		}

		if (ckptMsg->checkpointName[0] != 0) {
			replica = (SaCkptReplicaT*)g_hash_table_lookup(
				saCkptService->replicaHash, 
				(gconstpointer)ckptMsg->checkpointName); 
		}

		switch (ckptMsg->msgSubtype) {
		/*
		 * Sent out when checkpoint service started
		 */
		case M_CKPT_CREATED:
			receiveCkptCreateMsg(ckptMsg);
			break;
		/*
		 * Response for M_CKPT_CREATED
		 */
		case M_CKPT_CREATED_REPLY:
			receiveCkptCreateReplyMsg(ckptMsg);
			break;
		/*
		 * unlink the checkpoint
		 * add its name to the unlinkCheckpointHash
		 */
		case M_CKPT_UNLINK_BCAST:
			unlinkParam = ckptMsg->param;

			unlinkName = g_hash_table_lookup(
				saCkptService->unlinkedCheckpointHash,
				unlinkParam->ckptName.value);
			if (unlinkName != NULL) {
				cl_log(LOG_INFO, 
				"Name %s has already been in unlink hashtable",
				unlinkParam->ckptName.value);
				break;
			}

			unlinkName = SaCkptMalloc(sizeof(SaNameT));
			if (unlinkName == NULL) {
				cl_log(LOG_ERR, "No memory in daemon");
				break;
			} 
			
			unlinkName->length = 
				unlinkParam->ckptName.length;
			g_hash_table_insert(
				saCkptService->unlinkedCheckpointHash,
				(gpointer)(unlinkParam->ckptName.value), 
				(gpointer)unlinkName);
			cl_log(LOG_INFO, 
				"Name %s is added into unlink hash table",
				unlinkParam->ckptName.value);
			break;

		/* 
		 * the first checkpoint message 
		 * before create local checkpoint, it has to broadcast this
		 * message. if no reply after timeout, it can create, or else
		 * it will copy the data from active checkpoint
		 */
		case M_CKPT_OPEN_BCAST:
		
			if(isLoopMessage(ckptMsg)){
				ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY_SELF;
				if(saCkptService->flagVerbose){
					cl_log(LOG_INFO,
					"self M_CKPT_OPEN_BCAST, ignore it\n");
				}
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
				break;
			}
			openParam = (SaCkptReqOpenParamT*)ckptMsg->param;
			
			if (replica == NULL) {
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
					"No replica\n");
				}
				if(isOnOpenProcess(openParam) == NULL){
					if (saCkptService->flagVerbose) {
						cl_log(LOG_INFO,
						"No replica and no local open request\n");
					}
					ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY_NO_REPLICA;
				}
				/*
			 	 * This node have already a open request with 
				 * the same name, so if it is a conflict open, 
				 * the high priority node will do it, 
				 * otherwise , the earlier one do it. 
				 * Just compare the node name for priority now
			 	 */
				else {
					updateOpenProcessQueue(ckptMsg, &updateOpenProcessRes);
					
					if(updateOpenProcessRes == RES_RACE_HIGH_PRIO){
						ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY_RACE_HIGH;
					}else if(updateOpenProcessRes == RES_RACE_LOW_PRIO){
						ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY_RACE_LOW;
					
					}else{
						ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY_EARLIER;
					}
				}
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
				break;
			}

			if (!replica->flagIsActive) {
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
					"Standby replica for open request\n");
				}
				ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY_STANDBY;
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
				break;	
			}


			/* 
			 * if the createattribute is not null and is 
			 * different from the replica, return error
			 */
			attr = &(openParam->attr);
			if ((attr->checkpointSize != 
				replica->maxCheckpointSize) ||
			    (attr->maxSectionIdSize !=
				replica->maxSectionIDSize) || 
			    (attr->maxSections != 
				replica->maxSectionNumber) ||
			    (attr->maxSectionSize !=
				replica->maxSectionSize) ||
			    (attr->creationFlags !=
			    	replica->createFlag)) {
				cl_log(LOG_ERR, 
					"create attribute is different");
				
				ckptMsg->msgSubtype = 
					M_CKPT_OPEN_BCAST_REPLY;
				ckptMsg->retVal = 
					SA_ERR_EXIST;
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
				break;
			}
			
			/* if replica is unlinked, open should fail */
			if (replica->flagUnlink == TRUE) {
				cl_log(LOG_ERR, 
					"checkpoint %s has been unlinked",
					replica->checkpointName);
				ckptMsg->retVal = SA_ERR_INVALID_PARAM;
			} else {
				strcpy(ckptMsg->activeNodeName,
					saCkptService->nodeName);
			}
			
			ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY;
			SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);

			break;

		/* 
		 * the reply message to M_CKPT_OPEN_BCAST when active replica
		 * on sent node
		 */
		case M_CKPT_OPEN_BCAST_REPLY:
			if (replica != NULL) {
				cl_log(LOG_ERR, 
					"Replica exists, ignore message");
				break;
			}

			SACKPTMESSAGEVALIDATEREQ(ckptMsg);

			openParam = ckptReq->clientRequest->reqParam;

			updateOpenParamNodeStatus(openParam, 
				ckptMsg->fromNodeName, RES_HAVE_REPLICA);

			removeOpenPendingQueue(ckptReq->clientRequest->reqParam);
			if (ckptMsg->retVal != SA_OK) {
				ckptResp = SaCkptResponseCreate(ckptReq);
				ckptResp->resp->retVal = ckptMsg->retVal;
				ckptResp->resp->dataLength = 0;
				ckptResp->resp->data = NULL;
				
				SaCkptResponseSend(&ckptResp);
				
				break;
			}
			
			
			if (openParam->openFlag & 
				SA_CKPT_CHECKPOINT_COLOCATED) {
				ckptMsg->msgSubtype = M_RPLC_CRT;
				
				ckptReq->operation = OP_RPLC_CRT;
			} else {
				ckptMsg->msgSubtype = M_CKPT_OPEN_REMOTE;
			}
			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);

			break;
	
		/*
		 * No replica for M_CKPT_OPEN_BCAST on sent node
		 * ugly goto , may remove it ...
		 */
		case M_CKPT_OPEN_BCAST_REPLY_NO_REPLICA:
			openRespType = RES_NO_REPLICA;
			goto here;
		/*
		 * Standby replica for M_CKPT_OPEN_BCAST on sent node
		 */

		case M_CKPT_OPEN_BCAST_REPLY_STANDBY:
			openRespType = RES_STANDBY;
			goto here;
			
		/*
		 * A race condition for M_CKPT_OPEN_BCAST 
		 * sent node have high priority
		 */
		case M_CKPT_OPEN_BCAST_REPLY_RACE_HIGH:
			openRespType = RES_RACE_HIGH_PRIO;
			goto here;
			
		/*
		 * A race condition for M_CKPT_OPEN_BCAST 
		 * sent node have low priority
		 */
		case M_CKPT_OPEN_BCAST_REPLY_RACE_LOW:
			openRespType = RES_RACE_LOW_PRIO;
			goto here;
		/*
		 * The sent node have open on progress
		 * this node send M_CKPT_OPEN_BCAST_REPLY_NO_REPLICA for 
		 * sent node's M_CKPT_OPEN_BCAST
		 */
		case M_CKPT_OPEN_BCAST_REPLY_EARLIER:
			openRespType = RES_EARLIER;
			goto here;
			
		case M_CKPT_OPEN_BCAST_REPLY_SELF:
			openRespType = RES_SELF;
			/* Ugly goto */
			
here:			SACKPTMESSAGEVALIDATEREQ(ckptMsg);
			openParam = ckptReq->clientRequest->reqParam;
			if( NULL == isOnOpenProcess(openParam)){
			/* The reason not found may because it have receive reply from other side*/
				if(saCkptService->flagVerbose){
					cl_log(LOG_INFO,
					"open reponse miss\n");
				}	
				break;
			}else{
				if(saCkptService->flagVerbose){
					cl_log(LOG_INFO,
					"open reponse found\n");
				}
				updateOpenParamNodeStatus(
					openParam, ckptMsg->fromNodeName, 
					openRespType);

				if( openReqFinishedForLocalCreate(openParam)){
					SaCkptRequestStopTimer(ckptReq);
					ckptResp = createLocalReplical(ckptReq);
					notifyLowPrioNode(openParam);
					removeOpenPendingQueue(ckptReq->clientRequest->reqParam);
					SaCkptResponseSend(&ckptResp);
				}
				break;
			}	

		/* 
		 * if the client do not want to create a local copy of 
		 * the checkpoint, it will open the checkpoint remotely
		 */
		case M_CKPT_OPEN_REMOTE:
			if (replica == NULL) {
				cl_log(LOG_ERR,
					"No replica, ignore message");
				break;
			}
			
			if (!replica->flagIsActive) {
				cl_log(LOG_ERR,
					"Standby replica, ignore message");
				break;	
			}

			openParam = (SaCkptReqOpenParamT*)ckptMsg->param;

			/* 
			 * if the createattribute is not null and is 
			 * different from the replica, return error
			 */
			attr = &(openParam->attr);
			if ((attr->checkpointSize != 
				replica->maxCheckpointSize) ||
			    (attr->maxSectionIdSize !=
				replica->maxSectionIDSize) || 
			    (attr->maxSections != 
				replica->maxSectionNumber) ||
			    (attr->maxSectionSize !=
				replica->maxSectionSize) ||
			    (attr->creationFlags !=
			    	replica->createFlag)) {
				cl_log(LOG_ERR, 
					"create attribute is different");
				
				ckptMsg->msgSubtype = 
					M_CKPT_OPEN_REMOTE_REPLY;
				ckptMsg->retVal = 
					SA_ERR_EXIST;
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
				break;
			}
			
			openCkpt = SaCkptCheckpointOpen(NULL, replica, 
				openParam);
			SACKPTASSERT(openCkpt != NULL);
			strcpy(openCkpt->clientHostName, 
				ckptMsg->clientHostName);
			openCkpt->clientHandle = ckptMsg->clientHandle;

			ckptMsg->msgSubtype = M_CKPT_OPEN_REMOTE_REPLY;
			ckptMsg->dataLength = 
				sizeof(openCkpt->checkpointHandle);
			ckptMsg->data = SaCkptMalloc(
				sizeof(openCkpt->checkpointHandle));
			SACKPTASSERT(ckptMsg->data != NULL);
			memcpy(ckptMsg->data,
				&(openCkpt->checkpointHandle),
				sizeof(openCkpt->checkpointHandle));

			SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);
			
			break;

		/*
		 * the reply message to M_CKPT_OPEN_REMOTE
		 */
		case M_CKPT_OPEN_REMOTE_REPLY:
			if (replica != NULL) {
				cl_log(LOG_ERR, 
					"Replica exists, ignore message");
				break;
			}

			SACKPTMESSAGEVALIDATEREQ(ckptMsg);
			SaCkptRequestStopTimer(ckptReq);
			

			if (ckptMsg->retVal != SA_OK) {
				ckptResp = SaCkptResponseCreate(ckptReq);
				ckptResp->resp->retVal = ckptMsg->retVal;
				SaCkptResponseSend(&ckptResp);
				break;
			}
	
			openParam = ckptReq->clientRequest->reqParam;
			openCkpt = SaCkptCheckpointOpen(client, NULL,
				openParam);
			strcpy(openCkpt->activeNodeName, 
				ckptMsg->activeNodeName);
			strcpy(openCkpt->checkpointName, 
				ckptMsg->checkpointName);
			openCkpt->checkpointRemoteHandle = 
				*(int*)(ckptMsg->data);
			ckptReq->openCkpt = openCkpt;
			
			ckptResp = SaCkptResponseCreate(ckptReq);
			ckptResp->resp->retVal = ckptMsg->retVal;
			
			ckptResp->resp->dataLength = 
				sizeof(SaCkptCheckpointHandleT);
			ckptResp->resp->data = 
				SaCkptMalloc(ckptResp->resp->dataLength);
			SACKPTASSERT(ckptResp->resp->data != NULL);
			memcpy(ckptResp->resp->data, 
				&(openCkpt->checkpointHandle), 
				ckptResp->resp->dataLength);

			SaCkptResponseSend(&ckptResp);

			break;

		/* 
		 * close the remotely opened checkpoint
		 */
		case M_CKPT_CLOSE_REMOTE:
			if (replica == NULL) {
				cl_log(LOG_ERR,
					"No replica, ignore message");
				break;
			}
			
			if (!replica->flagIsActive) {
				cl_log(LOG_ERR,
					"Standby replica, ignore message");
				break;	
			}

			checkpointHandle = *(int*)ckptMsg->data;

			openCkpt = (SaCkptOpenCheckpointT*)g_hash_table_lookup(
				saCkptService->openCheckpointHash,
				(gpointer)&checkpointHandle);
			if (openCkpt == NULL) {
				cl_log(LOG_ERR, 
					"No opencheckpoint, ignore message");
				break;
			}

			ckptMsg->retVal = SaCkptCheckpointClose(&openCkpt);
			ckptMsg->msgSubtype = M_CKPT_CLOSE_REMOTE_REPLY;

			SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);

			break;

		/* 
		 * the reply message to M_CKPT_CLOSE_REMOTE
		 */
		case M_CKPT_CLOSE_REMOTE_REPLY:
			if (replica != NULL) {
				cl_log(LOG_ERR, 
					"Replica exists, ignore message");
				break;
			}

			SACKPTMESSAGEVALIDATEREQ(ckptMsg);
			SaCkptRequestStopTimer(ckptReq);

			closeParam = ckptReq->clientRequest->reqParam;
			checkpointHandle = closeParam->checkpointHandle;
			openCkpt = g_hash_table_lookup(
				saCkptService->openCheckpointHash,
				(gpointer)&checkpointHandle);

			ckptResp = SaCkptResponseCreate(ckptReq);
			if (openCkpt == NULL) {
				ckptResp->resp->retVal= SA_ERR_BAD_HANDLE;
			} else {
				ckptResp->resp->retVal = 
					SaCkptCheckpointClose(&openCkpt);
			}

			SaCkptResponseSend(&ckptResp);

			break;

		/* 
		 * if the client want to create a local copy of the 
		 * checkpoint, it will send this message to ask the 
		 * active node to send it the data
		 */
		case M_RPLC_CRT:
			/* FIXME: */
			/* if the message size is exceed 1400, break */
			/* this message into several messages */
			
			if (replica == NULL) {
				cl_log(LOG_ERR,
					"No replica, ignore message");
				break;
			}
			
			if (!replica->flagIsActive) {
				cl_log(LOG_ERR,
					"Standby replica, ignore message");
				break;	
			}

			ckptOp = SaCkptOperationCreate(ckptMsg, replica);
			ckptOp->operation= OP_RPLC_CRT;
			if (replica->flagReplicaLock != TRUE){
				/* lock the replica first */
				replica->flagReplicaLock = TRUE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s locked",
						replica->checkpointName);
				}

				ckptOp->state = OP_STATE_STARTED;
				
				SaCkptReplicaPack(&(ckptMsg->data),
					&(ckptMsg->dataLength), 
					replica);
				ckptMsg->msgSubtype = M_RPLC_CRT_REPLY;
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);

				g_hash_table_insert(replica->operationHash,
					(gpointer)&(ckptOp->operationNO),
					(gpointer)ckptOp);
			} else {
				replica->pendingOperationList = 
					g_list_append(
					replica->pendingOperationList,
					(gpointer)ckptOp);
				
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO, 
					"Send operation to pending list");
				}
			}
			SaCkptOperationStartTimer(ckptOp);

			break;

		/* 
		 * the reply message to M_RPLC_CRT
		 * the active node send its data to the standby node via
		 * this message
		 */
		case M_RPLC_CRT_REPLY:
			if (replica != NULL) {
				cl_log(LOG_ERR, 
					"Replica exists, ignore message");
				break;
			}
			
			SACKPTMESSAGEVALIDATEREQ(ckptMsg);

			replica = SaCkptReplicaUnpack(ckptMsg->data, 
				ckptMsg->dataLength);
			g_hash_table_insert(saCkptService->replicaHash, 
				(gpointer)replica->checkpointName, 
				(gpointer)replica);
			
			SaCkptFree((void**)&(ckptMsg->data));
			ckptMsg->data = NULL;
			ckptMsg->dataLength = 0;
			ckptMsg->msgSubtype = M_RPLC_ADD;
			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);
			
			ckptReq->operation = OP_RPLC_ADD;
			
			break;

		/*
		 * after create the local copy of the checkpoint, it will send
		 * this message to active node. The active node then tell all 
		 * nodes to update their replica list
		 */
		case M_RPLC_ADD:
			SACKPTMESSAGEVALIDATEOP(ckptMsg);

			ckptOp->operation = OP_RPLC_ADD;
			
			state = (SaCkptStateT*)SaCkptMalloc( 
				sizeof(SaCkptStateT));
			SACKPTASSERT(state != NULL);
			state->state = OP_STATE_STARTED;
			strcpy(state->nodeName, ckptMsg->clientHostName);
			ckptOp->stateList = 
				g_list_append(ckptOp->stateList,
				(gpointer)state);

			ckptMsg->msgSubtype = M_RPLC_ADD_PREPARE_BCAST;
			SaCkptMessageMulticast(ckptMsg, 
				ckptOp->stateList);
			 
			break;

		/*
		 * the active node ask other node to prepare the update
		 */
		case M_RPLC_ADD_PREPARE_BCAST:
			if (replica == NULL) {
				cl_log(LOG_ERR, "No replica, ignore message");
				break;
			}

			state = (SaCkptStateT*)SaCkptMalloc( 
				sizeof(SaCkptStateT));
			if (state != NULL) {
				state->state = OP_STATE_PREPARED;
				strcpy(state->nodeName,
					ckptMsg->clientHostName);
				replica->nodeList = 
					g_list_append(replica->nodeList,
					(gpointer)state);
				/*should update the request on pending list*/
				updateReplicaPendingOption(replica, ckptMsg->clientHostName);
				ckptMsg->retVal = SA_OK;
			} else {
				ckptMsg->retVal = SA_ERR_NO_MEMORY;
			}
			ckptMsg->msgSubtype = M_RPLC_ADD_PREPARE_BCAST_REPLY;

			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);

			break;

		/*
		 * the reply message to M_RPLC_ADD_PREPARE_BCAST
		 * the standby nodes tell the active node whether the 
		 * preparation is successfull or not
		 */
		case M_RPLC_ADD_PREPARE_BCAST_REPLY:
			SACKPTMESSAGEVALIDATEOP(ckptMsg);

			if (ckptOp->state != OP_STATE_STARTED) {
				cl_log(LOG_INFO, 
					"Op state error, ignore message");
				break;
			}

			if (ckptMsg->retVal != SA_OK) {
				finished = TRUE;
				ckptOp->state = OP_STATE_ROLLBACKED;
			} else {
				finished = SaCkptOperationFinished(
					ckptMsg->fromNodeName, 
					OP_STATE_PREPARED, 
					ckptOp->stateList);
				if (finished == TRUE) {
					ckptOp->state = OP_STATE_PREPARED;
				}
			}

			if (finished == TRUE) {
				if (ckptOp->state == OP_STATE_ROLLBACKED) {
					ckptMsg->msgSubtype = 
						M_RPLC_ADD_ROLLBACK_BCAST;
				} 
				if (ckptOp->state == OP_STATE_PREPARED) {
					ckptMsg->msgSubtype = 
						M_RPLC_ADD_COMMIT_BCAST;
				} 
				SaCkptOperationStopTimer(ckptOp);
				SaCkptMessageMulticast(ckptMsg, 
					ckptOp->stateList);
			}

			break;

		/*
		 * if all the nodes prepared successfully, the active node
		 * tell them to commit the update
		 */
		case M_RPLC_ADD_COMMIT_BCAST:
			if (replica == NULL) {
				cl_log(LOG_INFO, 
					"No replica, ignore message");
				break;
			}

			list = replica->nodeList;
			while (list != NULL) {
				state = (SaCkptStateT*)list->data;
				if (!strcmp(state->nodeName,
					ckptMsg->clientHostName)) {
					state->state = 	OP_STATE_COMMITTED;
					break;
				}

				list = list->next;
			}

			ckptMsg->msgSubtype = M_RPLC_ADD_COMMIT_BCAST_REPLY;
			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);

			break;

		/*
		 * the reply message to M_RPLC_ADD_COMMIT_BCAST
		 * the commit will always success
		 */
		case M_RPLC_ADD_COMMIT_BCAST_REPLY:
			SACKPTMESSAGEVALIDATEOP(ckptMsg);
			
			if (ckptOp->state != OP_STATE_PREPARED) {
				cl_log(LOG_ERR, 
					"Op state error, ignore message");
				break;
			}

			finished = SaCkptOperationFinished(
				ckptMsg->fromNodeName, 
				OP_STATE_COMMITTED, 
				ckptOp->stateList);

			if (finished == TRUE) {
				ckptOp->state = OP_STATE_COMMITTED;
				
				ckptMsg->msgSubtype = M_RPLC_ADD_REPLY;
				SaCkptMessageSend(ckptMsg, 
					ckptOp->clientHostName);

				/* unlock the replica */
				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}

				SaCkptOperationRemove(&ckptOp);
			}

			break;

		/*
		 * the reply message to M_RPLC_ADD
		 * after got the reply, it will send response the client
		 * application
		 */
		case M_RPLC_ADD_REPLY:
			if (replica == NULL) {
				cl_log(LOG_ERR, 
					"No replica, ignore message");
				break;
			}

			SACKPTMESSAGEVALIDATEREQ(ckptMsg);
			SaCkptRequestStopTimer(ckptReq);
			
			ckptResp = SaCkptResponseCreate(ckptReq);
			ckptResp->resp->retVal = ckptMsg->retVal;
			if (ckptMsg->retVal != SA_OK) {
				SaCkptReplicaRemove(&replica);
			} else {
				replica->replicaState = STATE_CREATE_COMMITTED;
				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}
				replica->flagPendOperation = FALSE;
				
				openParam = ckptReq->clientRequest->reqParam;
				openCkpt = SaCkptCheckpointOpen(client, 
					replica, openParam);
				ckptReq->openCkpt = openCkpt;
				
				ckptResp->resp->dataLength = 
					sizeof(SaCkptCheckpointHandleT);
				ckptResp->resp->data = SaCkptMalloc(
					ckptResp->resp->dataLength);
				SACKPTASSERT(ckptResp->resp->data != NULL);
				memcpy(ckptResp->resp->data, 
					&(openCkpt->checkpointHandle), 
					ckptResp->resp->dataLength);
			}

			SaCkptResponseSend(&ckptResp);

			break;

		/*
		 * one or more nodes cannot update their replica list, so
		 * rollback the operation
		 */
		case M_RPLC_ADD_ROLLBACK_BCAST:
			if (replica == NULL) {
				cl_log(LOG_ERR, 
					"No replica, ignore message");
				break;
			}

			if (!strcmp(ckptMsg->clientHostName,
				saCkptService->nodeName)) {
				SaCkptReplicaRemove(&replica);
			} else {
				list = replica->nodeList;
				while (list != NULL) {
					state = (SaCkptStateT*)list->data;
					if (!strcmp(state->nodeName,
						ckptMsg->clientHostName)) {
						replica->nodeList = 
							g_list_remove(
							replica->nodeList,
							(gpointer)state);
						break;
					}

					list = list->next;
				}
			}

			ckptMsg->msgSubtype = M_RPLC_ADD_ROLLBACK_BCAST_REPLY;
			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);

			break;

		/* 
		 * the reply message to M_RPLC_ADD_ROLLBACK_BCAST
		 */
		case M_RPLC_ADD_ROLLBACK_BCAST_REPLY:
			SACKPTMESSAGEVALIDATEOP(ckptMsg);

			if (ckptOp->state != OP_STATE_ROLLBACKED) {
				cl_log(LOG_INFO, 
					"Op state error, ignore message");
				break;
			}

			finished = SaCkptOperationFinished(
				ckptMsg->fromNodeName, 
				OP_STATE_ROLLBACKED,
				ckptOp->stateList);
			if (finished == TRUE) {
				ckptOp->state = OP_STATE_ROLLBACKED;
				
				ckptMsg->msgSubtype = M_RPLC_ADD_REPLY;
				SaCkptMessageSend(ckptMsg, 
					ckptOp->clientHostName);

				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}

				SaCkptOperationRemove(&ckptOp);
			}

			break;

		/*
		 * after the checkpoint was closed and deleted on one node, 
		 * the other nodes have to update their replica list
		 */
		case M_RPLC_DEL_BCAST:
			if (replica == NULL) {
				cl_log(LOG_ERR, 
					"No replica, ignore message");
				break;
			}

			list = replica->nodeList;
			while (list != NULL) {
				state = (SaCkptStateT*)list->data;
				if (!strcmp(state->nodeName,
					ckptMsg->fromNodeName)) {
					replica->nodeList = 
						g_list_remove(
						replica->nodeList,
						(gpointer)state);
					break;
				}
				
				list = list->next;
			}

			if (!strcmp(replica->activeNodeName, 
				ckptMsg->fromNodeName)) {
				/* FIXME:  */
				/* got the active replica by election */
				/* the most updated replica will be the active  */
				/* replica */
				strcpy(replica->activeNodeName, 
					saCkptService->nodeName);
				replica->flagIsActive = TRUE;
				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}

#if 0
				/* FIXME:  */
				/* if there are started requests on this replica */
				/* return SA_ERR_TRY_AGAIN */
				g_hash_table_foreach(saCkptService->clientHash, 
					xxxx,
					(gpointer)replica);
#endif
			}
			
			break;

		/*
		 * Any update request will need to send this message to active
		 * node first
		 *
		 * if the checkpoint has SA_CKPT_WR_ACTIVE_REPLICA flag, the 
		 * active node will update and commit first, then send reply 
		 * to the client application, no matter the standby nodes
		 * can update or not. This is not two phase commit
		 *
		 * if the checkpoint has SA_CKPT_WR_ALL_REPLICAS flag, it will
		 * update the replicas on all the nodes via two phase commit
		 * algorithm
		 */
		case M_CKPT_UPD:
			if (replica == NULL) {
				cl_log(LOG_ERR,
					"No replica, ignore message");
				break;
			}
			
			if (!replica->flagIsActive) {
				cl_log(LOG_ERR,
					"Standby replica, ignore message");
				break;	
			}

			ckptOp = SaCkptOperationCreate(ckptMsg, replica);
			SACKPTASSERT (ckptOp!= NULL);
			
			if (replica->flagReplicaLock == TRUE) {
				replica->pendingOperationList = 
					g_list_append(
					replica->pendingOperationList,
					(gpointer)ckptOp);

				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO, 
					"Send operation to pending list");
				}
				
				SaCkptOperationStartTimer(ckptOp);
				
				break;
			}

			replica->flagReplicaLock = TRUE;
			if (saCkptService->flagVerbose) {
				cl_log(LOG_INFO,
					"Replica %s locked",
					replica->checkpointName);
			}
			
			/*
			 * if replica is opened with SA_CKPT_WR_ACTIVE_REPLICA
			 * update active replica and return
			 */
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
					replica->flagReplicaLock = FALSE;
					if (saCkptService->flagVerbose) {
						cl_log(LOG_INFO,
							"Replica %s unlocked",
							replica->checkpointName);
					}

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
				SaCkptMessageMulticast(ckptMsg, nodeList);
				g_list_free(nodeList);

				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}
				
			}else {
				g_hash_table_insert(replica->operationHash,
					(gpointer)&(ckptOp->operationNO),
					(gpointer)ckptOp);
				ckptOp->state = OP_STATE_STARTED;

				ckptMsg->msgSubtype = 
					M_CKPT_UPD_PREPARE_BCAST;
				SaCkptMessageMulticast(ckptMsg, 
					ckptOp->stateList);

				SaCkptOperationStartTimer(ckptOp);
			}

			break;

		/* 
		 * after the active node has updated and committed its 
		 * checkpoint, it will broadcast this message to ask 
		 * other standby node to update their checkpoint.
		 *
		 * if the update on the standby nodes is not successful, 
		 * the data of this checkpoint will be marked as invalid
		 *
		 * this is not two phase commit since the active node has
		 * already committed
		 *
		 * this message do not need reply
		 */
		case M_CKPT_UPD_BCAST:
			if (replica == NULL) {
				cl_log(LOG_INFO, 
					"No replica, ignore message");
				break;
			}

			SaCkptReplicaUpdate(replica,
				ckptMsg->clientRequest,
				ckptMsg->dataLength,
				ckptMsg->data,
				ckptMsg->paramLength,
				ckptMsg->param);

			/*
			 * update the standby replica
			 * FIXME:
			 */
			replica->nextOperationNumber = ckptMsg->operationNO + 1;

			/* do not send reply */
			
			break;

		/*
		 * the beginning of two phase commit algorithm
		 */
		case M_CKPT_UPD_PREPARE_BCAST:
			if (replica == NULL) {
				cl_log(LOG_INFO, 
					"No replica, ignore message");
				break;
			}
			
			retVal = SaCkptReplicaUpdPrepare(replica, 
				ckptMsg->clientRequest,
				ckptMsg->dataLength,
				ckptMsg->data,
				ckptMsg->paramLength,
				ckptMsg->param);

			ckptMsg->msgSubtype = M_CKPT_UPD_PREPARE_BCAST_REPLY;
			ckptMsg->retVal = retVal;
			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);

			break;

		/*
		 * the reply message of M_CKPT_UPD_PREPARE_BCAST
		 */
		case M_CKPT_UPD_PREPARE_BCAST_REPLY:
			SACKPTMESSAGEVALIDATEOP(ckptMsg);
			
			if (ckptOp->state != OP_STATE_STARTED) {
				cl_log(LOG_INFO, 
					"Op state error, ignore message");
				break;
			}
			
			if (ckptMsg->retVal != SA_OK) {
				finished = TRUE;
				ckptOp->state = 
					OP_STATE_ROLLBACKED;
			} else {
				finished = SaCkptOperationFinished(
					ckptMsg->fromNodeName, 
					OP_STATE_PREPARED, 
					ckptOp->stateList);
				if (finished == TRUE) {
					ckptOp->state = OP_STATE_PREPARED;
				}
			}
			
			if (finished == TRUE) {
				if (ckptOp->state == OP_STATE_ROLLBACKED) {
					ckptMsg->msgSubtype = 
						M_CKPT_UPD_ROLLBACK_BCAST;
				} 
				if (ckptOp->state == OP_STATE_PREPARED) {
					ckptMsg->msgSubtype = 
						M_CKPT_UPD_COMMIT_BCAST;
				} 
				SaCkptOperationStopTimer(ckptOp);
				SaCkptMessageMulticast(ckptMsg, 
					ckptOp->stateList);
			}
			
			break;

		/*
		 * the update commit broadcast message
		 */
		case M_CKPT_UPD_COMMIT_BCAST:
			if (replica == NULL) {
				cl_log(LOG_INFO, 
					"No replica, ignore message");
				break;
			}
			
			retVal = SaCkptReplicaUpdCommit(replica, 
				ckptMsg->clientRequest,
				ckptMsg->dataLength,
				ckptMsg->data,
				ckptMsg->paramLength,
				ckptMsg->param);

			replica->nextOperationNumber = ckptMsg->operationNO + 1;

			ckptMsg->msgSubtype = M_CKPT_UPD_COMMIT_BCAST_REPLY;
			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);

			break;

		/*
		 * the reply message to M_CKPT_UPD_COMMIT_BCAST
		 */
		case M_CKPT_UPD_COMMIT_BCAST_REPLY:
			SACKPTMESSAGEVALIDATEOP(ckptMsg);
			
			if (ckptOp->state != OP_STATE_PREPARED) {
				cl_log(LOG_INFO, 
					"Op state error, ignore message");
				break;
			}

			finished = SaCkptOperationFinished(
				ckptMsg->fromNodeName,
				OP_STATE_COMMITTED,
				ckptOp->stateList);

			if (finished == TRUE) {
				ckptMsg->msgSubtype = M_CKPT_UPD_REPLY;
				
				/* update reply message do not need data */
				if (ckptMsg->dataLength > 0) {
					SaCkptFree((void**)&(ckptMsg->data));
					ckptMsg->data = NULL;
					ckptMsg->dataLength = 0;
				}
				
				SaCkptMessageSend(ckptMsg,
					ckptMsg->clientHostName);
				
				/* unlock replica */
				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}
				
				SaCkptOperationRemove(&ckptOp);
			}
			
			break;

		/*
		 * the rollback message
		 */
		case M_CKPT_UPD_ROLLBACK_BCAST:
			if (replica == NULL) {
				cl_log(LOG_INFO, 
					"No replica, ignore message");
				break;
			}
			
			retVal = SaCkptReplicaUpdRollback(replica,
				ckptMsg->clientRequest,
				ckptMsg->dataLength,
				ckptMsg->data,
				ckptMsg->paramLength,
				ckptMsg->param);

			ckptMsg->msgSubtype = M_CKPT_UPD_ROLLBACK_BCAST_REPLY;
			SaCkptMessageSend(ckptMsg, ckptMsg->activeNodeName);
			
			break;

		/*
		 * the reply message to M_CKPT_UPD_ROLLBACK_BCAST
		 */
		case M_CKPT_UPD_ROLLBACK_BCAST_REPLY:
			SACKPTMESSAGEVALIDATEOP(ckptMsg);

			if (ckptOp->state != OP_STATE_ROLLBACKED) {
				cl_log(LOG_INFO, 
					"Op state error, ignore message");
				break;
			}

			finished = SaCkptOperationFinished(
				ckptMsg->fromNodeName,
				OP_STATE_ROLLBACKED, 
				ckptOp->stateList);

			if (finished == TRUE) {
				ckptMsg->msgSubtype = M_CKPT_UPD_REPLY;
				
				/* update reply message do not need data */
				if (ckptMsg->dataLength > 0) {
					SaCkptFree((void**)&(ckptMsg->data));
					ckptMsg->data = NULL;
					ckptMsg->dataLength = 0;
				}
				
				SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);

				/* unlock replica */
				replica->flagReplicaLock = FALSE;
				if (saCkptService->flagVerbose) {
					cl_log(LOG_INFO,
						"Replica %s unlocked",
						replica->checkpointName);
				}

				SaCkptOperationRemove(&ckptOp);
			}

			break;

		/*
		 * The read (and also udpate) operation has to be sent to active
		 * node first, so all these operations can be serialized
		 */
		case M_CKPT_READ:
			if (replica == NULL) {
				cl_log(LOG_INFO,
					"No replica, ignore message");
				break;
			}
			
			if (!replica->flagIsActive) {
				cl_log(LOG_INFO,
					"Standby replica, ignore message");
				break;	
			}

			if (replica->flagReplicaLock != TRUE) {
				retVal = SaCkptReplicaRead(replica, 
					&(ckptMsg->dataLength),
					&(ckptMsg->data),
					ckptMsg->paramLength,
					ckptMsg->param);
				
				ckptMsg->retVal = retVal;
				ckptMsg->msgSubtype = M_CKPT_READ_REPLY;
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
			} else {
				ckptOp = SaCkptOperationCreate(
					ckptMsg, replica);
				replica->pendingOperationList = 
					g_list_append(
					replica->pendingOperationList,
					(gpointer)ckptOp);
				cl_log(LOG_INFO, 
					"Send read operation to pending list");
				
				SaCkptOperationStartTimer(ckptOp);
			}

			break;

		/*
		 * sync message 
		 * on receive this message, if the replica is not lock and no 
		 * other pending operations, the active replica reply it with
		 * OK. Else, the active replica add it to the the pending list
		 */
		case M_CKPT_SYNC:
			if (replica == NULL) {
				cl_log(LOG_INFO,
					"No replica, ignore message");
				break;
			}
			
			if (!replica->flagIsActive) {
				cl_log(LOG_INFO,
					"Standby replica, ignore message");
				break;	
			}

			if ((replica->flagReplicaLock != TRUE)  && 
				(g_list_length(
				replica->pendingOperationList) == 0)) {
				
				ckptMsg->retVal = SA_OK;
				ckptMsg->msgSubtype = M_CKPT_SYNC_REPLY;
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
			} else {
				ckptOp = SaCkptOperationCreate(
					ckptMsg, replica);
				replica->pendingOperationList = 
					g_list_append(
					replica->pendingOperationList,
					(gpointer)ckptOp);
				cl_log(LOG_INFO, 
					"Send sync operation to pending list");
				
				SaCkptOperationStartTimer(ckptOp);
			}

			break;

		/* 
		 * set active broadcast message
		 * on receive this message, all the replica stop sending 
		 * requests to the active replica.
		 */
		case M_CKPT_ACT_SET_BCAST:
			if (replica == NULL) {
				cl_log(LOG_INFO,
					"No replica, ignore message");
				break;
			}
			
			replica->flagReplicaPending = TRUE;
			cl_log(LOG_INFO,
				"Replica %s stop sending requests",
				replica->checkpointName);
			
			if (!replica->flagIsActive) {
				cl_log(LOG_INFO,
					"Standby replica, ignore message");
				break;	
			}

			if ((replica->flagReplicaLock != TRUE)  && 
				(g_list_length(
				replica->pendingOperationList) == 0)) {

				ckptMsg->retVal = SA_OK;
				ckptMsg->msgSubtype = M_CKPT_ACT_SET_BCAST_REPLY;
				SaCkptMessageSend(ckptMsg, 
					ckptMsg->clientHostName);
			} else {
				ckptOp = SaCkptOperationCreate(
					ckptMsg, replica);
				replica->pendingOperationList = 
					g_list_append(
					replica->pendingOperationList,
					(gpointer)ckptOp);
				cl_log(LOG_INFO, 
				"Send act_set operation to pending list");
				
				SaCkptOperationStartTimer(ckptOp);
			}

			break;

		case M_CKPT_ACT_SET_BCAST_REPLY:
			if (replica == NULL) {
				cl_log(LOG_INFO,
					"No replica, ignore message");
				break;
			}
			SACKPTMESSAGEVALIDATEREQ(ckptMsg);

			replica->flagIsActive = TRUE;
			strcpy(replica->activeNodeName,
				saCkptService->nodeName);
			cl_log(LOG_INFO, 
				"checkpoint %s is set as active replica",
				replica->checkpointName);
			
			replica->flagReplicaPending = FALSE;
			cl_log(LOG_INFO,
				"Replica %s resume sending requests",
				replica->checkpointName);
			
			ckptMsg->retVal = SA_OK;
			ckptMsg->msgSubtype = M_CKPT_ACT_SET_FINISH_BCAST;
			SaCkptMessageBroadcast(ckptMsg);

			/* stop timer and send back response */
			SaCkptRequestStopTimer(ckptReq);
			
			ckptResp = SaCkptResponseCreate(ckptReq);
			ckptResp->resp->retVal = ckptMsg->retVal;

			SaCkptResponseSend(&ckptResp);

			break;
			
		case M_CKPT_ACT_SET_FINISH_BCAST:
			if (replica == NULL) {
				cl_log(LOG_INFO,
					"No replica, ignore message");
				break;
			}

			if (strcmp(saCkptService->nodeName, 
				ckptMsg->clientHostName) == 0) {
				cl_log(LOG_INFO,
					"Send from itself, ignore message");
				break;
			}

			replica->flagIsActive = FALSE;
			strcpy(replica->activeNodeName,
				ckptMsg->clientHostName);
			cl_log(LOG_INFO, 
			"Active node of replica %s has been switched to %s",
			replica->checkpointName, replica->activeNodeName);

			replica->flagReplicaPending = FALSE;
			cl_log(LOG_INFO,
				"Replica %s resume sending requests",
				replica->checkpointName);
			
			break;	
			
		case M_CKPT_UPD_REPLY:
		case M_CKPT_READ_REPLY:
		case M_CKPT_SYNC_REPLY:
#if 0			
			if (replica == NULL) {
				cl_log(LOG_INFO, 
					"No replica, ignore message");
				break;
			}
#endif
			SACKPTMESSAGEVALIDATEREQ(ckptMsg);
			SaCkptRequestStopTimer(ckptReq);
			
			ckptResp = SaCkptResponseCreate(ckptReq);
			ckptResp->resp->retVal = ckptMsg->retVal;

			if ((ckptMsg->msgSubtype == M_CKPT_READ_REPLY) &&
				(ckptMsg->dataLength > 0)) {
				ckptResp->resp->dataLength = 
					ckptMsg->dataLength;
				ckptResp->resp->data = 
					SaCkptMalloc(ckptMsg->dataLength);
				SACKPTASSERT(ckptResp->resp->data != NULL);
				memcpy(ckptResp->resp->data,
					ckptMsg->data,
					ckptMsg->dataLength);
			} 

			SaCkptResponseSend(&ckptResp);

			break;

		default:
			
			break;
		}

	/*
	 * other heartbeat messages
	 */
	} else if(!strcmp(ckptMsg->msgType, T_STATUS)) {
		/* 
		 * if node is dead, remove it 
		 */
		if (!strcmp(ckptMsg->hamsgStatus, "dead")) {
			cl_log(LOG_INFO, 
				"Node %s dead", 
				ckptMsg->fromNodeName);
				
			/*
			 * for each replica, remove the dead node from its
			 * nodeList
			 */
			g_hash_table_foreach(saCkptService->replicaHash,
				SaCkptReplicaNodeFailure,
				(gpointer)ckptMsg->fromNodeName);
			/*
			 *FIXME update saCkptService->nodeStatus
			 */
			/* 
			 * for each sent client request, redo it since all the 
			 * operations are reentriable
			 */
			 g_hash_table_foreach(saCkptService->clientHash,
			 	SaCkptClientNodeFailure,
			 	(gpointer)ckptMsg->fromNodeName);
		}

	} else {
		cl_log(LOG_INFO, 
			"Unrecognized message %s ", ckptMsg->msgType);
	}
	
	SaCkptMessageDelete(&ckptMsg);

	/*
	 * if there are still other messages waiting for processing,
	 * continue to process them.
	 */
	SaCkptClusterMsgProcess();

	return TRUE;

}

/* 
 * receive message from heartbeat daemon 
 * it read message from the heartbeat daemon and 
 * convert it to ckpt message 
 */
SaCkptMessageT* 
SaCkptMessageReceive()
{
	ll_cluster_t	*hb		= saCkptService->heartbeat;
	struct ha_msg	*haMsg		= NULL;
	SaCkptMessageT	*ckptMsg 	= NULL;

	if (!hb->llc_ops->msgready(hb)) {
		return NULL;
	}
	
	haMsg = hb->llc_ops->readmsg(hb, TRUE);
	if (haMsg == NULL) {
		return NULL;
	}

#ifdef CKPTDEBUG	
	if (saCkptService->flagVerbose) {
		char *	msgString = NULL;
		
		msgString = msg2string(haMsg);
		cl_log(LOG_DEBUG, 
			"Receive message\n%s", 
			msgString);
		SaCkptFree((void**)&msgString);
	}
#endif	

	ckptMsg = SaHamsg2CkptMessage(haMsg);

	if (saCkptService->flagVerbose) {
		char* strSubtype = NULL;
		char* strErr = NULL;

		strSubtype = SaCkptMsgSubtype2String(ckptMsg->msgSubtype);
		strErr = SaCkptErr2String(ckptMsg->retVal);
		cl_log(LOG_INFO, 
			"Message from %s, type %s, subtype %s, status %s",
			ckptMsg->fromNodeName,
			ckptMsg->msgType, strSubtype, strErr);
		SaCkptFree((void*)&strSubtype);
		SaCkptFree((void*)&strErr);
	}
	
	ha_msg_del(haMsg);

	return ckptMsg;
}

/* convert ckpt messaage to Linux-HA message format */
struct ha_msg* 
SaCkptMessage2Hamsg(SaCkptMessageT* ckptMsg) {
	struct ha_msg	*haMsg = NULL;

	char *strVersion = NULL;
	char *strTemp = NULL;

	int 	rc;

	strVersion = (char*)SaCkptMalloc(32);
	SACKPTASSERT(strVersion != NULL);
	strTemp = (char*)SaCkptMalloc(MAXMSG);
	SACKPTASSERT(strTemp != NULL);
	
	haMsg = ha_msg_new(30);
	SACKPTASSERT(haMsg != NULL);
	
	rc = ha_msg_mod(haMsg, F_TYPE, T_CKPT);
	if (rc != HA_OK) {
		cl_log(LOG_ERR, 
			"Add field %s to hamsg failed",
			F_TYPE);
	}
	
	rc = ha_msg_mod(haMsg, F_ORIG, saCkptService->nodeName);
	if (rc != HA_OK) {
		cl_log(LOG_ERR, 
			"Add field %s to hamsg failed",
			F_ORIG);
	}
	
	strTemp[0] = 0;
	sprintf(strTemp, "%d", ckptMsg->msgSubtype);
	rc = ha_msg_mod(haMsg, F_CKPT_SUBTYPE, strTemp);
	if (rc != HA_OK) {
		cl_log(LOG_ERR, 
			"Add field %s to hamsg failed",
			F_CKPT_SUBTYPE);
	}
	
	SaCkptPackVersion(strVersion, &(ckptMsg->msgVersion));
	rc = ha_msg_mod(haMsg, F_CKPT_VERSION, strVersion);
	if (rc != HA_OK) {
		cl_log(LOG_ERR, 
			"Add field %s to hamsg failed",
			F_CKPT_VERSION);
	}
	
	if (ckptMsg->checkpointName[0] != 0) {
		rc = ha_msg_mod(haMsg, F_CKPT_CHECKPOINT_NAME, 
			ckptMsg->checkpointName);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_CHECKPOINT_NAME);
		}
	}
	
	if (ckptMsg->clientHostName[0] != 0) {
		rc = ha_msg_mod(haMsg, F_CKPT_CLIENT_HOSTNAME, 
			ckptMsg->clientHostName);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_CLIENT_HOSTNAME);
		}
	}
	
	if (ckptMsg->clientHandle > 0) {
		strTemp[0] = 0;
		sprintf(strTemp, "%d", ckptMsg->clientHandle);
		rc = ha_msg_mod(haMsg, F_CKPT_CLIENT_HANDLE, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_CLIENT_HANDLE);
		}
	}
	
	if (ckptMsg->clientRequest > 0) {
		strTemp[0] = 0;
		sprintf(strTemp, "%d", ckptMsg->clientRequest);
		rc = ha_msg_mod(haMsg, F_CKPT_CLIENT_REQUEST, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_CLIENT_REQUEST);
		}
	}
	
	if (ckptMsg->clientRequestNO > 0) {
		strTemp[0] = 0;
		sprintf(strTemp, "%d", ckptMsg->clientRequestNO);
		rc = ha_msg_mod(haMsg, F_CKPT_CLIENT_REQUEST_NO, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_CLIENT_REQUEST_NO);
		}
	}
	
	if (ckptMsg->activeNodeName[0] != 0) {
		rc = ha_msg_mod(haMsg, F_CKPT_ACTIVE_NODENAME, 
			ckptMsg->activeNodeName);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_ACTIVE_NODENAME);
		}
	}
	
	if (ckptMsg->operation > 0) {
		strTemp[0] = 0;
		sprintf(strTemp, "%d", ckptMsg->operation);
		rc = ha_msg_mod(haMsg, F_CKPT_OPERATION, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_OPERATION);
		}
	}
	
	if (ckptMsg->operationNO > 0) {
		strTemp[0] = 0;
		sprintf(strTemp, "%d", ckptMsg->operationNO);
		rc = ha_msg_mod(haMsg, F_CKPT_OPERATION_NO, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_OPERATION_NO);
		}
	}
	
	if (ckptMsg->paramLength > 0) {
		strTemp[0] = 0;
		sprintf(strTemp, "%d", (int)ckptMsg->paramLength);
		rc = ha_msg_mod(haMsg, F_CKPT_PARAM_LENGTH, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_PARAM_LENGTH);
		}
	
		strTemp[0] = 0;
		binary_to_base64(ckptMsg->param, ckptMsg->paramLength, 
			strTemp, MAXMSG);
		rc = ha_msg_mod(haMsg, F_CKPT_PARAM, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_PARAM);
		}
	}
	
	if (ckptMsg->dataLength > 0) {
		strTemp[0] = 0;
		sprintf(strTemp, "%d", (int)ckptMsg->dataLength);
		rc = ha_msg_mod(haMsg, F_CKPT_DATA_LENGTH, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_DATA_LENGTH);
		}
		
		strTemp[0] = 0;
		binary_to_base64(ckptMsg->data, ckptMsg->dataLength, 
			strTemp, MAXMSG);
		rc = ha_msg_mod(haMsg, F_CKPT_DATA, strTemp);
		if (rc != HA_OK) {
			cl_log(LOG_ERR, 
				"Add field %s to hamsg failed",
				F_CKPT_DATA);
		}
	}

	strTemp[0] = 0;
	sprintf(strTemp, "%d", ckptMsg->retVal);
	rc = ha_msg_mod(haMsg, F_CKPT_RETVAL, strTemp);
	if (rc != HA_OK) {
		cl_log(LOG_ERR, 
			"Add field %s to hamsg failed",
			F_CKPT_RETVAL);
	}

	SaCkptFree((void*)&strTemp);
	SaCkptFree((void*)&strVersion);

	return haMsg;
}

/* convert Linux-HA message to ckpt message */
SaCkptMessageT* 
SaHamsg2CkptMessage(struct ha_msg* haMsg)
{
	const char	*strType		= NULL;
	const char	*strSubtype		= NULL;
	const char	*strVersion		= NULL;
	const char	*strOrig		= NULL;
	const char	*strCheckpointName	= NULL;
	const char	*strActiveHostname	= NULL;
	const char	*strClientHostname	= NULL;
	const char	*strClientHandle	= NULL;
	const char	*strClientRequest	= NULL;
	const char	*strClientRequestNO	= NULL;
	const char	*strOperation		= NULL;
	const char	*strOperationNO 	= NULL;
	const char	*strParam		= NULL;
	const char	*strParamLength 	= NULL;
	const char	*strData		= NULL;
	const char	*strDataLength		= NULL;
	const char	*strRc		= NULL;
	
	const char	*strHostname		= NULL;
	const char	*strStatus		= NULL;
	
	SaCkptMessageT	*ckptMsg = NULL;
	
	ckptMsg = SaCkptMalloc(sizeof(SaCkptMessageT));
	if (ckptMsg == NULL) {
		return NULL;
	}
	
	strType = ha_msg_value(haMsg, F_TYPE);
	if (strType != NULL) {
		strcpy(ckptMsg->msgType, strType);
	}
	
	strSubtype = ha_msg_value(haMsg, F_CKPT_SUBTYPE);
	if (strSubtype != NULL) {
		ckptMsg->msgSubtype = atoi(strSubtype);
	}
	
	strVersion = ha_msg_value(haMsg, F_CKPT_VERSION);
	if (strVersion != NULL) {
		SaCkptUnpackVersion(strVersion, &(ckptMsg->msgVersion));
	}

	strOrig = ha_msg_value(haMsg, F_ORIG);
	if (strOrig != NULL) {
		strcpy(ckptMsg->fromNodeName, strOrig);
	}
	
	strCheckpointName = ha_msg_value(haMsg, F_CKPT_CHECKPOINT_NAME);
	if (strCheckpointName != NULL) {
		strcpy(ckptMsg->checkpointName, strCheckpointName);
	}
	
	strActiveHostname = ha_msg_value(haMsg, F_CKPT_ACTIVE_NODENAME);
	if (strActiveHostname != NULL) {
		strcpy(ckptMsg->activeNodeName, strActiveHostname);
	}
	
	strClientHostname = ha_msg_value(haMsg, F_CKPT_CLIENT_HOSTNAME);
	if (strClientHostname != NULL) {
		strcpy(ckptMsg->clientHostName, strClientHostname);
	}
	
	strClientHandle = ha_msg_value(haMsg, F_CKPT_CLIENT_HANDLE);
	if (strClientHandle != NULL) {
		ckptMsg->clientHandle = atoi(strClientHandle);
	}
	
	strClientRequest = ha_msg_value(haMsg, F_CKPT_CLIENT_REQUEST);
	if (strClientRequest != NULL) {
		ckptMsg->clientRequest = atoi(strClientRequest);
	}
	
	strClientRequestNO = ha_msg_value(haMsg, F_CKPT_CLIENT_REQUEST_NO);
	if (strClientRequestNO != NULL) {
		ckptMsg->clientRequestNO = atoi(strClientRequestNO);
	}
	
	strOperation = ha_msg_value(haMsg, F_CKPT_OPERATION);
	if (strOperation != NULL) {
		ckptMsg->operation = atoi(strOperation);
	}
	
	strOperationNO = ha_msg_value(haMsg, F_CKPT_OPERATION_NO);
	if (strOperationNO != NULL) {
		ckptMsg->operationNO = atoi(strOperationNO);
	}
	
	strParam = ha_msg_value(haMsg, F_CKPT_PARAM);
	strParamLength = ha_msg_value(haMsg, F_CKPT_PARAM_LENGTH);
	if (strParamLength != NULL) {
		ckptMsg->paramLength = atoi(strParamLength);
		
		ckptMsg->param = SaCkptMalloc(ckptMsg->paramLength);
		SACKPTASSERT(ckptMsg->param != NULL);
		base64_to_binary(strParam, strlen(strParam), 
			ckptMsg->param, ckptMsg->paramLength);
	}
	
	strData = ha_msg_value(haMsg, F_CKPT_DATA);
	strDataLength = ha_msg_value(haMsg, F_CKPT_DATA_LENGTH);
	if (strDataLength != NULL) {
		ckptMsg->dataLength = atoi(strDataLength);
	
		ckptMsg->data = SaCkptMalloc(ckptMsg->dataLength);
		SACKPTASSERT(ckptMsg->data != NULL);
		base64_to_binary(strData, strlen(strData), 
			ckptMsg->data, ckptMsg->dataLength);
	}
	
	strRc = ha_msg_value(haMsg, F_CKPT_RETVAL);
	if (strRc != NULL) {
		ckptMsg->retVal = atoi(strRc);
	} 
	
	strHostname = ha_msg_value(haMsg, F_ORIG);
	if (strHostname != NULL) {
		strcpy(ckptMsg->fromNodeName, strHostname);
	}
	
	strStatus = ha_msg_value(haMsg, F_STATUS);
	if (strStatus != NULL) {
		strcpy(ckptMsg->hamsgStatus, strStatus);
	}
	
	return ckptMsg;
}

/* send message to one node */
int 
SaCkptMessageSend(SaCkptMessageT* ckptMsg, char* nodename)
{
	ll_cluster_t	*hb = saCkptService->heartbeat;
	struct ha_msg	*haMsg = NULL;

	char		*strSubtype = NULL;
	char		*strErr = NULL;
	int		rc;

	strcpy(ckptMsg->fromNodeName, saCkptService->nodeName);
	haMsg = SaCkptMessage2Hamsg(ckptMsg);

	if (haMsg == NULL) {
		cl_log(LOG_ERR, 
			"Convert ckptmsg to hamsg failed");
		return HA_OK;
	} else {
		rc =  hb->llc_ops->sendnodemsg(hb, haMsg, nodename);
		if (rc == HA_OK) {
			if (saCkptService->flagVerbose) {
				strSubtype = SaCkptMsgSubtype2String(
					ckptMsg->msgSubtype);
				strErr = SaCkptErr2String(ckptMsg->retVal);
				cl_log(LOG_INFO, 
				"Send message to %s, type %s, subtype %s, status %s",
				nodename, ckptMsg->msgType, 
				strSubtype, strErr);
				SaCkptFree((void*)&strSubtype);
				SaCkptFree((void*)&strErr);
			}

#ifdef CKPTDEBUG
			{
				char *	strHamsg = NULL;
				
				strHamsg = msg2string(haMsg);
				cl_log(LOG_DEBUG, 
					"Send cluster message\n%s", 
					strHamsg);
				SaCkptFree((void**)&strHamsg);
			}
#endif
			
		} else {
			cl_log(LOG_ERR, 
				"Send message to %s failed", 
				nodename);
		}
		
		ha_msg_del(haMsg);
		haMsg = NULL;
	}


	return rc;
}

/* send message to all the cluster nodes */
int 
SaCkptMessageBroadcast(SaCkptMessageT* ckptMsg)
{
	ll_cluster_t	*hb = saCkptService->heartbeat;
	struct ha_msg	*haMsg = NULL;

	char		*strMsgSubtype = NULL;
	int		rc;
	
	strcpy(ckptMsg->fromNodeName, saCkptService->nodeName);
	haMsg = SaCkptMessage2Hamsg(ckptMsg);
	
	if (haMsg == NULL) {
		cl_log(LOG_ERR, 
			"Convert ckptmsg to hamsg failed");
		return HA_OK;
	} else {
		rc = hb->llc_ops->sendclustermsg(hb, haMsg);
		if (rc == HA_OK) {
			if (saCkptService->flagVerbose) {
				strMsgSubtype = SaCkptMsgSubtype2String(
					ckptMsg->msgSubtype);
				cl_log(LOG_INFO, 
				"Broadcast message, type %s, subtype %s",
				ckptMsg->msgType, strMsgSubtype);
				SaCkptFree((void*)&strMsgSubtype);
			}

#ifdef CKPTDEBUG			
			{
				char *	strHamsg = NULL;
				
				strHamsg = msg2string(haMsg);
				cl_log(LOG_DEBUG, 
					"Broadcast cluster message\n%s", 
					strHamsg);
				SaCkptFree((void**)&strHamsg);
			}
#endif
		
		} else {
			cl_log(LOG_ERR, 
				"Broadcast message to cluster failed");
		}
		
		ha_msg_del(haMsg);
	}

	return rc;
}

/* send message to multiple nodes */
int 
SaCkptMessageMulticast(SaCkptMessageT* ckptMsg, GList* list)
{
	ll_cluster_t	*hb = saCkptService->heartbeat;
	struct ha_msg	*haMsg = NULL;

	SaCkptStateT	*state = NULL;
	char		*strMsgSubtype = NULL;
	int 	rc;

	strcpy(ckptMsg->fromNodeName, saCkptService->nodeName);
	haMsg = SaCkptMessage2Hamsg(ckptMsg);

	if (haMsg == NULL) {
		cl_log(LOG_ERR, "Convert ckptmsg to hamsg failed");
		return HA_OK;
	}

	while (list != NULL) {
		state = (SaCkptStateT*)list->data;
		
		rc = hb->llc_ops->sendnodemsg(hb, haMsg, state->nodeName);
		if (rc == HA_OK) {
			if (saCkptService->flagVerbose) {
				strMsgSubtype = SaCkptMsgSubtype2String(
					ckptMsg->msgSubtype);
				cl_log(LOG_INFO, 
				"Send message to %s, type %s, subtype %s",
				state->nodeName,
				ckptMsg->msgType, strMsgSubtype);
				SaCkptFree((void*)&strMsgSubtype);
			}
		} else {
			cl_log(LOG_ERR, 
				"Send message to %s failed", 
				state->nodeName);
		}
		
		list = list->next;
	}
	
#ifdef CKPTDEBUG
	{
		char *	strHamsg = NULL;
		
		strHamsg = msg2string(haMsg);
		cl_log(LOG_DEBUG, 
			"Multicast message\n%s", 
			strHamsg);
		SaCkptFree((void**)&strHamsg);
	}
#endif

	ha_msg_del(haMsg);
	
	return HA_OK;
}

/* 
 * convert ckpt message subtype to string
 * used for debug purpose
 */
char* 
SaCkptMsgSubtype2String(SaCkptMsgSubtypeT msgSubtype)
{
	char* strMsgSubtype = NULL;
	char *strTemp = NULL;

	strTemp = (char*)SaCkptMalloc(64);
	SACKPTASSERT(strTemp != NULL);
	
	switch (msgSubtype) {
		
	case M_CKPT_CREATED:
		strcpy(strTemp, "M_CKPT_CREATED");
		break;
	case M_CKPT_CREATED_REPLY:	
		strcpy(strTemp, "M_CKPT_CREATED_REPLY");
		break;
	case M_CKPT_OPEN_BCAST:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST");
		break;
	case M_CKPT_OPEN_BCAST_REPLY:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST_REPLY");
		break;
	
	case M_CKPT_OPEN_BCAST_REPLY_NO_REPLICA:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST_REPLY_NO_REPLICA");
		break;
	
	case M_CKPT_OPEN_BCAST_REPLY_STANDBY:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST_REPLY_STANDBY");
		break;
	
	case M_CKPT_OPEN_BCAST_REPLY_RACE_HIGH:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST_REPLY_RACE_HIGH");
		break;
	
	case M_CKPT_OPEN_BCAST_REPLY_RACE_LOW:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST_REPLY_RACE_LOW");
		break;
		
	case M_CKPT_OPEN_BCAST_REPLY_SELF:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST_REPLY_SELF");
		break;
	case M_CKPT_OPEN_BCAST_REPLY_EARLIER:
		strcpy(strTemp, "M_CKPT_OPEN_BCAST_REPLY_EARLIER");
		break;
	

	case M_RPLC_CRT:
		strcpy(strTemp, "M_RPLC_CRT");
		break;
	case M_RPLC_CRT_REPLY:
		strcpy(strTemp, "M_RPLC_CRT_REPLY");
		break;
	case M_RPLC_ADD:
		strcpy(strTemp, "M_RPLC_ADD");
		break;
	case M_RPLC_ADD_REPLY:
		strcpy(strTemp, "M_RPLC_ADD_REPLY");
		break;
	case M_RPLC_ADD_PREPARE_BCAST:
		strcpy(strTemp, "M_RPLC_ADD_PREPARE_BCAST");
		break;
	case M_RPLC_ADD_PREPARE_BCAST_REPLY:
		strcpy(strTemp, "M_RPLC_ADD_PREPARE_BCAST_REPLY");
		break;
	case M_RPLC_ADD_COMMIT_BCAST:
		strcpy(strTemp, "M_RPLC_ADD_COMMIT_BCAST");
		break;
	case M_RPLC_ADD_COMMIT_BCAST_REPLY:
		strcpy(strTemp, "M_RPLC_ADD_COMMIT_BCAST_REPLY");
		break;
	case M_RPLC_ADD_ROLLBACK_BCAST:
		strcpy(strTemp, "M_RPLC_ADD_ROLLBACK_BCAST");
		break;
	case M_RPLC_ADD_ROLLBACK_BCAST_REPLY:
		strcpy(strTemp, "M_RPLC_ADD_ROLLBACK_BCAST_REPLY");
		break;
	case M_CKPT_OPEN_REMOTE:
		strcpy(strTemp, "M_CKPT_OPEN_REMOTE");
		break;
	case M_CKPT_OPEN_REMOTE_REPLY:
		strcpy(strTemp, "M_CKPT_OPEN_REMOTE_REPLY");
		break;
	case M_CKPT_CLOSE_REMOTE:
		strcpy(strTemp, "M_CKPT_CLOSE_REMOTE");
		break;
	case M_CKPT_CLOSE_REMOTE_REPLY:
		strcpy(strTemp, "M_CKPT_CLOSE_REMOTE_REPLY");
		break;
	case M_CKPT_CKPT_CREATE_BCAST:
		strcpy(strTemp, "M_CKPT_CKPT_CREATE_BCAST");
		break;
	case M_CKPT_CKPT_CREATE_BCAST_REPLY:
		strcpy(strTemp, "M_CKPT_CKPT_CREATE_BCAST_REPLY");
		break;
	case M_RPLC_DEL:
		strcpy(strTemp, "M_RPLC_DEL");
		break;
	case M_RPLC_DEL_REPLY:
		strcpy(strTemp, "M_RPLC_DEL_REPLY");
		break;
	case M_RPLC_DEL_BCAST:
		strcpy(strTemp, "M_RPLC_DEL_BCAST");
		break;
	case M_RPLC_DEL_BCAST_REPLY:
		strcpy(strTemp, "M_RPLC_DEL_BCAST_REPLY");
		break;
	case M_CKPT_UPD:
		strcpy(strTemp, "M_CKPT_UPD");
		break;
	case M_CKPT_UPD_REPLY:
		strcpy(strTemp, "M_CKPT_UPD_REPLY");
		break;
	case M_CKPT_UPD_PREPARE_BCAST:
		strcpy(strTemp, "M_CKPT_UPD_PREPARE_BCAST");
		break;
	case M_CKPT_UPD_PREPARE_BCAST_REPLY:
		strcpy(strTemp, "M_CKPT_UPD_PREPARE_BCAST_REPLY");
		break;
	case M_CKPT_UPD_COMMIT_BCAST:
		strcpy(strTemp, "M_CKPT_UPD_COMMIT_BCAST");
		break;
	case M_CKPT_UPD_COMMIT_BCAST_REPLY:
		strcpy(strTemp, "M_CKPT_UPD_COMMIT_BCAST_REPLY");
		break;
	case M_CKPT_UPD_ROLLBACK_BCAST:
		strcpy(strTemp, "M_CKPT_UPD_ROLLBACK_BCAST");
		break;
	case M_CKPT_UPD_ROLLBACK_BCAST_REPLY:
		strcpy(strTemp, "M_CKPT_UPD_ROLLBACK_BCAST_REPLY");
		break;
	case M_CKPT_UPD_BCAST:
		strcpy(strTemp, "M_CKPT_UPD_BCAST");
		break;
	case M_CKPT_UPD_BCAST_REPLY:
		strcpy(strTemp, "M_CKPT_UPD_BCAST_REPLY");
		break;
	case M_CKPT_SYNC:
		strcpy(strTemp, "M_CKPT_SYNC");
		break;
	case M_CKPT_SYNC_REPLY:
		strcpy(strTemp, "M_CKPT_SYNC_REPLY");
		break;
	case M_CKPT_ACT_SET_BCAST:
		strcpy(strTemp, "M_CKPT_ACT_SET_BCAST");
		break;
	case M_CKPT_ACT_SET_BCAST_REPLY:
		strcpy(strTemp, "M_CKPT_ACT_SET_BCAST_REPLY");
		break;
	case M_CKPT_ACT_SET_FINISH_BCAST:
		strcpy(strTemp, "M_CKPT_ACT_SET_FINISH_BCAST");
		break;
	case M_CKPT_READ:
		strcpy(strTemp, "M_CKPT_READ");
		break;
	case M_CKPT_READ_REPLY:
		strcpy(strTemp, "M_CKPT_READ_REPLY");
		break;
	case M_CKPT_UNLINK_BCAST:
		strcpy(strTemp, "M_CKPT_UNLINK_BCAST");
		break;
	default:
		strcpy(strTemp, "NULL");
	}

	strMsgSubtype = SaCkptMalloc(strlen(strTemp)+1);
	if (strMsgSubtype == NULL) {
		return NULL;
	}
	memcpy(strMsgSubtype, strTemp, strlen(strTemp)+1);

	SaCkptFree((void*)&strTemp);

	return strMsgSubtype;
}

/* create ckpt message according to a request */
SaCkptMessageT* 
SaCkptMessageCreateReq(SaCkptRequestT* ckptReq, SaCkptMsgSubtypeT msgSubtype)
{
	SaCkptMessageT* ckptMsg = NULL;

	ckptMsg = (SaCkptMessageT*)SaCkptMalloc(sizeof(SaCkptMessageT));
	if (ckptMsg == NULL) {
		return NULL;
	}

	strcpy(ckptMsg->msgType, T_CKPT);
	ckptMsg->msgSubtype = msgSubtype;
	ckptMsg->msgVersion = saCkptService->version;
	ckptMsg->retVal = SA_OK;

	if (ckptReq->openCkpt != NULL) {
		strcpy(ckptMsg->checkpointName, 
			ckptReq->openCkpt->checkpointName);
		strcpy(ckptMsg->activeNodeName,
			ckptReq->openCkpt->activeNodeName);
	}

	ckptMsg->clientHandle = ckptReq->clientRequest->clientHandle;
	strcpy(ckptMsg->fromNodeName, saCkptService->nodeName);
	strcpy(ckptMsg->clientHostName, saCkptService->nodeName);
	ckptMsg->clientRequest = ckptReq->clientRequest->req;
	ckptMsg->clientRequestNO = ckptReq->clientRequest->requestNO;

	ckptMsg->param = ckptReq->clientRequest->reqParam;
	ckptMsg->paramLength = ckptReq->clientRequest->reqParamLength;
	ckptMsg->data = ckptReq->clientRequest->data;
	ckptMsg->dataLength = ckptReq->clientRequest->dataLength;

	return ckptMsg;
}

/* create ckpt message according to an operation */
SaCkptMessageT* 
SaCkptMessageCreateOp(SaCkptOperationT* ckptOp, SaCkptMsgSubtypeT msgSubtype)
{
	SaCkptMessageT* ckptMsg = NULL;

	ckptMsg = (SaCkptMessageT*)SaCkptMalloc(sizeof(SaCkptMessageT));
	if (ckptMsg == NULL) {
		return NULL;
	}

	strcpy(ckptMsg->msgType, T_CKPT);
	ckptMsg->msgSubtype = msgSubtype;
	ckptMsg->msgVersion = saCkptService->version;
	ckptMsg->retVal = SA_OK;

	strcpy(ckptMsg->checkpointName, ckptOp->replica->checkpointName);
	strcpy(ckptMsg->activeNodeName, ckptOp->replica->activeNodeName);

	ckptMsg->clientHandle = ckptOp->clientHandle;
	strcpy(ckptMsg->fromNodeName, saCkptService->nodeName);
	strcpy(ckptMsg->clientHostName, ckptOp->clientHostName);
	ckptMsg->clientRequest = ckptOp->clientRequest;
	ckptMsg->clientRequestNO = ckptOp->clientRequestNO;

	ckptMsg->operation = ckptOp->operation;
	ckptMsg->operationNO = ckptOp->operationNO;
	ckptMsg->param = ckptOp->param;
	ckptMsg->paramLength = ckptOp->paramLength;
	ckptMsg->data = ckptOp->data;
	ckptMsg->dataLength = ckptOp->dataLength;

	return ckptMsg;
}

/* create an operation on the active replica according to the ckpt message */
SaCkptOperationT* 
SaCkptOperationCreate(SaCkptMessageT* ckptMsg, SaCkptReplicaT* replica)
{
	SaCkptOperationT* ckptOp = NULL;
	GList* list = NULL;
	SaCkptStateT* state = NULL;

	ckptOp = (SaCkptOperationT*)SaCkptMalloc(sizeof(SaCkptOperationT));
	if (ckptOp == NULL) {
		return NULL;
	}

	ckptOp->replica = replica;
	ckptOp->clientHandle = ckptMsg->clientHandle;
	strcpy(ckptOp->clientHostName, ckptMsg->clientHostName);
	ckptOp->clientRequest = ckptMsg->clientRequest;
	ckptOp->clientRequestNO = ckptMsg->clientRequestNO;
	ckptOp->stateList = NULL;

	switch (ckptMsg->msgSubtype) {
	case M_RPLC_CRT:
		ckptOp->operation = OP_RPLC_CRT;
		break;
	case M_RPLC_ADD:
		ckptOp->operation = OP_RPLC_ADD;
		break;
	case M_CKPT_UPD:
		ckptOp->operation = OP_CKPT_UPD;
		break;
	case M_CKPT_READ:
		ckptOp->operation = OP_CKPT_READ;
		break;
	default:
		ckptOp->operation = OP_NULL;
	}

	replica->nextOperationNumber++;
	if (replica->nextOperationNumber <= 0) {
		replica->nextOperationNumber = 1;
	}
	ckptOp->operationNO = replica->nextOperationNumber;
	if (ckptMsg->paramLength > 0) {
		ckptOp->paramLength = ckptMsg->paramLength;
		ckptOp->param = SaCkptMalloc(ckptMsg->paramLength);
		SACKPTASSERT(ckptOp->param != NULL);
		memcpy(ckptOp->param, ckptMsg->param, ckptMsg->paramLength);
	}
	if (ckptMsg->dataLength > 0) {
		ckptOp->dataLength = ckptMsg->dataLength;
		ckptOp->data = SaCkptMalloc(ckptMsg->dataLength);
		SACKPTASSERT(ckptOp->data != NULL);
		memcpy(ckptOp->data, ckptMsg->data, ckptMsg->dataLength);
	}

	ckptOp->state = OP_STATE_PENDING;
	list = replica->nodeList;
	while (list != NULL) {
		state = (SaCkptStateT*)SaCkptMalloc(sizeof(SaCkptStateT));
		SACKPTASSERT(state != NULL);
		memcpy(state, list->data, sizeof(SaCkptStateT));
		state->state = OP_STATE_STARTED;
		ckptOp->stateList = g_list_append(ckptOp->stateList,
			(gpointer)state);
		
		list = list->next;
	}
	ckptOp->timeoutTag = 0;

	/* update message */
	ckptMsg->operation = ckptOp->operation;
	ckptMsg->operationNO = ckptOp->operationNO;

	return ckptOp;
}

/* free the ckpt message */
void 
SaCkptMessageDelete(SaCkptMessageT** pCkptMsg)
{
	SaCkptMessageT* ckptMsg = *pCkptMsg;
	
	if (ckptMsg->paramLength > 0) {
		SaCkptFree((void**)&(ckptMsg->param));
	}

	if (ckptMsg->dataLength > 0) {
		SaCkptFree((void**)&(ckptMsg->data));
	}

	SaCkptFree((void*)&ckptMsg);

	*pCkptMsg = NULL;

	return;
}

/*
* keep this open request 
*/
void 
initOpenReqNodeStatus(SaCkptClientRequestT *clientReq){
	
	SaCkptReqOpenParamT	*openParam	= NULL;
	
	if(clientReq == NULL){
		cl_log(LOG_ERR, "NULL clientReq in initOpenReqNodeStatus");
		return;
	}
	if((clientReq->req != REQ_CKPT_OPEN) 
				&& (clientReq->req != REQ_CKPT_OPEN_ASYNC)){
		cl_log(LOG_ERR, "Not Open Request in initOpenReqNodeStatus");
		return;
	}
	
	openParam = (SaCkptReqOpenParamT *)clientReq->reqParam;
	
	g_hash_table_foreach(saCkptService->nodeStatusHash, getNodeCkptStatus,openParam);
	
	g_hash_table_insert(saCkptService->openRequestHash, (gpointer)openParam->ckptName.value,(gpointer)clientReq);
}

/* set the open request's status according to node status*/
void
getNodeCkptStatus(gpointer key,gpointer value,
			gpointer user_data){
	saCkptNodeInfo *ckptNodeInfo = value;
	const char *	nodeName = key;
	SaCkptReqOpenParamT * openParam = user_data;
	saOpenNodeStatusT  	*status 	= NULL;
	status = (saOpenNodeStatusT *)ha_malloc(
		sizeof(saOpenNodeStatusT));
	if(status == NULL){
		/*FIXME how to report error on hash fucntions*/
		cl_log(LOG_INFO,"malloc error in getNodeCkptStatus");
		return;
	}
	strncpy(status->nodeName, nodeName, SA_MAX_NAME_LENGTH);
	
	if(ckptNodeInfo->ckptStatus == CKPT_RUNNING){
		status->status = RES_NO_RESPONSE ;
	}else{
		status->status = RES_NOT_RUN;
	}

	openParam->nodeReponse = g_list_append(openParam->nodeReponse,status);
}

gboolean 
isLoopMessage(SaCkptMessageT * ckptMsg){
	if( ckptMsg == NULL) return FALSE;
	
	if( strncmp(ckptMsg->fromNodeName, 
		saCkptService->nodeName, SA_MAX_NAME_LENGTH)){
		return FALSE;	
	}
	
	return TRUE;
}

/* Check if a open request exist already*/
SaCkptClientRequestT *
isOnOpenProcess(SaCkptReqOpenParamT *openParam ){
	SaCkptClientRequestT *	clientReq = NULL;

	if(openParam == NULL){
		if(saCkptService->flagVerbose){
			cl_log(LOG_INFO,"NULL openParam on isOnOpenProcess");
			return 0;
		}
	}
	
	clientReq = (SaCkptClientRequestT *)g_hash_table_lookup(	\
			saCkptService->openRequestHash,	\
			(gconstpointer)openParam->ckptName.value);
	
	return clientReq ;
}

gboolean
isHighPriority(const SaCkptMessageT *ckptMsg ){
	if(ckptMsg == NULL){
		cl_log(LOG_INFO,"NULL ckptMsg on isHighPriority");
		return FALSE;
	}
	if( strncmp(ckptMsg->fromNodeName, 
		saCkptService->nodeName, SA_MAX_NAME_LENGTH) < 0){
		return FALSE;	
	}
	
	return TRUE;
}

gint
updateOpenProcessQueue(const SaCkptMessageT *ckptMsg ,saOpenResponseTypeT *type){
	
	SaCkptClientRequestT 		*clientReq	=	NULL;
	SaCkptReqOpenParamT 		*openParam	=	NULL;
	saOpenResponseTypeT		nodeStatus	=	RES_NO_REPLICA;
	gint 				result		=	1;
	if(ckptMsg == NULL){
		cl_log(LOG_INFO,"NULL ckptMsg on isHighPriority");
		return 0;
	}
	openParam = (SaCkptReqOpenParamT*)ckptMsg->param;
	
	if(( clientReq = isOnOpenProcess(openParam)) != NULL){
		cl_log(LOG_INFO,
		"\tupdate open process queue for checkpoint %s \n",
		openParam->ckptName.value);

		if( getOpenParamNodeStatus(
				(SaCkptReqOpenParamT *)clientReq->reqParam, 
				ckptMsg->fromNodeName,&nodeStatus)){
			if(nodeStatus == RES_NO_REPLICA){
				cl_log(LOG_INFO,"\t get res_no_replica already");
				updateOpenParamNodeStatus(
					(SaCkptReqOpenParamT *)clientReq->reqParam,
					ckptMsg->fromNodeName, RES_LATER);

 				setOpenParamNodeStatusCkptMessage(
					(SaCkptReqOpenParamT *)clientReq->reqParam,
					ckptMsg->fromNodeName,ckptMsg);
				*type = RES_EARLIER;
			}
			return 1;
			
		}
		if(isHighPriority(ckptMsg)){
			cl_log(LOG_INFO,"\t\tresult is high");
			updateOpenParamNodeStatus(
				(SaCkptReqOpenParamT *)clientReq->reqParam,
				ckptMsg->fromNodeName, 
				RES_RACE_LOW_PRIO);

			setOpenParamNodeStatusCkptMessage(
				(SaCkptReqOpenParamT *)clientReq->reqParam,
				ckptMsg->fromNodeName,
				ckptMsg);

			*type = RES_RACE_HIGH_PRIO;
		}else{
			cl_log(LOG_INFO,"\t\tresult is low");
			updateOpenParamNodeStatus(
				(SaCkptReqOpenParamT*)clientReq->reqParam,
				ckptMsg->fromNodeName, 
				RES_RACE_HIGH_PRIO);
			*type = RES_RACE_LOW_PRIO;
		}
	}else{
		
		result = 0;
	}
	return result;

}
/* Check if we should create a local replica*/
gboolean
openReqFinishedForLocalCreate(SaCkptReqOpenParamT  *openParam){
	
	gboolean allFinished = TRUE;
	gboolean noRemoteReplical = TRUE;
	saOpenNodeStatusT 	*status = NULL;
	GList  			*list	= NULL;
	if(openParam == NULL ){
		if(saCkptService->flagVerbose){
			cl_log(LOG_INFO, 
			"NULL parameter on openReqFinishedForLocalCreate\n");
		}
		return FALSE;
	}
	openParamNodeStatusDump(openParam);

	list = openParam->nodeReponse;
	while(list != NULL){
		status = (saOpenNodeStatusT *)(list->data);
		if(status->status  == RES_NO_RESPONSE){
			 allFinished = FALSE;
		}else if(status->status  == RES_RACE_HIGH_PRIO 
			||status->status  == RES_EARLIER ){
			noRemoteReplical = FALSE;
		}
		
		list = list->next;
	}
	
	return (allFinished && noRemoteReplical) ;
}

void
openParamNodeStatusDump(SaCkptReqOpenParamT  *openParam){
	saOpenNodeStatusT 	*status = NULL;
	GList  			*list	= NULL;
	if(saCkptService->flagVerbose){
		cl_log(LOG_INFO,
		"\tOpen Req %s status is: \n",
		openParam->ckptName.value);
	}
	list = openParam->nodeReponse;
	while(list != NULL){
		status = (saOpenNodeStatusT *)(list->data);
		if(saCkptService->flagVerbose){
			cl_log(LOG_INFO,
			"\tNode %s is %d\n",
			status->nodeName,
			status->status);
		}
		list = list->next;
	}
}

gint 
getOpenParamNodeStatus(SaCkptReqOpenParamT  *openParam, const char *nodeName, saOpenResponseTypeT *type){
	saOpenNodeStatusT 	*status = NULL;
	GList  			*list	= NULL;
	
	if(openParam == NULL ||nodeName == NULL || type == NULL){
		cl_log(LOG_ERR, "NULL parameter on getOpenParamNodeStatus");
		return 0;
	}
	list = openParam->nodeReponse;

	while(list != NULL){
		status = (saOpenNodeStatusT *)(list->data);
		if(!strncmp(status->nodeName,nodeName,SA_MAX_NAME_LENGTH)){
			*type = status->status ;
			break;
		}
		list = list->next;
	}
	return 1;
	
}	


void 
updateOpenParamNodeStatus(SaCkptReqOpenParamT  *openParam, const char *nodeName, saOpenResponseTypeT type){
	
	saOpenNodeStatusT 	*status = NULL;
	GList  			*list	= NULL;
	
	if(openParam == NULL ||nodeName == NULL){
		cl_log(LOG_ERR, 
		"NULL parameter on updateOpenParamNodeStatus\n");
		return;
	}
	list = openParam->nodeReponse;
	
	while(list != NULL){
		status = (saOpenNodeStatusT *)(list->data);
		if(!strncmp(status->nodeName,nodeName,SA_MAX_NAME_LENGTH)){
			status->status = type;
			break;
		}
		list = list->next;
	}
	
	
}

gint
setOpenParamNodeStatusCkptMessage(SaCkptReqOpenParamT  *openParam, const char *nodeName, const SaCkptMessageT *ckptMsg)
{
	saOpenNodeStatusT 	*status = NULL;
	GList  			*list	= NULL;
	SaCkptMessageT 	*localCkptMsg = NULL;
	
	if(openParam == NULL ||nodeName == NULL){
		cl_log(LOG_ERR, 
		"NULL parameter on updateOpenParamNodeStatus\n");
		return HA_FAIL;
	}
	localCkptMsg = dupCkptMessage(ckptMsg);
	
	if(localCkptMsg == NULL){
		cl_log(LOG_ERR,
		 "Malloc error on updateOpenParamNodeStatus\n");
		return HA_FAIL;
	}
	list = openParam->nodeReponse;
	
	while(list != NULL){
		status = (saOpenNodeStatusT *)(list->data);
		if(!strncmp(status->nodeName,nodeName,SA_MAX_NAME_LENGTH)){
			if(status->status != RES_RACE_LOW_PRIO){
				cl_log(LOG_INFO,
				"not RES_RACE_LOW_PRIO in setOpenParamNodeStatusCkptMessage\n");
				return HA_FAIL;
			}
			status->originalMessage = localCkptMsg;
			break;
		}
		list = list->next;
	}
	return HA_OK;
}

void
notifyLowPrioNode(SaCkptReqOpenParamT  *openParam){
	GList  			*list	= NULL;
	SaCkptMessageT* ckptMsg = NULL;
	saOpenNodeStatusT *status = NULL;
	ckptMsg = (SaCkptMessageT*)SaCkptMalloc(sizeof(SaCkptMessageT));
	
	list = openParam->nodeReponse;
	cl_log(LOG_INFO,"Enter notifyLowPrioNode");
	while(list != NULL){
		status = (saOpenNodeStatusT *)(list->data);
		if(status->status == RES_RACE_LOW_PRIO 
				||status->status == RES_LATER ){
			ckptMsg= (SaCkptMessageT *)status->originalMessage;
			cl_log(LOG_INFO,"notify lower prio node %s",ckptMsg->clientHostName);
			ckptMsg->msgSubtype = M_CKPT_OPEN_BCAST_REPLY;
			ckptMsg->retVal = SA_OK;
			strcpy(ckptMsg->activeNodeName, saCkptService->nodeName);			SaCkptMessageSend(ckptMsg, ckptMsg->clientHostName);
			break;
		}
		list = list->next;
	}
}

void
removeOpenPendingQueue(SaCkptReqOpenParamT  *openParam ){
	if(openParam == NULL){
		cl_log(LOG_INFO, "NULL openParam on updateOpenParamNodeStatus");
		return;
	}		
	openParamNodeStatusDump(openParam);
	g_hash_table_remove(
		saCkptService->openRequestHash, 
		(gpointer)openParam->ckptName.value);
	
}

void
receiveCkptCreateMsg(SaCkptMessageT* ckptMsg ){
	saCkptNodeInfo *  nodeStatus = NULL;
	char targetNodeName[SA_MAX_NAME_LENGTH];
	if(ckptMsg->msgSubtype != M_CKPT_CREATED){
		cl_log(LOG_ERR,
		"ckptMsg is not M_CKPT_CREATED on receiveCkptCreateMsg\n");
		return;
	}
	nodeStatus = g_hash_table_lookup(saCkptService->nodeStatusHash,
		(gpointer)ckptMsg->fromNodeName);
	if(nodeStatus == NULL){
		cl_log(LOG_INFO,
		"the node %s not found on hashtable\n",ckptMsg->fromNodeName);
		return ;
	}
	nodeStatus->ckptStatus = CKPT_RUNNING;
	
	if(!isLoopMessage(ckptMsg))
	{	
		strncpy(targetNodeName,
			ckptMsg->fromNodeName,
			SA_MAX_NAME_LENGTH);
		targetNodeName[SA_MAX_NAME_LENGTH - 1] = '\0';
		ckptMsg->msgSubtype = M_CKPT_CREATED_REPLY;
		SaCkptMessageSend(ckptMsg,targetNodeName);
	}
	return;
}

void
receiveCkptCreateReplyMsg(SaCkptMessageT* ckptMsg ){
	saCkptNodeInfo *  nodeStatus = NULL;
	if(ckptMsg->msgSubtype != M_CKPT_CREATED_REPLY){
		cl_log(LOG_ERR,
		"ckptMsg is not M_CKPT_CREATED on receiveCkptCreateMsg\n");
		return;
	}
	nodeStatus = g_hash_table_lookup(
			saCkptService->nodeStatusHash,
			(gpointer)ckptMsg->fromNodeName);
	if(nodeStatus == NULL){
		cl_log(LOG_INFO,
		"the node %s not found on hashtable\n",ckptMsg->fromNodeName);
		return ;
	}
	nodeStatus->ckptStatus = CKPT_RUNNING;
	return;
}

SaCkptMessageT *
dupCkptMessage(const SaCkptMessageT *ckptMsg){

	SaCkptMessageT * ret = NULL;
	void * param = NULL;
	void * data = NULL;
	if(ckptMsg == NULL){
		cl_log(LOG_INFO,"NULL ckptMsg in dupCkptMessage");
		return NULL;
	}
	ret = (SaCkptMessageT *)ha_malloc(sizeof(SaCkptMessageT));
	param = ha_malloc(ckptMsg->paramLength);
	data = ha_malloc(ckptMsg->dataLength);
	
	if(ret == NULL ||param== NULL ||  data== NULL){
		cl_log(LOG_INFO,"malloc error in dupCkptMessage");
		if(ret!= NULL) ha_free(ret);
		if(param!= NULL) ha_free(param);
		if(data!= NULL) ha_free(data);
		return NULL;
	} 
	memset(ret,0,sizeof(SaCkptMessageT));
	memset(param,0,ckptMsg->paramLength);
	memset(data,0,ckptMsg->dataLength);
	
	memcpy(ret,ckptMsg,sizeof(SaCkptMessageT));
	memcpy(param,ckptMsg->param,ckptMsg->paramLength);
	memcpy(data,ckptMsg->data,ckptMsg->dataLength);
	
	ret->param = param;
	ret->data = data;
	return ret;

}


void 
displayOpenQueStatus(gpointer key, 
	gpointer value, 
	gpointer userdata){
	SaCkptClientRequestT *clientReq = key;
	SaCkptReqOpenParamT * openParam = clientReq->reqParam;
	GList * list = openParam->nodeReponse;
	saOpenNodeStatusT * status = NULL;
	if(saCkptService->flagVerbose){
		cl_log(LOG_INFO,
		"show open request response for chkpt %s \n",
		openParam->ckptName.value);
	}
	while(list!= NULL){
		status = (saOpenNodeStatusT *)list->data;
		if(saCkptService->flagVerbose){
			cl_log(LOG_INFO,
			"\tnode %s have status of %d\n",
			status->nodeName,
			status->status);
		}
		list = list->next;
	}
}

