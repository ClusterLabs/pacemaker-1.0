/* --- ais.h
  Header file of SA Forum AIS APIs Version 1.0
  In order to compile, all opaque types which appear as <...> in 
  the spec have been defined as OPAQUE_TYPE (which is an integer).
*/
#ifndef _AIS_H_
#define _AIS_H_

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

/* added by steve jin */

#define SA_TIME_END 0 
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

#define SA_MAX_NAME_LENGTH	32
#define SA_MAX_ID_LENGTH	4

typedef struct {
    SaUint16T length;
    unsigned char value[SA_MAX_NAME_LENGTH];
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

/* Chapter 7 */
typedef OPAQUE_TYPE SaCkptHandleT;
typedef OPAQUE_TYPE SaCkptCheckpointHandleT;
typedef OPAQUE_TYPE SaCkptSectionIteratorT;

#define SA_CKPT_WR_ALL_REPLICAS        0X1
#define SA_CKPT_WR_ACTIVE_REPLICA      0X2
#define SA_CKPT_WR_ACTIVE_REPLICA_WEAK 0X4
typedef SaUint32T SaCkptCheckpointCreationFlagsT;

typedef struct {
    SaCkptCheckpointCreationFlagsT creationFlags;
    SaSizeT checkpointSize;
    SaTimeT retentionDuration;
    SaUint32T maxSections;
    SaSizeT maxSectionSize;
    SaUint32T maxSectionIdSize;
} SaCkptCheckpointCreationAttributesT;

#define SA_CKPT_CHECKPOINT_READ      0X1
#define SA_CKPT_CHECKPOINT_WRITE     0X2
#define SA_CKPT_CHECKPOINT_COLOCATED 0X4
typedef SaUint32T SaCkptCheckpointOpenFlagsT;

#define SA_CKPT_DEFAULT_SECTION_ID   {NULL, 0}
#define SA_CKPT_GENERATED_SECTION_ID {NULL, 0}

typedef struct {
    SaUint8T id[SA_MAX_ID_LENGTH];		// Pan Deng
    SaUint32T idLen;
} SaCkptSectionIdT;

typedef struct {
    SaCkptSectionIdT *sectionId;
    SaTimeT expirationTime;
} SaCkptSectionCreationAttributesT;

typedef enum {
    SA_CKPT_SECTION_VALID = 1,
    SA_CKPT_SECTION_CORRUPTED = 2
} SaCkptSectionStateT;

typedef struct {
    SaCkptSectionIdT sectionId;
    SaTimeT expirationTime;
    SaSizeT sectionSize;
    SaCkptSectionStateT sectionState;
    SaTimeT lastUpdate;
} SaCkptSectionDescriptorT;

typedef enum {
    SA_CKPT_SECTIONS_FOREVER = 1,
    SA_CKPT_SECTIONS_LEQ_EXPIRATION_TIME = 2,
    SA_CKPT_SECTIONS_GEQ_EXPIRATION_TIME = 3,
    SA_CKPT_SECTIONS_CORRUPTED = 4,
    SA_CKPT_SECTIONS_ANY = 5
} SaCkptSectionsChosenT;

typedef struct {
    SaCkptSectionIdT sectionId;
    void *dataBuffer;
    SaSizeT dataSize;
    SaOffsetT dataOffset;
    SaSizeT readSize; //[out]
} SaCkptIOVectorElementT;


typedef struct {
    SaCkptCheckpointCreationAttributesT checkpointCreationAttributes;
    SaUint32T numberOfSections;
    SaUint32T memoryUsed;
} SaCkptCheckpointStatusT;


typedef void 
(*SaCkptCheckpointOpenCallbackT)(SaInvocationT invocation,
                                 const SaCkptCheckpointHandleT 
                                     *checkpointHandle,
                                 SaErrorT error);
typedef void 
(*SaCkptCheckpointSynchronizeCallbackT)(SaInvocationT invocation,
                                        SaErrorT error);

typedef struct {
    SaCkptCheckpointOpenCallbackT saCkptCheckpointOpenCallback;
    SaCkptCheckpointSynchronizeCallbackT saCkptCheckpointSynchronizeCallback;
} SaCkptCallbacksT;

    SaErrorT 
saCkptInitialize(SaCkptHandleT *ckptHandle, const SaCkptCallbacksT *callbacks,
                 const SaVersionT *version);

    SaErrorT 
saCkptSelectionObjectGet(const SaCkptHandleT *ckptHandle,
                         SaSelectionObjectT *selectionObject);

    SaErrorT 
saCkptDispatch(const SaCkptHandleT *ckptHandle, 
               SaDispatchFlagsT dispatchFlags);

    SaErrorT 
saCkptFinalize(const SaCkptHandleT *ckptHandle);

    SaErrorT
saCkptCheckpointOpen(
					 const SaCkptHandleT *ckptHandle,
					 const SaNameT *ckeckpointName,
                     const SaCkptCheckpointCreationAttributesT 
                         *checkpointCreationAttributes,
                     SaCkptCheckpointOpenFlagsT checkpointOpenFlags,
                     SaTimeT timeout,
                     SaCkptCheckpointHandleT *checkpointHandle);

    SaErrorT 
saCkptCheckpointOpenAsync(const SaCkptHandleT *ckptHandle,
                          SaInvocationT invocation,
                          const SaNameT *ckeckpointName,
                          const SaCkptCheckpointCreationAttributesT 
                              *checkpointCreationAttributes,
                          SaCkptCheckpointOpenFlagsT checkpointOpenFlags);

    SaErrorT
saCkptCheckpointClose(const SaCkptCheckpointHandleT *checkpointHandle);

    SaErrorT 
saCkptCheckpointUnlink(
	const SaCkptHandleT *ckptHandle,
	const SaNameT *checkpointName);

    SaErrorT 
saCkptCheckpointRetentionDurationSet(const SaCkptCheckpointHandleT 
                                         *checkpointHandle,
                                     SaTimeT retentionDuration);

    SaErrorT 
saCkptActiveCheckpointSet(const SaCkptCheckpointHandleT *checkpointHandle);

    SaErrorT 
saCkptCheckpointStatusGet(const SaCkptCheckpointHandleT *checkpointHandle,
                          SaCkptCheckpointStatusT *checkpointStatus);

    SaErrorT 
saCkptSectionCreate(const SaCkptCheckpointHandleT *checkpointHandle,
                    SaCkptSectionCreationAttributesT 
                        *sectionCreationAttributes,
                    const void *initialData,
                    SaUint32T initialDataSize);

    SaErrorT 
saCkptSectionDelete(const SaCkptCheckpointHandleT *checkpointHandle,
                    const SaCkptSectionIdT *sectionId);


    SaErrorT 
saCkptSectionExpirationTimeSet(const SaCkptCheckpointHandleT *checkpointHandle,
                               const SaCkptSectionIdT* sectionId,
                               SaTimeT expirationTime);

    SaErrorT 
saCkptSectionIteratorInitialize(const SaCkptCheckpointHandleT 
                                    *checkpointHandle,
                                SaCkptSectionsChosenT sectionsChosen,
                                SaTimeT expirationTime,
                                SaCkptSectionIteratorT *sectionIterator);

    SaErrorT 
saCkptSectionIteratorNext(SaCkptSectionIteratorT *sectionIterator,
                          SaCkptSectionDescriptorT *sectionDescriptor);

    SaErrorT 
saCkptSectionIteratorFinalize(SaCkptSectionIteratorT *sectionIterator);

    SaErrorT 
saCkptCheckpointWrite(const SaCkptCheckpointHandleT *checkpointHandle,
                      const SaCkptIOVectorElementT *ioVector,
                      SaUint32T numberOfElements,
                      SaUint32T *erroneousVectorIndex);

    SaErrorT 
saCkptSectionOverwrite(const SaCkptCheckpointHandleT *checkpointHandle,
                       const SaCkptSectionIdT *sectionId,
                       SaUint8T *dataBuffer,
                       SaSizeT dataSize);

    SaErrorT 
saCkptCheckpointRead(const SaCkptCheckpointHandleT *checkpointHandle,
                     SaCkptIOVectorElementT *ioVector,
                     SaUint32T numberOfElements,
                     SaUint32T *erroneousVectorIndex);

    SaErrorT 
saCkptCheckpointSynchronize(const SaCkptCheckpointHandleT *ckeckpointHandle,
                            SaTimeT timeout);

    SaErrorT 
saCkptCheckpointSynchronizeAsync(const SaCkptHandleT *ckptHandle,
                                 SaInvocationT invocation,
                                 const SaCkptCheckpointHandleT 
                                     *checkpointHandle);

#endif
