#ifndef _CHECKPOINTD_H
#define _CHECKPOINTD_H

#include <glib.h>
#include <saf/ais.h>
#include "hb_api.h"
#include "heartbeat.h"
/* 
 * The default timeout value in seconds 
 */
#define REQUEST_TIMEOUT		10
#define OPERATION_TIMEOUT	 8

#define saCkptMajorVersion	0x01
#define saCkptMinorVersion	0x01

typedef enum{
	HB_INIT,
	HB_UP,
	HB_ACTIVE,
	HB_DEAD,
	HB_UNKNOWN 
}saCkptNodeHBStatus;


typedef enum{
	CKPT_UNKNOWN,
	CKPT_DEAD,
	CKPT_NOT_INIT,
	CKPT_RUNNING
}saCkptNodeCkptStatus;

typedef struct {
	char nodeName[SA_MAX_NAME_LENGTH];
	saCkptNodeHBStatus nodeHbStatus ;
	saCkptNodeCkptStatus ckptStatus ;
}saCkptNodeInfo;
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
	
	/*
	 * the not finished open request
	 */
	 GHashTable 		*openRequestHash;
	
	/*
	 * the node status on the cluster
	 */
	GHashTable		 * nodeStatusHash;
	
	int	nextClientHandle;
	int	nextCheckpointHandle;

	gboolean	flagDaemon;
	gboolean	flagVerbose;
	
} SaCkptServiceT;	
gint checkpointNodeStatusInit(void);

saCkptNodeHBStatus transHbNodeStatus(const char *hbStatus);

void getNodeCkptStatus(gpointer key,gpointer value,
			gpointer user_data);
gint serviceBeginNotify(void);

#endif
