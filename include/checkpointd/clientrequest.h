/* $Id: clientrequest.h,v 1.5 2004/04/07 17:22:16 alan Exp $ */
/* checkpoint request.h */
#ifndef _CKPT_CLIENT_REQUEST_H
#define _CKPT_CLIENT_REQUEST_H

#include <saf/ais.h>

#define CKPTIPC CKPTVARLIBDIR "/ckpt.sock"
#define DEBUGIPC CKPTVARLIBDIR "/debug.sock"

#define SA_MAX_ID_LENGTH	4

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

typedef struct {
	SaUint8T	id[SA_MAX_ID_LENGTH];
	SaSizeT		idLen;
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

/* saCkptCheckpointOpen() */
typedef struct {
	SaNameT 				ckptName;
	SaCkptCheckpointOpenFlagsT 		openFlag;
	SaCkptCheckpointCreationAttributesT 	attr;
	SaTimeT					timetout; 
} SaCkptReqOpenParamT;

typedef struct {
	SaNameT 				ckptName;
	SaCkptCheckpointOpenFlagsT 		openFlag;
	SaCkptCheckpointCreationAttributesT 	attr;
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
	SaCkptFixLenSectionIdT	sectionID;
	SaTimeT 		expireTime;
} SaCkptReqSecCrtParamT;


/* saCkptSectionDelete()*/
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecDelParamT;


/* saCkptCheckpointWrite() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptFixLenSectionIdT	sectionID;
	SaSizeT	 		offset; 
} SaCkptReqSecWrtParamT;


/* saCkptCheckpointOverwrite() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptFixLenSectionIdT	sectionID;
} SaCkptReqSecOwrtParamT;


/* saCkptCheckpointRead() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptFixLenSectionIdT	sectionID;
	SaSizeT			offset; 
	SaSizeT			dataSize; 
} SaCkptReqSecReadParamT;

/* saCkptSectionExpirationTimeSet */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
	SaCkptFixLenSectionIdT	sectionID;
	SaTimeT expireTime;
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
