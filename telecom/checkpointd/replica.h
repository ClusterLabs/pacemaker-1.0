#ifndef _CKPT_REPLICA_H
#define _CKPT_REPLICA_H

#include <glib.h>

/* #include <clplumbing/longclock.h> */

#include <saf/ais.h>
#include "client.h"
#include "checkpointd.h"

/* replica and section create/delete state */
typedef enum {
	STATE_CREATE_PREPARED		= 1,
	STATE_CREATE_COMMITTED		= 2,
	STATE_DELETE_PREPARED		= 3,
	STATE_DELETE_COMMITTED		= 4
} SaCkptReplicaStateT;

typedef struct _SaCkptStateT {
	char	nodeName[SA_MAX_NAME_LENGTH];
	int	state;
} SaCkptStateT;

typedef struct _SaCkptReplicaT {
	SaCkptServiceT* saCkptService;
	
	char	checkpointName[SA_MAX_NAME_LENGTH];

	/* Section restrictions */
	SaUint32T	maxSectionNumber;
#if 0
	size_t		maxSectionSize;
#else
	SaSizeT		maxSectionSize;
#endif
	SaUint32T	maxSectionIDSize;

	SaSizeT		maxCheckpointSize;
	
	/*
	 * The total number of bytes of the checkpoint, not including 
	 * the header information. Actually, it is the sum of all 
	 * section size
	 */
	int	checkpointSize;
	
	/* Number of sections in the checkpoint */
	SaUint32T	sectionNumber;
	
	/* retention duration in seconds */
	SaTimeT	retentionDuration; 

	/* retention timeout tag */
	unsigned int	retentionTimeoutTag;

	/* whethere the retention is timeout or not */
	int	flagRetentionTimeout;

	/*
	 * The number of times the checkpoint is opened. After the number is
	 * decreased to zero, it will be purged after the retention duration
	 */
	int	referenceCount;	


	/*
	 * Synchronization flag when creating the checkpoint. The valid values
	 * are SA_CKPT_WR_ALL_REPLICAS, SA_CKPT_WR_ACTIVE_REPLICAS and
	 * SA_CKPT_WR_ACTIVE_REPLICAS_WEAK. Defined in AIS section 7.3.2.1
	 */
	SaCkptCheckpointCreationFlagsT	createFlag;


	/*
	 * The pid of the process that create the checkpoint. Maybe used for 
	 * authentication
	 */
	pid_t	ownerPID;

	/* 
	 * The checkpoint replicas, including its name and state 
	 * the valid state is NULL, prepared, committed and rollbacked
	 *
	 * while adding replica to its replica list, it may fail
	 * the commit operation is NULL operation, and 
	 * the rollback operation is to remove the replica from the list
	 */
	GList*	nodeList;

	/*
	 * replica state.
	 * the valid states are create_prepared, create_committed,
	 * delete_prepared, delete_committed, and so on
	 */
	int	replicaState;

	/* The active replica node of the checkpoint */
	char		activeNodeName[SA_MAX_NAME_LENGTH];
	gboolean	flagIsActive;

	/* The sections of the checkpoint */
	GList*	sectionList;

	/* the next operation number */
	int	nextOperationNumber;
	
	/* 
	 * the started operation queue 
	 * the operation no is the hash key
	 */
	GHashTable*	operationHash;
	
	/* 
	 * the pending operation queue 
	 *
	 * when move pending operation into queue, it should be in order
	 * so it cannot be hash table
	 */
	GList*	pendingOperationList;

	/* 
	 * the connected clients on this replica
	 * the client handle is the hash table key 
	 */
	GList*	openCheckpointList;

	/*
	 * if this flag is set, the active replica will not start any new
	 * operations. Used while opening and synchronizing replicas
	 */
	gboolean	flagPendOperation;

	/*
	 * if this flag is set, the replica is locked, the active replica 
	 * will not start any new operations. all the operations will be 
	 * added to pending operation queue.
	 */
	gboolean	flagReplicaLock;

	/*
	 * if this flag is set, all the requests will not be sent to active 
	 * replica. Instead, the requests will be added pending request queue
	 */
	gboolean	flagReplicaPending;

	/* whether this replica is unlinked or not */
	gboolean	flagUnlink;
	
 } SaCkptReplicaT;

typedef struct _SaCkptSectionT{
	SaCkptReplicaT* 	replica;
	
	SaTimeT	expirationTime;
	SaTimeT	lastUpdateTime;

	/*
	 * section state.
	 * the valid states are create_prepared, create_committed,
	 * delete_prepared, delete_committed, and so on
	 */
	int	sectionState;

	/*
	 * Section data state, can be valid SA_CKPT_SECTION_VALID or 
	 * SA_CKPT_SECTION_CORRUPTED, defined in SAF spec
	 */
	int	dataState;

	/*
	 * the valid section data index, 0 or 1
	 * 
	 * the section keeps two copies of data, one is for active
	 * and the other is  backup.
	 * whiling updating the section data, the update is performed on the 
	 * backup copy first. If all the nodes success, the backup copy becomes
	 * the active and active copy becomes the backup.  Otherwise, rollback 
	 * the backup copy.
	 */
	int	dataIndex;

	/* 
	 * the real data 
	 */
	SaSizeT	dataLength[2];
	void*	data[2];

	/* 
	 * the section data update state
	 * valid state is NULL, prepared, committed and rollback
	 * 
	 * the commit operation is swap the index
	 * the rollback operation is rollback the backup operation
	 */
	int	dataUpdateState;
	
	SaCkptFixLenSectionIdT sectionID;
	 
} SaCkptSectionT;


typedef struct _SaCkptOpenCheckpointT {
	SaCkptClientT*	client;
	
	/* 
	 * The client hostname name and the client handle 
	 * used when the client is NULL
	 */
	char	clientHostName[SA_MAX_NAME_LENGTH];
	int	clientHandle;
	
	SaCkptReplicaT* replica;
	
	/*
	 * the checkpoint name and the active replica node name 
	 * used when open remote checkpoint and replica is NULL
	 */
	char	checkpointName[SA_MAX_NAME_LENGTH];
	char	activeNodeName[SA_MAX_NAME_LENGTH];
	
	int	checkpointHandle;
	
	/*
	 * if the replica is not on the local node and opened on the
	 * remote active node, this is the handle on the remote node
	 */
	int	checkpointRemoteHandle;
	

	/*
	 * the open flags. Valid flags are SA_CKPT_CHECKPOINT_READ,
	 * SA_CKPT_CHECKPOINT_WRITE, SA_CKPT_CHECKPOINT_COLOCATED. Defined
	 * in AIS spec section 7.3.2.3
	 */
	SaCkptCheckpointOpenFlagsT	checkpointOpenFlags;

	/* whether the checkpoint has local replica on the node */
	gboolean	flagLocalReplica;
	gboolean	flagLocalClient;

	/* 
	 * if this flag is set, the active replica will not recieve any
	 * client request any more. Used while setting replica as active 
	 */
	gboolean	flagPendRequest;
} SaCkptOpenCheckpointT;

SaCkptReplicaT* SaCkptReplicaCreate(SaCkptReqOpenParamT*);
int SaCkptReplicaRemove(SaCkptReplicaT**);
int SaCkptReplicaFree(SaCkptReplicaT**);

int SaCkptReplicaPack(void**, SaSizeT*, SaCkptReplicaT*);
SaCkptReplicaT* SaCkptReplicaUnpack(void*, int);

int SaCkptReplicaUpdate(SaCkptReplicaT*, SaCkptReqT, 
	SaSizeT, void*, int, void*);
int SaCkptReplicaUpdPrepare(SaCkptReplicaT*, SaCkptReqT, 
	int, void*, int, void*);
int SaCkptReplicaUpdCommit(SaCkptReplicaT*, SaCkptReqT, 
	int, void*, int, void*);
int SaCkptReplicaUpdRollback(SaCkptReplicaT*, SaCkptReqT, 
	int, void*, int, void*);

int SaCkptReplicaRead(SaCkptReplicaT*,  
	SaSizeT*, void**, SaSizeT, void*);

int SaCkptSectionRead(SaCkptReplicaT*,	SaCkptSectionT*,
	SaSizeT, SaSizeT*, void**);

int SaCkptSectionCreate(SaCkptReplicaT*, 
	SaCkptReqSecCrtParamT*, SaSizeT, void*, SaCkptSectionT**);
int SaCkptSectionDelete(SaCkptReplicaT*, SaCkptFixLenSectionIdT*);
int SaCkptSectionWrite(SaCkptReplicaT*,	SaCkptSectionT*, 
	SaSizeT, SaSizeT, void*);
int SaCkptSectionOverwrite(SaCkptReplicaT*, SaCkptSectionT*, 
	SaSizeT, void*);

SaCkptSectionT* SaCkptSectionFind(SaCkptReplicaT*, SaCkptFixLenSectionIdT*);

SaCkptOpenCheckpointT* SaCkptCheckpointOpen(SaCkptClientT*, 
	SaCkptReplicaT*, SaCkptReqOpenParamT*);
int SaCkptCheckpointClose(SaCkptOpenCheckpointT**);

void SaCkptReplicaStartTimer(SaCkptReplicaT*);
void SaCkptReplicaStopTimer(SaCkptReplicaT*);

/* replica retention timeout routine */
gboolean SaCkptRetentionTimeout(gpointer );

/* after node failuer, remove it from replica node list */
void SaCkptReplicaNodeFailure(gpointer, gpointer, gpointer);

char* SaCkptSectionId2String(SaCkptFixLenSectionIdT);

void 
SaCkptDumpReplica(SaCkptReplicaT* replica);

#endif
