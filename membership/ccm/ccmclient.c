/* 
 * client.c: Consensus Cluster Client tracker
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
 * 
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <lha_internal.h>
#include <ccm.h>
#include <ccmlib.h>

typedef struct ccm_client_s {
	int 	ccm_clid;
	int 	ccm_flags;
	struct  IPC_CHANNEL *ccm_ipc_client;
} ccm_client_t;

#define CL_INIT   	0x0
#define CL_LLM    	0x1
#define CL_MEM    	0x2
#define CL_ERROR    	0x4


typedef struct ccm_ipc_s {
	int		   count;
	struct IPC_MESSAGE ipcmsg;/*this should be the last field*/
} ccm_ipc_t;

static ccm_ipc_t *ipc_llm_message  = NULL; /* active low level membership */
static ccm_ipc_t *ipc_mem_message  = NULL; /* active membership           */
static ccm_ipc_t *ipc_misc_message = NULL; /* active misc information     */

#define MAXIPC 100

static gboolean membership_ready     = FALSE;
static void refresh_llm_msg(llm_info_t *llm);


/* 
 * the fully initialized clients.
 */
static GHashTable  *ccm_hashclient = NULL;



static void 
send_message(ccm_client_t *ccm_client, ccm_ipc_t *msg)
{
	int send_rc = IPC_OK;
	struct IPC_CHANNEL *chan = ccm_client->ccm_ipc_client;

	++(msg->count);

	do {
		if (chan->ops->get_chan_status(chan) == IPC_CONNECT){
			send_rc = chan->ops->send(chan, &(msg->ipcmsg));
		}
		if(send_rc != IPC_OK){
			if (chan->ops->get_chan_status(chan) != IPC_CONNECT){
				ccm_debug(LOG_WARNING, "Channel is dead.  Cannot send message.");
				break;
			}else {
				cl_shortsleep();				
			}
		}
		
		
	} while(send_rc == IPC_FAIL);
	
	return;
}

static void 
send_func(gpointer key, gpointer value, gpointer user_data)
{
	ccm_client_t  	   *ccm_client = (ccm_client_t *)value;
	int msg_type = GPOINTER_TO_INT(user_data);
	switch (msg_type) {
	case CCM_EVICTED:
		if(ccm_client->ccm_flags == CL_MEM) {
			struct IPC_CHANNEL* chan = ccm_client->ccm_ipc_client;
			
			if (chan->ops->get_chan_status(chan) == IPC_CONNECT){
				send_message(ccm_client, ipc_misc_message);
			}else {
				/* IPC is broken, the client is already gone
				 * Do nothing
				 */
			}
			ccm_client->ccm_flags = CL_INIT;
		}
		break;
	case CCM_INFLUX:
		send_message(ccm_client, ipc_misc_message);
		break;
	case CCM_NEW_MEMBERSHIP:
		if(membership_ready) {
			send_message(ccm_client, ipc_llm_message);
			send_message(ccm_client, ipc_mem_message);
		}
		break;
	default:
		ccm_log(LOG_ERR, "send_func:unknown message");
	}
}

static void
delete_message(ccm_ipc_t *ccmipc)
{
	g_free(ccmipc);
}

static 
void  
send_func_done(struct IPC_MESSAGE *ipcmsg)
{
	ccm_ipc_t *ccmipc = (ccm_ipc_t *)ipcmsg->msg_private;
	int count = --(ccmipc->count);

	if(count==0){
		delete_message(ccmipc);
	}
	return;
}


static ccm_ipc_t *
create_message(void *data, int size)
{
	ccm_ipc_t *ipcmsg;

	ipcmsg = g_malloc(sizeof(ccm_ipc_t)+size);

	ipcmsg->count = 0;
	
	memset(&ipcmsg->ipcmsg, 0, sizeof(IPC_Message));
	
	ipcmsg->ipcmsg.msg_body = ipcmsg+1;
	memcpy(ipcmsg->ipcmsg.msg_body, data, size);

	ipcmsg->ipcmsg.msg_len     = size;
	ipcmsg->ipcmsg.msg_done    = send_func_done;
	ipcmsg->ipcmsg.msg_private = ipcmsg;
	ipcmsg->ipcmsg.msg_buf     = NULL;

	return ipcmsg;
}


static void 
send_all(int msg_type)
{
	if(g_hash_table_size(ccm_hashclient)) {
		g_hash_table_foreach(ccm_hashclient, send_func, GINT_TO_POINTER(msg_type));
	}
	return;
}

static void 
flush_func(gpointer key, gpointer value, gpointer user_data)
{
	struct IPC_CHANNEL *ipc_client = (struct IPC_CHANNEL *)key;
	while(ipc_client->ops->is_sending_blocked(ipc_client)) {
		ccm_debug(LOG_WARNING, "ipc channel blocked");
		cl_shortsleep();
		if(ipc_client->ops->resume_io(ipc_client) == IPC_BROKEN) {
			break;
		}
	}
}

static void 
flush_all(void)
{
	if(g_hash_table_size(ccm_hashclient)) {
		g_hash_table_foreach(ccm_hashclient, flush_func, NULL);
	}
	return;
}

static void
cleanup(void)
{
	membership_ready=FALSE;
	flush_all(); /* flush out all the messages to all the clients*/
	if (ipc_mem_message) {
		delete_message(ipc_mem_message);
	}
	if (ipc_misc_message) {
		delete_message(ipc_misc_message);
	}
	ipc_mem_message = NULL;
	ipc_misc_message = NULL;

	/* NOTE: ipc_llm_message is never destroyed. */
	/* Also, do not free the client structure. */

	return;
}


void
client_init(void)
{
	if(ccm_hashclient) {
		ccm_log(LOG_INFO, "client already initialized");
		return;
	}
	ccm_hashclient =  g_hash_table_new(g_direct_hash, 
				g_direct_equal);

	return;
}

int
client_add(struct IPC_CHANNEL *ipc_client)
{
	ccm_client_t  *ccm_client;

	if(!ccm_hashclient) {
		ccm_log(LOG_ERR, "client subsystem not initialized");
		return -1;
	}

	ccm_client =  (ccm_client_t *)
		g_malloc(sizeof(ccm_client_t));

	ccm_client->ccm_clid = 0; /* don't care, TOBEDONE */
	ccm_client->ccm_ipc_client = ipc_client;
	ccm_client->ccm_flags = CL_INIT;

	send_func(ipc_client, ccm_client, (gpointer)CCM_NEW_MEMBERSHIP);

	g_hash_table_insert(ccm_hashclient, ipc_client, ccm_client);
	return 0;
}

static void
client_destroy(struct IPC_CHANNEL *ipc_client)
{
	ccm_client_t  *ccm_client;
	if((ccm_client = g_hash_table_lookup(ccm_hashclient, ipc_client))
	!=	NULL){
		g_free(ccm_client);
	}
	/* IPC_Channel is automatically destroyed when channel is disconnected */
}

void
client_delete(struct IPC_CHANNEL *ipc_client)
{

	g_hash_table_remove(ccm_hashclient, ipc_client);
	client_destroy(ipc_client);
	return;
}

static gboolean 
destroy_func(gpointer key, gpointer value, gpointer user_data)
{
	struct IPC_CHANNEL *ipc_client = (struct IPC_CHANNEL *)key;

	client_destroy(ipc_client);
	return TRUE;
}

void
client_delete_all(void)
{
	if(g_hash_table_size(ccm_hashclient)) {
		g_hash_table_foreach_remove(ccm_hashclient, destroy_func, NULL);
	}
	return;
}


static gboolean
get_quorum(ccm_info_t* info)
{
	if (info->has_quorum != -1) {
		return info->has_quorum;
	}
	return ccm_calculate_quorum(info);
}

static void
display_func(gpointer key, gpointer value, gpointer user_data)
{
	ccm_client_t * ccm_client = (ccm_client_t*) value;
	ccm_debug(LOG_DEBUG, "client: pid =%d", ccm_client->ccm_ipc_client->farside_pid);	
	
	return;
}

void
client_new_mbrship(ccm_info_t* info, void* borndata)
{
	/* creating enough heap memory in order to avoid allocation */
	static struct born_s	bornbuffer[MAXNODE+10];
	ccm_meminfo_t *ccm=(ccm_meminfo_t *)bornbuffer;
	struct born_s *born_arry = (struct born_s *)borndata;
	int		n = info->memcount;
	int		trans = info->ccm_transition_major;
	int*		member = info->ccm_member;
	int i, j;
	
	assert( n<= MAXNODE);

	membership_ready=TRUE;


	ccm->ev = CCM_NEW_MEMBERSHIP;
	ccm->n = n;
	ccm->trans = trans;
	ccm->quorum = get_quorum(info);
	(void)get_quorum;
	ccm_debug(LOG_DEBUG, "quorum is %d", ccm->quorum);


	for (i = 0; i < n; i++) {
		ccm->member[i].index = member[i];
		ccm->member[i].bornon = -1;
		for (j = 0; j < n; j ++) {
			if (born_arry[j].index == ccm->member[i].index) {
				ccm->member[i].bornon = born_arry[j].bornon;
			}
		}
	}	

	if(ipc_mem_message && --(ipc_mem_message->count)==0){
		delete_message(ipc_mem_message);
	}
	ipc_mem_message = create_message(ccm, 
 			(sizeof(ccm_meminfo_t) + n*sizeof(born_t)));
	ipc_mem_message->count++;
	refresh_llm_msg(&info->llm);
#if 1
	ccm_debug(LOG_DEBUG, "delivering new membership to %d clients: ",
	       g_hash_table_size(ccm_hashclient));
	if(g_hash_table_size(ccm_hashclient)){
		g_hash_table_foreach(ccm_hashclient, display_func, NULL);	
	}
#else
	(void)display_func;
#endif 
	
	send_all(CCM_NEW_MEMBERSHIP);
	ccm_debug2(LOG_DEBUG, "membership state: new membership");
}


void
client_influx(void)
{
	int type = CCM_INFLUX;

	if(membership_ready){
		membership_ready = FALSE;
		if(ipc_misc_message && --(ipc_misc_message->count)==0){
			delete_message(ipc_misc_message);
		}
		ipc_misc_message = create_message(&type, sizeof(int));
		ipc_misc_message->count++;
		send_all(CCM_INFLUX);
	}

	ccm_debug2(LOG_DEBUG, "membership state: not primary");
}


void
client_evicted(void)
{
	int type = CCM_EVICTED;
	if(ipc_misc_message && --(ipc_misc_message->count)==0){
		delete_message(ipc_misc_message);
	}
	ipc_misc_message = create_message(&type, sizeof(int));
	ipc_misc_message->count++;
	send_all(CCM_EVICTED);

	cleanup();
	ccm_debug2(LOG_DEBUG, "membership state: evicted");
}

void 
client_llm_init(llm_info_t *llm)
{
	refresh_llm_msg(llm);
	return;
}

void 
refresh_llm_msg(llm_info_t *llm)
{
	int  maxnode = llm_get_nodecount(llm);
       	int size = sizeof(ccm_llm_t)+ maxnode*sizeof(struct node_s);
	ccm_llm_t *data = (ccm_llm_t *)g_malloc(size);
	int  i;

	data->ev = CCM_LLM;
	/* copy the relevent content of llm into data */
	CLLM_SET_NODECOUNT(data,maxnode);
	CLLM_SET_MYNODE(data, llm_get_myindex(llm));
	for ( i = 0; i < maxnode; i ++ ) {
		CLLM_SET_NODEID(data,i,llm_get_nodename(llm,i));
		CLLM_SET_UUID(data,i,i);

	}

	if(ipc_llm_message && --(ipc_llm_message->count)==0){
		delete_message(ipc_llm_message);
	}
	ipc_llm_message = create_message(data, size);
	ipc_llm_message->count++;
	g_free(data);
	
	return;
}

