/* $Id: client.c,v 1.6 2004/02/17 22:12:02 lars Exp $ */
/* 
 * client.c: 
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

#include <portability.h>
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

void 
SaCkptClientDelete(SaCkptClientT** pClient)
{
	SaCkptClientT* client = *pClient;

	cl_log(LOG_INFO, 
		"Client %d deleted, PID %d, ThreadID %d",
		client->clientHandle,
		client->pid, 
		client->threadID);
	
	g_hash_table_remove(saCkptService->clientHash, 
		(gpointer)&(client->clientHandle));

	g_hash_table_destroy(client->requestHash);
	g_list_free(client->openCheckpointList);
	g_list_free(client->pendingRequestList);

	SaCkptFree((void*)&client);

	*pClient = NULL;

	return;
}

SaCkptClientT* 
SaCkptClientCreate(SaCkptReqInitParamT* initParam)
{
	SaCkptClientT* client = NULL;

	
	client = (SaCkptClientT*) SaCkptMalloc(sizeof(SaCkptClientT));
	SACKPTASSERT (client != NULL);
	client->saCkptService = saCkptService;
	saCkptService->nextClientHandle++;
	if (saCkptService->nextClientHandle <=0) {
		saCkptService->nextClientHandle = 1;
	}
	client->clientHandle = saCkptService->nextClientHandle;
	strcpy(client->hostName, saCkptService->nodeName);
	client->pid = initParam->pid;
	client->threadID= initParam->tid;
	client->requestHash = g_hash_table_new(g_int_hash, g_int_equal);
	client->pendingRequestList = NULL;
	client->openCheckpointList = NULL;

	g_hash_table_insert(saCkptService->clientHash,
		(gpointer)&(client->clientHandle), 
		(gpointer)client);
	
	cl_log(LOG_INFO, "Client %d added, PID %d, ThreadID %d",
		client->clientHandle,
		client->pid, 
		client->threadID);

	return client;
}

void 
SaCkptClientNodeFailure(gpointer key, 
	gpointer value, 
	gpointer userdata)
{
	SaCkptClientT* client = value;
	char* strNodeName = userdata;

	SaCkptOpenCheckpointT* openCkpt = NULL;
	GList* list = NULL;

	list = client->openCheckpointList;
	while (list != NULL) {
		openCkpt = list->data;
		/* 
		 * if the opened chekcpoint has no local copy and  
		 * the remove node failed, close it 
		 */
		if ((openCkpt->flagLocalReplica == FALSE) &&
			!strcmp(openCkpt->activeNodeName, strNodeName)) {
			SaCkptCheckpointClose(&openCkpt);
		}

		list = list->next;
	}

	g_hash_table_foreach(client->requestHash,
		SaCkptRequestNodeFailure,
		strNodeName);

	return;
}


