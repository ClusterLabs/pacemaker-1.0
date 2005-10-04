/* $Id: ccm_statemachine.c,v 1.6 2005/10/04 15:45:49 gshi Exp $ */
/* 
 * ccm.c: Consensus Cluster Service Program 
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
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
#include <config.h>
#include <ha_config.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>
#include "ccmmsg.h"
#include "ccmmisc.h"

extern int global_verbose;
extern int global_debug;


int ccm_send_node_msg(ll_cluster_t* hb, struct ha_msg* msg,  const char* node);
int ccm_send_cluster_msg(ll_cluster_t* hb, struct ha_msg* msg);



/* PROTOTYPE */
static void
ccm_reset_all_join_request(ccm_info_t* info);
static void report_reset(void);
static int ccm_already_joined(ccm_info_t *);
static void ccm_memcomp_reset(ccm_info_t *);

/* For enhanced membership service */
static void append_change_msg(ccm_info_t *info,const char *node);
static int received_all_change_msg(ccm_info_t *info);
static int is_expected_change_msg(ccm_info_t *info, const char *node,
		enum change_event_type);
static void add_change_msg(ccm_info_t *info, const char *node, 
		const char *orig, enum change_event_type);
static void update_membership(ccm_info_t *info, const char *node, 
		enum change_event_type change_type);
static void reset_change_info(ccm_info_t *info); 
static void send_mem_list_to_all(ll_cluster_t *hb, ccm_info_t *info, 
		char *cookie);
static void ccm_fill_update_table(ccm_info_t *info, 
		ccm_update_t *update_table, const void *uptime_list);


static longclock_t change_time;
static gboolean gl_membership_converged = FALSE;

const char state_strings[12][64]={
	"CCM_STATE_NONE",
	"CCM_STATE_VERSION_REQUEST",
	"CCM_STATE_JOINING",  		
	"CCM_STATE_SENT_MEMLISTREQ",					
	"CCM_STATE_MEMLIST_RES",				
	"CCM_STATE_JOINED",    
	"CCM_STATE_WAIT_FOR_MEM_LIST",
	"CCM_STATE_WAIT_FOR_CHANGE",
	"CCM_STATE_NEW_NODE_WAIT_FOR_MEM_LIST",
	"CCM_STATE_END"		
};

const char*
state2string(int state){
	if (state > CCM_STATE_END){
		return "INVALID STATE";
	}
	
	return state_strings[state];
}

static int
string2state(const char* state_str)
{
	int  i; 
	
	if (state_str == NULL){
		cl_log(LOG_ERR, "%s: state_str is NULL", __FUNCTION__);
		return -1;
	}

	for (i = 0 ; i < DIMOF(state_strings); i++){
		if (strncmp(state_strings[i], state_str, 64) == 0){
			return i;
		}		
	}
	
	cl_log(LOG_ERR, "%s: Cannot find a match for string %s", 
	       __FUNCTION__, state_str);

	return -1;
	
}


static void
ccm_set_state(ccm_info_t* info, int istate,const struct ha_msg*  msg)	
{									
			int leader = info->ccm_cluster_leader;		
			
			cl_log(LOG_INFO,"change state from %s to %s, current leader is %s",   
			       state2string(info->state),state2string(istate), 
			       leader < 0 ?"none": info->llm.nodes[leader].nodename); 
			
			if (ANYDEBUG){
				if (msg) {
					cl_log_message(LOG_DEBUG, msg);		
				}else{
					cl_log(LOG_DEBUG, "Trigging msg is NULL");
				}
			}
			
			info->state = (istate);		
			if((istate)==CCM_STATE_JOINING){
				client_influx();	
			}		

			if (istate == CCM_STATE_JOINED){
				gl_membership_converged =TRUE;
			}
}




static void
change_time_init(void)
{
	change_time = ccm_get_time();
}
static int
change_timeout(unsigned long timeout)
{
	return(ccm_timeout(change_time, ccm_get_time(), timeout));
}

static longclock_t mem_list_time;
static void
mem_list_time_init(void)
{
	mem_list_time = ccm_get_time();
}
static int
mem_list_timeout(unsigned long timeout)
{
	return(ccm_timeout(mem_list_time, ccm_get_time(), timeout));
}

static longclock_t  new_node_mem_list_time;
static void new_node_mem_list_time_init(void)
{
    new_node_mem_list_time = ccm_get_time();
}
static int new_node_mem_list_timeout(unsigned long timeout)
{
    return(ccm_timeout(new_node_mem_list_time, ccm_get_time(), timeout));
}

#define CCM_GET_MYNODE_ID(info) \
	info->llm.nodes[info->llm.myindex].nodename
#define CCM_GET_CL_NODEID(info) \
	info->llm.nodes[CCM_GET_CL(info)].nodename 
#define CCM_GET_RECEIVED_CHANGE_MSG(info, node) \
	CCM_GET_LLM(info)->nodes[info->ccm_member[ccm_get_membership_index(info, node)]].received_change_msg
#define CCM_SET_RECEIVED_CHANGE_MSG(info, node, value) \
	CCM_GET_LLM(info)->nodes[info->ccm_member[ccm_get_membership_index(info, node)]].received_change_msg = value

/*
////////////////////////////////////////////////////////////////
// BEGIN OF Functions associated with CCM token types that are
// communicated accross nodes and their values.
////////////////////////////////////////////////////////////////
*/

static void ccm_state_wait_for_mem_list(enum ccm_type ccm_msg_type, 
			struct ha_msg *reply, 
			ll_cluster_t *hb, 
			ccm_info_t *info);
static void ccm_state_new_node_wait_for_mem_list(enum ccm_type ccm_msg_type, 
	              struct ha_msg *reply, 
	              ll_cluster_t *hb, 
			ccm_info_t *info);





/* END OF TYPE_STR datastructure and associated functions */


/* */
/* timeout configuration function */
/* */
static void
ccm_configure_timeout(ll_cluster_t *hb, ccm_info_t *info)
{
	long keepalive = hb->llc_ops->get_keepalive(hb);

	if(global_debug) {
		cl_log(LOG_INFO, "ccm_configure_timeout  "
			"keepalive=%ld", keepalive);
	}

	CCM_TMOUT_SET_U(info, 9*keepalive);
	CCM_TMOUT_SET_LU(info, 30*keepalive);
	CCM_TMOUT_SET_VRS(info, 9*keepalive);
	CCM_TMOUT_SET_ITF(info, 18*keepalive);
	CCM_TMOUT_SET_IFF(info, 12*keepalive);
	CCM_TMOUT_SET_FL(info, CCM_TMOUT_GET_ITF(info)+5);
}


/* */
/* ccm_get_my_hostname: return my nodename. */
/* */
static char *
ccm_get_my_hostname(ccm_info_t *info)
{
	llm_info_t *llm = CCM_GET_LLM(info);
	return(LLM_GET_MYNODEID(llm));
}


/* */
/* timeout_msg_create:  */
/*	fake up a timeout message, which is in the */
/* 	same format as the other messages that are */
/*	communicated across the nodes. */
/* */




#ifdef TIMEOUT_MSG_FUNCTIONS_NEEDED
/* */
/* timeout_msg_done:  */
/*   done with the processing of this message. */
static void
timeout_msg_done(void)
{
	/* nothing to do. */
	return;
}


/* */
/* timeout_msg_del:  */
/*   delete the given timeout message. */
/*   nobody calls this function.  */
/*   someday somebody will call it :) */
static void
timeout_msg_del(void)
{
	ha_msg_del(timeout_msg);
	timeout_msg = NULL;
}
#endif


/* */
/* These are the function that keep track of number of time a version */
/* response message has been dropped. These function are consulted by */
/* the CCM algorithm to determine if a version response message has */
/* to be dropped or not. */
/* */
static int respdrop=0;
#define MAXDROP 3

static int
resp_can_i_drop(void)
{
	if (respdrop >= MAXDROP){
		return FALSE;
	}
	return TRUE;
}

static void
resp_dropped(void)
{
	respdrop++;
}

static void
resp_reset(void)
{
	respdrop=0;
}
/* */
/* End of response processing messages. */
/* */


/* */
/* BEGIN OF functions that track the time since a connectivity reply has */
/* been sent to the leader. */
/* */
static longclock_t finallist_time;

static void
finallist_init(void)
{
	finallist_time = ccm_get_time();
}

static void
finallist_reset(void)
{
	finallist_time = 0;
}

static int
finallist_timeout(unsigned long timeout)
{
	return(ccm_timeout(finallist_time, ccm_get_time(), timeout));
}
/* */
/* END OF functions that track the time since a connectivity reply has */
/* been sent to the leader. */
/* */







/* Reset all the datastructures. Go to a state which is equivalent */
/* to a state when the node is just about to join a cluster. */
void 
ccm_reset(ccm_info_t *info)
{

	if(ccm_already_joined(info)){
		client_evicted();
	}

	CCM_RESET_MEMBERSHIP(info);
	ccm_memcomp_reset(info);
	CCM_SET_ACTIVEPROTO(info, CCM_VER_NONE);
	CCM_SET_COOKIE(info,"");
	CCM_SET_MAJORTRANS(info,0);
	CCM_SET_MINORTRANS(info,0);
	CCM_SET_CL(info,-1);
	CCM_SET_JOINED_TRANSITION(info, 0);
	ccm_set_state(info, CCM_STATE_NONE, NULL);
	update_reset(CCM_GET_UPDATETABLE(info));
	ccm_reset_all_join_request(info);
	version_reset(CCM_GET_VERSION(info));
	finallist_reset();
	leave_reset();
	report_reset();
}

static void 
ccm_init(ccm_info_t *info)
{
	update_init(CCM_GET_UPDATETABLE(info));
	ccm_reset_all_join_request(info);
	CCM_INIT_MAXTRANS(info);
        leave_init();
        (void)timeout_msg_init(info);
	ccm_reset(info);
}



/*
 * BEGIN OF ROUTINES THAT REPORT THE MEMBERSHIP TO CLIENTS.
 */
static void
report_reset(void)
{
	return;
}

/* */
/* print and report the cluster membership to clients. */
/* */
static void
report_mbrs(ccm_info_t *info)
{
	int i;
	char *nodename;

	static struct born_s  {
		int index;
		int bornon;
	}  bornon[MAXNODE];/*avoid making it a 
				stack variable*/
	

	if(CCM_GET_MEMCOUNT(info)==1){
		bornon[0].index  = CCM_GET_MEMINDEX(info,0);
		bornon[0].bornon = CCM_GET_MAJORTRANS(info);
	} else for(i=0; i < CCM_GET_MEMCOUNT(info); i++){
		bornon[i].index = CCM_GET_MEMINDEX(info,i);
		bornon[i].bornon = update_get_uptime(CCM_GET_UPDATETABLE(info), 
						     CCM_GET_LLM(info),
						     CCM_GET_MEMINDEX(info,i));
		if(bornon[i].bornon==0) 
			bornon[i].bornon=CCM_GET_MAJORTRANS(info);
		assert(bornon[i].bornon!=-1);
	}
	
	cl_log(LOG_DEBUG,"\t\t the following are the members " 
	       "of the group of transition=%d",
	       CCM_GET_MAJORTRANS(info));
	
	for (i=0 ;  i < CCM_GET_MEMCOUNT(info); i++) {
		nodename = LLM_GET_NODEID(CCM_GET_LLM(info), 
					  CCM_GET_MEMINDEX(info,i));
		cl_log(LOG_DEBUG,"\t\tnodename=%s bornon=%d", nodename, 
		       bornon[i].bornon);
	}

	/* 
	 * report to clients, the new membership 
	 */
	client_new_mbrship(info,
			   bornon);
	return;
}
/*
 * END OF ROUTINES THAT REPORT THE MEMBERSHIP TO CLIENTS.
 */




/* */
/* generate a random cookie. */
/* NOTE: cookie  is  a  mechanism  of  seperating out the contexts */
/* of  messages  of  partially  partitioned  clusters. */
/* For example, consider  a  case  where   node  A  is  physically */
/* in  the  partition  X  and  partition  Y,  and  but  has joined  */
/* membership  in partition X. It will end up getting ccm protocol */
/* messages  sent  by  members in both the partitions. In order to  */
/* seperate  out  messages  belonging  to  individual partition, a  */
/* random  string  is  used  as  a identifier by each partition to  */
/* identify  its  messages.  In  the above case A will get message  */
/* from  both  the  partitions  but  only listens to messages from  */
/* partition X and drops messages from partition Y. */
/* */
static char *
ccm_generate_random_cookie(void)
{
	char *cookie;
	int i;
	struct timeval tmp;

	cookie = g_malloc(COOKIESIZE*sizeof(char));
	/* g_malloc never returns NULL: assert(cookie); */

	/* seed the random with a random value */
	gettimeofday(&tmp, NULL);
	srandom((unsigned int)tmp.tv_usec); 

	for ( i = 0 ; i < COOKIESIZE-1; i++ ) {
		cookie[i] = random()%(127-'!')+'!';
	}
	cookie[i] = '\0';
	return cookie;
}


static void
ccm_free_random_cookie(char *cookie)
{
	assert(cookie && *cookie);
	g_free(cookie);
}



/* BEGIN OF FUNCTIONS that keep track of connectivity  information  */
/* conveyed by individual members of the cluster. These  functions  */
/* are used by only the cluster leader. Ultimately these connectivity */
/* information is used by the cluster to extract out the members */
/* of the cluster that have total connectivity. */
static int
ccm_memcomp_cmpr(gconstpointer a, gconstpointer b)
{
	return(*((const uint32_t *)a)-*((const uint32_t *)b));
}
static void
ccm_memcomp_free(gpointer data, gpointer userdata)
{
	if(data) {
		g_free(data);
	}
	return;
}

static void
ccm_memcomp_note(ccm_info_t *info, const char *orig, 
		uint32_t maxtrans, const char *memlist)
{
	int index, numbytes;
	char *bitmap = NULL;
	uint32_t *ptr;
	memcomp_t *mem_comp = CCM_GET_MEMCOMP(info);

	bitmap_create(&bitmap, MAXNODE);
	if (bitmap == NULL){
		cl_log(LOG_ERR, "bitmap creatation failed");
		return;
	}
	/* find the index of the originator */
	index = llm_get_index(CCM_GET_LLM(info), orig);
	
	/* convert the memlist into a bit map and feed it to the graph */
	numbytes = ccm_str2bitmap(memlist, strlen(memlist), bitmap);
	
	graph_update_membership(MEMCOMP_GET_GRAPH(mem_comp), 
				index, bitmap);
	/*NOTE DO NOT DELETE bitlist, because it is 
	 * being handled by graph*/

	ptr = (uint32_t *)g_malloc(2*sizeof(uint32_t));
	ptr[0] = maxtrans;
	ptr[1] = index;
	MEMCOMP_SET_MAXT(mem_comp, 
		(g_slist_insert_sorted(MEMCOMP_GET_MAXT(mem_comp), 
			ptr, ccm_memcomp_cmpr)));
	return;
}

/* called by the cluster leader only  */
static void
ccm_memcomp_note_my_membership(ccm_info_t *info)
{
	char memlist[MAX_MEMLIST_STRING];
	int str_len;

	str_len = update_strcreate(CCM_GET_UPDATETABLE(info), 
			memlist, CCM_GET_LLM(info));
	ccm_memcomp_note(info, ccm_get_my_hostname(info), 
			CCM_GET_MAXTRANS(info), memlist);
	return;
}

/* add a new member to the membership list */
static void
ccm_memcomp_add(ccm_info_t *info, const char *orig)
{
	int index, myindex;
	memcomp_t *mem_comp = CCM_GET_MEMCOMP(info);

	index = llm_get_index(CCM_GET_LLM(info), orig);
	myindex = llm_get_index(CCM_GET_LLM(info), 
			ccm_get_my_hostname(info));
	graph_add_uuid(MEMCOMP_GET_GRAPH(mem_comp), index);
	graph_add_to_membership(MEMCOMP_GET_GRAPH(mem_comp), 
				myindex, index);
	/* ccm_memcomp_note(info, orig, maxtrans, memlist); */
	return;
}

static void 
ccm_memcomp_init(ccm_info_t *info)
{
	int track=-1;
	int index;
	
	memcomp_t *mem_comp = CCM_GET_MEMCOMP(info);

	MEMCOMP_SET_GRAPH(mem_comp, graph_init());

	/* go through the update list and note down all the members who
	 * had participated in the join messages. We should be expecting
	 * reply memlist bitmaps atleast from these nodes.
	 */
	while((index = update_get_next_index(CCM_GET_UPDATETABLE(info), 
				CCM_GET_LLM(info), &track)) != -1) {
		graph_add_uuid(MEMCOMP_GET_GRAPH(mem_comp),index); 
	}
	MEMCOMP_SET_MAXT(mem_comp,  NULL);
	MEMCOMP_SET_INITTIME(mem_comp, ccm_get_time());
}


static void 
ccm_memcomp_reset(ccm_info_t *info)
{
	GSList *head;
	memcomp_t *mem_comp = CCM_GET_MEMCOMP(info);

	graph_free(MEMCOMP_GET_GRAPH(mem_comp));
	MEMCOMP_SET_GRAPH(mem_comp,NULL);
	head = MEMCOMP_GET_MAXT(mem_comp);
	g_slist_foreach(MEMCOMP_GET_MAXT(mem_comp), 
			ccm_memcomp_free, NULL);
	g_slist_free(MEMCOMP_GET_MAXT(mem_comp));
	MEMCOMP_SET_MAXT(mem_comp,  NULL);
	return;
}


static int
ccm_memcomp_rcvd_all(ccm_info_t *info)
{
	return graph_filled_all(MEMCOMP_GET_GRAPH(CCM_GET_MEMCOMP(info)));
}

static int
ccm_memcomp_timeout(ccm_info_t *info, long timeout)
{
	memcomp_t *mem_comp = CCM_GET_MEMCOMP(info);

	return(ccm_timeout(MEMCOMP_GET_INITTIME(mem_comp), 
				ccm_get_time(), timeout));
}

static int
ccm_memcomp_get_maxmembership(ccm_info_t *info, char **bitmap)
{
	GSList *head;
	uint32_t *ptr;
	int 	uuid;

	memcomp_t *mem_comp = CCM_GET_MEMCOMP(info);

	(void)graph_get_maxclique(MEMCOMP_GET_GRAPH(mem_comp), 
			bitmap);

	head = MEMCOMP_GET_MAXT(mem_comp);

	while (head) {
		ptr = (uint32_t *)g_slist_nth_data(head, 0);
		uuid = ptr[1];
		if(bitmap_test(uuid, *bitmap, MAXNODE)) {
			return ptr[0];
		}
		head = g_slist_next(head);
	}
	return 0;
}


/* */
/* END OF the membership tracking functions. */
/* */


static int 
ccm_am_i_member(ccm_info_t *info, const char *memlist)
{
	char *bitmap = NULL;
	int numBytes, myindex;
	llm_info_t *llm;

	bitmap_create(&bitmap, MAXNODE);
	if (bitmap == NULL){
		cl_log(LOG_ERR ," bitmap creatation failed");
		return FALSE;
	}
	
	numBytes = ccm_str2bitmap(memlist, strlen(memlist), bitmap);
	
	/* what is my node Uuid */
	llm = CCM_GET_LLM(info);

	myindex = llm_get_myindex(llm);
	
	if (bitmap_test(myindex, bitmap, numBytes*BitsInByte)){
		bitmap_delete(bitmap);
		return TRUE;
	}

	bitmap_delete(bitmap);
	return FALSE;
}




static int
ccm_get_index(ccm_info_t* info, const char* node)
{
	uint i;
	llm_info_t *llm = CCM_GET_LLM(info);
	for ( i = 0 ; i < llm->nodecount ; i++ ) {
		if(strncmp(LLM_GET_NODEID(llm, i), node, 
			   LLM_GET_NODEIDSIZE(llm)) == 0){
			return i;
		}
	}
	return -1;
	
	
}

static int
ccm_am_i_leader(ccm_info_t *info)
{
	llm_info_t *llm = CCM_GET_LLM(info);
	
	if ( LLM_GET_MYNODE(llm) == CCM_GET_CL(info)){
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
node_is_leader(ccm_info_t* info, const char* nodename)
{
	return( ccm_get_index(info, nodename) == CCM_GET_CL(info));	
	
}

static int
ccm_already_joined(ccm_info_t *info)
{
	if (CCM_GET_JOINED_TRANSITION(info)) {
		return TRUE;
	}
	return FALSE;
}

/* 
 * END  OF  FUNCTIONS  that  keep track of stablized membership list 
 */


/* 
 * BEGIN OF FUNCTIONS THAT KEEP TRACK of cluster nodes that have shown 
 * interest in joining the cluster. 
 *
 * NOTE: when a new node wants to join the cluster, it multicasts a  
 * message asking for the necessary information to send out a  join 
 * message. (it needs the current major transistion number, the context 
 * string i.e cookie, the protocol number that everybody is operating 
 * in). 
 * 
 * The functions below track these messages sent out by new potential 
 * members showing interest in acquiring the initial context. 
 */
static void 
ccm_add_new_joiner(ccm_info_t *info, const char *orig)
{
	llm_info_t* llm = &info->llm;
	
	int idx = llm_get_index(&info->llm, orig);
	
	llm->nodes[idx].join_request = TRUE;
	
	return;
}


static gboolean
ccm_get_all_active_join_request(ccm_info_t* info)
{	
	
	llm_info_t* llm = &info->llm;
	size_t i;
	
	for (i = 0 ; i < llm->nodecount; i++){
		if (STRNCMP_CONST(llm->nodes[i].status,"dead") != 0
		    && llm->nodes[i].join_request == FALSE ){
			return FALSE;
		}
	}
	
	return TRUE;
	
}


static void
ccm_reset_all_join_request(ccm_info_t* info)
{
	llm_info_t* llm = &info->llm;
	size_t i;
	
	for (i = 0 ; i < llm->nodecount; i++){
		llm->nodes[i].join_request = FALSE;		
	}	
}


static int
ccm_am_i_highest_joiner(ccm_info_t *info)
{

	llm_info_t*	llm = &info->llm;
	int		total_nodes =llm->nodecount;
	int		my_indx = llm->myindex;
	int		i;

	for (i = my_indx + 1 ; i < total_nodes; i++){
		if (llm->nodes[i].join_request){
			return FALSE;
		}
	}
	
	return TRUE;
}

static void 
ccm_remove_new_joiner(ccm_info_t *info, const char *orig)
{
	llm_info_t* llm = &info->llm;
	int index = llm_get_index(llm, orig);
	
	llm->nodes[index].join_request = FALSE;
	
	return;
}




/* send reply to a join quest and clear the request*/

static void 
ccm_send_join_reply(ll_cluster_t *hb, ccm_info_t *info)
{
	llm_info_t* llm = &info->llm;
	size_t i;
	
	for (i = 0 ; i < llm->nodecount; i++){
		if ( i == (size_t)llm->myindex){
			continue;
		}
		if (llm->nodes[i].join_request){
			ccm_send_one_join_reply(hb,info, llm->nodes[i].nodename);
			llm->nodes[i].join_request = FALSE;
		}
	}
}


/* */
/* END OF FUNCTIONS THAT KEEP TRACK of cluster nodes that have shown */
/* interest in joining the cluster. */
/* */


/*
/////////////////////////////////////////////////////////////////////
//
// BEGIN OF FUNCTIONS THAT SEND OUT messages to nodes of the cluster
//
/////////////////////////////////////////////////////////////////////
*/

/* compute the final membership list from the acquired connectivity */
/* information from other nodes. And send out the consolidated */
/* members of the cluster information to the all the members of  */
/* that have participated in the CCM protocol. */
/* */
/* NOTE: Called by the cluster leader only. */
/* */
static void
ccm_compute_and_send_final_memlist(ll_cluster_t *hb, ccm_info_t *info)
{
	char *bitmap;
	uint maxtrans;
	char string[MAX_MEMLIST_STRING];
	char *cookie = NULL;
	int strsize;
	int repeat;

	/* get the maxmimum membership list */
	maxtrans = ccm_memcomp_get_maxmembership(info, &bitmap);

	
	/* create a string with the membership information */
	strsize  = ccm_bitmap2str(bitmap,  string, MAX_MEMLIST_STRING);
	

	/* check if the membership has changed from that before. If so we
	 * have to generate a new cookie.
	 */
	if(ccm_memlist_changed(info, (char *)bitmap)) {
		cookie = ccm_generate_random_cookie();
	}
	repeat = 0;
	while (ccm_send_final_memlist(hb, info, cookie, string, maxtrans+1) 
					!= HA_OK) {
		if(repeat < REPEAT_TIMES){
			cl_log(LOG_ERR,
				"ccm_compute_and_send_final_memlist: failure "
				"to send finalmemlist");
			cl_shortsleep();
			repeat++;
		}else{
			bitmap_delete(bitmap);
			return;
		}
	}

	/* fill my new memlist and update the new cookie if any */
	ccm_fill_memlist_from_bitmap(info, bitmap);
	bitmap_delete(bitmap);

	/* increment the major transition number and reset the
	 * minor transition number
	 */
	CCM_SET_MAJORTRANS(info, maxtrans+1); 
	CCM_RESET_MINORTRANS(info);

	/* if cookie has changed update it.
	 */
	if (cookie) {
		cl_log(LOG_INFO, "ccm_compute_and_send_final_list: "
				"cookie changed ");
		CCM_SET_COOKIE(info, cookie); 
		ccm_free_random_cookie(cookie);
	}

	/* check if any joiner is waiting for a response from us. 
	 * If so respond and free all the joiners.
	 */
	ccm_send_join_reply(hb, info);

	CCM_SET_CL(info, LLM_GET_MYNODE(CCM_GET_LLM(info)));
	report_mbrs(info);/* call this before update_reset() */
	update_reset(CCM_GET_UPDATETABLE(info));
	ccm_memcomp_reset(info);
	ccm_set_state(info, CCM_STATE_JOINED, NULL);
	if(!ccm_already_joined(info)) {
		CCM_SET_JOINED_TRANSITION(info, CCM_GET_MAJORTRANS(info));
	}

	return;
}







/* */
/* Browse through the list of all the connectivity request messages */
/* from cluster leaders. Send out the connectivity information only */
/* to the node which we believe is the cluster leader. To everybody  */
/* else send out a null message. */
/* */
static int
ccm_send_cl_reply(ll_cluster_t *hb, ccm_info_t *info)
{
	int ret=FALSE, bitmap_strlen;
	char memlist[MAX_MEMLIST_STRING];
	char *cl, *cl_tmp;
	void *cltrack;
	uint  trans;
	int repeat;
	/*
        * Get the name of the cluster leader
	*/
	cl = update_get_cl_name(CCM_GET_UPDATETABLE(info), 
				CCM_GET_LLM(info));

	/* search through the update list and find if any Cluster
	 * leader has sent a memlist request. For each, check if
	 * that node is the one which we believe is the leader.
	 * if it is the leader, send it our membership list.
	 * if not send it an NULL membership reply.
	 */
	cltrack = update_initlink(CCM_GET_UPDATETABLE(info));
	while((cl_tmp = update_next_link(CCM_GET_UPDATETABLE(info), 
				CCM_GET_LLM(info), cltrack, &trans)) != NULL) {
		if(strncmp(cl, cl_tmp, 
			LLM_GET_NODEIDSIZE(CCM_GET_LLM(info))) == 0) {

			if(ccm_already_joined(info) && 
				CCM_GET_MAJORTRANS(info) != trans){
				cl_log(LOG_INFO, "evicted");
				ccm_reset(info);
				return FALSE;
			}
			ret = TRUE;
			bitmap_strlen = update_strcreate(CCM_GET_UPDATETABLE(info), 
							 memlist, CCM_GET_LLM(info));

			/* send Cluster Leader our memlist only if we are 
			 * operating in the same transition as that of 
			 * the leader, provided we have been a cluster member 
			 * in the past 
			 */
			repeat = 0;
			while (ccm_send_memlist_res(hb, info, cl, memlist)
						!=HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_ERR,
						"ccm_state_version_request: "
						"failure to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}
		} else {
			/* I dont trust this Cluster Leader.
			Send NULL memlist message */
			repeat = 0;
			while (ccm_send_memlist_res(hb, info, cl_tmp, NULL)
					!= HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_ERR, 
					"ccm_state_version_request: failure "
						"to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}
		}
	}
	update_freelink(CCM_GET_UPDATETABLE(info), cltrack);
	update_free_memlist_request(CCM_GET_UPDATETABLE(info)); 
	return ret;
}
/*
/////////////////////////////////////////////////////////////////////
//
// END OF FUNCTIONS THAT SEND OUT messages to nodes of the cluster
//
/////////////////////////////////////////////////////////////////////
*/


struct ha_msg * ccm_readmsg(ccm_info_t *info, ll_cluster_t *hb);


struct ha_msg *
ccm_readmsg(ccm_info_t *info, ll_cluster_t *hb)
{
	int 	uuid;

	assert(hb);

	/* check if there are any leave events to be delivered */
	if ((uuid=leave_get_next()) != -1) {
		/* create a leave message and return it */
		return ccm_create_leave_msg(info, uuid);
	}
	
	return hb->llc_ops->readmsg(hb, 0);
}



/* */
/* Move the state of this ccm node, from joining state directly to */
/* the joined state. */
/* */
/* NOTE: this is generally called when a joining nodes determines */
/* that it is the only node in the cluster, and everybody else are */
/* dead. */
/* */
static void
ccm_joining_to_joined(ll_cluster_t *hb, ccm_info_t *info)
{
	char *bitmap;
	char *cookie = NULL;

	/* create a bitmap with the membership information */
	(void) bitmap_create(&bitmap, MAXNODE);
	bitmap_mark(llm_get_myindex(&info->llm), bitmap, MAXNODE);

	/* 
	 * I am the only around! Lets discard any cookie that we
	 * got from others, and create a new cookie.
	 * This bug was noticed: when testing with partitioned
	 * clusters.
	 */
	cookie = ccm_generate_random_cookie();

	/* fill my new memlist and update the new cookie if any */
	ccm_fill_memlist_from_bitmap(info, bitmap);
	bitmap_delete(bitmap);

	/* increment the major transition number and reset the
	 * minor transition number
	 */
	CCM_INCREMENT_MAJORTRANS(info); 
	CCM_RESET_MINORTRANS(info);

	/* if cookie has changed update it.
	 */
	if (cookie) {
		cl_log(LOG_INFO, "ccm_joining_to_joined: "
				"cookie changed ");
		CCM_SET_COOKIE(info, cookie); 
		ccm_free_random_cookie(cookie);
	}

	/* check if any joiner is waiting for a response from us. 
	 * If so respond 
	 */
	ccm_send_join_reply(hb, info);
	
	CCM_SET_CL(info, LLM_GET_MYNODE(CCM_GET_LLM(info)));
	update_reset(CCM_GET_UPDATETABLE(info));
	ccm_set_state(info, CCM_STATE_JOINED, NULL);
	report_mbrs(info);
	if(!ccm_already_joined(info)) {
		CCM_SET_JOINED_TRANSITION(info, 1);
	}
	return;
}

/* */
/* Move the state of this ccm node, from init state directly to */
/* the joined state. */
/* */
/* NOTE: this is generally called when a node when it  determines */
/* that it is all alone in the cluster. */
/* */
static void
ccm_init_to_joined(ccm_info_t *info)
{
	int numBytes;
	char *bitlist;
	char *cookie;

	numBytes = bitmap_create(&bitlist, MAXNODE);
	bitmap_mark(llm_get_myindex(&info->llm), bitlist,MAXNODE);
	ccm_fill_memlist_from_bitmap(info, bitlist);
	bitmap_delete(bitlist);
	CCM_SET_MAJORTRANS(info, 1);
	CCM_SET_MINORTRANS(info, 0);
	cookie = ccm_generate_random_cookie();
	CCM_SET_COOKIE(info, cookie);
	ccm_free_random_cookie(cookie);
	CCM_SET_CL(info, LLM_GET_MYNODE(CCM_GET_LLM(info)));
	ccm_set_state(info, CCM_STATE_JOINED, NULL);
	CCM_SET_JOINED_TRANSITION(info, 1);
	report_mbrs(info);
	return;
}





static void
ccm_all_restart(ll_cluster_t* hb, ccm_info_t* info, struct ha_msg* msg)
{
	const char * orig;
	llm_info_t* llm = & info->llm;

	if ( (orig = ha_msg_value(msg, F_ORIG)) ==NULL){
		cl_log(LOG_ERR, "orig not found in message");
		return ; 
	}
	
	if (strncmp(orig, LLM_GET_MYNODEID(llm), NODEIDSIZE) == 0){
		/*don't react to our own message*/
		return ;
	}
	
	if (info->state != CCM_STATE_VERSION_REQUEST
	    && gl_membership_converged ){
		gl_membership_converged = FALSE;
		ccm_set_state(info, CCM_STATE_NONE, msg);
		CCM_SET_CL(info,-1);
		if (ccm_send_restart_msg(hb, info) != HA_OK){
			cl_log(LOG_ERR, "sending out restart msg failed");
			return;
		}
		
		if (ccm_send_protoversion(hb, info) != HA_OK){
			cl_log(LOG_ERR, "sending protoversion failed");
			return;
		}
		ccm_set_state(info, CCM_STATE_VERSION_REQUEST, NULL);
	}
	
}

static int
ccm_handle_state_info(ll_cluster_t* hb, ccm_info_t* info, struct ha_msg* msg)
{
	const char* other_node_state;
	int state;
	
	if (!part_of_cluster(info->state)){
		return HA_OK;
	}
	
	other_node_state = ha_msg_value(msg, F_STATE);
	state =  string2state(other_node_state);
	
	if (state < 0){
		cl_log(LOG_ERR, "%s: wrong state", __FUNCTION__);
		return HA_FAIL;
	}
	
	if (!part_of_cluster(state)){
		return HA_OK;
	}
	
	/*both machines are already part of a cluster, 
	  i.e. we are merging two partitions
	*/
	ccm_all_restart(hb, info, msg);
	
	return HA_OK;
		
}

/* */
/* The state machine that processes message when it is */
/*	the CCM_STATE_VERSION_REQUEST state */
/* */
static void
ccm_state_version_request(enum ccm_type ccm_msg_type,
			struct ha_msg *reply,
			ll_cluster_t *hb, 
			ccm_info_t *info)
{
	const char *orig, *proto, *cookie, *trans, *clsize;
	uint trans_val;
	int  proto_val;
	uint clsize_val;
	int try;
	int repeat;
	
	/* who sent this message */
	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_version_request: "
			"received message from unknown");
		return;
	}

	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) { 
		cl_log(LOG_WARNING, "ccm_state_version_request: "
			"received message from unknown host %s", orig);
		return;
	}

	switch (ccm_msg_type)  {

	case CCM_TYPE_PROTOVERSION_RESP:

		/* get the protocol version */
		if ((proto = ha_msg_value(reply, CCM_PROTOCOL)) == NULL) {
			cl_log(LOG_WARNING, "ccm_state_version_request: "
					"no protocol information");
			return;
		}

		proto_val = atoi(proto); /*TOBEDONE*/
		if (proto_val >= CCM_VER_LAST) {
			cl_log(LOG_WARNING, "ccm_state_version_request: "
					"unknown protocol value");
			ccm_reset(info);
			return;
		}


		/* if this reply has come from a node which is a member
		 * of a larger cluster, we will try to join that cluster
		 * else we will wait for some time, by dropping this
		 * response.
		 */
		if(resp_can_i_drop()) {
			if ((clsize = ha_msg_value(reply, CCM_CLSIZE)) == NULL){
				cl_log(LOG_WARNING, "ccm_state_version_request: "
						" no cookie information");
				return;
			}
			clsize_val = atoi(clsize);
			if((clsize_val+1) <=  
			   (LLM_GET_NODECOUNT(CCM_GET_LLM(info))+1)/2) {
				/* drop the response. We will wait for 
			  	 * a response from a bigger group 
				 */
				resp_dropped();
				cl_shortsleep(); /* sleep for a while */
				/* send a fresh version request message */
				version_reset(CCM_GET_VERSION(info));
				ccm_set_state(info, CCM_STATE_NONE, reply);
				/* free all the joiners that we accumulated */
				ccm_reset_all_join_request(info);
				break;
			} 
		}
		resp_reset();
	

		/* get the cookie string */
		if ((cookie = ha_msg_value(reply, CCM_COOKIE)) == NULL) {
			cl_log(LOG_WARNING, "ccm_state_version_request: no cookie "
							"information");
			return;
		}

		/* get the major transition version */
		if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) { 
			cl_log(LOG_WARNING, "ccm_state_version_request: "
					"no protocol information");
			return;
		}

		trans_val = atoi(trans);

		/* send the alive message to the cluster
		    The alive msg means: "I want to join this partition!"*/
		CCM_SET_ACTIVEPROTO(info, proto_val);
		CCM_SET_MAJORTRANS(info, trans_val);
		CCM_SET_MINORTRANS(info, 0);
		CCM_SET_COOKIE(info, cookie);
		version_set_nresp(CCM_GET_VERSION(info),0);
		repeat = 0;
		while(ccm_send_alive_msg(hb, info) != HA_OK){
			if(repeat < REPEAT_TIMES){
				cl_log(LOG_WARNING, 
				"ccm_state_version_request: failure to send alive");
				cl_shortsleep();
				repeat++;
			}else{
				break;
			}
		}

		/* initialize the update table  
			and set our state to NEW_NODE_WAIT_FOR_MEM_LIST */
		update_reset(CCM_GET_UPDATETABLE(info));
		new_node_mem_list_time_init();
		ccm_set_state(info, CCM_STATE_NEW_NODE_WAIT_FOR_MEM_LIST, reply);

		/* free all the joiners that we accumulated */
		ccm_reset_all_join_request(info);

		break;

	case CCM_TYPE_TIMEOUT:
		try = version_retry(CCM_GET_VERSION(info), 
					CCM_TMOUT_GET_VRS(info));
		switch (try) {
		case VER_NO_CHANGE: 
			break;
		case VER_TRY_AGAIN:
			ccm_set_state(info, CCM_STATE_NONE, reply);
			break;
		case VER_TRY_END:
			if(ccm_am_i_highest_joiner(info)) {
				ccm_init_to_joined(info);
				ccm_send_join_reply(hb, info);
				
			} else {
				if(global_debug)
					cl_log(LOG_DEBUG,
					       "joined but not really");
				version_reset(CCM_GET_VERSION(info));
				ccm_set_state(info, CCM_STATE_NONE, reply);
				ccm_reset_all_join_request(info);
			}
			break;
		}
		break;
				
	case CCM_TYPE_PROTOVERSION:
		/*
		 * cache this request. If we declare ourselves as
		 * a single member group, and if we find that
		 * somebody else also wanted to join the group.
		 * we will restart the join.
		 */
		ccm_add_new_joiner(info, orig);
		if (ccm_get_all_active_join_request(info)
		    && ccm_am_i_highest_joiner(info)){
			ccm_init_to_joined(info);
			ccm_send_join_reply(hb, info);
		}
		
		break;


	case CCM_TYPE_ABORT:
		/* note down there is some activity going 
		 * on and we are not yet alone in the cluster 
		 */
		version_some_activity(CCM_GET_VERSION(info));
	default:
		/* nothing to do. Just forget the message */
		break;
	}

	return;
}


static void
ccm_state_none(enum ccm_type msgtype,
	       struct ha_msg *msg,
	       ll_cluster_t *hb, 
	       ccm_info_t *info)
{
	
	if (ccm_send_protoversion(hb, info)!= HA_OK){
		cl_log(LOG_ERR, "sending version message failed");
		return;
	}
	
	ccm_set_state(info, CCM_STATE_VERSION_REQUEST, NULL);	
	
	ccm_state_version_request(msgtype, msg, hb, info);
}


/* */
/* The state machine that processes message when it is */
/*	CCM_STATE_JOINED state. */
/* */
static void
ccm_state_joined(enum ccm_type ccm_msg_type, 
			struct ha_msg *reply, 
			ll_cluster_t *hb, 
			ccm_info_t *info)
{
	const char *orig,  *trans, *uptime;
	uint  trans_majorval=0, trans_minorval=0, uptime_val;
	int repeat;
	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_joined: received message "
							"from unknown");
		return;
	}



	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) { 
		cl_log(LOG_WARNING, "ccm_state_joined: received message "
				"from unknown host %s", orig);
		return;
	}
	
	if(ccm_msg_type != CCM_TYPE_PROTOVERSION
	   && ccm_msg_type !=  CCM_TYPE_STATE_INFO
	   && ccm_msg_type != CCM_TYPE_RESTART) {
		const char* tmpcookie =  ha_msg_value(reply, CCM_COOKIE);
		if (tmpcookie == NULL){
			abort();
		}
		if(strncmp(CCM_GET_COOKIE(info), 
			ha_msg_value(reply, CCM_COOKIE), COOKIESIZE) != 0){
			cl_log(LOG_WARNING, "ccm_state_joined: received message "
			       "with unknown cookie, just dropping");
			return;
		}



		/* get the major transition version */
		if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) { 
			cl_log(LOG_WARNING, "ccm_state_joined: no transition major "
				"information");
			return;
		}
		trans_majorval = atoi(trans);

	 	/*drop the message if it has lower major transition number */
		if (CCM_TRANS_EARLIER(trans_majorval,  
					CCM_GET_MAJORTRANS(info))) {
			cl_log(LOG_WARNING, "ccm_state_joined: received "
				"%s message with "
				"a earlier major transition number "
				"recv_trans=%d, mytrans=%d",
				ccm_type2string(ccm_msg_type), trans_majorval, 
				CCM_GET_MAJORTRANS(info));
			return;
		}


		/* get the minor transition version */
		if ((trans = ha_msg_value(reply, CCM_MINORTRANS)) == NULL) { 
			cl_log(LOG_WARNING, "ccm_state_joined: no transition minor "
					"information");
			return;
		}

		trans_minorval = atoi(trans);
	}

	switch (ccm_msg_type)  {

		case CCM_TYPE_PROTOVERSION_RESP:
			cl_log(LOG_WARNING, "ccm_state_joined: dropping message "
				"of type %s.  Is this a Byzantine failure?", 
					ccm_type2string(ccm_msg_type));

			break;

		case CCM_TYPE_PROTOVERSION:
			/* If we were leader in the last successful itteration,
 			 * then we shall respond with the neccessary information
			 */
			if (ccm_am_i_leader(info)){
				repeat = 0;
				while (ccm_send_one_join_reply(hb, info, orig)
						!= HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_joined: "
						"failure to send join reply");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
			}
			break;

		case CCM_TYPE_JOIN:
			/* get the update value */
			if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
				cl_log(LOG_WARNING, "ccm_state_joined: no update "
						"information");
				return;
			}
			uptime_val = atoi(uptime);

			/* update the minor transition number if it is of 
			 * higher value and send a fresh JOIN message 
			 */
			assert (trans_minorval >= CCM_GET_MINORTRANS(info));
			update_reset(CCM_GET_UPDATETABLE(info));
			update_add(CCM_GET_UPDATETABLE(info), CCM_GET_LLM(info),
						orig, uptime_val, TRUE);

			CCM_SET_MINORTRANS(info, trans_minorval);
			repeat = 0;
			while (ccm_send_join(hb, info) != HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_WARNING,
						"ccm_state_joined: failure "
						"to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}

			ccm_set_state(info, CCM_STATE_JOINING, reply);
			break;	

		case CCM_TYPE_LEAVE: 
			
			if (!node_is_member(info, orig)){
				return;
			}
			
			/* If the dead node is the partition leader, go to
			 * JOINING state
			 */
			if (node_is_leader(info, orig)){
				update_reset(CCM_GET_UPDATETABLE(info));
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_joined:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING,reply);
				return;
			}

			/* If I'm the leader, record this "I received the
			 * LEAVE message" and transit to WAIT_FOR_CHANGE
			 */
			if(ccm_am_i_leader(info)){
				reset_change_info(info);
				update_reset(CCM_GET_UPDATETABLE(info));
				add_change_msg(info, orig, CCM_GET_MYNODE_ID(info), 
						NODE_LEAVE);
				update_add(CCM_GET_UPDATETABLE(info), 
						CCM_GET_LLM(info), CCM_GET_MYNODE_ID(info),
						CCM_GET_JOINED_TRANSITION(info), FALSE);
				if(received_all_change_msg(info)){
					char *newcookie = ccm_generate_random_cookie();

					update_membership(info, orig, NODE_LEAVE);          
					send_mem_list_to_all(hb, info, newcookie);
					CCM_SET_MAJORTRANS(info, trans_majorval+1); 
					CCM_RESET_MINORTRANS(info);
					CCM_SET_COOKIE(info, newcookie); 
					ccm_free_random_cookie(newcookie);
					report_mbrs(info);
					return;
				}
				change_time_init();
				ccm_bcast_node_leave_notice(hb,info, orig);
				ccm_set_state(info, CCM_STATE_WAIT_FOR_CHANGE, reply);	
				
				
			}

			break;
			
		case CCM_TYPE_NODE_LEAVE_NOTICE:{
			const char* node;
			const char*	leader = orig;
			
			node = ha_msg_value(reply, F_NODE);
			if(node == NULL){
				cl_log(LOG_ERR, "ccm_state_wait_for_memlist:"
				       "node not found in the message");
				cl_log_message(LOG_INFO, reply);
				return;
			}
			
			if (!node_is_member(info, node)){
				return;
			}			
			
			if( !ccm_am_i_leader(info)){				
				send_node_leave_to_leader(hb, info, leader);
				mem_list_time_init();
				ccm_set_state(info,CCM_STATE_WAIT_FOR_MEM_LIST, reply);
			}
			break;
		}
		case CCM_TYPE_NODE_LEAVE:
			if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
				cl_log(LOG_WARNING, "ccm_state_joined: no update "
						"information");
				return;
			}
			uptime_val = atoi(uptime);

			/* If I'm leader, record received LEAVE message by orig 
			 * and transition to WAIT_FOR_CHANGE state
			 */
			if(ccm_am_i_leader(info)){           
				const char *node = ha_msg_value(reply, F_NODE);

				reset_change_info(info);
				update_reset(CCM_GET_UPDATETABLE(info));
				add_change_msg(info,node,orig,NODE_LEAVE);
				update_add(CCM_GET_UPDATETABLE(info), 
						CCM_GET_LLM(info), orig, uptime_val, FALSE);
				change_time_init();
				ccm_set_state(info, CCM_STATE_WAIT_FOR_CHANGE, reply);
			}
			break;

		case CCM_TYPE_ALIVE:
			/* If I'm leader, record I received the ALIVE message and 
			 * transit to WAIT_FOR_CHANGE
			 */
			if (ccm_am_i_leader(info)){
				reset_change_info(info);
				update_reset(CCM_GET_UPDATETABLE(info));
				add_change_msg(info,orig, CCM_GET_MYNODE_ID(info), 
                            		NEW_NODE);
				update_add(CCM_GET_UPDATETABLE(info),
                            		CCM_GET_LLM(info), CCM_GET_MYNODE_ID(info),
					CCM_GET_JOINED_TRANSITION(info), FALSE);

				if(received_all_change_msg(info)){
					char *newcookie = ccm_generate_random_cookie();

					update_membership(info, orig, NEW_NODE);
					update_add(CCM_GET_UPDATETABLE(info),
						CCM_GET_LLM(info),
						info->change_node_id, trans_majorval+1, FALSE);
					send_mem_list_to_all(hb, info, newcookie);
					CCM_SET_MAJORTRANS(info, trans_majorval+1);
					CCM_RESET_MINORTRANS(info);
					CCM_SET_COOKIE(info, newcookie);
					ccm_free_random_cookie(newcookie);
					report_mbrs(info);
					return;
				}
				change_time_init();
				ccm_set_state(info, CCM_STATE_WAIT_FOR_CHANGE, reply);
			}else{
				/* I'm not leader, send CCM_TYPE_NEW_NODE
				 * to leader and transit to WAIT_FOR_MEM_LIST
				 */
				ccm_send_newnode_to_leader(hb, info, orig);
				mem_list_time_init();
				ccm_set_state(info,CCM_STATE_WAIT_FOR_MEM_LIST, reply);
			}
			break;

		case CCM_TYPE_NEW_NODE:
			if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
				cl_log(LOG_WARNING, "ccm_state_joined: no update "
						"information");
				return;
			}
			uptime_val = atoi(uptime);

			/* If I'm leader, record received ALIVE message by orig 
			 * and transition to WAIT_FOR_CHANGE state
			 */
			if(ccm_am_i_leader(info)){
				const char *node = ha_msg_value(reply, F_NODE);

				reset_change_info(info);	
				update_reset(CCM_GET_UPDATETABLE(info));
				add_change_msg(info,node, orig, NEW_NODE);
				update_add(CCM_GET_UPDATETABLE(info), 
						CCM_GET_LLM(info),
						orig, uptime_val, FALSE);
				change_time_init();
				ccm_set_state(info, CCM_STATE_WAIT_FOR_CHANGE, reply);
			}
			break;

		case CCM_TYPE_STATE_INFO:
			ccm_handle_state_info(hb, info, reply);
			break;			
		case CCM_TYPE_RESTART:
			ccm_all_restart(hb, info, reply);
			break;
		case CCM_TYPE_MEM_LIST:{
			const char* memlist;
			if (strncmp(orig, LLM_GET_MYNODEID((&info->llm) ), NODEIDSIZE) == 0){
				/*this message is from myself, ignore it*/
				break;
			}

			memlist = ha_msg_value(reply, CCM_MEMLIST);
			if (memlist == NULL){
				break;
			}
			
			if (node_is_leader(info, orig)
			    && !ccm_am_i_member(info, memlist)){
				ccm_set_state(info, CCM_STATE_NONE, reply);
				break;
			}

			break;
		}

		case CCM_TYPE_REQ_MEMLIST:
		case CCM_TYPE_RES_MEMLIST:
		case CCM_TYPE_FINAL_MEMLIST:
		case CCM_TYPE_ABORT:
			cl_log(LOG_ERR, "ccm_state_joined: dropping message "
				"of type %s. Is this a Byzantine failure?", 
				ccm_type2string(ccm_msg_type));
			/* nothing to do. Just forget the message */
			break;
		default:
			break;
	}
}

/* */
/* The state machine that processes message when it is in */
/* CCM_STATE_WAIT_FOR_CHANGE state. */
/* */
static void ccm_state_wait_for_change(enum ccm_type ccm_msg_type,
			struct ha_msg *reply,
			ll_cluster_t *hb,
			ccm_info_t *info)
{
	const char *orig, *trans, *uptime, *node;
	uint trans_majorval=0, trans_minorval=0, uptime_val=0;
	gboolean uptime_set = FALSE;
	int repeat;
	
	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_joined: received message "
							"from unknown");
		return;
	}

	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) {
		cl_log(LOG_WARNING, "ccm_state_joined: received message "
				"from unknown host %s", orig);
		return;
	}
	node = ha_msg_value(reply, F_NODE);

	if(ccm_msg_type != CCM_TYPE_PROTOVERSION
	   && ccm_msg_type !=  CCM_TYPE_STATE_INFO
	   && ccm_msg_type != CCM_TYPE_RESTART) {
		
		if(strncmp(CCM_GET_COOKIE(info),
			ha_msg_value(reply, CCM_COOKIE), COOKIESIZE) != 0){
			cl_log(LOG_WARNING, "ccm_state_joined: received message "
					"with unknown cookie, just dropping");
			return;
		}

		/* get the major transition version */
		if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) {
			cl_log(LOG_WARNING, "ccm_state_joined: no transition major "
				"information");
			return;
		}
		trans_majorval = atoi(trans);

	 	/* drop the message if it has lower major transition number */
		if (CCM_TRANS_EARLIER(trans_majorval,
					CCM_GET_MAJORTRANS(info))) {
			cl_log(LOG_WARNING, "ccm_state_joined: received "
				"%s message with "
				"a earlier major transition number "
				"recv_trans=%d, mytrans=%d",
				ccm_type2string(ccm_msg_type), trans_majorval,
				CCM_GET_MAJORTRANS(info));
			return;
		}

		/* get the minor transition version */
		if ((trans = ha_msg_value(reply, CCM_MINORTRANS)) == NULL) { 
			cl_log(LOG_WARNING, "ccm_state_joined: no transition minor "
					"information");
			return;
		}
		trans_minorval = atoi(trans);
	}

	switch (ccm_msg_type) {
		case CCM_TYPE_PROTOVERSION:
			/*
			 * cache this request. We will respond to it,
			 * after transition is complete.
			 */
			ccm_add_new_joiner(info, orig);
			break;
			
		case CCM_TYPE_NODE_LEAVE_NOTICE:
			/* It is my own message, then I can ignore it
			 * or from another lead, i.e. we are in split-brain
			 * and I can do nothing about it
			 */
			break;
		case CCM_TYPE_LEAVE:
			
			if (!node_is_member(info, orig)){
				return;
			}			
			
			if(strcmp(info->change_node_id, orig) == 0
			   && info->change_type == NODE_LEAVE){
				/*It is the same node leaving*/
				return;
			}
			
			node = orig;
			orig = CCM_GET_MYNODE_ID(info);
			uptime_val = CCM_GET_JOINED_TRANSITION(info);
			uptime_set = TRUE;
			/*fall through*/
		case CCM_TYPE_NODE_LEAVE:               
			/* only leader can stay in this state */
			if(!ccm_am_i_leader(info))
				break;

			if (!uptime_set){
				if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
					cl_log(LOG_WARNING, "ccm_state_wait_for_change: no update "
						"information");
					return;
				}
				uptime_val = atoi(uptime);
				uptime_set = TRUE;
			}

			/* Record received LEAVE message by orig.
			 * If received all change msg, send mem_list to members.
			 */
			if(is_expected_change_msg(info,node,NODE_LEAVE)){
				append_change_msg(info,orig);
				update_add(CCM_GET_UPDATETABLE(info),
					CCM_GET_LLM(info), orig, uptime_val, FALSE);

				if(received_all_change_msg(info)){ 
					char *newcookie = ccm_generate_random_cookie();

					update_membership(info, node, NODE_LEAVE);        
     					send_mem_list_to_all(hb, info, newcookie);
					CCM_SET_MAJORTRANS(info, trans_majorval+1); 
					CCM_RESET_MINORTRANS(info);
					CCM_SET_COOKIE(info, newcookie); 
					report_mbrs(info);
					reset_change_info(info); 
					update_reset(CCM_GET_UPDATETABLE(info));
					ccm_free_random_cookie(newcookie);
					ccm_send_join_reply(hb, info);
					CCM_SET_CL(info, LLM_GET_MYNODE(CCM_GET_LLM(info)));
					ccm_set_state(info, CCM_STATE_JOINED,reply);
					return;
				}
			}else{
				reset_change_info(info);
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_joined:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
				return;
			}                  
			break;
		
		case CCM_TYPE_ALIVE:
			node = orig;	
			orig = CCM_GET_MYNODE_ID(info);
			uptime_val = CCM_GET_JOINED_TRANSITION(info);
			uptime_set = TRUE;
			/*fall through*/	    	
		case CCM_TYPE_NEW_NODE:
			/* only leader can stay in this state */
			if(!ccm_am_i_leader(info)){
				assert(0);
			}

			if (!uptime_set){
				if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
					cl_log(LOG_WARNING, "ccm_state_wait_for_change: no update "
						"information");
					return;
				}
				uptime_val = atoi(uptime);
				uptime_set = TRUE;
			}

			if(is_expected_change_msg(info,node, NEW_NODE)){
				append_change_msg(info,orig);
				update_add(CCM_GET_UPDATETABLE(info), CCM_GET_LLM(info),
					orig, uptime_val, FALSE);

				if(received_all_change_msg(info)){
					char *newcookie = ccm_generate_random_cookie();

					update_membership(info, node, NEW_NODE); 
					update_add(CCM_GET_UPDATETABLE(info), 
						CCM_GET_LLM(info),
						info->change_node_id, trans_majorval+1, FALSE);                                
					send_mem_list_to_all(hb, info, newcookie);
					CCM_SET_MAJORTRANS(info, trans_majorval+1); 
					CCM_RESET_MINORTRANS(info);
					CCM_SET_COOKIE(info, newcookie); 
					report_mbrs(info);
					reset_change_info(info); 
					update_reset(CCM_GET_UPDATETABLE(info));
					ccm_free_random_cookie(newcookie);
					ccm_send_join_reply(hb, info);
					ccm_set_state(info, CCM_STATE_JOINED, reply);
					return;
				}                       
			}else{
				reset_change_info(info);
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				while (ccm_send_join(hb, info) != HA_OK) {
					cl_log(LOG_WARNING, "ccm_state_wait_for_change:"
						" failure to send join");
					cl_shortsleep();
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
				return;
			}
			break;
	    	
		case CCM_TYPE_TIMEOUT:
			if(change_timeout(CCM_TMOUT_GET_U(info))){
				reset_change_info(info);
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_wait_for_change:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
			}
			break;

		case CCM_TYPE_JOIN:
			/* get the update value */
			if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
				cl_log(LOG_WARNING, "ccm_state_joined: no update "
						"information");
				return;
			}
			uptime_val = atoi(uptime);
			uptime_set = TRUE;

			/* update the minor transition number if it is of 
			 * higher value and send a fresh JOIN message 
			 */
			assert (trans_minorval >= CCM_GET_MINORTRANS(info));
			update_reset(CCM_GET_UPDATETABLE(info));
			update_add(CCM_GET_UPDATETABLE(info), CCM_GET_LLM(info),
						orig, uptime_val, TRUE);

			CCM_SET_MINORTRANS(info, trans_minorval);
			repeat = 0;
			while (ccm_send_join(hb, info) != HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_WARNING,
						"ccm_state_joined: failure "
						"to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}

			ccm_set_state(info, CCM_STATE_JOINING, reply);
			break;		

		case CCM_TYPE_STATE_INFO:
			ccm_handle_state_info(hb, info, reply);
			break;			
		case CCM_TYPE_RESTART:
			ccm_all_restart(hb, info, reply);
			break;

		default:  
			cl_log(LOG_ERR, "ccm_state_joined: dropping message "
				"of type %s. Is this a Byzantine failure?", 
				ccm_type2string(ccm_msg_type));
			/* nothing to do. Just forget the message */
			break;	
	}
}


/* */
/* The state machine that processes message when it is */
/*	in the CCM_STATE_SENT_MEMLISTREQ state */
/* */
static void
ccm_state_sent_memlistreq(enum ccm_type ccm_msg_type, 
			struct ha_msg *reply, 
			ll_cluster_t *hb, 
			ccm_info_t *info)
{
	const char *orig,  *trans, *memlist, *uptime;
	uint   trans_minorval=0, trans_majorval=0, trans_maxval=0;
        uint    uptime_val;
	int repeat;

	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_sent_memlistreq: received message "
						"from unknown");
		return;
	}

	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) { 
		cl_log(LOG_WARNING, "ccm_state_sent_memlistreq: received message "
				"from unknown host %s", orig);
		return;
	}
	
	if(ccm_msg_type == CCM_TYPE_PROTOVERSION
	   || ccm_msg_type ==  CCM_TYPE_STATE_INFO
	   || ccm_msg_type == CCM_TYPE_RESTART) {		
		goto switchstatement;
	}

	if(strncmp(CCM_GET_COOKIE(info), ha_msg_value(reply, CCM_COOKIE), 
				COOKIESIZE) != 0){
		cl_log(LOG_WARNING, "ccm_state_memlist_res: received message "
				"with unknown cookie, just dropping");
		return;
	}

	/* get the major transition version */
	if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) { 
		cl_log(LOG_WARNING, "ccm_state_sent_memlistreq:no transition major "
				"information");
		return;
	}

	trans_majorval = atoi(trans);
	 /*	drop the message if it has lower major transition number */
	if (CCM_TRANS_EARLIER(trans_majorval,  CCM_GET_MAJORTRANS(info))) {
		cl_log(LOG_WARNING, "ccm_state_sent_memlistreq: received "
					"CCM_TYPE_JOIN message with"
					"a earlier major transition number");
		return;
	}


	/* get the minor transition version */
	if ((trans = ha_msg_value(reply, CCM_MINORTRANS)) == NULL) { 
		cl_log(LOG_WARNING, "ccm_state_sent_memlistreq:no transition minor "
				"information");
		return;
	}

	trans_minorval = atoi(trans);

switchstatement:
	switch (ccm_msg_type)  {
		case CCM_TYPE_PROTOVERSION_RESP:

			cl_log(LOG_WARNING, "ccm_state_sent_memlistreq: "
				"dropping message of type %s. "
				" Is this a Byzantine failure?",
				ccm_type2string(ccm_msg_type));

			break;


		case CCM_TYPE_PROTOVERSION:
			/*
			 * cache this request. We will respond to it, 
			 * if we become the leader.
			 */
			ccm_add_new_joiner(info, orig);
			
			break;

		case CCM_TYPE_JOIN:

			/* The join request has come too late.
			 * I am already the leader, and my
			 * leadership cannot be relinquished
			 * because that can confuse everybody.
			 * This join request shall be considered.
			 * But leadership shall not be relinquished.
			 */
			assert(trans_majorval == CCM_GET_MAJORTRANS(info));
			assert(trans_minorval == CCM_GET_MINORTRANS(info));
			cl_log(LOG_INFO, "considering a late join message "
					  "from orig=%s", orig);
			/* get the update value */
			if ((uptime = ha_msg_value(reply, CCM_UPTIME)) 
						== NULL){
				cl_log(LOG_WARNING, 
					"ccm_state_sent_memlistreq: no "
					"update information");
				return;
			}
			uptime_val = atoi(uptime);
			update_add(CCM_GET_UPDATETABLE(info), 
				CCM_GET_LLM(info), orig, uptime_val, FALSE);
			ccm_memcomp_add(info, orig);
			break;

		case CCM_TYPE_TIMEOUT:
			if (ccm_memcomp_timeout(info,
				CCM_TMOUT_GET_IFF(info))) {
				/* we waited long for membership response 
				 * from all nodes, stop waiting and send
				 * final membership list
				 */				
				ccm_compute_and_send_final_memlist(hb, info);
			}
			break;

		case CCM_TYPE_REQ_MEMLIST:

			/* if this is my own message just forget it */
			if(strncmp(orig, ccm_get_my_hostname(info),
				LLM_GET_NODEIDSIZE(CCM_GET_LLM(info))) == 0) 
				break;


			/* whoever is requesting memlist from me thinks it is 
			 * the leader. Hmm....., we will send it a NULL memlist.
			 * In partitioned network case both of us can be 
			 * leaders. Right?
			 */

			repeat = 0;
			while (ccm_send_memlist_res(hb, info, orig, NULL) != 
						HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_ERR,
						"ccm_state_sent_memlistreq: "
						"failure to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}
			break;

		case CCM_TYPE_RES_MEMLIST:
			/* mark that this node has sent us a memlist reply.
			 * Calculate the membership list with this new message 
			 */
			if(trans_minorval != CCM_GET_MINORTRANS(info)){
				break;
			}

			if(trans_majorval != CCM_GET_MAJORTRANS(info)) {
				cl_log(LOG_INFO, 
				   "dropping CCM_TYPE_RES_MEMLIST "
				   "from orig=%s mymajor=%d msg_major=%d", 
				   orig, trans_majorval, 
					CCM_GET_MAJORTRANS(info));
				assert(0);
				break;
			}
			if ((memlist = ha_msg_value(reply, CCM_MEMLIST)) 
						== NULL) { 
				cl_log(LOG_WARNING, "ccm_state_sent_memlistreq: "
						"no memlist ");
				break;
			}
			/* get the max transition version */
			if (!(trans = ha_msg_value(reply, CCM_MAXTRANS))) { 
				cl_log(LOG_WARNING, 
					"ccm_state_sent_memlistreq: "
					"no max transition "
					"information %s, type=%d", 
					orig, ccm_msg_type);
				return;
			}

			trans_maxval = atoi(trans);

			ccm_memcomp_note(info, orig, trans_maxval, memlist);

			if (ccm_memcomp_rcvd_all(info)) {				
				ccm_compute_and_send_final_memlist(hb,info);
			}
			break;

		case CCM_TYPE_LEAVE: 
			/* since we are waiting for a memlist from all the 
			 * members who have sent me a join message, we 
			 * should be waiting for their message or their 
			 * leave message atleast.
			 */

			/* if this node had not participated in the update 
			 * exchange than just neglect it 
			 */
			if(!update_is_node_updated(CCM_GET_UPDATETABLE(info), 
						   CCM_GET_LLM(info), orig)) {
				break;
			}
			
			/* if this node had sent a memlist before dying,
			 * reset its memlist information */
			ccm_memcomp_note(info, orig, 0, "");

			if (ccm_memcomp_rcvd_all(info)) {
				ccm_compute_and_send_final_memlist(hb, info);
			}
			break;
			
		case CCM_TYPE_STATE_INFO:
			ccm_handle_state_info(hb, info, reply);
			break;
		case CCM_TYPE_RESTART:
			ccm_all_restart(hb, info, reply);
			break;

		case CCM_TYPE_FINAL_MEMLIST:
		case CCM_TYPE_ABORT:
		default:
			cl_log(LOG_ERR, "ccm_state_sent_memlistreq: "
					"dropping message of type %s. Is this "
					"a Byzantine failure?", 
					ccm_type2string(ccm_msg_type));
			/* nothing to do. Just forget the message */
			break;
	}
}

/* */
/* the state machine that processes messages when it is in the */
/* CCM_STATE_MEMLIST_RES state. */
/* */
static void
ccm_state_memlist_res(enum ccm_type ccm_msg_type, 
		struct ha_msg *reply, 
		ll_cluster_t *hb, 
		ccm_info_t *info)
{
	const char *orig,  *trans, *uptime, *memlist, *cookie, *cl;
	uint   trans_majorval=0, trans_minorval=0, trans_maxval=0;
	uint    uptime_val;
	uint  curr_major, curr_minor;
	int   indx;
	int repeat;


	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_memlist_res: received message "
		       "from unknown");
		return;
	}

	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) { 
		cl_log(LOG_WARNING, "ccm_state_memlist_res: received message "
				"from unknown host %s", orig);
		return;
	}
	if(ccm_msg_type == CCM_TYPE_PROTOVERSION
	   || ccm_msg_type ==  CCM_TYPE_STATE_INFO
	   || ccm_msg_type == CCM_TYPE_RESTART) {
		goto switchstatement;
	}

	if(strncmp(CCM_GET_COOKIE(info), ha_msg_value(reply, CCM_COOKIE), 
				COOKIESIZE) != 0){
		cl_log(LOG_WARNING, "ccm_state_memlist_res: received message "
				"with unknown cookie, just dropping");
		return;
	}

	/* get the major transition version */
	if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) { 
		cl_log(LOG_WARNING, "ccm_state_memlist_res: no transition major "
				"information");
		return;
	}

	trans_majorval = atoi(trans);
	 /*	drop the message if it has lower major transition number */
	if (CCM_TRANS_EARLIER(trans_majorval,  CCM_GET_MAJORTRANS(info))) {
		cl_log(LOG_WARNING, "ccm_state_memlist_res: received "
					"CCM_TYPE_JOIN message with"
					"a earlier major transition number");
		return;
	}


	/* get the minor transition version */
	if ((trans = ha_msg_value(reply, CCM_MINORTRANS)) == NULL) { 
		cl_log(LOG_WARNING, "ccm_state_memlist_res: no transition minor "
				"information");
		return;
	}

	trans_minorval = atoi(trans);


switchstatement:

	switch (ccm_msg_type)  {
		case CCM_TYPE_PROTOVERSION_RESP:
			cl_log(LOG_WARNING, "ccm_state_memlist_res:dropping message"
					" of type %s. Is this a Byzantine "
					" failure?", 
					ccm_type2string(ccm_msg_type));
			break;

		case CCM_TYPE_PROTOVERSION:
			/*
			 * cache this request. We will respond to it, if we 
			 * become the leader.
			 */
			ccm_add_new_joiner(info, orig);
			
			break;

		case CCM_TYPE_JOIN:

			/*
			 * This could have happened because the leader died 
			 * and somebody noticed this and sent us this request. 
			 * In such a case the minor transition number should 
			 * have incremented. Or
			 * This could have happened because the leader's 
			 * FINAL_MEMLIST	
			 * has not reach us, whereas it has reached somebody 
			 * else, and since that somebody saw a change in 
			 * membership, initiated another join protocol. 
			 * In such a case the major transition
			 * number should have incremented.
			 */
			/* 
			 * if major number is incremented, send an abort message
			 * to the sender. The sender must resend the message.
			 */
			if (trans_majorval > CCM_GET_MAJORTRANS(info)) {
				repeat = 0;
				while (ccm_send_abort(hb, info, orig, 
					trans_majorval, trans_minorval) 
							!= HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_ERR,
						"ccm_state_memlist_res:"
						" failure to send abort");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				break;
			}

			/* if minor transition number is incremented, 
			 * reset uptable table and start a join protocol
			 */
			if (trans_minorval > CCM_GET_MINORTRANS(info)) {
				/* get the update value */
				if ((uptime = ha_msg_value(reply, CCM_UPTIME)) 
							== NULL){
					cl_log(LOG_WARNING, 
						"ccm_state_memlist_res: no "
						"update information");
					return;
				}
				uptime_val = atoi(uptime);

				update_reset(CCM_GET_UPDATETABLE(info));
				update_add(CCM_GET_UPDATETABLE(info), 
					CCM_GET_LLM(info), orig, uptime_val, TRUE);

				CCM_SET_MINORTRANS(info, trans_minorval);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_ERR,
						"ccm_state_memlist_res:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
			}

			break;
			

		case CCM_TYPE_REQ_MEMLIST:
			/* there are two reasons that can bring us here 
			 * 1. Because some other node still thinks he is 
			 * the master,(though we dont think so). Send 
			 * a NULL membership list to him immidiately.
			 * 2. Because of byzantine failures, though we have 
			 * not recieved the the membership list in the last 
			 * round. We have waited to such an exent that some 
			 * node already thinks he is the master of the
			 * the new group transition. Well, there is something 
			 * seriously wrong with us. We will send a leave 
			 * message to everybody and say good bye. And we 
			 * will start all fresh!
			 */
			if (trans_minorval == CCM_GET_MINORTRANS(info)) {
				repeat = 0;
				while (ccm_send_memlist_res(hb, info, orig, 
							NULL) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_ERR,
						"ccm_state_memlist_res:"
						 " failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				break;
			}

			break;

        	case CCM_TYPE_TIMEOUT:
			/* If we have waited too long for the leader to respond
			 * just assume that the leader is dead and start over
			 * a new round of the protocol
			 */
			if(!finallist_timeout(CCM_TMOUT_GET_FL(info))) {
				break;
			}
			update_reset(CCM_GET_UPDATETABLE(info));
			CCM_INCREMENT_MINORTRANS(info);
			repeat = 0;
			while (ccm_send_join(hb, info) != HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_ERR,
						"ccm_state_memlist_res:"
						" failure to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}
			finallist_reset();
			ccm_set_state(info, CCM_STATE_JOINING, reply);
			break;

		case CCM_TYPE_LEAVE: 
			/* 
			 * If this message is because of loss of connectivity 
			 * with the node which we think is the master, then 
			 * restart the join. Loss of anyother node should be 
			 * confirmed by the finalmemlist of the master.
		 	 */
			cl = update_get_cl_name(CCM_GET_UPDATETABLE(info), 
					CCM_GET_LLM(info));
			if(strncmp(cl, orig, 
				LLM_GET_NODEIDSIZE(CCM_GET_LLM(info))) == 0) {
				/* increment the current minor transition value 
				 * and resend the join message 
				 */
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_ERR,
						"ccm_state_memlist_res:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				finallist_reset();
				ccm_set_state(info, CCM_STATE_JOINING, reply);
			}

			break;
		
		case CCM_TYPE_FINAL_MEMLIST:
			/* WOW we received the membership list from the master.
			 * Check if I am part of the membership list. If not, 
			 * voluntarily leave the cluster and start all over 
			 * again 
			 */
			cl = update_get_cl_name(CCM_GET_UPDATETABLE(info), 
						CCM_GET_LLM(info));

			if(strncmp(cl, orig, 
				LLM_GET_NODEIDSIZE(CCM_GET_LLM(info))) != 0) {
				/* received memlist from a node we do not 
				 * think is the leader. We just reject the 
				 * message and wait for a message from the 
				 * our percieved master
				 */
				cl_log(LOG_WARNING, "ccm_state_memlist_res: "
					"received final memlist from "
					"non-master,neglecting");
									
				break;
			}
	
			/* 
			 * confirm that the major transition and minor 
			 * transition version match
			 */
			curr_major = CCM_GET_MAJORTRANS(info);
			curr_minor = CCM_GET_MINORTRANS(info);

			if(curr_major != trans_majorval || 
				curr_minor !=  trans_minorval){
				cl_log(LOG_WARNING, "ccm_state_memlist_res: "
					"received final memlist from master, "
					"but transition versions do not match: "
					"rejecting the message");
				break;
			}
			
			if ((memlist = ha_msg_value(reply, CCM_MEMLIST)) 
						== NULL) { 
				cl_log(LOG_WARNING, "ccm_state_sent_memlistreq: "
						"no membership list ");
				return;
			}
			if ((trans = ha_msg_value(reply, CCM_MAXTRANS)) 
						== NULL) { 
				cl_log(LOG_WARNING, "ccm_state_sent_memlistreq: "
						"no membership list ");
				return;
			}
			trans_maxval = atoi(trans);

			if (!ccm_am_i_member(info, memlist)) {
				ccm_reset(info); 
				break;
			}

			ccm_fill_memlist_from_str(info, (const char *)memlist);
			/* increment the major transition number and reset the
			 * minor transition number
			 */
			CCM_SET_MAJORTRANS(info, trans_maxval); 
			CCM_RESET_MINORTRANS(info);

			/* check if leader has changed the COOKIE, this can
			 * happen if the leader sees a partitioned group
			 */
			if ((cookie = ha_msg_value(reply, CCM_NEWCOOKIE)) 
						!= NULL) { 
				cl_log(LOG_INFO, "ccm_state_sent_memlistreq: "
					"leader  changed  cookie ");
				CCM_SET_COOKIE(info, cookie); 
			}

			indx = ccm_get_index(info, cl); 
			assert(indx != -1);
			CCM_SET_CL(info, indx); 
			report_mbrs(info); /* call before update_reset */
			update_reset(CCM_GET_UPDATETABLE(info));
			finallist_reset();
			ccm_set_state(info, CCM_STATE_JOINED, reply);
			ccm_reset_all_join_request(info);
			if(!ccm_already_joined(info)) 
				CCM_SET_JOINED_TRANSITION(info, 
					CCM_GET_MAJORTRANS(info));
			break;

		case CCM_TYPE_STATE_INFO:
			ccm_handle_state_info(hb, info, reply);
			break;		

		case CCM_TYPE_RESTART:
			ccm_all_restart(hb, info, reply);
			break;

		case CCM_TYPE_ABORT:
		case CCM_TYPE_RES_MEMLIST:
		default:
			cl_log(LOG_ERR, "ccm_state_sendmemlistreq: "
					"dropping message of type %s. "
					"Is this a Byzantine failure?", 
					ccm_type2string(ccm_msg_type));
			/* nothing to do. Just forget the message */
			break;
	}
}



/* */
/* the state machine that processes messages when it is in the */
/* CCM_STATE_JOINING state. */
/* */
static void
ccm_state_joining(enum ccm_type ccm_msg_type, 
		struct ha_msg *reply, 
		ll_cluster_t *hb, 
		ccm_info_t *info)
{
	const char *orig,  *trans, *uptime;
	uint   trans_majorval=0, trans_minorval=0;
        uint	uptime_val;
	int repeat;
	
	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_joining: received message "
							"from unknown");
		return;
	}

	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) { 
		cl_log(LOG_WARNING, "ccm_state_joining: received message "
				"from unknown host %s", orig);
		return;
	}

	if(ccm_msg_type == CCM_TYPE_PROTOVERSION
	   || ccm_msg_type ==  CCM_TYPE_STATE_INFO
	   || ccm_msg_type == CCM_TYPE_RESTART) {
		goto switchstatement;
	}

	if(strncmp(CCM_GET_COOKIE(info), ha_msg_value(reply, CCM_COOKIE), 
			COOKIESIZE) != 0){

		if(ccm_msg_type ==  CCM_TYPE_PROTOVERSION_RESP) {
			version_inc_nresp(CCM_GET_VERSION(info));
			cl_log(LOG_WARNING, "ccm_state_joining: received message "
			"incrementing versionresp counter %d", 
				version_get_nresp(CCM_GET_VERSION(info)));
		}

		cl_log(LOG_WARNING, "ccm_state_joining: received message "
			"with unknown cookie, just dropping");
		return;
	}

	


	/* get the major transition version */
	if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) { 
		cl_log(LOG_WARNING, "ccm_state_joining: no transition major "
				"information");
		return;
	}

	trans_majorval = atoi(trans);
	 /*	drop the message if it has lower major transition number */
	if (CCM_TRANS_EARLIER(trans_majorval,  CCM_GET_MAJORTRANS(info))) {
		cl_log(LOG_WARNING, "ccm_state_joining: received "
				"CCM_TYPE_JOIN message with"
				"a earlier major transition number");
		return;
	}


	/* get the minor transition version */
	if ((trans = ha_msg_value(reply, CCM_MINORTRANS)) == NULL) { 
		cl_log(LOG_WARNING, "ccm_state_joining: no transition minor "
				"information");
		return;
	}

	trans_minorval = atoi(trans);
	if (trans_minorval < CCM_GET_MINORTRANS(info)) {
		return;
	}


switchstatement:
	switch (ccm_msg_type)  {

		case CCM_TYPE_PROTOVERSION_RESP:

			/* If we were joined in an earlier iteration, then this
			 * message should not have arrived. A bug in the logic!
			 */
			if(ccm_already_joined(info)) {
				cl_log(LOG_WARNING, "ccm_state_joining: BUG:"
					" received CCM_TYPE_PROTOVERSION_RESP "
					"message when we have not asked for "
					"it ");
				break;
			}

			cl_log(LOG_WARNING, "ccm_state_joining: dropping message "
					" of type %s. Is this a Byzantine "
					"failure?", 
					ccm_type2string(ccm_msg_type));
			break;
				

		case CCM_TYPE_PROTOVERSION:
			/*
			 * cache this request. We will respond to it, 
			 * if we become the leader.
			 */
			ccm_add_new_joiner(info, orig);
			
			break;

        	case CCM_TYPE_JOIN:
			/* get the update value */
			if((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){ 
				cl_log(LOG_WARNING, "ccm_state_joining: no update "
						"information");
				return;
			}
			uptime_val = atoi(uptime);

			/* 
			 * note down all the information contained in the 
			 * message There is a possibility that I am the leader,
			 * if all the nodes died, and I am the only surviving 
			 * node! If this message has originated from me, 
			 * note down the current time. This information is 
			 * needed, to later recognize that I am the only 
			 * surviving node.
			 */
			/* update the minor transition number if it is of 
			 * higher value 
			 * and send a fresh JOIN message 
			 */
			if (trans_minorval > CCM_GET_MINORTRANS(info)) {
				update_reset(CCM_GET_UPDATETABLE(info));
				update_add( CCM_GET_UPDATETABLE(info),
					CCM_GET_LLM(info), orig, uptime_val, TRUE);

				CCM_SET_MINORTRANS(info, trans_minorval);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_ERR, 
						"ccm_state_joining: failure "
						"to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
			} else {
				/* update the update table  */
				update_add( CCM_GET_UPDATETABLE(info),
					CCM_GET_LLM(info), orig, uptime_val, 
					TRUE);

				/* if all nodes have responded, its time 
				 * to elect the leader 
				 */
				if (UPDATE_GET_NODECOUNT(
					CCM_GET_UPDATETABLE(info)) ==
					CCM_GET_LLM_NODECOUNT(info)) {

					/* check if I am the leader */
					if (update_am_i_leader(
						CCM_GET_UPDATETABLE(info),
						CCM_GET_LLM(info))) {
						/* send out the 
						 * membershiplist request */
						repeat = 0;
						while(ccm_send_memlist_request(
							hb, info)!=HA_OK) {
							if(repeat < REPEAT_TIMES){
							cl_log(LOG_ERR, 
							"ccm_state_joining: "
							"failure to send "
							"memlist request");
							cl_shortsleep();
							repeat++;
							}else{
								break;
							}
						}
						ccm_memcomp_init(info);
						ccm_memcomp_note_my_membership(
								info);
						ccm_set_state(info, 
							      CCM_STATE_SENT_MEMLISTREQ, reply);
					} else {
						/* check if we have already 
						 * received memlist request
						 * from any node(which 
						 * believes itself to be the 
						 * leader)
						 * If so,we have to reply to 
						 * them with our membership
						 * list. But there is a catch. 
						 * If we do not think the
						 * requestor to be the leader, 
						 * then we send it an null
						 * membership message!
						 */
						if (ccm_send_cl_reply(hb,info) 
								== TRUE) {
							finallist_init();
							ccm_set_state(info, 
								      CCM_STATE_MEMLIST_RES, reply);
						}
					}
					break; /* done all processing */
				} 
			}
				   
			break;	

		case CCM_TYPE_REQ_MEMLIST:

			/* well we have not yet timedout! And a memlist
			 * request has arrived from the cluster leader.  Hmm...
			 * We should wait till timeout, to respond.
			 *
			 * NOTE:  there is a chance
			 * that more than one cluster leader might request
			 * the membership list. Due to cluster partitioning :( )
			 */
			update_add_memlist_request(CCM_GET_UPDATETABLE(info),
					CCM_GET_LLM(info), orig, trans_majorval);
			/*
			 * FALL THROUGH
			 */
		case CCM_TYPE_TIMEOUT:
			/*
			 * If timeout expired, elect the leader.
			 * If I am the leader, send out the membershiplist request
			 */
			if (!update_timeout_expired(CCM_GET_UPDATETABLE(info), 
					CCM_TMOUT_GET_U(info))) {
						break;
			}

			if (update_am_i_leader(CCM_GET_UPDATETABLE(info),
						CCM_GET_LLM(info))) {

				/* if I am the only one around go directly
				 * to joined state.
				 */
				if (UPDATE_GET_NODECOUNT(
					CCM_GET_UPDATETABLE(info)) == 1) {

					if(ccm_already_joined(info) || 
						!version_get_nresp(
						  CCM_GET_VERSION(info))){
						ccm_joining_to_joined(hb,
							       	info);
					} else {
						ccm_reset(info);
					}
					break;
				}

				/* send out the membershiplist request */
				repeat = 0;
				while (ccm_send_memlist_request(hb, info) 
							!= HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_ERR,
						"ccm_state_joining: "
						"failure to send memlist "
						"request");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_memcomp_init(info);
				ccm_memcomp_note_my_membership(info);
				ccm_set_state(info, CCM_STATE_SENT_MEMLISTREQ, reply);
			} else {
				/* check if we have already received memlist 
				 * request from any node(which believes itself 
				 * to be the leader)
				 * If so,we have to reply to them with our 
				 * membership list. But there is a catch. 
				 * If we do not think the
				 * requestor to be the leader, then we send 
				 * it an abort message!
				 */
				if (ccm_send_cl_reply(hb, info) == TRUE) {
					/* free the update data*/
					finallist_init();
					ccm_set_state(info, 
						      CCM_STATE_MEMLIST_RES, reply);
				}
			}
			break;


		case CCM_TYPE_ABORT:

			/*
			 * This is a case where my JOIN request is not honoured
			 * by the recieving host(probably because it is waiting
			 * on some message, before which it cannot initiate 
			 * the join).
			 * We will resend the join message, incrementing the
			 * minor version number, provided this abort is 
			 * requested
			 * for this minor version.
			 */
			if(trans_majorval != CCM_GET_MAJORTRANS(info) ||
				trans_minorval != CCM_GET_MINORTRANS(info)) {
				/* nothing to worry  just forget this message */
					break;
			}
					
			/* increment the current minor transition value 
			 * and resend the
				join message */
			CCM_INCREMENT_MINORTRANS(info);
			update_reset(CCM_GET_UPDATETABLE(info));
			repeat = 0;
			while (ccm_send_join(hb, info) != HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_ERR,
						"ccm_state_joining: failure "
						"to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}

			break;

		case CCM_TYPE_LEAVE: 

			/* 
			 * Has that node already sent a valid update message 
			 * before death. If so, remove him from the update 
			 * table.
			 */
			update_remove(CCM_GET_UPDATETABLE(info),
					CCM_GET_LLM(info), 
					orig);
			/* if we have any cached version-request from this node 
			 * we will get rid of that too
			 */
			ccm_remove_new_joiner(info, orig);
			break;
			
		case CCM_TYPE_STATE_INFO:
			ccm_handle_state_info(hb, info, reply);
			break;
		case CCM_TYPE_RESTART:
			ccm_all_restart(hb, info, reply);
			break;

		case CCM_TYPE_RES_MEMLIST:
		case CCM_TYPE_FINAL_MEMLIST:
			/* this message is from other partitions*/
			cl_log(LOG_WARNING, "ccm_state_joinging: received a %s message", 
			       ccm_type2string(ccm_msg_type));
			cl_log(LOG_WARNING, "We probably have different partitions");
			break;
			
			
		default:
			cl_log(LOG_ERR, "ccm_state_joining: dropping message "
			       "of type %s. Is this a Byzantine failure?", 
			       ccm_type2string(ccm_msg_type));
			/* nothing to do. Just forget the message */
			break;
	}
	return;
}


/*  */
/* The most important function which tracks the state machine. */
/*  */
static void
ccm_control_init(ccm_info_t *info)
{
	ccm_init(info);

	/* if this is the only active node in the cluster, go to the 
			JOINED state */
	if (llm_get_live_nodecount(CCM_GET_LLM(info)) == 1) {
		ccm_init_to_joined(info);
	} else {
		ccm_set_state(info, CCM_STATE_NONE, NULL);
	}

	return;
}



/* */
/* The callback function which is called when the status of a link */
/* changes. */
/* */
static void
LinkStatus(const char * node, const char * lnk, const char * status ,
		void * private)
{
	if(global_debug) {
		cl_log(LOG_DEBUG, "Link Status update: Link %s/%s "
				"now has status %s", node, lnk, status);
	}
}





/*  */
/* The most important function which tracks the state machine. */
/*  */




/*  look at the current state machine and decide if  */
/*  the state machine needs immidiate control for further */
/*  state machine processing. Called by the check function */
/*  of heartbeat-source of the main event loop. */
int
ccm_need_control(void *data)
{
	ccm_info_t *info =  (ccm_info_t *)((ccm_t *)data)->info;

	if(leave_any() || 
	   CCM_GET_STATE(info) != CCM_STATE_JOINED){
			return TRUE;
	}
	return FALSE;
}

/*  look at the current state machine and decide if  */
/*  the state machine needs immidiate control for further */
/*  state machine processing. Called by the check function */
/*  of heartbeat-source of the main event loop. */
int
ccm_take_control(void *data)
{
	ccm_info_t *info =  (ccm_info_t *)((ccm_t *)data)->info;
	ll_cluster_t *hbfd = (ll_cluster_t *)((ccm_t *)data)->hbfd;
	static gboolean client_flag=FALSE;

	if(!client_flag) {
		client_llm_init(CCM_GET_LLM(info));
		client_flag=TRUE;
	}

	return  ccm_control_process(info, hbfd);
}

IPC_Channel *
ccm_get_ipcchan(void *data)
{
	ll_cluster_t *hbfd = (ll_cluster_t *)((ccm_t *)data)->hbfd;

	return hbfd->llc_ops->ipcchan(hbfd);
}


#define	PINGNODE        "ping"
static int
set_llm_from_heartbeat(ll_cluster_t* llc, ccm_info_t* info){
	llm_info_t*	llm = &info->llm;
	struct llc_ops* ops = llc->llc_ops;
	const char*	status;
	const char*	node;
	const char*	mynode = ops->get_mynodeid(llc);

	
	if (mynode == NULL){
		cl_log(LOG_ERR, "mynode is NULL");
		return HA_FAIL;
	}

	if(global_debug) {
		cl_log(LOG_DEBUG, "==== Starting  Node Walk =========");
	}
	if (ops->init_nodewalk(llc) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk");
		cl_log(LOG_ERR, "REASON: %s", ops->errmsg(llc));
		return HA_FAIL;
	}
	
	llm = CCM_GET_LLM(info);
	llm_init(llm);
	while((node = ops->nextnode(llc)) != NULL) {		
		if (strcmp(ops->node_type(llc, node), PINGNODE)==0){
			continue;
		}
		
		status = ops->node_status(llc, node);
		if(global_debug) {
			cl_log(LOG_DEBUG, "Cluster node: %s: status: %s", node,
			       status);
		}
		
		llm_add(llm, node, status, mynode);
		
	}
	llm_end(llm);
	
	display_llm(llm);
	
	if (ops->end_nodewalk(llc) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk");
		cl_log(LOG_ERR, "REASON: %s", ops->errmsg(llc));
		return HA_FAIL;
	}
	
	if(global_debug) {
		cl_log(LOG_DEBUG, "======= Ending  Node Walk ==========");
		cl_log(LOG_DEBUG, "Total # of Nodes in the Cluster: %d", 
		       LLM_GET_NODECOUNT(llm));
	}		
	
	return HA_OK;
}


void *
ccm_initialize()
{
	unsigned	fmask;
	const char *	hname;
	ccm_info_t 	*global_info = NULL;
	ll_cluster_t*	hb_fd;
	ccm_t		*ccmret = NULL;
	int		facility;
	const char *	parameter;

	if(global_debug) {
		cl_log(LOG_DEBUG, "========================== Starting CCM ===="
			"======================");
	}

	CL_SIGINTERRUPT(SIGTERM, 1);

	hb_fd = ll_cluster_new("heartbeat");

	cl_log(LOG_INFO, "PID=%ld", (long)getpid());

	cl_log(LOG_INFO, "Signing in with Heartbeat");
	if (hb_fd->llc_ops->signon(hb_fd, "ccm")!= HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		goto errout;
	}

	/* See if we should drop cores somewhere odd... */
	parameter = hb_fd->llc_ops->get_parameter(hb_fd, KEY_COREROOTDIR);
	if (parameter) {
		cl_set_corerootdir(parameter);
	}
	cl_cdtocoredir();

	/* change the logging facility to the one used by heartbeat daemon
	 * the signon MUST BE FIRST! */
	if ((facility = hb_fd->llc_ops->get_logfacility(hb_fd))>0) {
		/* If someone cares, map it to its name ... */
		cl_log(LOG_INFO, "Switched to heartbeat syslog facility: %d", facility);
		cl_log_set_facility(facility);
	}
	
	if((global_info = (ccm_info_t *)g_malloc(sizeof(ccm_info_t))) == NULL){
		cl_log(LOG_ERR, "Cannot allocate memory ");
		goto errout;
	}

	if((ccmret = (ccm_t *)g_malloc(sizeof(ccm_t))) == NULL){
		cl_log(LOG_ERR, "Cannot allocate memory");
		goto errout;
	}

	if((hname = hb_fd->llc_ops->get_mynodeid(hb_fd)) == NULL) {
		cl_log(LOG_ERR, "get_mynodeid() failed");
		goto errout;
	}
	cl_log(LOG_INFO, "Hostname: %s", hname);


	if (hb_fd->llc_ops->set_ifstatus_callback(hb_fd, LinkStatus, NULL)
					!=HA_OK){
		cl_log(LOG_ERR, "Cannot set if status callback");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		goto errout;
	}
	
	fmask = LLC_FILTER_DEFAULT;
	if (hb_fd->llc_ops->setfmode(hb_fd, fmask) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set filter mode");
		cl_log(LOG_ERR, "REASON: %s", hb_fd->llc_ops->errmsg(hb_fd));
		goto errout;
	}


	if (set_llm_from_heartbeat(hb_fd, global_info) != HA_OK){
		goto errout;
	}

	ccm_control_init(global_info);
	ccm_configure_timeout(hb_fd, global_info);

	ccmret->info = global_info;
	ccmret->hbfd = hb_fd;
	return  (void*)ccmret;

 errout:
	if (ccmret){
		g_free(ccmret);
		ccmret = NULL;
	}
	if (global_info){
		g_free(global_info);
		global_info = NULL;
	}	
	return NULL;


}

static void add_change_msg(ccm_info_t *info, const char *node, const char *orig, enum change_event_type type)
{
	strcpy(info->change_node_id, node);
	info->change_type = type;
	if(type == NODE_LEAVE){
		info->change_event_remaining_count = CCM_GET_MEMCOUNT(info)-1;
	}else{
		info->change_event_remaining_count = CCM_GET_MEMCOUNT(info);
	}
	append_change_msg(info, orig);
	return;
}

static void append_change_msg(ccm_info_t *info, const char *node)
{
	if (CCM_GET_RECEIVED_CHANGE_MSG(info, node) == 0){
		CCM_SET_RECEIVED_CHANGE_MSG(info, node, 1);
		info->change_event_remaining_count--;
	}
	return;
}

static int received_all_change_msg(ccm_info_t *info)
{
	if(info->change_event_remaining_count == 0){
		return 1;
	}else{
		return 0;
	}
}

static int is_expected_change_msg(ccm_info_t *info, const char *node,enum change_event_type type)
{
	if(strcmp(info->change_node_id, node) == 0){
		if(info->change_type == type){
			return 1;
		}
	}
	return 0;
}


static void ccm_state_wait_for_mem_list(enum ccm_type ccm_msg_type, 
			struct ha_msg *reply, 
			ll_cluster_t *hb, 
			ccm_info_t *info)
{
	const char *orig, *trans, *uptime, *cookie, *memlist;
	int uptime_list[MAXNODE];
	size_t uptime_size = MAXNODE;
	uint trans_majorval=0,trans_minorval=0, uptime_val;
	uint curr_major, curr_minor;
	int repeat;
	
	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list: received message "
							"from unknown");
		return;
	}

	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) {
		cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list: received message "
				"from unknown host %s", orig);
		return;
	}

	
	if(ccm_msg_type != CCM_TYPE_PROTOVERSION
	   && ccm_msg_type !=  CCM_TYPE_STATE_INFO
	   && ccm_msg_type != CCM_TYPE_RESTART) {
		if(strncmp(CCM_GET_COOKIE(info),
			ha_msg_value(reply, CCM_COOKIE), COOKIESIZE) != 0){
			cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list: received message"
					" with unknown cookie, just dropping");
			return;
		}

		/* get the major transition version */
		if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) {
			cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list:"
					"no transition major information");
			return;
		}
		trans_majorval = atoi(trans);

		/* drop the message if it has lower major transition number */
		if (CCM_TRANS_EARLIER(trans_majorval,  
					CCM_GET_MAJORTRANS(info))) {
			cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list: received "
				"%s message with "
				"a earlier major transition number "
				"recv_trans=%d, mytrans=%d",
				ccm_type2string(ccm_msg_type), trans_majorval, 
				CCM_GET_MAJORTRANS(info));
			return;
		}

		/* get the minor transition version */
		if ((trans = ha_msg_value(reply, CCM_MINORTRANS)) == NULL) { 
			cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list: "
					"no transition minor information");
			return;
		}

		trans_minorval = atoi(trans);
	}


	switch(ccm_msg_type){
		
		case CCM_TYPE_MEM_LIST:
        	
			curr_major = CCM_GET_MAJORTRANS(info);
			curr_minor = CCM_GET_MINORTRANS(info);

			if(curr_major != trans_majorval || 
				curr_minor !=  trans_minorval){
				cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list: "
					"received final memlist from master, "
					"but transition versions do not match: "
					"rejecting the message");
				break;
			}
			if ((memlist = ha_msg_value(reply, CCM_MEMLIST)) 
							== NULL) {
				cl_log(LOG_WARNING, "ccm_state_wait_for_mem_list: "
						"no membership list ");
				return;
			}

			
			if (cl_msg_get_list_int(reply,CCM_UPTIMELIST, 
						uptime_list, &uptime_size) != HA_OK){
				cl_log(LOG_ERR," ccm_state_new_node_wait_for_mem_list:"
				       "geting uptie_list failed");
				return;
			}

			ccm_fill_memlist_from_str(info, (const char *)memlist);
			CCM_SET_MAJORTRANS(info, curr_major+1);
			CCM_RESET_MINORTRANS(info);
			if ((cookie = ha_msg_value(reply, CCM_NEWCOOKIE))
						!= NULL) { 
				cl_log(LOG_INFO, "ccm_state_sent_memlistreq: "
					"leader  changed  cookie ");
				CCM_SET_COOKIE(info, cookie);
			}
			CCM_SET_CL(info, ccm_get_index(info,orig));
			ccm_fill_update_table(info, CCM_GET_UPDATETABLE(info),
						uptime_list);
			report_mbrs(info);
			ccm_set_state(info, CCM_STATE_JOINED, reply);
			break;
        	
		case CCM_TYPE_TIMEOUT:
        		if (mem_list_timeout(CCM_TMOUT_GET_U(info))){
				reset_change_info(info);
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_joined:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
			}
			break;

		case CCM_TYPE_JOIN:
        		/* get the update value */
			if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
				cl_log(LOG_WARNING, "ccm_state_joined: no update "
						"information");
				return;
			}
			uptime_val = atoi(uptime);

			/* update the minor transition number if it is of
			 * higher value and send a fresh JOIN message
			 */
			assert (trans_minorval >= CCM_GET_MINORTRANS(info));
			update_reset(CCM_GET_UPDATETABLE(info));
			update_add(CCM_GET_UPDATETABLE(info), CCM_GET_LLM(info),
						orig, uptime_val, TRUE);

			CCM_SET_MINORTRANS(info, trans_minorval);
			repeat = 0;
			while (ccm_send_join(hb, info) != HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_WARNING,
						"ccm_state_joined: failure "
						"to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}

			ccm_set_state(info, CCM_STATE_JOINING, reply);
			break;

		case CCM_TYPE_LEAVE:
			
			/* if the dead node is leader, jump to CCM state machine */
			if(node_is_leader(info, orig)){
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_joined:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
				return;
			}

		case CCM_TYPE_ALIVE:
			/* We do nothing here because we believe leader
			 * will deal with this LEAVE message. SPOF?
			 */
			break;

		case CCM_TYPE_PROTOVERSION:
			/* leader will handle this message
			 * we can safely ignore it
			 */
			break;


		case CCM_TYPE_STATE_INFO:
			ccm_handle_state_info(hb, info, reply);
			break;		
		case CCM_TYPE_RESTART:
			ccm_all_restart(hb, info, reply);
			break;


	
		default:
			cl_log(LOG_ERR, "ccm_state_wait_for_mem_list: dropping message "
				"of type %s. Is this a Byzantine failure?",
				ccm_type2string(ccm_msg_type));
			/* nothing to do. Just forget the message */
			break;
	}
}

static void
update_membership(ccm_info_t *info, const char *node, 
		enum change_event_type change_type)
{
	unsigned	i;
	int		index;
	llm_info_t *llm = CCM_GET_LLM(info);
    
	if (change_type == NODE_LEAVE){
		index = ccm_get_membership_index(info, node);
		info->ccm_member[index] = info->ccm_member[info->ccm_nodeCount-1];
		info->ccm_nodeCount--;
	}else{
		for ( i = 0 ; i < LLM_GET_NODECOUNT(llm); i++ ) {
			if(strcmp(node, llm->nodes[i].nodename) == 0){
				/* update the membership list with this member */
				CCM_ADD_MEMBERSHIP(info, i);
				break;
			}
		}
	}
	return;
}

static void
reset_change_info(ccm_info_t *info)
{
	llm_info_t *llm = CCM_GET_LLM(info);
	unsigned i;

	for(i=0; i<LLM_GET_NODECOUNT(llm); i++) {
		llm->nodes[i].received_change_msg = 0;
	}
	return;
}


/* */
/*  Construct and send mem_list, uptime_list to all members in the partition */
/* */
static void send_mem_list_to_all(ll_cluster_t *hb, 
		ccm_info_t *info, char *cookie)
{
	int numBytes, i, size, strsize,  j, tmp, tmp_mem[100];
	char *bitmap;
	char memlist[MAX_MEMLIST_STRING];
	int *uptime;
    
	numBytes = bitmap_create(&bitmap, MAXNODE);
	size = info->ccm_nodeCount;
	uptime = g_malloc(sizeof(int)*size);
	
	for (i=0; i<size; i++){
		tmp_mem[i] = info->ccm_member[i]; 
	}	
        for (i=0; i<size; i++){
                for(j=0; j<(size-1-i); j++){
                        if(tmp_mem[j] > tmp_mem[j+1]){
                                tmp = tmp_mem[j];
                                tmp_mem[j] = tmp_mem[j+1];
                                tmp_mem[j+1] = tmp;
                        }
                }
        }

	for ( i = 0 ; i < size ; i++ ) {
		bitmap_mark(info->ccm_member[i], 
			    bitmap, MAXNODE);
		uptime[i] = htonl(update_get_uptime(CCM_GET_UPDATETABLE(info), 
						    CCM_GET_LLM(info),
						    tmp_mem[i]));
	}    
	strsize  = ccm_bitmap2str(bitmap, memlist, MAX_MEMLIST_STRING);
	bitmap_delete(bitmap);
	ccm_send_to_all(hb, info, memlist, cookie, uptime, sizeof(int)*size);
	g_free(uptime);
	return;
}

static void ccm_state_new_node_wait_for_mem_list(enum ccm_type ccm_msg_type, 
	              struct ha_msg *reply, 
	              ll_cluster_t *hb, 
			ccm_info_t *info)
{
    	const char *orig,  *trans, *uptime, *memlist, *cookie;
	int uptime_list[MAXNODE];
	size_t uptime_size = MAXNODE;
	uint  trans_majorval=0,trans_minorval=0, uptime_val;
	uint  curr_major, curr_minor;
	int repeat;

	if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
		cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list: " 
					"received message from unknown");
		return;
	}

	if(!llm_is_valid_node(CCM_GET_LLM(info), orig)) { 
		cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list: " 
					"received message from unknown host %s", orig);
		return;
	}

	if(ccm_msg_type != CCM_TYPE_PROTOVERSION
	   && ccm_msg_type !=  CCM_TYPE_STATE_INFO
	   && ccm_msg_type != CCM_TYPE_RESTART) {
		
		if(strncmp(CCM_GET_COOKIE(info), 
			ha_msg_value(reply, CCM_COOKIE), COOKIESIZE) != 0){
			cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list: "
					"received message with unknown cookie, just dropping");
			return;
		}

		/* get the major transition version */
		if ((trans = ha_msg_value(reply, CCM_MAJORTRANS)) == NULL) { 
			cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list:"
					" no transition major information");
			return;
		}
		trans_majorval = atoi(trans);

	 	/*drop the message if it has lower major transition number */
		if (CCM_TRANS_EARLIER(trans_majorval,  
					CCM_GET_MAJORTRANS(info))) {
			cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list:received"
				" %s message with "
				"a earlier major transition number "
				"recv_trans=%d, mytrans=%d",
				ccm_type2string(ccm_msg_type), trans_majorval, 
				CCM_GET_MAJORTRANS(info));
			return;
		}

		/* get the minor transition version */
		if ((trans = ha_msg_value(reply, CCM_MINORTRANS)) == NULL) { 
			cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list: "
					"no transition minor information");
			return;
		}

		trans_minorval = atoi(trans);
	}
    	
	switch(ccm_msg_type){
		
		case CCM_TYPE_MEM_LIST:
			curr_major = CCM_GET_MAJORTRANS(info);
			curr_minor = CCM_GET_MINORTRANS(info);

			if(curr_major != trans_majorval || 
				curr_minor !=  trans_minorval){
				cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list: "
					"received final memlist from master, "
					"but transition versions do not match: "
					"rejecting the message");
				break;
			}
			if ((memlist = ha_msg_value(reply, CCM_MEMLIST)) 
						== NULL) { 
				cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list: "
						"no membership list ");
				return;
			}
			
			if (cl_msg_get_list_int(reply,CCM_UPTIMELIST, 
						uptime_list, &uptime_size) != HA_OK){
				cl_log(LOG_ERR," ccm_state_new_node_wait_for_mem_list:"
				       "geting uptie_list failed");
				return;
			}
			
			ccm_fill_memlist_from_str(info, (const char *)memlist);
			if(ccm_get_membership_index(info, 
					CCM_GET_MYNODE_ID(info)) == -1){
				version_reset(CCM_GET_VERSION(info));
				ccm_set_state(info, CCM_STATE_NONE, reply);
				ccm_reset_all_join_request(info);
				break;
			}
			CCM_SET_MAJORTRANS(info, curr_major+1); 
			CCM_RESET_MINORTRANS(info);
			if ((cookie = ha_msg_value(reply, CCM_NEWCOOKIE)) 
						!= NULL) { 
				cl_log(LOG_INFO, "ccm_state_new_node_wait_for_mem_list: "
					"leader  changed  cookie ");
				CCM_SET_COOKIE(info, cookie); 
			}
			CCM_SET_CL(info,ccm_get_index(info, orig));
			CCM_SET_JOINED_TRANSITION(info, CCM_GET_MAJORTRANS(info));
			ccm_fill_update_table(info, 
				CCM_GET_UPDATETABLE(info), uptime_list);
			ccm_set_state(info, CCM_STATE_JOINED, reply);	        
			report_mbrs(info);
			break;

		case CCM_TYPE_TIMEOUT:
			if (new_node_mem_list_timeout(CCM_TMOUT_GET_U(info))){
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_new_node_wait_for_mem_list:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
			}	
			break;

		case CCM_TYPE_JOIN:
			/* get the update value */
			if ((uptime = ha_msg_value(reply, CCM_UPTIME)) == NULL){
				cl_log(LOG_WARNING, "ccm_state_new_node_wait_for_mem_list: "
						"no update information");
				return;
			}
			uptime_val = atoi(uptime);

			/* update the minor transition number if it is of 
			 * higher value and send a fresh JOIN message 
			 */
			assert (trans_minorval >= CCM_GET_MINORTRANS(info));
			update_reset(CCM_GET_UPDATETABLE(info));
			update_add(CCM_GET_UPDATETABLE(info), CCM_GET_LLM(info),
						orig, uptime_val, TRUE);

			CCM_SET_MINORTRANS(info, trans_minorval);
			repeat = 0;
			while (ccm_send_join(hb, info) != HA_OK) {
				if(repeat < REPEAT_TIMES){
					cl_log(LOG_WARNING,
					"ccm_state_new_node_wait_for_mem_list: "
					"failure to send join");
					cl_shortsleep();
					repeat++;
				}else{
					break;
				}
			}
			ccm_set_state(info, CCM_STATE_JOINING, reply);
			break;		

		case CCM_TYPE_LEAVE:
			
			/* if the dead node is leader, jump to CCM state machine */
			if(node_is_leader(info, orig)){
				update_reset(CCM_GET_UPDATETABLE(info));
				CCM_INCREMENT_MINORTRANS(info);
				repeat = 0;
				while (ccm_send_join(hb, info) != HA_OK) {
					if(repeat < REPEAT_TIMES){
						cl_log(LOG_WARNING,
						"ccm_state_new_node_wait_for_mem_list:"
						" failure to send join");
						cl_shortsleep();
						repeat++;
					}else{
						break;
					}
				}
				ccm_set_state(info, CCM_STATE_JOINING, reply);
			}

		case CCM_TYPE_ALIVE:
			/* We do nothing here because we believe leader
			 * will deal with this LEAVE message. SPOF?
			 */
			break;		

		case CCM_TYPE_PROTOVERSION:
			/* we are waiting for the leader for membership list
			 * it's ok if someone want to join -- just ignore
			 * the message and let the leader handl it
			 */
			
			break;

		case CCM_TYPE_STATE_INFO:
			ccm_handle_state_info(hb, info, reply);
			break;
		case CCM_TYPE_RESTART:
			ccm_all_restart(hb, info, reply);
			break;
		default:
			cl_log(LOG_ERR,"ccm_state_new_node_waitfor_memlst:dropping message"
				" of type %s. Is this a Byzantine failure?", 
				ccm_type2string(ccm_msg_type));
			/* nothing to do. Just forget the message */
			break;	
	}
}


static void ccm_fill_update_table(ccm_info_t *info,
		ccm_update_t *update_table, const void *uptime_list)
{
	const int *uptime;
	int i;

	uptime = (const int *)uptime_list;

	UPDATE_SET_NODECOUNT(update_table, info->ccm_nodeCount);
	for (i = 0; i< info->ccm_nodeCount; i++){
		update_table->update[i].index = info->ccm_member[i];
		update_table->update[i].uptime = ntohl(uptime[i]);
	}
	return;
} 

int
jump_to_joining_state(ll_cluster_t *hb, 
		      ccm_info_t *info,
		      struct ha_msg* msg){
	
	reset_change_info(info);
	update_reset(CCM_GET_UPDATETABLE(info));
	CCM_INCREMENT_MINORTRANS(info);
	if (ccm_send_join(hb, info) != HA_OK){
		cl_log(LOG_ERR, "sending joining message failed");
		return HA_FAIL;
		
	}
	ccm_set_state(info, CCM_STATE_JOINING, msg);
	return HA_OK;
}

state_msg_handler_t	state_msg_handler[]={
	ccm_state_none,
	ccm_state_version_request,
	ccm_state_joining,  
	ccm_state_sent_memlistreq,
	ccm_state_memlist_res,
	ccm_state_joined, 
	ccm_state_wait_for_mem_list,
	ccm_state_wait_for_change,
	ccm_state_new_node_wait_for_mem_list,
};
	

