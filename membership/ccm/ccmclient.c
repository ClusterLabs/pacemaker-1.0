/* $Id: ccmclient.c,v 1.32 2005/12/09 20:15:31 gshi Exp $ */
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
#include <ccm.h>
#include <ccmlib.h>
#include <clplumbing/cl_plugin.h>
#include <clplumbing/cl_quorum.h>
#include <clplumbing/cl_tiebreaker.h>

typedef struct ccm_client_s {
	int 	ccm_clid;
	int 	ccm_flags;
	struct  IPC_CHANNEL *ccm_ipc_client;
} ccm_client_t;

#define CL_INIT   	0x0
#define CL_LLM    	0x1
#define CL_MEM    	0x2
#define CL_ERROR    	0x4

extern int global_verbose;
extern int global_debug;

typedef struct ccm_ipc_s {
	int		   count;
	GMemChunk	   *chkptr;
	struct IPC_MESSAGE ipcmsg;/*this should be the last field*/
} ccm_ipc_t;

static ccm_ipc_t *ipc_llm_message  = NULL; /* active low level membership */
static ccm_ipc_t *ipc_mem_message  = NULL; /* active membership           */
static ccm_ipc_t *ipc_born_message = NULL; /* active bornon information   */
static ccm_ipc_t *ipc_misc_message = NULL; /* active misc information     */

#define MAXIPC 100

static GMemChunk *ipc_mem_chk  = NULL;
static GMemChunk *ipc_born_chk = NULL;
static GMemChunk *ipc_misc_chk = NULL;

static gboolean llm_flag      = FALSE;
static gboolean evicted_flag  = FALSE;
static gboolean prim_flag     = FALSE;
static gboolean restored_flag = FALSE;


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
				cl_log(LOG_WARNING, "Channel is dead.  Cannot send message.");
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

	if(evicted_flag) {
		/*send evicted message*/
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
		return;
	}

	switch(ccm_client->ccm_flags) {
	case CL_INIT:
		if(!llm_flag) {
			break;
		}
		/* send llm message */
		send_message(ccm_client, ipc_llm_message);
		ccm_client->ccm_flags = CL_LLM;
		/* 
		 * FALL THROUGH
		 */
	case CL_LLM:
		if(prim_flag) {
			/* send born message */
			send_message(ccm_client, ipc_born_message);
			/* send mem message */
			send_message(ccm_client, ipc_mem_message);
			ccm_client->ccm_flags = CL_MEM;
		} 
		break;

	case CL_MEM:
		if(restored_flag){
			/* send restored message */
			send_message(ccm_client, 
					ipc_misc_message);
		}else if(prim_flag) {
			/* send mem message */
			send_message(ccm_client, 
					ipc_mem_message);
		}else {
			/* send nonprimary message */
			send_message(ccm_client, 
					ipc_misc_message);
		}
		break;
	default :
		break;
	}
}

static void
delete_message(ccm_ipc_t *ccmipc)
{
	GMemChunk  *chkptr = ccmipc->chkptr;
	if(chkptr){
		g_mem_chunk_free(chkptr, ccmipc);
		ccmipc->chkptr = NULL;
	}
}

static 
void  send_func_done(struct IPC_MESSAGE *ipcmsg)
{
	ccm_ipc_t *ccmipc = (ccm_ipc_t *)ipcmsg->msg_private;
	int count = --(ccmipc->count);

	if(count==0){
		delete_message(ccmipc);
	}
	return;
}


static ccm_ipc_t *
create_message(GMemChunk *chk, void *data, int size)
{
	ccm_ipc_t *ipcmsg;

	if(chk){
		ipcmsg = g_chunk_new(ccm_ipc_t, chk);
	} else {
		ipcmsg = g_malloc(sizeof(ccm_ipc_t)+size);
	}

	ipcmsg->chkptr = chk;
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
send_all(void)
{
	if(g_hash_table_size(ccm_hashclient)) {
		g_hash_table_foreach(ccm_hashclient, send_func, NULL);
	}
	return;
}

static void 
flush_func(gpointer key, gpointer value, gpointer user_data)
{
	struct IPC_CHANNEL *ipc_client = (struct IPC_CHANNEL *)key;
	while(ipc_client->ops->is_sending_blocked(ipc_client)) {
		cl_log(LOG_WARNING, "ipc channel blocked");
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
	evicted_flag=FALSE;
	prim_flag=FALSE;
	restored_flag=FALSE;
	flush_all(); /* flush out all the messages to all the clients*/
	g_mem_chunk_reset(ipc_mem_chk);
	g_mem_chunk_reset(ipc_born_chk);
	g_mem_chunk_reset(ipc_misc_chk);
	ipc_mem_message = NULL;
	ipc_born_message = NULL;
	ipc_misc_message = NULL;

	/* NOTE: ipc_llm_message is never destroyed. */
	/* Also, do not free the client structure. */

	return;
}


void
client_init(void)
{
	if(ccm_hashclient) {
		cl_log(LOG_INFO, "ccm: client already initialized");
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
		cl_log(LOG_ERR, "ccm: client subsystem not initialized");
		return -1;
	}

	ccm_client =  (ccm_client_t *)
		g_malloc(sizeof(ccm_client_t));

	ccm_client->ccm_clid = 0; /* don't care, TOBEDONE */
	ccm_client->ccm_ipc_client = ipc_client;
	ccm_client->ccm_flags = CL_INIT;

	send_func(ipc_client, ccm_client, NULL);

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
	static struct hb_quorum_fns* funcs = NULL;
	static struct hb_tiebreaker_fns* tiebreaker_funcs = NULL;
	const char* quorum_plugin = "classic";
	const char* tiebreaker_plugin = NULL;
	int rc;

	if (funcs == NULL){
		funcs = cl_load_plugin("quorum", quorum_plugin);
		if (funcs == NULL){
			cl_log(LOG_ERR, "%s: loading plugin %s failed",
			       __FUNCTION__, quorum_plugin);
			return FALSE;
		}
	}
	
        rc = funcs->getquorum(info->memcount, info->llm.nodecount);

	if (rc == QUORUM_YES){
		return TRUE;
	}else if (rc ==  QUORUM_NO){
		return FALSE;
	}
	
	if (tiebreaker_funcs == NULL){
		if ( tiebreaker_plugin == NULL){
			return FALSE;
		}
		
		tiebreaker_funcs = cl_load_plugin("tiebreaker", tiebreaker_plugin);
		if (tiebreaker_funcs == NULL){
			cl_log(LOG_ERR, "%s: loading plugin %s failed",
			       __FUNCTION__, tiebreaker_plugin);
			return FALSE;
		}
	}
	
	return tiebreaker_funcs->break_tie(info->memcount, info->llm.nodecount);
	
}

static void
display_func(gpointer key, gpointer value, gpointer user_data)
{
	ccm_client_t * ccm_client = (ccm_client_t*) value;
	cl_log(LOG_INFO, "client: pid =%d", ccm_client->ccm_ipc_client->farside_pid);	
	
	return;
}

void
client_new_mbrship(ccm_info_t* info, void* borndata)
{
	/* creating enough heap memory in order to avoid allocation */
	static struct born_s	bornbuffer[MAXNODE+10];
	ccm_meminfo_t *ccm=(ccm_meminfo_t *)bornbuffer;
	ccm_born_t    *born=(ccm_born_t *)bornbuffer;
	struct born_s *born_arry = (struct born_s *)borndata;
	int		n = info->memcount;
	int		trans = info->ccm_transition_major;
	int*		member = info->ccm_member;
	
	assert( n<= MAXNODE);

	prim_flag=TRUE;
	restored_flag=FALSE;


	ccm->ev = CCM_NEW_MEMBERSHIP;
	ccm->n = n;
	ccm->trans = trans;
	ccm->quorum = get_quorum(info);
	(void)get_quorum;
	cl_log(LOG_INFO, "quorum is %d", ccm->quorum);

	memcpy(ccm->member, member, n*sizeof(int));

	if(ipc_mem_message && --(ipc_mem_message->count)==0){
		delete_message(ipc_mem_message);
	}
	ipc_mem_message = create_message(ipc_mem_chk, ccm, 
 			(sizeof(ccm_meminfo_t) + n*sizeof(int)));
	ipc_mem_message->count++;

	/* bornon array is sent in a seperate message */
	born->n = n;
	memcpy(born->born, born_arry, n*sizeof(struct born_s));
	if(ipc_born_message && --(ipc_born_message->count)==0){
		delete_message(ipc_born_message);
	}
	ipc_born_message = create_message(ipc_born_chk, born, 
					  sizeof(ccm_born_t )+n*sizeof(struct born_s));
	ipc_born_message->count++;
	
#if 1
	cl_log(LOG_INFO, "delivering new membership to %d clients: ",
	       g_hash_table_size(ccm_hashclient));
	if(g_hash_table_size(ccm_hashclient)){
		g_hash_table_foreach(ccm_hashclient, display_func, NULL);	
	}
#else
	(void)display_func;
#endif 
	
	send_all();
	if(global_verbose) {
		cl_log(LOG_DEBUG, "membership state: new membership");
	}
}


void
client_influx(void)
{
	int type = CCM_INFLUX;

	if(prim_flag){
		prim_flag = FALSE;
		restored_flag = FALSE;
		if(ipc_misc_message && --(ipc_misc_message->count)==0){
			delete_message(ipc_misc_message);
		}
		ipc_misc_message = create_message(ipc_misc_chk, 
				&type, sizeof(int));
		ipc_misc_message->count++;
		send_all();
	}

	if(global_verbose)
		cl_log(LOG_DEBUG, "membership state: not primary");
}


void
client_evicted(void)
{
	int type = CCM_EVICTED;
	evicted_flag=TRUE;
	if(llm_flag) {
		if(ipc_misc_message && --(ipc_misc_message->count)==0){
			delete_message(ipc_misc_message);
		}
		ipc_misc_message = create_message(ipc_misc_chk, 
				&type, sizeof(int));
		ipc_misc_message->count++;
		send_all();
	}
	cleanup();
	if(global_verbose)
		cl_log(LOG_DEBUG, "membership state: evicted");
}


void 
client_llm_init(llm_info_t *llm)
{
	char memstr[] = "membership chunk";
	char bornstr[] = "born chunk";
	char miscstr[] = "misc chunk";
	int  maxnode = llm_get_nodecount(llm);
       	int size = sizeof(ccm_llm_t)+ maxnode*sizeof(struct node_s);
	ccm_llm_t *data = (ccm_llm_t *)g_malloc(size);
	int  i;

	/* copy the relevent content of llm into data */
	CLLM_SET_NODECOUNT(data,maxnode);
	CLLM_SET_MYNODE(data, llm_get_myindex(llm));
	for ( i = 0; i < maxnode; i ++ ) {
		CLLM_SET_NODEID(data,i,llm_get_nodename(llm,i));
		CLLM_SET_UUID(data,i,i);

	}

	ipc_llm_message = create_message(NULL, data, size);
	g_free(data);
	ipc_llm_message->count++; /* make sure it never gets
				     	dellocated */
	llm_flag = TRUE;

	ipc_mem_chk = g_mem_chunk_new(memstr,
				sizeof(ccm_ipc_t)+
				sizeof(ccm_meminfo_t)+
				maxnode*sizeof(int), 
				MAXIPC, G_ALLOC_AND_FREE);
	ipc_born_chk = g_mem_chunk_new(bornstr,
				sizeof(ccm_ipc_t)+
				sizeof(ccm_born_t)+
				maxnode*sizeof(struct born_s), 
				MAXIPC, G_ALLOC_AND_FREE);
	ipc_misc_chk = g_mem_chunk_new(miscstr,
				sizeof(ccm_ipc_t)+
				sizeof(int), 
				MAXIPC, G_ALLOC_AND_FREE);

	/* check if anybody is waiting */
	send_all();
	return;
}
