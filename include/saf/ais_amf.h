#ifndef _AIS_AMF_H_
#define _AIS_AMF_H_

#include "ais_base.h"

/* Chapter 5 */
typedef OPAQUE_TYPE SaAmfHandleT;

typedef enum {
    SA_AMF_HEARTBEAT = 1,
    SA_AMF_HEALTHCHECK_LEVEL1 = 2,
    SA_AMF_HEALTHCHECK_LEVEL2 = 3,
    SA_AMF_HEALTHCHECK_LEVEL3 = 4
} SaAmfHealthcheckT;

typedef enum {
    SA_AMF_OUT_OF_SERVICE = 1,
    SA_AMF_IN_SERVICE = 2,
    SA_AMF_STOPPING = 3
} SaAmfReadinessStateT;

typedef enum {
    SA_AMF_ACTIVE = 1,
    SA_AMF_STANDBY = 2,
    SA_AMF_QUIESCED = 3
} SaAmfHAStateT;

typedef enum {
    SA_AMF_COMPONENT_CAPABILITY_X_ACTIVE_AND_Y_STANDBY= 1,
    SA_AMF_COMPONENT_CAPABILITY_X_ACTIVE_OR_X_STANDBY = 2,
    SA_AMF_COMPONENT_CAPABILITY_1_ACTIVE_OR_Y_STANDBY = 3,
    SA_AMF_COMPONENT_CAPABILITY_1_ACTIVE_OR_1_STANDBY = 4,
    SA_AMF_COMPONENT_CAPABILITY_X_ACTIVE = 5,
    SA_AMF_COMPONENT_CAPABILITY_1_ACTIVE = 6,
    SA_AMF_COMPONENT_CAPABILITY_NO_STATE = 7
} SaAmfComponentCapabilityModelT;

#define SA_AMF_CSI_ADD_NEW_INSTANCE 0X1
#define SA_AMF_CSI_ALL_INSTANCES 0X2

typedef SaUint32T SaAmfCSIFlagsT;

typedef enum {
    SA_AMF_CSI_NEW_ASSIGN = 1,
    SA_AMF_CSI_QUIESCED = 2,
    SA_AMF_CSI_NOT_QUIESCED = 3,
    SA_AMF_CSI_STILL_ACTIVE = 4
} SaAmfCSITransitionDescriptorT;

typedef enum {
    SA_AMF_RESET = 1,
    SA_AMF_REBOOT = 2,
    SA_AMF_POWER_ON = 3,
    SA_AMF_POWER_OFF = 4
} SaAmfExternalComponentActionT;

#define SA_AMF_SWITCHOVER_OPERATION 0X1
#define SA_AMF_SHUTDOWN_OPERATION 0X2
typedef SaUint32T SaAmfPendingOperationFlagsT;

typedef struct {
    SaNameT compName;
    SaAmfReadinessStateT readinessState;
    SaAmfHAStateT haState;
} SaAmfProtectionGroupMemberT;

typedef enum {
    SA_AMF_PROTECTION_GROUP_NO_CHANGE = 1,
    SA_AMF_PROTECTION_GROUP_ADDED = 2,
    SA_AMF_PROTECTION_GROUP_REMOVED = 3,
    SA_AMF_PROTECTION_GROUP_STATE_CHANGE = 4
} SaAmfProtectionGroupChangesT;

typedef struct {
    SaAmfProtectionGroupMemberT member;
    SaAmfProtectionGroupChangesT change;
} SaAmfProtectionGroupNotificationT;

typedef enum {
    SA_AMF_COMMUNICATION_ALARM_TYPE = 1,
    SA_AMF_QUALITY_OF_SERVICE_ALARM_TYPE = 2,
    SA_AMF_PROCESSING_ERROR_ALARM_TYPE = 3,
    SA_AMF_EQUIPMENT_ALARM_TYPE = 4,
    SA_AMF_ENVIRONMENTAL_ALARM_TYPE = 5
} SaAmfErrorReportTypeT;

typedef enum {
    SA_AMF_APPLICATION_SUBSYSTEM_FAILURE = 1,
    SA_AMF_BANDWIDTH_REDUCED = 2,
    SA_AMF_CALL_ESTABLISHMENT_ERROR = 3,
    SA_AMF_COMMUNICATION_PROTOCOL_ERROR = 4,
    SA_AMF_COMMUNICATION_SUBSYSTEM_FAILURE = 5,
    SA_AMF_CONFIGURATION_ERROR = 6,
    SA_AMF_CONGESTION = 7,
    SA_AMF_CORRUPT_DATA = 8,
    SA_AMF_CPU_CYCLES_LIMIT_EXCEEDED = 9,
    SA_AMF_EQUIPMENT_MALFUNCTION = 10,
    SA_AMF_FILE_ERROR = 11,
    SA_AMF_IO_DEVICE_ERROR = 12,
    SA_AMF_LAN_ERROR, SA_AMF_OUT_OF_MEMORY = 13,
    SA_AMF_PERFORMANCE_DEGRADED = 14,
    SA_AMF_PROCESSOR_PROBLEM = 15,
    SA_AMF_RECEIVE_FAILURE = 16,
    SA_AMF_REMOTE_NODE_TRANSMISSION_ERROR = 17,
    SA_AMF_RESOURCE_AT_OR_NEARING_CAPACITY = 18,
    SA_AMF_RESPONSE_TIME_EXCESSIVE = 19,
    SA_AMF_RETRANSMISSION_RATE_EXCESSIVE = 20,
    SA_AMF_SOFTWARE_ERROR = 21,
    SA_AMF_SOFTWARE_PROGRAM_ABNORMALLY_TERMINATED = 22,
    SA_AMF_SOFTWARE_PROGRAM_ERROR = 23,
    SA_AMF_STORAGE_CAPACITY_PROBLEM = 24,
    SA_AMF_TIMING_PROBLEM = 25,
    SA_AMF_UNDERLYING_RESOURCE_UNAVAILABLE = 26,
    SA_AMF_INTERNAL_ERROR = 27,
    SA_AMF_NO_SERVICE_ERROR = 28,
    SA_AMF_SOFTWARE_LIBRARY_ERROR = 29
} SaAmfProbableCauseT;

typedef enum {
    SA_AMF_CLEARED = 1,
    SA_AMF_NO_IMPACT = 2,
    SA_AMF_INDETERMINATE = 3,
    SA_AMF_CRITICAL = 4,
    SA_AMF_MAJOR = 5,
    SA_AMF_WEDGED_COMPONENT_FAILURE = 6,
    SA_AMF_COMPONENT_TERMINATED_FAILURE= 7,
    SA_AMF_NODE_FAILURE = 8,
    SA_AMF_MINOR = 9,
    SA_AMF_WARNING = 10
} SaAmfErrorImpactAndSeverityT;

typedef enum {
    SA_AMF_NO_RECOMMENDATION = 1,
    SA_AMF_INTERNALLY_RECOVERED = 2,
    SA_AMF_COMPONENT_RESTART = 3,
    SA_AMF_COMPONENT_FAILOVER = 4,
    SA_AMF_NODE_SWITCHOVER = 5,
    SA_AMF_NODE_FAILOVER = 6,
    SA_AMF_NODE_FAILFAST = 7,
    SA_AMF_CLUSTER_RESET = 8
} SaAmfRecommendedRecoveryT;

#define SA_AMF_OPAQUE_BUFFER_SIZE_MAX 256

typedef struct {
    char *buffer;
    SaSizeT size;
} SaAmfErrorBufferT;

typedef struct {
    SaAmfErrorBufferT *specificProblem;
    SaAmfErrorBufferT *additionalText;
    SaAmfErrorBufferT *additionalInformation;
} SaAmfAdditionalDataT;

typedef struct {
    SaAmfErrorReportTypeT errorReportType;
    SaAmfProbableCauseT probableCause;
    SaAmfErrorImpactAndSeverityT errorImpactAndSeverity;
    SaAmfRecommendedRecoveryT recommendedRecovery;
} SaAmfErrorDescriptorT;


typedef void 
(*SaAmfHealthcheckCallbackT)(SaInvocationT invocation,
                             const SaNameT *compName,
                             SaAmfHealthcheckT checkType);

typedef void 
(*SaAmfReadinessStateSetCallbackT)(SaInvocationT invocation,
                                   const SaNameT *compName,
                                   SaAmfReadinessStateT readinessState);

typedef void 
(*SaAmfComponentTerminateCallbackT)(SaInvocationT invocation,
                                    const SaNameT *compName);

typedef void 
(*SaAmfCSISetCallbackT)(SaInvocationT invocation, 
                        const SaNameT *compName,
                        const SaNameT *csiName,
                        SaAmfCSIFlagsT csiFlags,
                        SaAmfHAStateT *haState,
                        SaNameT *activeCompName,
                        SaAmfCSITransitionDescriptorT transitionDescriptor);

typedef void 
(*SaAmfCSIRemoveCallbackT)(SaInvocationT invocation,
                           const SaNameT *compName,
                           const SaNameT *csiName,
                           const SaAmfCSIFlagsT *csiFlags);

typedef void 
(*SaAmfProtectionGroupTrackCallbackT)(const SaNameT *csiName,
                                      SaAmfProtectionGroupNotificationT 
                                          *notificationBuffer,
                                      SaUint32T numberOfItems,
                                      SaUint32T numberOfMembers,
                                      SaErrorT error);
typedef void 
(*SaAmfExternalComponentRestartCallbackT)(SaInvocationT invocation,
                                          const SaNameT *externalCompName); typedef void 
(*SaAmfExternalComponentControlCallbackT)(const SaInvocationT invocation,
                                          const SaNameT *externalCompName,
                                          SaAmfExternalComponentActionT 
                                              controlAction);

typedef void 
(*SaAmfPendingOperationConfirmCallbackT)(const SaInvocationT invocation,
                                         const SaNameT *compName,
                                         SaAmfPendingOperationFlagsT 
                                             pendingOperationFlags);


typedef struct {
    SaAmfHealthcheckCallbackT
        saAmfHealthcheckCallback;
    SaAmfReadinessStateSetCallbackT	   
        saAmfReadinessStateSetCallback;
    SaAmfComponentTerminateCallbackT	   
        saAmfComponentTerminateCallback;
    SaAmfCSISetCallbackT		   
        saAmfCSISetCallback;
    SaAmfCSIRemoveCallbackT		   
        saAmfCSIRemoveCallback;
    SaAmfProtectionGroupTrackCallbackT	   
        saAmfProtectionGroupTrackCallback;
    SaAmfExternalComponentRestartCallbackT 
        saAmfExternalComponentRestartCallback;
    SaAmfExternalComponentControlCallbackT 
        saAmfExternalComponentControlCallback;
    SaAmfPendingOperationConfirmCallbackT  
        saAmfPendingOperationConfirmCallback;
} SaAmfCallbacksT;

    SaErrorT 
saAmfInitialize(SaAmfHandleT *amfHandle, const SaAmfCallbacksT *amfCallbacks,
                const SaVersionT *version);
    SaErrorT 
saAmfSelectionObjectGet(const SaAmfHandleT *amfHandle, 
                        SaSelectionObjectT *selectionObject);
    SaErrorT 
saAmfDispatch(const SaAmfHandleT *amfHandle, SaDispatchFlagsT dispatchFlags);
    SaErrorT 
saAmfFinalize(const SaAmfHandleT *amfHandle);
    SaErrorT 
saAmfComponentRegister( const SaAmfHandleT *amfHandle,
                        const SaNameT *compName, const SaNameT *proxyCompName);
    SaErrorT 
saAmfComponentUnregister(const SaAmfHandleT *amfHandle,
                         const SaNameT *compName, 
                         const SaNameT *proxyCompName);
    SaErrorT 
saAmfCompNameGet(const SaAmfHandleT *amfHandle, SaNameT *compName);

    SaErrorT
saAmfReadinessStateGet(const SaNameT *compName, 
                       SaAmfReadinessStateT *readinessState);

    SaErrorT 
saAmfStoppingComplete(SaInvocationT invocation, SaErrorT error);

    SaErrorT 
saAmfHAStateGet(const SaNameT *compName, const SaNameT *csiName, 
                SaAmfHAStateT *haState);

    SaErrorT 
saAmfProtectionGroupTrackStart(const SaAmfHandleT *amfHandle,
                               const SaNameT *csiName, 
                               SaUint8T trackFlags,
                               const SaAmfProtectionGroupNotificationT 
                                         *notificationBufffer,
                               SaUint32T numberOfItems);

    SaErrorT 
saAmfProtectionGroupTrackStop(const SaAmfHandleT *amfHandle, 
                              const SaNameT *csiName);

    SaErrorT 
saAmfErrorReport(const SaNameT *reportingComponent, 
                 const SaNameT *erroneousComponent,
                 SaTimeT errorDetectionTime, 
                 const SaAmfErrorDescriptorT *errorDescriptor,
                 const SaAmfAdditionalDataT *additionalData);

    SaErrorT 
saAmfErrorCancelAll(const SaNameT *compName);

    SaErrorT 
saAmfComponentCapabilityModelGet(const SaNameT *compName,
                                 SaAmfComponentCapabilityModelT 
                                     *componentCapabilityModel);
    SaErrorT 
saAmfPendingOperationGet(const SaNameT *compName,
                         SaAmfPendingOperationFlagsT *pendingOperationFlags);

    SaErrorT 
saAmfResponse(SaInvocationT invocation, SaErrorT error);

#endif /* _AIS_AMF_H_ */
