#ifndef _CKPT_MESSAGE_H
#define _CKPT_MESSAGE_H

#include <glib.h>

#include <ha_msg.h>

#include <checkpointd/clientrequest.h>
#include "request.h"
#include "operation.h"

/* checkpoint message type */
#define T_CKPT				"ckpt"

/* checkpointd message fields */
#define F_CKPT_SUBTYPE			"cksub"
#define F_CKPT_VERSION			"ckver"
#define F_CKPT_CHECKPOINT_NAME		"ckname"
#define F_CKPT_ACTIVE_NODENAME		"ckact"

#define F_CKPT_CLIENT_HOSTNAME		"cname"
#define F_CKPT_CLIENT_HANDLE		"chdl"

#define F_CKPT_CLIENT_REQUEST		"ckreq"
#define F_CKPT_CLIENT_REQUEST_NO	"creqno"

#define F_CKPT_OPERATION		"ckop"
#define F_CKPT_OPERATION_NO		"ckopno"

#define F_CKPT_PARAM			"ckpar"
#define F_CKPT_PARAM_LENGTH		"ckparl"

#define F_CKPT_DATA			"ckdat"
#define F_CKPT_DATA_LENGTH		"ckdatl"

#define F_CKPT_RETVAL			"ckrt"

/* checkpoint message subtypes */
typedef enum {
	M_NULL,

	/* checkpoint open message */
	M_CKPT_OPEN_BCAST,
	M_CKPT_OPEN_BCAST_REPLY,
	M_RPLC_CRT,
	M_RPLC_CRT_REPLY,
	M_RPLC_ADD,
	M_RPLC_ADD_REPLY,
	M_RPLC_ADD_PREPARE_BCAST,
	M_RPLC_ADD_PREPARE_BCAST_REPLY,
	M_RPLC_ADD_COMMIT_BCAST,
	M_RPLC_ADD_COMMIT_BCAST_REPLY,
	M_RPLC_ADD_ROLLBACK_BCAST,
	M_RPLC_ADD_ROLLBACK_BCAST_REPLY,

	/* open checkpoint without COLOCATED flag */
	M_CKPT_OPEN_REMOTE,
	M_CKPT_OPEN_REMOTE_REPLY,

	/* close remote checkpoint message */
	M_CKPT_CLOSE_REMOTE,
	M_CKPT_CLOSE_REMOTE_REPLY,

	/* checkpoint create */
	M_CKPT_CKPT_CREATE_BCAST,
	M_CKPT_CKPT_CREATE_BCAST_REPLY,

	/* checkpoint close message */
	M_RPLC_DEL,
	M_RPLC_DEL_REPLY,
	M_RPLC_DEL_BCAST,
	M_RPLC_DEL_BCAST_REPLY,

	/* checkpoint update message */
	M_CKPT_UPD,
	M_CKPT_UPD_REPLY,
	M_CKPT_UPD_PREPARE_BCAST,
	M_CKPT_UPD_PREPARE_BCAST_REPLY,
	M_CKPT_UPD_COMMIT_BCAST,
	M_CKPT_UPD_COMMIT_BCAST_REPLY,
	M_CKPT_UPD_ROLLBACK_BCAST,
	M_CKPT_UPD_ROLLBACK_BCAST_REPLY,
	M_CKPT_UPD_BCAST,
	M_CKPT_UPD_BCAST_REPLY,

	/* checkpoint sync message */
	M_CKPT_SYNC,
	M_CKPT_SYNC_REPLY,
	M_CKPT_SYNC_FINISH,

	/* checkpoint set active message */
	M_CKPT_SETACTIVE,
	M_CKPT_PENDING_BCAST,
	M_CKPT_PENDING_BCAST_REPLY,
	M_CKPT_SETACTIVE_REPLY,
	M_CKPT_SETACTIVE_BCAST,

	/* checkpoint read message */
	M_CKPT_READ,
	M_CKPT_READ_REPLY
} SaCkptMsgSubtypeT;


typedef struct _SaCkptMessageT {
	char		msgType[SA_MAX_NAME_LENGTH];
	SaCkptMsgSubtypeT	msgSubtype;

	SaVersionT	msgVersion;
	
	char		fromNodeName[SA_MAX_NAME_LENGTH];

	char		checkpointName[SA_MAX_NAME_LENGTH];

	char		clientHostName[SA_MAX_NAME_LENGTH];
	int		clientHandle;
	SaCkptReqT	clientRequest;
	int		clientRequestNO;
	
	char		activeNodeName[SA_MAX_NAME_LENGTH];
	SaCkptOpT	operation;
	int		operationNO;
	
	size_t		paramLength;
	void*		param;
	size_t		dataLength;
	void*		data;

	SaErrorT	retVal;

	/* linux-HA message */
	char		hamsgNodeName[SA_MAX_NAME_LENGTH];
	char		hamsgStatus[SA_MAX_NAME_LENGTH];
} SaCkptMessageT;

/* message process routine */
gboolean SaCkptClusterMsgProcess(void);

/* receive message from heartbeat daemon */
SaCkptMessageT* SaCkptMessageReceive(void);

/* send message */
int SaCkptMessageSend(SaCkptMessageT*, char*);
int SaCkptMessageMulticast(SaCkptMessageT*, GList*);
int SaCkptMessageBroadcast(SaCkptMessageT*);

/* convert to and from Linux-HA message */
struct ha_msg* SaCkptMessage2Hamsg(SaCkptMessageT*);
SaCkptMessageT* SaHamsg2CkptMessage(struct ha_msg*);

/* create message from request and operation */
SaCkptMessageT* SaCkptMessageCreateReq(SaCkptRequestT*, SaCkptMsgSubtypeT);
SaCkptMessageT* SaCkptMessageCreateOp(SaCkptOperationT*, SaCkptMsgSubtypeT);
void SaCkptMessageDelete(SaCkptMessageT**);

/* create operation from message */
SaCkptOperationT* SaCkptOperationCreate(SaCkptMessageT*, SaCkptReplicaT*);

char* SaCkptMsgSubtype2String(SaCkptMsgSubtypeT);


#endif  /* _SA_CKPT_MESSAGE_H */

