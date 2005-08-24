#ifndef _AIS_MEMBERSHIP_H_
#define _AIS_MEMBERSHIP_H_

#include "ais_base.h"

/* Chapter 6 */
#ifdef __CPLUSPLUS
extern "C"{
#endif

typedef SaUint32T SaClmHandleT;
typedef SaUint32T SaClmNodeIdT;

#define SA_CLM_MAX_ADDRESS_LENGTH 64
typedef struct {
    SaUint8T length;
    char value[SA_CLM_MAX_ADDRESS_LENGTH];
} SaClmNodeAddressT;

typedef struct {
    SaClmNodeIdT nodeId;
    SaClmNodeAddressT nodeAddress;
    SaNameT nodeName;
    SaNameT clusterName;
    SaBoolT member;
    SaTimeT bootTimestamp;
} SaClmClusterNodeT;

typedef enum {
    SA_CLM_NODE_NO_CHANGE = 1,
    SA_CLM_NODE_JOINED = 2,
    SA_CLM_NODE_LEFT = 3
} SaClmClusterChangesT;

typedef struct {
    SaClmClusterNodeT clusterNode;
    SaClmClusterChangesT clusterChanges;
} SaClmClusterNotificationT;

typedef void 
(*SaClmClusterTrackCallbackT) (SaClmClusterNotificationT *notificationBuffer,
                               SaUint32T numberOfItems,
                               SaUint32T numberOfMembers,
                               SaUint64T viewNumber,
                               SaErrorT error);
typedef void 
(*SaClmClusterNodeGetCallbackT)(SaInvocationT invocation,
                                SaClmClusterNodeT *clusterNode,
                                SaErrorT error);

typedef struct {
    SaClmClusterNodeGetCallbackT saClmClusterNodeGetCallback;
    SaClmClusterTrackCallbackT   saClmClusterTrackCallback;
} SaClmCallbacksT;

    SaErrorT 
saClmInitialize(SaClmHandleT *clmHandle, const SaClmCallbacksT *clmCallbacks,
                const SaVersionT *version);

    SaErrorT 
saClmSelectionObjectGet(const SaClmHandleT *clmHandle, 
                        SaSelectionObjectT *selectionObject);

    SaErrorT
saClmDispatch(const SaClmHandleT *clmHandle, 
              SaDispatchFlagsT dispatchFlags);

    SaErrorT 
saClmFinalize(SaClmHandleT *clmHandle);

    SaErrorT 
saClmClusterTrackStart(const SaClmHandleT *clmHandle,
                       SaUint8T trackFlags,
                       SaClmClusterNotificationT *notificationBuffer,
                       SaUint32T numberOfItems);
    SaErrorT 
saClmClusterTrackStop(const SaClmHandleT *clmHandle);

    SaErrorT 
saClmClusterNodeGet(SaClmNodeIdT nodeId, SaTimeT timeout,
                    SaClmClusterNodeT *clusterNode);

    SaErrorT
saClmClusterNodeGetAsync(const SaClmHandleT *clmHandle,
                         SaInvocationT invocation,
                         SaClmNodeIdT nodeId,
                         SaClmClusterNodeT *clusterNode);
#ifdef __CPLUSPLUS
}
#endif

#endif /* _AIS_MEMBERSHIP_H_ */
