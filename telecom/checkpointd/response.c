/* $Id: response.c,v 1.11 2005/03/16 17:11:15 lars Exp $ */
/* 
 * response.c
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
#include <clplumbing/base64.h>
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

int
SaCkptResponseSend(SaCkptResponseT** pCkptResp)
{
	SaCkptResponseT* ckptResp = *pCkptResp;
	SaCkptRequestT* ckptReq = ckptResp->ckptReq;
	IPC_Channel* chan = ckptReq->clientChannel;

	IPC_Message* ipcMsg = NULL;
	char* p = NULL;

	char* strErr = NULL;
	char* strReq = NULL;

	if (saCkptService->flagVerbose) {
		strReq = SaCkptReq2String(ckptReq->clientRequest->req);
		strErr = SaCkptErr2String(ckptResp->resp->retVal);
		cl_log(LOG_INFO,
			"Send response for request %lu (%s)",
			ckptResp->resp->requestNO, strReq);
		cl_log(LOG_INFO,
			"\tclient %d, data length %lu, status %s",
			ckptResp->resp->clientHandle, 
			(unsigned long)ckptResp->resp->dataLength, strErr);
		SaCkptFree((void*)&strErr);
		SaCkptFree((void*)&strReq);
	}

	/* build response message */
	ipcMsg = (IPC_Message*)SaCkptMalloc(sizeof(IPC_Message));
	SACKPTASSERT(ipcMsg != NULL);
	
	ipcMsg->msg_private = NULL;
	ipcMsg->msg_done = NULL;
	ipcMsg->msg_ch = ckptReq->clientChannel;
	ipcMsg->msg_len = sizeof(SaCkptClientResponseT) - 
		sizeof(void*) + 
		ckptResp->resp->dataLength;
	ipcMsg->msg_body = SaCkptMalloc(ipcMsg->msg_len);
	SACKPTASSERT(ipcMsg->msg_body != NULL);

	p = ipcMsg->msg_body;
	
	memcpy(p, ckptResp->resp, 
		sizeof(SaCkptClientResponseT) - sizeof(void*));
	p += sizeof(SaCkptClientResponseT) - sizeof(void*);

	if (ckptResp->resp->dataLength > 0) {
		memcpy(p, ckptResp->resp->data, 
			ckptResp->resp->dataLength);
	}
	
#if 0
	cl_log(LOG_DEBUG,
		"msglen %d, initHandle %d, reqno %lu, retval %d, dataLen %lu\n", 
		ipcMsg->msg_len,
		*(SaCkptHandleT*)(ipcMsg->msg_body),
		*(SaUint32T*)((char*)ipcMsg->msg_body+4),
		*(SaErrorT*)((char*)ipcMsg->msg_body+8),
		*(SaUint32T*)((char*)ipcMsg->msg_body+12));
#endif 

	/* send response message */
	if (chan->ch_status == IPC_CONNECT) {
		while (chan->ops->send(chan, ipcMsg) == IPC_FAIL) {
			cl_log(LOG_ERR, "Send response failed");
			cl_log(LOG_ERR, "Sleep for a while and try again");
			cl_shortsleep();
		}
		chan->ops->waitout(chan);
	}
	
	/* free ipc message */
	if (ipcMsg != NULL) {
		if (ipcMsg->msg_body != NULL) {
			SaCkptFree((void**)&(ipcMsg->msg_body));
		}
		SaCkptFree((void*)&ipcMsg);
	}

	/* free response */
	if (ckptResp->resp != NULL) {
		if (ckptResp->resp->data != NULL) {
			SaCkptFree((void**)&(ckptResp->resp->data));
		}
		SaCkptFree((void**)&(ckptResp->resp));
	}
	SaCkptFree((void*)&ckptResp);

	*pCkptResp = NULL;

	/* the end of the request */
	if (saCkptService->flagVerbose) {
		cl_log(LOG_INFO, "--->>>");
	}
	
	/* remove request */
	SaCkptRequestRemove(&ckptReq);

	return HA_OK;
}


SaCkptResponseT* 
SaCkptResponseCreate(SaCkptRequestT* ckptReq)
{
	SaCkptResponseT* ckptResp = NULL;

	ckptResp = (SaCkptResponseT*)SaCkptMalloc(
		sizeof(SaCkptResponseT));
	SACKPTASSERT(ckptResp != NULL);
	ckptResp->ckptReq = ckptReq;

	ckptResp->resp = SaCkptMalloc(sizeof(SaCkptClientResponseT));
	SACKPTASSERT(ckptResp->resp != NULL);
	ckptResp->resp->clientHandle = ckptReq->clientRequest->clientHandle;
	ckptResp->resp->requestNO= ckptReq->clientRequest->requestNO;
	ckptResp->resp->dataLength = 0;
	ckptResp->resp->data = NULL;
	ckptResp->resp->retVal = SA_OK;

	return ckptResp;
}

