/*
 * cms_cluster.h: cms daemon cluster header
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
#ifndef __CMS_CLUSTER_H__
#define __CMS_CLUSTER_H__

#include <hb_api.h>
#include <heartbeat.h>

#include "saf/ais_message.h"
#include "cms_data.h"
#include "cms_mqueue.h"

enum mqname_type {
	MQNAME_TYPE_INIT = 1,
	MQNAME_TYPE_REQUEST = 2,
	MQNAME_TYPE_GRANTED = 3,
	MQNAME_TYPE_REOPEN = 4,
	MQNAME_TYPE_DENIED = 5,
	MQNAME_TYPE_CLOSE = 6,
	MQNAME_TYPE_UNLINK = 7,
	MQNAME_TYPE_SEND = 8,
	MQNAME_TYPE_INSERT = 9,
	MQNAME_TYPE_REMOVE = 10,
	MQNAME_TYPE_ACK = 11,
	MQNAME_TYPE_UPDATE = 12,
	MQNAME_TYPE_REOPEN_MSGFEED = 13,
	MQNAME_TYPE_MSGFEED_END = 14,
	MQNAME_TYPE_STATUS_REQUEST = 15,
	MQNAME_TYPE_STATUS_REPLY = 16,
	MQNAME_TYPE_UPDATE_REQUEST = 17,
	MQNAME_TYPE_RECEIVE = 18,
	MQNAME_TYPE_REPLY = 19,
	MQNAME_TYPE_LAST = 21
};


#define F_MQREQUEST	"mqrequesttype"
#define F_MQNAME	"mqname"
#define F_MQGROUPNAME 	"mqgroupname"
#define F_MQINVOCATION	"mqinvocation"
#define F_MQPOLICY    	"mqpolicy"
#define F_MQCREATEFLAG	"mqcreateflag"
#define F_MQOPENFLAG	"mqopenflag"
#define F_MQRETENTION	"mqretention"
#define F_MQHOST	"mqhost"
#define F_MQCLIENT	"mqclient"
#define F_MQMSGTYPE	"mqmsgtype"
#define F_MQMSGVER	"mqmsgversion"
#define F_MQMSGPRI	"mqmsgpriority"
#define F_MQMSGSIZE	"mqmsgsize"
#define F_MQMSGDATA	"mqmsgdata"
#define F_MQCLOSED	"mqclosed"
#define F_MQSTATUS	"mqstatus"
#define F_MQERROR	"mqerror"
#define F_MQMSGACK	"mqmsgack"
#define F_MQMSGSEQ	"mqmsgseq"
#define F_MQUPDATE	"mqupdate"
#define F_MQSIZE	"mqsize"
#define F_MQUSED	"mqused"
#define F_MQMSGNUM	"mqmsgnum"
#define F_MQEXPIRE	"mqexpire"
#define F_SENDRECEIVE   "mqsendreceive"
#define F_MQMSGREPLYSEQ "mqreplyseq"

#define S_MQCLOSED	"mq_s_closed"

#define PACKSTRSIZE	(SA_MSG_MESSAGE_LOWEST_PRIORITY + 1) * (16 + 1) + 1

enum mqname_type mqname_string2type(const char *str);
int cluster_hash_table_init(void);


/*
 * Naming conventions: functions prefixed with request_ are operation
 * request functions originate from the node where the request client
 * locates on.
 */
int request_mqname_open(mqueue_request_t * request, cms_data_t * cmsdata);
int request_mqname_close(const char *name, cms_data_t * cmsdata);
int request_mqname_unlink(const char *name, cms_data_t * cmsdata);
int request_mqname_send(mqueue_request_t *request, const char *node,
			const char *client,SaMsgMessageT *msg,
			cms_data_t * cmsdata);
int request_mqgroup_insert(const char *gname, const char *name,
			   cms_data_t * cmsdata);
int request_mqgroup_remove(const char *gname, const char *name,
			   cms_data_t * cmsdata);
int request_mqueue_status(mqueue_t * mqueue, cms_data_t * cmsdata);
int request_mqinfo_update(cms_data_t * cmsdata);

int send_mq_reply(mqueue_request_t * request, SaMsgSenderIdT senderId, SaMsgMessageT *msg, cms_data_t * cmsdata);


/*
 * Naming conventions: functions prefixed with reply_ are operation
 * execution functions ran on the target node, which can be either
 * a mqname master node or a mqueue owner node.
 */
int reply_mqueue_status(struct ha_msg *msg, cms_data_t * cmsdata);
int reply_mqname_open(ll_cluster_t *hb, struct ha_msg *msg);
int reply_mqinfo_update(const char * node, cms_data_t * cmsdata);

/*
 * Naming conventions: functions prefixed with process_ are post
 * execution functions ran on the original node for this request.
 * On the other hand, other nodes in the cluster may also ran this
 * function when it is a a broadcast message that requires action
 * from all nodes.
 */
int process_mqname_granted(struct ha_msg *msg, cms_data_t * cmsdata);
int process_mqname_reopen(struct ha_msg *msg, enum mqname_type type,
			  cms_data_t * cmsdata);
int process_mqname_denied(struct ha_msg *msg);
int process_mqname_close(struct ha_msg *msg);
int process_mqname_unlink(struct ha_msg *msg);
int process_mqname_send(struct ha_msg *msg, cms_data_t * cmsdata);
int process_mqgroup_insert(struct ha_msg *msg);
int process_mqgroup_remove(struct ha_msg *msg);
int process_mqname_ack(struct ha_msg *msg);
int process_mqname_update(struct ha_msg *msg, cms_data_t * cmsdata);
int process_mqueue_status(struct ha_msg *msg);
int process_mqinfo_update_request(struct ha_msg *msg, cms_data_t * cmsdata);
int process_mqsend_reply(struct ha_msg * msg, cms_data_t * cmsdata);

#endif	/* __CMS_CLUSTER_H__ */
