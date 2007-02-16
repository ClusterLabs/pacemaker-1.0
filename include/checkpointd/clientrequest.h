/* checkpoint request.h */
#ifndef _CKPT_CLIENT_REQUEST_H
#define _CKPT_CLIENT_REQUEST_H

#include <saf/ais.h>

#define CKPTIPC CKPTVARLIBDIR "/ckpt.sock"
#define DEBUGIPC CKPTVARLIBDIR "/debug.sock"

/* #define SA_MAX_ID_LENGTH	32 */

typedef enum {
	REQ_SERVICE_INIT 	= 1,	/* service initialization*/
	REQ_SERVICE_FINL 	= 2,	/* service finalization */
	REQ_CKPT_OPEN 		= 3,	/* open checkpoint */
	REQ_CKPT_OPEN_ASYNC	= 4,	/* open checkpoint async*/
	REQ_CKPT_CLOSE		= 5,	/* close checkpoint */
	REQ_CKPT_ULNK 		= 6,	/* unlink checkpoint */
	REQ_CKPT_RTN_SET 	= 7,	/* set checkpoint retention */
	REQ_CKPT_ACT_SET 	= 8,	/* set active checkpoint replica */
	REQ_CKPT_STAT_GET 	= 9,	/* get checkpoint status */
	REQ_SEC_CRT 		= 10,	/* create section */
	REQ_SEC_DEL 		= 11,	/* delete section */
	REQ_SEC_EXP_SET 	= 12,	/* set section expiration */
	REQ_SEC_QUERY 		= 13,	/* query section */
	REQ_SEC_WRT 		= 14,	/* write section */
	REQ_SEC_OWRT 		= 15,	/* overwrite section */
	REQ_SEC_RD 		= 16,	/* read secton */
	REQ_CKPT_SYNC 		= 17,	/* synchronize the checkpoint */
	REQ_CKPT_SYNC_ASYNC 	= 18	/* synchronize the checkpoint */
} SaCkptReqT;

/*
	Currently it is only a quick work around,it is not a good way for it may cause pointer error
	In future, it should be changed
	On attention is, whenever contain this in another structure, it should be the last one
*/
typedef struct {
	SaSizeT		idLen;
	SaUint8T	id[0];
} SaCkptFixLenSectionIdT;

/* saCkptInitialize() */
typedef struct {
	SaInt32T 	pid;
	SaInt32T 	tid;
	SaVersionT 	ver;
} SaCkptReqInitParamT;

/* saCkptFinalize() */
typedef struct {
	SaCkptHandleT	clientHandle;
} SaCkptReqFinalParamT;


typedef enum{
	RES_INVLID		=		0,
	RES_NO_REPLICA 		= 		1,	
	RES_STANDBY 		=		2,	
	RES_RACE_HIGH_PRIO	=		3,
	RES_RACE_LOW_PRIO	=		4,
	RES_HAVE_REPLICA	=		5,
	RES_NO_RESPONSE		=		6,
	RES_SELF		= 		7,
	RES_NOT_RUN		=		8,
	RES_EARLIER		= 		9,
	RES_LATER		=		10
}saOpenResponseTypeT;

typedef struct{
	char 				nodeName[SA_MAX_NAME_LENGTH];
	saOpenResponseTypeT 		status ;
	/*If this node is a low priority one, keep the original message to notify it */
	void			*originalMessage;
}saOpenNodeStatusT;

/* saCkptCheckpointOpen() */
typedef struct {
	SaNameT 				ckptName;
	SaCkptCheckpointOpenFlagsT 		openFlag;
	SaCkptCheckpointCreationAttributesT 	attr;
	/*  the node list */ 
	GList *					nodeReponse;					
	SaTimeT					timetout;
} SaCkptReqOpenParamT;

typedef struct {
	SaNameT 				ckptName;
	SaCkptCheckpointOpenFlagsT 		openFlag;
	SaCkptCheckpointCreationAttributesT 	attr;
	/* the node list */ 
	GList *					nodeReponse;					
	SaInvocationT				invocation; 
} SaCkptReqOpenAsyncParamT;


/* saCkptCheckpointClose() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
} SaCkptReqCloseParamT;


/* saCkptCheckpointUnlink() */
typedef struct {
	SaCkptHandleT	clientHandle;
	SaNameT 	ckptName;
} SaCkptReqUlnkParamT;


/* saCkptCheckpointRetentionDurationSet() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaTimeT retention;
} SaCkptReqRtnParamT;


/* saCkptActiveCheckpointSet() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
} SaCkptReqActSetParamT;


/* saCkptCheckpointStatusGet() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
} SaCkptReqStatGetParamT;


/* saCkptSectionCreate()*/
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaTimeT 		expireTime;
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecCrtParamT;


/* saCkptSectionDelete()*/
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecDelParamT;


/* saCkptCheckpointWrite() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaSizeT	 		offset; 
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecWrtParamT;


/* saCkptCheckpointOverwrite() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecOwrtParamT;


/* saCkptCheckpointRead() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaSizeT			offset; 
	SaSizeT			dataSize; 
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecReadParamT;

/* saCkptSectionExpirationTimeSet */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaTimeT expireTime;
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecExpSetParamT;

/* saCkptSectionIteratorInitialize */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptSectionsChosenT chosenFlag;
	SaTimeT expireTime;
} SaCkptReqSecQueryParamT;

/* saCkptCheckpointSynchronize() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaTimeT timeout;
} SaCkptReqSyncParamT;

/* saCkptCheckpointSynchronize() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaInvocationT invocation;
} SaCkptReqAsyncParamT;

/* the request stream in the socket */
typedef struct _SaCkptClientRequestT{
	SaCkptHandleT		clientHandle;
	SaUint32T		requestNO;

	SaCkptReqT		req;

	SaSizeT			reqParamLength;
	SaSizeT		dataLength;
	void*			reqParam;
	void*			data;
} SaCkptClientRequestT;

/* the request stream in the socket */
typedef struct _SaCkptClientResponseT {
	SaCkptHandleT	clientHandle;
	SaUint32T	requestNO;

	SaErrorT	retVal;

	SaSizeT		dataLength;
	void*		data;
} SaCkptClientResponseT;

#endif
