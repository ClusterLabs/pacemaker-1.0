/* $Id: replica.c,v 1.18 2005/03/16 17:11:15 lars Exp $ */
/* 
 * replica.c: 
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
 * replica retention timeout process
 * delte local replica and sent message to other nodes to update replica list
 */
gboolean
SaCkptRetentionTimeout(gpointer timeout_data)
{
	SaCkptReplicaT* replica = (SaCkptReplicaT*)timeout_data;

	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO, 
			"Checkpoint %s retention timeout",
			replica->checkpointName);
	}

	/* 
	 * if there are still not finished operations 
	 * do not delete the replica
	 */
	if (g_hash_table_size(replica->operationHash) > 0) {
		replica->flagRetentionTimeout = TRUE;

		return FALSE;
	}

	if (replica != NULL) {
		SaCkptReplicaRemove(&replica);
	}

	return FALSE;
}

/* remove the replica and free its memory */
int 
SaCkptReplicaRemove(SaCkptReplicaT** pReplica)
{
	SaCkptReplicaT* replica = *pReplica;
	SaCkptMessageT* ckptMsg = NULL;

	ckptMsg = SaCkptMalloc(sizeof(SaCkptMessageT));

	ckptMsg->msgVersion = saCkptService->version;
	
	strcpy(ckptMsg->msgType, T_CKPT);
	ckptMsg->msgSubtype = M_RPLC_DEL_BCAST;
	strcpy(ckptMsg->checkpointName, replica->checkpointName);
	ckptMsg->retVal = SA_OK;

	SaCkptMessageMulticast(ckptMsg, replica->nodeList);

	g_hash_table_remove(saCkptService->replicaHash, 
		(gpointer)replica->checkpointName);
	
	cl_log(LOG_INFO, "Replica %s deleted",
		replica->checkpointName);

	SaCkptReplicaFree(pReplica);

	return HA_OK;
}

/* create a replica from scratch */
SaCkptReplicaT* 
SaCkptReplicaCreate(SaCkptReqOpenParamT* openParam) 
{
	SaCkptReplicaT* replica = NULL;
	SaCkptSectionT* sec = NULL;
	SaCkptStateT* state = NULL;

	replica = (SaCkptReplicaT*)SaCkptMalloc(sizeof(SaCkptReplicaT));
	SACKPTASSERT(replica != NULL);

	replica->saCkptService = saCkptService;
	
	strcpy(replica->checkpointName, openParam->ckptName.value);
	strcpy(replica->activeNodeName, saCkptService->nodeName);
	replica->flagIsActive= TRUE;
	replica->createFlag = openParam->attr.creationFlags;
	replica->maxSectionIDSize = openParam->attr.maxSectionIdSize;
	replica->maxSectionNumber = openParam->attr.maxSections;
	replica->maxSectionSize = openParam->attr.maxSectionSize;
	replica->maxCheckpointSize= openParam->attr.checkpointSize;
	replica->checkpointSize = 0;
	replica->retentionDuration = openParam->attr.retentionDuration;
	replica->flagRetentionTimeout = FALSE;

	replica->operationHash = 
		g_hash_table_new(g_int_hash, g_int_equal);
	replica->nextOperationNumber = 0;
	replica->referenceCount = 0;
	replica->flagUnlink = FALSE;
	replica->flagPendOperation = FALSE;
	replica->flagReplicaLock = FALSE;
	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO,
			"Replica %s unlocked",
			replica->checkpointName);
	}
	replica->replicaState= STATE_CREATE_PREPARED;

	replica->openCheckpointList= NULL;
	replica->pendingOperationList = NULL;
	replica->sectionList = NULL;
	replica->nodeList = NULL;

	replica->retentionTimeoutTag = 0;

	/* create default section */
	sec = (SaCkptSectionT*)SaCkptMalloc(sizeof(SaCkptSectionT) + sizeof(SaUint8T));
	SACKPTASSERT(sec != NULL);
	sec->replica = replica;
	sec->sectionID.id[0] = 0;
	sec->sectionID.idLen = 0;
	sec->expirationTime = SA_TIME_END;
	sec->lastUpdateTime = time_longclock();
	sec->dataIndex = 0;
	sec->dataUpdateState = OP_STATE_COMMITTED;
	sec->sectionState = STATE_CREATE_COMMITTED;
	sec->dataState = SA_CKPT_SECTION_VALID;
	sec->dataLength[0] = 0;
	sec->dataLength[1] = 0;
	sec->data[0] = NULL;
	sec->data[1] = NULL;

	replica->sectionList = 
		g_list_append(replica->sectionList,
		(gpointer)sec);
	replica->sectionNumber = 1;

	state = (SaCkptStateT*)SaCkptMalloc(sizeof(SaCkptStateT));
	strcpy(state->nodeName, saCkptService->nodeName);
	state->state = OP_STATE_COMMITTED;
	replica->nodeList = g_list_append(replica->nodeList, 
		(gpointer)state);

	g_hash_table_insert(saCkptService->replicaHash, 
		(gpointer)replica->checkpointName,
		(gpointer)replica);

	cl_log(LOG_INFO, "Replica %s created with default section",
		openParam->ckptName.value);

	return replica;
}


int 
SaCkptReplicaFree(SaCkptReplicaT** pReplica)
{
	SaCkptReplicaT* replica = *pReplica;
	GList* list = NULL;
	SaCkptSectionT* sec = NULL;

	list = replica->nodeList;
	while (list != NULL) {
		if (list->data != NULL) {
			SaCkptFree((void**)&(list->data));
		}
		list = list->next;
	}
	g_list_free(replica->nodeList);

	list = replica->sectionList;
	while (list != NULL) {
		sec = (SaCkptSectionT*)list->data;
		if (sec != NULL) {
			if (sec->data[0] != NULL) {
				SaCkptFree((void**)&(sec->data[0]));
			}
			if (sec->data[1] != NULL) {
				SaCkptFree((void**)&(sec->data[1]));
			}
			SaCkptFree((void*)&sec);
		}
		list = list->next;
	}
	g_list_free(replica->sectionList);

	g_hash_table_destroy(replica->operationHash);

	SaCkptFree((void*)&replica);

	*pReplica = NULL;

	return HA_OK;
}


/* open a checkpoint */
SaCkptOpenCheckpointT* 
SaCkptCheckpointOpen(SaCkptClientT* client, 
	SaCkptReplicaT* replica, 
	SaCkptReqOpenParamT* openParam)
{
	SaCkptOpenCheckpointT* openCkpt = NULL;
	
	/* create opencheckpoint */
	openCkpt = (SaCkptOpenCheckpointT*)SaCkptMalloc(
		sizeof(SaCkptOpenCheckpointT));
	SACKPTASSERT(openCkpt != NULL);
	openCkpt->client = client;
	openCkpt->replica = replica;

	saCkptService->nextCheckpointHandle++;
	if (saCkptService->nextCheckpointHandle <= 0) {
		saCkptService->nextCheckpointHandle = 1;
	}
	openCkpt->checkpointHandle = 
		saCkptService->nextCheckpointHandle;
	openCkpt->checkpointRemoteHandle = -1;
	openCkpt->checkpointOpenFlags = openParam->openFlag;
	openCkpt->flagLocalClient = FALSE;
	openCkpt->flagLocalReplica = FALSE;
	
	if (replica != NULL) {
		/* update the replica reference count */
		replica->referenceCount++;
		
		strcpy(openCkpt->checkpointName, 
			replica->checkpointName);
		strcpy(openCkpt->activeNodeName, 
			replica->activeNodeName);
		
		openCkpt->flagLocalReplica = TRUE;

		replica->openCheckpointList= 
			g_list_append(replica->openCheckpointList,
			(gpointer)openCkpt);
	} 

	if (client != NULL) {
		openCkpt->clientHandle = client->clientHandle;
		strcpy(openCkpt->clientHostName, saCkptService->nodeName);
		
		client->openCheckpointList= 
			g_list_append(client->openCheckpointList, 
			(gpointer)openCkpt);
	}
	
	g_hash_table_insert(saCkptService->openCheckpointHash,
		(gpointer)&(openCkpt->checkpointHandle),
		(gpointer)openCkpt);

	cl_log(LOG_INFO, "client %d opened checkpoint %s, handle %d",
		client!=NULL?client->clientHandle:0,
		replica!=NULL?replica->checkpointName:"REMOTE",
		openCkpt->checkpointHandle);

	return openCkpt;
}

/* close a checkpoint */
int
SaCkptCheckpointClose(SaCkptOpenCheckpointT** pOpenCkpt) 
{
	SaCkptOpenCheckpointT* openCkpt = *pOpenCkpt;
	SaCkptClientT* client = openCkpt->client;
	SaCkptReplicaT* replica = openCkpt->replica;
	int	checkpointHandle = openCkpt->checkpointHandle;

	cl_log(LOG_INFO, "Client %d closed checkpoint %s",
		client!=NULL?client->clientHandle:0,
		replica!=NULL?replica->checkpointName:"REMOTE");
	
	g_hash_table_remove(saCkptService->openCheckpointHash, 
		(gpointer)&checkpointHandle);
	
	if (client != NULL) {
		client->openCheckpointList = 
			g_list_remove(client->openCheckpointList, 
			(gpointer)openCkpt);
	}

	if (replica != NULL) {
		replica->openCheckpointList= 
			g_list_remove(replica->openCheckpointList, 
			(gpointer)openCkpt);
		replica->referenceCount--;

		 if (replica->referenceCount == 0) {
			if (replica->flagUnlink == TRUE) {
				 SaCkptReplicaRemove(&replica);
			} else {
				/* 
				 * if the retention time is SA_TIME_END,
				 * it will exist forever
				 */
				if (replica->retentionDuration != 
					SA_TIME_END) {
					SaCkptReplicaStartTimer(replica);
				}
			}
		 }
	}

	SaCkptFree((void*)&openCkpt);

	*pOpenCkpt = openCkpt;

	return HA_OK;
}

/* pack the replica so that it can be sent in a ckpt message */
int 
SaCkptReplicaPack(void** data, SaSizeT* dataLength,
	SaCkptReplicaT* replica)
{
	SaCkptSectionT* sec = NULL;
	SaCkptStateT* state = NULL;
	int sectionLength = 0;

	GList*	list = NULL;

	char *p = NULL;
	char *q = NULL;
	int n = 0;
	int index = 0;

	q = (char*)SaCkptMalloc(MAXMSG);
	SACKPTASSERT(q != NULL);
	
	p = q;

	n = strlen(replica->checkpointName);
	memcpy(p, &n, sizeof(n));
	p += sizeof(n);
	memcpy(p, replica->checkpointName, n);
	p += n;

	memcpy(p, &replica->maxSectionNumber, 
		sizeof(replica->maxSectionNumber));
	p += sizeof(replica->maxSectionNumber);

	memcpy(p, &replica->maxSectionSize, 
		sizeof(replica->maxSectionSize));
	p += sizeof(replica->maxSectionSize);
	
	memcpy(p, &replica->maxSectionIDSize, 
		sizeof(replica->maxSectionIDSize));
	p += sizeof(replica->maxSectionIDSize);

	memcpy(p, &replica->retentionDuration, 
		sizeof(replica->retentionDuration));
	p += sizeof(replica->retentionDuration);

	memcpy(p, &replica->maxCheckpointSize, 
		sizeof(replica->maxCheckpointSize));
	p += sizeof(replica->maxCheckpointSize);

	memcpy(p, &replica->checkpointSize, 
		sizeof(replica->checkpointSize));
	p += sizeof(replica->checkpointSize);

	memcpy(p, &replica->sectionNumber,
		sizeof(replica->sectionNumber));
	p += sizeof(replica->sectionNumber);

	memcpy(p, &replica->createFlag, 
		sizeof(replica->createFlag));
	p += sizeof(replica->createFlag);

	memcpy(p, &replica->ownerPID, 
		sizeof(replica->ownerPID));
	p += sizeof(replica->ownerPID);

	list = replica->nodeList;
	n = g_list_length(list);
	memcpy(p, &n, sizeof(n));
	p += sizeof(n);

	while (list != NULL) {
		state = (SaCkptStateT*)list->data;
		n = strlen(state->nodeName);
		memcpy(p, &n, sizeof(n));
		p += sizeof(n);
		memcpy(p, state->nodeName, n);
		p += n;
		memcpy(p, &state->state, 
			sizeof(state->state));
		p += sizeof(state->state);

		list = list->next;
	}

	n = strlen(replica->activeNodeName);
	memcpy(p, &n, sizeof(n));
	p += sizeof(n);
	memcpy(p, replica->activeNodeName, n);
	p += n;

	memcpy(p, &replica->nextOperationNumber,
		sizeof(replica->nextOperationNumber));
	p += sizeof(replica->nextOperationNumber);

	list = replica->sectionList;
	while (list != NULL) {
		sec = (SaCkptSectionT* )list->data;
		sectionLength = sizeof(SaCkptSectionT) + sec->sectionID.idLen;
		index = sec->dataIndex;

		memcpy(p,&sectionLength,sizeof(sectionLength));
		p += sizeof(sizeof(sectionLength));
		
		memcpy(p, &sec->sectionID.idLen, 
			sizeof(sec->sectionID.idLen));
		p += sizeof(sec->sectionID.idLen);

		if (sec->sectionID.idLen > 0) {
			memcpy(p, sec->sectionID.id, 
				sec->sectionID.idLen);
			p += sec->sectionID.idLen;
		}

		memcpy(p, &sec->dataState,
			sizeof(sec->dataState));
		p += sizeof(sec->dataState);
		
		if (sec->dataState == 
			SA_CKPT_SECTION_CORRUPTED) {
			list = list->next;
			continue;
		}

		memcpy(p, &sec->expirationTime, 
			sizeof(sec->expirationTime));
		p += sizeof(sec->expirationTime);

		memcpy(p, &sec->lastUpdateTime, 
			sizeof(sec->lastUpdateTime));
		p += sizeof(sec->lastUpdateTime);

		memcpy(p, &sec->dataLength[index], 
			sizeof(sec->dataLength[index]));
		p += sizeof(sec->dataLength[index]);

		if (sec->dataLength[index] > 0) {
			memcpy(p, sec->data[index], 
				sec->dataLength[index]);
			p += sec->dataLength[index];
		}

		list = list->next;
	}

	*dataLength = p - q;
	*data = SaCkptMalloc(*dataLength);
	SACKPTASSERT(*data != NULL);
	memcpy(*data, q, *dataLength);

	SaCkptFree((void*)&q);

	return HA_OK;
}

/* create local replica from the received message */
SaCkptReplicaT* 
SaCkptReplicaUnpack(void* data, int dataLength)
{
	char* p = NULL;
	char* q = NULL;
	SaUint32T i = 0;
	SaUint32T n = 0;
	int m = 0;
	int sectionLength = 0;

	GList* list = NULL;
	SaCkptSectionT* sec = NULL;
	SaCkptStateT* state = NULL;

	SaCkptReplicaT* replica = NULL;

	replica = (SaCkptReplicaT*)SaCkptMalloc(sizeof(SaCkptReplicaT));
	SACKPTASSERT(replica != NULL);

	p = data;

	memcpy(&n, p, sizeof(n));
	p += sizeof(n);

	memcpy(replica->checkpointName, p, n);
	replica->checkpointName[n] = 0;
	p += n;

	memcpy(&(replica->maxSectionNumber), p,
		sizeof(replica->maxSectionNumber));
	p += sizeof(replica->maxSectionNumber);

	memcpy(&(replica->maxSectionSize), p,
		sizeof(replica->maxSectionSize));
	p += sizeof(replica->maxSectionSize);

	memcpy(&(replica->maxSectionIDSize), p, 
		sizeof(replica->maxSectionIDSize));
	p += sizeof(replica->maxSectionIDSize);

	memcpy(&(replica->retentionDuration), p, 
		sizeof(replica->retentionDuration));
	p += sizeof(replica->retentionDuration);

	memcpy(&(replica->maxCheckpointSize), p, 
		sizeof(replica->maxCheckpointSize));
	p += sizeof(replica->maxCheckpointSize);

	memcpy(&(replica->checkpointSize), p, 
		sizeof(replica->checkpointSize));
	p += sizeof(replica->checkpointSize);

	memcpy(&(replica->sectionNumber), p, 
		sizeof(replica->sectionNumber));
	p += sizeof(replica->sectionNumber);

	memcpy(&(replica->createFlag), p, 
		sizeof(replica->createFlag));
	p += sizeof(replica->createFlag);

	memcpy(&(replica->ownerPID), p, 
		sizeof(replica->ownerPID));
	p += sizeof(replica->ownerPID);

	list = replica->nodeList = NULL;

	memcpy(&n, p, sizeof(n));
	p += sizeof(n);

	for(i=0; i<n; i++) {
		state = (SaCkptStateT*)SaCkptMalloc(sizeof(SaCkptStateT));
		SACKPTASSERT(state != NULL);
		
		memcpy(&m, p, sizeof(m));
		p += sizeof(m);

		memcpy(state->nodeName, p, m);
		state->nodeName[m] = 0;
		p += m;

		memcpy(&(state->state), p, sizeof(state->state));
		p += sizeof(state->state);

		replica->nodeList = g_list_append(
			replica->nodeList, (gpointer)state);
	}

	memcpy(&n, p, sizeof(n));
	p += sizeof(n);

	memcpy(replica->activeNodeName, p, n);
	replica->activeNodeName[n] = 0;
	p += n;

	memcpy(&(replica->nextOperationNumber), p, 
		sizeof(replica->nextOperationNumber));
	p += sizeof(replica->nextOperationNumber);

	list = replica->sectionList;
	for(i=0; i<replica->sectionNumber; i++) {
		memcpy(&sectionLength ,p,sizeof(sectionLength));
		p += sizeof(sectionLength);
		
		sec = (SaCkptSectionT*)SaCkptMalloc(sectionLength);
		SACKPTASSERT (sec != NULL);
		
		replica->sectionList = g_list_append(replica->sectionList, 
			(gpointer)sec);

		memcpy(&(sec->sectionID.idLen), p, 
			sizeof(sec->sectionID.idLen));
		p += sizeof(sec->sectionID.idLen);

		if (sec->sectionID.idLen > 0) {
			memcpy(sec->sectionID.id, p, sec->sectionID.idLen);
			p += sec->sectionID.idLen;
		} 

		memcpy(&(sec->dataState), p, sizeof(sec->dataState));
		p += sizeof(sec->dataState);

		if (sec->dataState == SA_CKPT_SECTION_CORRUPTED) {
			continue;
		}

		memcpy(&(sec->expirationTime), p, 
			sizeof(sec->expirationTime));
		p += sizeof(sec->expirationTime);

		memcpy(&(sec->lastUpdateTime), p, 
			sizeof(sec->lastUpdateTime));
		p += sizeof(sec->lastUpdateTime);

		/* by default , 0 will be the active copy */
		memcpy(&(sec->dataLength[0]), p, 
			sizeof(sec->dataLength[0]));
		p += sizeof(sec->dataLength[0]);

		if (sec->dataLength[0] > 0) {
			sec->data[0] = (void*)SaCkptMalloc( 
				sec->dataLength[0]);
			SACKPTASSERT(sec->data[0] != NULL);
			
			memcpy(sec->data[0], p, sec->dataLength[0]);
			p += sec->dataLength[0];
		}

		sec->dataIndex = 0;
		sec->dataUpdateState = OP_STATE_COMMITTED;
		sec->sectionState = STATE_CREATE_COMMITTED;

		sec->replica = replica;

		sec->dataLength[1] = 0;
		sec->data[1] = NULL;
	}

	/* default value for new created replica */
	replica->saCkptService = saCkptService;

	replica->referenceCount = 0;
	replica->replicaState = STATE_CREATE_PREPARED;
	replica->pendingOperationList = NULL;
	replica->flagIsActive = FALSE;
	replica->flagPendOperation = TRUE;
	replica->flagReplicaLock = TRUE;
	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO,
			"Replica %s locked",
			replica->checkpointName);
	}

	replica->retentionTimeoutTag = 0;
	
	replica->operationHash = 
		g_hash_table_new(g_int_hash, g_int_equal);
	replica->openCheckpointList= NULL;

	SaCkptFree((void*)&q);
	if(saCkptService->flagVerbose){
		SaCkptDumpReplica(replica);
	}
	cl_log(LOG_INFO, "Replica %s was copied from node %s",
		replica->checkpointName,
		replica->activeNodeName);

	return replica;
}

int 
SaCkptReplicaRead(SaCkptReplicaT* replica, 
	SaSizeT* dataLength, void** data,
	SaSizeT paramLength, void* param)
{
	SaCkptSectionT* sec = NULL;
	SaCkptReqSecReadParamT* secReadParam = NULL;

	secReadParam = (SaCkptReqSecReadParamT*)param;

	sec = SaCkptSectionFind(replica, 
		&(secReadParam->sectionID));
	if (sec == NULL) {
		return SA_ERR_NOT_EXIST;
	}

	*dataLength = secReadParam->dataSize;
	return SaCkptSectionRead(replica, sec, secReadParam->offset, 
		dataLength, data);
}

int 
SaCkptReplicaUpdate(SaCkptReplicaT* replica, SaCkptReqT req, 
	SaSizeT dataLength, void* data,
	int paramLength, void* param)
{
	SaCkptSectionT* sec = NULL;
	
	SaCkptReqSecCrtParamT* secCrtParam = NULL;
	SaCkptReqSecDelParamT* secDelParam = NULL;
	SaCkptReqSecWrtParamT* secWrtParam = NULL;
	SaCkptReqSecOwrtParamT* secOwrtParam = NULL;

	int	retVal = SA_OK;

	char*	strReq = NULL;
	char* 	strErr = NULL;

	int	index = 0;
	
	switch (req) {
	case REQ_SEC_CRT:
		secCrtParam = (SaCkptReqSecCrtParamT*)param;
		retVal = SaCkptSectionCreate(replica, secCrtParam,
			dataLength, data, &sec);
		if (retVal != SA_OK) {
			break;
		}
		
		sec->sectionState = STATE_CREATE_COMMITTED;

		/* update replica */
		replica->checkpointSize += 
			sec->dataLength[sec->dataIndex];
		replica->sectionNumber++;
		break;

	case REQ_SEC_DEL:
		secDelParam = (SaCkptReqSecDelParamT*)param;
		
		sec = SaCkptSectionFind(replica, 
			&(secDelParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}

		/* update replica */
		replica->sectionList = g_list_remove(
			replica->sectionList,
			(gpointer)sec);
		replica->sectionNumber--;
		replica->checkpointSize -= 
			sec->dataLength[sec->dataIndex];

		if (saCkptService->flagVerbose) {
			char* strSectionID = NULL;
			strSectionID = SaCkptSectionId2String(sec->sectionID);
			cl_log(LOG_INFO, 
				"Update: section %s deleted from replica %s",
				sec->sectionID.id,
				replica->checkpointName);
			SaCkptFree((void*)&strSectionID);
		}
		
		/* free section */
		if (sec->data[0] != NULL) {
			SaCkptFree((void**)&(sec->data[0]));
		}
		if (sec->data[1] != NULL) {
			SaCkptFree((void**)&(sec->data[1]));
		}
		SaCkptFree((void*)&sec);
		break;
		
	case REQ_SEC_WRT:
		secWrtParam = (SaCkptReqSecWrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secWrtParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_INVALID_PARAM;
			break;
		}

		retVal = SaCkptSectionWrite(replica, sec,
			secWrtParam->offset, 
			dataLength, data);
		if (retVal == SA_OK) {
			index = sec->dataIndex;
			
			/* update replica */
			if ((secWrtParam->offset + dataLength) >
				sec->dataLength[index]){
				replica->checkpointSize += 
					secWrtParam->offset + 
					dataLength -
					sec->dataLength[index];

				sec->dataLength[(index+1)%2] = 
					secWrtParam->offset +
					dataLength;
			} else {
				sec->dataLength[(index+1)%2] = 
					sec->dataLength[sec->dataIndex];
			}
			
			/* commit the update */
			SaCkptFree((void**)&(sec->data[index]));
			sec->dataLength[index] = 0;
			sec->dataIndex = (index + 1) % 2;
			sec->dataUpdateState = OP_STATE_COMMITTED;
			sec->lastUpdateTime = time_longclock();
		}

		break;

	case REQ_SEC_OWRT:
		secOwrtParam = (SaCkptReqSecOwrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secOwrtParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_INVALID_PARAM;
			break;
		}
		
		retVal = SaCkptSectionOverwrite(replica, sec, 
			dataLength, data);
		if (retVal == SA_OK) {
			index = sec->dataIndex;
			
			replica->checkpointSize += 
				dataLength - sec->dataLength[index];
			
			/* commit the update */
			SaCkptFree((void**)&(sec->data[index]));
			sec->dataLength[index]= 0;
			sec->dataIndex = (index + 1) % 2;
			sec->dataLength[sec->dataIndex] = dataLength;
			sec->dataUpdateState = OP_STATE_COMMITTED;
			sec->lastUpdateTime = time_longclock();
		}

		break;

	default:
		break;
	}

	if (saCkptService->flagVerbose) {
		strReq = SaCkptReq2String(req);
		strErr = SaCkptErr2String(retVal);
		cl_log(LOG_INFO, 
			"Replica %s update, request %s, status %s",
			replica->checkpointName, strReq, strErr);
		SaCkptFree((void*)&strReq);
		SaCkptFree((void*)&strErr);
	}

	return retVal;
}

int 
SaCkptReplicaUpdPrepare(SaCkptReplicaT* replica, SaCkptReqT req, 
	int dataLength, void* data,
	int paramLength, void* param)
{
	SaCkptSectionT* sec = NULL;
	
	SaCkptReqSecCrtParamT* secCrtParam = NULL;
	SaCkptReqSecDelParamT* secDelParam = NULL;
	SaCkptReqSecWrtParamT* secWrtParam = NULL;
	SaCkptReqSecOwrtParamT* secOwrtParam = NULL;

	int	retVal = SA_OK;

	char*	strReq = NULL;
	char* 	strErr = NULL;

	switch (req) {
	case REQ_SEC_CRT:
		secCrtParam = (SaCkptReqSecCrtParamT*)param;
		retVal = SaCkptSectionCreate(replica, secCrtParam,
			dataLength, data, &sec);
		if (retVal != SA_OK) {
			break;
		}
		sec->sectionState = STATE_CREATE_PREPARED;
		
		break;
		
	case REQ_SEC_DEL:
		secDelParam = (SaCkptReqSecDelParamT*)param;
		
		sec = SaCkptSectionFind(replica, 
			&(secDelParam->sectionID));
		if (sec == NULL) {
			if(saCkptService->flagVerbose){
				cl_log(LOG_INFO, 
				"Can not find section %s \n", secDelParam->sectionID.id);
			}
			retVal = SA_ERR_NOT_EXIST;
			
			break;
		}

		sec->sectionState = STATE_DELETE_PREPARED;

		break;
		
	case REQ_SEC_WRT:
		secWrtParam = (SaCkptReqSecWrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secWrtParam->sectionID));
		if (sec == NULL) {
			if(saCkptService->flagVerbose){
				cl_log(LOG_INFO, "Can not find section %s", secWrtParam->sectionID.id);
			}
			retVal = SA_ERR_INVALID_PARAM;
			
			break;
		}

		retVal = SaCkptSectionWrite(replica, sec,
			secWrtParam->offset, 
			dataLength, data);
		if (retVal == SA_OK) {
			sec->dataUpdateState = OP_STATE_PREPARED;
		}
		
		break;
		
	case REQ_SEC_OWRT:
		secOwrtParam = (SaCkptReqSecOwrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secOwrtParam->sectionID));
		if (sec == NULL) {
			if(saCkptService->flagVerbose){
				cl_log(LOG_INFO,
				 "Can not find section %s \n", secOwrtParam->sectionID.id);
			}
			retVal = SA_ERR_INVALID_PARAM;
			break;
		}
		
		retVal = SaCkptSectionOverwrite(replica, sec, 
			dataLength, data);
		
		if (retVal == SA_OK) {
			sec->dataUpdateState = OP_STATE_PREPARED;
		}

		break;
		
	default:
		break;
		
	}

	if (saCkptService->flagVerbose) {
		strReq = SaCkptReq2String(req);
		strErr = SaCkptErr2String(retVal);
		cl_log(LOG_INFO, 
			"Replica %s update prepared, request %s, status %s",
			replica->checkpointName, strReq, strErr);
		SaCkptFree((void*)&strReq);
		SaCkptFree((void*)&strErr);
	}

	return retVal;
}

int 
SaCkptReplicaUpdCommit(SaCkptReplicaT* replica, SaCkptReqT req, 
	int dataLength, void* data,
	int paramLength, void* param)
{
	SaCkptSectionT* sec = NULL;
	
	SaCkptReqSecCrtParamT* secCrtParam = NULL;
	SaCkptReqSecDelParamT* secDelParam = NULL;
/*	SaCkptReqSecReadParamT* secReadParam = NULL; */
	SaCkptReqSecWrtParamT* secWrtParam = NULL;
	SaCkptReqSecOwrtParamT* secOwrtParam = NULL;

	char*	strReq = NULL;
	char*	strErr = NULL;
	int	retVal = SA_OK;

	int	index = 0;

	switch (req) {
	case REQ_SEC_CRT:
		secCrtParam = (SaCkptReqSecCrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secCrtParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->sectionState != STATE_CREATE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section create commit: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}
		
		sec->sectionState = OP_STATE_COMMITTED;

		/* update replica */
		replica->checkpointSize += 
			sec->dataLength[sec->dataIndex];
		replica->sectionNumber++;

		break;
		
	case REQ_SEC_DEL:
		secDelParam = (SaCkptReqSecDelParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secDelParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->sectionState != STATE_DELETE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section delete commit: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}

		/* update replica */
		replica->sectionList = g_list_remove(
			replica->sectionList,
			(gpointer)sec);
		replica->sectionNumber--;
		replica->checkpointSize -= 
			sec->dataLength[sec->dataIndex];

		if (saCkptService->flagVerbose) {
			char* strSectionID = NULL;
			strSectionID = SaCkptSectionId2String(sec->sectionID);
			cl_log(LOG_INFO, 
				"Commit: section %s deleted from replica %s",
				sec->sectionID.id,
				replica->checkpointName);
			SaCkptFree((void*)&strSectionID);
		}
		
		/* free section */
		if (sec->data[0] != NULL) {
			SaCkptFree((void**)&(sec->data[0]));
		}
		if (sec->data[1] != NULL) {
			SaCkptFree((void**)&(sec->data[1]));
		}
		SaCkptFree((void*)&sec);
		break;
		
	case REQ_SEC_WRT:
		secWrtParam = (SaCkptReqSecWrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secWrtParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->dataUpdateState != OP_STATE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section write commit: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}

		index = sec->dataIndex;
		
		/* update replica */
		if ((secWrtParam->offset + dataLength) >
			sec->dataLength[index]){
			replica->checkpointSize += 
				secWrtParam->offset + 
				dataLength -
				sec->dataLength[index];

			sec->dataLength[(index+1)%2] = 
				secWrtParam->offset +
				dataLength;
		} else {
			sec->dataLength[(index+1)%2] = 
				sec->dataLength[sec->dataIndex];
		}
		
		/* commit the update */
		SaCkptFree((void**)&(sec->data[index]));
		sec->dataLength[index] = 0;
		sec->dataIndex = (index + 1) % 2;
		sec->dataUpdateState = OP_STATE_COMMITTED;
		sec->lastUpdateTime = time_longclock();
		
		break;
		
	case REQ_SEC_OWRT:
		secOwrtParam = (SaCkptReqSecOwrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secOwrtParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->dataUpdateState != OP_STATE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section overwrite commit: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}

		index = sec->dataIndex;

		/* update replica */
		replica->checkpointSize += 
			dataLength - sec->dataLength[index];
		
		/* commit the update */
		SaCkptFree((void**)&(sec->data[index]));
		sec->dataLength[index]= 0;
		sec->dataIndex = (index + 1) % 2;
		sec->dataLength[sec->dataIndex] = dataLength;
		sec->dataUpdateState = OP_STATE_COMMITTED;
		sec->lastUpdateTime = time_longclock();
		
		break;
		
	default:
		break;
			
	}

	if (saCkptService->flagVerbose) {
		strReq = SaCkptReq2String(req);
		strErr = SaCkptErr2String(retVal);
		cl_log(LOG_INFO, 
			"Replica %s update committed, request %s, status %s",
			replica->checkpointName, strReq, strErr);
		SaCkptFree((void*)&strReq);
		SaCkptFree((void*)&strErr);
	}
	
	return SA_OK;
}


int 
SaCkptReplicaUpdRollback(SaCkptReplicaT* replica, SaCkptReqT req, 
	int dataLength, void* data,
	int paramLength, void* param)
{
	SaCkptSectionT* sec = NULL;
	
	SaCkptReqSecCrtParamT* secCrtParam = NULL;
	SaCkptReqSecDelParamT* secDelParam = NULL;
/*	SaCkptReqSecReadParamT* secReadParam = NULL; */
	SaCkptReqSecWrtParamT* secWrtParam = NULL;
	SaCkptReqSecOwrtParamT* secOwrtParam = NULL;

	char*	strReq = NULL;
	char*	strErr = NULL;
	int	retVal = SA_OK;

	int	index = 0;
	
	switch (req) {
	case REQ_SEC_CRT:
		secCrtParam = (SaCkptReqSecCrtParamT*)param;
		
		sec = SaCkptSectionFind(replica, 
			&(secCrtParam->sectionID));
		if (sec == NULL) {
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->sectionState != STATE_CREATE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section create rollback: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}

		/* remove section from section list */
		replica->sectionList = g_list_remove(
			replica->sectionList,
			(gpointer)sec);
		
		if (saCkptService->flagVerbose) {
			char* strSectionID = NULL;
			strSectionID = SaCkptSectionId2String(sec->sectionID);
			cl_log(LOG_INFO, 
				"Rollback: section %s deleted from replica %s",
				sec->sectionID.id,
				replica->checkpointName);
			SaCkptFree((void*)&strSectionID);
		}
		
		/* free section */
		if (sec->data[0] != NULL) {
			SaCkptFree((void**)&(sec->data[0]));
		}
		if (sec->data[1] != NULL) {
			SaCkptFree((void**)&(sec->data[1]));
		}
		SaCkptFree((void*)&sec);
		break;
		
	case REQ_SEC_DEL:
		secDelParam = (SaCkptReqSecDelParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secDelParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->sectionState != STATE_DELETE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section delete rollback: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}
		sec->sectionState = STATE_CREATE_COMMITTED;

		break;
		
	case REQ_SEC_WRT:
		secWrtParam = (SaCkptReqSecWrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secWrtParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->dataUpdateState != OP_STATE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section write rollback: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}

		index = (sec->dataIndex + 1) % 2;
		
		/* rollback  */
		SaCkptFree((void**)&(sec->data[index]));
		sec->dataLength[index] = 0;
		sec->dataUpdateState = OP_STATE_ROLLBACKED;

		break;
		
	case REQ_SEC_OWRT:
		secOwrtParam = (SaCkptReqSecOwrtParamT*)param;

		sec = SaCkptSectionFind(replica, 
			&(secOwrtParam->sectionID));
		if (sec == NULL) {
			cl_log(LOG_INFO, "Can not find section");
			retVal = SA_ERR_NOT_EXIST;
			break;
		}
		if (sec->dataUpdateState != OP_STATE_PREPARED) {
			cl_log(LOG_ERR, 
				"Section overwrite rollback: not prepared");
			retVal = SA_ERR_FAILED_OPERATION;
			break;
		}

		index = (sec->dataIndex + 1) % 2;

		/* rollback */
		SaCkptFree((void**)&(sec->data[index]));
		sec->dataLength[index]= 0;
		sec->dataUpdateState = OP_STATE_ROLLBACKED;

		break;
		
	default:
		break;

	}

	if (saCkptService->flagVerbose) {	
		strReq = SaCkptReq2String(req);
		strErr = SaCkptErr2String(retVal);
		cl_log(LOG_INFO, 
			"Replica %s update rollbacked, request %s, status %s",
			replica->checkpointName, strReq, strErr);
		SaCkptFree((void*)&strReq);
		SaCkptFree((void*)&strErr);
	}
	
	return SA_OK;
}


SaCkptSectionT* 
SaCkptSectionFind(SaCkptReplicaT* replica, SaCkptFixLenSectionIdT* sectionID)
{
	GList* list = replica->sectionList;
	SaCkptSectionT* sec = NULL;

	while (list != NULL) {
		sec = (SaCkptSectionT*)list->data;
		if ((sectionID->idLen == sec->sectionID.idLen) && !memcmp(sec->sectionID.id, sectionID->id,
			sectionID->idLen)) {
			return sec;
		}
		
		list = list->next;
	}

	return NULL;
}

int
SaCkptSectionRead(SaCkptReplicaT* replica,
	SaCkptSectionT* sec,
	SaSizeT offset, 
	SaSizeT* dataLength,
	void** data)
{
	if (offset > sec->dataLength[sec->dataIndex]) {
		data = NULL;
		*dataLength = 0;

		cl_log(LOG_ERR,
			"Section read failed, SA_ERR_INVALID_PARAM");

		return SA_ERR_INVALID_PARAM;
	}

	if ((offset + *dataLength) > 
		sec->dataLength[sec->dataIndex]) {
		*dataLength = 
			sec->dataLength[sec->dataIndex] - offset;
		
		cl_log(LOG_ERR,
			"Section read, read beyond the end of data");
	} 

	/* read from the end of the section */
	if (*dataLength == 0) {
		*data = NULL;
		cl_log(LOG_ERR,
			"Section read, read from the end of the section");
		
		return SA_ERR_FAILED_OPERATION;
	}

	*data = SaCkptMalloc(*dataLength);
	if (*data == NULL) {
		*dataLength = 0;
		
		cl_log(LOG_ERR,
			"Section read failed, SA_ERR_NO_MEMORY");
		
		return SA_ERR_NO_MEMORY;
	}
	
	memcpy(*data, 
		(char*)sec->data[sec->dataIndex] + offset,
		*dataLength);

	return SA_OK;
}


int 
SaCkptSectionCreate(SaCkptReplicaT* replica, 
	SaCkptReqSecCrtParamT* secCrtParam, 
	SaSizeT dataLength, void* data,
	SaCkptSectionT** pSec)
{
	SaCkptSectionT* sec = NULL;

	if (dataLength > replica->maxSectionSize) {
		cl_log(LOG_ERR,
			"Section create failed, section data too huge");
		return SA_ERR_FAILED_OPERATION;
	}

	if (replica->maxSectionNumber == replica->sectionNumber) {
		cl_log(LOG_ERR,
			"Section create failed, too many sections");
		return SA_ERR_FAILED_OPERATION;
	}

	/* if section exists, return error */
	sec = SaCkptSectionFind(replica, &(secCrtParam->sectionID));
	if (sec != NULL) {
		cl_log(LOG_ERR,
			"Section create failed, section %d already existed",
			(int)(*sec->sectionID.id));
		return SA_ERR_EXIST;
	}

	/* create section */
	sec = (SaCkptSectionT*)SaCkptMalloc(sizeof(SaCkptSectionT) + secCrtParam->sectionID.idLen);
	if (sec == NULL) {
		cl_log(LOG_ERR,
			"Section create failed, no memory");
		return SA_ERR_NO_MEMORY;
	}
	sec->replica = replica;
	
	sec->sectionID.idLen = secCrtParam->sectionID.idLen;
	memcpy(sec->sectionID.id,secCrtParam->sectionID.id,secCrtParam->sectionID.idLen);
	
	sec->expirationTime = secCrtParam->expireTime;
	sec->lastUpdateTime = time_longclock();
	sec->dataIndex = 0;
	sec->dataUpdateState = OP_STATE_COMMITTED;
	sec->sectionState = STATE_CREATE_PREPARED;
	sec->dataState = SA_CKPT_SECTION_VALID;
	sec->dataLength[0] = dataLength;
	if (dataLength > 0) {
		sec->data[0] = SaCkptMalloc(dataLength);
		if (sec->data[0] == NULL) {
			SaCkptFree((void*)&sec);
			cl_log(LOG_ERR,
				"Section create failed, no memory");
			return SA_ERR_NO_MEMORY;
		} else {
			memcpy(sec->data[0], data, dataLength);
		}
	} else {
		sec->data[0] = NULL;
	}
	sec->dataLength[1] = 0;
	sec->data[1] = NULL;

	/* add it to section list */
	replica->sectionList = g_list_append(
		replica->sectionList,
		(gpointer)sec);
	
	if (saCkptService->flagVerbose) {
		char* strSectionID = NULL;
		strSectionID = SaCkptSectionId2String(sec->sectionID);
		cl_log(LOG_INFO, 
			"Section %s created in replica %s",
			sec->sectionID.id,
			replica->checkpointName);
		SaCkptFree((void*)&strSectionID);
	}

	*pSec = sec;
	
	return SA_OK;
}


int
SaCkptSectionDelete(SaCkptReplicaT* replica, 
	SaCkptFixLenSectionIdT* sectionID)
{
	SaCkptSectionT* sec = NULL;

	sec = SaCkptSectionFind(replica, sectionID);

	if (sec == NULL) {
		cl_log(LOG_ERR,
			"Section delete failed, section does not exist");
		return SA_ERR_NOT_EXIST;
	}

	/* free section */
	if (sec->data[0] != NULL) {
		SaCkptFree((void**)&(sec->data[0]));
	}
	if (sec->data[1] != NULL) {
		SaCkptFree((void**)&(sec->data[1]));
	}
	SaCkptFree((void*)&sec);

	return HA_OK;
}

int
SaCkptSectionWrite(SaCkptReplicaT* replica,
	SaCkptSectionT* sec,
	SaSizeT offset, SaSizeT dataLength, void* data)
{
	int	index = 0;
	
	index = (sec->dataIndex + 1) % 2;
	
	if ((offset + dataLength) < sec->dataLength[sec->dataIndex]){
		sec->dataLength[index] = sec->dataLength[sec->dataIndex];
	} else {
		sec->dataLength[index] = offset + dataLength;
	}

	if (sec->dataLength[index] > replica->maxSectionSize) {
		cl_log(LOG_ERR,
			"Section write failed, section date too huge");
		return SA_ERR_FAILED_OPERATION;
	}

	if (sec->data[index] != NULL) {
		SaCkptFree((void**)&(sec->data[index]));
	}
	sec->data[index] = SaCkptMalloc(sec->dataLength[index]);
	if (sec->data[index] == NULL) {
		cl_log(LOG_ERR,
			"Section write failed, no memory");
		return SA_ERR_NO_MEMORY;
	}
	memcpy(sec->data[index], 
		sec->data[sec->dataIndex],
		sec->dataLength[sec->dataIndex]);
	memcpy((char*)(sec->data[index]) + offset,
		data, dataLength);

	sec->dataUpdateState = OP_STATE_PREPARED;

	return SA_OK;

}


int
SaCkptSectionOverwrite(SaCkptReplicaT* replica, 
	SaCkptSectionT* sec,
	SaSizeT dataLength, void* data)
{
	int	index = 0;

	if (dataLength > replica->maxSectionSize) {
		cl_log(LOG_ERR,
			"Section overwrite failed, section date too huge");
		return SA_ERR_FAILED_OPERATION;
	}

	index = (sec->dataIndex + 1) % 2;

	if (sec->data[index] != NULL) {
		SaCkptFree((void**)&(sec->data[index]));
	}
	sec->data[index] = SaCkptMalloc(dataLength);
	if (sec->data[index] == NULL) {
		cl_log(LOG_ERR,
			"Section overwrite failed, no memory");
		return SA_ERR_NO_MEMORY;
	}
	memcpy(sec->data[index], data,	dataLength);

	sec->dataUpdateState = OP_STATE_PREPARED;

	return SA_OK;
}

/* start replica retention timer */
void 
SaCkptReplicaStartTimer(SaCkptReplicaT* replica)
{
	replica->retentionTimeoutTag = 
		Gmain_timeout_add(
			replica->retentionTimeoutTag/1000000, 
			SaCkptRetentionTimeout, 
			(gpointer)replica);

	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO, 
			"Start retention timer %u for checkpoint %s",
			replica->retentionTimeoutTag,
			replica->checkpointName);
	}
	
	return;
}

/* stop replica retention timer */
void 
SaCkptReplicaStopTimer(SaCkptReplicaT* replica)
{
	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO, 
			"Delete retention timer %d for checkpoint %s",
			replica->retentionTimeoutTag,
			replica->checkpointName);
	}
	
	g_source_remove(
		replica->retentionTimeoutTag);
	replica->retentionTimeoutTag = 0;
	
	return;
}

/* after a node failure, remove it from node list */
void 
SaCkptReplicaNodeFailure(gpointer key, 
	gpointer value, 
	gpointer userdata)
{
	SaCkptReplicaT* replica;
	SaCkptOpenCheckpointT* openCkpt = NULL;
	SaCkptStateT* state;
	char* strNodeName = NULL;
	GList* list = NULL;

	replica = (SaCkptReplicaT*)value;
	strNodeName = (char*)userdata;

	list = replica->nodeList;
	while (list != NULL) {
		state = (SaCkptStateT*)list->data;
		if (!strcmp(state->nodeName, strNodeName)) {
			cl_log(LOG_INFO,
				"Replica %s, remove node %s from node list",
				replica->checkpointName,
				strNodeName);
			
			replica->nodeList = g_list_remove(
				replica->nodeList,
				(gpointer)state);
			break;
		}

		list = list->next;
	}

	if (replica->flagIsActive) {
		g_hash_table_foreach(replica->operationHash,
			SaCkptOperationNodeFailure,
			strNodeName);
	} else {
		/* 
		 * the other replica has dead, set the local
		 * replica as the active replica
		 *
		 * FIXME: how to choose active replica?
		 */

		cl_log(LOG_INFO, 
			"Replica %s, set myself as active",
			replica->checkpointName);
		
		replica->flagIsActive = TRUE;
		strcpy(replica->activeNodeName, 
			saCkptService->nodeName);

		/* update opened checkpoints */
		list = replica->openCheckpointList;
		while (list != NULL) {
			openCkpt = list->data;
			strcpy(openCkpt->activeNodeName,
				replica->activeNodeName);

			list = list->next;
		}

		replica->flagReplicaLock = FALSE;
		if (saCkptService->flagVerbose) {
			cl_log(LOG_INFO,
				"Replica %s unlocked",
				replica->checkpointName);
		}
	}

	return;
}


char* 
SaCkptSectionId2String(SaCkptFixLenSectionIdT sectionId)
{
	char* strSectionId = NULL, *pSec = NULL;
	int i = 0;
	strSectionId = (char*)SaCkptMalloc(sectionId.idLen * 4 + strlen("\0"));
	pSec = strSectionId;
	SACKPTASSERT(strSectionId != NULL);

	for(i =0; i< sectionId.idLen; i++)
	{
		sprintf(pSec, "%d",sectionId.id[i]);
		pSec ++;
	}
	strncat(pSec, "\0", strlen("\0")); /* FIXME, if you care */

	return strSectionId;
}

void 
SaCkptDumpReplica(SaCkptReplicaT* replica){
	GList * list = NULL;
	SaCkptSectionT * section = NULL;
	int i = 0;
	cl_log(LOG_INFO, "\treplica info");
	cl_log(LOG_INFO, "\tname is %s",replica->checkpointName);
	cl_log(LOG_INFO, "\tsection number is %d",(int)replica->sectionNumber);
	list = replica->sectionList;
	
	while(list != NULL){
		section = (SaCkptSectionT *)list->data;
		cl_log(LOG_INFO, "\t %d section length is %d, name is %s",i++,section->sectionID.idLen,section->sectionID.id);
		list=list->next;
		
	}
}
