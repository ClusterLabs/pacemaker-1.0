/*
 * cmslib_client.c: SAForum AIS Message Service client library
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 * Author: Zhu Yi (yi.zhu@intel.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	/* dup, dup2 */
#include <string.h>
#include <assert.h>

#include <clplumbing/realtime.h>
#include <cl_log.h>
#include <heartbeat.h>
#include <saf/ais.h>

#include "cmslib_client.h"
#include "cms_client_types.h"

#define PIPETRICK_DEBUG	0

#ifdef DEBUG_LIBRARY
#define dprintf(arg...)		fprintf(stderr, ##arg)
#else
#define dprintf(arg...)		{}
#endif

#define GET_CMS_HANDLE(x) ((x == NULL) ? NULL : \
			   (__cms_handle_t *)g_hash_table_lookup( \
			   __cmshandle_hash, x))

#define GET_MQ_HANDLE(x)  ((x == NULL) ? NULL : \
			   (__cms_handle_t *)g_hash_table_lookup( \
			   __mqhandle_hash, x))

static GHashTable * __cmshandle_hash;
static GHashTable * __mqhandle_hash;
static GHashTable * __group_tracking_hash;
static guint __cmshandle_counter = 0;
static gboolean __cmsclient_init_flag = FALSE;
static gboolean __notify_acked = TRUE;

void cmsclient_hash_init(void);
IPC_Channel *cms_channel_conn(void);
int enqueue_dispatch_msg(__cms_handle_t * hd, client_header_t * msg);
client_header_t * dequeue_dispatch_msg(GList ** queue);
int read_and_queue_ipc_msg(__cms_handle_t * handle);
int dispatch_msg(__cms_handle_t * handle, client_header_t * msg);
int wait_for_msg(__cms_handle_t * handle, size_t msgtype,
		 const SaNameT * name, client_header_t ** msg, SaTimeT timeout);
int get_timeout_value(SaTimeT timeout, struct timeval * tv);


static int
saname_cmp(const SaNameT s1, const SaNameT s2)
{
	SaUint16T len1, len2;

	/* dprintf("Length of s1: %d, s2: %d\n", s1.length, s2.length); */
	len1 = s1.value[s1.length - 1] ? s1.length : s1.length - 1;
	len2 = s2.value[s2.length - 1] ? s2.length : s2.length - 1;

	if (len1 != len2)
		return len2 - len1;

	return strncmp(s1.value, s2.value, len1);
}

static int
bad_saname(const SaNameT * name)
{
	int i;

	if (!name || name->length <= 0
	||	name->length > SA_MAX_NAME_LENGTH - 1)
		return TRUE;

	/*
	 * We don't support '\0' inside a SaNameT.value.
	 */
	for (i = 0; i < name->length; i++)
		if (name->value[i] == '\0')
			return TRUE;

	return FALSE;
}

static char *
saname2str(SaNameT name)
{
	char * str;
	
	if (name.length <= 0)
		return NULL;
	
	if (name.length > SA_MAX_NAME_LENGTH - 1)
		name.length = SA_MAX_NAME_LENGTH - 1;
	
	if ((str = (char *)ha_malloc(name.length + 1)) == NULL)
		return NULL;
	
	strncpy(str, name.value, name.length);
	str[name.length] = '\0';
	
	return str;
}

static int
active_poll(__cms_handle_t * hd)
{
	int fd;

	if (hd->backup_fd >= 0) {
		cl_log(LOG_WARNING, "%s: recursion detected", __FUNCTION__);
		return 1;
	}
	if ((fd = hd->ch->ops->get_recv_select_fd(hd->ch)) < 0) {
		cl_log(LOG_ERR, "%s: get_recv_select_fd failed", __FUNCTION__);
		return 1;
	}
	if ((hd->backup_fd = dup(fd)) == -1) {
		cl_log(LOG_ERR, "%s: dup2 failed", __FUNCTION__);
		perror("dup2");
		return 1;
	}
	close(fd);
	if (dup2(hd->active_fd, fd) == -1) {
		cl_log(LOG_ERR, "%s: dup2 failed", __FUNCTION__);
		perror("dup2");
		return 1;
	}

#if PIPETRICK_DEBUG
	dprintf("acitve_poll for <%p>\n", hd);
#endif
	return 0;
}

static int
restore_poll(__cms_handle_t * hd)
{
	int fd;

	if (hd->backup_fd < 0) {
		cl_log(LOG_WARNING, "%s: recursion detected", __FUNCTION__);
		return 1;
	}
	if ((fd = hd->ch->ops->get_recv_select_fd(hd->ch)) < 0) {
		cl_log(LOG_ERR, "%s: get_recv_select_fd failed", __FUNCTION__);
		return 1;
	}
	if (dup2(hd->backup_fd, fd) == -1) {
		cl_log(LOG_ERR, "%s: dup2 failed", __FUNCTION__);
		return 1;
	}

	hd->backup_fd = -1;	/* mark as unused */
	
#if PIPETRICK_DEBUG
	dprintf("restore_poll for <%p>\n", hd);
#endif
	return 0;
}

static int
cmsclient_message_recv(__cms_handle_t * hd, client_header_t ** data)
{
	int ret;
	IPC_Message * ipc_msg;

	if (hd->backup_fd >= 0)
		restore_poll(hd);

	ret = hd->ch->ops->recv(hd->ch, &ipc_msg);

	if (ret != IPC_OK)
		return ret;

	*data = ha_malloc(ipc_msg->msg_len);
	memcpy(*data, ipc_msg->msg_body, ipc_msg->msg_len);

	ipc_msg->msg_done(ipc_msg);

	return ret;
}

static void
cmsclient_message_done(IPC_Message * msg)
{
	char * name;
	client_header_t * message;

	message = msg->msg_body;
	name = saname2str(message->name);
	ha_free(msg->msg_private);
	ha_free(name);
}

static int
cmsclient_message_send(__cms_handle_t * hd, size_t len, gpointer data)
{
	IPC_Message * msg;

	if ((msg = ha_malloc(sizeof(IPC_Message) + len)) == NULL) {
		cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
		return FALSE;
	}

	if (hd->backup_fd >= 0)
		restore_poll(hd);

	msg->msg_body = msg + 1;
	memcpy(msg->msg_body, data, len);
	msg->msg_len = len;
	msg->msg_private = msg;
	msg->msg_done = cmsclient_message_done;
	msg->msg_buf = 0;

	return hd->ch->ops->send(hd->ch, msg);
}

static gboolean
msgqueue_remove(gpointer key, gpointer value, gpointer user_data)
{
	__cms_queue_handle_t * qhd = (__cms_queue_handle_t *) value;
	client_mqueue_close_t cmg;
	SaNameT * qname;

	CMS_LIBRARY_TRACE();

	qname = &(qhd->queue_name);

	cmg.header.type = CMS_QUEUE_CLOSE;
	cmg.header.name = *qname;
	cmg.handle = qhd->queue_handle;
	cmg.silent = TRUE;

	cmsclient_message_send(qhd->cms_handle, sizeof(cmg), &cmg);

	g_hash_table_remove(__mqhandle_hash, key);
	ha_free((__cms_queue_handle_t *) qhd);

	return TRUE;
}

static gboolean
library_initialized(void)
{
	return __cmsclient_init_flag;
}

void
cmsclient_hash_init()
{
	if (library_initialized())
		return;

	__cmshandle_hash = g_hash_table_new(g_int_hash, g_int_equal);
	__mqhandle_hash = g_hash_table_new(g_int_hash, g_int_equal);
	__group_tracking_hash = g_hash_table_new(g_str_hash, g_str_equal);
	__cmsclient_init_flag = TRUE;
}

/*
 * This is a blocking wait for a particular type of msg on a particular queue. 
 * Note: memory allocated in this function.  caller needs to free().
 */

int
wait_for_msg(__cms_handle_t * handle, size_t msgtype,
             const SaNameT * queueName, client_header_t ** msg,
             SaTimeT timeout)
{
	int fd;
	client_header_t * cms_msg;
	longclock_t t_start = 0, t_end = 0;

	if (timeout < 0)
		return SA_ERR_INVALID_PARAM;

	if (timeout != SA_TIME_END) {
		t_start = time_longclock();
		t_end = t_start + msto_longclock(timeout/1000);
	}

	if (handle->backup_fd >= 0)
		restore_poll(handle);

	fd = handle->ch->ops->get_recv_select_fd(handle->ch);

	dprintf("In %s for message type 0x%x\n", __FUNCTION__, msgtype);

	while (1) {
		int ret = -1;
		struct timeval * tv, to;
		fd_set rset;
		FD_ZERO(&rset);
		FD_SET(fd, &rset);

		tv = NULL;
		if (timeout != SA_TIME_END) {
			to.tv_sec = longclockto_ms((t_end - t_start))/1000;
			to.tv_usec = (((t_end - t_start) -
					secsto_longclock(to.tv_sec)))/1000;
			tv = &to;
		}

		if (!handle->ch->ops->is_message_pending(handle->ch)
		&&	(ret = select(fd + 1, &rset, NULL, NULL, tv)) == -1) {

			cl_log(LOG_ERR, "%s: select error", __FUNCTION__);
			return SA_ERR_LIBRARY;

		} else if (ret == 0) {
			cl_log(LOG_WARNING, "%s: timeout!", __FUNCTION__);
			return SA_ERR_TIMEOUT;
		} 

		if ((ret = cmsclient_message_recv(handle, &cms_msg))!= IPC_OK) {
			if (ret == IPC_FAIL) {
				cl_shortsleep();
				continue;
			}
			cl_log(LOG_ERR, "%s: cmsclient_message_recv failed, "
					"rc = %d", __FUNCTION__, ret);
			return SA_ERR_LIBRARY;
		}

		if (cms_msg->type & msgtype) {

			if (!queueName || (queueName && (saname_cmp(cms_msg->name, *queueName) == 0))) {
				*msg = cms_msg;

				if (g_list_length(handle->dispatch_queue))
					active_poll(handle);

				return SA_OK;
			} 
		} 

		enqueue_dispatch_msg(handle, cms_msg);
		t_start = time_longclock();
	}
}

IPC_Channel *
cms_channel_conn(void)
{
	IPC_Channel * ch;
	GHashTable * attrs;
	char path[] = IPC_PATH_ATTR;
	char cms_socket[] = CMS_DOMAIN_SOCKET;
	int ret;

	attrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(attrs, path, cms_socket);
	ch = ipc_channel_constructor(IPC_DOMAIN_SOCKET, attrs);
	g_hash_table_destroy(attrs);

	if (ch ) {
		ret = ch->ops->initiate_connection(ch);
		if (ret != IPC_OK) {
			cl_log(LOG_ERR, "cms_channel_conn failed, maybe "
					"you don't have cms server running...");
			return NULL;
		}
		return ch;
	}
	else 
		return NULL;
}

static int
enqueue_dispatch_item(GList **queue, client_header_t * item)
{
	*queue = g_list_append(*queue, item);
	return SA_OK;
}

int
enqueue_dispatch_msg(__cms_handle_t * hd, client_header_t * msg)
{
	client_message_t * fmsg = (client_message_t *)msg;

	dprintf("calling enqueue_dispatch_msg ..... \n");

	/*
	 * If it is a message, then add it to the msg queue.
	 */
	if (msg->type == CMS_MSG_NOTIFY) {
		dprintf("got a CMS_MSG_NOTIFY msg\n");
		__notify_acked = FALSE;
	} 

	return enqueue_dispatch_item(&(hd->dispatch_queue),
				     (client_header_t *) fmsg);
}

client_header_t *
dequeue_dispatch_msg(GList ** queue)
{
	client_header_t * msg = NULL;
	GList * head;

	if (!g_list_length(*queue))
		return NULL;

	head = g_list_first(*queue);
	*queue = g_list_remove_link(*queue, head);

	msg = head->data;

	g_list_free_1(head);

	return msg;
}


/**
 * Read all the ipc msg in the buffer and queue them to 
 * the msg queue or the dispatch queue.
 */

int
read_and_queue_ipc_msg(__cms_handle_t * handle)
{
	int ret, count = 0;
	client_header_t *rcmg;
	__mqgroup_track_t * track;
	client_mqgroup_notify_t *nsg, *m;

	dprintf("b4 the do loop of the read_and_queue_ipc_msg ...\n");

	if (handle->backup_fd >= 0)
		restore_poll(handle);

	while (handle->ch->ops->is_message_pending(handle->ch)) {

		ret = cmsclient_message_recv(handle, &rcmg);

		if (ret == IPC_FAIL) {
			cl_shortsleep();
			cl_log(LOG_WARNING, "%s: cmsclient_message_recv "
					"failed, rc = %d", __FUNCTION__, ret);
			break;
		} 

		switch (rcmg->type) {

		case CMS_QUEUEGROUP_NOTIFY:
			/*
			 * prepare the notify buffer
			 */
			m = (client_mqgroup_notify_t *)rcmg;
			m->data = (char *)rcmg +
				sizeof(client_mqgroup_notify_t);

			track = g_hash_table_lookup(__group_tracking_hash,
					(m->group_name).value);
			if (track == NULL) {
				/*
				 * This is possible, because TrackStop
				 * may be called before we get here.
				 */
				cl_log(LOG_INFO, "No one tracks the group"
						 " [%s] membership now!"
				,	m->group_name.value);
				return TRUE;
			}

			track->policy = m->policy;
			track->buf.numberOfItems = m->number;
			track->buf.notification =
				(SaMsgQueueGroupNotificationT *)
				ha_malloc(m->number
					* sizeof(SaMsgQueueGroupNotificationT));

			memcpy(track->buf.notification, m->data, m->number *
				sizeof(SaMsgQueueGroupNotificationT));

			/*
			 * only enqueue head is enough for us
			 */
			dprintf("enqueue group notify msg head\n");
			nsg = (client_mqgroup_notify_t *)
				ha_malloc(sizeof(client_mqgroup_notify_t));
			memcpy(nsg, m, sizeof(client_mqgroup_notify_t));
			enqueue_dispatch_msg(handle, (client_header_t *)nsg);

			ha_free(rcmg);
			break;

		default:
			enqueue_dispatch_msg(handle, rcmg);

			/* TODO: we have a memory leak here 
			   need to call the msg_done()
			 */

			break;
		}
	} 

	return count;
}

int
dispatch_msg(__cms_handle_t * handle, client_header_t * msg) 
{
	client_mqueue_open_t * omsg;
	client_mqgroup_notify_t * nmsg;
	client_message_ack_t * amsg;
	__mqgroup_track_t * track;
	client_message_t * gmsg;
	char * name;
	__cms_queue_handle_t * qhd;

	dprintf("In Function %s..\n", __FUNCTION__);
	dprintf("handle=<%p> msg->type=<%d>\n", handle, msg->type);

	if (handle == NULL || msg == NULL) 
		return HA_FAIL;

	switch (msg->type) {

	case CMS_QUEUE_OPEN_ASYNC:
		omsg = (client_mqueue_open_t *) msg;

		if ((handle->callbacks).saMsgQueueOpenCallback) {
			if (omsg->header.flag != SA_OK) {
				omsg->handle = 0;
			}
			(handle->callbacks).saMsgQueueOpenCallback(
			   	omsg->invocation, &(omsg->handle),
				omsg->header.flag);
		}

		ha_free(omsg);
		break;

  	case CMS_MSG_NOTIFY:
		gmsg = (client_message_t *) msg;
		qhd = g_hash_table_lookup(handle->queue_handle_hash, 
				&(gmsg->handle));

		if (handle->callbacks.saMsgMessageReceivedCallback)
			handle->callbacks.saMsgMessageReceivedCallback(
					&(qhd->queue_handle));
			
		ha_free(gmsg);
 		break;
 
	case CMS_MSG_ACK:
		amsg = (client_message_ack_t *) msg;

		if ((handle->callbacks).saMsgMessageDeliveredCallback) {
			(handle->callbacks).saMsgMessageDeliveredCallback(
					amsg->invocation,
					msg->flag);
		}

		ha_free(amsg);
		break;

 	case CMS_QUEUEGROUP_NOTIFY:
 		nmsg = (client_mqgroup_notify_t *)msg;
 
 		name = (char *) ha_malloc(nmsg->group_name.length + 1);
 		if (name == NULL) {
 			cl_log(LOG_ERR, "%s: ha_malloc failed", __FUNCTION__);
 			return FALSE;
 		}

 		dprintf("group name [%s], length [%d]\n"
 		,	nmsg->group_name.value, nmsg->group_name.length);

 		strncpy(name, nmsg->group_name.value, nmsg->group_name.length);
 		name[nmsg->group_name.length] = '\0';
 		dprintf("name = [%s]\n", name);
 
 		track = g_hash_table_lookup(__group_tracking_hash, name);
 
 		if (track == NULL) {
 			cl_log(LOG_ERR, "Cannot find track buffer");
 			return FALSE;
 		}
 
 		if ((handle->callbacks).saMsgQueueGroupTrackCallback == NULL)
 			return FALSE;
 
 		(handle->callbacks).saMsgQueueGroupTrackCallback(
 			track->name, &(track->buf), track->policy,
 			track->buf.numberOfItems, SA_OK);
 			
		ha_free(name);
		ha_free(nmsg);
 		break;

	default:
		return HA_FAIL;
	}

	return HA_OK;
}

SaErrorT
saMsgInitialize(SaMsgHandleT *msgHandle, const SaMsgCallbacksT *msgCallbacks,
                const SaVersionT *version)
{
	IPC_Channel *ch;
	__cms_handle_t *hd; 
	SaMsgHandleT * key;
	int pipefd[2];

	cl_log_set_entity("libcms");
	cl_log_set_facility(LOG_USER);
#ifdef DEBUG_LIBRARY
	cl_log_enable_stderr(TRUE);
#endif

	if ((!version)
	||	version->releaseCode < 'A' || version->releaseCode > 'Z'
	||	(version->releaseCode << 8) + (version->major << 4) +
		version->minor > (AIS_VERSION_RELEASE_CODE << 8) +
		(AIS_VERSION_MAJOR << 4) + AIS_VERSION_MINOR) {

		cl_log(LOG_ERR, "AIS library version is lower then required");
		return SA_ERR_VERSION;
	}

	if (!msgHandle)
		return SA_ERR_INVALID_PARAM;

	if (!(ch = cms_channel_conn())) {
		cl_log(LOG_ERR, "cms_channel_conn failed.");
		return SA_ERR_LIBRARY;
	}

	if (pipe(pipefd) == -1) {
		cl_log(LOG_ERR, "create pipe failed");
		return SA_ERR_LIBRARY;
        }
	/*
	 * Write something to the pipe but we never read so that
	 * select to this fd will always return immediately.
	 */
	if (write(pipefd[1], "ACTIVE", 6) < 0) {
		cl_log(LOG_ERR, "write pipe failed");
		return SA_ERR_LIBRARY;
	}

	cmsclient_hash_init();

	dprintf("ch_status = %d\n", ch->ch_status);
	dprintf("farside_pid = %d\n", ch->farside_pid);

	hd = (__cms_handle_t *)ha_malloc(sizeof(__cms_handle_t));

	memset(hd, 0, sizeof(__cms_handle_t));
	hd->queue_handle_hash = g_hash_table_new(g_int_hash, g_int_equal);

	hd->ch = ch;
	if (msgCallbacks) {
		memcpy(&(hd->callbacks), msgCallbacks, sizeof(SaMsgCallbacksT));
	} else {
		memset(&(hd->callbacks), 0, sizeof(SaMsgCallbacksT));
	}

	*msgHandle = __cmshandle_counter++;
	hd->service_handle = *msgHandle;
	hd->active_fd = pipefd[0];
	hd->backup_fd = -1;

	key = (SaMsgHandleT *) ha_malloc(sizeof(SaMsgHandleT));
	key = msgHandle;

	g_hash_table_insert(__cmshandle_hash, key, hd);

	return SA_OK;
}

SaErrorT
saMsgFinalize(SaMsgHandleT *msgHandle)
{
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	g_hash_table_foreach_remove(hd->queue_handle_hash, msgqueue_remove, hd);

	g_hash_table_remove(__cmshandle_hash, msgHandle);

	if (hd->backup_fd >= 0)
		restore_poll(hd);
	hd->ch->ops->destroy(hd->ch);

	close(hd->active_fd);
	/* TODO: need to free the glist on the dispatch queue */
	ha_free(hd);
	return SA_OK;
}

SaErrorT
saMsgQueueOpen(const SaMsgHandleT *msgHandle,
               const SaNameT *queueName,
               const SaMsgQueueCreationAttributesT *creationAttributes,
               SaMsgQueueOpenFlagsT openFlags,
               SaTimeT timeout,
               SaMsgQueueHandleT *queueHandle)
{
	int ret;

	client_mqueue_open_t cmg;
	client_mqueue_open_t *rcmg;
	client_header_t * reply;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);

	if (!openFlags)
		return SA_ERR_BAD_FLAGS;

	if (bad_saname(queueName) || !queueHandle
	||	(!creationAttributes && (openFlags & SA_MSG_QUEUE_CREATE))
	||	(creationAttributes && !(openFlags & SA_MSG_QUEUE_CREATE)))
		return SA_ERR_INVALID_PARAM;


	cmg.header.type = CMS_QUEUE_OPEN;
	cmg.header.name = *queueName;

	if (creationAttributes) {

		if ((creationAttributes->creationFlags != 0)
		&&	creationAttributes->creationFlags !=
				SA_MSG_QUEUE_PERSISTENT)

			return SA_ERR_BAD_FLAGS;

		cmg.attr = *creationAttributes;
	} else {
		/*
		 * else set to -1, so that daemon knows client didn't
		 * provide a creationAttributes
		 */
		cmg.attr.creationFlags = -1;
	}

	cmg.openflag = openFlags;
	cmg.invocation = 0;
	cmg.policy = 0;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	if (openFlags & SA_MSG_QUEUE_RECEIVE_CALLBACK
	&&	!(hd->callbacks).saMsgMessageReceivedCallback)
		return SA_ERR_INIT;

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

        dprintf("%s: cmsclient_message_send returns %d\n", __FUNCTION__, ret);

	/*
	 * We should only have one client blocking for it.
	 */
	ret = wait_for_msg(hd, CMS_QUEUE_OPEN, queueName, &reply, timeout);
	if (ret != SA_OK) {
		return ret;
	}

	rcmg = (client_mqueue_open_t *) reply;

	if ((ret = (rcmg->header).flag) == SA_OK) {
		SaMsgQueueHandleT *key;
		__cms_queue_handle_t *qhd;

		key = (SaMsgQueueHandleT *)
				ha_malloc(sizeof(SaMsgQueueHandleT));
		qhd = (__cms_queue_handle_t *)
				ha_malloc(sizeof(__cms_queue_handle_t));
		memset(qhd, 0, sizeof(__cms_queue_handle_t));

		qhd->queue_handle = rcmg->handle;
		qhd->queue_name = *queueName;
		qhd->cms_handle = hd;

		*key = qhd->queue_handle;

		g_hash_table_insert(hd->queue_handle_hash, key, qhd);
		g_hash_table_insert(__mqhandle_hash, key, hd);

		*queueHandle = *key;
	}

	ha_free(rcmg);
	return ret;
}

SaErrorT
saMsgQueueClose(SaMsgQueueHandleT *queueHandle)
{
	int ret;

	client_mqueue_close_t cmg;
	client_header_t *rcmg;
	__cms_handle_t *hd = NULL; 
	__cms_queue_handle_t *qhd;
	SaNameT * qname;
	gpointer origkey, orighd;


	if (g_hash_table_lookup_extended(__mqhandle_hash, queueHandle,
			&origkey, &orighd)) {
		hd = (__cms_handle_t *) orighd;
	};

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, queueHandle ? *queueHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	qhd = g_hash_table_lookup(hd->queue_handle_hash, queueHandle); 
	if (!qhd) {
		cl_log(LOG_ERR, "%s: Cannot find handlle [%d]"
		,	__FUNCTION__, queueHandle ? *queueHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	qname = &(qhd->queue_name);

	cmg.header.type = CMS_QUEUE_CLOSE;
	cmg.header.name = *qname;
	cmg.handle = *queueHandle;
	cmg.silent = FALSE;

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	ret = wait_for_msg(hd, CMS_QUEUE_CLOSE, qname, &rcmg, SA_TIME_END);
	if (ret != SA_OK) 
		return ret;

	if ((ret = rcmg->flag) == SA_OK) {
		g_hash_table_remove(hd->queue_handle_hash, queueHandle);
		g_hash_table_remove(__mqhandle_hash, queueHandle);

		ha_free(origkey);
		ha_free(qhd);

		/* TODO: free the queue msgs. */
	}

	ha_free((client_mqueue_close_t *) rcmg);

	return ret;
}

static void
lookup_queuehandle(gpointer key, gpointer value, gpointer user_data)
{
	char * qname;
	__cms_queue_handle_t *qhd = (__cms_queue_handle_t *)value;
	char * name = (char *)user_data;
	SaMsgQueueHandleT *queueHandle = (SaMsgQueueHandleT *)key;

	qname = saname2str(qhd->queue_name);

	if (!strcmp(qname, name)) {
		g_hash_table_remove(qhd->cms_handle->queue_handle_hash, key);
	}

	g_hash_table_remove(__mqhandle_hash, queueHandle);
}

SaErrorT 
saMsgQueueUnlink(SaMsgHandleT *msgHandle, const SaNameT *queueName)
{
	int ret;
	char * name;
	client_mqueue_unlink_t cmg;
	client_header_t *rcmg;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);

	if (bad_saname(queueName))
		return SA_ERR_INVALID_PARAM;

	cmg.header.type = CMS_QUEUE_UNLINK;
	cmg.header.name = *queueName;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	ret = wait_for_msg(hd, CMS_QUEUE_UNLINK, queueName, &rcmg, 
			SA_TIME_END);

	if (ret != SA_OK) 
		return ret;

	/*
	 * remove from the mq from queue_handle_hash if possible
	 */
	name = saname2str(*queueName);
	g_hash_table_foreach(hd->queue_handle_hash, lookup_queuehandle, name);

	ret = rcmg->flag;
	ha_free((client_mqueue_unlink_t *) rcmg);
	ha_free(name);
	return ret;
}

SaErrorT 
saMsgQueueStatusGet(SaMsgHandleT *msgHandle, const SaNameT *queueName
,	SaMsgQueueStatusT *queueStatus)
{
	int ret;

	client_mqueue_status_t cmg;
	client_mqueue_status_t *rcmg;
	client_header_t * reply;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);

	if (bad_saname(queueName))
		return SA_ERR_INVALID_PARAM;

	cmg.header.type = CMS_QUEUE_STATUS;
	cmg.header.name = *queueName;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	ret = wait_for_msg(hd, CMS_QUEUE_STATUS, queueName, &reply,
			SA_TIME_END);

	if (ret != SA_OK) 
		return ret;

	rcmg = (client_mqueue_status_t *) reply;

	ret = reply->flag;
	if (ret == SA_OK) 
		*queueStatus = rcmg->qstatus;

	ha_free((client_mqueue_status_t *) reply);
	return ret;
}

SaErrorT
saMsgMessageSend(const SaMsgHandleT *msgHandle,
                 const SaNameT *destination,
                 const SaMsgMessageT *message,
                 SaMsgAckFlagsT ackFlags,
                 SaTimeT timeout)
{
	client_message_t *cmg;
	client_header_t *rcmg;
	client_message_ack_t * ack;
	int ret;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	if (message->priority > SA_MSG_MESSAGE_LOWEST_PRIORITY 
	||	!(ackFlags & SA_MSG_MESSAGE_DELIVERED_ACK))
		return SA_ERR_INVALID_PARAM;

	if (ackFlags & ~SA_MSG_MESSAGE_DELIVERED_ACK)
		return SA_ERR_BAD_FLAGS;

	cmg = (client_message_t *)
		ha_malloc(sizeof(client_message_t) + message->size);

	cmg->header.type = CMS_MSG_SEND;
	cmg->header.name = *destination;
	cmg->msg = *message;
	cmg->invocation = 0;
	cmg->data = cmg + 1;
	memcpy(cmg->data, message->data, message->size);
	cmg->ack = SA_MSG_MESSAGE_DELIVERED_ACK; /* according to the spec */

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	if (ackFlags & SA_MSG_MESSAGE_DELIVERED_ACK
	&&	!(hd->callbacks).saMsgMessageDeliveredCallback)
		return SA_ERR_INIT;

	ret = cmsclient_message_send(hd,
			sizeof(client_message_t) + message->size, cmg);

	ha_free(cmg);

	while (1) {

		ret = wait_for_msg(hd, CMS_MSG_ACK,
				destination, &rcmg, timeout);
		if (ret != SA_OK) 
			return ret;

		ret = rcmg->flag;
		ack = (client_message_ack_t *) rcmg;

		/*
		 * CMS_MSG_SEND is a blocking call, so we can only
		 * have one client waiting for it. Thus when we get
		 * an ACK that is for the request type CMS_MSG_SEND,
		 * we know this is the ACK we are waiting for.
		 */
		dprintf("type is %d\n", ack->send_type);

		if (ack->send_type == CMS_MSG_SEND) {
			ha_free((client_message_t *) rcmg);
			return ret;

		} else {
			enqueue_dispatch_msg(hd, rcmg);
		}
	}
}

SaErrorT
saMsgMessageSendAsync(const SaMsgHandleT *msgHandle,
                      SaInvocationT invocation,
                      const SaNameT *destination,
                      const SaMsgMessageT *message,
                      SaMsgAckFlagsT ackFlags)
{
	client_message_t *cmg;
	int ret;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	cmg = (client_message_t *)
			ha_malloc(sizeof(client_message_t) + message->size);

	cmg->header.type = CMS_MSG_SEND_ASYNC;
	cmg->header.name = *destination;
	cmg->msg = *message;
	cmg->invocation = invocation;
	cmg->data = (char *)cmg + sizeof(client_message_t);
	memcpy(cmg->data, message->data, message->size);
	cmg->ack = ackFlags;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd,
			sizeof(client_message_t) + message->size, cmg);

	ha_free(cmg);

	return ret == IPC_OK ? SA_OK : SA_ERR_LIBRARY;
}

static int
request_for_message(__cms_handle_t * hd, const SaNameT * name)
{
	client_header_t request_msg;

	request_msg.type = CMS_MSG_REQUEST;
	request_msg.name = *name;

	return cmsclient_message_send(hd, sizeof(request_msg), &request_msg);
}

SaErrorT
saMsgMessageGet(const SaMsgQueueHandleT *queueHandle,
                SaMsgMessageT *message,
                SaMsgMessageInfoT *messageInfo,
                SaTimeT timeout)
{
	int ret;
	SaErrorT error = SA_OK;
	client_message_t * cmg;
	client_header_t *rcmg;
	__cms_handle_t *hd = GET_MQ_HANDLE(queueHandle);
	__cms_queue_handle_t *qhd;
	SaNameT * qname;
	int freecmg = 0;


	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by queueHandle [%d]"
		,	__FUNCTION__, queueHandle ? *queueHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	if (!messageInfo || !message) 
		return SA_ERR_INVALID_PARAM;

	memset(messageInfo, 0, sizeof(SaMsgMessageInfoT));

	qhd = g_hash_table_lookup(hd->queue_handle_hash, queueHandle);
	assert(qhd != NULL);
	qname = &(qhd->queue_name);

	/*
	 * request a message from daemon
	 */
	while (1) {
		request_for_message(hd, qname);

		ret = wait_for_msg(hd, CMS_MSG_GET | CMS_MSG_NOTIFY, qname, &rcmg,
				   timeout);

		if (ret != SA_OK) {
			cl_log(LOG_ERR, "wait_for_msg error [%d]", ret);
			return ret;
		}

		if (rcmg->type == CMS_MSG_NOTIFY) {
			dprintf("Received CMS_MSG_NOTIFY\n");
			continue;
		} else 
			break;
	} 

	cmg = (client_message_t *)rcmg;
	cmg->data = (void *)((char *)cmg + sizeof(client_message_t));
	dprintf("message.data is [%s]\n", (char *)cmg->data);

	if (cmg->senderId) {
		messageInfo->senderId = cmg->senderId;
	}

	if (message->size < cmg->msg.size) 
		error = SA_ERR_NO_SPACE;

	message->size = (cmg->msg).size;

	if (message->data) {
		memcpy(message->data, cmg->data, 
			(cmg->msg.size > message->size ? 
			 message->size : cmg->msg.size));
		freecmg = 1;
	} else {
		message->data = cmg->data;
	}

	message->type = cmg->msg.type;
	message->version = cmg->msg.version;
	message->priority = cmg->msg.priority;

	if (freecmg) 
		ha_free(cmg);

	/* TODO: message info */

	return error;
}

SaErrorT 
saMsgMessageReceivedGet(const SaMsgQueueHandleT *queueHandle,
                        const SaMsgMessageHandleT *messageHandle,
                        SaMsgMessageT *message,
                        SaMsgMessageInfoT *messageInfo)
{
	return SA_ERR_NOT_SUPPORTED;
}

SaErrorT 
saMsgMessageCancel(const SaMsgQueueHandleT *queueHandle)
{
	return SA_ERR_NOT_SUPPORTED;
}


SaErrorT
saMsgSelectionObjectGet(const SaMsgHandleT *msgHandle, 
		SaSelectionObjectT *selectionObject)
{
	int ret;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	dprintf("hd->backup_fd is [%d]\n", hd->backup_fd);

	ret = hd->backup_fd >= 0 ? hd->active_fd : 
		hd->ch->ops->get_recv_select_fd(hd->ch);

	if (ret < 0)
		return SA_ERR_LIBRARY;

	*selectionObject = ret;

	return SA_OK;
}

SaErrorT
saMsgDispatch(const SaMsgHandleT *msgHandle, SaDispatchFlagsT dispatchFlags)
{
	int ret;
	client_header_t *msg;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	switch (dispatchFlags) {
	case SA_DISPATCH_ONE:
		read_and_queue_ipc_msg(hd);
		if ((msg = dequeue_dispatch_msg(&hd->dispatch_queue)) == NULL) {
			cl_log(LOG_ERR, "%s: dequeue_dispatch_msg got NULL"
			,	__FUNCTION__);
			return SA_OK;
		}

		ret = dispatch_msg(hd, msg);

		if (g_list_length(hd->dispatch_queue))
			active_poll(hd);

		if (ret != HA_OK) 
			return SA_ERR_LIBRARY;
		break;

	case SA_DISPATCH_ALL:
		read_and_queue_ipc_msg(hd);

		do {
			if ((msg = dequeue_dispatch_msg(&hd->dispatch_queue)) 
					!= NULL) {
				dispatch_msg(hd, msg);
			}
		} while (g_list_length(hd->dispatch_queue));

		break;

	case SA_DISPATCH_BLOCKING:
		break;

	default:
		cl_log(LOG_ERR, "%s: wrong dispatchFlags [%d]", 
				__FUNCTION__, dispatchFlags);
		return SA_ERR_INVALID_PARAM;
	}
	return SA_OK;
}

SaErrorT
saMsgQueueOpenAsync(const SaMsgHandleT *msgHandle,
		    SaInvocationT invocation,
		    const SaNameT *queueName,
		    const SaMsgQueueCreationAttributesT *creationAttributes,
		    SaMsgQueueOpenFlagsT openFlags)
{
	client_mqueue_open_t cmg;
	int ret;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);

	if (!openFlags)
		return SA_ERR_BAD_FLAGS;

	if (bad_saname(queueName) || !msgHandle
	||	(!creationAttributes && openFlags & SA_MSG_QUEUE_CREATE)
	||	(creationAttributes && !(openFlags & SA_MSG_QUEUE_CREATE)))
		return SA_ERR_INVALID_PARAM;


	cmg.header.type = CMS_QUEUE_OPEN_ASYNC;
	cmg.header.name = *queueName;

	if (creationAttributes) {

		if ((creationAttributes->creationFlags != 0)
		&&	creationAttributes->creationFlags !=
				SA_MSG_QUEUE_PERSISTENT)

			return SA_ERR_BAD_FLAGS;

		cmg.attr = *creationAttributes;
	}

	cmg.openflag = openFlags;
	cmg.invocation = invocation;
	cmg.policy = 0;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	if (openFlags & SA_MSG_QUEUE_RECEIVE_CALLBACK
	&&	!(hd->callbacks).saMsgQueueOpenCallback
	&&	!(hd->callbacks).saMsgMessageReceivedCallback)
		return SA_ERR_INIT;

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	if (ret != IPC_OK)
		cl_log(LOG_ERR, "%s: cmsclient_message_send failed, rc = %d"
		,	__FUNCTION__, ret);

	return ret == IPC_OK ? SA_OK : SA_ERR_LIBRARY;
}


SaErrorT
saMsgQueueGroupCreate(SaMsgHandleT *msgHandle,
                      const SaNameT *queueGroupName,
                      SaMsgQueueGroupPolicyT queueGroupPolicy)
{
	int ret;

	client_mqueue_open_t cmg;
	client_mqueue_open_t *rcmg;
	client_header_t * reply;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	cmg.header.type = CMS_QUEUEGROUP_CREATE;
	cmg.header.name = *queueGroupName;
	cmg.invocation = 0;

	if (queueGroupPolicy != SA_MSG_QUEUE_GROUP_ROUND_ROBIN)
		return SA_ERR_INVALID_PARAM;

	cmg.policy = queueGroupPolicy;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	if (ret != IPC_OK) {
		cl_log(LOG_ERR, "%s: cmsclient_message_send returns %d"
		,	__FUNCTION__,ret);
		return SA_ERR_LIBRARY;
	}

	ret = wait_for_msg(hd, CMS_QUEUEGROUP_CREATE, queueGroupName, &reply,
			   SA_TIME_END);

	if (ret != SA_OK)
		return ret;

	rcmg = (client_mqueue_open_t *) reply;

	if ((rcmg->header).flag == SA_OK) {
		SaMsgQueueHandleT *key;

		key = (SaMsgQueueHandleT *)ha_malloc(sizeof(SaMsgQueueHandleT));
		*key = rcmg->handle;
		g_hash_table_insert(__mqhandle_hash, key, hd);
	}

	ret = (rcmg->header).flag;

	ha_free(rcmg);
	return ret;
}

SaErrorT 
saMsgQueueGroupDelete(SaMsgHandleT *msgHandle, const SaNameT *queueGroupName)
{
	/* TODO: we should remove the key that's in the __mqhandle_hash
	 * as well
	 */
	return saMsgQueueUnlink(msgHandle, queueGroupName);
}

SaErrorT 
saMsgQueueGroupInsert(SaMsgHandleT *msgHandle,
                      const SaNameT *queueGroupName,
                      const SaNameT *queueName)
{
	int ret;

	client_header_t * rcmg;
	client_mqgroup_ops_t cmg;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	if (bad_saname(queueName))
		return SA_ERR_INVALID_PARAM;

	cmg.header.type = CMS_QUEUEGROUP_INSERT;
	cmg.header.name = *queueName;
	cmg.qgname = *queueGroupName;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	if (ret != IPC_OK) {
	        cl_log(LOG_ERR, "%s: cmsclient_message_send returns %d"
		,	__FUNCTION__, ret);
		return SA_ERR_LIBRARY;
	}

	ret = wait_for_msg(hd, CMS_QUEUEGROUP_INSERT, queueName, &rcmg,
			   SA_TIME_END);

	if (ret != SA_OK)
		return ret;

	ret = rcmg->flag;

	ha_free(rcmg);
	return ret;
}

SaErrorT 
saMsgQueueGroupRemove(SaMsgHandleT *msgHandle,
                      const SaNameT *queueGroupName,
                      const SaNameT *queueName)
{
	int ret;

	client_header_t * rcmg;
	client_mqgroup_ops_t cmg;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	if (bad_saname(queueName))
		return SA_ERR_INVALID_PARAM;

	cmg.header.type = CMS_QUEUEGROUP_REMOVE;
	cmg.header.name = *queueName;
	cmg.qgname = *queueGroupName;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	if (ret != IPC_OK) {
	        cl_log(LOG_ERR, "%s: cmsclient_message_send returns %d"
		,	__FUNCTION__, ret);
		return SA_ERR_LIBRARY;
	}

	ret = wait_for_msg(hd, CMS_QUEUEGROUP_REMOVE, queueName, &rcmg, 
			SA_TIME_END);

	if (ret != SA_OK) 
		return ret;
	
	ret = rcmg->flag;

	ha_free((client_mqueue_unlink_t *) rcmg);
	return ret;
}

SaErrorT 
saMsgQueueGroupTrack(const SaMsgHandleT *msgHandle,
                     const SaNameT *queueGroupName,
                     SaUint8T trackFlags,
                     SaMsgQueueGroupNotificationBufferT *notificationBuffer)
{
	int ret;
	client_mqgroup_mem_t cmg;
	client_header_t * rcmg;
	client_mqgroup_notify_t * rmsg;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	if (!(hd->callbacks).saMsgQueueGroupTrackCallback)
		return SA_ERR_INIT;

	if ((trackFlags & SA_TRACK_CHANGES)
	&&	(trackFlags & SA_TRACK_CHANGES_ONLY))
		return SA_ERR_BAD_FLAGS;

	/* tell server we care about the membership information */

	cmg.header.type = CMS_QUEUEGROUP_TRACK_START;
	cmg.header.name = *queueGroupName;
	cmg.group_name = *queueGroupName;
	cmg.flag = trackFlags;

	ret = cmsclient_message_send(hd, sizeof(cmg), &cmg);

	if (ret != IPC_OK) {
	        cl_log(LOG_ERR, "%s: cmsclient_message_send returns %d"
		,	__FUNCTION__, ret);
		return SA_ERR_LIBRARY;
	}

	ret = wait_for_msg(hd, CMS_QUEUEGROUP_TRACK_START, queueGroupName,
			&rcmg, SA_TIME_END);

	if (ret != SA_OK) 
		return ret;

	if ((ret = rcmg->flag) != SA_OK)
		return ret;

	rmsg = (client_mqgroup_notify_t *)rcmg;

	if ((trackFlags & SA_TRACK_CHANGES)
	||	(trackFlags & SA_TRACK_CHANGES_ONLY)) {
		/*
		 * Track membership changes with callbacks.
		 */
		__mqgroup_track_t * track;
		char * name;

		name = (char *) ha_malloc(queueGroupName->length + 1);
		strncpy(name, queueGroupName->value, queueGroupName->length);
		name[queueGroupName->length] = '\0';
		
		track = (__mqgroup_track_t *)
				ha_malloc(sizeof(__mqgroup_track_t));

		track->name = queueGroupName;
		track->flag = trackFlags & ~SA_TRACK_CURRENT;
		g_hash_table_insert(__group_tracking_hash, name, track);
	}

	if (trackFlags & SA_TRACK_CURRENT) {
		rmsg->data = (char *)rmsg + sizeof(client_mqgroup_notify_t);

		if (!notificationBuffer) {
			/*
			 * Client wants saMsgQueueGroupTrackCallback.
			 */
			goto exit;
		}

		dprintf("numberOfItems %lu, real number %lu\n"
		,	notificationBuffer->numberOfItems, rmsg->number);

		if (!notificationBuffer->notification) {
			notificationBuffer->notification =
				(SaMsgQueueGroupNotificationT *)
				ha_malloc(rmsg->number *
					sizeof(SaMsgQueueGroupNotificationT));
			if (!notificationBuffer->notification) {
				ret = SA_ERR_NO_MEMORY;
				goto exit;
			}
		} else if (notificationBuffer->numberOfItems < rmsg->number) {
			ret = SA_ERR_NO_SPACE;
			goto exit;
		}

		notificationBuffer->numberOfItems = rmsg->number;
		memcpy(notificationBuffer->notification, rmsg->data
		,	rmsg->number * sizeof(SaMsgQueueGroupNotificationT));

	}

exit:
	ha_free(rcmg);
	return ret;
}

SaErrorT 
saMsgQueueGroupTrackStop(const SaMsgHandleT *msgHandle,
                         const SaNameT *queueGroupName)
{
	char * name;
	gpointer key, track;
	__cms_handle_t *hd = GET_CMS_HANDLE(msgHandle);


	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle ? *msgHandle : -1);
		return SA_ERR_BAD_HANDLE;
	}

	if (bad_saname(queueGroupName))
		return SA_ERR_INVALID_PARAM;

	name = (char *) ha_malloc(queueGroupName->length + 1);
	strncpy(name, queueGroupName->value, queueGroupName->length);
	name[queueGroupName->length] = '\0';

	if (g_hash_table_lookup_extended(__group_tracking_hash, name, &key
	,	&track) == TRUE) {
		g_hash_table_remove(__group_tracking_hash, key);
		ha_free(key);
	} else
		return SA_ERR_NOT_EXIST;

	ha_free(name);

	return SA_OK;
}

SaErrorT 
saMsgMessageSendReceive(SaMsgHandleT msgHandle,
			const SaNameT *destination,
                        const SaMsgMessageT *sendMessage,
                        SaMsgMessageT *receiveMessage,
			SaTimeT *replySendTime,
                        SaTimeT timeout)
{
	SaErrorT error = SA_OK;
	int ret;
	__cms_handle_t *hd = GET_CMS_HANDLE(&msgHandle);
	const SaMsgMessageT * message;
	client_message_t *cmg;
	client_header_t *rcmg;
	client_message_t * ack;
	int freeack = 0;

	message = sendMessage;

	if (message->priority > SA_MSG_MESSAGE_LOWEST_PRIORITY)
		return SA_ERR_INVALID_PARAM;

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle);
		return SA_ERR_BAD_HANDLE;
	}

	cmg = (client_message_t *)
		ha_malloc(sizeof(client_message_t) + message->size);

	cmg->header.type = CMS_MSG_SEND_RECEIVE;
	cmg->header.name = *destination;
	cmg->msg = *message;
	cmg->invocation = 0;
	cmg->data = cmg + 1;
	cmg->sendreceive = 1;
	memcpy(cmg->data, message->data, message->size);
	cmg->ack = SA_MSG_MESSAGE_DELIVERED_ACK; /* according to the spec */

	ret = cmsclient_message_send(hd,
			sizeof(client_message_t) + message->size, cmg);

	/* TODO: fix needed.  this can only be called after the msg_done */
	ha_free(cmg);

	while (1) {
		ret = wait_for_msg(hd, CMS_MSG_RECEIVE, 
				NULL, &rcmg, timeout);
		if (ret != SA_OK) 
			return ret;
		else 
			break;
	}

	ret = rcmg->flag;
	ack = (client_message_t *) rcmg;
	ack->data = (void *)((char *)cmg + sizeof(client_message_t));
	if (ack->msg.size > receiveMessage->size) {
		error = SA_ERR_NO_SPACE;
	}
	receiveMessage->size = ack->msg.size;

	if (receiveMessage->data) {
		memcpy(receiveMessage->data, ack->data,
			(ack->msg.size > receiveMessage->size ? 
			 receiveMessage->size : ack->msg.size));
		freeack = 1;
	} else {
		receiveMessage->data = ack->data;
	}

	receiveMessage->type = ack->msg.type;
	receiveMessage->version = ack->msg.version;
	receiveMessage->priority = 0;

	if (freeack) 
		ha_free(ack);

	dprintf("type is %d\n", ack->send_type);

	return error;
}

SaErrorT 
saMsgMessageReply(SaMsgHandleT msgHandle,
		  const SaMsgMessageT *replyMessage,
                  const SaMsgSenderIdT *senderId,
                  SaTimeT timeout)
{
	client_message_t *cmg;
	client_header_t *rcmg;
	client_message_ack_t * ack;
	int ret;
	__cms_handle_t *hd = GET_CMS_HANDLE(&msgHandle);

	cmg = (client_message_t *)
		ha_malloc(sizeof(client_message_t) + replyMessage->size);

	cmg->header.type = CMS_MSG_REPLY;
	cmg->header.name.length = 0;
	cmg->msg = *replyMessage;
	cmg->invocation = 0;
	cmg->data = cmg + 1;
	memcpy(cmg->data, replyMessage->data, replyMessage->size);
	cmg->ack = SA_MSG_MESSAGE_DELIVERED_ACK; /* according to the spec */

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd,
		sizeof(client_message_t) + replyMessage->size, cmg);

	ha_free(cmg);

	while (1) {

		ret = wait_for_msg(hd, CMS_MSG_ACK,
				NULL, &rcmg, timeout);
		if (ret != SA_OK) 
			return ret;

		ret = rcmg->flag;
		ack = (client_message_ack_t *) rcmg;

		/*
		 * CMS_MSG_SEND is a blocking call, so we can only
		 * have one client waiting for it. Thus when we get
		 * an ACK that is for the request type CMS_MSG_SEND,
		 * we know this is the ACK we are waiting for.
		 */
		dprintf("type is %d\n", ack->send_type);

		if (ack->send_type == CMS_MSG_REPLY) {
			ha_free((client_message_t *) rcmg);
			return ret;

		} else {
			enqueue_dispatch_msg(hd, rcmg);
		}
	}

	return SA_ERR_NOT_SUPPORTED;
}

SaErrorT 
saMsgMessageReplyAsync(SaMsgHandleT msgHandle,
                       SaInvocationT invocation,
                       const SaMsgMessageT *replyMessage,
		       const SaMsgSenderIdT *senderId,
                       SaMsgAckFlagsT ackFlags)
{
	client_message_t *cmg;
	int ret;
	__cms_handle_t *hd = GET_CMS_HANDLE(&msgHandle);


	cmg = (client_message_t *)
			ha_malloc(sizeof(client_message_t) + replyMessage->size);

	cmg->header.type = CMS_MSG_REPLY_ASYNC;
	cmg->header.name.length = 0;
	cmg->msg = *replyMessage;
	cmg->invocation = 0;
	cmg->data = cmg + 1;
	memcpy(cmg->data, replyMessage->data, replyMessage->size);
	cmg->ack = SA_MSG_MESSAGE_DELIVERED_ACK; /* according to the spec */

	if (hd == NULL) {
		cl_log(LOG_ERR, "%s: Cannot find hd by handlle [%d]"
		,	__FUNCTION__, msgHandle);
		return SA_ERR_BAD_HANDLE;
	}

	ret = cmsclient_message_send(hd,
		sizeof(client_message_t) + replyMessage->size, cmg);

	ha_free(cmg);

	return ret == IPC_OK ? SA_OK : SA_ERR_LIBRARY;
}



