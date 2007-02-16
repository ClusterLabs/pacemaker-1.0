#ifndef _CKPT_CLIENT_H
#define _CKPT_CLIENT_H

#include <glib.h>

#include "checkpointd.h"
#include <checkpointd/clientrequest.h>

typedef struct _SaCkptClientT{
	SaCkptServiceT* saCkptService;
	
	/* 
	 * The client channel 
	 * clientChannel[0] is for the sync calls
	 * clientChannel[1] is for the async calls
	 */
	IPC_Channel*	channel[2];
	
	/* The handle returned to the client after initialization */
	SaCkptHandleT	clientHandle;

	char		hostName[SA_MAX_NAME_LENGTH];
	pid_t		pid;
	int		threadID;

	/* 
	 * the opened checkpoints
	 * the checkpoint handle is the hash table key
	 */
	GList*		openCheckpointList;

	/* 
	 * the sent out client request queue
	 * the request no is the key
	 */
	GHashTable*	requestHash;

	/* 
	 * the pending client request queue
	 * 
	 * when move the pending request to the request queue, it need to be
	 * in order, so it cannot be hash table.
	 */
	GList*		pendingRequestList;

} SaCkptClientT;

void SaCkptClientDelete(SaCkptClientT**);

SaCkptClientT* SaCkptClientCreate(SaCkptReqInitParamT*);

void SaCkptClientNodeFailure(gpointer, gpointer, gpointer);

#endif

