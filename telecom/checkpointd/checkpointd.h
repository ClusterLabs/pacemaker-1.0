/* $Id: checkpointd.h,v 1.4 2004/04/02 05:16:48 deng.pan Exp $ */
#ifndef _CHECKPOINTD_H
#define _CHECKPOINTD_H

#include <glib.h>

#include <saf/ais.h>

/* 
 * The default timeout value in seconds 
 */
#define REQUEST_TIMEOUT		10
#define OPERATION_TIMEOUT	 8

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

	/* 
	 * the unlinked checkpoint name
	 */
	GHashTable	*unlinkedCheckpointHash;
	
	int	nextClientHandle;
	int	nextCheckpointHandle;

	gboolean	flagDaemon;
	gboolean	flagVerbose;
} SaCkptServiceT;	

#endif
