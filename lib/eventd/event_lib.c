/* 
 * event_lib.c: source file for event library
 *
 * Copyright (C) 2004 Forrest,Zhao <forrest.zhao@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
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
#include <event.h>

int global_debug=0;
int global_verbose=0;

static GHashTable *evt_handle_hash = NULL;
static GHashTable *evt_channel_hash = NULL;
static GHashTable *evt_event_hash = NULL;
static GHashTable *subscription_global = NULL;

#define RELEASE_CODE 'A'
#define MAJOR_VERSION 1
#define MINOR_VERSION 1

typedef struct evt_event_handle_s {
	SaSizeT event_size;
	void *patternArray;
	SaUint8T priority;
	SaTimeT retentionTime;
	SaNameT publisherName;
	SaTimeT publishTime;
	SaEvtEventIdT eventId;
	void *eventData;
	SaSizeT eventDataSize;
	SaEvtChannelHandleT ch_handle;
	SaEvtSubscriptionIdT subscription_id;
	SaEvtEventIdT evtId;
	SaEvtChannelHandleT channelId;
	struct evt_event_handle_s *next;
	int set_flag;
} evt_event_handle;

struct queue_head {
	evt_event_handle *head;
	evt_event_handle *tail; 
};

struct open_channel_reply_queue {
	struct open_channel_reply *head;
	struct open_channel_reply *tail;
};

struct event_queue_s {
	SaSizeT event_number;
	struct queue_head queue[SA_EVT_HIGHEST_PRIORITY+1];
	SaSizeT reply_number;
	struct open_channel_reply_queue open_ch_reply_queue;
};

typedef struct evt_handle_s {
	struct IPC_CHANNEL *ch;
	SaSelectionObjectT selectionObject;
	SaEvtCallbacksT callbacks;
	GHashTable *evt_channel_hash;
	struct event_queue_s *event_queue;
} evt_handle;

typedef struct evt_channel_handle_s{
	SaNameT channelName;
	struct IPC_CHANNEL *ch;
	SaSelectionObjectT selectionObject;
	SaEvtEventIdT channelId;	
	GHashTable *event_handle_hash;
	GHashTable *subscription_hash;
	SaEvtHandleT evt_handle;
	SaInvocationT invocation; 
	SaEvtChannelOpenFlagsT open_flags;
	void *ch_instance; 
} evt_channel_handle;

typedef struct evt_subscription_s {
	const SaNameT *channelName;
	SaEvtEventFilterArrayT *filters;
	SaEvtEventIdT channelId;
	SaEvtSubscriptionIdT subscriptionId;
} evt_subscription;

struct open_channel_reply{
	SaNameT channel_name;
	SaEvtChannelHandleT clt_ch_handle;
	void* ch_instance;
	SaErrorT ret_code;
	struct open_channel_reply *next;
};

struct publish_reply{
	SaEvtEventHandleT eventHandle;
	SaErrorT ret_code;
	SaEvtEventIdT event_id;
};

struct clear_retention_time_reply{
	SaNameT channel_name;
	SaEvtEventIdT event_id;
	SaErrorT ret_code;
};

struct daemon_msg {
	enum evt_type msg_type;	
	union {
		struct open_channel_reply *open_ch_reply;
		struct publish_reply *pub_reply;
		struct clear_retention_time_reply *clear_retention_reply;
		evt_event_handle *event;
	} private;
};

static struct sa_handle_database evt_handle_database;
static struct sa_handle_database ch_handle_database;
static struct sa_handle_database event_handle_database;


static void init_global_variable(void)
{
	evt_handle_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	evt_channel_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	evt_event_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	subscription_global = g_hash_table_new(g_direct_hash, g_direct_equal);
	evt_handle_database.handle_count = 0;
	ch_handle_database.handle_count = 0;
	event_handle_database.handle_count = 0;
	return;
}

static SaErrorT send_to_evt_daemon(struct IPC_CHANNEL *ch,
				void *msg, SaSizeT msg_size)
{
	struct IPC_MESSAGE	Msg;
	
	memset(&Msg, 0, sizeof(Msg));
	Msg.msg_body = msg;
	Msg.msg_len = msg_size;
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = ch;
	if(ch->ops->send(ch, &Msg) != IPC_OK){
		return SA_ERR_LIBRARY;
	}

	return SA_OK;
}

static SaErrorT send_evt_init(struct IPC_CHANNEL *ch)
{
	SaSizeT msg_size;
	SaUint8T *tmp_char;
	void *msg;
				
	msg_size = 1;
	msg = g_malloc(msg_size);
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_INITIALIZE;
	printf("%d", *(tmp_char));
	send_to_evt_daemon(ch, msg, msg_size);
	
	return SA_OK;
}

static void free_event(evt_event_handle *event_hd);

SaErrorT 
saEvtInitialize(SaEvtHandleT *evtHandle,
				const SaEvtCallbacksT *callbacks,
				                SaVersionT *version)
{
	struct IPC_CHANNEL *ch;
	evt_handle *evt_hd;
	GHashTable *attrs, *channel_hash;
	char	path[] = IPC_PATH_ATTR;
	char	sockpath[] = EVTFIFO;
	struct event_queue_s *event_queue;
	static int init = 0;

	if((evtHandle == NULL) || (callbacks == NULL) || (version == NULL)){
		return SA_ERR_INVALID_PARAM;
	}

	if(init == 0){
		init_global_variable();
		init++;
	}
	if((version->releaseCode == RELEASE_CODE) &&
						(MAJOR_VERSION >= version->major)){
		version->major = MAJOR_VERSION;
		version->minor = MINOR_VERSION;
	}else{
		version->releaseCode = RELEASE_CODE;
		version->major = MAJOR_VERSION;
		version->minor = MINOR_VERSION;
		return SA_ERR_VERSION;
	}
	if(get_handle(&evt_handle_database, evtHandle) != SA_OK)
		return SA_ERR_NO_MEMORY;
			
	evt_hd = (evt_handle *)g_malloc(sizeof(evt_handle));
	if (!evt_hd){
		put_handle(&evt_handle_database, *evtHandle);
		return SA_ERR_NO_MEMORY;
	}
	channel_hash = g_hash_table_new(g_direct_hash,g_direct_equal);
	if(!channel_hash){
		g_free(evt_hd);
		put_handle(&evt_handle_database, *evtHandle);
		return SA_ERR_NO_MEMORY;
	}
	event_queue = (struct event_queue_s *)g_malloc0(
			sizeof(struct event_queue_s));
	attrs = g_hash_table_new(g_str_hash,g_str_equal);
	if(!attrs){
		g_free(evt_hd);
		put_handle(&evt_handle_database, *evtHandle);
		g_hash_table_destroy(channel_hash);
		return SA_ERR_NO_MEMORY;
	}
	g_hash_table_insert(attrs, path, sockpath);
	ch = ipc_channel_constructor(IPC_DOMAIN_SOCKET, attrs);
	g_hash_table_destroy(attrs);
	if(!ch || ch->ops->initiate_connection(ch) != IPC_OK){
		g_free(evt_hd);
		put_handle(&evt_handle_database, *evtHandle);
		g_hash_table_destroy(channel_hash);
		return SA_ERR_LIBRARY;
	}

	ch->ops->set_recv_qlen(ch, 0);
	if(send_evt_init(ch) != SA_OK){
		g_free(evt_hd);
		put_handle(&evt_handle_database, *evtHandle);
		g_hash_table_destroy(channel_hash);
		ch->ops->destroy(ch);
		return SA_ERR_LIBRARY;
	}
						
	evt_hd->ch = ch;
	evt_hd->selectionObject = ch->ops->get_recv_select_fd(ch);	
	evt_hd->callbacks.saEvtChannelOpenCallback = 
							callbacks->saEvtChannelOpenCallback;
	evt_hd->callbacks.saEvtEventDeliverCallback = 
							callbacks->saEvtEventDeliverCallback;
	evt_hd->evt_channel_hash = channel_hash;	
	evt_hd->event_queue = event_queue;
	g_hash_table_insert(evt_handle_hash, (gpointer)*evtHandle, evt_hd);	
	return SA_OK;	
}

static void read_normal_event(void *msg, struct daemon_msg *ret)
{
	evt_event_handle *event;
	SaSizeT  publisher_len;
	SaUint8T *tmp_char;	

	event = (evt_event_handle *)g_malloc(sizeof(evt_event_handle));
	ret->private.event = event;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&(event->channelId), tmp_char, sizeof(SaEvtChannelHandleT));
	tmp_char += sizeof(SaEvtChannelHandleT);
	memcpy(&(event->subscription_id), tmp_char, sizeof(SaEvtSubscriptionIdT));
	tmp_char += sizeof(SaEvtSubscriptionIdT);
	memcpy(&(event->event_size), tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	event->patternArray = g_malloc(event->event_size);
	memcpy(event->patternArray, tmp_char, event->event_size);
	tmp_char += event->event_size;
	event->priority = *(tmp_char);
	tmp_char++;
	memcpy(&(event->retentionTime), tmp_char, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(&(publisher_len), tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	event->publisherName.length = publisher_len;
	memcpy(event->publisherName.value, tmp_char, publisher_len);
	event->publisherName.value[publisher_len] = '\0';
	tmp_char += publisher_len;
	memcpy(&(event->publishTime), tmp_char, sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(&(event->eventId), tmp_char, sizeof(SaEvtEventIdT));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(&(event->eventDataSize), tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	event->eventData = g_malloc(event->eventDataSize);
	memcpy(event->eventData, tmp_char, event->eventDataSize);
	return;
}

static void read_ch_open_reply(void *msg, struct daemon_msg *ret)
{
	SaUint8T *tmp_char;
	SaSizeT str_len;
	struct open_channel_reply *open_ch_reply;	

	open_ch_reply = (struct open_channel_reply *)g_malloc(
			sizeof(struct open_channel_reply));
	ret->private.open_ch_reply = open_ch_reply;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&(str_len), tmp_char, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	open_ch_reply->channel_name.length = str_len;
	memcpy(open_ch_reply->channel_name.value, tmp_char, str_len);
	open_ch_reply->channel_name.value[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(open_ch_reply->clt_ch_handle), tmp_char, sizeof(SaEvtChannelHandleT));
	tmp_char += sizeof(SaEvtChannelHandleT);
	memcpy(&(open_ch_reply->ch_instance), tmp_char, sizeof(void *));
	tmp_char += sizeof(void *);
	memcpy(&(open_ch_reply->ret_code), tmp_char, sizeof(SaErrorT));
	return;
}

static void read_publish_reply(void *msg, struct daemon_msg *ret)
{
	struct publish_reply *pub_reply;
	SaUint8T *tmp_char;
	
	pub_reply = (struct publish_reply *)g_malloc(
			sizeof(struct publish_reply));
	ret->private.pub_reply = pub_reply;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&(pub_reply->eventHandle), tmp_char, sizeof(SaEvtEventHandleT));
	tmp_char += sizeof(SaEvtEventHandleT);
	memcpy(&(pub_reply->event_id), tmp_char, sizeof(SaEvtEventIdT));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(&(pub_reply->ret_code), tmp_char, sizeof(SaErrorT));
	return;
}

static void read_clear_retention_reply(void *msg, struct daemon_msg *ret)
{
	struct clear_retention_time_reply *clear_reply;
	SaUint8T *tmp_char;
	SaSizeT str_len;
	
	clear_reply = (struct clear_retention_time_reply *)g_malloc(
			sizeof(struct clear_retention_time_reply));
	ret->private.clear_retention_reply = clear_reply;
	tmp_char = (SaUint8T *)msg;
	tmp_char++;
	memcpy(&str_len, tmp_char, sizeof(SaSizeT));
	clear_reply->channel_name.length = str_len;	
	tmp_char += sizeof(SaSizeT);
	memcpy(clear_reply->channel_name.value, tmp_char, str_len);
	clear_reply->channel_name.value[str_len] = '\0';
	tmp_char += str_len;
	memcpy(&(clear_reply->event_id), tmp_char, sizeof(SaEvtEventIdT));
	tmp_char += sizeof(SaEvtEventIdT);
	memcpy(&(clear_reply->ret_code), tmp_char, sizeof(SaErrorT));
	return;
}

static struct daemon_msg *read_from_ipc(IPC_Channel *ch)
{
	IPC_Message* ipc_msg;
	char* msg_type;
	struct daemon_msg *ret;
	
	ret = (struct daemon_msg *)g_malloc(sizeof(struct daemon_msg));
	ch->ops->is_message_pending(ch);
	if(ch->ops->recv(ch, &ipc_msg) != IPC_OK){
		return NULL;	
	}	
	msg_type = (char *)ipc_msg->msg_body;
	ret->msg_type = *(msg_type);
	printf("the msg type is: %d\n", ret->msg_type);
	switch(*(msg_type)){
		case EVT_NORMAL_EVENT:			
			read_normal_event(ipc_msg->msg_body, ret);
			break;
		case EVT_CH_OPEN_REPLY_FROM_DAEMON:
			read_ch_open_reply(ipc_msg->msg_body, ret);
			break;
		case EVT_PUBLISH_REPLY:
			read_publish_reply(ipc_msg->msg_body, ret);
			break;
		case EVT_CLEAR_RETENTION_TIME_REPLY:
			read_clear_retention_reply(ipc_msg->msg_body, ret);
			break;
		default:
			break;

	}
	ipc_msg->msg_done(ipc_msg);
	return ret;
}

static SaErrorT append_to_event_queue(struct event_queue_s *event_queue,
		evt_event_handle *event_hd)
{
	SaUint8T priority;
	struct queue_head *queue;

	priority = event_hd->priority;
	if(priority > SA_EVT_LOWEST_PRIORITY)
	{
		return SA_ERR_INVALID_PARAM;
	}
	event_queue->event_number++;
	queue = &(event_queue->queue[priority]);
	if((queue->head == NULL) && (queue->tail == NULL)){
		queue->head = event_hd;
		queue->tail = event_hd;
	}else{	
		queue->tail->next = event_hd;
		queue->tail = event_hd;
	}
	return SA_OK;
}

static SaErrorT append_to_reply_queue(struct event_queue_s *event_queue,
		struct open_channel_reply *open_ch_reply)
{
	event_queue->reply_number++;
	if((event_queue->open_ch_reply_queue.tail == NULL) &&
			(event_queue->open_ch_reply_queue.head == NULL)){
		event_queue->open_ch_reply_queue.head = open_ch_reply;
		event_queue->open_ch_reply_queue.tail = open_ch_reply;
	}else{
		event_queue->open_ch_reply_queue.tail->next = open_ch_reply;
		event_queue->open_ch_reply_queue.tail = open_ch_reply;
	}
	return SA_OK;
}

SaErrorT 
saEvtChannelOpen(const SaEvtHandleT evtHandle, const SaNameT *channelName,
                 SaEvtChannelOpenFlagsT channelOpenFlags, SaTimeT timeout,
                 SaEvtChannelHandleT *channelHandle)
{
	evt_handle *evt_hd;
	evt_channel_handle *evt_channel_hd;
	GHashTable *channel_hash;
	struct IPC_CHANNEL *ch;
	fd_set rset;
	struct timeval time_out;
	int fd, select_ret;
	SaEvtChannelHandleT channel_handle;
	void *msg;
	struct daemon_msg *msg_reply;	
	evt_event_handle *event_hd;
	struct open_channel_reply *open_ch_reply;
	SaSizeT msg_len, str_len;
	SaUint8T *tmp_char;
		
	if((channelHandle == NULL) || (channelName == NULL)){
		return SA_ERR_INVALID_PARAM;
	}
	if(channelOpenFlags > 7){
		return SA_ERR_BAD_FLAGS;
	}
	evt_hd = (evt_handle *)g_hash_table_lookup(evt_handle_hash,
			(gpointer)evtHandle);
	if( evt_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	ch = evt_hd->ch;
	channel_hash = evt_hd->evt_channel_hash;
	evt_channel_hd = (evt_channel_handle *)g_malloc(
			sizeof(evt_channel_handle));
	if (!evt_channel_hd)
		return SA_ERR_NO_MEMORY;
	if(get_handle(&ch_handle_database, &channel_handle) != SA_OK){
		g_free(evt_channel_hd);
		return SA_ERR_LIBRARY;
	}
	
	/*send channel_open request */
	str_len = channelName->length;
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaEvtChannelHandleT)
		+sizeof(SaEvtChannelOpenFlagsT);
	msg = g_malloc(msg_len);
	if(msg == NULL){
		return SA_ERR_NO_MEMORY;
	}
	tmp_char = (SaUint8T *)msg;
	*tmp_char = EVT_OPEN_EVENT_CHANNEL;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	strncpy(tmp_char, channelName->value, str_len);
	tmp_char += str_len;

	memcpy(tmp_char, &(channel_handle), sizeof(SaEvtChannelHandleT));
	tmp_char += sizeof(SaEvtChannelHandleT);
	memcpy(tmp_char, &(channelOpenFlags), sizeof(SaEvtChannelOpenFlagsT));
	send_to_evt_daemon(evt_hd->ch, msg, msg_len);

/*	sleep(1); */
	g_free(msg);

	/*wait for reply */
	fd = evt_hd->selectionObject;
	time_out.tv_sec = 0;
	time_out.tv_usec = timeout;
	/*if msg_type is normal event, buffer it and continue */
	/*if msg_type is open_channel_reply, break */
	for(;;){
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		select_ret = select(fd + 1, &rset, NULL,NULL, NULL);
		if(select_ret == -1){
			/*perror("select"); */
			return SA_ERR_LIBRARY;		
		}else if(select_ret == 0){
			return SA_ERR_TIMEOUT;
		}
		msg_reply = read_from_ipc(ch);
		if(msg_reply == NULL){
			printf("received NULL msg from daemon\n");
			return 0;
		}
		if(msg_reply->msg_type == EVT_CH_OPEN_REPLY_FROM_DAEMON){
			if(channel_handle == 
				msg_reply->private.open_ch_reply->clt_ch_handle){
				if(msg_reply->private.open_ch_reply->ret_code
						== SA_OK){
					break;
				}else{
					return msg_reply->private.open_ch_reply->ret_code;
				}
			}else{
				/*update timeout, continue */
			}
		}else if(msg_reply->msg_type == EVT_NORMAL_EVENT) {
			
			event_hd = msg_reply->private.event;			
			append_to_event_queue(evt_hd->event_queue, event_hd);
			/*TODO: update timeout, continue */
		}else if(msg_reply->msg_type == EVT_ASYN_CH_OPEN_REPLY_FROM_DAEMON)
		{
			open_ch_reply = msg_reply->private.open_ch_reply;
			append_to_reply_queue(evt_hd->event_queue,
					open_ch_reply);
		}else 
		{
			/*update timeout, continue */
			/*error msg */
		}
		
	}
	/*if open evt_channel succeed	 */
	evt_channel_hd->channelName.length = str_len;
	strncpy(evt_channel_hd->channelName.value, channelName->value, str_len);
	evt_channel_hd->evt_handle = evtHandle;
	evt_channel_hd->ch = evt_hd->ch;
	evt_channel_hd->selectionObject = fd;
	evt_channel_hd->open_flags = channelOpenFlags;
	evt_channel_hd->ch_instance = 
		msg_reply->private.open_ch_reply->ch_instance;
	evt_channel_hd->event_handle_hash = g_hash_table_new(g_direct_hash, 
				g_direct_equal);
	evt_channel_hd->subscription_hash = g_hash_table_new(g_direct_hash, 
				g_direct_equal);
	g_hash_table_insert(channel_hash, (gpointer)channel_handle,
			evt_channel_hd);
	g_hash_table_insert(evt_channel_hash, (gpointer)channel_handle,
			evt_channel_hd);
	*(channelHandle) = channel_handle;
	return SA_OK;
}

static void free_open_ch_reply(struct open_channel_reply *open_ch_reply)
{
	
	g_free(open_ch_reply);
	return;
}

static void free_event_queue(struct event_queue_s *event_queue)
{
	SaSizeT i;
	struct queue_head *queue;
	evt_event_handle *event_tmp;
	struct open_channel_reply *open_ch_reply;
	
	if(event_queue->event_number != 0){
		for(i=SA_EVT_HIGHEST_PRIORITY;i<=SA_EVT_LOWEST_PRIORITY;i++){
			queue = &(event_queue->queue[i]);
			while(queue->head != NULL){
				event_tmp = queue->head;
				queue->head = event_tmp->next;
				free_event(event_tmp);
			}
			g_free(queue);
		}
	}
	if(event_queue->reply_number != 0){
		while(event_queue->open_ch_reply_queue.head != NULL){
			open_ch_reply = event_queue->open_ch_reply_queue.head;
			event_queue->open_ch_reply_queue.head = 
				open_ch_reply->next;
			free_open_ch_reply(open_ch_reply);
		}
	}
	g_free(event_queue);
}

static void free_event(evt_event_handle *event_hd)
{
	/*free patternArray, publisherName, then free event_hd */
	g_free(event_hd->patternArray);	
	g_free(event_hd->eventData);
	g_free(event_hd);
	return;
}

static void free_subscription_resource(gpointer key,
		gpointer value,
		gpointer user_data)
{
	GHashTable *sub_hash = user_data;
	g_hash_table_remove(sub_hash, key);
	g_hash_table_remove(subscription_global, key);
	return;
}

static void free_event_resource(gpointer key,
				gpointer value, gpointer user_data)
{
	SaEvtEventHandleT event_handle;
	evt_event_handle *event_hd;
	GHashTable *event_handle_hash;

	event_handle = (SaEvtEventHandleT)key;
	event_hd = (evt_event_handle *)value;
	event_handle_hash = (GHashTable *)user_data;
	free_event(event_hd);
	g_hash_table_remove(evt_event_hash, key);
	return;
}

static void free_ch_resource(gpointer key,
				gpointer value, gpointer user_data)
{
	SaEvtChannelHandleT ch_handle;
	evt_channel_handle *ch_hd;
	GHashTable *ch_hash, *event_handle_hash, *subscription_hash;

	ch_handle = (SaEvtChannelHandleT)key;
	ch_hd = (evt_channel_handle *)value;
	ch_hash = (GHashTable *)user_data;
	g_hash_table_remove(evt_channel_hash, (gpointer)ch_handle);
	g_hash_table_remove(ch_hash, (gpointer)ch_handle);	
	event_handle_hash = ch_hd->event_handle_hash;
	if(event_handle_hash != NULL){
	g_hash_table_foreach(event_handle_hash, 
				free_event_resource, event_handle_hash);
	}
	subscription_hash = ch_hd->subscription_hash;
	if(subscription_hash != NULL){
	g_hash_table_foreach(subscription_hash, 
				free_subscription_resource, subscription_hash);
	}
	return;
}

static SaErrorT send_evt_finalize(struct IPC_CHANNEL *ch,
							evt_handle *evt_hd)
{
	SaSizeT msg_len;
	void *msg;
	SaUint8T *tmp_char;

	msg_len = 1;
	msg = g_malloc(msg_len);
	if(msg == NULL){
		return SA_ERR_NO_MEMORY;
	}
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_FINALIZE;	
	send_to_evt_daemon(ch, msg, msg_len);	
	
	return SA_OK;
}

SaErrorT 
saEvtFinalize(SaEvtHandleT evtHandle)
{
	evt_handle *evt_hd;
	struct IPC_CHANNEL *ch;
	
	evt_hd = g_hash_table_lookup(evt_handle_hash, (gpointer)evtHandle);
	if( evt_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	/*close the connection */
	ch = evt_hd->ch;
	send_evt_finalize(ch, evt_hd);	
	ch->ops->destroy(ch);
	/*free up the resources, free the channel, the events */
	g_hash_table_remove(evt_handle_hash, (gpointer)evtHandle);
	g_hash_table_foreach(evt_hd->evt_channel_hash,
			free_ch_resource,
			evt_hd->evt_channel_hash);
	free_event_queue(evt_hd->event_queue);
	g_free(evt_hd);
	return SA_OK;
}

static SaErrorT send_channel_close(IPC_Channel *ch,
		evt_channel_handle *evt_channel_hd)
{
	void *msg;
	char *tmp_char;
	SaSizeT str_len, msg_len;
		
	str_len = evt_channel_hd->channelName.length;
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(void *);
	msg = g_malloc(msg_len);
	if(msg == NULL){
		return SA_ERR_NO_MEMORY;
	}
	tmp_char = msg;
	*tmp_char = EVT_CLOSE_EVENT_CHANNEL;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	strncpy(tmp_char, evt_channel_hd->channelName.value, str_len);
	tmp_char = tmp_char + str_len;
	memcpy(tmp_char, &(evt_channel_hd->ch_instance), sizeof(void *));
	send_to_evt_daemon(ch, msg, msg_len);
	return SA_OK;
}

SaErrorT 
saEvtChannelClose(SaEvtChannelHandleT channelHandle)
{
	evt_channel_handle *evt_channel_hd;
	evt_handle *evt_hd;
	GHashTable *event_handle_hash, *subscription_hash;
	/*free events, free subscriptions, free item in hash table, free evt_channel_hd */
	evt_channel_hd = g_hash_table_lookup(evt_channel_hash,
			(gpointer)channelHandle);
	if(evt_channel_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	send_channel_close(evt_channel_hd->ch ,evt_channel_hd);
	
	event_handle_hash = evt_channel_hd->event_handle_hash;
	if(event_handle_hash != NULL){
		g_hash_table_foreach(event_handle_hash, 
				free_event_resource, event_handle_hash);
		g_hash_table_destroy(event_handle_hash);
	}
	
	subscription_hash = evt_channel_hd->subscription_hash;
	if(subscription_hash != NULL){
	g_hash_table_foreach(subscription_hash, 
				free_subscription_resource, subscription_hash);
	g_hash_table_destroy(subscription_hash);
	}

	evt_hd = g_hash_table_lookup(evt_handle_hash, 
			(gpointer)evt_channel_hd->evt_handle);
	g_hash_table_remove(evt_hd->evt_channel_hash, (gpointer)channelHandle);
	g_hash_table_remove(evt_channel_hash, (gpointer)(channelHandle));
	g_free(evt_channel_hd);
	return SA_OK;
}

SaErrorT 
saEvtEventAllocate(const SaEvtChannelHandleT channelHandle,
                   SaEvtEventHandleT *eventHandle)
{
	evt_event_handle *event_hd;	
	evt_channel_handle *evt_channel_hd;
	GHashTable *event_handle_hash;
	
	if(eventHandle == NULL){
		return SA_ERR_INVALID_PARAM;
	}
	evt_channel_hd = g_hash_table_lookup(evt_channel_hash,
			(gpointer)channelHandle);
	if(evt_channel_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	event_hd = (evt_event_handle *)g_malloc0(sizeof(evt_event_handle));
	if (!event_hd)
		return SA_ERR_NO_MEMORY;
	event_handle_hash = evt_channel_hd->event_handle_hash;
	event_hd->channelId = channelHandle;
	if(get_handle(&event_handle_database, eventHandle) != SA_OK){
		g_free(event_hd);
		return SA_ERR_LIBRARY;
	}
	g_hash_table_insert(evt_event_hash, (gpointer)*eventHandle, event_hd);
	g_hash_table_insert(evt_channel_hd->event_handle_hash, 
			(gpointer)*eventHandle, event_hd);
	return SA_OK;
}

SaErrorT 
saEvtEventFree(SaEvtEventHandleT eventHandle)
{
	evt_event_handle *event_hd;
	evt_channel_handle *evt_channel_hd;
	
	event_hd = g_hash_table_lookup(evt_event_hash, (gpointer)eventHandle);
	if(event_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}	
	free_event(event_hd);	
	evt_channel_hd = g_hash_table_lookup(evt_channel_hash,
			(gpointer)event_hd->channelId);
	if(evt_channel_hd == NULL){
		return SA_ERR_LIBRARY;
	}	
	g_hash_table_remove(evt_channel_hd->event_handle_hash,
			(gpointer)eventHandle);
	g_hash_table_remove(evt_event_hash,
			(gpointer)eventHandle);
	put_handle(&event_handle_database, eventHandle);
	return SA_OK;
}

static SaErrorT copy_patternarray(evt_event_handle *event_hd,
		const SaEvtEventPatternArrayT *patternArray)
{
	SaSizeT number, i, size=0;
	SaEvtEventPatternT *pattern;
	SaSizeT *tmp_size;
	SaUint8T *tmp_char;
	number = patternArray->patternsNumber;
	pattern = patternArray->patterns;
	/*size = field1(number of patterns)+ field2(length of each pattern)+ field3(patterns) */
	for(i=0; i<number; i++){
		size = size + (pattern+i)->patternSize;
	}
	size = size + (number+1)*sizeof(SaSizeT);
	event_hd->patternArray = g_malloc(size);
	event_hd->event_size = size;
	if(event_hd->patternArray == NULL){
		return SA_ERR_NO_MEMORY;
	}
	tmp_size = event_hd->patternArray;
	*(tmp_size) = number;
	tmp_size++;
	for(i=0; i<number; i++){
		*(tmp_size) = (pattern+i)->patternSize;
		tmp_size++;
	}
	tmp_char = (SaUint8T *)tmp_size;
	for(i=0; i<number; i++){
		strncpy(tmp_char, (pattern+i)->pattern,
				(pattern+i)->patternSize);
		tmp_char = tmp_char + (pattern+i)->patternSize;
	}
	return SA_OK;
}

SaErrorT 
saEvtEventAttributesSet(SaEvtEventHandleT eventHandle,
                        const SaEvtEventPatternArrayT *patternArray,
                        SaUint8T priority,
                        SaTimeT retentionTime,
                        const SaNameT *publisherName)
{
	evt_event_handle *event_hd;

	if((patternArray == NULL) || (publisherName == NULL)){
		return SA_ERR_INVALID_PARAM;
	}
	if(priority > SA_EVT_LOWEST_PRIORITY){
		return SA_ERR_INVALID_PARAM;
	}	
	event_hd = (evt_event_handle *)g_hash_table_lookup(evt_event_hash,
			(gpointer)(eventHandle));
	if(event_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}	
	copy_patternarray(event_hd, patternArray);	
	event_hd->priority = priority;
	event_hd->retentionTime = retentionTime;
	event_hd->publisherName.length = publisherName->length;
	memcpy(event_hd->publisherName.value, publisherName->value,
			publisherName->length);		
	event_hd->publishTime = (SaTimeT)time(NULL);	
	event_hd->set_flag = 1;

	return SA_OK;
}

#define min(A,B) ((A)<(B) ? (A) : (B))

SaErrorT 
saEvtEventAttributesGet(const SaEvtEventHandleT eventHandle,
                        SaEvtEventPatternArrayT *patternArray,
                        SaUint8T *priority,
                        SaTimeT *retentionTime,
                        SaNameT *publisherName,                        
                        SaTimeT *publishTime,
                        SaEvtEventIdT *eventId)
{
	evt_event_handle *event_hd;
	SaSizeT number, *tmp_size, min_number, i;
	SaUint8T *tmp_char;
	SaEvtEventPatternT *patterns;
	
	event_hd = (evt_event_handle *)g_hash_table_lookup(
			evt_event_hash, (gpointer)eventHandle);
	if(event_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	
	/*TODO: what should be done if patterSize conflicts */
	if(patternArray != NULL){
		tmp_size = (SaSizeT *)event_hd->patternArray;
		number = *(tmp_size);
		tmp_size++;
		tmp_char = (SaUint8T *)(tmp_size+number);
		min_number = min(patternArray->patternsNumber, number);
		patternArray->patternsNumber = min_number;
		patterns = patternArray->patterns;
		for(i=0; i<min_number; i++){
			patterns[i].patternSize = min(patterns[i].patternSize,
					*(tmp_size));
			memcpy(patterns[i].pattern, tmp_char, 
					patterns[i].patternSize);
			tmp_char += *(tmp_size);
			tmp_size++;
		}
	}
	if(priority != NULL){
		*(priority) = event_hd->priority;
	}
	if(retentionTime != NULL){
		*(retentionTime) = event_hd->retentionTime;
	}
	if(publisherName != NULL){
		publisherName->length = event_hd->publisherName.length;
		memcpy(publisherName->value, event_hd->publisherName.value,
			publisherName->length);
	}
	if(publishTime != NULL){
		*(publishTime) = event_hd->publishTime;
	}
	if(eventId != NULL){
		*(eventId) = event_hd->eventId;
	}
	return SA_OK;
}

static SaErrorT send_publish(IPC_Channel *ch,
		SaNameT *channel_name,
		SaEvtEventHandleT eventHandle,
		evt_event_handle *event_hd,
		const void *eventData,
		SaSizeT eventDataSize)
{
	void *msg;
	char *tmp_char;
	SaSizeT str_len, publisher_len, msg_len;	

	str_len = channel_name->length;
	publisher_len = event_hd->publisherName.length;
	msg_len = 1+sizeof(SaSizeT)+str_len+sizeof(SaEvtEventHandleT)+
		sizeof(SaUint8T)+sizeof(SaTimeT)+sizeof(SaSizeT)+
		publisher_len+sizeof(SaTimeT)+sizeof(SaSizeT)+
		event_hd->event_size+sizeof(SaSizeT)+eventDataSize;		
	msg = g_malloc(msg_len);
	if(msg == NULL){
		return SA_ERR_NO_MEMORY;
	}
	tmp_char = (char *)msg;
	*(tmp_char) = EVT_PUBLISH;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, channel_name->value, str_len);
	tmp_char += str_len;
	memcpy(tmp_char, &eventHandle, sizeof(SaEvtEventHandleT));
	tmp_char += sizeof(SaEvtEventHandleT);

	memcpy(tmp_char, &(event_hd->priority), sizeof(SaUint8T));
	tmp_char += sizeof(SaUint8T);
	memcpy(tmp_char, &(event_hd->retentionTime), sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(tmp_char, &publisher_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, event_hd->publisherName.value, publisher_len);
	tmp_char += publisher_len;	
	memcpy(tmp_char, &(event_hd->publishTime), sizeof(SaTimeT));
	tmp_char += sizeof(SaTimeT);
	memcpy(tmp_char, &(event_hd->event_size), sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, event_hd->patternArray, event_hd->event_size);
	tmp_char += event_hd->event_size;
	memcpy(tmp_char, &eventDataSize, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, eventData, eventDataSize);
	tmp_char += eventDataSize;
	send_to_evt_daemon(ch, msg, msg_len);
	return SA_OK;
}

SaErrorT 
saEvtEventPublish(const SaEvtEventHandleT eventHandle,
                  const void *eventData,
                  SaSizeT eventDataSize,
		  SaEvtEventIdT *eventId)
{
	evt_channel_handle *evt_channel_hd;
	evt_event_handle *event_hd;
	evt_handle *evt_hd;
	struct IPC_CHANNEL *ch;
	SaNameT *channel_name;	
	int fd;
	fd_set rset;
	struct daemon_msg *msg_reply;
	struct open_channel_reply *open_ch_reply;
		
	event_hd = g_hash_table_lookup(evt_event_hash, (gpointer)eventHandle);
	if(event_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	if(event_hd->set_flag == 0){
		return SA_ERR_INVALID_PARAM;
	}
	evt_channel_hd = g_hash_table_lookup(evt_channel_hash,
			(gpointer)event_hd->channelId);
	if(evt_channel_hd == NULL){
		return SA_ERR_LIBRARY;
	}
	evt_hd = g_hash_table_lookup(evt_handle_hash,
			(gpointer)(evt_channel_hd->evt_handle));
	if(evt_hd == NULL){
		return SA_ERR_LIBRARY;
	}
	ch = evt_channel_hd->ch;
	channel_name = &(evt_channel_hd->channelName);
	send_publish(ch, channel_name, eventHandle, 
			event_hd, eventData, eventDataSize);
	sleep(1);
	fd = evt_channel_hd->selectionObject;
	
	for(;;){
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		if(select(fd + 1, &rset, NULL,NULL,NULL) == -1){
			/*perror("select"); */
			return(1);
		}
		msg_reply = read_from_ipc(ch);
		if(msg_reply->msg_type == EVT_PUBLISH_REPLY){
			if(eventHandle == 
				msg_reply->private.pub_reply->eventHandle){
				if(msg_reply->private.pub_reply->ret_code
						== SA_OK){
					break;
				}else{
					return msg_reply->private.pub_reply->ret_code;
				}
			}else{
				/*update timeout, continue */
			}
		}else if(msg_reply->msg_type == EVT_NORMAL_EVENT)
		{
			/*TODO: update timeout, continue */
			event_hd = msg_reply->private.event;
			append_to_event_queue(evt_hd->event_queue, event_hd);
		}else if(msg_reply->msg_type == EVT_ASYN_CH_OPEN_REPLY_FROM_DAEMON)
		{
			open_ch_reply = msg_reply->private.open_ch_reply;
			append_to_reply_queue(evt_hd->event_queue,
					open_ch_reply);
		}else
		{
			/*update timeout, continue */
			/*error msg */
		}
		/*if msg_type is normal event, buffer it and continue */
		/*if msg_type is publish_reply, break */
	}

	*(eventId) = (SaEvtEventIdT)msg_reply->private.pub_reply->event_id;
	return SA_OK;	
}

SaErrorT 
saEvtEventSubscribe(const SaEvtChannelHandleT channelHandle,
                    const SaEvtEventFilterArrayT *filters,
                    SaEvtSubscriptionIdT subscriptionId)
{
	evt_channel_handle *evt_channel_hd;
	struct IPC_CHANNEL *ch;
	GHashTable *subscription_hash;
	SaUint8T *tmp_char;
	SaSizeT msg_size;
	SaSizeT str_len, number, i, filter_size=0;
	void *msg;
	SaEvtEventFilterT *filter;
	void *tmp_pointer;
	
	evt_channel_hd = (evt_channel_handle *)g_hash_table_lookup(
			evt_channel_hash, 
			(gpointer)channelHandle);
	if(evt_channel_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	if((evt_channel_hd->open_flags & SA_EVT_CHANNEL_SUBSCRIBER)
			!= SA_EVT_CHANNEL_SUBSCRIBER){
		return SA_ERR_INVALID_PARAM;
	}
	ch = evt_channel_hd->ch;
	subscription_hash = evt_channel_hd->subscription_hash;

	tmp_pointer = g_hash_table_lookup(subscription_global,
				(gpointer)subscriptionId);
	if(tmp_pointer != NULL){
		return SA_ERR_EXIST;
	}
	 
	g_hash_table_insert(subscription_hash, 
			(gpointer)subscriptionId, 
			(gpointer)subscriptionId);
	g_hash_table_insert(subscription_global, 
			(gpointer)subscriptionId, 
			(gpointer)subscriptionId);
	
	/*msg_size= msg_type+ch_name_len+ch_name+channel_id+sub_id+filter_len+filters_number+pattern */
	str_len = evt_channel_hd->channelName.length;
	/* calculate filter length, then copy it to msg */
	number = filters->filtersNumber;
	filter = filters->filters;
	for(i=0; i<number; i++){
		filter_size += filter[i].filter.patternSize;	
	}
	filter_size = sizeof(SaSizeT)+(number*sizeof(SaSizeT))+
		(number*sizeof(SaEvtEventFilterTypeT))+filter_size;
	msg_size = 1+sizeof(SaSizeT)+str_len+sizeof(void *)+
		sizeof(subscriptionId)+sizeof(SaSizeT)+
		filter_size+sizeof(SaEvtChannelHandleT);
	msg = g_malloc(msg_size);
	if(msg == NULL){
		return SA_ERR_NO_MEMORY;
	}
	tmp_char = (SaUint8T *)msg;
	*(tmp_char) = EVT_SUBSCRIBE;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, evt_channel_hd->channelName.value, str_len);
	tmp_char += str_len;
	memcpy(tmp_char, &(evt_channel_hd->ch_instance), sizeof(void *));
	tmp_char += sizeof(void *);
	memcpy(tmp_char, &subscriptionId, sizeof(SaEvtSubscriptionIdT));
	tmp_char += sizeof(SaEvtSubscriptionIdT);
	memcpy(tmp_char, &filter_size, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	memcpy(tmp_char, &number, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	
	for(i=0; i<number; i++){

		memcpy(tmp_char, &(filter[i].filterType), sizeof(SaEvtEventFilterTypeT));
		tmp_char += sizeof(SaEvtEventFilterTypeT);
		memcpy(tmp_char, &(filter[i].filter.patternSize), sizeof(SaSizeT));
		tmp_char += sizeof(SaSizeT);
		memcpy(tmp_char, filter[i].filter.pattern,
				filter[i].filter.patternSize);
		tmp_char += filter[i].filter.patternSize;
	}	
	memcpy(tmp_char, &channelHandle, sizeof(SaEvtChannelHandleT));
	send_to_evt_daemon(ch, msg, msg_size);
	return SA_OK;
}

SaErrorT saEvtEventUnsubscribe(
	SaEvtChannelHandleT channelHandle,
	SaEvtSubscriptionIdT subscriptionId
	)
{
	evt_channel_handle *evt_channel_hd;
	GHashTable *subscription_hash;	
	SaSizeT msg_size;
	SaSizeT str_len;
	void *msg;
	char *tmp_char;
	struct IPC_CHANNEL *ch;
		
	evt_channel_hd = (evt_channel_handle *)g_hash_table_lookup(
			evt_channel_hash, 
			(gpointer)channelHandle);
	if(evt_channel_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	ch = evt_channel_hd->ch;
	subscription_hash = evt_channel_hd->subscription_hash;
	if(g_hash_table_lookup(subscription_hash,(gpointer)subscriptionId)
					 == NULL){
		return SA_ERR_NAME_NOT_FOUND;
	}
	g_hash_table_remove(subscription_hash, (gpointer)subscriptionId);
	/* construct the msg and send to daemon */
	str_len = evt_channel_hd->channelName.length;
	msg_size = 1+sizeof(SaSizeT)+str_len+sizeof(void *)+
		sizeof(SaEvtSubscriptionIdT);
	msg = g_malloc(msg_size);
	if(msg == NULL){
		return SA_ERR_LIBRARY;
	}
	tmp_char = msg;
	*(tmp_char) = EVT_UNSUBSCRIBE;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	strncpy(tmp_char, evt_channel_hd->channelName.value, str_len);
	tmp_char += str_len;
	memcpy(tmp_char, &(evt_channel_hd->ch_instance), sizeof(void *));
	tmp_char += sizeof(void *);
	memcpy(tmp_char, &subscriptionId, sizeof(SaEvtSubscriptionIdT));
	send_to_evt_daemon(ch, msg, msg_size);
	return SA_OK;
}

SaErrorT 
saEvtSelectionObjectGet(const SaEvtHandleT evtHandle,
                        SaSelectionObjectT *selectionObject)
{
	evt_handle *evt_hd;
	
	if(selectionObject == NULL){
		return SA_ERR_INVALID_PARAM;
	}
	evt_hd = g_hash_table_lookup(evt_handle_hash, (gpointer)evtHandle);
	if( evt_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	*selectionObject = evt_hd->selectionObject;
	return SA_OK;
}

static void *read_from_event_queue(struct event_queue_s *event_queue,
		SaUint8T *type)
{
	SaSizeT i;
	struct queue_head *queue;
	evt_event_handle *event_tmp = NULL;
	struct open_channel_reply *reply_tmp = NULL;
	void *ret = NULL;

	if(event_queue->event_number != 0){		
		for(i=SA_EVT_HIGHEST_PRIORITY;i<=SA_EVT_LOWEST_PRIORITY;i++){
			queue = &(event_queue->queue[i]);
			if(queue->head != NULL){
				event_tmp = queue->head;
				queue->head = queue->head->next;
				if(queue->head == NULL){
					queue->tail = NULL;
				}
				event_tmp->next = NULL;
				event_queue->event_number--;
				*(type) = EVT_NORMAL_EVENT;
				ret = (void *)event_tmp;
				break;
			}else{
				continue;
			}		
		}
	}else if(event_queue->reply_number != 0){
		reply_tmp = event_queue->open_ch_reply_queue.head;
		event_queue->open_ch_reply_queue.head = 
			event_queue->open_ch_reply_queue.head->next;
		if(event_queue->open_ch_reply_queue.head == NULL){
			event_queue->open_ch_reply_queue.tail = NULL;
		}
		reply_tmp->next = NULL;
		event_queue->reply_number--;
		*(type) = EVT_CH_OPEN_REPLY_FROM_DAEMON;
		ret = (void *)reply_tmp;
	}	
	return ret;
}

SaErrorT 
saEvtDispatch(const SaEvtHandleT evtHandle,
				SaDispatchFlagsT dispatchFlags)
{
	evt_handle *evt_hd;
	struct IPC_CHANNEL *ch;
	evt_event_handle *event_hd;
	evt_channel_handle *evt_channel_hd;
	struct daemon_msg *msg;
	SaEvtEventHandleT event_handle;
	SaUint8T event_type;
	void *tmp_void;
	struct open_channel_reply *open_ch_reply;
	int fd;
	fd_set rset;
	struct timeval time_out;
	int ret;
	
	evt_hd = (evt_handle *)g_hash_table_lookup(evt_handle_hash,
			(gpointer)evtHandle);
	if( evt_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	if((dispatchFlags < 1) || (dispatchFlags > 3)){
		return SA_ERR_BAD_FLAGS;
	}	
	/*can read two types of message from IPC: */
	/*1 EVT_NORMAL_EVENT */
	/*2 EVT_CH_OPEN_REPLY_FROM_DAEMON: for channel_open_async */
		
	switch(dispatchFlags){
		case SA_DISPATCH_ONE:
			tmp_void = read_from_event_queue(evt_hd->event_queue,
						&event_type);
			if(tmp_void == NULL){
				ch = evt_hd->ch;
				msg = read_from_ipc(ch);
				if(msg == NULL){
					ch->ops->destroy(ch);
					return SA_ERR_LIBRARY;
				}
				event_type = msg->msg_type;
				tmp_void = (void *)msg->private.event;
			}
			if(event_type == EVT_NORMAL_EVENT){
				event_hd = (evt_event_handle *)tmp_void;
				evt_channel_hd = g_hash_table_lookup(
						evt_channel_hash,
						(gpointer)event_hd->channelId);
				if(evt_channel_hd == NULL){
					return SA_ERR_LIBRARY;
				}
				if(get_handle(&event_handle_database,
							&event_handle) != SA_OK){
					return SA_ERR_LIBRARY;
				}
				g_hash_table_insert(evt_event_hash,
					(gpointer)event_handle,
					event_hd);
				g_hash_table_insert(
					evt_channel_hd->event_handle_hash, 
					(gpointer)event_handle,
					event_hd);
				evt_hd->callbacks.saEvtEventDeliverCallback(
					event_hd->subscription_id,
					event_handle,
					event_hd->eventDataSize);
			}else if(event_type ==
					EVT_ASYN_CH_OPEN_REPLY_FROM_DAEMON){
				open_ch_reply = (struct open_channel_reply *)
					tmp_void;
				evt_channel_hd = g_hash_table_lookup(
						evt_channel_hash, 
						(gpointer)open_ch_reply->clt_ch_handle);
				if(evt_channel_hd == NULL){
					/*TODO: free open_ch_reply */
					return SA_ERR_LIBRARY;
				}
				evt_channel_hd->ch_instance = 
					open_ch_reply->ch_instance;
				evt_hd->callbacks.saEvtChannelOpenCallback(
						evt_channel_hd->invocation,
						open_ch_reply->clt_ch_handle,
						open_ch_reply->ret_code);
			}
			break;
		case SA_DISPATCH_ALL:
			fd = evt_hd->selectionObject;
			time_out.tv_sec = 0;
			for(;;){
				tmp_void = read_from_event_queue(
						evt_hd->event_queue,
						&event_type);
				if(tmp_void == NULL){
					FD_ZERO(&rset);
					FD_SET(fd, &rset);
					ret = select(fd + 1, &rset,NULL,NULL,
							&time_out);
					if( ret == -1){
						/*error */
						return SA_ERR_LIBRARY;
					}else if(ret == 0){
						return SA_OK;
					}
					ch = evt_hd->ch;
					msg = read_from_ipc(ch);
					if(msg == NULL){
						ch->ops->destroy(ch);
						return SA_ERR_LIBRARY;
					}
					event_type = msg->msg_type;
					tmp_void = (void *)msg->private.event;	
				}
				if(event_type == EVT_NORMAL_EVENT){
					event_hd = (evt_event_handle *)
						tmp_void;
					evt_channel_hd = g_hash_table_lookup(
						evt_channel_hash,
						(gpointer)event_hd->channelId);
					if(evt_channel_hd == NULL){
						return SA_ERR_LIBRARY;
					}
					if(get_handle(&event_handle_database,
							&event_handle) != SA_OK){
						return SA_ERR_LIBRARY;
					}
					g_hash_table_insert(evt_event_hash,
						(gpointer)event_handle,
						event_hd);
					g_hash_table_insert(
					  evt_channel_hd->event_handle_hash, 
					  (gpointer)event_handle,
					  event_hd);
					evt_hd->callbacks.
						saEvtEventDeliverCallback(
						event_hd->subscription_id,
						event_handle,
						event_hd->eventDataSize);
				}else if(event_type == 
					EVT_ASYN_CH_OPEN_REPLY_FROM_DAEMON){
					open_ch_reply = 
						(struct open_channel_reply *)
						tmp_void;
					evt_channel_hd = g_hash_table_lookup(
						evt_channel_hash, 
						(gpointer)open_ch_reply->clt_ch_handle);
					if(evt_channel_hd == NULL){
						/*TODO: free open_ch_reply */
						return SA_ERR_LIBRARY;
					}
					evt_channel_hd->ch_instance = 
						open_ch_reply->ch_instance;
					evt_hd->callbacks.
						saEvtChannelOpenCallback(
						evt_channel_hd->invocation,
						open_ch_reply->clt_ch_handle,
						open_ch_reply->ret_code);
				}
			}
			break;
		case SA_DISPATCH_BLOCKING:
			break;
	}
	return SA_OK;
}

SaErrorT
saEvtEventDataGet(SaEvtEventHandleT eventHandle,
		void *eventData,
		SaSizeT *eventDataSize)
{
	evt_event_handle *event_hd;
	SaSizeT data_size;
	
	event_hd = (evt_event_handle *)g_hash_table_lookup(evt_event_hash,
			(gpointer)(eventHandle));
	if(event_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	data_size = (event_hd->eventDataSize > *eventDataSize)
		? *eventDataSize : event_hd->eventDataSize;
	*eventDataSize = data_size;
	memcpy(eventData, event_hd->eventData, data_size);
	return SA_OK;
}

SaErrorT saEvtChannelUnlink(SaEvtHandleT evtHandle,
		const SaNameT *channelName)
{
	void *msg;
	char *tmp_char;
	SaSizeT str_len, msg_len;
	evt_handle *evt_hd;
	
	evt_hd = (evt_handle *)g_hash_table_lookup(evt_handle_hash,
					(gpointer)(long)evtHandle);
	if(evt_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	str_len = channelName->length;
	msg_len = sizeof(char)+sizeof(SaSizeT)+str_len;
	msg = g_malloc(msg_len);
	if(msg == NULL){
		return SA_ERR_NO_MEMORY;
	}	
	tmp_char = (char *)msg;
	*(tmp_char) = EVT_CHANNEL_UNLINK;
	tmp_char++;
	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	strncpy(tmp_char, channelName->value, str_len);
	send_to_evt_daemon(evt_hd->ch, msg, msg_len);
	return SA_OK;
}

SaErrorT saEvtEventRetentionTimeClear(
	SaEvtChannelHandleT channelHandle,
	const SaEvtEventIdT eventId
	)
{
	evt_channel_handle *evt_channel_hd;
	SaSizeT str_len, msg_size;
	void *msg;
	char *tmp_char;	
	struct IPC_CHANNEL *ch;
	int fd;
	fd_set rset;
	struct daemon_msg *msg_reply;
	struct clear_retention_time_reply *clear_reply;
	evt_event_handle *event_hd;
	struct open_channel_reply *open_ch_reply;
	evt_handle *evt_hd;
	
	evt_channel_hd = (evt_channel_handle *)g_hash_table_lookup(
			evt_channel_hash, 
			(gpointer)(long)channelHandle);
	if(evt_channel_hd == NULL){
		return SA_ERR_BAD_HANDLE;
	}
	evt_hd = (evt_handle *)g_hash_table_lookup(evt_handle_hash,
			(gpointer)(long)(evt_channel_hd->evt_handle));
	if(evt_hd == NULL){
		return SA_ERR_LIBRARY;
	}
	/*msg_size=1+sizeof(SaSizeT)+str_len+sizeof(SaEvtEventIdT) */
	str_len = evt_channel_hd->channelName.length;
	msg_size = 1+sizeof(SaSizeT)+str_len+sizeof(SaEvtEventIdT);
	msg = g_malloc(msg_size);
	tmp_char = (char *)msg;
	*(tmp_char) = EVT_CLEAR_RETENTION_TIME;
	tmp_char++;

	memcpy(tmp_char, &str_len, sizeof(SaSizeT));
	tmp_char += sizeof(SaSizeT);
	strncpy(tmp_char, evt_channel_hd->channelName.value, str_len);
	tmp_char += str_len;
	memcpy(tmp_char, &eventId, sizeof(SaEvtEventIdT));
	send_to_evt_daemon(evt_channel_hd->ch, msg, msg_size);	
	/*select wait until receiving the reply	 */
	ch = evt_channel_hd->ch;
	fd = evt_channel_hd->selectionObject;
	for(;;){
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		if(select(fd + 1, &rset, NULL,NULL,NULL) == -1){
			/*perror("select"); */
			return SA_ERR_LIBRARY;
		}
		msg_reply = read_from_ipc(ch);
		if(msg_reply->msg_type == EVT_CLEAR_RETENTION_TIME_REPLY){
			clear_reply = msg_reply->private.clear_retention_reply;
			if(clear_reply->event_id == eventId){
				return clear_reply->ret_code;
			}else{
				/*error */
				/*continue to wait reply from daemon */
				continue;
			}
		}else if(msg_reply->msg_type == EVT_NORMAL_EVENT){
			event_hd = msg_reply->private.event;
			append_to_event_queue(evt_hd->event_queue, event_hd);
		}else if(msg_reply->msg_type == EVT_ASYN_CH_OPEN_REPLY_FROM_DAEMON){
			open_ch_reply = msg_reply->private.open_ch_reply;
			append_to_reply_queue(evt_hd->event_queue,
					open_ch_reply);
		}
	}
}




