/*
 * cms_mqueue.c: cms daemon message queue operation
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
#include <string.h>
#include <stdlib.h>

#include "hb_api.h"
#include "clplumbing/GSource.h"
#include "saf/ais_message.h"
#include "heartbeat.h"

#include "cms_common.h"

static GHashTable * mqtable_name_hash;   /* mq hash key by name */
static GHashTable * mqtable_handle_hash; /* mq hash key by handle, ditto */
static guint __msghandle_counter = 0;	 /* mqtable_handle_hash handle key */

int
mqueue_table_init(void)
{
	mqtable_name_hash = g_hash_table_new(g_str_hash, g_str_equal);
	mqtable_handle_hash = g_hash_table_new(g_int_hash, g_int_equal);
	return HA_OK;
}

guint
mqueue_handle_insert(mqueue_t *mq)
{
	guint * handle;

	handle = (guint *) cl_malloc(sizeof(guint));
	*handle = __msghandle_counter++;
	g_hash_table_insert(mqtable_handle_hash, handle, mq);
	mq->handle = *handle;

	return *handle;
}

int
mqueue_table_insert(mqueue_t *mq)
{
	g_hash_table_insert(mqtable_name_hash, mq->name, mq);

	return SA_OK;
}

void
dump_mqueue_list(mqueue_t * mq)
{
	dprintf("Begin to dump mq%s list for [%s]...\n"
	,	IS_MQGROUP(mq) ? "group" : "queue", mq->name);
	dprintf("first <%p>, current %s <%p>, last <%p>\n"
	,	g_list_first(mq->list), mq->name, mq->current
	,	g_list_last(mq->list));
}

void
unref_mqueue(gpointer data, gpointer user_data)
{
	mqueue_t *mq = (mqueue_t *) data;
	mqueue_t *mqg = (mqueue_t *) user_data;

	mq->list = g_list_remove(mq->list, mqg);
	if (G_LIST_EMPTY(mq->list))
		mq->list = NULL;

	dump_mqueue_list(mq);
}

void
unref_mqgroup(gpointer data, gpointer user_data)
{
	mqueue_t *mqg = (mqueue_t *)data;
	mqueue_t *mq = (mqueue_t *)user_data;

	dprintf("%s, mqg = %s, mq = %s\n", __FUNCTION__, mqg->name, mq->name);

	mqgroup_unref_mqueue(mqg, mq);
}

int
mqgroup_unref_mqueue(mqueue_t * mqg, mqueue_t * mq)
{
	dump_mqueue_list(mqg);
	dprintf("%s: mq <%p>\n", __FUNCTION__, mq);

	mqg->current = CIRCLE_LIST_NEXT(mqg->list, mqg->current);

	if (mq == mqg->current->data)
		mqg->current = CIRCLE_LIST_NEXT(mqg->list, mqg->current);

	mqg->list = g_list_remove(mqg->list, mq);

	if (G_LIST_EMPTY(mqg->list))
		mqg->current = NULL;

	dump_mqueue_list(mqg);
	return TRUE;
}

int
mqueue_table_remove(const char * qname)
{
	mqueue_t * mq;

	CMS_TRACE();

	dprintf("%s, queue name = %s\n", __FUNCTION__, qname);

	mq = g_hash_table_lookup(mqtable_name_hash, qname);
	if (!mq) 
		return HA_FAIL;

	g_hash_table_remove(mqtable_name_hash, qname);

	/* TODO: list memory needs to be freed as well */

	if (mq->list && !IS_MQGROUP(mq)) {
		/*
		 * unreference this mqueue from mqgroups include it
		 *
		 * the mq->policy check above make sure that this is a 
		 * regular queue, not a queue group.
		 */
		g_list_foreach(mq->list, unref_mqgroup, mq);
		g_list_free(mq->list);
	} else {
		/* handle the queue group case 
		 * remove the reference from the mq about this mqg.
		 */

		g_list_foreach(mq->list, unref_mqueue, mq);
		g_list_free(mq->list);
	}
	if (mq->notify_list) 
		g_list_free(mq->notify_list);

	if (mq->client)
		cl_free(mq->client);

	cl_free(mq->name);
	cl_free(mq->host);

	cl_free(mq);

	return HA_OK;
}

int
mqueue_handle_remove(guint *hd)
{
	gpointer orig_hd, mq;
	gboolean found;

	found = g_hash_table_lookup_extended(mqtable_handle_hash, hd,
			&orig_hd, &mq);

	if (!found)
		return HA_FAIL;

	g_hash_table_remove(mqtable_handle_hash, hd);

	cl_free((guint *)orig_hd);

	/*
	 * mq is not freed here. in case the queue might be reopened later
	 */

	return HA_OK;
}

mqueue_t *
mqueue_table_lookup(const char * qname, int * group)
{
	mqueue_t *mq;

	if ((mq = g_hash_table_lookup(mqtable_name_hash, qname))) {
		if (group != NULL) {
			if (mq->policy == 0)
				*group = FALSE;
			else
				*group = TRUE;
		}
		return mq;
	}

	return NULL;
}

/**
 * mqname_lookup - lookup a message queue in the cluster
 * @name:	message queue name
 * @group:	It will be set to TRUE is the name belongs to a message
 *		queue group, otherwise set to FALSE.
 *
 * If the message queue is found, returns the message queue; otherwise
 * returns NULL. This call is non-blocking.
 */
mqueue_t *
mqname_lookup(const char *name, int *group)
{
	return mqueue_table_lookup(name, group);
}

mqueue_t *
mqueue_handle_lookup(guint *handle, int * group)
{
	mqueue_t *mq;

	if ((mq = g_hash_table_lookup(mqtable_handle_hash, handle)))
		return mq;

	return NULL;
}

static void
dump_mqinfo(const struct mq_info * buf, size_t buflen)
{
	const struct mq_info * pbuf;
	const struct mq_groupinfo * pgbuf;
	int gcount = 0;
	int i, j = 0;

	pbuf = buf;
	dprintf("mq info update: \n");

	while ((const char *)pbuf != ((const char *) buf + buflen)) 
	{
		j++;
		dprintf(" mq_info No. %d: \n", j);
		dprintf("    qname = %s\n", pbuf->qname.value);
		dprintf("    host = %s\n", pbuf->host.value);
		dprintf("    mqstat = %d\n", pbuf->mqstat);
		dprintf("    policy = %d\n", pbuf->policy);
		dprintf("    mq_groupinfo_count = %d\n"
		,	pbuf->mq_groupinfo_count);

		gcount = pbuf->mq_groupinfo_count;
		++pbuf;

		if (gcount > 0) {
			pgbuf = (const struct mq_groupinfo *) pbuf;
			for (i = 0; i < gcount; i++) {
				dprintf("	name = %s\n",
						pgbuf->name.value);
				pgbuf++;
			}
			pbuf = (const struct mq_info *) (&pgbuf);
		}
	}
}

static void
count_group(gpointer key, gpointer value, gpointer user_data)
{
	mqueue_t * mq = (mqueue_t *) value;
	size_t * count = (size_t *) user_data;

	if (mq->list && g_list_length(mq->list)) {
		*count += g_list_length(mq->list);
	}

	return;
}

static void
pack_mqinfo(gpointer key, gpointer value, gpointer mqinfo)
{
	int i;
	mqueue_t * mq = (mqueue_t *) value;
	struct mq_info * info = * ((struct mq_info * *) mqinfo);
	size_t gcount = 0;

	mqueue_t * mqg = NULL;
	struct mq_groupinfo * group;

	info->qname.length = strlen(mq->name) + 1;
	strncpy(info->qname.value, mq->name, SA_MAX_NAME_LENGTH); 

	info->host.length = strlen(mq->host) + 1;
	strncpy(info->host.value, mq->host, SA_MAX_NAME_LENGTH);

	info->mqstat = mq->mqstat;

	info->policy = mq->policy;

	info->mq_groupinfo_count = (mq->list) ? g_list_length(mq->list) : 0;
	if (info->mq_groupinfo_count)
		gcount = info->mq_groupinfo_count;

	info++;
	if (gcount) {
		group = (struct mq_groupinfo *) info;
		for (i = 0; i < gcount; i++) {
			mqg = g_list_nth_data(mq->list, i);

			group->name.length = strlen(mqg->name) + 1;
			strncpy(group->name.value, mqg->name, SA_MAX_NAME_LENGTH);
			group++;
		}
		info = (struct mq_info *) (&group);
	}

	*((struct mq_info * *) mqinfo) = info;

	return;
}

int
mqueue_table_pack(struct mq_info * * mqinfo_buf, size_t * mqinfo_len)
{
	size_t buflen, count, gcount = 0;
	struct mq_info * buf, * pbuf;

	*mqinfo_buf = NULL;
	*mqinfo_len = 0;

	count = g_hash_table_size(mqtable_name_hash);
	g_hash_table_foreach(mqtable_name_hash, count_group, &gcount);

	buflen = sizeof(struct mq_info) * count + 
		sizeof(struct mq_groupinfo) * gcount;

	buf = (struct mq_info *) cl_malloc(buflen);
	pbuf = buf;

	g_hash_table_foreach(mqtable_name_hash, pack_mqinfo, &pbuf);

	dump_mqinfo(buf, buflen);

	*mqinfo_buf = buf;
	*mqinfo_len = buflen;

	return HA_OK;
}

int
mqueue_table_unpack(const struct mq_info * buf, size_t buflen)
{
	const struct mq_info * pbuf;
	const struct mq_groupinfo * pgbuf;
	size_t gcount = 0;
	int i, j = 0;
	mqueue_t * mq, * mqg;
	int group_listing = 0;

	pbuf = buf;
	while ((const char *)pbuf != (const char *) buf + buflen)
	{
		j++;
		dprintf(" mq_info No. %d: \n", j);
		if ((mq = (mqueue_t *) cl_malloc(sizeof(mqueue_t))) == NULL) {
			cl_log(LOG_ERR, "%s: cl_malloc failed for mqueue ",
					__FUNCTION__);
			return HA_FAIL;
		}
		memset(mq, 0, sizeof(mqueue_t));

		dprintf("    qname = %s\n", pbuf->qname.value);
		mq->name = cl_strdup(pbuf->qname.value);

		dprintf("    host = %s\n", pbuf->host.value);
		mq->host = cl_strdup(pbuf->host.value);

		dprintf("    mqstat = %d\n", pbuf->mqstat);
		mq->mqstat = pbuf->mqstat;
		if (mq->mqstat == MQ_STATUS_OPEN) 
			(mq->status).sendingState = SA_MSG_QUEUE_AVAILABLE;
		else 
			(mq->status).sendingState = SA_MSG_QUEUE_UNAVAILABLE;

		dprintf("    policy = %d\n", pbuf->policy);
		mq->policy = pbuf->policy;

		dprintf("    mq_groupinfo_count = %d\n"
		,	pbuf->mq_groupinfo_count);

		gcount = pbuf->mq_groupinfo_count;
		++pbuf;
		if (gcount > 0) {
			group_listing = 1;

			pgbuf = (const struct mq_groupinfo *) pbuf;
			for (i = 0; i < gcount; i++) {
				dprintf("	name = %s\n",
						pgbuf->name.value);
				pgbuf++;
			}
			pbuf = (const struct mq_info *) (&pgbuf);
		}
		mqueue_table_insert(mq);
	}

	if (!group_listing) 
		return HA_OK;

	/* mq_info unpacking is a little bit more complicated.
	   we have to do a two pass thing to get all the group
	   listing right. 

	   Is there any other way of doing this? 
	 */

	pbuf = buf;
	while ((const char *)pbuf != (const char *) buf + buflen)
	{
		gcount = pbuf->mq_groupinfo_count;
		++pbuf;
		if (gcount == 0) {
			continue;
		}

		mqg = mqueue_table_lookup(pbuf->qname.value, NULL);
			
		pgbuf = (const struct mq_groupinfo *) pbuf;
		for (i = 0; i < gcount; i++) {
			dprintf("	name = %s\n",
					pgbuf->name.value);

			mq = mqueue_table_lookup(pgbuf->name.value, NULL);
			if (!mq) {
				cl_log(LOG_ERR, "queue name does not "
					"exists in current local database.");
				continue;
			}

			mqg->list = g_list_append(mqg->list, mq);
			cl_log(LOG_INFO, "Adding mq <%s> to [%s] list"
			,	mq->name, mqg->name);

			pgbuf++;
		}

		if (mqg->current == NULL) 
			mqg->current = g_list_first(mqg->list);

		pbuf = (const struct mq_info *) (&pgbuf);
	}

	return HA_OK;
}

static void
close_mqueue(gpointer key, gpointer value, gpointer user_data)
{
	char * node = (char *)user_data;
	mqueue_t * mq = (mqueue_t *)value;

	dprintf("%s: node is %s\n", __FUNCTION__, node);

	if (strcmp(node, mq->host) == 0) {
		dprintf("close mqueue [%s] on Node [%s]\n", mq->name, node);
		request_mqname_close(mq->name, &cms_data);
	}
}

void
mqueue_close_node(char * node)
{
	g_hash_table_foreach(mqtable_name_hash, close_mqueue, node);
}

void
enqueue_message(mqueue_t * mq, SaUint8T prio, message_t * msg)
{
	dprintf("%s: mq = [%s], priority = %u\n", __FUNCTION__, mq->name, prio);

	mq->message_buffer[prio] = g_list_append(mq->message_buffer[prio], msg);
	mqueue_update_usage(mq, prio, msg->msg.size);
}

message_t *
dequeue_message(mqueue_t * mq)
{
	message_t * message = NULL;
	GList *head, *queue;
	SaUint8T i;

	for (i = SA_MSG_MESSAGE_HIGHEST_PRIORITY
	;	i <= SA_MSG_MESSAGE_LOWEST_PRIORITY
	;	i++) {
		queue = mq->message_buffer[i];

		if (g_list_length(queue)) {
			head = g_list_first(queue);
			mq->message_buffer[i] = g_list_remove_link(queue, head);
			message = (message_t *) head->data;

			g_list_free_1(head);
			mqueue_update_usage(mq, i, -(message->msg.size));

			return message;
		}
		dprintf("%s: mq [%s] priority %u is NULL\n"
		,	__FUNCTION__, mq->name, i);
	}
	return NULL;
}

void
mqueue_update_usage(mqueue_t * mq, int priority, SaSizeT size)
{
	if (priority < 0 || priority > SA_MSG_MESSAGE_LOWEST_PRIORITY) {
		cl_log(LOG_ALERT, "%s: Invalid priority [%d]"
		,	__FUNCTION__, priority);
		return;
	}

	mq->status.saMsgQueueUsage[priority].queueUsed += size;
	mq->status.saMsgQueueUsage[priority].numberOfMessages +=
			size < 0 ?  -1 : 1;

	if (mq->status.saMsgQueueUsage[priority].queueUsed < 0)
		mq->status.saMsgQueueUsage[priority].queueUsed = 0;
	if (mq->status.saMsgQueueUsage[priority].queueUsed > 
			mq->status.saMsgQueueUsage[priority].queueSize)
		mq->status.saMsgQueueUsage[priority].queueUsed =
			mq->status.saMsgQueueUsage[priority].queueSize;
	if (mq->status.saMsgQueueUsage[priority].numberOfMessages < 0)
		mq->status.saMsgQueueUsage[priority].numberOfMessages = 0;

	dprintf("%s: queueUsed [%d], numberOfMessages [%lu]\n"
	,	__FUNCTION__
	,	mq->status.saMsgQueueUsage[priority].queueUsed
	,	mq->status.saMsgQueueUsage[priority].numberOfMessages);
}

int
sa_mqueue_usage_encode(char *size, char *used, char * number,
                       SaMsgQueueUsageT * usage)
{
	char *p = size, *q = used, *r = number;
	int i;

	for (i = 0; i <= SA_MSG_MESSAGE_LOWEST_PRIORITY; i++) {
		if (p) {
			p += sprintf(p, "%lx:", usage[i].queueSize);
		}
		if (q) {
			q += sprintf(q, "%x:", usage[i].queueUsed);
		}
		if (r) {
			r += sprintf(r, "%lx:", usage[i].numberOfMessages);
		}
	}

	return TRUE;
}

int
sa_mqueue_usage_decode(const char *size, const char *used, const char * number,
                       SaMsgQueueUsageT * usage)
{
	const char *p = size, *q = used, *r = number;
	char * c;
	int i;

	for (i = 0; i <= SA_MSG_MESSAGE_LOWEST_PRIORITY; i++) {
		if (p) {
			if ((c = strchr(size, ':'))) {
				usage[i].queueSize = strtoul(p, &c, 16);
				p = c + 1;
			} else
				break;
		}
		if (q) {
			if ((c = strchr(used, ':'))) {
				usage[i].queueUsed = strtoul(q, &c, 16);
				q = c + 1;
			} else
				break;
		}
		if (r) {
			if ((c = strchr(number, ':'))) {
				usage[i].numberOfMessages = strtoul(r, &c, 16);
				r = c + 1;
			} else
				break;
		}
	}

	if (i != SA_MSG_MESSAGE_LOWEST_PRIORITY + 1) {
		cl_log(LOG_ALERT, "%s: Invalid priority [%d]"
		,	__FUNCTION__, i);
		return FALSE;
	}

	return TRUE;
}

void
mqueue_copy_notify_data(gpointer data, gpointer user_data)
{
	SaMsgQueueGroupNotificationT * current;
	notify_buffer_t * buf = (notify_buffer_t *)user_data;

	buf->change_buff = realloc(buf->change_buff
	,	(++(buf->number)) * sizeof(SaMsgQueueGroupNotificationT));

	dprintf("%s: mqname is [%s]\n", __FUNCTION__, ((mqueue_t *)data)->name);
	current = buf->change_buff + buf->number - 1;
	strncpy(current->member.queueName.value, ((mqueue_t *)data)->name
	,	SA_MAX_NAME_LENGTH);
	current->member.queueName.length = strlen(((mqueue_t *)data)->name) + 1;
	current->member.queueStatus = ((mqueue_t *)data)->status;

	if (strcmp(((mqueue_t *)data)->name
	,	buf->changeonly_buff.member.queueName.value) == 0)
		current->change = buf->changeonly_buff.change;
	else
		current->change = SA_MSG_QUEUE_GROUP_NO_CHANGE;
}

