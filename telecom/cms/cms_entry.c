/*
 * cms_entry.c: cms daemon state machine entry
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

#include <portability.h>
#include <assert.h>

#include "cms_data.h"
#include "cms_common.h"
#include "cms_cluster.h"
#include "cms_client.h"
#include "cms_membership.h"


gboolean
cluster_input_dispatch(IPC_Channel * channel, gpointer user_data)
{
	struct ha_msg	* reply;
	const char	* type;
	ll_cluster_t	* hb = ((cms_data_t *)user_data)->hb_handle;

repeat:
	/*
	 * heartbeat message received
	 */
	reply = hb->llc_ops->readmsg(hb, 0);

	if (!reply)
		return TRUE;

	type = ha_msg_value(reply, F_TYPE);
	assert(type);

	dprintf("%s: Received msg from hb, type = %s\n", __FUNCTION__, type);

#if DEBUG_CLUSTER
	cl_log_message(reply);
#endif

	switch (mqname_string2type(type)) {

	case MQNAME_TYPE_REQUEST:
		reply_mqname_open(hb, reply);
		break;

	case MQNAME_TYPE_GRANTED:
		process_mqname_granted(reply, &cms_data);
		break;

	case MQNAME_TYPE_REOPEN:
	case MQNAME_TYPE_REOPEN_MSGFEED:
	case MQNAME_TYPE_MSGFEED_END:
		process_mqname_reopen(reply, mqname_string2type(type)
		,	&cms_data);
		break;

	case MQNAME_TYPE_DENIED:
		process_mqname_denied(reply);
		break;

	case MQNAME_TYPE_CLOSE:
		process_mqname_close(reply);
		break;

	case MQNAME_TYPE_UNLINK:
		process_mqname_unlink(reply);
		break;

	case MQNAME_TYPE_SEND:
		process_mqname_send(reply, &cms_data);
		break;

	case MQNAME_TYPE_INSERT:
		process_mqgroup_insert(reply);
		break;

	case MQNAME_TYPE_REMOVE:
		process_mqgroup_remove(reply);
		break;

	case MQNAME_TYPE_ACK:
		process_mqname_ack(reply);
		break;

	case MQNAME_TYPE_UPDATE:
		process_mqname_update(reply, &cms_data);
		break;

	case MQNAME_TYPE_STATUS_REQUEST:
		reply_mqueue_status(reply, &cms_data);
		break;

	case MQNAME_TYPE_STATUS_REPLY:
		process_mqueue_status(reply);
		break;

	case MQNAME_TYPE_UPDATE_REQUEST:
		process_mqinfo_update_request(reply, &cms_data);
		break;

	case MQNAME_TYPE_REPLY:
		process_mqsend_reply(reply, &cms_data);
		break;

	default:
		cl_log(LOG_ERR, "%s: Unknow type [%s]", __FUNCTION__, type);
		break;
	}

	ha_msg_del(reply);

	goto repeat;
}

static gboolean
process_client_message(IPC_Channel * client, client_header_t * msg)
{
	int ret;

	/*
	 * client message received
	 */
	dprintf("%s: Received msg from client <%p>, type = %d\n"
	,	__FUNCTION__, client, (int)msg->type);


	switch (msg->type) {
		case CMS_QUEUE_STATUS:

			ret = client_process_qstatus(client, msg, &cms_data);
			break;

		case CMS_QUEUE_OPEN_ASYNC:
		case CMS_QUEUE_OPEN:
		case CMS_QUEUEGROUP_CREATE:

			ret = client_process_mqopen(client, msg, &cms_data);
			break;

		case CMS_QUEUE_CLOSE:
			ret = client_process_mqclose(client, msg, &cms_data);
			break;

		case CMS_QUEUE_UNLINK:
			ret = client_process_mqunlink(client, msg, &cms_data);
			break;

		case CMS_MSG_SEND:
		case CMS_MSG_SEND_ASYNC:
		case CMS_MSG_SEND_RECEIVE:
			ret = client_process_mqsend(client, msg, &cms_data);
			break;

		case CMS_MSG_REQUEST:
			ret = client_process_message_request(client, msg);
			break;

		case CMS_QUEUEGROUP_INSERT:
			ret = client_process_mqgroup_insert(client, msg,
					&cms_data);
			break;

		case CMS_QUEUEGROUP_REMOVE:
			ret = client_process_mqgroup_remove(client, msg,
					&cms_data);
			break;

		case CMS_QUEUEGROUP_TRACK_START:
			ret =client_process_mqgroup_track(client, msg);
			break;

		case CMS_QUEUEGROUP_TRACK_STOP:
			ret = client_process_mqgroup_track_stop(client, msg);
			break;

		case CMS_MSG_REPLY:
		case CMS_MSG_REPLY_ASYNC:
			ret = client_process_mqsend_reply(client, msg, &cms_data);
			break;
		default:
			cl_log(LOG_ERR, "Unknow message type [%d]"
			,	(int)msg->type);
			return HA_FAIL;
	}

	return ret;
}

static void
cms_close_mqueue(gpointer data, gpointer user_data)
{
	mqueue_t * mq = (mqueue_t *)data;
	cms_client_t * cms_client = (cms_client_t *) user_data;

	dprintf("%s: mqname_close %s\n", __FUNCTION__, mq->name);
	request_mqname_close(mq->name, &cms_data);

	cms_client->opened_mqueue_list =
                g_list_remove(cms_client->opened_mqueue_list, mq);
}

#if DEBUG_EXIT
static void
dump_client_table(gpointer key, gpointer value, gpointer user_data)
{
	dprintf("Begin dumping hash table ...\n");
	dprintf("key = %d, value = <%p>\n", *(int *)key, value);
	dprintf("End dumping hash table\n");
}
#endif

gboolean
client_input_dispatch(IPC_Channel * client, gpointer user_data)
{
	IPC_Message * msg;
	client_header_t *cms_msg;
	int ret;
	cms_client_t * cms_client;
	cms_data_t * cmsdata = (cms_data_t *) user_data;
	gpointer orig_key, value;


	if (client->ops->is_message_pending(client)) {

		ret = client->ops->recv(client, &msg);

		if (ret == IPC_FAIL) {
			cl_log(LOG_ERR, "%s: client->ops->recv failed. "
					"ret = %d\n", __FUNCTION__, ret);
			return FALSE;

		} else if (ret == IPC_BROKEN) {
			cl_log(LOG_ERR, "%s: client->ops->recv failed. "
					"ret = %d\n", __FUNCTION__, ret);

			/*
			 * Client disconnected. According to the spec,
			 * we should close all message queues opened by
			 * this client.
			 */
#if DEBUG_EXIT
			cl_log(LOG_INFO
			,	"Close all message queues for farside_pid "
				"[%d], client_table <%p>\n"
			,	client->farside_pid
			,	cmsdata->client_table);

			dprintf("client_table <%p>\n", cmsdata->client_table);

			g_hash_table_foreach(cmsdata->client_table
			,	dump_client_table, NULL);
#endif

			ret = g_hash_table_lookup_extended(
					cmsdata->client_table,
					&client->farside_pid,
					&orig_key,
					&value);

			assert(ret == TRUE);

			cms_client = (cms_client_t *)value;

#if DEBUG_EXIT
			dprintf("%s: channel_count now is %d\n", __FUNCTION__
			,	cms_client->channel_count);
#endif

			/*
			 * If we are not the last one, skip.
			 */
			if (--cms_client->channel_count == 0) {
				g_list_foreach(cms_client->opened_mqueue_list
				,	cms_close_mqueue, &cms_client);

#if DEBUG_EXIT
				dprintf("%s: remove key [%d] from "
					"client_table\n", __FUNCTION__,
					client->farside_pid);
#endif
				g_hash_table_remove(cmsdata->client_table
				,	&client->farside_pid);

				ha_free(orig_key);
				ha_free(cms_client);
			}

			return FALSE;
		}

		cms_msg = (client_header_t *) msg->msg_body;

		if (!cmsdata->cms_ready) {
			cl_log(LOG_INFO, "cms_ready = %d"
			,	cmsdata->cms_ready);

			client_send_notready_msg(client, cms_msg);
			return TRUE;
		}

		/*
		 * process this client messages
		 */
		ret = process_client_message(client, cms_msg);

		if (msg->msg_done) {
			msg->msg_done(msg);
		}

		if (ret != HA_OK) {
			return FALSE;
		}
	}

	return TRUE;
}

