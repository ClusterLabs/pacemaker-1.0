#ifndef _AIS_MESSAGE_H_
#define _AIS_MESSAGE_H_

#include "ais_base.h"

/* Chapter 9 */
#ifdef __CPLUSPLUS
extern "C" {
#endif

typedef OPAQUE_TYPE SaMsgHandleT;
typedef OPAQUE_TYPE SaMsgMessageHandleT;
typedef OPAQUE_TYPE SaMsgQueueHandleT;
typedef OPAQUE_TYPE SaMsgSenderIdT;


#define SA_MSG_MESSAGE_DELIVERED_ACK 0x1
typedef SaUint32T SaMsgAckFlagsT;

#define SA_MSG_QUEUE_PERSISTENT 0x1
#define SA_MSG_QUEUE_MIGRATABLE 0x2
typedef SaUint32T SaMsgQueueCreationFlagsT;

#define SA_MSG_MESSAGE_HIGHEST_PRIORITY 0
#define SA_MSG_MESSAGE_LOWEST_PRIORITY 3

typedef struct {
    SaMsgQueueCreationFlagsT creationFlags;
    SaSizeT size[SA_MSG_MESSAGE_LOWEST_PRIORITY + 1];
    SaTimeT retentionTime;
} SaMsgQueueCreationAttributesT;

#define SA_MSG_QUEUE_CREATE 0x1
#define SA_MSG_QUEUE_RECEIVE_CALLBACK 0x2
#define SA_MSG_QUEUE_SELECTION_OBJECT_SET 0x4
#define SA_MSG_QUEUE_EMPTY 0x8
typedef SaUint32T SaMsgQueueOpenFlagsT;

typedef enum {
    SA_MSG_QUEUE_UNAVAILABLE = 1,
    SA_MSG_QUEUE_AVAILABLE = 2
} SaMsgQueueSendingStateT;

typedef struct {
    SaUint32T queueSize;
    SaSizeT queueUsed;
    SaUint32T numberOfMessages;
} SaMsgQueueUsageT;

typedef struct {
    SaMsgQueueSendingStateT sendingState;
    SaMsgQueueCreationFlagsT creationFlags;
    SaMsgQueueOpenFlagsT openFlags;
    SaTimeT retentionTime;
    SaTimeT closeTime;
    SaSizeT headerLength;
    SaMsgQueueUsageT saMsgQueueUsage[SA_MSG_MESSAGE_LOWEST_PRIORITY + 1];
} SaMsgQueueStatusT;

typedef enum {
    SA_MSG_QUEUE_GROUP_ROUND_ROBIN = 1
} SaMsgQueueGroupPolicyT;

typedef enum {
    SA_MSG_QUEUE_GROUP_NO_CHANGE = 1,
    SA_MSG_QUEUE_GROUP_ADDED = 2,
    SA_MSG_QUEUE_GROUP_REMOVED = 3,
    SA_MSG_QUEUE_GROUP_STATE_CHANGED = 4
} SaMsgQueueGroupChangesT;

typedef struct {
    SaNameT queueName;
    SaMsgQueueStatusT queueStatus;
} SaMsgQueueGroupMemberT;

typedef struct {
    SaMsgQueueGroupChangesT change;
    SaMsgQueueGroupMemberT member;
} SaMsgQueueGroupNotificationT;

typedef struct {
    SaUint32T numberOfItems;
    SaMsgQueueGroupNotificationT *notification;
} SaMsgQueueGroupNotificationBufferT;

typedef struct {
    SaSizeT type;
    SaSizeT version;
    SaSizeT size;
    void *data;
    SaUint8T priority;
} SaMsgMessageT;

typedef struct {
    SaTimeT sendTime;
    SaNameT senderName;
    SaBoolT sendReceive;
    SaMsgSenderIdT senderId;
} SaMsgMessageInfoT;

typedef void
(*SaMsgQueueOpenCallbackT)(SaInvocationT invocation,
                           const SaMsgQueueHandleT *queueHandle,
                           SaErrorT error);
typedef void 
(*SaMsgQueueGroupTrackCallbackT)(const SaNameT *queueGroupName,
                                 const SaMsgQueueGroupNotificationBufferT 
                                     *notificationBuffer,
                                 SaMsgQueueGroupPolicyT queueGroupPolicy,
                                 SaUint32T numberOfMembers,
                                 SaErrorT error);

typedef void 
(*SaMsgMessageDeliveredCallbackT)(SaInvocationT invocation,
                                  SaErrorT error);

typedef void 
(*SaMsgMessageReceivedCallbackT)(const SaMsgQueueHandleT *queueHandle);

typedef struct {
    const SaMsgQueueOpenCallbackT saMsgQueueOpenCallback;
    const SaMsgQueueGroupTrackCallbackT saMsgQueueGroupTrackCallback;
    const SaMsgMessageDeliveredCallbackT saMsgMessageDeliveredCallback;
    const SaMsgMessageReceivedCallbackT saMsgMessageReceivedCallback; } SaMsgCallbacksT;

    SaErrorT 
saMsgInitialize(SaMsgHandleT *msgHandle, const SaMsgCallbacksT *msgCallbacks,
                const SaVersionT *version);

    SaErrorT 
saMsgSelectionObjectGet(const SaMsgHandleT *msgHandle,
                        SaSelectionObjectT *selectionObject);

    SaErrorT 
saMsgDispatch(const SaMsgHandleT *msgHandle, SaDispatchFlagsT dispatchFlags);

    SaErrorT 
saMsgFinalize(SaMsgHandleT *msgHandle);

    SaErrorT 
saMsgQueueOpen(const SaMsgHandleT *msgHandle,
               const SaNameT *queueName,
               const SaMsgQueueCreationAttributesT *creationAttributes,
               SaMsgQueueOpenFlagsT openFlags,
               SaTimeT timeout,
               SaMsgQueueHandleT *queueHandle);

    SaErrorT 
saMsgQueueOpenAsync(const SaMsgHandleT *msgHandle,
                    SaInvocationT invocation,
                    const SaNameT *queueName,
                    const SaMsgQueueCreationAttributesT *creationAttributes,
                    SaMsgQueueOpenFlagsT openFlags);

    SaErrorT 
saMsgQueueClose(SaMsgQueueHandleT *queueHandle);

    SaErrorT 
saMsgQueueStatusGet(SaMsgHandleT *msgHandle,
                    const SaNameT *queueName,
                    SaMsgQueueStatusT *queueStatus);

    SaErrorT 
saMsgQueueUnlink(SaMsgHandleT *msgHandle, const SaNameT *queueName);

    SaErrorT
saMsgQueueGroupCreate(SaMsgHandleT *msgHandle,
                      const SaNameT *queueGroupName,
                      SaMsgQueueGroupPolicyT queueGroupPolicy);

    SaErrorT 
saMsgQueueGroupDelete(SaMsgHandleT *msgHandle, const SaNameT *queueGroupName);

    SaErrorT 
saMsgQueueGroupInsert(SaMsgHandleT *msgHandle,
                      const SaNameT *queueGroupName,
                      const SaNameT *queueName);

    SaErrorT 
saMsgQueueGroupRemove(SaMsgHandleT *msgHandle,
                      const SaNameT *queueGroupName,
                      const SaNameT *queueName);

    SaErrorT 
saMsgQueueGroupTrack(const SaMsgHandleT *msgHandle,
                     const SaNameT *queueGroupName,
                     SaUint8T trackFlags,
                     SaMsgQueueGroupNotificationBufferT *notificationBuffer);

    SaErrorT 
saMsgQueueGroupTrackStop(const SaMsgHandleT *msgHandle,
                         const SaNameT *queueGroupName);

    SaErrorT 
saMsgMessageSend(const SaMsgHandleT *msgHandle,
                 const SaNameT *destination,
                 const SaMsgMessageT *message,
                 SaMsgAckFlagsT ackFlags,
                 SaTimeT timeout);

    SaErrorT 
saMsgMessageSendAsync(const SaMsgHandleT *msgHandle,
                      SaInvocationT invocation,
                      const SaNameT *destination,
                      const SaMsgMessageT *message,
                      SaMsgAckFlagsT ackFlags);

    SaErrorT
saMsgMessageGet(const SaMsgQueueHandleT *queueHandle,
                SaMsgMessageT *message,
                SaMsgMessageInfoT *messageInfo,
                SaTimeT timeout);

    SaErrorT 
saMsgMessageReceivedGet(const SaMsgQueueHandleT *queueHandle,
                        const SaMsgMessageHandleT *messageHandle,
                        SaMsgMessageT *message,
                        SaMsgMessageInfoT *messageInfo);

    SaErrorT 
saMsgMessageCancel(const SaMsgQueueHandleT *queueHandle);

    SaErrorT 
saMsgMessageSendReceive(SaMsgHandleT msgHandle,
			const SaNameT *destination,
                        const SaMsgMessageT *sendMessage,
                        SaMsgMessageT *receiveMessage,
			SaTimeT *replySendTime,
                        SaTimeT timeout);

    SaErrorT 
saMsgMessageReply(SaMsgHandleT msgHandle,
		  const SaMsgMessageT *replyMessage,
                  const SaMsgSenderIdT *senderId,
                  SaTimeT timeout);

    SaErrorT 
saMsgMessageReplyAsync(SaMsgHandleT msgHandle,
                       SaInvocationT invocation,
                       const SaMsgMessageT *replyMessage,
		       const SaMsgSenderIdT *senderId,
                       SaMsgAckFlagsT ackFlags);
#ifdef __CPLUSPLUS
}
#endif
#endif /* _AIS_MESSAGE_H_ */
