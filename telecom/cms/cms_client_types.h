/*
 * cmsclient_types.h: cms daemon and client library types header
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
#ifndef __CMSCLIENT_TYPES_H__
#define __CMSCLIENT_TYPES_H__


/*
 * Note: modify together with cmsrequest_type_str
 * at the same time.
 */
#define	CMS_TYPE_TOTAL			25

enum cms_client_msg_type {
	CMS_QUEUE_STATUS = 1,
	CMS_QUEUE_OPEN = 2,
	CMS_QUEUE_OPEN_ASYNC = 3,
	CMS_QUEUE_CLOSE	= 4,
	CMS_QUEUE_UNLINK = 5,
	CMS_MSG_SEND = 6, 
	CMS_MSG_SEND_ASYNC = 7,
	CMS_MSG_ACK = 8,
	CMS_MSG_RECEIVED_GET = 10, 
	CMS_QUEUEGROUP_CREATE = 11,
	CMS_QUEUEGROUP_DELETE = 12,
	CMS_QUEUEGROUP_INSERT = 13,
	CMS_QUEUEGROUP_REMOVE = 14,
	CMS_QUEUEGROUP_TRACK_START = 15,
	CMS_QUEUEGROUP_TRACK_STOP = 16,
	CMS_QUEUEGROUP_NOTIFY = 17,
	CMS_MSG_REQUEST	= 19,
	CMS_MSG_SEND_RECEIVE = 20,
	CMS_MSG_RECEIVE = 21,
	CMS_MSG_REPLY = 22,
	CMS_MSG_REPLY_ASYNC = 23,

	/* below are special msg types that can be bitwised 
	   together */

	CMS_MSG_GET = 1 << 5, /* 32 */
	CMS_MSG_NOTIFY = 1 << 6, /* 64 */
};

#define CMS_MSG_GET  (1 << 5) /* 32 */
#define	CMS_MSG_NOTIFY  (1 << 6) /* 64 */

#define CMS_DOMAIN_SOCKET	"/var/lib/heartbeat/cms/cms"

typedef struct {

	GList * opened_mqueue_list;
			/* mqueue or mqgroup list opened by this client */
	int channel_count;
	 		/* channel count for this farside_pid */
} cms_client_t ;


typedef struct {
	size_t	 	type;
	size_t		len;
	SaErrorT	flag; /* used for msg reply */
	SaNameT		name; /* mqueue or mqgroup name for this request */

} client_header_t;


typedef struct {
	client_header_t		header;
	SaMsgQueueStatusT	qstatus;

} client_mqueue_status_t;


typedef struct {
	client_header_t 		header;
	SaMsgQueueCreationAttributesT	attr;
	SaMsgQueueOpenFlagsT		openflag;
	SaMsgQueueHandleT		handle;
	SaTimeT				timeout;
	SaInvocationT			invocation;/* only used by open_async */
	SaMsgQueueGroupPolicyT		policy;

} client_mqueue_open_t;


typedef struct {
	client_header_t		header;
	SaMsgQueueHandleT	handle;
	gboolean		silent;

} client_mqueue_close_t;

typedef struct {
	client_header_t		header;
	SaMsgQueueHandleT	handle;
} client_mqueue_handle_t;


typedef client_mqueue_handle_t client_mqueue_notify_t;
typedef client_mqueue_handle_t client_mqueue_unlink_t;


typedef struct {
	client_header_t 	header;
	SaNameT			qgname;

} client_mqgroup_ops_t;


typedef struct {
	client_header_t 	header;
	SaNameT			group_name;
	SaUint8T		flag;

} client_mqgroup_mem_t;


typedef struct {
	client_header_t 	header;
	SaNameT			group_name;
	SaMsgQueueGroupPolicyT	policy;
	SaUint32T		number;
	void *			data;

} client_mqgroup_notify_t;


typedef struct {
	client_header_t		header;
	SaMsgQueueHandleT       handle;
	SaMsgMessageT		msg;
	SaInvocationT		invocation; /* only used by send_async */
	SaMsgAckFlagsT		ack;
	int			sendreceive; /* only used by send_receive */
	SaMsgSenderIdT		senderId; /* only used by reply or reply_async */

	void * 			data;

} client_message_t;


typedef struct {
	client_header_t		header;
	SaMsgQueueHandleT	handle;
	SaInvocationT		invocation; /* only used by send_async */
	size_t			send_type;

} client_message_ack_t;


typedef struct {
	IPC_Channel *ch;
	SaUint8T flag;

} client_mqgroup_track_t;


#endif	/* __CMSCLIENT_TYPES_H__ */
