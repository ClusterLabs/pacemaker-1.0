/*
 * cms_cluster.c: cms daemon cluster operation
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <cl_log.h>
#include <heartbeat.h>

#include "cms_common.h"

#define MQ_MEMBER_COUNT	g_list_length(mqmember_list)

#define RETENTION_TIME_EXPIRES(mq) (				    \
		mq->status.creationFlags != SA_MSG_QUEUE_PERSISTENT \
		&&	mq->status.closeTime > 0		    \
		&&	get_current_satime() - mq->status.closeTime \
			>= mq->status.retentionTime)


GHashTable * mq_open_pending_hash;
GHashTable * mq_status_pending_hash;
GHashTable * mq_ack_pending_hash;

char mqname_type_str[MQNAME_TYPE_LAST][TYPESTRSIZE] = {
	"",
	"mqinit",
	"mqrequest",
	"mqgranted",
	"mqreopen",
	"mqdenied",
	"mqclose",
	"mqunlink",
	"mqsend",
	"mqinsert",
	"mqremove",
	"mqmsgack",
	"mqinfoupdate",
	"mqreopenmsgfeed",
	"mqmsgfeedend",
	"mqstatusrequest",
	"mqstatusreply",
	""
};

char cmsrequest_type_str[CMS_TYPE_TOTAL][TYPESTRSIZE] = {
	"cms_qstatus",
	"cms_qopen",
	"cms_qopenasync",
	"cms_qclose",
	"cms_qunlink",
	"cms_msend",
	"cms_msendasync",
	"cms_mack",
	"cms_mget",
	"cms_mreceivedget",
	"cms_qg_creat",
	"cms_qg_delete",
	"cms_qg_insert",
	"cms_qg_remove",
	"cms_qg_track_start",
	"cms_qg_track_stop",
	"cms_qg_notify",
	"cms_msg_notify",
	"cms_msg_request"
};

char sa_errortype_str[SA_ERR_BAD_FLAGS + 2][TYPESTRSIZE] = {
	"",
	"sa_ok",
	"sa_err_library",
	"sa_err_version",
	"sa_err_init",
	"sa_err_timeout",
	"sa_err_try_again",
	"sa_err_invalid_param",
	"sa_err_no_memory",
	"sa_err_bad_handle",
	"sa_err_busy",
	"sa_err_access",
	"sa_err_not_exist",
	"sa_err_name_too_long",
	"sa_err_exist",
	"sa_err_no_space",
	"sa_err_interrupt",
	"sa_err_system",
	"sa_err_name_not_found",
	"sa_err_no_resources",
	"sa_err_not_supported",
	"sa_err_bad_operation",
	"sa_err_failed_operation",
	"sa_err_message_error",
	"sa_err_no_message",
	"sa_err_queue_full",
	"sa_err_queue_not_available",
	"sa_err_bad_checkpoint",
	"sa_err_bad_flags",
	""
};


/**
 * cluster_hash_table_init - initialize local message queue database
 */
int
cluster_hash_table_init()
{
	mqueue_table_init();
	mq_open_pending_hash = g_hash_table_new(g_str_hash, g_str_equal);
	mq_status_pending_hash = g_hash_table_new(g_str_hash, g_str_equal);
	mq_ack_pending_hash = g_hash_table_new(g_int_hash, g_int_equal);

	return HA_OK;
}

/**
 * request_mqname_open - apply for a message queue in the cluster
 * @name: message queue name
 * @cmsdata: pointer to cms daemon private data struct
 *
 * If the request is sent out successfully, it returns TRUE; otherwise
 * returns FALSE. This call is non-blocking.
 */
int
request_mqname_open(mqueue_request_t * request, cms_data_t * cmsdata)
{
	int ret;
	ll_cluster_t *hb;
	struct ha_msg *msg;
	const char *tonode, *type;
	int index, i;
	char *data, *p;
	char size_string[64];

	index = g_str_hash(request->qname) % MQ_MEMBER_COUNT;
	dprintf("index = %d, total = %d\n", index, MQ_MEMBER_COUNT);
	data = g_list_nth_data(mqmember_list, (guint)index);
	assert(data);
	tonode = data;
	type = mqname_type2string(MQNAME_TYPE_REQUEST);

	for (p = size_string, i = 0; i <= SA_MSG_MESSAGE_LOWEST_PRIORITY; i++) {
		p += sprintf(p, "%d:", request->size[i]);
	}

	dprintf("%s: tonode is [%s]\n", __FUNCTION__, tonode);

	if ((msg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}

	if (ha_msg_add(msg, F_TYPE, type) == HA_FAIL 
	||	ha_msg_add(msg, F_MQREQUEST, 
			cmsrequest_type2string(request->request_type))
			== HA_FAIL
	||	ha_msg_add(msg, F_MQNAME, request->qname) == HA_FAIL
	||	ha_msg_addbin(msg, F_MQPOLICY, &(request->policy), sizeof(int))
			== HA_FAIL
	||	ha_msg_addbin(msg, F_MQINVOCATION, &(request->invocation), 
			sizeof(int)) == HA_FAIL
	||	ha_msg_addbin(msg, F_MQCREATEFLAG, &(request->create_flag),
			sizeof(SaMsgQueueCreationFlagsT)) == HA_FAIL
	||	ha_msg_addbin(msg, F_MQOPENFLAG, &(request->open_flag),
			sizeof(SaMsgQueueOpenFlagsT)) == HA_FAIL
	||	ha_msg_addbin(msg, F_MQRETENTION, &(request->retention),
			sizeof(SaTimeT)) == HA_FAIL
	||	ha_msg_add(msg, F_MQSIZE, size_string) == HA_FAIL) {

		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);

		ret = FALSE;

	} else {
		hb = cmsdata->hb_handle;
		hb->llc_ops->sendnodemsg(hb, msg, tonode);

		ret = TRUE;
	}

	ha_msg_del(msg);
	return ret;
}

/**
 * request_mqname_close - apply for a message queue in the cluster
 * @name: message queue name
 * @cmsdata: pointer to cms daemon private data struct
 *
 * Always returns TRUE. This call is non-blocking.
 */
int
request_mqname_close(const char *name, cms_data_t * cmsdata)
{
	ll_cluster_t *hb;
	struct ha_msg *msg;
	const char *type;

	CMS_TRACE();

	type = mqname_type2string(MQNAME_TYPE_CLOSE);

	if ((msg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (ha_msg_add(msg, F_TYPE, type) == HA_FAIL 
	||	ha_msg_add(msg, F_MQNAME, name) == HA_FAIL) {

		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;
	}

	hb = cmsdata->hb_handle;
#if DEBUG_CLUSTER
	cl_log_message(msg);
#endif
	hb->llc_ops->sendclustermsg(hb, msg);

	ha_msg_del(msg);
	return SA_OK;
}

/**
 * request_mqname_unlink - apply for a message queue in the cluster
 * @name: message queue name
 * @cmsdata: pointer to cms daemon private data struct
 *
 * Always returns TRUE. This call is non-blocking.
 */
int
request_mqname_unlink(const char *name, cms_data_t * cmsdata)
{
	ll_cluster_t *hb;
	struct ha_msg *msg;
	const char *type;

	type = mqname_type2string(MQNAME_TYPE_UNLINK);

	if ((msg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (ha_msg_add(msg, F_TYPE, type) == HA_FAIL 
	||	ha_msg_add(msg, F_MQNAME, name) == HA_FAIL) {

		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;

	} else {
		hb = cmsdata->hb_handle;
#if DEBUG_CLUSTER
		cl_log_message(msg);
#endif
		hb->llc_ops->sendclustermsg(hb, msg);
	}

	ha_msg_del(msg);
	return SA_OK;
}

/**
 * request_mqname_send - send a message to a message queue in the cluster
 *
 * Always returns TRUE. This call is non-blocking.
 */
int
request_mqname_send(mqueue_request_t * request, const char *node,
		    const char * client, SaMsgMessageT *msg,
		    cms_data_t * cmsdata)
{
	int ret;
	ll_cluster_t *hb;
	struct ha_msg *m;
	const char *type;

	type = mqname_type2string(MQNAME_TYPE_SEND);

	if ((m = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (ha_msg_add(m, F_TYPE, type) == HA_FAIL 
	|| 	ha_msg_add(m, F_MQREQUEST, 
		    cmsrequest_type2string(request->request_type)) == HA_FAIL
	||	ha_msg_add(m, F_MQNAME, request->qname) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGTYPE, (char *) &msg->type, 
			sizeof(SaSizeT)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGVER, (char *) &msg->version,
			sizeof(SaSizeT)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGSIZE, (char *) &msg->size,
			sizeof(SaSizeT)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGPRI, (char *) &msg->priority,
			sizeof(SaUint8T)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGDATA, (char *) msg->data, 
			msg->size) == HA_FAIL
	||	ha_msg_addbin(m, F_MQINVOCATION, &(request->invocation),
		    	sizeof(int)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGSEQ, &(request->seq),
		    	sizeof(unsigned long)) == HA_FAIL
	|| 	ha_msg_addbin(m, F_MQMSGACK, &(request->ack), 
		    	sizeof(SaMsgAckFlagsT)) == HA_FAIL) {

		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		ha_msg_del(m);
		return FALSE;
	} 

	if (request->gname != NULL) {
		if (ha_msg_add(m, F_MQGROUPNAME, request->gname) == HA_FAIL) {
			cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
			ha_msg_del(m);
			return FALSE;
		}
	}
	
	hb = cmsdata->hb_handle;
#if DEBUG_CLUSTER
	cl_log_message(m);
#endif
	ret = hb->llc_ops->sendnodemsg(hb, m, node);
	dprintf("%s: node = %s, ops->sendnodemsg: ret = %d\n"
	,	__FUNCTION__, node, ret);

	ha_msg_del(m);
	return SA_OK;
}

/*
 * ack the mqname_send back to the sender's client.
 */
static int
mqname_send_ack(mqueue_request_t * request, const char *node,
		const char *client, const SaErrorT ret, cms_data_t * cmsdata)
{
	ll_cluster_t *hb;
	struct ha_msg *m;
	const char *type;

	type = mqname_type2string(MQNAME_TYPE_ACK);

	if ((m = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (ha_msg_add(m, F_TYPE, type) == HA_FAIL 
	||	ha_msg_add(m, F_MQNAME, request->qname) == HA_FAIL
	|| 	ha_msg_add(m, F_MQREQUEST, 
		    cmsrequest_type2string(request->request_type)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQINVOCATION, &(request->invocation),
		    sizeof(int)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGSEQ, &(request->seq),
		    sizeof(unsigned long)) == HA_FAIL
	||	ha_msg_add(m, F_MQERROR, saerror_type2string(ret)) == HA_FAIL) {
		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;

	} 

	dprintf("mqname_send_ack, request->qname = %s\n", request->qname);

	hb = cmsdata->hb_handle;
#if DEBUG_CLUSTER
	cl_log_message(m);
#endif
	hb->llc_ops->sendnodemsg(hb, m, node);

	ha_msg_del(m);
	return TRUE;
}

int
request_mqgroup_insert(const char *gname, const char *name,
		       cms_data_t * cmsdata)
{
	ll_cluster_t *hb;
	struct ha_msg *msg;
	const char *type;

	type = mqname_type2string(MQNAME_TYPE_INSERT);

	if ((msg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (ha_msg_add(msg, F_TYPE, type) == HA_FAIL
	||	ha_msg_add(msg, F_MQGROUPNAME, gname) == HA_FAIL
	||	ha_msg_add(msg, F_MQNAME, name) == HA_FAIL) {
		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;
	} else {
		hb = cmsdata->hb_handle;
#if DEBUG_CLUSTER
		cl_log_message(msg);
#endif
		hb->llc_ops->sendclustermsg(hb, msg);
	}
	ha_msg_del(msg);
	return SA_OK;
}

int
request_mqgroup_remove(const char *gname, const char *name,
		       cms_data_t * cmsdata)
{
	ll_cluster_t *hb;
	struct ha_msg *msg;
	const char *type;

	type = mqname_type2string(MQNAME_TYPE_REMOVE);

	if ((msg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (ha_msg_add(msg, F_TYPE, type) == HA_FAIL
	||	ha_msg_add(msg, F_MQGROUPNAME, gname) == HA_FAIL
	||	ha_msg_add(msg, F_MQNAME, name) == HA_FAIL) {
		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;
	} else {
		hb = cmsdata->hb_handle;
#if DEBUG_CLUSTER
		cl_log_message(msg);
#endif
		hb->llc_ops->sendclustermsg(hb, msg);
	}
	ha_msg_del(msg);
	return SA_OK;
}

int
request_mqname_update(const char * node, cms_data_t * cmsdata)
{
	struct mq_info * mqinfo;
	size_t mqinfo_len;

	const char * type;
	struct ha_msg *msg;
	ll_cluster_t *hb;

	if (mqueue_table_pack(&mqinfo, &mqinfo_len) != HA_OK) {
		return HA_FAIL;
	}

	if ((msg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return HA_FAIL;
	}

	type = mqname_type2string(MQNAME_TYPE_UPDATE);
	
	if (ha_msg_add(msg, F_TYPE, type) == HA_FAIL
	||	ha_msg_addbin(msg, F_MQUPDATE, mqinfo, mqinfo_len) == HA_FAIL)
	{
		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return HA_FAIL;
	} 

	hb = cmsdata->hb_handle;
#if DEBUG_CLUSTER
	cl_log_message(msg);
#endif
	hb->llc_ops->sendnodemsg(hb, msg, node);
	ha_free(mqinfo);

	ha_msg_del(msg);
	return HA_OK;
}

/**
 * reply_mqname_open - process the request message as the master node
 *		       for this message queue name
 *
 * @hb:		heartbeat IPC Channel handle
 * @msg:	received message from heartbeat IPC Channel
 */
int
reply_mqname_open(ll_cluster_t *hb, struct ha_msg *msg)
{
	const char *name, *type, *request;
	size_t  invocation_size, cflag_size, oflag_size, retention_size;
	const SaInvocationT * invocation = NULL;
	const SaMsgQueueCreationFlagsT *cflag = NULL, *oflag = NULL;
	const SaTimeT * retention = NULL;
	const int * policy = NULL;
	const char * size_string;
	SaMsgQueueSendingStateT sending_state;

	mqueue_t *mq;
	struct ha_msg *reply;
	SaErrorT error = SA_OK;

	request = NULL;

	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL
	||	(request = ha_msg_value(msg, F_MQREQUEST)) == NULL
	||	(invocation = cl_get_binary(msg, F_MQINVOCATION, 
			&invocation_size)) == NULL
	||	(policy = cl_get_binary(msg, F_MQPOLICY, NULL)) == NULL
	||	(cflag = cl_get_binary(msg, F_MQCREATEFLAG, &cflag_size))
			== NULL
	||	(oflag = cl_get_binary(msg, F_MQOPENFLAG, &oflag_size))
			== NULL
	||	(retention = cl_get_binary(msg, F_MQRETENTION, &retention_size))
			== NULL
	||	(size_string = ha_msg_value(msg, F_MQSIZE)) == NULL) {

		cl_log(LOG_ERR, "received bad mq request: name = %s, request"
		 		" = %s, invo = %d, policy = %d", name, request
		 		,	*invocation, *policy);

		return FALSE;
	}

	if ((reply = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}

	if (((mq = mqname_lookup(name, NULL)) != NULL)
	&&	((mq->mqstat != MQ_STATUS_CLOSE)
	||	(mq->mqstat == MQ_STATUS_CLOSE && mq->policy != *policy))) {

		cl_log(LOG_INFO, "mq name [%s] already exists", name);
		error = SA_ERR_EXIST;
		type = mqname_type2string(MQNAME_TYPE_DENIED);

		if (ha_msg_add(reply, F_TYPE, type) == HA_FAIL
		|| 	ha_msg_add(reply, F_MQREQUEST, request) == HA_FAIL
		||	ha_msg_add(reply, F_MQNAME, name) == HA_FAIL
		||	ha_msg_addbin(reply, F_MQINVOCATION, invocation, 
				invocation_size) == HA_FAIL
		||	ha_msg_add(reply, F_MQERROR, saerror_type2string(error))
				== HA_FAIL) {

			cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
			return FALSE;
		}

		dprintf("error is %d\n", error);
		goto send_msg;

	} else if ((mq != NULL) && mq->mqstat == MQ_STATUS_CLOSE) {
		/*
		 * The mq is closed, here need to be reopened.
		 */
		type = mqname_type2string(MQNAME_TYPE_REOPEN);
		error = SA_OK;

		ha_free(mq->host);
		mq->host = ha_strdup(ha_msg_value(msg, F_ORIG));

		/* we must not set mq->mqstat to MQ_STATUS_OPEN here
		 * because on reopen case, the original master name
		 * server need to check this bit before msgfeed.
		 */
		//mq->mqstat = MQ_STATUS_OPEN;

		mq->list = NULL;
		mq->current = NULL;
		mq->notify_list = NULL; 

	} else {
		type = mqname_type2string(MQNAME_TYPE_GRANTED);
		sending_state = SA_MSG_QUEUE_AVAILABLE;

		mq = (mqueue_t *) ha_malloc(sizeof(mqueue_t));
		if (!mq) {
			cl_log(LOG_ERR, "%s: ha_malloc failed\n", __FUNCTION__);
			return FALSE;
		}

		memset(mq, 0, sizeof(mqueue_t));
		mq->name = ha_strdup(name);
		mq->host = ha_strdup(ha_msg_value(msg, F_ORIG));
		mq->mqstat = MQ_STATUS_OPEN;
		mq->policy = *policy; 
		
		error = mqueue_table_insert(mq);
	}

	/*
	 * master node broadcast the result in the cluster
	 */
	if (ha_msg_add(reply, F_TYPE, type) == HA_FAIL
	||	ha_msg_add(reply, F_MQNAME, name) == HA_FAIL
	||	ha_msg_add(reply, F_MQREQUEST, request) == HA_FAIL
	||	ha_msg_addbin(reply, F_MQINVOCATION, invocation, 
			invocation_size) == HA_FAIL
	||	ha_msg_add(reply, F_MQHOST, mq->host) == HA_FAIL
	||	ha_msg_addbin(reply, F_MQSTATUS, &sending_state,
			sizeof(SaMsgQueueSendingStateT)) == HA_FAIL
	||	ha_msg_addbin(reply, F_MQPOLICY, policy, sizeof(int)) == HA_FAIL
	||	ha_msg_addbin(reply, F_MQCREATEFLAG, cflag, cflag_size)
			== HA_FAIL
	||	ha_msg_addbin(reply, F_MQOPENFLAG, oflag, oflag_size)
			== HA_FAIL
	||	ha_msg_addbin(reply, F_MQRETENTION, retention, retention_size)
			== HA_FAIL
	||	ha_msg_add(reply, F_MQSIZE, size_string) == HA_FAIL
	||	ha_msg_add(reply, F_MQERROR, saerror_type2string(error))
			== HA_FAIL) {

		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;
	}

send_msg:
	hb->llc_ops->sendclustermsg(hb, reply);
	ha_msg_del(reply);

	return TRUE;
}

int
process_mqname_close(struct ha_msg *msg)
{
	const char *name;
	mqueue_t *mq;

	CMS_TRACE();

	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL) {
		cl_log(LOG_ERR, "received NULL mq name request");
		return FALSE;
	}
	if (((mq = mqname_lookup(name, NULL)) != NULL)
	&&	(mq->mqstat != MQ_STATUS_CLOSE)) {
		mq->mqstat = MQ_STATUS_CLOSE;
		cl_log(LOG_INFO, "%s: Set mq [%s] status to [%d]"
		,	__FUNCTION__, name, mq->mqstat);
	}

#if DEBUG_CLUSTER
	cl_log_message(msg);
#endif
	return SA_OK;
}

int
process_mqname_unlink(struct ha_msg *msg)
{
	const char *name;
	mqueue_t *mq;

	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL) {
		cl_log(LOG_ERR, "received NULL mq name request");
		return FALSE;
	}
	if ((mq = mqname_lookup(name, NULL)) != NULL) {
		mqueue_table_remove(name);
		/*
		 * TODO: remove handle hash also
		 */
	}

#if DEBUG_CLUSTER
	cl_log_message(msg);
#endif
	return SA_OK;
}

int
process_mqname_send(struct ha_msg *msg, cms_data_t * cmsdata)
{
	const char *name, * gname, * request_type;
	const void * data, * ack, *invocation, *seq, *msg_pri;
	const SaSizeT * msg_type, * msg_ver, * msg_size;
	const char *node;
	mqueue_request_t request;
	SaMsgMessageT * message;
	SaErrorT ret = SA_OK;
	size_t data_len, ack_len, invocation_len, seq_len;
	size_t type_len, ver_len, pri_len, size_len;
	mqueue_t *mq;
	client_mqueue_notify_t m;
	const SaUint8T * priority;

	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL
	||	(request_type = ha_msg_value(msg, F_MQREQUEST)) == NULL
	||	(msg_type = cl_get_binary(msg, F_MQMSGTYPE, &type_len)) 
			== NULL
	||	(msg_ver = cl_get_binary(msg, F_MQMSGVER, &ver_len)) 
			== NULL
	||	(msg_pri = cl_get_binary(msg, F_MQMSGPRI, &pri_len))
			== NULL
	||	(msg_size = cl_get_binary(msg, F_MQMSGSIZE, &size_len))
			== NULL
	||	(data = cl_get_binary(msg, F_MQMSGDATA, &data_len)) == NULL
	||	(invocation = cl_get_binary(msg, F_MQINVOCATION, 
			&invocation_len)) == NULL
	||	(seq = cl_get_binary(msg, F_MQMSGSEQ, &seq_len)) == NULL
	||	(ack = cl_get_binary(msg, F_MQMSGACK, &ack_len)) == NULL
	||	*msg_size != data_len ) {

		cl_log(LOG_ERR, "received bad mqname_send request.");
		return FALSE;
	}

	gname = ha_msg_value(msg, F_MQGROUPNAME);
	priority = (const SaUint8T *) msg_pri;
	if ((*priority) > SA_MSG_MESSAGE_LOWEST_PRIORITY) {
		cl_log(LOG_ALERT, "Wrong priorty [%u]\n", *priority);
		return FALSE;
	}
	
	dprintf("%s: going to send to %s\n", __FUNCTION__, name);

	if ((mq = mqname_lookup(name, NULL)) != NULL) {

		dprintf("%s: data_len = %d\n", __FUNCTION__, (int)data_len);
		dprintf("buff_avai[%d] = %lu\n", *priority, 
			BUFFER_AVAILABLE(mq, *priority));

		/*
		 * don't deliver the msg if no buffer
		 */
		if ((BUFFER_AVAILABLE(mq, *priority) - data_len
		-	sizeof(SaMsgMessageT)) < 0) {
			cl_log(LOG_DEBUG, "%s: buffer over flow, msg not "
				"delivered. ", __FUNCTION__);
			ret = SA_ERR_QUEUE_FULL;

		} else {
			/*
			 * save message to mq->message_buffer
			 */
			message = (SaMsgMessageT*)
				ha_malloc(sizeof(SaMsgMessageT) + data_len);
			if (!message)
				return SA_ERR_NO_MEMORY;

			message->type = *msg_type;
			message->version = *msg_ver;
			message->size = *msg_size;
			message->priority = * priority;
			message->data = (char *)message + sizeof(SaMsgMessageT);
			memcpy(message->data, data, data_len);

			enqueue_message(mq, *priority, message);

			/*
			 * send only limited info to client
			 */
			m.header.type = CMS_MSG_NOTIFY;
			m.header.len = sizeof(client_header_t);
			m.header.flag = SA_OK;
			m.header.name.length = strlen(name) + 1;
			m.handle = mq->handle;
			strncpy(m.header.name.value, name, SA_MAX_NAME_LENGTH);

			/*
			 * send the info to the client
			 */
			ret = client_send_msg(mq->client,
					sizeof(client_mqueue_notify_t), &m);

			mq->notified = TRUE;
			ret = SA_OK;
		}

		/*
		 * send the ack back
		 */
		if (ret == SA_OK && ack) {
			node = ha_msg_value(msg, F_ORIG);

			if (gname) {
				request.qname = ha_strdup(gname);
			} else {
				request.qname = ha_strdup(name);
			}
			request.gname = NULL;
			request.request_type =
				cmsrequest_string2type(request_type);
			request.invocation = *(const int *)invocation;
			request.ack = *(const int *)ack;
			request.seq = *(const unsigned long *) seq;

			mqname_send_ack(&request, node, NULL, ret, cmsdata);
			ha_free(request.qname);

			dprintf("send the ack, ret = %d\n", ret);
		}
	} else {
		node = ha_msg_value(msg, F_ORIG);
		cl_log(LOG_ERR, "%s: msg queue not found. the name server"
			" database on node %s is bad.", __FUNCTION__, node);
	}

	return SA_OK;
}

static void
send_msg_notify(gpointer data, gpointer user_data)
{
	client_mqueue_notify_t m;
	mqueue_t * mq = user_data;

	m.header.type = CMS_MSG_NOTIFY;
	m.header.len = sizeof(client_header_t);
	m.header.flag = SA_OK;
	m.header.name.length = strlen(mq->name) + 1;
	m.handle = mq->handle;
	strncpy(m.header.name.value, mq->name, SA_MAX_NAME_LENGTH);

	client_send_msg(mq->client, sizeof(client_mqueue_notify_t), &m);
}

static void
send_migrate_message_notify(mqueue_t * mq)
{
	SaUint8T i;

	for (i = SA_MSG_MESSAGE_HIGHEST_PRIORITY
	;	i <= SA_MSG_MESSAGE_LOWEST_PRIORITY
	;	i++)

		g_list_foreach(mq->message_buffer[i], send_msg_notify, mq);
}


/**
 * process_mqname_granted - process the granted message from the master node
 *			    for this message queue name
 * @msg: received message from heartbeat IPC Channel
 */
int
process_mqname_granted(struct ha_msg *msg, cms_data_t * cmsdata)
{
	const char *name, *host, *error, *request;
	const int * invocation, *policy;
	size_t invocation_size; 
	const SaMsgQueueCreationFlagsT *cflag = NULL, *oflag = NULL;
	const SaMsgQueueSendingStateT * sending_state;
	const SaTimeT * retention = NULL;
	const char *size_string;
	
	IPC_Channel *client;
	mqueue_t *mq, *mq_pending;
	mqueue_request_t mq_request;
	cms_client_t * cms_client;
	guint handle;
	int flag;


	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL
	||	(request = ha_msg_value(msg, F_MQREQUEST)) == NULL
	||	(invocation = cl_get_binary(msg, F_MQINVOCATION, 
			&invocation_size)) == NULL
	||	(host = ha_msg_value(msg, F_MQHOST)) == NULL
	||	(policy = cl_get_binary(msg, F_MQPOLICY, NULL)) == NULL
	||	(cflag = cl_get_binary(msg, F_MQCREATEFLAG, NULL)) == NULL
	||	(oflag = cl_get_binary(msg, F_MQOPENFLAG, NULL)) == NULL
	||	(retention = cl_get_binary(msg, F_MQRETENTION, NULL)) == NULL
	||	(sending_state = cl_get_binary(msg, F_MQSTATUS, NULL)) == NULL
	||	(size_string = ha_msg_value(msg, F_MQSIZE)) == NULL
	||	(error = ha_msg_value(msg, F_MQERROR)) == NULL) {

			cl_log(LOG_ERR, "%s: ha_msg_value error", __FUNCTION__);
			return FALSE;
	}

	flag = saerror_string2type(error);

	/*
	 * This node might be the mqname master node, so make sure don't
	 * duplicate insertion.
	 */
	if ((mq = mqueue_table_lookup(name, NULL)) == NULL) {
		/*
		 * this is not the master node
		 */
		mq = (mqueue_t *) ha_malloc(sizeof(mqueue_t));
		if (!mq) {
			cl_log(LOG_ERR, "%s: ha_malloc failed\n", __FUNCTION__);
			return FALSE;
		}
		memset(mq, 0, sizeof(mqueue_t));

		mq->name = ha_strdup(name);
		mq->host = ha_strdup(host);
		mq->mqstat = MQ_STATUS_OPEN;
		mq->client = NULL;
		mq->policy = *policy;
		
		mqueue_table_insert(mq);
	}

	sa_mqueue_usage_decode(size_string, NULL, NULL
	,	mq->status.saMsgQueueUsage);

	/*
	 * set SaMsgQueueStatus in local database
	 */
	mq->status.sendingState = *sending_state;
	mq->status.creationFlags = *cflag;
	mq->status.openFlags = *oflag;
	mq->status.retentionTime = *retention;
	mq->status.headerLength = 0;

	mq_pending = g_hash_table_lookup(mq_open_pending_hash, name);

	if (mq_pending != NULL) {
		/*
		 * This is the local node for this msg queue, 
		 * we have clients open pending, send out reply.
		 */
		client = mq_pending->client;
		handle = mqueue_handle_insert(mq);

		/*
		 * insert this mq to client's opened_mqueue_list
		 */
		dprintf("lookup farside_pid [%d] in <%p>\n"
		,	client->farside_pid, cmsdata->client_table);

		cms_client = g_hash_table_lookup(cmsdata->client_table,
					&(client->farside_pid));

		assert(cms_client != NULL);

		cms_client->opened_mqueue_list =
			g_list_append(cms_client->opened_mqueue_list, mq);

		mq->client = (IPC_Channel *)ha_malloc(sizeof(IPC_Channel));

		if (mq->client == NULL) {
			cl_log(LOG_ERR, "%s: ha_malloc failed\n", __FUNCTION__);
			return FALSE;
		}

		*mq->client = *mq_pending->client;

		mq_request.qname = ha_strdup(name);
		mq_request.invocation = *invocation;
		mq_request.request_type = cmsrequest_string2type(request);

		client_send_client_qopen(client, &mq_request, handle, flag);
		ha_free(mq_request.qname);

		g_hash_table_remove(mq_open_pending_hash, name);

		dprintf("%p %p %p\n", mq_pending->name, mq_pending->client
		,	mq_pending);
		ha_free(mq_pending->name);
		ha_free(mq_pending->client);
		ha_free(mq_pending);

		/*
		 * send out notify msg for migratable mq if any
		 */
		send_migrate_message_notify(mq);
	}

	return TRUE;
}

static int
send_undelivered_message(ll_cluster_t *hb, mqueue_t *mq, const char *node)
{
	SaMsgMessageT * message;
	struct ha_msg *m;
	const char *type = mqname_type2string(MQNAME_TYPE_REOPEN_MSGFEED);
	int invalid = FALSE;
	const char * request_type;
	char size_string[PACKSTRSIZE]; 


	sa_mqueue_usage_encode(size_string, NULL, NULL
	,	mq->status.saMsgQueueUsage);

	dprintf("Timer: current - close %s retention\n"
	,	get_current_satime() - mq->status.closeTime
	<	mq->status.retentionTime ? "<" : ">=");

	if (RETENTION_TIME_EXPIRES(mq)) {
		/* retention timer expires */
		cl_log(LOG_INFO, "Original node: open a expired queue [%s]"
		,	mq->name);
		invalid = TRUE;
		goto end;
	}

	while ((message = dequeue_message(mq))) {

	if ((m = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (ha_msg_add(m, F_TYPE, type) == HA_FAIL 
	||	ha_msg_add(m, F_MQNAME, mq->name) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGTYPE, (char *) &message->type, 
			sizeof(SaSizeT)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGVER, (char *) &message->version,
			sizeof(SaSizeT)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGSIZE, (char *) &message->size,
			sizeof(SaSizeT)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGPRI, (char *) &message->priority,
			sizeof(SaUint8T)) == HA_FAIL
	||	ha_msg_addbin(m, F_MQMSGDATA, (char *) message->data, 
			message->size) == HA_FAIL) {

		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		ha_msg_del(m);
		return FALSE;
	} 
	hb->llc_ops->sendnodemsg(hb, m, node);
	dprintf("Send 1 msgfeed %d size msg to %s\n"
	,	message->size, node);
	ha_msg_del(m);

	}

end:
	/*
	 * send the MSGFEED_END message
	 */
	type = mqname_type2string(MQNAME_TYPE_MSGFEED_END);
	if ((m = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}
	if (invalid == TRUE)
		request_type = "invalid";
	else
		request_type = "valid";

	if (ha_msg_add(m, F_TYPE, type) == HA_FAIL
	||	ha_msg_add(m, F_MQNAME, mq->name) == HA_FAIL
	||      ha_msg_add(m, F_MQREQUEST, request_type) == HA_FAIL
	||      ha_msg_addbin(m, F_MQPOLICY, &mq->policy, sizeof(int))
			== HA_FAIL
	||      ha_msg_addbin(m, F_MQCREATEFLAG, &mq->status.creationFlags,
			sizeof(SaMsgQueueCreationFlagsT)) == HA_FAIL
	||      ha_msg_addbin(m, F_MQOPENFLAG, &mq->status.openFlags,
			sizeof(SaMsgQueueOpenFlagsT)) == HA_FAIL
	||      ha_msg_addbin(m, F_MQRETENTION, &mq->status.retentionTime,
			sizeof(SaTimeT)) == HA_FAIL
	||      ha_msg_add(m, F_MQSIZE, size_string) == HA_FAIL) {

		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		ha_msg_del(m);
		return FALSE;
	}

	hb->llc_ops->sendnodemsg(hb, m, node);
	dprintf("Send msgfeed_end msg to %s\n", node);
	ha_msg_del(m);
	return TRUE;
}

int
process_mqname_reopen(struct ha_msg *msg, enum mqname_type type,
		      cms_data_t * cmsdata)
{
	static struct ha_msg * saved_msg = NULL;
	struct ha_msg * new_msg;
	const char *name, *node;
	ll_cluster_t *hb = cmsdata->hb_handle;
	mqueue_t * mq;
	SaMsgMessageT * message;
	const SaSizeT * msg_type, * msg_ver, * msg_size, * msg_pri, * data;
	size_t type_len, ver_len, pri_len, size_len, data_len;
	const char *valid;
	const char *size_string = NULL;
	const int *s_invocation, *policy;
	size_t s_invocation_size, cflag_size, oflag_size, retention_size;
	const SaTimeT * retention = NULL;
	char *s_request, *s_error;
	const SaMsgQueueCreationFlagsT *cflag = NULL, *oflag = NULL;
	SaMsgQueueSendingStateT sending_state = SA_MSG_QUEUE_AVAILABLE;

	dprintf("%s: type is %d\n", __FUNCTION__, type);
	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL) {
		cl_log(LOG_ERR, "%s: ha_msg_value failed",__FUNCTION__);
		return FALSE;
	}

	switch (type) {

	case MQNAME_TYPE_REOPEN:
		if ((node = ha_msg_value(msg, F_MQHOST)) == NULL) {
			cl_log(LOG_ERR, "%s: ha_msg_value failed",__FUNCTION__);
			return FALSE;
		}
		if (((mq = mqname_lookup(name, NULL)) != NULL)
		&&	mq->mqstat == MQ_STATUS_CLOSE) {

			/* Do I have the original mq? */ 
			cl_log(LOG_INFO, "Original mq host is %s", mq->host);
			if (is_host_local(mq->host, cmsdata)) {
				/* Send undelivered message to new mq */
				send_undelivered_message(hb, mq, node);
			}
		}
		dprintf("mq->mqstat = <%d>\n", mq->mqstat);
		if (!is_host_local(node, cmsdata))
			return TRUE;

		saved_msg = ha_msg_copy(msg);
		cl_log(LOG_INFO, "%s: waiting for msgfeed...", __FUNCTION__);
		break;

	case MQNAME_TYPE_REOPEN_MSGFEED:
		if ((msg_type = cl_get_binary(msg, F_MQMSGTYPE, &type_len))
				== NULL
		||	(msg_ver = cl_get_binary(msg, F_MQMSGVER, &ver_len)) 
				== NULL
		||	(msg_pri = cl_get_binary(msg, F_MQMSGPRI, &pri_len))
				== NULL
		||	(msg_size = cl_get_binary(msg, F_MQMSGSIZE, &size_len))
				== NULL
		||	(data = cl_get_binary(msg, F_MQMSGDATA, &data_len))
				== NULL) {

			cl_log(LOG_ERR, "received bad msgfeed msg.");
			return FALSE;
		}
		if ((mq = mqname_lookup(name, NULL)) == NULL
		||	mq->mqstat != MQ_STATUS_CLOSE) {
			cl_log(LOG_ALERT, "State machine BUG");
			return FALSE;
		}

		message = (SaMsgMessageT *)
			ha_malloc(sizeof(SaMsgMessageT) + data_len);

		if (!message) {
			cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
			return FALSE;
		}

		message->type = *msg_type;
		message->version = *msg_ver;
		message->size = *msg_size;
		message->priority = *(const SaUint8T *) msg_pri;
		message->data = (char *)message + sizeof(SaMsgMessageT);
		memcpy(message->data, data, data_len);

		enqueue_message(mq, message->priority, message);
		break;

	case MQNAME_TYPE_MSGFEED_END:
		if ((mq = mqname_lookup(name, NULL)) == NULL
		||	mq->mqstat != MQ_STATUS_CLOSE) {
			cl_log(LOG_ALERT, "State machine BUG");
			return FALSE;
		}
		mq->mqstat = MQ_STATUS_OPEN;

		dprintf("in feedend, used is [%d]\n"
		,	mq->status.saMsgQueueUsage[3].queueUsed);

		/*
		 * read saved_msg
		 */
		if ((s_request = ha_strdup(ha_msg_value(saved_msg,
				F_MQREQUEST))) == NULL
		||	(s_invocation = cl_get_binary(saved_msg, F_MQINVOCATION,
				&s_invocation_size)) == NULL
		||	(s_error = ha_strdup(ha_msg_value(saved_msg,F_MQERROR)))
				== NULL) {
			cl_log(LOG_ERR, "%s: ha_msg_value error", __FUNCTION__);
			return FALSE;
		}

		/*
		 * If mq retention time not expired, we will receive
		 * a valid mq status information, so that we need to
		 * use this original mq status.
		 */
		if ((valid = ha_msg_value(msg, F_MQREQUEST)) == NULL) {
			cl_log(LOG_ERR, "%s: cannot read invalid bit"
			,	__FUNCTION__);
			return FALSE;
		}

		if (strncmp(valid, "invalid", 7) == 0)
			goto invalid;

		/*
		 * read msgfeedend
		 */
		if ((policy = cl_get_binary(msg, F_MQPOLICY, NULL))
			== NULL
		||	(cflag = cl_get_binary(msg, F_MQCREATEFLAG
			,	&cflag_size)) == NULL
		||	(oflag = cl_get_binary(msg, F_MQOPENFLAG
			,	&oflag_size)) == NULL
		||	(retention = cl_get_binary(msg, F_MQRETENTION
			,	&retention_size)) == NULL
		||	(size_string = ha_msg_value(msg, F_MQSIZE)) == NULL) {

			cl_log(LOG_ERR, "%s: cl_get_binary failed"
			,	__FUNCTION__);
		}

		/*
		 * create a new msg according to msgfeedend
		 */
		if ((new_msg = ha_msg_new(0)) == NULL) {
			cl_log(LOG_ERR, "%s: no memory", __FUNCTION__);
			return FALSE;
		}
		if (ha_msg_add(new_msg, F_TYPE
		,	mqname_type2string(MQNAME_TYPE_GRANTED)) == HA_FAIL
		||	ha_msg_add(new_msg, F_MQNAME, name) == HA_FAIL
		||	ha_msg_add(new_msg, F_MQREQUEST, s_request)
				== HA_FAIL
		||	ha_msg_addbin(new_msg, F_MQINVOCATION, s_invocation,
				s_invocation_size) == HA_FAIL
		||	ha_msg_add(new_msg, F_MQHOST, mq->host) == HA_FAIL
		||	ha_msg_addbin(new_msg, F_MQSTATUS, &sending_state,
				sizeof(SaMsgQueueSendingStateT)) == HA_FAIL
		||	ha_msg_addbin(new_msg, F_MQPOLICY, policy, sizeof(int))
				== HA_FAIL
		||	ha_msg_addbin(new_msg, F_MQCREATEFLAG, cflag,
				cflag_size) == HA_FAIL
		||	ha_msg_addbin(new_msg, F_MQOPENFLAG, oflag, oflag_size)
				== HA_FAIL
		||	ha_msg_addbin(new_msg, F_MQRETENTION, retention,
				retention_size) == HA_FAIL
		||	ha_msg_add(new_msg, F_MQSIZE, size_string) == HA_FAIL
		||	ha_msg_add(new_msg, F_MQERROR, s_error) == HA_FAIL) {

			cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
			ha_msg_del(new_msg);
			return FALSE;
		}

		ha_msg_del(saved_msg);
		saved_msg = new_msg;
		goto sendmsg;

invalid:
		/*
		 * If the open request set SA_MSG_QUEUE_OPEN_ONLY,
		 * deny this request, since retention timer expired.
		 */
		mq = g_hash_table_lookup(mq_open_pending_hash, name);
		if (!mq) {
			cl_log(LOG_ERR, "BUG: cannot find mq in pending hash");
			return TRUE;
		}
		if ((mq->status.creationFlags & SA_MSG_QUEUE_OPEN_ONLY)) {
			mqueue_request_t reply;

			cl_log(LOG_INFO, "retention timer expired and "
				"SA_MSG_QUEUE_OPEN_ONLY is set, reject!");

			reply.qname = ha_strdup(name);
			reply.gname = NULL;
			reply.request_type = cmsrequest_string2type(s_request);
			reply.invocation = *s_invocation;
			client_send_client_qopen(mq->client, &reply, -1
			,	SA_ERR_NOT_EXIST);
			g_hash_table_remove(mq_open_pending_hash, name);

			/*
			 * We get the change to unlink the mqueue here.
			 */
			request_mqname_unlink(name, cmsdata);

			ha_free(mq);
			ha_free(reply.qname);
			return TRUE;
		}

sendmsg:
		/*
		 * Send granted message to all.
		 */
		ha_msg_mod(saved_msg, F_TYPE
		,	mqname_type2string(MQNAME_TYPE_GRANTED));

		hb->llc_ops->sendclustermsg(hb, saved_msg);

		ha_msg_del(saved_msg);
		saved_msg = NULL;
		return TRUE;

	default:
		break;
	}

	return TRUE;
}

/**
 * process_mqname_denied - process the denied message from the master node
 *			   for this message queue name
 * @msg: received message from heartbeat IPC Channel
 */
int
process_mqname_denied(struct ha_msg *msg)
{
	const char * name, * error, * request;
	const int * invocation;
	size_t invocation_size;
	IPC_Channel *client = NULL;
	mqueue_request_t reply;
	mqueue_t * mq_pending;
	int flag;

	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL ||
		(error = ha_msg_value(msg, F_MQERROR)) == NULL ||
		(request = ha_msg_value(msg, F_MQREQUEST)) == NULL ||
		(invocation = cl_get_binary(msg, F_MQINVOCATION,
				&invocation_size)) == NULL ) {

		cl_log(LOG_ERR, "received NULL mq name or mq error reply");
		return FALSE;
	}

	flag = saerror_string2type(error);

	mq_pending = g_hash_table_lookup(mq_open_pending_hash, name);
	if (mq_pending != NULL) {
		/*
		 * we have clients open pending, send out reply
		 */
		client = mq_pending->client;
		cl_log(LOG_INFO, "%s: found client <%p>", __FUNCTION__, client);

		reply.qname = ha_strdup(name);
		reply.gname = NULL;
		reply.request_type = cmsrequest_string2type(request);
		reply.invocation = *invocation;

		client_send_client_qopen(client, &reply, -1, flag);
		ha_free(reply.qname);

		g_hash_table_remove(mq_open_pending_hash, name);
		ha_free(mq_pending->name);
		ha_free(mq_pending);
	}

	return TRUE;
}

static void
group_mem_dispatch(gpointer data, gpointer user_data)
{
	IPC_Message msg;
	client_mqgroup_notify_t * cmg = NULL;
	int size;
	client_mqgroup_track_t * track = (client_mqgroup_track_t *)data;
	notify_buffer_t * notify = (notify_buffer_t *)user_data;


	cmg = (client_mqgroup_notify_t *)
			ha_malloc(sizeof(client_mqgroup_notify_t));

	if (cmg == NULL) {
		cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
		return;
	}

	cmg->header.type = CMS_QUEUEGROUP_NOTIFY;
	cmg->header.name = notify->name;
	cmg->policy = notify->policy;
	cmg->group_name = notify->name;

	switch (track->flag) {

	case SA_TRACK_CHANGES:
		size = notify->number * sizeof(SaMsgQueueGroupNotificationT);
		msg.msg_len = sizeof(client_mqgroup_notify_t) + size;
		cmg->number = notify->number;
		cmg = realloc(cmg, msg.msg_len);
		cmg->data = (char *)cmg + sizeof(client_mqgroup_notify_t);
		memcpy(cmg->data, notify->change_buff, size);
		break;

	case SA_TRACK_CHANGES_ONLY:
		size = sizeof(SaMsgQueueGroupNotificationT);
		msg.msg_len = sizeof(client_mqgroup_notify_t) + size;
		cmg->number = 1;
		cmg = realloc(cmg, msg.msg_len);
		cmg->data = (char *)cmg + sizeof(client_mqgroup_notify_t);
		*(SaMsgQueueGroupNotificationT *)(cmg->data)
				= notify->changeonly_buff;
		break;

	default:
		cl_log(LOG_ERR, "Unknown track flag [%d]", track->flag);
		ha_free(cmg);
		return;
	}

	msg.msg_body = cmg;
	msg.msg_private = &msg;
	msg.msg_done = NULL;
	/* TODO: msg.msg_done to free memory here */

	dprintf("%s: Send Track information to my clients...\n", __FUNCTION__);
	track->ch->ops->send(track->ch, &msg);
}

int
process_mqgroup_insert(struct ha_msg *msg)
{
	const char *gname, *name;
	mqueue_t *mqg, *mq;
	notify_buffer_t buf;

	if ((gname = ha_msg_value(msg, F_MQGROUPNAME)) == NULL) {
		cl_log(LOG_ERR, "received NULL mq group name request");
		return FALSE;
	}
	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL) {
		cl_log(LOG_ERR, "received NULL mq name request");
		return FALSE;
	}

	/*
	 * Check carefully again here in case there are mess
	 * message in cluster.
	 */
	if ((mqg = mqname_lookup(gname, NULL)) == NULL) {
		cl_log(LOG_ERR
		,	"group name [%s] doesn't exist in local database!"
		,	gname);
		return FALSE;
	}
	if (mqg->policy == 0) {
		cl_log(LOG_ERR, "[%s] is a mq group name instead of a mq name"
		,	gname);
		return FALSE;
	}
	if ((mq = mqname_lookup(name, NULL)) == NULL) {
		cl_log(LOG_ERR
		,	"mq name [%s] doesn't exist in local database!"
		,	name);
		return FALSE;
	}
	
	/*
	 * The mqueue may already in the group, i.e this
	 * node is the master name node.
	 */
	if (g_list_find(mqg->list, mq) == NULL) {
		mqg->list = g_list_append(mqg->list, mq);
		cl_log(LOG_INFO, "Adding mq <%p> to [%s] list", mq, gname);
	}

	/*
	 * Update the mqueue list to point to append the group.
	 */
	if (g_list_find(mq->list, mqg) == NULL) {
		mq->list = g_list_append(mq->list, mqg);
		cl_log(LOG_INFO, "Adding mqg <%p> to [%s] list", mqg, name);
	}

	/*
	 * Current Round Robin counter set to the first list,
	 * we may want to set it as a random index to gain
	 * more load balance.
	 */
	if (mqg->current == NULL)
		mqg->current = g_list_first(mqg->list);

	/*
	 * Notify my clients who care about the group
	 * membership change message.
	 */
	if (mqg->notify_list != NULL) {
		strcpy(buf.changeonly_buff.member.queueName.value, name);
		buf.changeonly_buff.member.queueName.length = strlen(name) + 1;
		buf.changeonly_buff.member.queueStatus = mq->status;
		buf.changeonly_buff.change = SA_MSG_QUEUE_GROUP_ADDED;
		buf.policy = mqg->policy;
		buf.number = 0;
		strcpy(buf.name.value, gname);
		buf.name.length = strlen(gname) + 1;

		buf.change_buff = NULL;
		g_list_foreach(mqg->list, mqueue_copy_notify_data, &buf);
		g_list_foreach(mqg->notify_list, group_mem_dispatch, &buf);
	}

	return SA_OK;
}

int
process_mqgroup_remove(struct ha_msg *msg)
{
	const char *gname, *name;
	mqueue_t *mqg, *mq;
	notify_buffer_t buf;

	if ((gname = ha_msg_value(msg, F_MQGROUPNAME)) == NULL) {
		cl_log(LOG_ERR, "received NULL mq group name request");
		return FALSE;
	}
	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL) {
		cl_log(LOG_ERR, "received NULL mq name request");
		return FALSE;
	}

	/*
	 * Check carefully again here in case there are mess
	 * message in cluster.
	 */
	if ((mqg = mqname_lookup(gname, NULL)) == NULL) {
		cl_log(LOG_ERR
		,	"group name [%s] doesn't exist in local database!"
		,	gname);
		return FALSE;
	}
	if (!IS_MQGROUP(mqg)) {
		cl_log(LOG_ERR, "[%s] is a mq name instead of a mq group name"
		,	gname);
		return FALSE;
	}
	if ((mq = mqname_lookup(name, NULL)) == NULL) {
		cl_log(LOG_ERR
		,	"mq name [%s] doesn't exist in local database!"
		,	name);
		return FALSE;
	}
	
	/*
	 * mqueue may already be removed from the group, i.e
	 * this node is the master name node
	 */
	if (g_list_find(mqg->list, mq) != NULL)
		mqg->list = g_list_remove(mqg->list, mq);

	/*
	 * remove the mqgroup from the mqueue list as well
	 */
	if (g_list_find(mq->list, mqg) != NULL)
		mq->list = g_list_remove(mq->list, mqg);

	/*
	 * Notify my clients who care about the group
	 * membership change message.
	 */
	if (mqg->notify_list != NULL) {
		strcpy(buf.changeonly_buff.member.queueName.value, name);
		buf.changeonly_buff.member.queueName.length = strlen(name) + 1;
		buf.changeonly_buff.member.queueStatus = mq->status;
		buf.changeonly_buff.change = SA_MSG_QUEUE_GROUP_REMOVED;
		buf.policy = mqg->policy;
		buf.number = 0;
		strcpy(buf.name.value, gname);
		buf.name.length = strlen(gname) + 1;
	
		g_list_foreach(mqg->list, mqueue_copy_notify_data, &buf);
		g_list_foreach(mqg->notify_list, group_mem_dispatch, &buf);
	}

#if DEBUG_CLUSTER
	cl_log_message(msg);
#endif
	return SA_OK;
}

int
process_mqname_ack(struct ha_msg *msg)
{
	const char * qname, * error, * request;
	const SaInvocationT * invocation;
	size_t invocation_len, seq_len;
	const unsigned long * seq; 
	gpointer orig_seq, client;
	mqueue_request_t ack;
	gboolean found;

	if ((qname = ha_msg_value(msg, F_MQNAME)) == NULL
	     || (request = ha_msg_value(msg, F_MQREQUEST)) == NULL
	     || (error = ha_msg_value(msg, F_MQERROR)) == NULL
	     || (seq = cl_get_binary(msg, F_MQMSGSEQ, &seq_len)) == NULL
	     || (invocation = cl_get_binary(msg, F_MQINVOCATION, 
			     &invocation_len)) == NULL
	     ) {
		cl_log(LOG_ERR, "received NULL mq name request");
		return FALSE;
	}

	found = g_hash_table_lookup_extended(mq_ack_pending_hash, 
			seq, &orig_seq, &client);
	if (found) {
		/*
		 * we have clients waiting for acks, send out reply
		 */
		dprintf("%s: found client <%p>\n", __FUNCTION__, client);
		ack.qname = ha_strdup(qname);
		ack.request_type = cmsrequest_string2type(request);
		ack.invocation = *invocation;

		client_send_ack_msg((IPC_Channel *) client, 
				&ack, -1, saerror_string2type(error));
		ha_free(ack.qname);

		g_hash_table_remove(mq_open_pending_hash, seq); 
		ha_free((unsigned long *) orig_seq);
		ha_free((IPC_Channel *) client);
	} else {
		cl_log(LOG_ERR, "client is not found. "
			"nobody to send the ack to. mqname = %s", qname);
	}

	return TRUE;
}

int
process_mqname_update(struct ha_msg *msg, cms_data_t * cmsdata)
{
	const struct mq_info * info;
	size_t info_len;

	if ((info = cl_get_binary(msg, F_MQUPDATE, &info_len)) == NULL) {
		cl_log(LOG_ERR, "received NULL mq info update");
		return HA_FAIL;
	}

	/*
	 * turn on the ready bit and start accepting client request
	 */
	if (mqueue_table_unpack(info, info_len) == HA_OK) {
		cmsdata->cms_ready = 1;
		
	}

	return HA_OK;
}

int
request_mqueue_status(mqueue_t * mqueue, cms_data_t * cmsdata)
{
	struct ha_msg * msg;
	ll_cluster_t * hb;

	dprintf("in request status, used is [%d]\n"
	,	mqueue->status.saMsgQueueUsage[3].queueUsed);

	if ((msg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}

	if (ha_msg_add(msg, F_TYPE,
		mqname_type2string(MQNAME_TYPE_STATUS_REQUEST)) == HA_FAIL
	||	ha_msg_add(msg, F_MQNAME, mqueue->name) == HA_FAIL) {
		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;
	}

	hb = cmsdata->hb_handle;
	hb->llc_ops->sendnodemsg(hb, msg, mqueue->host);

	ha_msg_del(msg);
	return TRUE;
}


int
reply_mqueue_status(struct ha_msg *msg, cms_data_t * cmsdata)
{
	char usedstring[PACKSTRSIZE], numstring[PACKSTRSIZE];
	const char * name;
	mqueue_t * mq;
	struct ha_msg * m;
	ll_cluster_t * hb;
	const char * expire = "FALSE";

	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL) {
		cl_log(LOG_ERR, "%s: ha_msg_value failed", __FUNCTION__);
		return FALSE;
	}

	if ((mq = mqname_lookup(name, NULL)) == NULL) {
		cl_log(LOG_ERR, "%s: cannot find mqname [%s]"
		,	__FUNCTION__, name);
		return FALSE;
	}

	if (RETENTION_TIME_EXPIRES(mq)) {
		cl_log(LOG_WARNING, "%s: retention time expires [%s]"
		,	__FUNCTION__, name);
		expire = "TRUE";
		request_mqname_unlink(name, cmsdata);
	}

	sa_mqueue_usage_encode(NULL, usedstring, numstring
	,	mq->status.saMsgQueueUsage);

	dprintf("queueUsed [%s], numberOfMessages [%s]\n"
	,	usedstring, numstring);

	/*
	 * send reply to the request node
	 */
	if ((m = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: out of memory", __FUNCTION__);
		return FALSE;
	}

	if (ha_msg_add(m, F_TYPE, mqname_type2string(MQNAME_TYPE_STATUS_REPLY))
			== HA_FAIL
	||	ha_msg_add(m, F_MQNAME, name) == HA_FAIL
	||	ha_msg_add(m, F_MQEXPIRE, expire) == HA_FAIL
	||	ha_msg_add(m, F_MQUSED, usedstring) == HA_FAIL
	||	ha_msg_add(m, F_MQMSGNUM, numstring) == HA_FAIL) {
		cl_log(LOG_ERR, "%s: ha_msg_add failed", __FUNCTION__);
		return FALSE;
	}

	hb = cmsdata->hb_handle;
	hb->llc_ops->sendnodemsg(hb, m, mq->host);

	ha_msg_del(m);
	return TRUE;
}

int
process_mqueue_status(struct ha_msg *msg)
{
	const char *name, *expire = NULL, *usedstring = NULL, *numstring = NULL;
	SaErrorT ret = SA_OK;
	IPC_Channel * client;
	mqueue_t * mq;
	gpointer orig_key, value;

	if ((name = ha_msg_value(msg, F_MQNAME)) == NULL
	||	(expire = ha_msg_value(msg, F_MQEXPIRE)) == NULL
	||	(usedstring = ha_msg_value(msg, F_MQUSED)) == NULL
	||	(numstring = ha_msg_value(msg, F_MQMSGNUM)) == NULL) {

		cl_log(LOG_ERR, "%s: ha_msg_value failed", __FUNCTION__);
		ret = SA_ERR_LIBRARY;
	}

	if (!strncmp(expire, "TRUE", 4))
		ret = SA_ERR_NOT_EXIST;

	/* TODO: currently there can be only one client for a mqueue
	 * name in the hash table. But there might be more clients
	 * query the mqueue status at the same time. Need to make a
	 * GList for each mqueue name.
	 */
	if ((g_hash_table_lookup_extended(mq_status_pending_hash,
			name, &orig_key, &value)) == FALSE) {

		cl_log(LOG_ALERT, "%s: cannot find mqname [%s]"
		,	__FUNCTION__, name);
		return FALSE;
	}

	g_hash_table_remove(mq_status_pending_hash, name);

	client = value;

	/*
	 * update local mqueue hash table
	 */
	if ((mq = mqueue_table_lookup(name, NULL))) {
		sa_mqueue_usage_decode(NULL, usedstring, numstring
		,	mq->status.saMsgQueueUsage);
	}

	/*
	 * response to my client
	 */
	dprintf("%s: before respond to my client, ret = [%d]\n"
	,	__FUNCTION__, ret);

	if (ret != SA_OK)
		client_send_error_msg(client, name, CMS_QUEUE_STATUS, ret);
	else
		ret = client_send_qstatus(client, mq, SA_OK);

	ha_free(orig_key);
	return ret;
}

