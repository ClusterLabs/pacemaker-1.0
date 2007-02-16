#ifndef _CKPT_REQUEST_H
#define _CKPT_REQUEST_H

#include <glib.h>
#include <clplumbing/ipc.h>

#include <saf/ais.h>
#include <checkpointd/clientrequest.h>
#include "replica.h"
#include "client.h"
#include "operation.h"


typedef struct _SaCkptRequestT{
	SaCkptClientT*		client;
	IPC_Channel*		clientChannel;

	SaCkptClientRequestT*	clientRequest;

	/* the requested open checkpoint */
	SaCkptOpenCheckpointT*	openCkpt;
	
	/* 
	 * usually, one request need one operation
	 * but some request may require several operations
	 * for example, open request need several operations
	 */
	SaCkptOpT	operation;
	
	/* request timeout handler tag */
	guint		timeoutTag;

	/* the hostname the request had been sent to */
	char		toNodeName[SA_MAX_NAME_LENGTH];
	
} SaCkptRequestT;

/* receive request from the client */
SaCkptRequestT* SaCkptRequestReceive(IPC_Channel*);

int SaCkptRequestStart(SaCkptRequestT*);
int SaCkptRequestRemove(SaCkptRequestT**);

char* SaCkptReq2String(SaCkptReqT);


void SaCkptRequestStartTimer(SaCkptRequestT* ckptReq, SaTimeT);
void SaCkptRequestStopTimer(SaCkptRequestT* ckptReq);

/* the request process routine */
gboolean SaCkptRequestProcess(IPC_Channel*);

/* the request timeout routine */
gboolean SaCkptRequestTimeout(gpointer);

/* 
 * after the node failure, 
 * the request will be resent to the new active replica 
 */
void SaCkptRequestNodeFailure(gpointer, gpointer, gpointer );


#endif
