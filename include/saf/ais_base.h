/* --- ais.h
  Header file of SA Forum AIS APIs Version 1.0
  In order to compile, all opaque types which appear as <...> in 
  the spec have been defined as OPAQUE_TYPE (which is an integer).
*/
#ifndef _AIS_BASE_H_
#define _AIS_BASE_H_

/*
typedef OPAQUE_TYPE SaInvocationT;
typedef OPAQUE_TYPE SaSizeT;
typedef OPAQUE_TYPE SaOffsetT;
typedef OPAQUE_TYPE SaSelectionObjectT;
typedef OPAQUE_TYPE SaAmfHandleT;
typedef OPAQUE_TYPE SaClmHandleT;
typedef OPAQUE_TYPE SaCkptHandleT;
typedef OPAQUE_TYPE SaCkptCheckpointHandleT;
typedef OPAQUE_TYPE SaCkptSectionIteratorT;
typedef OPAQUE_TYPE SaEvtHandleT;
typedef OPAQUE_TYPE SaEvtEventHandleT;
typedef OPAQUE_TYPE SaEvtChannelHandleT;
typedef OPAQUE_TYPE SaMsgHandleT;
typedef OPAQUE_TYPE SaMsgMessageHandleT;
typedef OPAQUE_TYPE SaMsgQueueHandleT;
typedef OPAQUE_TYPE SaMsgSenderIdT;
typedef OPAQUE_TYPE SaLckHandleT;
typedef OPAQUE_TYPE SaLckLockIdT;
typedef OPAQUE_TYPE SaLckResourceIdT;
*/


/* Chapter 3 */
#define OPAQUE_TYPE  int

typedef OPAQUE_TYPE SaInvocationT;
typedef OPAQUE_TYPE SaSizeT;
typedef OPAQUE_TYPE SaOffsetT;
typedef OPAQUE_TYPE SaSelectionObjectT;

typedef enum {
    SA_FALSE = 0,
    SA_TRUE = 1
} SaBoolT;

typedef char  			SaInt8T;
typedef short 			SaInt16T;
typedef long  			SaInt32T;
typedef long long 		SaInt64T;
typedef unsigned char 		SaUint8T;
typedef unsigned short 		SaUint16T;
typedef unsigned long 		SaUint32T;
typedef unsigned long long 	SaUint64T;
typedef SaInt64T 		SaTimeT;

/* 
 * the largest timestamp value: 
 * Fri Apr 11 23:47:16.854775807 UTC 2262 
 */
#define SA_TIME_END ((SaTimeT)0x7FFFFFFFFFFFFFFFLL)

/*
 * the smallest timestamp value: 
 * Tue Sep 21 00:12:43.145224193 UTC 1667
 */
#define SA_TIME_BEGIN ((SaTimeT)0x8000000000000001LL)

#define SA_MAX_NAME_LENGTH	32
#define SA_MAX_ID_LENGTH	128

typedef struct {
    SaUint16T length;
    char value[SA_MAX_NAME_LENGTH];
} SaNameT;

typedef struct {
    char releaseCode;
    unsigned char major;
    unsigned char minor;
} SaVersionT;

#define SA_TRACK_CURRENT 0x01
#define SA_TRACK_CHANGES 0x02
#define SA_TRACK_CHANGES_ONLY 0x04

typedef enum {
    SA_DISPATCH_ONE = 1,
    SA_DISPATCH_ALL = 2,
    SA_DISPATCH_BLOCKING = 3
} SaDispatchFlagsT;

typedef enum {
    SA_OK = 1,
    SA_ERR_LIBRARY = 2,
    SA_ERR_VERSION = 3,
    SA_ERR_INIT = 4,
    SA_ERR_TIMEOUT = 5,
    SA_ERR_TRY_AGAIN = 6,
    SA_ERR_INVALID_PARAM = 7,
    SA_ERR_NO_MEMORY = 8,
    SA_ERR_BAD_HANDLE = 9,
    SA_ERR_BUSY = 10,
    SA_ERR_ACCESS = 11,
    SA_ERR_NOT_EXIST = 12,
    SA_ERR_NAME_TOO_LONG = 13,
    SA_ERR_EXIST = 14,
    SA_ERR_NO_SPACE = 15,
    SA_ERR_INTERRUPT =16,
    SA_ERR_SYSTEM = 17,
    SA_ERR_NAME_NOT_FOUND = 18,
    SA_ERR_NO_RESOURCES = 19,
    SA_ERR_NOT_SUPPORTED = 20,
    SA_ERR_BAD_OPERATION = 21,
    SA_ERR_FAILED_OPERATION = 22,
    SA_ERR_MESSAGE_ERROR = 23,
    SA_ERR_NO_MESSAGE = 24,
    SA_ERR_QUEUE_FULL = 25,
    SA_ERR_QUEUE_NOT_AVAILABLE = 26,
    SA_ERR_BAD_CHECKPOINT = 27,
    SA_ERR_BAD_FLAGS = 28
} SaErrorT;

#endif /* _AIS_BASE_H_ */
