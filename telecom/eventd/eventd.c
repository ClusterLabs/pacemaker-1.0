/* 
 * eventd.c: source file for event daemon
 *
 * Copyright (C) 2004 Forrest,Zhao <forrest.zhao@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <clplumbing/cl_signal.h>
#include "event.h"

int global_debug=0;
int global_verbose=0;

static struct sa_handle_database event_id_database;

GHashTable *hash_table_for_ipc;
GHashTable *hash_table_for_channel_name;

typedef struct {
	SaEvtEventFilterT *filters;
	SaSizeT filtersNumber;
}evt_filter_array;

struct evt_event {
	SaEvtEventIdT event_id; /*globally unique  */
	SaTimeT retention_time;
	SaNameT publisherName;
	SaTimeT publish_time;
	SaUint8T priority;
	SaEvtEventPatternArrayT *pattern_array;
	SaSizeT pattern_size;
	void *event_data;
	SaSizeT data_size;
	SaEvtEventHandleT clt_event_hd;
};

struct evt_subscription {
	SaSizeT filters_size;
	SaEvtEventFilterArrayT *filters;
	IPC_Channel *client;
	SaEvtChannelHandleT clt_ch_handle;
	SaEvtSubscriptionIdT subscription_id;	
	void *ch_id; /*at daemon side */
};

struct evt_new_subscription {
	char *channel_name;
	SaEvtEventFilterArrayT *filters;
	char *orig;
	/*at daemon side */
	/*void *ch_id; */
	SaUint64T ch_id; /*in fact it's a pointer, in order to portable between 32 bit and 64 bit platform, we define it as 64 bit length */
	SaEvtSubscriptionIdT subscription_id;
};

struct evt_new_subscription_reply {
	char *channel_name;
	SaUint64T ch_id; /*at daemon side*/
	SaEvtSubscriptionIdT subscription_id;
	struct evt_event *event;
};

struct channel_instance {
	GHashTable *subscriptions;
	SaEvtChannelHandleT clt_ch_handle;
	char *ch_name;
};

struct evt_channel {
	char *channel_name;
	int unlink;
	GHashTable *channel_instances;
	unsigned int use_count;	
	GHashTable *event_cache;
};

struct ipc {
	IPC_Channel *client;
	GHashTable *channel_instances;
};

struct evt_ch_open_request {
	char *channel_name;
	SaEvtChannelHandleT clt_ch_handle;
	SaTimeT time_out;
	IPC_Channel *client;	
};

struct evt_ch_open {
	char *channel_name;
	SaEvtChannelHandleT clt_ch_handle;
	SaEvtChannelOpenFlagsT ch_open_flags;
	SaTimeT time_out;
};

struct evt_ch_close {
	char *channel_name;
	void *ch_ins;
};

struct evt_retention_clear {
	SaEvtEventIdT event_id;
};

struct evt_retention_clear_reply {
	SaEvtEventIdT event_id;
	SaErrorT ret_code;
};

struct evt_ch_open_request_remote{
	char *channel_name;
	SaUint64T key;
};

struct evt_ch_open_reply_remote{
	char *channel_name;
	SaUint64T key;
};

struct client_msg{
	enum evt_type msg_type;
	char *channel_name;
	union {
		struct evt_event *event;  /*publish*/
		struct evt_subscription *subscription;
		struct evt_ch_open *ch_open;
		struct evt_ch_close *ch_close;
		struct evt_retention_clear *retention_clear;
		struct evt_retention_clear_reply *retention_clear_reply;
		struct evt_new_subscription *new_subscription;
		struct evt_new_subscription_reply *new_sub_reply;
		struct evt_ch_open_request_remote *ch_open_request_remote;
		struct evt_ch_open_reply_remote *ch_open_reply_remote;
	} private;
};

struct node_element {
	uint  NodeUuid;
	char NodeID[NODEIDSIZE];
	char Status[STATUSSIZE];		
};

struct node_list_s {
	uint	   node_count;
	uint	   mynode;
	struct node_element nodes[MAXNODE];
};

struct node_list_s node_list;

#define EVT_SERVICE "event_service"
#define BIN_CONTENT "bin_content"

void hton_64(const SaUint64T *src_64, SaUint64T *dst_64);
void ntoh_64(const SaUint64T *src_64, SaUint64T *dst_64);

static void add_node_l(const char *node,
					const char *status, const char *mynode)
{
	int nodecount, mynode_idx, i, j;
	char value;

	nodecount = node_list.node_count;
	if (nodecount == 0) {
		mynode_idx = -1;
	} else {
		mynode_idx = node_list.mynode;
	}
	for ( i = 0 ; i < nodecount ; i++ ) {
		value = strncmp(node_list.nodes[i].NodeID, node, NODEIDSIZE);
		assert(value!=0);
		if(value > 0) {
			break;
		}
	}
	for ( j = nodecount; j>i; j-- ) {
		node_list.nodes[j] = node_list.nodes[j-1];
		node_list.nodes[j].NodeUuid = j;
	}
	strncpy(node_list.nodes[i].NodeID, node, NODEIDSIZE);
	strncpy(node_list.nodes[i].Status, status, STATUSSIZE);
	node_list.nodes[i].NodeUuid = i;
	node_list.node_count++;
	if (strncmp(mynode, node, NODEIDSIZE) == 0) {
		node_list.mynode = i;
	} else if (mynode_idx != -1 && i <= mynode_idx) {
		node_list.mynode = mynode_idx+1;
	}
	return;
}

static void
LinkStatus(const char * node, const char * lnk, const char * status ,
				void * private)
{
	cl_log(LOG_INFO, "Link Status update: Link %s/%s "
		"now has status %s", node, lnk, status);
	return;
}

struct evt_info {
	ll_cluster_t *hb;
	struct evt_channel *evt_channel_head;	
	GHashTable *evt_pending_ch_open_requests;
	GHashTable *evt_pending_clear_requests;
};

struct evt_info *info;

void *evt_daemon_initialize(void);

void *evt_daemon_initialize()
{
	ll_cluster_t*	hb_fd;
	const char *	node;
	const char *	hname;
	const char *	status;
	unsigned	fmask;

	hb_fd = ll_cluster_new("heartbeat");
	if (hb_fd->llc_ops->signon(hb_fd, "ccm")!= HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		return NULL;
	}
	if((hname = hb_fd->llc_ops->get_mynodeid(hb_fd)) == NULL) {
		cl_log(LOG_ERR, "get_mynodeid() failed");
		return NULL;
	}
	if (hb_fd->llc_ops->set_ifstatus_callback(hb_fd, LinkStatus, NULL)
								!=HA_OK){
		cl_log(LOG_ERR, "Cannot set if status callback");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		return NULL;
	}
	fmask = LLC_FILTER_DEFAULT;
	if (hb_fd->llc_ops->setfmode(hb_fd, fmask) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set filter mode");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		return NULL;
	}
	if (hb_fd->llc_ops->init_nodewalk(hb_fd) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		return NULL;
	}
	while((node = hb_fd->llc_ops->nextnode(hb_fd))!= NULL) {

		/* ignore non normal nodes */
		if(strcmp(hb_fd->llc_ops->node_type(hb_fd, node), "normal") != 0) {
			if(strcmp(node,hname) == 0) {
				cl_log(LOG_ERR, "This cluster node: %s: " "is a ping node",
								node);
				return NULL;
			}
			continue;
		}
		status =  hb_fd->llc_ops->node_status(hb_fd, node);
		if(global_debug) {
			cl_log(LOG_DEBUG, "Cluster node: %s: status: %s", node,	status);
		}
		/* add the node to the node list */
		add_node_l(node, status, hname);
	}
	if (hb_fd->llc_ops->end_nodewalk(hb_fd) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		return NULL;
	}
	info = (struct evt_info *)g_malloc0(sizeof(struct evt_info));
	info->evt_pending_ch_open_requests = g_hash_table_new(g_direct_hash, 
				g_direct_equal);
	info->evt_pending_clear_requests = g_hash_table_new(g_direct_hash, 
				g_direct_equal);

	event_id_database.handle_count = 0;
	return hb_fd;
}

typedef struct hb_usrdata_s {
	ll_cluster_t	*hb_fd;
	GMainLoop	*mainloop;
} hb_usrdata_t;

static void read_publish(void *msg, struct client_msg *ret)
{
	struct evt_event *event;
	SaUint8T *tmp_char, *tmp_char_pattern;
	SaSizeT str_len, number, i;
	SaEvtEventPatternArrayT *pattern_array;
	SaEvtEventPatternT *patterns;

	event = (struct evt_event *)g_malloc(sizeof(struct evt_event));
	
	ret->private.event = event;
	pattern_array = (SaEvtEventPatternArrayT *)g_malloc(
			sizeof(SaEvtEventPatternArrayT));
	event->pattern_array = pattern_array;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	ret->channel_name = g_malloc(str_len+1);
	tmp_char += sizeof(SaSizeT);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(event->clt_event_hd), tmp_char, sizeof(SaEvtEventHandleT));
	tmp_char += sizeof(SaEvtEventHandleT);
	event->priority = *(tmp_char);
	tmp_char++;
	memcpy(&(event->retention_time), tmp_char, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	event->publisherName.length = str_len;
	memcpy(event->publisherName.value, tmp_char, str_len);
	/*event->publisherName[str_len] = '\0';*/
	tmp_char += str_len;
	memcpy(&(event->publish_time), tmp_char, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(&(event->pattern_size), tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(&number, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	pattern_array->patternsNumber = number;
	patterns = (SaEvtEventPatternT *)g_malloc(
			sizeof(SaEvtEventPatternT)*number);
	pattern_array->patterns = patterns;
	tmp_char_pattern = tmp_char + number*sizeof(SaSizeT);
	for(i=0; i<number; i++){
		memcpy(&(patterns[i].patternSize), tmp_char, sizeof(SaSizeT));
		tmp_char += sizeof(SaSizeT);

		patterns[i].pattern = g_malloc(patterns[i].patternSize);
		memcpy(patterns[i].pattern, tmp_char_pattern, patterns[i].patternSize);
		tmp_char_pattern += patterns[i].patternSize;
	}
	tmp_char = tmp_char_pattern;
	memcpy(&(event->data_size), tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	event->event_data = g_malloc(event->data_size);
	memcpy(event->event_data, tmp_char, event->data_size);
	return;
}

static void read_unsubscribe(void *msg, struct client_msg *ret)
{
	struct evt_subscription *subscription;
	SaUint8T *tmp_char;
	SaSizeT str_len;
	
	
	subscription = (struct evt_subscription *)g_malloc(
			sizeof(struct evt_subscription));
	ret->private.subscription = subscription;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(subscription->ch_id), tmp_char, sizeof(void *));
	tmp_char += sizeof(void *);
	memcpy(&(subscription->subscription_id), tmp_char, sizeof(SaEvtSubscriptionIdT));
	return;
}

static void read_open_channel(void *msg, struct client_msg *ret)
{
	SaUint8T *tmp_char;
	SaSizeT str_len;	
	struct evt_ch_open *ch_open;
	
	ch_open = (struct evt_ch_open *)g_malloc(sizeof(struct evt_ch_open));
	ret->private.ch_open = ch_open;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	ret->channel_name = g_malloc(str_len+1);
	ch_open->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	memcpy(ch_open->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	ch_open->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(ch_open->clt_ch_handle), tmp_char, sizeof(SaEvtChannelHandleT));
	tmp_char += sizeof(SaEvtChannelHandleT);
	memcpy(&(ch_open->ch_open_flags), tmp_char, sizeof(SaEvtChannelOpenFlagsT));
	return;
}

static void read_close_channel(void *msg, struct client_msg *ret)
{
	SaUint8T *tmp_char;
	SaSizeT str_len;
	struct evt_ch_close *ch_close;
	
	ch_close = (struct evt_ch_close *)g_malloc(
			sizeof(struct evt_ch_close));
	ret->private.ch_close = ch_close;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(ch_close->ch_ins), tmp_char, sizeof(void *));
	return;
}

static void read_clear_retention_time(void *msg, struct client_msg *ret)
{
	struct evt_retention_clear *retention_clear;
	SaUint8T *tmp_char;
	SaSizeT str_len;
	
	retention_clear = (struct evt_retention_clear *)g_malloc(
			sizeof(struct evt_retention_clear));
	ret->private.retention_clear = retention_clear;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(retention_clear->event_id), tmp_char, sizeof(SaEvtEventIdT));
	return;
}

static void read_subscribe(IPC_Channel *ch, void *msg, struct client_msg *ret)
{
	struct evt_subscription *subscription;
	SaUint8T *tmp_char;
	SaSizeT str_len, number, i;		
	SaEvtEventFilterT *filter;
	
	subscription = (struct evt_subscription *)g_malloc(
			sizeof(struct evt_subscription));
	subscription->filters = (SaEvtEventFilterArrayT *)g_malloc(
			sizeof(SaEvtEventFilterArrayT));
	subscription->client = ch;
	ret->private.subscription = subscription;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(subscription->ch_id), tmp_char, sizeof(void *));
	tmp_char += sizeof(void *);
	memcpy(&(subscription->subscription_id), tmp_char, sizeof(SaEvtSubscriptionIdT));
	tmp_char += sizeof(SaEvtSubscriptionIdT);
	memcpy(&(subscription->filters_size), tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(&number, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	subscription->filters->filtersNumber = number;
	
	filter = (SaEvtEventFilterT *)g_malloc(
			sizeof(SaEvtEventFilterT)*number);
	subscription->filters->filters = filter;

	for(i=0; i<number; i++){

		memcpy(&(filter[i].filterType), tmp_char, sizeof(SaEvtEventFilterTypeT));
		tmp_char += sizeof(SaEvtEventFilterTypeT);

		memcpy(&str_len, tmp_char, sizeof(SaSizeT));
		tmp_char += sizeof(SaSizeT);
		filter[i].filter.patternSize = str_len;
		filter[i].filter.pattern = (SaUint8T *)g_malloc(str_len);
		memcpy(filter[i].filter.pattern, tmp_char, str_len);
		tmp_char += str_len;
	}
	memcpy(&(subscription->clt_ch_handle), tmp_char, sizeof(SaEvtChannelHandleT));
	return;	
}

static void read_unlink_client(void *msg, struct client_msg *ret)
{
	SaUint8T *tmp_char;
	SaSizeT str_len;

	tmp_char = (SaUint8T *)msg;
	tmp_char++;

	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	return;
}

static struct client_msg *evt_read_client_msg(IPC_Channel *ch)
{
	IPC_Message* ipc_msg;
	struct client_msg *ret;
	char *tmp_char;
	
	ret = (struct client_msg *)g_malloc0(sizeof(struct client_msg));
	ch->ops->waitin(ch);
	if(ch->ops->recv(ch, &ipc_msg) != IPC_OK){
		return NULL;	
	}
	tmp_char = (char *)ipc_msg->msg_body;
	ret->msg_type = *(tmp_char);
/*	printf("msg_type == %d\n", (char)ret->msg_type);*/
	switch(*(tmp_char)){
		case EVT_INITIALIZE:
			break;
		case EVT_FINALIZE:
			break;
		case EVT_PUBLISH:
			read_publish(ipc_msg->msg_body, ret);
			break;
		case EVT_SUBSCRIBE:
			read_subscribe(ch, ipc_msg->msg_body, ret);
			break;
		case EVT_UNSUBSCRIBE:
			read_unsubscribe(ipc_msg->msg_body, ret);
			break;
		case EVT_OPEN_EVENT_CHANNEL:
			read_open_channel(ipc_msg->msg_body, ret);
			break;
		case EVT_CLOSE_EVENT_CHANNEL:
			read_close_channel(ipc_msg->msg_body, ret);
			break;
		case EVT_CLEAR_RETENTION_TIME:
			read_clear_retention_time(ipc_msg->msg_body, ret);
			break;
		case EVT_CHANNEL_UNLINK:
			read_unlink_client(ipc_msg->msg_body, ret);
			break;
		default:
			break;			
	}

	return ret;
}

static void free_filters(SaEvtEventFilterArrayT *filters)
{
	SaSizeT filters_number, i;
	filters_number = filters->filtersNumber;
	for(i=0; i<filters_number; i++){
		if((filters->filters+i)->filter.pattern != NULL){
			g_free((filters->filters+i)->filter.pattern);
		}
	}
	g_free(filters->filters);
	g_free(filters);
	return;
}

static void free_subscriptions(gpointer key, gpointer value, gpointer user_data)
{
	struct evt_subscription *sub = (struct evt_subscription *)value;

	free_filters(sub->filters);
	g_free(sub);
	return;
}

static struct evt_channel *find_channel_by_name(SaUint8T *channel_name)
{
	return (struct evt_channel *)g_hash_table_lookup(
			hash_table_for_channel_name, (gpointer)channel_name);
}

static void free_ch_instance(gpointer key, gpointer value, gpointer user_data)
{
	struct channel_instance *ch_instance;
	GHashTable *channel_instances;
	struct evt_channel *evt_ch;

	ch_instance = (struct channel_instance *)value;
	channel_instances = (GHashTable *)user_data;
	evt_ch = find_channel_by_name(ch_instance->ch_name);
	g_hash_table_remove(evt_ch->channel_instances, key);
	if(ch_instance->subscriptions != NULL){
	g_hash_table_foreach(ch_instance->subscriptions,
			free_subscriptions, ch_instance->subscriptions);
	g_hash_table_destroy(ch_instance->subscriptions);
	}
	g_free(ch_instance->ch_name);
	g_free(ch_instance);
	return;
}


static struct evt_ch_open_request *add_pending_ch_open_request(IPC_Channel *client, 
		char *ch_name, 
		struct client_msg *msg)
{
	struct evt_ch_open_request *ch_open_req;

	ch_open_req = (struct evt_ch_open_request *)g_malloc(
			sizeof(struct evt_ch_open_request));
	/*TODO: copy channel name*/
	ch_open_req->clt_ch_handle = msg->private.ch_open->clt_ch_handle;
	ch_open_req->client = client;
	ch_open_req->time_out = msg->private.ch_open->time_out;
	/*TODO: we need compare client+clt_ch_handle+ch_name to determine a key*/
	g_hash_table_insert(info->evt_pending_ch_open_requests, 
			(gpointer)ch_open_req, (gpointer)ch_open_req);
	/*TODO: start a timer*/
	return ch_open_req;	
}

static struct ipc *find_ipc(gpointer key)
{
	struct ipc *ret;

	ret = (struct ipc *)g_hash_table_lookup(hash_table_for_ipc, key);
	return ret;
}

static void send_to_client(struct IPC_CHANNEL *client,
		void *msg, SaSizeT msg_size)
{
	struct IPC_MESSAGE	Msg;

	memset(&Msg, 0, sizeof(Msg));

	Msg.msg_body = msg;
	Msg.msg_len = msg_size;
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = client;
	client->ops->send(client, &Msg);
	return;
}

static int send_open_channel_reply(IPC_Channel *client, 
		struct evt_ch_open *ch_open, 
		struct channel_instance *ch_ins,
		SaErrorT ret_code)
{
	SaSizeT str_len, msg_size;
	void *msg;
	char *tmp_char;
		
	str_len = strlen(ch_open->channel_name);
	msg_size = 1+sizeof(SaSizeT)+str_len+sizeof(SaEvtChannelHandleT)
		+sizeof(void *)+sizeof(SaErrorT);
	msg = g_malloc(msg_size);
	tmp_char = (char *)msg;
	*(tmp_char) = EVT_CH_OPEN_REPLY_FROM_DAEMON;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, ch_open->channel_name, str_len);
	tmp_char += str_len;
	memcpy(tmp_char, &(ch_open->clt_ch_handle), sizeof(SaEvtChannelHandleT));
	tmp_char += sizeof(SaEvtChannelHandleT);
	memcpy(tmp_char, &(ch_ins), sizeof(void *));
	tmp_char += sizeof(void *);
	memcpy(tmp_char, &ret_code, sizeof(SaErrorT));
	tmp_char += sizeof(SaErrorT);
	send_to_client(client, msg, msg_size);	
	return 0;
}

/*determine the byte order of platform*/
/*0 indicate big-endian; 1 indicate little-endian*/
static int byte_order(void)
{
	union{
		short s;
		char c[2];
	} un;

	un.s = 0x0102;
	if((un.c[0] == 1) && (un.c[1] == 2)){
		return 0;
	}else if((un.c[0] == 2) && (un.c[1] == 1)){
		return 1;
	}
	return -1;
}

static void deliver_event_to_local_subscriber(struct evt_subscription *subscription, 
		struct evt_event *event)
{
	SaSizeT number, msg_size, i, size=0, publisher_len;
	SaEvtEventPatternT *pattern;
	SaUint8T *tmp_char;
	void *msg;
	
	/*calculate the size of pattern_array*/
	number = event->pattern_array->patternsNumber;
	pattern = event->pattern_array->patterns;
	for(i=0; i<number; i++){
		size = size + (pattern+i)->patternSize;
	}
	size = size + (number+1)*sizeof(SaSizeT);
	/*calculate the size of publisher_name*/
	publisher_len = event->publisherName.length;
	msg_size = 1+sizeof(SaEvtChannelHandleT)+sizeof(SaEvtSubscriptionIdT)+
		sizeof(SaSizeT)+size+sizeof(SaUint8T)+sizeof(SaTimeT)+
		sizeof(SaSizeT)+publisher_len+sizeof(SaTimeT)+
		sizeof(SaEvtEventIdT)+sizeof(SaSizeT)+event->data_size;
	msg = g_malloc(msg_size);
	/*msg type == EVT_NORMAL_EVENT*/
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_NORMAL_EVENT;
	tmp_char++;

	memcpy(tmp_char, &(subscription->clt_ch_handle), sizeof(SaEvtChannelHandleT));
	tmp_char += sizeof(SaEvtChannelHandleT);
	memcpy(tmp_char, &(subscription->subscription_id), sizeof(SaEvtSubscriptionIdT));
	tmp_char += sizeof(SaEvtSubscriptionIdT);
	memcpy(tmp_char, &size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, &number, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	for(i=0; i<number; i++){
		memcpy(tmp_char, &((pattern+i)->patternSize), sizeof(SaSizeT));
		tmp_char += sizeof(SaSizeT);
	}

	for(i=0; i<number; i++){
		strncpy(tmp_char, (pattern+i)->pattern, 
				(pattern+i)->patternSize);
		tmp_char += (pattern+i)->patternSize;
	}	
	memcpy(tmp_char, &(event->priority), 1);
	tmp_char++;
	memcpy(tmp_char, &(event->retention_time), sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(tmp_char, &publisher_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, event->publisherName.value, publisher_len);
	tmp_char += publisher_len;
	memcpy(tmp_char, &(event->publish_time), sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(tmp_char, &(event->event_id), sizeof(SaEvtEventIdT));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(tmp_char, &(event->data_size), sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, event->event_data, event->data_size);
	send_to_client(subscription->client, msg, msg_size);
	return;
}

static int fliter_match(SaEvtEventPatternT *pattern, SaEvtEventFilterT *filter)
{

	SaSizeT filter_len = filter->filter.patternSize;
	SaSizeT pattern_len = pattern->patternSize;
	SaSizeT i;
	
	switch(filter->filterType){
		case SA_EVT_PREFIX_FILTER:
			if(pattern_len < filter_len){
				return 0;
			}
			for(i=0; i<filter_len; i++){
				if(pattern->pattern[i] == filter->filter.pattern[i]){
					continue;
				}else{
					return 0;
				}
			}
			break;

		case SA_EVT_EXACT_FILTER:
			if(pattern_len != filter_len){
				return 0;
			}
			for(i=0; i<filter_len; i++){
				if(pattern->pattern[i] == filter->filter.pattern[i]){
					continue;
				}else{
					return 0;
				}
			}
			break;

		case SA_EVT_SUFFIX_FILTER:
			if(pattern_len < filter_len){
				return 0;
			}
			for(i=0; i<filter_len; i++){
				if(filter->filter.pattern[filter_len-1-i]
					== pattern->pattern[pattern_len-1-i]){
					continue;
				}else{
					return 0;
				}
			}

			break;
		case SA_EVT_PASS_ALL_FILTER:			
			break;

		default:
			break;
			/*error message*/
	}
	return 1;
}

static int matched(SaEvtEventPatternArrayT *patterns,
					SaEvtEventFilterArrayT *filters)
{
	SaSizeT patterns_num = patterns->patternsNumber;
	SaSizeT filters_num = filters->filtersNumber;
	int i;
	SaSizeT len = (patterns_num < filters_num) ? patterns_num:filters_num;
	
	if(filters_num > patterns_num){
		for(i=len; i<filters_num; i++){
			if(((filters->filters+i)->filterType == SA_EVT_PASS_ALL_FILTER)||
					((filters->filters+i)->filter.patternSize == 0)){
				continue;
			}else{
				return 0;
			}			
		}
	}
	for(i=0; i<len; i++){
		if(fliter_match(patterns->patterns+i, filters->filters+i)){
			continue;
		}else{
			return 0;
		}
	}
	return 1;
}

static void search_subscription(gpointer key,
					gpointer value, gpointer user_data)
{
	struct evt_subscription *subscription;
	struct evt_event *event;
	
	subscription = (struct evt_subscription *)value;
	event = (struct evt_event *)user_data;
	if(matched(event->pattern_array, subscription->filters)){
		deliver_event_to_local_subscriber(subscription, event);
	}
	return;
}

static void search_ch_instance(gpointer key,
					gpointer value, gpointer user_data)
{
	struct channel_instance *ch_ins;
	ch_ins = (struct channel_instance *)value;
	/*user_data is event to be published*/
	g_hash_table_foreach(ch_ins->subscriptions,
			search_subscription,
			user_data);
	return;
}

static void publish_to_local_subscriber(struct evt_channel *evt_ch, 
		SaEvtEventPatternArrayT *pattern_array,
		struct evt_event *event)
{
	g_hash_table_foreach(evt_ch->channel_instances, 
			search_ch_instance, 
			event); 
	return;
}


static SaUint32T get_local_event_id(void)
{

	SaUint32T local_id;
	
	get_handle(&event_id_database, &local_id);
	return local_id;
}

static SaEvtEventIdT get_event_id(SaUint32T local_event_id)
{
	SaUint32T my_node_id;
	SaEvtEventIdT event_id;
	
	my_node_id = node_list.mynode;	
	event_id = my_node_id;
	event_id = (event_id << 32);	
	event_id += local_event_id;
	return event_id;
}

static void broadcast_event_msg_to_cluster(struct client_msg *msg)
{
	struct ha_msg *m;
	SaSizeT msg_len, str_len, publisher_len, number, tmp_size, i;
	SaEvtEventPatternT *pattern;
	void *bin_msg;
	SaUint8T *tmp_char;
	struct evt_event *event = msg->private.event;
	SaTimeT tmp_time;
	SaEvtEventIdT tmp_event_id;

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot broadcast event to cluster");
		return;
	}
	str_len = strlen(msg->channel_name);
	publisher_len = event->publisherName.length;
	msg_len = 1+sizeof(SaSizeT)+str_len+1+sizeof(SaTimeT)+sizeof(SaSizeT)+
				publisher_len+sizeof(SaTimeT)+sizeof(SaSizeT)+
				event->pattern_size+sizeof(SaSizeT)+event->data_size+
				sizeof(SaEvtEventIdT);
	bin_msg = g_malloc(msg_len);
	if(bin_msg == NULL){
		return;
	}
	number = event->pattern_array->patternsNumber;
	pattern = event->pattern_array->patterns;
	tmp_char = (SaUint8T *)bin_msg;
	*(tmp_char) = EVT_EVENT_MSG;
	tmp_char++;

	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, msg->channel_name, str_len);
	tmp_char += str_len;
	*(tmp_char) = event->priority;
	tmp_char++;
	hton_64(&(event->retention_time), &tmp_time);
	memcpy(tmp_char, &tmp_time, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	tmp_size = htonl(publisher_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, event->publisherName.value, publisher_len);
	tmp_char += publisher_len;
	hton_64(&(event->publish_time), &tmp_time);
	memcpy(tmp_char, &tmp_time, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	tmp_size = htonl(event->pattern_size);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	tmp_size = htonl(number);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	for(i=0; i<number; i++){
		tmp_size = htonl((pattern+i)->patternSize);
		memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
		tmp_char += sizeof(SaSizeT);
	}

	for(i=0; i<number; i++){
		memcpy(tmp_char, (pattern+i)->pattern,
				(pattern+i)->patternSize);
		tmp_char += (pattern+i)->patternSize;
	}
	tmp_size = htonl(event->data_size);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	memcpy(tmp_char, event->event_data, event->data_size);
	tmp_char += event->data_size;
	hton_64(&(event->event_id), &tmp_event_id);
	memcpy(tmp_char, &tmp_event_id, sizeof(SaEvtEventIdT));

	if((ha_msg_addbin(m, BIN_CONTENT, bin_msg, msg_len) == HA_FAIL)||
		(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create event");
		g_free(bin_msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendclustermsg(info->hb, m);
	g_free(bin_msg);
	ha_msg_del(m);
	return;
}

static void send_publish_reply_to_client(IPC_Channel *client, 
		struct evt_event *event, 
		SaErrorT ret_code,
		SaEvtEventIdT event_id)
{
	void *msg;
	SaSizeT msg_size;
	char *tmp_char;
		
	msg_size = 1+sizeof(SaEvtEventHandleT)+
				sizeof(SaEvtEventIdT)+sizeof(SaErrorT);
	msg = g_malloc(msg_size);
	tmp_char = msg;
	*(tmp_char) = EVT_PUBLISH_REPLY;
	tmp_char++;
	memcpy(tmp_char, &(event->clt_event_hd), sizeof(SaEvtEventHandleT));
	tmp_char += sizeof(SaEvtEventHandleT);
	memcpy(tmp_char, &event_id, sizeof(SaEvtEventIdT));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(tmp_char, &ret_code, sizeof(SaErrorT));	
	send_to_client(client, msg, msg_size);
	return;
}

static void search_cached_event(gpointer key,
					gpointer value, gpointer user_data)
{
	struct evt_subscription *subscription;
	struct evt_event *event;

	event = (struct evt_event *)value;
	subscription = (struct evt_subscription *)user_data;
	if(matched(event->pattern_array, subscription->filters)){
		deliver_event_to_local_subscriber(subscription, event);
	}
	return;
}

static void send_cached_events_to_client(char *channel_name,
		struct evt_subscription *subscription,
		GHashTable *event_cache)
{
	/*compare the pattern against the filter, deliver event to client if match*/
	g_hash_table_foreach(event_cache,
			search_cached_event,
			subscription);
	return;
}

static SaErrorT append_subscription(SaUint8T *channel_name, 
		struct evt_subscription *subscription)
{
	struct evt_channel *evt_ch;
	struct channel_instance *ch_instance;
	
	evt_ch = find_channel_by_name(channel_name);
	if(evt_ch == NULL){
		return SA_ERR_LIBRARY;
	}
	ch_instance = g_hash_table_lookup(evt_ch->channel_instances, 
			subscription->ch_id);
	g_hash_table_insert(ch_instance->subscriptions, 
			(gpointer)(long)(subscription->subscription_id),
			subscription);
	
	return SA_OK;
}

static void send_retention_clear_reply_to_client(IPC_Channel *client,
				char *channel_name,	SaEvtEventIdT event_id, SaErrorT reply)
{
	SaSizeT str_len, msg_size;
	void *msg;
	char *tmp_char;
			
	str_len = strlen(channel_name);
	msg_size = 1+sizeof(SaSizeT)+str_len+sizeof(SaEvtEventIdT)+
				sizeof(SaErrorT);
	msg = g_malloc(msg_size);
	tmp_char = (char *)msg;
	*(tmp_char) = EVT_CLEAR_RETENTION_TIME_REPLY;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	strncpy(tmp_char, channel_name, str_len);
	tmp_char += str_len;
	memcpy(tmp_char, &event_id, sizeof(SaEvtEventIdT));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(tmp_char, &reply, sizeof(SaErrorT));
	send_to_client(client, msg, msg_size);
	return;
}

static void broadcast_ch_open_req(SaUint8T *channel_name,
		struct evt_ch_open_request *ch_open_req)
{
	SaSizeT str_len, msg_len, tmp_size;
	SaUint64T tmp_key, key;
	SaUint8T *tmp_char;
	void *msg;
	struct ha_msg *m;

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot broadcast open_ch request to cluster");
		return;
	}
	str_len = strlen(channel_name);
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaUint64T);
	msg = g_malloc(msg_len);
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_CH_OPEN_REQUEST;
	tmp_char++;
	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, channel_name, str_len);
	tmp_char += str_len;
	key = (SaUint64T)(long)ch_open_req;
	hton_64(&key, &tmp_key);
	memcpy(tmp_char, &tmp_key, sizeof(SaUint64T));
	
	if((ha_msg_addbin(m, BIN_CONTENT, msg, msg_len) == HA_FAIL) ||
			(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create open_ch request message");
		g_free(msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendclustermsg(info->hb, m);
	g_free(msg);
	ha_msg_del(m);
	return;
}

static void broadcast_new_subscription(SaUint8T *channel_name,
		struct evt_subscription *subscription)
{
	struct ha_msg *m;
	SaSizeT str_len, msg_len, tmp_size, number, i;
	void *msg;
	SaUint8T *tmp_char;
	SaUint64T tmp_64, key;
	SaUint32T tmp_32;
	SaEvtEventFilterT *filter;
	SaEvtEventFilterTypeT tmp_filter_type;

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot broadcast new subscription to cluster");
		return;
	}
	str_len = strlen(channel_name);
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaUint64T)+sizeof(SaUint32T)+
				sizeof(SaSizeT)+subscription->filters_size; 
	msg = g_malloc(msg_len);

	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_NEW_SUBSCRIBE;
	tmp_char++;
	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, channel_name, str_len);
	tmp_char += str_len;
	key = (SaUint64T)(long)(subscription->ch_id);
	hton_64(&key, &tmp_64);
	memcpy(tmp_char, &tmp_64, sizeof(SaUint64T));
	tmp_char += sizeof(SaUint64T);
	tmp_32 = htonl(subscription->subscription_id);
	memcpy(tmp_char, &tmp_32, sizeof(SaUint32T));
	tmp_char += sizeof(SaUint32T);
	tmp_size = htonl(subscription->filters_size);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	number = subscription->filters->filtersNumber;
	tmp_size = htonl(number);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	filter = subscription->filters->filters;
	for(i=0; i<number; i++){

		tmp_filter_type = htonl(filter[i].filterType);
		memcpy(tmp_char, &tmp_filter_type, sizeof(SaEvtEventFilterTypeT));
		tmp_char += sizeof(SaEvtEventFilterTypeT);
		tmp_size = htonl(filter[i].filter.patternSize);
		memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
		tmp_char += sizeof(SaSizeT);
		memcpy(tmp_char, filter[i].filter.pattern,
				filter[i].filter.patternSize);
		tmp_char += filter[i].filter.patternSize;
	}
	if((ha_msg_addbin(m, BIN_CONTENT, msg, msg_len) == HA_FAIL) ||
		(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create open_ch request message");
		g_free(msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendclustermsg(info->hb, m);
	g_free(msg);
	ha_msg_del(m);
	return;
}

struct event_timeout_s{
	GHashTable *event_cache;
	struct evt_event *event;
	guint tag;
};

static void free_event(struct evt_event *event)
{
	SaEvtEventPatternArrayT *pattern_array;
	SaEvtEventPatternT *patterns;
	SaSizeT number, i;

	pattern_array = event->pattern_array;
	patterns = pattern_array->patterns;
	number = pattern_array->patternsNumber;	
	g_free(event->event_data);
	for(i=0; i<number; i++){
		g_free(patterns[i].pattern);
	}
	g_free(patterns);
	g_free(pattern_array);
	put_handle(&event_id_database, event->event_id);
	g_free(event);
	return;
}

static void free_remote_event(struct evt_event *event)
{
	SaEvtEventPatternArrayT *pattern_array;
	SaEvtEventPatternT *patterns;
	SaSizeT number, i;

	pattern_array = event->pattern_array;
	patterns = pattern_array->patterns;
	number = pattern_array->patternsNumber;
	/*g_free(event->publisherName);*/
	g_free(event->event_data);
	for(i=0; i<number; i++){
		g_free(patterns[i].pattern);
	}
	g_free(patterns);
	g_free(pattern_array);	
	g_free(event);
	return;
}

static gboolean timeout_for_retention_time(gpointer user_data)
{
	struct event_timeout_s *event_timeout;
	GHashTable *event_cache;
	struct evt_event *event;
	SaUint32T tmp_32;
	
	event_timeout = (struct event_timeout_s *)user_data;
	event_cache = event_timeout->event_cache;
	event = event_timeout->event;
	tmp_32 = event->event_id;
	g_hash_table_remove(event_cache, (gpointer)(long)(tmp_32));
	free_event(event);
	Gmain_timeout_remove(event_timeout->tag);
	g_free(event_timeout);
	return TRUE;
}

static SaErrorT clear_retention_time(char *channel_name,
									struct client_msg *msg)
{
	struct evt_channel *evt_ch;
	struct evt_event *event;
	SaUint32T tmp_32;
	
	evt_ch = g_hash_table_lookup(hash_table_for_channel_name,
					channel_name);
	tmp_32 = msg->private.retention_clear->event_id;
	
	event = g_hash_table_lookup(evt_ch->event_cache, 
				(gpointer)(long)(tmp_32));
	if(event == NULL){
		return SA_ERR_NOT_EXIST;
	}
	g_hash_table_remove(evt_ch->event_cache, 
				(gpointer)(long)(tmp_32));
	/*the event will be released in timeout*/
	/*free_event(event);*/
	return SA_OK;
}

struct clear_request{
	IPC_Channel *client;
	SaEvtEventIdT event_id;
};

static struct clear_request *append_clear_req(IPC_Channel *client,
		SaEvtEventIdT event_id)
{
	struct clear_request *clear_req;
	SaUint32T tmp_32;

	clear_req = (struct clear_request *)g_malloc(
			sizeof(struct clear_request));
	clear_req->client = client;
	clear_req->event_id = event_id;
	tmp_32 = event_id;
	g_hash_table_insert(info->evt_pending_clear_requests,
			(gpointer)(long)tmp_32, clear_req);
	return clear_req;
}

static void send_clear_to_node(SaUint8T *channel_name,
	SaEvtEventIdT event_id, SaInt32T node_id)
{
	struct ha_msg *m;
	void *msg;
	char *to_id;
	SaSizeT msg_len, str_len, tmp_size;
	SaUint8T *tmp_char;
	SaEvtEventIdT tmp_event_id;
	
	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send clear reply to remote");
		return;
	}
	to_id = node_list.nodes[node_id].NodeID;
	str_len = strlen(channel_name);
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaEvtEventIdT);
	msg = g_malloc(msg_len);
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_RETENTION_CLEAR_REQUEST;
	tmp_char++;

	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, channel_name, str_len);
	tmp_char += str_len;
	hton_64(&(event_id), &tmp_event_id);
	memcpy(tmp_char, &tmp_event_id, sizeof(SaEvtEventIdT));
	
	if((ha_msg_addbin(m, BIN_CONTENT, msg, msg_len) == HA_FAIL) || 
			(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create clear reply message");
		g_free(msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendnodemsg(info->hb, m, to_id);
	g_free(msg);
	ha_msg_del(m);
	return;
}

static void broadcast_unlink(SaUint8T *channel_name)
{
	struct ha_msg *m;
	void *msg;
	SaSizeT str_len, msg_len, tmp_size;
	SaUint8T *tmp_char;
	
	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot broadcast unlink msg to cluster");
		return;
	}
	str_len = strlen(channel_name);
	msg_len = 1+sizeof(SaSizeT)+str_len;
	msg = g_malloc(msg_len);
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_CHANNEL_UNLINK_NOTIFY;
	tmp_char++;
	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, channel_name, str_len);
	if((ha_msg_addbin(m, BIN_CONTENT, msg, msg_len) == HA_FAIL) ||
			(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create unlink msg");
		g_free(msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendclustermsg(info->hb, m);
	g_free(msg);
	ha_msg_del(m);
	return;
}

static int handle_msg_from_client(IPC_Channel *client, gpointer user_data)
{

	struct client_msg *msg;
	char *channel_name = NULL;
	struct evt_channel *evt_ch;
	struct channel_instance *ch_ins;
	struct ipc *evt_ipc;
	struct evt_ch_open_request *ch_open_req;
	struct evt_event *event;
	SaEvtEventPatternArrayT *pattern_array;
	SaErrorT reply;
	SaEvtEventIdT event_id;
	unsigned int node_id;
	struct clear_request *clear_req;
	int str_len;
	struct event_timeout_s *event_timeout;
	SaUint32T local_event_id;
	
	msg = evt_read_client_msg(client);
	if(msg == NULL){
		printf("received NULL msg\n");
		return 0;
	}
	printf("msg_type == %d\n", (char)msg->msg_type);

	channel_name = msg->channel_name;	

	switch(msg->msg_type){
		case EVT_INITIALIZE:
			evt_ipc = (struct ipc *)g_malloc(sizeof(struct ipc));
			evt_ipc->client = client;
			evt_ipc->channel_instances = g_hash_table_new(
					g_direct_hash,
					g_direct_equal);
			g_hash_table_insert(hash_table_for_ipc,
						(gpointer)client,
						(gpointer)evt_ipc);
			printf("after handle initialize!!\n");			
	
			break;

		case EVT_FINALIZE:
			evt_ipc = (struct ipc *)g_hash_table_lookup(
					hash_table_for_ipc,
					(gpointer)client);
			if(evt_ipc == NULL){
				break;
			}
			g_hash_table_remove(hash_table_for_ipc,
					(gpointer)client);
			printf("the hash tabe size == %d\n", g_hash_table_size(evt_ipc->channel_instances));
			if(evt_ipc->channel_instances != NULL){
			g_hash_table_foreach(evt_ipc->channel_instances, 
					free_ch_instance,
					evt_ipc->channel_instances);
			g_hash_table_destroy(evt_ipc->channel_instances);
			}
			g_free(evt_ipc);
			break;

		case EVT_OPEN_EVENT_CHANNEL:
			evt_ch = find_channel_by_name(channel_name);
			if((evt_ch != NULL)&&(evt_ch->unlink = FALSE)){
				ch_ins = (struct channel_instance *)
					g_malloc0(sizeof(struct channel_instance));				
				str_len = strlen(channel_name);
				ch_ins->ch_name = (char *)g_malloc(str_len+1);
				memcpy(ch_ins->ch_name, channel_name, str_len);
				ch_ins->ch_name[str_len] = '\0';
				ch_ins->clt_ch_handle = msg->private.ch_open->clt_ch_handle;
				ch_ins->subscriptions = g_hash_table_new(g_direct_hash, g_direct_equal);
				g_hash_table_insert(evt_ch->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);
				evt_ipc = find_ipc(client);
				g_hash_table_insert(evt_ipc->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);				
				/*sleep(1);*/
				send_open_channel_reply(client, 
					msg->private.ch_open, 
					ch_ins,
					SA_OK);
			}else if((evt_ch != NULL)&&(evt_ch->unlink = TRUE)){
				if((msg->private.ch_open->ch_open_flags & SA_EVT_CHANNEL_CREATE)
					== SA_EVT_CHANNEL_CREATE){
					evt_ch->unlink = FALSE;
					/*be the same as the above brach*/
					ch_ins = (struct channel_instance *)
						g_malloc0(sizeof(struct channel_instance));
					ch_ins->clt_ch_handle = msg->private.ch_open->clt_ch_handle;
					str_len = strlen(channel_name);
					ch_ins->ch_name = (char *)g_malloc(str_len+1);
					memcpy(ch_ins->ch_name, channel_name, str_len);
					ch_ins->ch_name[str_len] = '\0';
					ch_ins->subscriptions = g_hash_table_new(g_direct_hash,
												g_direct_equal);
					g_hash_table_insert(evt_ch->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);
					evt_ipc = find_ipc(client);
					g_hash_table_insert(evt_ipc->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);
					send_open_channel_reply(client, 
							msg->private.ch_open, 
							ch_ins,
							SA_OK);
				}else{
					ch_open_req = add_pending_ch_open_request(
							client, channel_name, msg);
					broadcast_ch_open_req(channel_name,
							ch_open_req);
				}
			}else if((evt_ch == NULL) && ((msg->private.ch_open->ch_open_flags
						 & SA_EVT_CHANNEL_CREATE) == SA_EVT_CHANNEL_CREATE)){
				evt_ch = (struct evt_channel *)g_malloc0(
								sizeof(struct evt_channel));
				str_len = strlen(channel_name);
				evt_ch->channel_name = (char *)g_malloc(str_len+1);
				memcpy(evt_ch->channel_name, channel_name, str_len);
				evt_ch->channel_name[str_len] = '\0';
				evt_ch->channel_instances = g_hash_table_new(g_direct_hash,
												g_direct_equal);
				evt_ch->event_cache = g_hash_table_new(g_direct_hash,
											g_direct_equal);
				evt_ch->use_count = 1;
				g_hash_table_insert(hash_table_for_channel_name,
						(gpointer)evt_ch->channel_name,
						(gpointer)evt_ch);
				ch_ins = (struct channel_instance *)
						g_malloc0(sizeof(struct channel_instance));
				ch_ins->ch_name = (char *)g_malloc(str_len+1);
				memcpy(ch_ins->ch_name, channel_name, str_len);
				ch_ins->ch_name[str_len] = '\0';
				ch_ins->clt_ch_handle = msg->private.ch_open->clt_ch_handle;
				ch_ins->subscriptions = g_hash_table_new(g_direct_hash,
											g_direct_equal);
				g_hash_table_insert(evt_ch->channel_instances,
					(gpointer)ch_ins,
					(gpointer)ch_ins);
				evt_ipc = find_ipc(client);
				g_hash_table_insert(evt_ipc->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);				
				/*sleep(1);*/
				send_open_channel_reply(client, 
					msg->private.ch_open, 
					ch_ins,
					SA_OK);

			}else{
				ch_open_req = add_pending_ch_open_request(client, channel_name,
					       	msg);
				broadcast_ch_open_req(channel_name,
						ch_open_req);
			}
			if(msg->private.ch_open->channel_name != NULL){
				g_free(msg->private.ch_open->channel_name);
			}
			g_free(msg->private.ch_open);
			break;

		case EVT_CLOSE_EVENT_CHANNEL:
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch == NULL){
				/*error msg*/
				return 1;
			}
			ch_ins = g_hash_table_lookup(evt_ch->channel_instances,
				msg->private.ch_close->ch_ins);
			if(ch_ins == NULL){
				/*error msg*/
				return 1;
			}
			if(ch_ins->subscriptions != NULL){
				g_hash_table_foreach(ch_ins->subscriptions,
						free_subscriptions,
						ch_ins->subscriptions);
				g_hash_table_destroy(ch_ins->subscriptions);
			}
			g_hash_table_remove(evt_ch->channel_instances,
						msg->private.ch_close->ch_ins);
			evt_ipc = (struct ipc *)g_hash_table_lookup(
						hash_table_for_ipc,
						(gpointer)client);
			g_hash_table_remove(evt_ipc->channel_instances,
					(gpointer)msg->private.ch_close->ch_ins);
			g_free(ch_ins);
			g_free(msg->private.ch_close);
			break;

		case EVT_PUBLISH:
			/*1 forward the event to local subscriber*/
			/*2 broadcast the event to cluster*/
			event = msg->private.event;
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch == NULL){
				send_publish_reply_to_client(client, 
						event, 
						SA_ERR_LIBRARY, 0);
				return 1;
			}
			local_event_id = get_local_event_id();
			event_id = get_event_id(local_event_id);
			printf("the event_id == %Ld\n", event_id);
			send_publish_reply_to_client(client, event,
					SA_OK, event_id);
			/*sleep(1);*/
			event->event_id = event_id;
			pattern_array = msg->private.event->pattern_array;
			publish_to_local_subscriber(evt_ch,
					pattern_array, event);			
			
			broadcast_event_msg_to_cluster(msg);
			if(event->retention_time != 0){
				evt_ch = g_hash_table_lookup(
						hash_table_for_channel_name,
						channel_name);

				g_hash_table_insert(evt_ch->event_cache,
						(gpointer)local_event_id,
						(gpointer)event);
				
				/*TODO: should start timer in order to remove event when timeout*/
				event_timeout = (struct event_timeout_s *)g_malloc(
									sizeof(struct event_timeout_s));
				event_timeout->event_cache = evt_ch->event_cache;
				event_timeout->event = event;
				event_timeout->tag = Gmain_timeout_add_full(G_PRIORITY_HIGH,
										event->retention_time,
										timeout_for_retention_time,
										event_timeout, NULL);				
				
			}else{
				/* free event*/
				free_event(event);
			}
			break;

		case EVT_SUBSCRIBE:			
			/*1 record the subscription*/
			/*2 send the events within the retention time to client			*/
			
			append_subscription(channel_name, 
					msg->private.subscription);
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch->event_cache != NULL){
				/*sleep(1);*/
				send_cached_events_to_client(channel_name,
						msg->private.subscription,
						evt_ch->event_cache);
			}			
			broadcast_new_subscription(channel_name,
							msg->private.subscription);
			break;
		case EVT_UNSUBSCRIBE:
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch == NULL){
				cl_log(LOG_ERR, "SA_ERR_LIBRARY");
				g_free(msg->private.subscription);
				break;
			}
			ch_ins = g_hash_table_lookup(evt_ch->channel_instances,
						msg->private.subscription->ch_id);
			if(ch_ins == NULL){
				cl_log(LOG_ERR, "SA_ERR_LIBRARY");
				g_free(msg->private.subscription);
				break;
			}
			g_hash_table_remove(ch_ins->subscriptions,
				(gpointer)(long)(msg->private.subscription->subscription_id));
			g_free(msg->private.subscription);
			break;

		case EVT_CLEAR_RETENTION_TIME: 
			event_id = msg->private.retention_clear->event_id;
			node_id =  (event_id >> 32);
			if(node_id == node_list.mynode){
				reply = clear_retention_time(channel_name,
						msg);
				send_retention_clear_reply_to_client(client,
					channel_name,
					msg->private.retention_clear->event_id,
					reply);
			}else{				
				send_clear_to_node(channel_name,
						event_id, node_id);
				clear_req = append_clear_req(client, event_id);
				/*TODO: timeout function associated with clear*/
			}
			g_free(msg->private.retention_clear);
			break;

		case EVT_CHANNEL_UNLINK:
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch != NULL){
				evt_ch->unlink = TRUE;
			}
			broadcast_unlink(channel_name);			
			break;

		default:
			break;
	}
	if(channel_name != NULL){
		g_free(channel_name);
	}
	g_free(msg);
	return 0;
}



void hton_64(const SaUint64T *src_64, SaUint64T *dst_64)
{
	SaUint32T high_value, low_value, *tmp_32;
	const SaUint32T *tmp_32_const;

	if(byte_order() == 0){
		tmp_32_const = (const SaUint32T *)src_64;
		high_value = *tmp_32_const;
		tmp_32_const++;
		low_value = *tmp_32_const;
		tmp_32 = (SaUint32T *)dst_64;	
		*(tmp_32) = htonl(low_value);
		tmp_32++;
		*(tmp_32) = htonl(high_value);
	}
	return;
}


void ntoh_64(const SaUint64T *src_64, SaUint64T *dst_64)
{
	SaUint32T high_value, low_value, *tmp_32;
	const SaUint32T *tmp_32_const;

	if(byte_order() == 0){
		tmp_32_const = (const SaUint32T *)src_64;
		low_value = ntohl(*(tmp_32_const));
		tmp_32_const++;
		high_value = ntohl(*(tmp_32_const));
		tmp_32 = (SaUint32T *)dst_64;
		*tmp_32 = high_value;
		tmp_32++;
		*tmp_32 = low_value;		
	}
	return;
}

static void read_ch_open_reply(const void *bin_msg, struct client_msg *ret)
{
	struct evt_ch_open_reply_remote *ch_open_reply;
	const SaUint8T *tmp_char;
	SaSizeT str_len;
	SaSizeT tmp_size;
	SaUint64T tmp_key;

	ch_open_reply = (struct evt_ch_open_reply_remote *)g_malloc0(
			sizeof(struct evt_ch_open_reply_remote));
	ret->private.ch_open_reply_remote = ch_open_reply;
	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);

	ret->channel_name = g_malloc(str_len+1);
	ch_open_reply->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	memcpy(ch_open_reply->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	ch_open_reply->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&tmp_key, tmp_char, sizeof(SaUint64T));
	ntoh_64(&tmp_key, &(ch_open_reply->key));
	return;
}

static void read_ch_open_request_remote(const void *bin_msg,
							struct client_msg *ret)
{
	struct evt_ch_open_request_remote *ch_open_request;
	const SaUint8T *tmp_char;
	SaSizeT str_len;
	SaSizeT tmp_size;
	SaUint64T tmp_key;
	
	ch_open_request = (struct evt_ch_open_request_remote *)g_malloc0(
			sizeof(struct evt_ch_open_request_remote));
	ret->private.ch_open_request_remote = ch_open_request;
	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);

	ret->channel_name = g_malloc(str_len+1);
	ch_open_request->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	memcpy(ch_open_request->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	ch_open_request->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&tmp_key, tmp_char, sizeof(SaUint64T));
	ntoh_64(&tmp_key, &(ch_open_request->key));
	return;	
}

static void read_event_msg(const void *bin_msg, struct client_msg *ret)
{
	struct evt_event *event;
	SaEvtEventPatternArrayT *pattern_array;
	const SaUint8T *tmp_char, *tmp_char_pattern;
	SaSizeT str_len, number, i;
	SaSizeT tmp_size;
	SaEvtEventPatternT *patterns;
	SaEvtEventIdT tmp_event_id;
	SaTimeT tmp_time;

	event = (struct evt_event *)g_malloc(sizeof(struct evt_event));
	ret->private.event = event;
	pattern_array = (SaEvtEventPatternArrayT *)g_malloc(
			sizeof(SaEvtEventPatternArrayT));
	event->pattern_array = pattern_array;
	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	event->priority = *(tmp_char);
	tmp_char++;
	memcpy(&tmp_time, tmp_char, sizeof(SaTimeT));
	ntoh_64(&tmp_time, &(event->retention_time));
	tmp_char += sizeof(SaTimeT);
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);

	event->publisherName.length = str_len;
	memcpy(event->publisherName.value, tmp_char, str_len);
	tmp_char += str_len;
	memcpy(&tmp_time, tmp_char, sizeof(SaTimeT));
	ntoh_64(&tmp_time, &(event->publish_time));
	tmp_char += sizeof(SaTimeT);
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	event->pattern_size = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	number = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	pattern_array->patternsNumber = number;
	patterns = (SaEvtEventPatternT *)g_malloc(
			sizeof(SaEvtEventPatternT)*number);
	pattern_array->patterns = patterns;
	tmp_char_pattern = tmp_char + number*sizeof(SaSizeT);
	for(i=0; i<number; i++){

		memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
		patterns[i].patternSize = ntohl(tmp_size);
		tmp_char += sizeof(SaSizeT);

		patterns[i].pattern = g_malloc(patterns[i].patternSize);
		memcpy(patterns[i].pattern, tmp_char_pattern, patterns[i].patternSize);
		tmp_char_pattern += patterns[i].patternSize;
	}

	tmp_char = tmp_char_pattern;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	event->data_size = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	event->event_data = g_malloc(event->data_size);
	memcpy(event->event_data, tmp_char, event->data_size);
	tmp_char += event->data_size;
	memcpy(&tmp_event_id, tmp_char, sizeof(SaEvtEventIdT));
	ntoh_64(&tmp_event_id, &(event->event_id));
	return;
}

static void read_unlink(const void *bin_msg, struct client_msg *ret)
{
	const SaUint8T *tmp_char;
	SaSizeT str_len;
	SaSizeT tmp_size;
	
	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	return;
}

static void read_new_subscribe(const void *bin_msg, struct client_msg *ret)
{
	const SaUint8T *tmp_char;
	SaSizeT str_len, number, i;
	SaSizeT tmp_size;
	SaUint64T tmp_64;
	struct evt_new_subscription *new_sub;
	SaUint32T tmp_32;
	SaEvtEventFilterT *filter;
	SaEvtEventFilterTypeT tmp_filter_type;
	
	new_sub = (struct evt_new_subscription *)g_malloc(
					sizeof(struct evt_new_subscription));
	ret->private.new_subscription = new_sub;
	new_sub->filters = (SaEvtEventFilterArrayT *)g_malloc(
			sizeof(SaEvtEventFilterArrayT));
	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	new_sub->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	memcpy(new_sub->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	new_sub->channel_name[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&tmp_64, tmp_char, sizeof(SaUint64T));
	ntoh_64(&tmp_64, &(new_sub->ch_id));
	tmp_char += sizeof(SaUint64T);
	memcpy(&tmp_32, tmp_char, sizeof(SaUint32T));
	new_sub->subscription_id = ntohl(tmp_32);
	tmp_char += sizeof(SaUint32T);
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	number = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);

	new_sub->filters->filtersNumber = number;
	filter = (SaEvtEventFilterT *)g_malloc(
			sizeof(SaEvtEventFilterT)*number);
	new_sub->filters->filters = filter;

	for(i=0; i<number; i++){

		memcpy(&tmp_filter_type, tmp_char, sizeof(SaEvtEventFilterTypeT));
		filter[i].filterType = ntohl(tmp_filter_type);
		tmp_char += sizeof(SaEvtEventFilterTypeT);
		memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
		str_len = ntohl(tmp_size);
		tmp_char += sizeof(SaSizeT);

		filter[i].filter.patternSize = str_len;
		filter[i].filter.pattern = (SaUint8T *)g_malloc(str_len);
		memcpy(filter[i].filter.pattern, tmp_char, str_len);
		tmp_char += str_len;
	}
	return;
}

static void read_new_sub_reply(const void *bin_msg, struct client_msg *ret)
{
	const SaUint8T *tmp_char, *tmp_char_pattern;
	SaSizeT str_len, number, i, publisher_len;
	SaSizeT tmp_size;
	struct evt_new_subscription_reply *new_sub_reply;
	SaUint64T tmp_64;
	SaUint32T tmp_32;
	struct evt_event *event;
	SaEvtEventPatternArrayT *pattern_array;
	SaEvtEventPatternT *patterns;
	SaTimeT tmp_time;
	SaEvtEventIdT tmp_evt_id;

	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;	
	new_sub_reply = (struct evt_new_subscription_reply *)g_malloc(
		sizeof(struct evt_new_subscription_reply));
	if(new_sub_reply == NULL){
		return;
	}
	ret->private.new_sub_reply = new_sub_reply;
	event = (struct evt_event *)g_malloc(sizeof(struct evt_event));
	new_sub_reply->event = event;
	pattern_array = (SaEvtEventPatternArrayT *)g_malloc(
			sizeof(SaEvtEventPatternArrayT));
	event->pattern_array = pattern_array;
	memcpy(&tmp_64, tmp_char, sizeof(SaUint64T));
	ntoh_64(&tmp_64, &(new_sub_reply->ch_id));
	tmp_char += sizeof(SaUint64T);
	memcpy(&tmp_32, tmp_char, sizeof(SaUint32T));
	new_sub_reply->subscription_id = ntohl(tmp_32);
	tmp_char += sizeof(SaUint32T);
	tmp_char += sizeof(SaSizeT);
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	number = htonl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	pattern_array->patternsNumber = number;
	patterns = (SaEvtEventPatternT *)g_malloc(
			sizeof(SaEvtEventPatternT)*number);
	pattern_array->patterns = patterns;
	tmp_char_pattern = tmp_char + number*sizeof(SaSizeT);

	for(i=0; i<number; i++){
/*		patterns[i].patternSize = ntohl(*(tmp_size));*/
/*		tmp_size++;*/
		memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
		patterns[i].patternSize = ntohl(tmp_size);
		tmp_char += sizeof(SaSizeT);

		patterns[i].pattern = g_malloc(patterns[i].patternSize);
		memcpy(patterns[i].pattern, tmp_char_pattern, patterns[i].patternSize);
		tmp_char_pattern += patterns[i].patternSize;
	}
	tmp_char = tmp_char_pattern;
	event->priority = *(tmp_char);
	tmp_char++;
	memcpy(&tmp_time, tmp_char, sizeof(SaTimeT));
	ntoh_64(&tmp_time, &(event->retention_time));
	tmp_char += sizeof(SaTimeT);
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	publisher_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	event->publisherName.length = publisher_len;
	memcpy(event->publisherName.value, tmp_char, publisher_len);
	tmp_char += publisher_len;
	memcpy(&tmp_time, tmp_char, sizeof(SaTimeT));
	ntoh_64(&tmp_time, &(event->publish_time));
	tmp_char += sizeof(SaTimeT);
	memcpy(&tmp_evt_id, tmp_char, sizeof(SaEvtEventIdT));
	ntoh_64(&tmp_evt_id, &(event->event_id));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	event->data_size = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	memcpy(event->event_data, tmp_char, event->data_size);
	return;
}

static void read_clear_req(const void *bin_msg, struct client_msg *ret)
{
	const SaUint8T *tmp_char;
	SaSizeT tmp_size;
	SaSizeT str_len;
	struct evt_retention_clear *clear_req;
	SaEvtEventIdT tmp_event_id;

	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	clear_req = (struct evt_retention_clear *)g_malloc(
			sizeof(struct evt_retention_clear));
	ret->private.retention_clear = clear_req;
	memcpy(&tmp_event_id, tmp_char, sizeof(SaEvtEventIdT));
	ntoh_64(&tmp_event_id, &(clear_req->event_id));
	return;
}

static void read_clear_reply_remote(const void *bin_msg,
								struct client_msg *ret)
{
	const SaUint8T *tmp_char;
	SaSizeT tmp_size;
	SaSizeT str_len;
	struct evt_retention_clear_reply *clear_reply_remote;
	SaEvtEventIdT tmp_event_id;
	SaErrorT tmp_err;

	tmp_char = (const SaUint8T *)bin_msg;
	tmp_char++;
	memcpy(&tmp_size, tmp_char, sizeof(SaSizeT));
	str_len = ntohl(tmp_size);
	tmp_char += sizeof(SaSizeT);
	ret->channel_name = g_malloc(str_len+1);
	memcpy(ret->channel_name, tmp_char, str_len);
	ret->channel_name[str_len] = '\0';
	tmp_char += str_len;
	clear_reply_remote = (struct evt_retention_clear_reply *)g_malloc(
						sizeof(struct evt_retention_clear_reply));
	ret->private.retention_clear_reply = clear_reply_remote;
	memcpy(&tmp_event_id, tmp_char, sizeof(SaEvtEventIdT));
	ntoh_64(&tmp_event_id, &(clear_reply_remote->event_id));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(&tmp_err, tmp_char, sizeof(SaErrorT));
	clear_reply_remote->ret_code = ntohl(tmp_err);
	return;
}

static struct client_msg *evt_read_hb_msg(struct ha_msg *m)
{	
	size_t			msg_len;
	const void*		bin_msg;
	const SaUint8T*		tmp_char;
	struct client_msg*	ret;
	
	ret = (struct client_msg *)g_malloc0(sizeof(struct client_msg));
	if(ret == NULL){
		return NULL;
	}
	bin_msg = cl_get_binary(m, BIN_CONTENT, &msg_len);
	tmp_char = (const SaUint8T *)bin_msg;
	ret->msg_type = *(tmp_char);
	
	switch(*(tmp_char)){
		case EVT_CH_OPEN_REQUEST:
			read_ch_open_request_remote(bin_msg, ret);
			break;
		case EVT_CH_OPEN_REPLY:
			read_ch_open_reply(bin_msg, ret);
			break;
		case EVT_EVENT_MSG:
			read_event_msg(bin_msg, ret);
			break;
		case EVT_CHANNEL_UNLINK_NOTIFY:
			read_unlink(bin_msg, ret);
			break;
		case EVT_NEW_SUBSCRIBE:
			read_new_subscribe(bin_msg, ret);
			break;
		case EVT_NEW_SUBSCRIBE_REPLY:
			read_new_sub_reply(bin_msg, ret);
			break;
		case EVT_RETENTION_CLEAR_REQUEST:
			read_clear_req(bin_msg, ret);
			break;
		case EVT_RETENTION_CLEAR_REPLY:
			read_clear_reply_remote(bin_msg, ret);
			break;
	}
	return ret;
}

static void evt_send_open_reply(const char *orig,
					char *channel_name, SaUint64T key)
{
	SaSizeT str_len, msg_len, tmp_size;
	SaUint64T tmp_key;
	SaUint8T *tmp_char;
	void *msg;
	struct ha_msg *m;

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send open_ch reply to remote");
		return;
	}
	str_len = strlen(channel_name);
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaUint64T);
	msg = g_malloc(msg_len);
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_CH_OPEN_REPLY;
	tmp_char++;
	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, channel_name, str_len);
	tmp_char += str_len;
	hton_64(&key, &tmp_key);
	memcpy(tmp_char, &tmp_key, sizeof(SaUint64T));

	if((ha_msg_addbin(m, BIN_CONTENT, msg, msg_len) == HA_FAIL) ||
			(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create open_ch reply to remote message");
		g_free(msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendnodemsg(info->hb, m, orig);
	g_free(msg);
	ha_msg_del(m);
	return;
}

static void deliver_event_to_remote_subscriber(
		struct evt_new_subscription *new_sub, struct evt_event *event)
{
	struct ha_msg *m;
	SaSizeT str_len, publisher_len, msg_len, tmp_size, number, i;
	void *msg;
	SaUint8T *tmp_char;
	SaUint64T tmp_64;
	SaUint32T tmp_32;
	SaEvtEventPatternT *pattern;
	SaTimeT tmp_time;
	SaEvtEventIdT tmp_evt_id;

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot deliver event to remote");
		return;
	}
	str_len = strlen(new_sub->channel_name);
	publisher_len = event->publisherName.length;
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaUint64T)+sizeof(SaUint32T)+
				sizeof(SaSizeT)+event->pattern_size+1+sizeof(SaTimeT)+
				sizeof(SaSizeT)+publisher_len+sizeof(SaTimeT)+
				sizeof(SaEvtEventIdT)+sizeof(SaSizeT)+event->data_size;
	msg = g_malloc(msg_len);
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_NEW_SUBSCRIBE_REPLY;
	tmp_char++;
	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, new_sub->channel_name, str_len);
	tmp_char += str_len;
	hton_64(&(new_sub->ch_id), &tmp_64);
	memcpy(tmp_char, &tmp_64, sizeof(SaUint64T));
	tmp_char += sizeof(SaUint64T);
	tmp_32 = htonl(new_sub->subscription_id);
	memcpy(tmp_char, &tmp_32, sizeof(SaUint32T));
	tmp_char += sizeof(SaUint32T);

	number = event->pattern_array->patternsNumber;
	pattern = event->pattern_array->patterns;
	tmp_size = htonl(event->pattern_size);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	tmp_size = htonl(number);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	
	for(i=0; i<number; i++){

		tmp_size = htonl((pattern+i)->patternSize);
		memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
		tmp_char += sizeof(SaSizeT);
	}

	for(i=0; i<number; i++){
		memcpy(tmp_char, (pattern+i)->pattern, 
				(pattern+i)->patternSize);
		tmp_char += (pattern+i)->patternSize;
	}
	*(tmp_char) = event->priority;
	tmp_char++;
	hton_64(&(event->retention_time), &tmp_time);
	memcpy(tmp_char, &tmp_time, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	tmp_size = htonl(publisher_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, event->publisherName.value, publisher_len);
	tmp_char += publisher_len;
	hton_64(&(event->publish_time), &tmp_time);
	memcpy(tmp_char, &tmp_time, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	hton_64(&(event->event_id), &tmp_evt_id);
	memcpy(tmp_char, &tmp_evt_id, sizeof(SaEvtEventIdT));
	tmp_char += sizeof(SaEvtEventIdT);
	tmp_size = htonl(event->data_size);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);

	memcpy(tmp_char, event->event_data, event->data_size);
	if((ha_msg_addbin(m, BIN_CONTENT, msg, msg_len) == HA_FAIL) ||
		(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create event to remote");
		g_free(msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendnodemsg(info->hb, m, new_sub->orig);
	g_free(msg);
	ha_msg_del(m);
	return;

}

static void search_cached_event_for_new_sub(gpointer key,
		gpointer value,
		gpointer user_data)
{
	struct evt_new_subscription *new_sub;
	struct evt_event *event;

	event = (struct evt_event *)value;
	new_sub = (struct evt_new_subscription *)user_data;
	if(matched(event->pattern_array, new_sub->filters)){
		deliver_event_to_remote_subscriber(new_sub, event);
	}
	return;
}

static void send_cached_events_to_new_subscriber(const char *orig,
				struct evt_new_subscription *new_sub,
				GHashTable *event_cache)
{
	SaSizeT str_len;

	str_len = strlen(orig);
	new_sub->orig = g_malloc(str_len+1);
	memcpy(new_sub->orig, orig, str_len);
	new_sub->orig[str_len] = '\0';
	g_hash_table_foreach(event_cache,
			search_cached_event_for_new_sub,
			new_sub);
	return;
}

static void send_retention_clear_reply(const char *orig,
				SaEvtEventIdT event_id,	SaErrorT ret_code,
				SaUint8T *channel_name)
{
	struct ha_msg *m;
	SaSizeT str_len, msg_len, tmp_size;
	void *msg;
	SaUint8T *tmp_char;
	SaUint64T tmp_64;
	SaErrorT tmp_err;

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send retention clear reply to remote");
		return;
	}
	str_len = strlen(channel_name);
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaUint64T)+sizeof(SaErrorT);
	msg = g_malloc(msg_len);
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_RETENTION_CLEAR_REPLY;
	tmp_char++;
	tmp_size = htonl(str_len);
	memcpy(tmp_char, &tmp_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, channel_name, str_len);
	tmp_char += str_len;
	hton_64(&event_id, &tmp_64);
	memcpy(tmp_char, &tmp_64, sizeof(SaUint64T));
	tmp_char += sizeof(SaUint64T);
	tmp_err = htonl(ret_code);
	memcpy(tmp_char, &tmp_err, sizeof(SaErrorT));

	if((ha_msg_addbin(m, BIN_CONTENT, msg, msg_len) == HA_FAIL) ||
		(ha_msg_add(m, F_TYPE, EVT_SERVICE) == HA_FAIL)){
		cl_log(LOG_ERR, "Cannot create retention clear reply to remote");
		g_free(msg);
		ha_msg_del(m);
		return;
	}
	info->hb->llc_ops->sendnodemsg(info->hb, m, orig);
	g_free(msg);
	ha_msg_del(m);
	return;
	
}

static int handle_msg_from_hb(ll_cluster_t *hb)
{
	struct ha_msg *m;
	struct client_msg *msg;
	const char *type, *orig;
	char *channel_name;
	char *my_node_id;
	enum evt_type evt_msg_type;
	SaEvtEventPatternArrayT *pattern_array;
	struct evt_ch_open_request *ch_open_req;
	struct channel_instance *ch_instance;
	struct channel_instance *ch_ins;
	SaSizeT str_len = 0;
	struct evt_subscription *subscription;
	struct evt_channel *evt_ch;
	struct evt_event *event;
	struct evt_retention_clear *clear_req;
	SaEvtEventIdT event_id;
	unsigned int node_id;
	struct evt_retention_clear_reply *clear_reply;
	struct clear_request *clear_requ;
	SaErrorT ret_code;
	IPC_Channel *client = NULL;
	struct ipc *evt_ipc;
	SaUint32T tmp_32;

	m = hb->llc_ops->readmsg(hb, 0);
	if(m == NULL){
		return 0;
	}
	orig = ha_msg_value(m, F_ORIG);
	my_node_id = node_list.nodes[node_list.mynode].NodeID;
	if(strcmp(orig, my_node_id) == 0){
		return 0;
	}
	type = ha_msg_value(m, F_TYPE);
	if(type != NULL){
		printf("the msg type is: %s \n", type);
	}
	if((type == NULL) || (strncmp(type, EVT_SERVICE, 13) != 0)){
		ha_msg_del(m);
		return 0;
	}
	printf("event daemon received msg from hb\n");
	/*retrieve message type*/
	msg = evt_read_hb_msg(m);
	if(msg == NULL){
		return 0;
	}
	
	evt_msg_type = msg->msg_type;
	
	/*retrieve channel name*/
	channel_name = msg->channel_name;
	printf("the type of msg from hb:%d\n", evt_msg_type);
	switch(evt_msg_type) {
	 
		case EVT_EVENT_MSG:
			/*if found matched filters, forward the event to client	*/
			event = msg->private.event;
			pattern_array = event->pattern_array;			
			evt_ch = find_channel_by_name(channel_name);
			if((evt_ch != NULL) && (evt_ch->channel_instances != NULL)){
				g_hash_table_foreach(evt_ch->channel_instances,
					search_ch_instance,
					event);
			}			
			free_remote_event(event);
			break;

		case EVT_CH_OPEN_REQUEST:
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch && (evt_ch->unlink = FALSE)){
				orig = ha_msg_value(m, F_ORIG);
				evt_send_open_reply(orig,
					channel_name,
					msg->private.ch_open_request_remote->key);
			}
			g_free(msg->private.ch_open_request_remote);
			break;
		case EVT_CH_OPEN_REPLY:

			ch_open_req = g_hash_table_lookup(
					info->evt_pending_ch_open_requests,
					(gpointer)(long)(msg->private.ch_open_reply_remote->key));

			if(ch_open_req != NULL){
				
				evt_ch = find_channel_by_name(channel_name);
				if(evt_ch == NULL){
					evt_ch = (struct evt_channel *)g_malloc0(
								sizeof(struct evt_channel));
					str_len = strlen(channel_name);
					evt_ch->channel_name = (char *)g_malloc(str_len+1);
					memcpy(evt_ch->channel_name, channel_name, str_len);
					evt_ch->channel_name[str_len] = '\0';
					evt_ch->channel_instances = g_hash_table_new(g_direct_hash,
													g_direct_equal);
					evt_ch->event_cache = g_hash_table_new(g_direct_hash,
											g_direct_equal);
					evt_ch->use_count = 1;
					evt_ch->unlink = FALSE;
					g_hash_table_insert(hash_table_for_channel_name,
						(gpointer)evt_ch->channel_name,
						(gpointer)evt_ch);
					ch_ins = (struct channel_instance *)
						g_malloc0(sizeof(struct channel_instance));
					ch_ins->ch_name = (char *)g_malloc(str_len+1);
					memcpy(ch_ins->ch_name, channel_name, str_len);
					ch_ins->ch_name[str_len] = '\0';
					ch_ins->clt_ch_handle = ch_open_req->clt_ch_handle;
					ch_ins->subscriptions = g_hash_table_new(g_direct_hash,
												g_direct_equal);
				
					g_hash_table_insert(evt_ch->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);
					client = ch_open_req->client;
					evt_ipc = find_ipc(client);
					g_hash_table_insert(evt_ipc->channel_instances,
							(gpointer)ch_ins,
							(gpointer)ch_ins);
				
					/*sleep(1);*/
					send_open_channel_reply(client, 
						msg->private.ch_open, 
						ch_ins,
						SA_OK);
				}else{
					ch_ins = (struct channel_instance *)
						g_malloc0(sizeof(struct channel_instance));
					ch_ins->ch_name = (char *)g_malloc(str_len+1);
					memcpy(ch_ins->ch_name, channel_name, str_len);
					ch_ins->ch_name[str_len] = '\0';
					ch_ins->clt_ch_handle = ch_open_req->clt_ch_handle;
					ch_ins->subscriptions = g_hash_table_new(g_direct_hash,
												g_direct_equal);
					evt_ch->use_count++;
					evt_ch->unlink = FALSE;
					g_hash_table_insert(evt_ch->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);
					client = ch_open_req->client;
					evt_ipc = find_ipc(client);
					g_hash_table_insert(evt_ipc->channel_instances,
						(gpointer)ch_ins,
						(gpointer)ch_ins);
				
					/*sleep(1);*/
					send_open_channel_reply(client, 
						msg->private.ch_open, 
						ch_ins,
						SA_OK);
				}
				/*TODO: free ch_open_request, remove it from hash table*/
				/*remove_pending_ch_open_request(key);*/
			}else{
				/*log or print error message*/
			}
			if(msg->private.ch_open_reply_remote->channel_name != NULL){
				g_free(msg->private.ch_open_reply_remote->channel_name);
			}
			g_free(msg->private.ch_open_reply_remote);
			break;

		case EVT_CHANNEL_UNLINK_NOTIFY:
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch != NULL){
				evt_ch->unlink = TRUE;
			}
			break;

		case EVT_NEW_SUBSCRIBE:
			
			evt_ch = find_channel_by_name(channel_name);			
			if((evt_ch != NULL) && (evt_ch->event_cache != NULL)){
				send_cached_events_to_new_subscriber(orig,
						msg->private.new_subscription,
						evt_ch->event_cache);
			}

			if(msg->private.new_subscription->channel_name != NULL){
				g_free(msg->private.new_subscription->channel_name);
			}
			if(msg->private.new_subscription->filters != NULL){
				free_filters(msg->private.new_subscription->filters);
			}
			g_free(msg->private.new_subscription);
			break;

		case EVT_NEW_SUBSCRIBE_REPLY:
			evt_ch = find_channel_by_name(channel_name);
			if(evt_ch == NULL){
				return 0;
			}

			ch_instance = g_hash_table_lookup(
					evt_ch->channel_instances, 
					(gpointer)(long)(msg->private.new_sub_reply->ch_id));

			if(ch_instance == NULL){
				return 0;
			}
			subscription = g_hash_table_lookup(
				ch_instance->subscriptions,
				(gpointer)(long)(msg->private.new_sub_reply->subscription_id));
			if(subscription == NULL){
				free_remote_event(msg->private.new_sub_reply->event);
				g_free(msg->private.new_sub_reply);
				break;
			}
			deliver_event_to_local_subscriber(subscription, 
					msg->private.new_sub_reply->event);
			free_remote_event(msg->private.new_sub_reply->event);
			g_free(msg->private.new_sub_reply);
			break;

		case EVT_RETENTION_CLEAR_REQUEST:
			clear_req = msg->private.retention_clear;
			event_id = clear_req->event_id;
			node_id =  (event_id >> 32);
			if(node_id == node_list.mynode){
				ret_code = clear_retention_time(channel_name,
						msg);
				send_retention_clear_reply(orig, event_id,
						ret_code, channel_name);
			}else{
				/*error*/
			}
			g_free(clear_req);
			break;

		case EVT_RETENTION_CLEAR_REPLY:
			clear_reply = msg->private.retention_clear_reply;
			event_id = clear_reply->event_id;
			tmp_32 = event_id;
			clear_requ = g_hash_table_lookup(info->evt_pending_clear_requests,
						(gpointer)(long)tmp_32);

			ret_code = clear_reply->ret_code;
			node_id =  (event_id >> 32);
			client = clear_requ->client;
			if(node_id == node_list.mynode){				
				send_retention_clear_reply_to_client(client,channel_name,
									event_id, ret_code);
				g_hash_table_remove(info->evt_pending_clear_requests,
										(gpointer)(long)tmp_32);
				g_free(clear_requ);
			}else{
			}
			g_free(clear_reply);
			break;

		default:
			break;

	}
	if(channel_name != NULL){
		g_free(channel_name);
	}
	ha_msg_del(m);
	g_free(msg);
	return 0;
}

static gboolean
hb_input_dispatch(IPC_Channel *client, gpointer user_data)
{
	if(handle_msg_from_hb(((hb_usrdata_t *)user_data)->hb_fd)){
			g_main_quit(((hb_usrdata_t *)user_data)->mainloop);
				return FALSE;
	}
	return TRUE;
}

static void
hb_input_destroy(gpointer user_data)
{
	exit(1);
}

static IPC_WaitConnection *
wait_channel_init(void)
{
	IPC_WaitConnection *wait_ch;
	mode_t mask;
	char path[] = IPC_PATH_ATTR;
	char evtfifo[] = EVTFIFO;
	char domainsocket[] = IPC_DOMAIN_SOCKET;

	GHashTable * attrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(attrs, path, evtfifo);
	mask = umask(0);
	wait_ch = ipc_wait_conn_constructor(domainsocket, attrs);
	if (wait_ch == NULL){
		cl_perror("Can't create wait channel");
		exit(1);
	}
	mask = umask(mask);
	g_hash_table_destroy(attrs);
	return wait_ch;
}

static gboolean
clntCh_input_dispatch(IPC_Channel *client, 
			      gpointer        user_data)
{
	if(client->ch_status == IPC_DISCONNECT){
		return FALSE;
	}else{
		handle_msg_from_client(client, user_data);
		return TRUE;
	}
}


static void
clntCh_input_destroy(gpointer user_data)
{
	IPC_Channel *client;
	struct ipc *evt_ipc;

	printf("A client connection is destroyed!\n");
				
	client = (IPC_Channel *)user_data;
	evt_ipc = (struct ipc *)g_hash_table_lookup(
				hash_table_for_ipc,
				(gpointer)client);
	if(evt_ipc == NULL){
		return;
	}
	g_hash_table_remove(hash_table_for_ipc,
						(gpointer)client);
	g_hash_table_foreach(evt_ipc->channel_instances, 
						free_ch_instance,
						(gpointer)evt_ipc->channel_instances);
	g_hash_table_destroy(evt_ipc->channel_instances);
	g_free(evt_ipc);
	cl_log(LOG_INFO, "clntCh_input_destroy:received HUP");
	return;
}

static gboolean
waitCh_input_dispatch(IPC_Channel *newclient, gpointer user_data)
{

	G_main_add_IPC_Channel(G_PRIORITY_HIGH, newclient, FALSE,
	                       clntCh_input_dispatch, newclient,
	                       clntCh_input_destroy);
	printf("A new client connected!\n");
	return TRUE;
}

static void
waitCh_input_destroy(gpointer user_data)
{
	IPC_WaitConnection *wait_ch = (IPC_WaitConnection *)user_data;
	wait_ch->ops->destroy(wait_ch);
	exit(1);
}

static gboolean
test_timeout(gpointer user_data)
{
	printf("timer expired \n");
	Gmain_timeout_remove(*((int *)user_data));
	return TRUE;
}

int main(int argc, char **argv)
{

	char *cmdname;	
	char *tmp_cmdname = strdup(argv[0]);
	ll_cluster_t    *hb_fd;
	hb_usrdata_t    usrdata;
	IPC_Channel *ipc_chan;
	IPC_WaitConnection *wait_ch;
	int temp;

	if ((cmdname = strrchr(tmp_cmdname, '/')) != NULL) {
		++cmdname;
	} else {
		cmdname = tmp_cmdname;
	}
    cl_log_set_entity(cmdname);
	cl_log_set_facility(LOG_DAEMON);
		
	CL_IGNORE_SIG(SIGPIPE);
	hb_fd = (ll_cluster_t *)evt_daemon_initialize();
	if(!hb_fd) {
		printf("failed to register!!\n");
	    exit(1);
	}
	printf("succeed to register!!\n");
	usrdata.hb_fd = hb_fd;
	usrdata.mainloop = g_main_new(TRUE);
	hash_table_for_ipc = g_hash_table_new(g_direct_hash,
								g_direct_equal);
	hash_table_for_channel_name = g_hash_table_new(g_str_hash,g_str_equal);
	info->hb = hb_fd;
	ipc_chan = hb_fd->llc_ops->ipcchan(hb_fd);
	G_main_add_IPC_Channel(G_PRIORITY_HIGH, ipc_chan, FALSE,
	                        hb_input_dispatch, &usrdata, hb_input_destroy);

	wait_ch = wait_channel_init();
	G_main_add_IPC_WaitConnection(G_PRIORITY_LOW, wait_ch, NULL,
				                FALSE, waitCh_input_dispatch, wait_ch,
				                waitCh_input_destroy);

	temp = Gmain_timeout_add_full(G_PRIORITY_HIGH, 1000, test_timeout, &temp, NULL);

	g_main_run(usrdata.mainloop);
	printf("exit from g_main_run\n");
	g_main_destroy(usrdata.mainloop);
	free(tmp_cmdname);

	return(1);
	/*(void)_heartbeat_h_Id;*/
	/*(void)_ha_msg_h_Id;*/
		
}

