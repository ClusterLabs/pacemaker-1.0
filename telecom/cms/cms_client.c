/*
 * cms_client.c: cms daemon client operation
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <clplumbing/cl_log.h>
#include <clplumbing/GSource.h>
#include <heartbeat.h>

#include "cms_common.h"

extern GHashTable * mq_open_pending_hash;
extern GHashTable * mq_status_pending_hash;
extern GHashTable * mq_ack_pending_hash;
extern GHashTable * cms_client_table;

static unsigned long gSendSeqNo = 0;

static gboolean
delete_cms_client(gpointer key, gpointer value, gpointer user_data)
{
	/* memory leak here? */
	return TRUE;
}

void
cms_client_input_destroy(gpointer user_data)
{
	dprintf("%s: received HUP.\n", __FUNCTION__);
	return;
}

int
cms_client_init(cms_data_t * cmsdata)
{
	IPC_WaitConnection * wait_ch = NULL;
	mode_t mask;
	GHashTable * attrs;

	char path[] = IPC_PATH_ATTR;
	char domainsocket[] = IPC_DOMAIN_SOCKET;
	char cms_socket[] = CMS_DOMAIN_SOCKET;

	cl_log(LOG_INFO, "initialize client tables and wait channel.");


	attrs = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(attrs, path, cms_socket);

	mask = umask(0);
	wait_ch = ipc_wait_conn_constructor(domainsocket, attrs);
	if (!wait_ch) {
		cl_perror("Can't create wait channel");
		return HA_FAIL;
	}
	mask = umask(mask);

	g_hash_table_destroy(attrs);

	cmsdata->wait_ch = wait_ch;
	cmsdata->client_table = g_hash_table_new(g_int_hash, g_int_equal);

	return HA_OK;
}

int
cms_client_add(GHashTable ** cms_client_table, struct IPC_CHANNEL * newclient) 
{
	cms_client_t * cms_client;
	pid_t * key;

	if (!cms_client_table) {
		cl_log(LOG_ERR, "cms: can't find client table"); 
		return HA_FAIL;
	}

	cms_client = g_hash_table_lookup(*cms_client_table,
			&(newclient->farside_pid));

	if (cms_client) {
		cms_client->channel_count++;
		dprintf("farside_pid [%d] already exists, channel_count [%d]\n"
		,	newclient->farside_pid, cms_client->channel_count);
		return HA_OK;
	}

	cms_client = (cms_client_t *) ha_malloc(sizeof(cms_client_t));
	key = (pid_t *)ha_malloc(sizeof(pid_t));
	if (!key || !cms_client) {
		cl_log(LOG_CRIT, "malloc key failiure for client add.");
		return HA_FAIL;
	}

	dprintf("Add farside_pid [%d] to daemon <%p>\n"
	,	newclient->farside_pid, *cms_client_table);
	cms_client->channel_count = 1;
	cms_client->opened_mqueue_list = NULL;

	*key = newclient->farside_pid;
	g_hash_table_insert(*cms_client_table, key, cms_client);

	return HA_OK;
}

void
cms_client_close_all(GHashTable * cms_client_table)
{
	dprintf("In Func %s ...\n", __FUNCTION__);

	if (g_hash_table_size(cms_client_table)) {
		g_hash_table_foreach_remove(cms_client_table, 
				delete_cms_client, NULL);
	}
	return;
}

int
client_process_qstatus(IPC_Channel * client, client_header_t *
msg, cms_data_t * cmsdata)
{
	client_mqueue_status_t * qstatus_msg;
	mqueue_t * queue;
	SaErrorT error;
	client_header_t reply;
	char * mqname;

	qstatus_msg = (client_mqueue_status_t *) msg;

	mqname = saname2str(qstatus_msg->header.name);

	/* get the queue/host mapping 
	 * 
	 * this function should be deterministic based 
	 * only on the queue name and the list of cluster
	 * membership available.  
	 */
	if ((queue = mqname_lookup(mqname, NULL)) == NULL) {
		error = SA_ERR_NOT_EXIST;

		reply.type = msg->type;
		reply.len = sizeof(client_header_t);
		reply.flag = error;
		reply.name = msg->name;

		client_send_msg(client, reply.len, &reply);
		return TRUE;
	};

	/* Get queueUsed and numberOfMessages for saMsgQueueUsage[4]
	 * from mqueue open node.
	 */
	g_hash_table_insert(mq_status_pending_hash, mqname, client);
	request_mqueue_status(queue, cmsdata);

	return TRUE;
}

int
client_process_mqopen(IPC_Channel * client, client_header_t * msg,
		      cms_data_t * cmsdata)
{
	mqueue_request_t request;
	IPC_Channel *cli;
	client_mqueue_open_t * m = (client_mqueue_open_t *) msg;
	mqueue_t * mq;
	char * mqname;
	int i;
	SaErrorT error = SA_OK;

	mqname = saname2str(m->header.name);

	if (g_hash_table_lookup(mq_open_pending_hash, mqname)) {

		cl_log(LOG_WARNING, "%s: mqname [%s] open pending from local"
		,	__FUNCTION__, mqname);

		request.qname = mqname;
		request.gname = NULL;
		request.request_type = m->header.type;
		request.invocation = 0;

		client_send_client_qopen(client, &request, -1, SA_ERR_EXIST);

		ha_free(mqname);
		return TRUE;
	}

	/* Search from local database firstly, if local database checking
	 * fails, we don't continue.
	 *
	 * Don't worry about retention time here, the original mqueue
	 * owner will deal with it when receiving REOPEN from hb.
	 */
	if ((mq = mqname_lookup(mqname, NULL)) != NULL) {

		if (mq->mqstat != MQ_STATUS_CLOSE)
			error = SA_ERR_EXIST;

		/* Client provide a creationAttributes, but it is 
		 * different from what we already have.
		 */
		else if ((m->attr.creationFlags != -1)
		&&	(mq->status.openFlags != m->openflag
		||	mq->status.creationFlags != m->attr.creationFlags
		||	mq->status.retentionTime != m->attr.retentionTime))
			error = SA_ERR_EXIST;
	}

	if (!mq && (m->header.type != CMS_QUEUEGROUP_CREATE)
	&&	(m->openflag & SA_MSG_QUEUE_OPEN_ONLY)) {

		error = SA_ERR_NOT_EXIST;
		cl_log(LOG_INFO, "%s: SA_MSG_QUEUE_OPEN_ONLY provided for %s"
		,	__FUNCTION__, mqname);
	}

	if (error == SA_OK)
		goto proceed;


	request.qname = mqname;
	request.gname = NULL;
	request.request_type = m->header.type;
	request.invocation = 0;

	client_send_client_qopen(client, &request, -1, error);

	ha_free(mqname);
	return TRUE;

proceed:
	if ((cli = (IPC_Channel *) ha_malloc(sizeof(IPC_Channel))) == NULL) {
		cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
		ha_free(mqname);
		return FALSE;
	}
	*cli = *client;

	mq = (mqueue_t *) ha_malloc(sizeof(mqueue_t));
	if (!mq) {
		cl_log(LOG_ERR, "%s: ha_malloc for mqueue_t failed.\n"
		,	__FUNCTION__);
		ha_free(cli);
		ha_free(mqname);
		return FALSE;
	}
	memset(mq, 0, sizeof(mqueue_t));
	mq->name = mqname;
	mq->policy = m->policy;
	mq->mqstat = MQ_STATUS_OPEN_PENDING;
	mq->client = cli;

	g_hash_table_insert(mq_open_pending_hash, mq->name, mq);

	request.qname = mqname;
	request.gname = NULL;
	request.request_type = m->header.type;
	request.invocation = m->invocation;
	request.policy = m->policy;
	request.create_flag = m->attr.creationFlags;
	request.retention = m->attr.retentionTime;
	request.ack = 1; 

	for (i = SA_MSG_MESSAGE_HIGHEST_PRIORITY
	;	i <= SA_MSG_MESSAGE_LOWEST_PRIORITY; i++)
		request.size[i] = m->attr.size[i];

	cl_log(LOG_INFO, "%s: invocation = %d, policy = %d, type = %d"
	,	__FUNCTION__, request.invocation, request.policy
	,	request.request_type);

	if (request_mqname_open(&request, cmsdata) == FALSE) {
		cl_log(LOG_ERR, "%s: cluster_request_mqname failed"
		,	__FUNCTION__);

		g_hash_table_remove(mq_open_pending_hash, mqname);
		ha_free(cli);
		ha_free(mq);
		ha_free(mqname);

		return FALSE;
	}

	return TRUE;
}

int
client_process_mqclose(IPC_Channel * client, client_header_t * msg,
		       cms_data_t * cmsdata)
{
	client_mqueue_close_t * m = (client_mqueue_close_t *) msg;
	mqueue_t *mq;
	client_header_t reply;
	cms_client_t * cms_client;

	CMS_TRACE();

	mq = mqueue_handle_lookup(&(m->handle), NULL);

	if (mq == NULL) {
		reply.type = msg->type;
		reply.len = sizeof(client_header_t);
		reply.flag = SA_ERR_NOT_EXIST;
		reply.name = msg->name;

		client_send_msg(client, reply.len, &reply);

		cl_log(LOG_WARNING, "%s: Cannot find mq by handle [%u]"
		,	__FUNCTION__, m->handle);
		return TRUE;
	}

	mq->status.closeTime = get_current_satime();

	/*
	 * close the mqueue in the cluster
	 */
	if (request_mqname_close(mq->name, cmsdata) == FALSE) {
		cl_log(LOG_ERR, "%s: mqname_close failed", __FUNCTION__);
		return FALSE;
	}
	mqueue_handle_remove(&(m->handle));

	/*
	 * remove this mq from client's opened_mqueue_list
	 */
	cms_client = g_hash_table_lookup(cmsdata->client_table,
				&client->farside_pid);

	assert(cms_client != NULL);

	cms_client->opened_mqueue_list =
		g_list_remove(cms_client->opened_mqueue_list, mq);


	if (m->silent)
		return TRUE;

	reply.type = CMS_QUEUE_CLOSE;
	reply.len = sizeof(client_header_t);
	reply.flag = SA_OK;
	str2saname(&reply.name, mq->name);

	client_send_msg(client, reply.len, &reply);	
	return TRUE;
}

int
client_process_mqunlink(IPC_Channel * client, client_header_t * msg,
			cms_data_t * cmsdata)
{
	mqueue_t *mq;
	char * mqname;
	client_mqueue_unlink_t * m = (client_mqueue_unlink_t *) msg;
	client_header_t reply;
	cms_client_t * cms_client;

	mqname = saname2str(m->header.name);
	dprintf("%s: mqname=[%s]\n", __FUNCTION__, mqname);

	mq = mqueue_table_lookup(mqname, NULL);

	if (mq == NULL) {
		reply.type = msg->type;
		reply.len = sizeof(client_header_t);
		reply.flag = SA_ERR_NOT_EXIST;
		reply.name = msg->name;

		cl_log(LOG_WARNING, "%s: Cannot find mq by handle [%u]"
		,	__FUNCTION__, m->handle);

		client_send_msg(client, reply.len, &reply); 

		return TRUE;
	}

	if (mq->list != NULL) {
		/*
		 * Remove the reference from my message queue
		 */
	}

	reply.type = CMS_QUEUE_UNLINK;
	reply.len = sizeof(client_header_t);
	reply.flag = SA_OK;
	str2saname(&reply.name, mq->name);

	if (request_mqname_unlink(mqname, cmsdata) == FALSE) {
		cl_log(LOG_ERR, "%s: mqname_unlink failed", __FUNCTION__);
		ha_free(mqname);
		return FALSE;
	}
	mqueue_handle_remove(&(m->handle));

	client_send_msg(client, reply.len, &reply);	

	/*
	 * remove this mq from client's opened_mqueue_list
	 */
	cms_client = g_hash_table_lookup(cmsdata->client_table,
				&client->farside_pid);

	assert(cms_client != NULL);

	cms_client->opened_mqueue_list =
		g_list_remove(cms_client->opened_mqueue_list, mq);


	ha_free(mqname);
	return TRUE;
}

int
client_process_mqsend(IPC_Channel * client, client_header_t * msg,
		      cms_data_t * cmsdata)
{
	mqueue_t *mq;
	char * mqname;
	mqueue_request_t request;
	IPC_Channel *cli = NULL; 
	unsigned long * seq = NULL;
	client_message_t * m = (client_message_t *) msg;
	client_header_t reply;
	SaErrorT error;

	mqname = saname2str(m->header.name);
	request.qname = mqname;
	request.gname = NULL;
	request.request_type = m->header.type;
	dprintf("request.request_type is %d\n", request.request_type);
	request.invocation = m->invocation;
	request.ack = m->ack;
	request.seq = gSendSeqNo++;

	dprintf("%s: mqname = %s\n", __FUNCTION__, mqname);
	m->data = (void *)((char *)msg + sizeof(client_message_t));
	m->msg.data = m->data;

	mq = mqname_lookup(mqname, NULL);

	if (mq == NULL) {

		cl_log(LOG_WARNING, "%s: Cannot find mq by name [%s]"
		,	__FUNCTION__, mqname);

		error = SA_ERR_NOT_EXIST;
		goto error;
	}

	/*
	 * for message queue group
	 */
 	if (IS_MQGROUP(mq)) {
 		mqueue_t *mqg = mq;
		char *mqueue_name = NULL;

		request.gname = mqg->name;
 
 		cl_log(LOG_INFO, "[%s] is a [Type %d] message queue group"
 		,	mqname, mqg->policy);
 
 		cl_log(LOG_INFO, "MQ Group [%s] current is <%p>"
 		,	mqname, mqg->current->data);
 
 		if (!(mqg->current)
 		||  !(mqueue_name = ((mqueue_t *)(mqg->current->data))->name)
 		||  !(mq = mqueue_table_lookup(mqueue_name, NULL))) {
 
			cl_log(LOG_ERR, "%s: Cannot find group current [%s]\n"
 			,	__FUNCTION__, mqueue_name);

			error = SA_ERR_NOT_EXIST;
			goto error;
 		}

		dump_mqueue_list(mqg);

 		mqg->current = CIRCLE_LIST_NEXT(mqg->list, mqg->current);

		request.qname = mqueue_name;
		request.gname = mqname;
 	}

	if (request.ack) {

		dprintf("%s: insert ack packet\n", __FUNCTION__);

		if ((cli = (IPC_Channel *) ha_malloc(sizeof(IPC_Channel))) 
				== NULL 
		||  (seq = (unsigned long *) ha_malloc(sizeof(unsigned long))) 
				== NULL ) {
			cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
			if (cli)
				ha_free(cli);

			error = SA_ERR_NO_MEMORY;
			goto error;
		}
		*cli = *client;
		*seq = request.seq;

		g_hash_table_insert(mq_ack_pending_hash, seq, cli);
	} 

	if (request_mqname_send(&request, mq->host, NULL, &(m->msg)
	,	cmsdata) == FALSE) {

		cl_log(LOG_ERR, "%s: mqname_send failed", __FUNCTION__);
		error = SA_ERR_LIBRARY;
		goto error;
	}

	ha_free(mqname);
	return TRUE;

error:
	/* This is actually a error respond instead of a ACK. */
	reply.type = CMS_MSG_SEND;
	reply.len = sizeof(client_header_t);
	reply.flag = error;
	reply.name = msg->name;
	client_send_msg(client, reply.len, &reply);

	ha_free(mqname);
	return TRUE;
}

int
client_process_message_request(IPC_Channel * client, client_header_t * msg)
{
	char * mqname;
	mqueue_t * mq;
	SaMsgMessageT * message;
	client_message_t * m;

	mqname = saname2str(msg->name);
	dprintf("%s: mqname is %s\n", __FUNCTION__, mqname);

	if ((mq = mqueue_table_lookup(mqname, NULL)) == NULL) {
		cl_log(LOG_ERR, "%s: cannot find mq [%s]", __FUNCTION__,mqname);
		ha_free(mqname);
		return FALSE;
	}

	if ((message = dequeue_message(mq)) == NULL) {
		cl_log(LOG_ERR, "%s: No message found for mq [%s], block"
		,	__FUNCTION__, mqname);
		ha_free(mqname);
		return TRUE;
	}

	dprintf("%s: dequeue_message [%s]\n", __FUNCTION__
	,	(char *)message->data);

	m = (client_message_t *)
			ha_malloc(sizeof(client_message_t) + message->size);
	if (!m) {
		cl_log(LOG_CRIT, "malloc failed for client message request.");
		return FALSE;
	}

	m->header.type = CMS_MSG_GET;
        m->header.len = sizeof(client_message_t) + message->size;
        m->header.flag = SA_OK;
	m->header.name = msg->name;
        m->handle = mq->handle;
        m->msg = *message;
        m->msg.data = NULL;
        m->data = m + 1;
        memcpy(m->data, message->data, message->size);

	client_send_msg(client, m->header.len, m);

	ha_free(message);
	ha_free(mqname);
	ha_free(m);
	return TRUE;
}

int
client_process_mqgroup_insert(IPC_Channel * client, client_header_t * msg,
			      cms_data_t * cmsdata)
{
	mqueue_t *mq, *mqg;
	char *mqname, *mqgname;
	client_mqgroup_ops_t * m = (client_mqgroup_ops_t *)msg;
	client_header_t reply;

	mqname = saname2str(m->header.name);
	mqgname = saname2str(m->qgname);

	reply.type = msg->type;
	reply.len = sizeof(client_header_t);
	reply.flag = SA_OK;
	reply.name = msg->name;

	mqg = mqueue_table_lookup(mqgname, NULL);
	if (mqg == NULL)
		goto bad_queue;

	if (mqg->policy == 0)	/* not a queue group */
		goto bad_queue;

	mq = mqueue_table_lookup(mqname, NULL);
	if (mq == NULL)
		goto bad_queue;

	/*
	 * check if the queue is already in the queue group
	 */
	if (g_list_find(mqg->list, mq) == NULL) {
		mqg->list = g_list_append(mqg->list, mq);
		cl_log(LOG_INFO, "Adding mq <%p> to [%s] list", mq, mqgname);
 
		if (mqg->current == NULL)
			mqg->current = g_list_first(mqg->list);

		/*
		 * Notify other nodes the change
		 */
		request_mqgroup_insert(mqgname, mqname, cmsdata);
	} else {
		reply.flag =  SA_ERR_EXIST;
		client_send_msg(client, reply.len, &reply);
		goto exit;
	}

	client_send_msg(client, reply.len, &reply);
	goto exit;

bad_queue:
	reply.flag = SA_ERR_NOT_EXIST;
	client_send_msg(client, reply.len, &reply);

exit:
	ha_free(mqname);
	ha_free(mqgname);
	return TRUE;
}

int
client_process_mqgroup_remove(IPC_Channel * client, client_header_t * msg,
			      cms_data_t * cmsdata)
{
	mqueue_t *mq, *mqg;
	char *mqname, *mqgname;
	client_mqgroup_ops_t * m = (client_mqgroup_ops_t *)msg;
	client_header_t reply;

	mqname = saname2str(m->header.name);
	mqgname = saname2str(m->qgname);

	reply.type = msg->type;
	reply.len = sizeof(client_header_t);
	reply.flag = SA_OK;
	reply.name = msg->name;

	mqg = mqueue_table_lookup(mqgname, NULL);
	if (mqg == NULL)
		goto error;

	if (!IS_MQGROUP(mqg))	/* not a queue group */
		goto error;

	mq = mqname_lookup(mqname, NULL);
	if (mq == NULL)
		goto error;

	/*
	 * check if the queue is already removed from the queue group
	 */
	if (g_list_find(mqg->list, mq) != NULL) {

		mqgroup_unref_mqueue(mqg, mq);

		/*
		 * Notify other nodes the change
		 */
		request_mqgroup_remove(mqgname, mqname, cmsdata);

	} else
		goto error;


	client_send_msg(client, reply.len, &reply);
	goto exit;

error:
	reply.flag = SA_ERR_NOT_EXIST;
	client_send_msg(client, reply.len, &reply);

exit:
	ha_free(mqname);
	ha_free(mqgname);
	return TRUE;
}

int
client_process_mqgroup_track(IPC_Channel * client, client_header_t * msg)
{
	mqueue_t *mqg;
	char * gname;
	int group;
	client_mqgroup_mem_t * m = (client_mqgroup_mem_t *)msg;
	client_mqgroup_notify_t * rmsg;

	rmsg = (client_mqgroup_notify_t *)
			malloc(sizeof(client_mqgroup_notify_t));
	if (!rmsg) {
		cl_log(LOG_CRIT, "malloc rmsg failed for mqgroup_track.");
		return FALSE;
	}

	rmsg->header.type = msg->type;
	rmsg->header.len = sizeof(client_mqgroup_notify_t);
	rmsg->header.flag = SA_OK;
	rmsg->header.name = msg->name;

	gname = saname2str(m->group_name);
	mqg = mqueue_table_lookup(gname, &group);

	if (mqg == NULL || group == FALSE)
		goto noexist;

	if ((m->flag & SA_TRACK_CHANGES)
	||      (m->flag & SA_TRACK_CHANGES_ONLY)) {

		client_mqgroup_track_t * track;

		track = (client_mqgroup_track_t *)
				ha_malloc(sizeof(client_mqgroup_track_t));

		if (track == NULL) {
			cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
			rmsg->header.flag = SA_ERR_NO_MEMORY;

			client_send_msg(client, rmsg->header.len
			,	(client_header_t *)rmsg);
		
			ha_free(rmsg);
			return TRUE;
		}
		track->ch = client;
		track->flag = m->flag;
		
		cl_log(LOG_INFO, "%s: append ch to [%s] notify_list,flag = <%d>"
		,	__FUNCTION__, gname, track->flag);
		mqg->notify_list = g_list_append(mqg->notify_list, track);
	}

	if (m->flag & SA_TRACK_CURRENT) {
		notify_buffer_t buf;
		size_t length;

		buf.number = 0;
		buf.change_buff = NULL;
		g_list_foreach(mqg->list, mqueue_copy_notify_data, &buf);
		length = buf.number * sizeof(SaMsgQueueGroupNotificationT);
		rmsg = realloc(rmsg, sizeof(client_mqgroup_notify_t) + length);
		rmsg->data = (char *)rmsg + sizeof(client_mqgroup_notify_t);

		memcpy(rmsg->data, buf.change_buff, length);
		ha_free(buf.change_buff);
		rmsg->number = buf.number;
		rmsg->policy = buf.policy;
		rmsg->group_name = buf.name;

		rmsg->header.len += length;
	}
		
	client_send_msg(client, rmsg->header.len, (client_header_t *)rmsg);

	free(rmsg);
	goto exit;

noexist:
	rmsg->header.flag = SA_ERR_NOT_EXIST;
	client_send_msg(client, rmsg->header.len, (client_header_t *)rmsg);

exit:
	ha_free(gname);
	ha_free(rmsg);
	return TRUE;
}

static gint
compare_client(gconstpointer a, gconstpointer b)
{
	const client_mqgroup_track_t * c;
	const IPC_Channel * d;

	c = (const client_mqgroup_track_t *)a;
	d = (const IPC_Channel *)b;

	if (c->ch == d)
		return TRUE;
	else
		return FALSE;
}

int
client_process_mqgroup_track_stop(IPC_Channel * client, client_header_t * msg)
{
	mqueue_t *mqg;
	char * gname;
	int group;
	client_mqgroup_track_t * track;
	client_mqgroup_mem_t * m = (client_mqgroup_mem_t *)msg;
	client_header_t reply;

	reply.type = msg->type;
	reply.len = sizeof(client_header_t);
	reply.flag = SA_OK;
	reply.name = msg->name;

	gname = saname2str(m->group_name);
	mqg = mqueue_table_lookup(gname, &group);
	if (mqg == NULL || group == FALSE)
		goto noexist;

	track = (client_mqgroup_track_t *)g_list_find_custom(mqg->notify_list,
				client, compare_client);

	if (track != NULL) {
		mqg->notify_list = g_list_remove(mqg->notify_list, track);
		ha_free(track);
	}

	client_send_msg(client, reply.len, &reply);
	goto exit;

noexist:
	reply.flag = SA_ERR_NOT_EXIST;
	client_send_msg(client, reply.len, &reply);

exit:
	ha_free(gname);
	return TRUE;
}

static void
cms_client_msg_done(IPC_Message * msg)
{
	client_header_t * message;
	size_t msg_type;
	char * mqname;
	/* mqueue_t * mq; */
	/* client_message_t * m = (client_message_t *) message; */

	message = msg->msg_body;
	msg_type = message->type;

	dprintf("cms_client_msg_done, type = %d\n", (int)msg_type);
	mqname = saname2str(message->name);

#if 0
	/* update the buffer size */
	if (msg_type == CMS_MSG_SEND || msg_type == CMS_MSG_SEND_ASYNC)
		if ((mq = mqname_lookup(mqname, NULL)))
			mqueue_update_usage(mq, m->msg.priority, -m->msg.size);
#endif

	ha_free(msg->msg_private);
	ha_free(mqname);
	return;
}

int
client_send_msg(IPC_Channel * client, size_t len, gpointer data)
{
	int ret;
	IPC_Message * msg;

	CMS_TRACE();

	if ((msg = ha_malloc(sizeof(IPC_Message) + len)) == NULL) {
		cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
		return FALSE;
	}
	
	memset(msg, 0, sizeof(IPC_Message) + len);
	
#if DEBUG_MEMORY
	dprintf("%s (%p) ha_malloc %p, size 0x%x\n", __FUNCTION__
	,	&client_send_msg, msg, sizeof(IPC_Message) + len);
#endif
	msg->msg_body = msg + 1;
	memcpy(msg->msg_body, data, len);
	msg->msg_len = len;
	msg->msg_done = cms_client_msg_done;
	msg->msg_private = msg;
	msg->msg_ch = client;

	ret = client->ops->send(client, msg);

	if (ret == IPC_OK) 
		return TRUE;
	else 
		return FALSE;
}

int
client_send_error_msg(IPC_Channel * client, const char * name,
		      size_t type, SaErrorT error)
{
	client_header_t msg;

	msg.type = type;
	msg.len = sizeof(client_header_t);
	msg.flag = error;
	str2saname(&msg.name, name);

	return client_send_msg(client, msg.len, &msg);
}

int
client_send_qstatus(IPC_Channel * client, mqueue_t * queue, int flag)
{
	client_mqueue_status_t qstatus;

	memset(&qstatus, 0, sizeof(client_mqueue_status_t));

	qstatus.header.type = CMS_QUEUE_STATUS;
	qstatus.header.len = sizeof(client_mqueue_status_t);
	qstatus.header.flag = flag;
	str2saname(&qstatus.header.name, queue->name);

	qstatus.qstatus = queue->status;

	return client_send_msg(client, qstatus.header.len, &qstatus);
}

int
client_send_client_qopen(IPC_Channel * client, mqueue_request_t * request, 
			 guint handle, int flag)
{
	client_mqueue_open_t qopen;

	CMS_TRACE();

	if (!request)
		return HA_FAIL;

	memset(&qopen, 0, sizeof(client_mqueue_open_t));

	qopen.header.type = request->request_type;
	qopen.header.len = sizeof(client_mqueue_open_t);
	qopen.header.flag = flag;
	str2saname(&qopen.header.name, request->qname);

	qopen.handle = handle;
	qopen.invocation = request->invocation;

	return client_send_msg(client, qopen.header.len, &qopen);
}

int
client_send_ack_msg(IPC_Channel * client, mqueue_request_t * request, 
		    guint handle, int flag)
{
	client_message_ack_t ack;

	if (!request)
		return HA_FAIL;

	memset(&ack, 0, sizeof(client_message_ack_t));

	ack.header.type = CMS_MSG_ACK;
	ack.header.len = sizeof(client_message_ack_t);
	ack.header.flag = flag;
	str2saname(&ack.header.name, request->qname);

	ack.handle = handle;
	ack.invocation = request->invocation;
	ack.send_type = request->request_type;

	return client_send_msg(client, ack.header.len, &ack);
}

int 
client_send_notready_msg(IPC_Channel * client, client_header_t * msg)
{
	client_header_t reply;

	reply.type = msg->type;
	reply.len = sizeof(client_header_t);
	reply.name = msg->name;
	reply.flag = SA_ERR_TRY_AGAIN;

	cl_log(LOG_ERR, "CMS is still waiting to receive message queue "
			"updates.  Please try again later.");

	return client_send_msg(client, reply.len, &reply);
}
