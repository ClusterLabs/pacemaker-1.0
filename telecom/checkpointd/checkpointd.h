#ifndef _CHECKPOINTD_H
#define _CHECKPOINTD_H

#include <glib.h>

#include <saf/ais.h>

/* timeout value in seconds */
#define CLIENT_REQUEST_TIMEOUT 	60

/* 
 * the operation timeout value should be smaller 
 * than the request timeout value 
 */
#define REQUEST_TIMEOUT		10
#define OPERATION_TIMEOUT	8

/*
 * the checkpoint service itself
 * all the global variables and configuration variables are put here
 */
typedef struct _SaCkptServiceT {
	ll_cluster_t*	heartbeat;
	char		nodeName[SA_MAX_NAME_LENGTH];
	SaVersionT	version ;

	/*
	 * the replicas on this node
	 * the checkpoint name is the hash table key 
	 */
	GHashTable	*replicaHash;
	
	/*
	 * the connected clients on this node
	 * the client handle is the hash table key
	 */
	GHashTable	*clientHash;

	/* 
	 * the opened checkpoints
	 * the checkpoint handle is the hash table key
	 */
	GHashTable	*openCheckpointHash;
	
	int	nextClientHandle;
	int	nextCheckpointHandle;

	gboolean	flagDaemon;
	gboolean	flagVerbose;
} SaCkptServiceT;	

#endif
