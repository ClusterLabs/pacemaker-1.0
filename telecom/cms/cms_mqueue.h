/*
 * cms_mqueue.h: cms daemon message queue functions header
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 * Author: Zhu Yi (yi.zhu@intel.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#ifndef __CMS_MQUEUE_H__
#define __CMS_MQUEUE_H__

#include "saf/ais_message.h"
#include "ha_msg.h"


#define BUFFER_AVAILABLE(mq, x)	(mq->status.saMsgQueueUsage[x].queueSize  \
				- mq->status.saMsgQueueUsage[x].queueUsed)

#define CIRCLE_LIST_NEXT(list, curr) (curr == g_list_last(list) ?	\
				      g_list_first(list) : g_list_next(curr))

#define	G_LIST_EMPTY(list)	((g_list_length(list) == 0) ? TRUE : FALSE)

#define IS_MQGROUP(mq)		((mq->policy != 0) ? TRUE : FALSE)

typedef enum {
	MQ_STATUS_OPEN_PENDING = 1,
	MQ_STATUS_OPEN,
	MQ_STATUS_CLOSE,
	MQ_STATUS_UNLINK,
} mqueue_status_t;

typedef struct {
	char * name;
	char * host;
	mqueue_status_t mqstat;
	SaMsgQueueStatusT status;
 	int policy;	/* message queue policy or 0 for message queue */
 	GList * list;	/* For message queue group, this is the list of
 			 * the message queues in its group;
 			 * for message queue, this is the list of the
 			 * message queue groups it belongs to. */

	/* stuff that is only used by local queue */
	IPC_Channel * client; /* the client who opened this mq */
	SaMsgQueueHandleT handle;
	GList * message_buffer[SA_MSG_MESSAGE_LOWEST_PRIORITY + 1];
	gboolean notified; /* notify status flag for async message delivery */

	GList * current;     /* Current RR counter ptr, only for mq group. */
 	GList * notify_list; /* Tracking membership, only for mq group. */
} mqueue_t;

typedef struct {
	char * qname;
	char * gname;
	int request_type;
	SaInvocationT invocation;
	gboolean policy;
	SaMsgQueueCreationFlagsT create_flag;
	SaMsgQueueOpenFlagsT open_flag;
	SaTimeT retention;
	SaSizeT size[SA_MSG_MESSAGE_LOWEST_PRIORITY + 1];
	SaMsgAckFlagsT ack;
	unsigned long seq;
} mqueue_request_t;

typedef struct {
	SaMsgQueueGroupNotificationT changeonly_buff;
	SaMsgQueueGroupNotificationT * change_buff;
	SaMsgQueueGroupPolicyT policy;
	SaUint32T number;
	SaNameT name;
} notify_buffer_t;


struct mq_info {
	SaNameT 	qname;
	SaNameT 	host;
	mqueue_status_t mqstat;
	SaMsgQueueGroupPolicyT 	policy;
	int		mq_groupinfo_count;
};

struct mq_groupinfo {
	SaNameT		name;
};

int mqueue_table_init(void);
mqueue_t *mqname_lookup(const char *name, int *group);
int mqueue_table_insert(mqueue_t *mq);
guint mqueue_handle_insert(mqueue_t *mq);
int mqueue_table_remove(const char * qname);
int mqueue_handle_remove(guint *hd);
mqueue_t * mqueue_table_lookup(const char * qname, int * group);
mqueue_t * mqueue_handle_lookup(guint *handle, int * group);
int mqueue_table_pack(struct mq_info * * buf, size_t * buf_len);
int mqueue_table_unpack(const struct mq_info * info, size_t info_len);
void mqueue_close_node(char * node);
void enqueue_message(mqueue_t * mq, SaUint8T prio, SaMsgMessageT * msg);
SaMsgMessageT * dequeue_message(mqueue_t * mq);
int sa_mqueue_usage_encode(char *size, char *used, char * number,
SaMsgQueueUsageT * usage);
int sa_mqueue_usage_decode(const char *size, const char *used, const char *
number, SaMsgQueueUsageT * usage);
void mqueue_copy_notify_data(gpointer data, gpointer user_data);
void mqueue_update_usage(mqueue_t * mq, int priority, SaSizeT size);
int mqgroup_unref_mqueue(mqueue_t * mqg, mqueue_t * mq);
void dump_mqueue_list(mqueue_t * mq);

#endif	/*__CMS_MQUEUE_H__ */
