#ifndef _AIS_LOCK_H_
#define _AIS_LOCK_H_ 

#include "ais_base.h"

/* Chapter 10 */

typedef OPAQUE_TYPE SaLckHandleT;
typedef OPAQUE_TYPE SaLckLockIdT;
typedef OPAQUE_TYPE SaLckResourceIdT;


#define SA_LCK_LOCK_NO_QUEUE 0x1
#define SA_LCK_LOCK_ORPHAN 0x2
#define SA_LCK_LOCK_TIMEOUT 0X4
typedef SaUint32T SaLckLockFlagsT;

typedef enum {
    SA_LCK_LOCK_GRANTED = 1,
    SA_LCK_LOCK_RELEASED = 2,
    SA_LCK_LOCK_DEADLOCK = 3,
    SA_LCK_LOCK_NOT_QUEUED = 4,
    SA_LCK_LOCK_TIMED_OUT = 5,
    SA_LCK_LOCK_ORPHANED = 6,
    SA_LCK_LOCK_NO_MORE = 7
} SaLckLockStatusT;

typedef enum {
    SA_LCK_PR_LOCK_MODE = 1,
    SA_LCK_EX_LOCK_MODE = 2
} SaLckLockModeT;

typedef void 
(*SaLckLockGrantCallbackT)(SaInvocationT invocation,
                           const SaLckResourceIdT *resourceId,
                           const SaLckLockIdT *lockId,
                           SaLckLockModeT lockMode,
                           SaLckLockStatusT lockStatus,
                           SaErrorT error);

typedef void 
(*SaLckLockWaiterCallbackT)(SaInvocationT invocation,
                            const SaLckResourceIdT *resourceId,
                            const SaLckLockIdT *lockId,
                            SaLckLockModeT modeHeld,
                            SaLckLockModeT modeRequested);

typedef void 
(*SaLckResourceUnlockCallbackT)(SaInvocationT invocation,
                                const SaLckResourceIdT *resourceId,
                                const SaLckLockIdT *lockId,
                                SaLckLockStatusT lockStatus,
                                SaErrorT error);
typedef struct{
    SaLckLockGrantCallbackT saLckLockGrantCallback;
    SaLckLockWaiterCallbackT saLckLockWaiterCallback;
    SaLckResourceUnlockCallbackT saLckResourceUnlockCallback;
} SaLckCallbacksT;

    SaErrorT 
saLckInitialize(SaLckHandleT *lckHandle, const SaLckCallbacksT *lckCallbacks,
                const SaVersionT *version);

    SaErrorT 
saLckSelectionObjectGet(const SaLckHandleT *lckHandle,
                        SaSelectionObjectT *selectionObject);

    SaErrorT 
saLckDispatch(const SaLckHandleT *lckHandle,
              const SaDispatchFlagsT dispatchFlags);

    SaErrorT 
saLckFinalize(SaLckHandleT *lckHandle);

    SaErrorT 
saLckResourceOpen(const SaLckHandleT *lckHandle,
                  const SaNameT *lockName,
                  SaLckResourceIdT *resourceId);

    SaErrorT 
saLckResourceClose(SaLckHandleT *lckHandle, SaLckResourceIdT *resourceId);

    SaErrorT 
saLckResourceLock(const SaLckHandleT *lckHandle, SaInvocationT invocation,
                  const SaLckResourceIdT *resourceId,
                  SaLckLockIdT *lockId,
                  SaLckLockModeT lockMode,
                  SaLckLockFlagsT lockFlags,
                  SaTimeT timeout,
                  SaLckLockStatusT *lockStatus);

    SaErrorT
SaLckResourceLockAsync(const SaLckHandleT *lckHandle,
                       SaInvocationT invocation,
                       const SaLckResourceIdT *resourceId,
                       SaLckLockIdT *lockId,
                       SaLckLockModeT lockMode,
                       SaLckLockFlagsT lockFlags,
                       SaTimeT timeout);

    SaErrorT 
saLckResourceUnlock(const SaLckHandleT *lckHandle,
                    SaLckLockIdT *lockId,
                    SaTimeT timeout);

    SaErrorT 
saLckResourceUnlockAsync(const SaLckHandleT *lckHandle,
                         SaInvocationT invocation,
                         const SaLckLockIdT *lockId);

    SaErrorT
saLckLockPurge(const SaLckHandleT *lckHandle,
               const SaLckResourceIdT *resourceId);

#endif /* _AIS_LOCK_H_ */
