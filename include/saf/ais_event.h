#ifndef _AIS_EVENT_H_
#define _AIS_EVENT_H_ 

#include "ais_base.h"


/* Chapter 8 */

typedef OPAQUE_TYPE SaEvtHandleT;
typedef OPAQUE_TYPE SaEvtEventHandleT;
typedef OPAQUE_TYPE SaEvtChannelHandleT;
typedef SaUint32T SaEvtSubscriptionIdT;


typedef void
(*SaEvtEventDeliverCallbackT)(const SaEvtChannelHandleT *channelHandle,
                              SaEvtSubscriptionIdT subscriptionId,
                              const SaEvtEventHandleT *eventHandle,
                              const SaSizeT eventDataSize);

typedef struct{
    const SaEvtEventDeliverCallbackT saEvtEventDeliverCallback; } SaEvtCallbacksT;

#define SA_EVT_CHANNEL_PUBLISHER  0X1
#define SA_EVT_CHANNEL_SUBSCRIBER 0X2
#define SA_EVT_CHANNEL_CREATE     0X4
typedef SaUint8T SaEvtChannelOpenFlagsT;

typedef struct {
    SaUint8T *pattern;
    SaSizeT patternSize;
} SaEvtEventPatternT;


#define SA_EVT_HIGHEST_PRIORITY 0
#define SA_EVT_LOWEST_PRIORITY  3

#define SA_EVT_LOST_EVENT "SA_EVT_LOST_EVENT_PATTERN"

typedef struct {
    SaEvtEventPatternT *patterns;
    SaSizeT patternsNumber;
} SaEvtEventPatternArrayT;

typedef SaUint8T SaEvtEventPriorityT;
typedef SaUint32T SaEvtEventIdT;

typedef enum {
    SA_EVT_PREFIX_FILTER = 1,
    SA_EVT_SUFFIX_FILTER = 2,
    SA_EVT_EXACT_FILTER = 3,
    SA_EVT_PASS_ALL_FILTER = 4
} SaEvtEventFilterTypeT;

typedef struct {
    SaEvtEventFilterTypeT filterType;
    SaEvtEventPatternT filter;
} SaEvtEventFilterT;

typedef struct {
    SaEvtEventFilterT *filters;
    SaSizeT filtersNumber;
} SaEvtEventFilterArrayT;

    SaErrorT 
saEvtInitialize(SaEvtHandleT *evtHandle, const SaEvtCallbacksT *callbacks,
                const SaVersionT *version);

    SaErrorT 
saEvtSelectionObjectGet(const SaEvtHandleT *evtHandle,
                        SaSelectionObjectT *selectionObject);

    SaErrorT 
saEvtDispatch(const SaEvtHandleT *evtHandle, SaDispatchFlagsT dispatchFlags);

    SaErrorT 
saEvtFinalize(SaEvtHandleT *evtHandle);

    SaErrorT 
saEvtChannelOpen(const SaEvtHandleT *evtHandle, const SaNameT *channelName,
                 SaEvtChannelOpenFlagsT channelOpenFlags,
                 SaEvtChannelHandleT *channelHandle);

    SaErrorT 
saEvtChannelClose(SaEvtChannelHandleT *channelHandle);

    SaErrorT 
saEvtEventAllocate(const SaEvtChannelHandleT *channelHandle,
                   SaEvtEventHandleT *eventHandle);

    SaErrorT 
saEvtEventFree(SaEvtEventHandleT *eventHandle);

    SaErrorT 
saEvtEventAttributesSet(const SaEvtEventHandleT *eventHandle,
                        const SaEvtEventPatternArrayT *patternArray,
                        SaUint8T priority,
                        SaTimeT retentionTime,
                        const SaNameT *publisherName);

    SaErrorT 
saEvtEventAttributesGet(const SaEvtChannelHandleT *channelHandle,
                        const SaEvtEventHandleT *eventHandle,
                        SaEvtEventPatternArrayT *patternArray,
                        SaUint8T *priority,
                        SaTimeT *retentionTime,
                        SaNameT *publisherName,
                        SaClmNodeIdT *publisherNodeId,
                        SaTimeT *publishTime,
                        SaEvtEventIdT *eventId);

    SaErrorT 
saEvtEventDataGet(const SaEvtEventHandleT *eventHandle,
                  void *eventData,
                  SaSizeT *eventDataSize);

    SaErrorT 
saEvtEventPublish(const SaEvtChannelHandleT *channelHandle,
                  const SaEvtEventHandleT *eventHandle,
                  const void *eventData,
                  SaSizeT eventDataSize);

    SaErrorT 
saEvtEventSubscribe(const SaEvtChannelHandleT *channelHandle,
                    const SaEvtEventFilterArrayT *filters,
                    SaEvtSubscriptionIdT subscriptionId);

    SaErrorT 
saEvtEventUnsubscribe(const SaEvtChannelHandleT *channelHandle,
                      SaEvtSubscriptionIdT subscriptionId);

    SaErrorT 
saEvtEventRetentionTimeClear(const SaEvtChannelHandleT *channelHandle,
                             const SaEvtEventHandleT *eventHandle);

#endif /* _AIS_EVENT_H_ */
