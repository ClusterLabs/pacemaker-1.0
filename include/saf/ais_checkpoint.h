#ifndef _AIS_CHECKPOINT_H_
#define _AIS_CHECKPOINT_H_

#include "ais_base.h"

/* Chapter 7 */
#ifdef __CPLUSPLUS
extern "C"{
#endif

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
    SaUint8T *id;
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
    SaSizeT readSize; /*[out] */
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
#ifdef __CPLUSPLUS
}
#endif

#endif /* _AIS_CHECKPOINT_H_ */
