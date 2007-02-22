/* 
 * ccmlib_memapi.c: Consensus Cluster Membership API
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <lha_internal.h>
#define __CCM_LIBRARY__
#include <ccmlib.h>
#include <ccm.h>
/*#include <syslog.h> */
#include <clplumbing/cl_log.h> 

/* structure to track the membership delivered to client */
typedef struct mbr_track_s {
	int			quorum;
	int			m_size;
	oc_ev_membership_t 	m_mem;
} mbr_track_t;


typedef struct mbr_private_s {
	int			magiccookie;
	gboolean		client_report; 	   /* report to client */
	oc_ev_callback_t 	*callback; /* the callback function registered
					      	by the client */
	struct IPC_CHANNEL      *channel; /* the channel to talk to ccm */
	ccm_llm_t 		*llm;	 /* list of all nodes */
	GHashTable  		*bornon; /* list of born time 
					    for all nodes */
	void 			*cookie; /* the last known 
					    membership event cookie */
	gboolean		special; /* publish non primary membership. 
					  * This is a kludge to accomodate 
					  * special behaviour not provided 
					  * but desired from the 0.2 API. 
					  * By default this behaviour is 
					  * turned off.
					  */
} mbr_private_t;


static char event_strings[5][32]={
	"OC_EV_MS_INVALID",
	"OC_EV_MS_NEW_MEMBERSHIP",
	"OC_EV_MS_NOT_PRIMARY",
	"OC_EV_MS_PRIMARY_RESTORED",
	"OC_EV_MS_EVICTED"
};

#define EVENT_STRING(x) event_strings[x - OC_EV_MS_INVALID]
#define OC_EV_SET_INSTANCE(m,trans)  m->m_mem.m_instance=trans
#define OC_EV_SET_N_MEMBER(m,n)  m->m_mem.m_n_member=n
#define OC_EV_SET_MEMB_IDX(m,idx)  m->m_mem.m_memb_idx=idx
#define OC_EV_SET_N_OUT(m,n)  m->m_mem.m_n_out=n
#define OC_EV_SET_OUT_IDX(m,idx)  m->m_mem.m_out_idx=idx
#define OC_EV_SET_N_IN(m,n)  m->m_mem.m_n_in=n
#define OC_EV_SET_IN_IDX(m,idx)  m->m_mem.m_in_idx=idx
#define OC_EV_SET_NODEID(m,idx,nodeid)  m->m_mem.m_array[idx].node_id=nodeid
#define OC_EV_SET_BORN(m,idx,born)  m->m_mem.m_array[idx].node_born_on=born

#define OC_EV_INC_N_MEMBER(m)  m->m_mem.m_n_member++
#define OC_EV_INC_N_IN(m)  m->m_mem.m_n_in++
#define OC_EV_INC_N_OUT(m)  m->m_mem.m_n_out++


#define OC_EV_SET_SIZE(m,size)  m->m_size=size
#define OC_EV_SET_DONEFUNC(m,f)  m->m_func=f

#define OC_EV_GET_INSTANCE(m)  m->m_mem.m_instance
#define OC_EV_GET_N_MEMBER(m)  m->m_mem.m_n_member
#define OC_EV_GET_MEMB_IDX(m)  m->m_mem.m_memb_idx
#define OC_EV_GET_N_OUT(m)  m->m_mem.m_n_out
#define OC_EV_GET_OUT_IDX(m)  m->m_mem.m_out_idx
#define OC_EV_GET_N_IN(m)  m->m_mem.m_n_in
#define OC_EV_GET_IN(m)  m->m_mem.m_in_idx
#define OC_EV_GET_NODEARRY(m)  m->m_mem.m_array
#define OC_EV_GET_NODE(m,idx)  m->m_mem.m_array[idx]
#define OC_EV_GET_NODEID(m,idx)  m->m_mem.m_array[idx].node_id
#define OC_EV_GET_BORN(m,idx)  m->m_mem.m_array[idx].node_born_on

#define OC_EV_COPY_NODE_WITHOUT_UNAME(m1,idx1,m2,idx2)  \
		m1->m_mem.m_array[idx1]=m2->m_mem.m_array[idx2]

#define OC_EV_COPY_NODE(m1,idx1,m2,idx2)  \
		m1->m_mem.m_array[idx1]=m2->m_mem.m_array[idx2]; \
		m1->m_mem.m_array[idx1].node_uname = \
				strdup(m2->m_mem.m_array[idx2].node_uname)

#define OC_EV_GET_SIZE(m)  m->m_size


/* prototypes of external functions used in this file 
 * Should be made part of some header file 
 */
void *cookie_construct(void (*f)(void *), void (*free_f)(void *), void *);
void * cookie_get_data(void *ck);
void * cookie_get_func(void *ck);
void cookie_ref(void *ck);
void cookie_unref(void *ck);
static const char *llm_get_Id_from_Uuid(ccm_llm_t *stuff, uint uuid);



static void
on_llm_msg(mbr_private_t *mem, struct IPC_MESSAGE *msg)
{
	unsigned long len = msg->msg_len;
	
	if (mem->llm != NULL) {
		g_free(mem->llm);
	}
	mem->llm = (ccm_llm_t *)g_malloc(len);
	memcpy(mem->llm, msg->msg_body, len);
	return;
}



static void
reset_bornon(mbr_private_t *private)
{
	g_hash_table_destroy(private->bornon);
	private->bornon = NULL;
}

static void
reset_llm(mbr_private_t *private)
{
	g_free(private->llm);
	private->llm = NULL;
}

static int
init_llm(mbr_private_t *private)
{
	struct IPC_CHANNEL *ch;
	int 	sockfd, ret;
	struct IPC_MESSAGE *msg;

	if(private->llm) {
		return 0;
	}

	ch 	   = private->channel;
	sockfd = ch->ops->get_recv_select_fd(ch);
	while(1) {
		if(ch->ops->waitin(ch) != IPC_OK){
			ch->ops->destroy(ch);
			return -1;
		}
		ret = ch->ops->recv(ch,&msg);
		if(ret == IPC_BROKEN) {
			fprintf(stderr, "connection denied\n");
			return -1;
		}
 		if(ret == IPC_FAIL){
			fprintf(stderr, ".");
			cl_shortsleep();
			continue;
		}
		break;
	}
	on_llm_msg(private, msg);
	
	private->bornon = g_hash_table_new(g_direct_hash, 
				g_direct_equal);
	private->cookie = NULL;
	
	private->client_report = TRUE;
	msg->msg_done(msg);
	return 0;
}


static gboolean
class_valid(class_t *class)
{
	mbr_private_t 	*private;

	if(class->type != OC_EV_MEMB_CLASS) {
		return FALSE;
	}

	private = (mbr_private_t *)class->private;

	if(!private || 
		private->magiccookie != 0xabcdef){
		return FALSE;
	}
	return TRUE;
}



static gboolean
already_present(oc_node_t *arr, uint size, oc_node_t node)
{
	uint i;
	for ( i = 0 ; i < size ; i ++ ) {
		if(arr[i].node_id == node.node_id) {
			return TRUE;
		}
	}
	return FALSE;
}

static int
compare(const void *value1, const void *value2)
{
	const oc_node_t *t1 = (const oc_node_t *)value1;
	const oc_node_t *t2 = (const oc_node_t *)value2;
	
	if (t1->node_born_on < t2->node_born_on){
		return -1;
	}

	if (t1->node_born_on > t2->node_born_on){
		return 1;
	}

	if (t1->node_id < t2->node_id) {
		return -1;
	}

	if (t1->node_id > t2->node_id) {
		return 1;
	}
	
	return 0;
}

static const char *
llm_get_Id_from_Uuid(ccm_llm_t *stuff, uint uuid)
{
	uint lpc = 0;
	for (; lpc < stuff->n; lpc++) {
		if(stuff->node[lpc].Uuid == uuid){		       
			return stuff->node[lpc].Id;
		}
	}
	return NULL;
}



static int
get_new_membership(mbr_private_t *private,
		ccm_meminfo_t *mbrinfo,
		int		len,
		mbr_track_t **mbr)
{
	mbr_track_t *newmbr, *oldmbr;
	int trans, i, j, in_index, out_index, born;
	int n_members;
	
	int n_nodes = CLLM_GET_NODECOUNT(private->llm);
	
	int size    = sizeof(oc_ev_membership_t) + 
		2*n_nodes*sizeof(oc_node_t);

 	newmbr = *mbr = (mbr_track_t *)g_malloc(size +
						sizeof(mbr_track_t)-sizeof(newmbr->m_mem));
	
	trans = OC_EV_SET_INSTANCE(newmbr,mbrinfo->trans);
	n_members = OC_EV_SET_N_MEMBER(newmbr,mbrinfo->n);
	OC_EV_SET_SIZE(newmbr, size);

	j = OC_EV_SET_MEMB_IDX(newmbr,0);

	for ( i = 0 ; i < n_members; i++ ) {
		const char *uname = NULL;
		int	index;

		index = mbrinfo->member[i].index;

		uname = llm_get_Id_from_Uuid(private->llm, index);

		newmbr->m_mem.m_array[j].node_uname = strdup(uname); 
		
		OC_EV_SET_NODEID(newmbr,j,index);

		/* gborn was an int to begin with - so this is safe */
		born = mbrinfo->member[i].bornon;

		/* if there is already a born entry for the
		 * node, use it. Otherwise create a born entry
		 * for the node.
	 	 *
		 * NOTE: born==0 implies the entry has not been
		 * 	initialized.
		 */
		OC_EV_SET_BORN(newmbr,j, born);
		j++;
	}
	/* sort the m_arry */
	qsort(OC_EV_GET_NODEARRY(newmbr), n_members, 
			sizeof(oc_node_t), compare);

	in_index = OC_EV_SET_IN_IDX(newmbr,j);
	out_index = OC_EV_SET_OUT_IDX(newmbr,(j+n_nodes));

	OC_EV_SET_N_IN(newmbr,0);
	OC_EV_SET_N_OUT(newmbr,0);

	oldmbr = (mbr_track_t *) cookie_get_data(private->cookie);

	if(oldmbr) {
		for ( i = 0 ; i < n_members; i++ ) {
			if(!already_present(OC_EV_GET_NODEARRY(oldmbr),
					OC_EV_GET_N_MEMBER(oldmbr),
					OC_EV_GET_NODE(newmbr,i))){
				OC_EV_COPY_NODE_WITHOUT_UNAME(newmbr
				,	in_index, newmbr, i);
				in_index++;
				OC_EV_INC_N_IN(newmbr);
			}
		}

		for ( i = 0 ; (uint)i < OC_EV_GET_N_MEMBER(oldmbr) ; i++ ) {
			if(!already_present(OC_EV_GET_NODEARRY(newmbr), 
					OC_EV_GET_N_MEMBER(newmbr), 
					OC_EV_GET_NODE(oldmbr,i))){
				OC_EV_COPY_NODE(newmbr, out_index, oldmbr, i);
				out_index++;
				OC_EV_INC_N_OUT(newmbr);
			}
		}
	} else {
		OC_EV_SET_IN_IDX(newmbr,0);
		OC_EV_SET_N_IN(newmbr,OC_EV_GET_N_MEMBER(newmbr));
	}

	return size;
}

static void
mem_free_func(void *data)
{
	unsigned lpc = 0;
	char * uname;
	mbr_track_t  *mbr_track =  (mbr_track_t *)data;

	if(mbr_track) {
		/* free m_n_member uname, m_n_in is actually the same ptr */
		for (lpc = 0 ; lpc < OC_EV_GET_N_MEMBER(mbr_track); lpc++ ) {
			if ((uname = OC_EV_GET_NODE(mbr_track, lpc).node_uname)){
				g_free(uname);
			}
		}
		/* free m_n_out uname */
		for (lpc = OC_EV_GET_OUT_IDX(mbr_track)
		;	lpc < OC_EV_GET_OUT_IDX(mbr_track)
			+	OC_EV_GET_N_OUT(mbr_track)
		;	lpc++) {

			if ((uname = OC_EV_GET_NODE(mbr_track, lpc).node_uname)){
				g_free(uname);
			}
		}
		g_free(mbr_track);
	}
	

	return;
}

static void
mem_callback_done(void *cookie)
{
	cookie_unref(cookie);
	return;
}



static void
update_bornons(mbr_private_t *private, mbr_track_t *mbr)
{
	uint i,j;
	for(i=0; i < OC_EV_GET_N_MEMBER(mbr); i++) {
		g_hash_table_insert(private->bornon,
			GINT_TO_POINTER(OC_EV_GET_NODEID(mbr,i)),
			GINT_TO_POINTER(OC_EV_GET_BORN(mbr,i)+1));
	}
	j=OC_EV_GET_OUT_IDX(mbr); 
	for(i=OC_EV_GET_OUT_IDX(mbr); i<j+OC_EV_GET_N_OUT(mbr); i++){
		g_hash_table_insert(private->bornon,
			GINT_TO_POINTER(OC_EV_GET_NODEID(mbr,i)),
			GINT_TO_POINTER(0));
	}
}

static gboolean
membership_unchanged(mbr_private_t *private, mbr_track_t *mbr)
{
	uint i;
	mbr_track_t *oldmbr = (mbr_track_t *) 
				cookie_get_data(private->cookie);

	if(!oldmbr) {
		return FALSE;
	}

	if(OC_EV_GET_N_MEMBER(mbr) != OC_EV_GET_N_MEMBER(oldmbr)){
			return FALSE;
	}

	for(i=0; i < OC_EV_GET_N_MEMBER(mbr); i++) {
		if((OC_EV_GET_NODEID(mbr,i) 
				!= OC_EV_GET_NODEID(oldmbr,i)) ||
		  OC_EV_GET_BORN(mbr,i) 
		  		!= OC_EV_GET_BORN(oldmbr,i)) { 
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean	 
mem_handle_event(class_t *class)
{
	struct IPC_MESSAGE *msg;
	mbr_private_t *private;
	struct IPC_CHANNEL *ch;
	mbr_track_t *mbr_track;
	int	size;
	int	type;
	oc_memb_event_t oc_type;
	void   *cookie;
	int ret;
	gboolean quorum;

	if(!class_valid(class)){
		return FALSE;
	}

	private = (mbr_private_t *)class->private;
	ch 	   = private->channel;

	if(init_llm(private)){
		return FALSE;
	}
	
	
	
	while(ch->ops->is_message_pending(ch)){
		/* receive the message and call the callback*/
		ret=ch->ops->recv(ch,&msg);
		
		if(ret != IPC_OK){
			/* If IPC is broken
			 * the we return FALSE, which results in removing of 
			 * this class in handle function
			 * This should only happen when ccm is shutdown before the client
			 *
			 */
			cl_log(LOG_INFO, "mem_handle_func:IPC broken, ccm is dead before the client!");
			return FALSE;
		}
		
		type = ((ccm_meminfo_t *)msg->msg_body)->ev;


		cookie= mbr_track = NULL;
		size=0;
		oc_type = OC_EV_MS_INVALID;

		switch(type) {
		case CCM_NEW_MEMBERSHIP :{
			
			ccm_meminfo_t* cmi = (ccm_meminfo_t*)msg->msg_body;
			
			size = get_new_membership(private, 
						  cmi,
						  msg->msg_len, 
						  &mbr_track);

			
			mbr_track->quorum = quorum = cmi->quorum;
			
			/* if no quorum, delete the bornon dates for lost 
			 * nodes, add  bornon dates for the new nodes and 
			 * return
			 *
			 * however if special behaviour is being asked
			 * for report the membership even when this node
			 * has no quorum.
			 */
			if (!private->special && !quorum){
				update_bornons(private, mbr_track);
				private->client_report = FALSE;
				mem_free_func(mbr_track);
				break;
			}
			private->client_report = TRUE;

			/* if quorum and old membership is same as the new 
			* membership set type to OC_EV_MS_RESTORED , 
			* pick the old membership and deliver it. 
			* Do not construct a new membership  
			*/
			if (membership_unchanged(private, mbr_track)){
				mbr_track_t* old_mbr_track;
				
				old_mbr_track = (mbr_track_t *)
					cookie_get_data(private->cookie);
				
				if (mbr_track->quorum == old_mbr_track->quorum){
					oc_type = OC_EV_MS_PRIMARY_RESTORED;
				}else {
					cl_log(LOG_DEBUG, "membership unchanged but quorum changed");
					oc_type = quorum?
						OC_EV_MS_NEW_MEMBERSHIP:
						OC_EV_MS_INVALID; 
				}

			} else {
				oc_type = quorum?
					OC_EV_MS_NEW_MEMBERSHIP:
					OC_EV_MS_INVALID; 
				/* NOTE: OC_EV_MS_INVALID overloaded to
				 * mean that the membership has no quorum.
				 * This is returned only when special behaviour
				 * is asked for. In normal behaviour case 
				 * (as per 0.2 version of the api),
				 * OC_EV_MS_INVALID is never returned.
				 * I agree this is a kludge!!
				 */
				if(!private->special) {
				  assert(oc_type == OC_EV_MS_NEW_MEMBERSHIP);
				}
			}

			update_bornons(private, mbr_track);
			cookie_unref(private->cookie);
			cookie = cookie_construct(mem_callback_done, 
						  mem_free_func, mbr_track);
			private->cookie = cookie;
			size = OC_EV_GET_SIZE(mbr_track);
			break;
		}
		case CCM_EVICTED:
			oc_type = OC_EV_MS_EVICTED;
			private->client_report = TRUE;
			
			size = 0;
			mbr_track = NULL;

			if (private->cookie){
				cookie_unref(private->cookie);
			}
			cookie= cookie_construct(mem_callback_done, NULL,NULL);
			if ( cookie == NULL){
				cl_log(LOG_ERR, "mem_handle_event: coookie construction failed");
				abort();
			}
			private->cookie=cookie;
			break;
			
		case CCM_INFLUX:

			if(type==CCM_INFLUX){
				oc_type = OC_EV_MS_NOT_PRIMARY;
			}


			cookie = private->cookie;
			if(cookie) {
				mbr_track = (mbr_track_t *)
					cookie_get_data(cookie);
				size=mbr_track? OC_EV_GET_SIZE(mbr_track): 0;
			} else {
				/* if no cookie exists, create one.
				 * This can happen if no membership 
				 * has been delivered.
				 */
				mbr_track=NULL;
				size=0;
				cookie = private->cookie = 
					cookie_construct(mem_callback_done,
						NULL,NULL);
			}
			break;
		case CCM_LLM:
			on_llm_msg(private, msg);
		}

		
		cl_log(LOG_INFO, "%s: Got an event %s from ccm"
		,	__FUNCTION__
		,	EVENT_STRING(oc_type));
#define ALAN_DEBUG 1
#ifdef ALAN_DEBUG
		if (!mbr_track) {
			cl_log(LOG_INFO, "%s: no mbr_track info"
			,	__FUNCTION__);
		}else{
			cl_log(LOG_INFO
			,	"%s: instance=%d, nodes=%d, new=%d, lost=%d"
			", n_idx=%d, new_idx=%d, old_idx=%d"
			,	__FUNCTION__
			,	mbr_track->m_mem.m_instance
			,	mbr_track->m_mem.m_n_member
			,	mbr_track->m_mem.m_n_in
			,	mbr_track->m_mem.m_n_out
			,	mbr_track->m_mem.m_memb_idx
			,	mbr_track->m_mem.m_in_idx
			,	mbr_track->m_mem.m_out_idx);
		}
#endif

		if(private->callback && private->client_report && cookie){
			cookie_ref(cookie);
			private->callback(oc_type,
				(uint *)cookie,
				size,
				mbr_track?&(mbr_track->m_mem):NULL);
		}

		if(ret==IPC_OK) {
			msg->msg_done(msg);
		} else {
			return FALSE;
		}

		if(type == CCM_EVICTED) {
			/* clean up the dynamic information in the 
			 * private structure 
			 */
			reset_llm(private);
			reset_bornon(private);
			cookie_unref(private->cookie);
			private->cookie = NULL;
 		}
	}
	return TRUE;
}


static int	 
mem_activate(class_t *class)
{
	mbr_private_t *private;
	struct IPC_CHANNEL *ch;
	int sockfd;

	if(!class_valid(class)) {
		return -1;
	}

	/* if already activated */

	private = (mbr_private_t *)class->private;
	if(private->llm){
		return -1;
	}

	ch 	   = private->channel;

	if(!ch || ch->ops->initiate_connection(ch) != IPC_OK) {
		return -1;
	}

	ch->ops->set_recv_qlen(ch, 0);
	sockfd = ch->ops->get_recv_select_fd(ch);

	return sockfd;
}


static void
mem_unregister(class_t *class)
{
	mbr_private_t  *private;
	struct IPC_CHANNEL *ch;

	private = (mbr_private_t *)class->private;
	ch 	   = private->channel;

	/* TOBEDONE
	 * call all instances, of message done
	 * on channel ch.
	 */

	ch->ops->destroy(ch);

	g_free(private->llm);
	g_free(private);
}


static oc_ev_callback_t *
mem_set_callback(class_t *class, oc_ev_callback_t f)
{
	mbr_private_t 	*private;
	oc_ev_callback_t *ret_f;

	if(!class_valid(class)){
		return NULL;
	}

	private = (mbr_private_t *)class->private;
	
	ret_f = private->callback;
	private->callback = f;

	return ret_f;
}


/* this function is a kludge, to accomodate special behaviour not
 * supported by 0.2 version of the API 
 */
static void
mem_set_special(class_t *class, int type)
{
	mbr_private_t 	*private;

	if(!class_valid(class)) {
		return;
	}

	private = (mbr_private_t *)class->private;
	
	private->special = 1; /* turn on the special behaviour not supported
				 	by 0.2 version of the API */


	return;
}

static gboolean
mem_is_my_nodeid(class_t *class, const oc_node_t *node)
{
	mbr_private_t 	*private;

	if(!class_valid(class)){
		return FALSE;
	}
	private = (mbr_private_t *)class->private;

	if (node->node_id == CLLM_GET_MYUUID(private->llm)){
		return TRUE;
	}
	
	return FALSE;
}


class_t *
oc_ev_memb_class(oc_ev_callback_t  *fn)
{
	mbr_private_t 	*private;
	class_t *memclass;
	struct IPC_CHANNEL *ch;
	GHashTable * attrs;
	static char 	path[] = IPC_PATH_ATTR;
	static char 	ccmfifo[] = CCMFIFO;

	memclass = g_malloc(sizeof(class_t));

	if (!memclass){
		return NULL;
	}

	private = (mbr_private_t *)g_malloc0(sizeof(mbr_private_t));
	if (!private) {
		g_free(memclass);
		return NULL;
	}

	memclass->type = OC_EV_MEMB_CLASS;
	memclass->set_callback  =  mem_set_callback;
	memclass->handle_event  =  mem_handle_event;
	memclass->activate = mem_activate;
	memclass->unregister = mem_unregister;
	memclass->is_my_nodeid = mem_is_my_nodeid;
	memclass->special = mem_set_special;

	memclass->private = (void *)private;
	private->callback = fn;
	private->magiccookie = 0xabcdef;
	private->client_report = FALSE;
	private->special = 0; /* no special behaviour */
	private->llm = NULL; 
	
	attrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(attrs, path, ccmfifo);
	ch = ipc_channel_constructor(IPC_DOMAIN_SOCKET, attrs);
	g_hash_table_destroy(attrs);

	if(!ch) {
		g_free(memclass);
		g_free(private);
		return NULL;
	}

	private->channel = ch;

	return memclass;
}
