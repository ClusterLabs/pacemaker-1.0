/* $Id: clientrequest.h,v 1.3 2004/02/17 22:11:58 lars Exp $ */
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
	REQ_CKPT_CLOSE		= 4,	/* close checkpoint */
	REQ_CKPT_ULNK 		= 5,	/* unlink checkpoint */
	REQ_CKPT_RTN_SET 	= 6,	/* set checkpoint retention */
	REQ_CKPT_ACT_SET 	= 7,	/* set active checkpoint replica */
	REQ_CKPT_STAT_GET 	= 8,	/* get checkpoint status */
	REQ_SEC_CRT 		= 9,	/* create section */
	REQ_SEC_DEL 		= 10,	/* delete section */
	REQ_SEC_EXP_SET 	= 11,	/* set section expiration */
	REQ_SEC_QUERY 		= 12,	/* query section */
	REQ_SEC_WRT 		= 13,	/* write section */
	REQ_SEC_OWRT 		= 14,	/* overwrite section */
	REQ_SEC_RD 		= 15,	/* read secton */
	REQ_CKPT_SYNC 		= 16,	/* synchronize the checkpoint */
} SaCkptReqT;

typedef struct {
	SaUint8T	id[SA_MAX_ID_LENGTH];
	SaUint32T	idLen;
} SaCkptFixLenSectionIdT;

/*
#if 0
typedef struct _SaCkptClientRequestT{
	SaCkptHandleT		clientHandle;
	SaUint32T		requestNO;

	SaCkptReqT		req;

	SaUint32T		reqParamLength;
	SaUint32T		dataLength;
	void*			reqParam;
	void*			data;
} SaCkptClientRequestT;

typedef struct _SaCkptClientResponseT {
	SaCkptHandleT	clientHandle;
	SaUint32T	requestNO;

	SaErrorT	retVal;

	SaUint32T	dataLength;
	void*		data;
} SaCkptClientResponseT;

#endif
*/

/* Request header */
typedef struct {
	SaUint32T msglen; 
	SaCkptHandleT initHandle;
	SaUint32T reqno;
	SaCkptReqT req;
	SaUint32T paramLen; /* r2 */
	SaUint32T dataLen;  /* r2 */
} SaCkptRequestHeadT;

/* Response header */
typedef struct {
	SaUint32T msglen; /* not include the size of msglen itself */
	SaCkptHandleT initHandle;
	SaUint32T reqno;
	SaErrorT retval;
	SaUint32T dataLen;  /* r2 */
} SaCkptResponseHeadT;


/* saCkptInitialize() */
typedef struct {
	SaInt32T 	pid;
	SaInt32T 	tid;
	SaVersionT 	ver;
} SaCkptReqInitParamT;


/* saCkptCheckpointOpen() */
typedef struct {
	SaNameT 				ckptName;
	SaCkptCheckpointOpenFlagsT 		openFlag;
	SaCkptCheckpointCreationAttributesT 	attr;
	//SaTimeT					timetout; /* jerry */
} SaCkptReqOpenParamT;


/* saCkptCheckpointClose() */
typedef struct {
	SaCkptCheckpointHandleT checkpointHandle;
} SaCkptReqCloseParamT;


/* saCkptCheckpointUnlink() */
typedef struct {
	SaNameT ckptName;
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
	SaUint32T 		offset; 
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
	SaUint32T		offset; 
	SaUint32T		dataSize; 
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

#endif
