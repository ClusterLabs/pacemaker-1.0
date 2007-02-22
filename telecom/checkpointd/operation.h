#ifndef _CKPT_OPERATION_H
#define _CKPT_OPERATION_H

#include <glib.h>

#include <checkpointd/clientrequest.h>
#include "replica.h"

/* the valid replica operation */
typedef enum {
	OP_NULL		= 0,
	OP_CKPT_OPEN	= 1,
	OP_RPLC_CRT	= 2,
	OP_RPLC_ADD	= 3,
	OP_CKPT_UPD	= 4,
	OP_CKPT_READ	= 5,
	OP_CKPT_ULNK	= 6,
	OP_CKPT_SYNC	= 7,
	OP_CKPT_ACT_SET	= 8
} SaCkptOpT;

/* the replica operation state */
typedef enum {
	OP_STATE_PENDING	= 1,
	OP_STATE_STARTED	= 2,
	OP_STATE_PREPARED	= 3,
	OP_STATE_COMMITTED	= 4,
	OP_STATE_ROLLBACKED	= 5
} SaCkptOpStateT;

typedef struct _SaCkptOperationT{
	SaCkptReplicaT* replica;
	int	operationNO;	

	/* the operation originator */
	char		clientHostName[SA_MAX_NAME_LENGTH];
	int		clientHandle;
	int		clientRequestNO;
	SaCkptReqT	clientRequest;

	/*
	 * replica operation state, prepared, committed
	 * and rollbacked
	 */
	GList*	stateList;
	SaCkptOpStateT	state;

	/* the replica operation and its parameters */
	SaCkptOpT 	operation;
	int		paramLength;
	void*		param;
	size_t		dataLength;
	void*		data;

	/* operation timeout handler tag */
	int	timeoutTag;
}SaCkptOperationT;

/* 
 * convert replica op to string
 * used for debug purpose
 */
char* SaCkptOp2String(SaCkptOpT);

/* start and continue the operation */
void SaCkptOperationStart(SaCkptOperationT*);
void SaCkptOperationContinue(SaCkptOperationT*);

/* 
 * remove the operation from its replica hash table and
 * free its memory
 */
int SaCkptOperationRemove(SaCkptOperationT**);

/* start and stop timer for the operation */
void SaCkptOperationStartTimer(SaCkptOperationT*);
void SaCkptOperationStopTimer(SaCkptOperationT*);

/* operation timeout process routine */
gboolean SaCkptOperationTimeout(gpointer);

/* continue the operation after a node failure */
void SaCkptOperationNodeFailure(gpointer, gpointer, gpointer);

/* whether the operation is finished or not */
int SaCkptOperationFinished(char*, SaCkptOpStateT, GList*);

void
updateReplicaPendingOption(SaCkptReplicaT *replica, const char *hostName);

#endif

