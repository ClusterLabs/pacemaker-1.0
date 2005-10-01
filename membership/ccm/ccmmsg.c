/*
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
 */

#include "ccm.h"
#include "ccmmsg.h"
#include "ccmmisc.h"
#include <config.h>
#include <ha_config.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>
int ccm_send_cluster_msg(ll_cluster_t* hb, struct ha_msg* msg);
int ccm_send_node_msg(ll_cluster_t* hb, struct ha_msg* msg, const char* node);
int
ccm_send_cluster_msg(ll_cluster_t* hb, struct ha_msg* msg)
{
	int rc;
	
	rc = hb->llc_ops->sendclustermsg(hb, msg);
	if (rc != HA_OK){
		cl_log(LOG_INFO, "sending out message failed");
		cl_log_message(LOG_INFO, msg);
		return rc;
	}
	
	return HA_OK;
}


int
ccm_send_node_msg(ll_cluster_t* hb, 
		  struct ha_msg* msg, 
		  const char* node)
{
	int rc;
	
	rc = hb->llc_ops->sendclustermsg(hb, msg);
	if (rc != HA_OK){
		cl_log(LOG_INFO, "sending out message failed");
		cl_log_message(LOG_INFO, msg);
		return rc;
	}
	
	return HA_OK;
	
	
}


static struct ha_msg* 
ccm_create_minimum_msg(ccm_info_t * info, int type)
{	
	struct ha_msg *m;

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "%s: creating a new message failed",
		       __FUNCTION__);
		return NULL;
	}

	if( ha_msg_add(m, F_TYPE, ccm_type2string(type)) == HA_FAIL
	    ||ha_msg_add_int(m, F_NUMNODES, info->llm.nodecount) == HA_FAIL){
		cl_log(LOG_ERR, "%s: adding fields to an message failed",
		       __FUNCTION__);
		ha_msg_del(m);
		return NULL;
	}
	
	return m;	
}


struct ha_msg* 
ccm_create_msg(ccm_info_t * info, int type)
{	
	struct ha_msg *m = ccm_create_minimum_msg(info, type);;
	char majortrans[15];
	char minortrans[15];
	char joinedtrans[15];
	char *cookie;
	
	if (m == NULL) {
		cl_log(LOG_ERR, "%s: creating a new message failed",
		       __FUNCTION__);
		return NULL;
	}
	
	snprintf(majortrans, sizeof(majortrans), "%d", 
		 info->ccm_transition_major);
	snprintf(minortrans, sizeof(minortrans), "%d", 
		 info->ccm_transition_minor);
	snprintf(joinedtrans, sizeof(joinedtrans), "%d", 
		 info->ccm_joined_transition);
	cookie = info->ccm_cookie;
	assert(cookie && *cookie);
	
	if((ha_msg_add(m, CCM_COOKIE, cookie) == HA_FAIL) 
	   ||(ha_msg_add(m, CCM_MAJORTRANS, majortrans) == HA_FAIL)
	   ||(ha_msg_add(m, CCM_UPTIME, joinedtrans) == HA_FAIL)
	   ||(ha_msg_add(m, CCM_MINORTRANS, minortrans) == HA_FAIL)){
		cl_log(LOG_ERR, "%s: adding fields to an message failed",
		       __FUNCTION__);
		ha_msg_del(m);
		return NULL;
	}
	
	return m;	
}


static int
ccm_mod_msg(struct ha_msg* msg, ccm_info_t * info, int type)
{	
	struct ha_msg *m;
	char majortrans[15]; /* 10 is the maximum number of digits in 
				UINT_MAX , adding a buffer of 5 */
	char minortrans[15]; /*		ditto 	*/
	char joinedtrans[15]; /*	ditto 	*/
	char *cookie;
	
	if (msg == NULL){
		cl_log(LOG_ERR, "NULL message");
		return HA_FAIL;
	}

	m = msg;
	
	snprintf(majortrans, sizeof(majortrans), "%d", 
		 info->ccm_transition_major);
	snprintf(minortrans, sizeof(minortrans), "%d", 
		 info->ccm_transition_minor);
	snprintf(joinedtrans, sizeof(joinedtrans), "%d", 
		 info->ccm_joined_transition);
	cookie = info->ccm_cookie;
	
	if (cookie == NULL){
		abort();
	}
	
	if((ha_msg_mod(m, F_TYPE, ccm_type2string(type)) == HA_FAIL)
	   ||(ha_msg_mod(m, CCM_COOKIE, cookie) == HA_FAIL) 
	   ||(ha_msg_mod(m, CCM_MAJORTRANS, majortrans) == HA_FAIL)
	   ||(ha_msg_mod(m, CCM_UPTIME, joinedtrans) == HA_FAIL)
	   ||(ha_msg_mod(m, CCM_MINORTRANS, minortrans) == HA_FAIL)){
		cl_log(LOG_ERR, "%s: moding fields to an message failed",
		       __FUNCTION__);
		return HA_FAIL;
	}
	
	return HA_OK;
}




int
ccm_send_standard_clustermsg(ll_cluster_t* hb, ccm_info_t* info, int type)
{
	struct ha_msg *m = ccm_create_msg(info, type);
	int  rc;
	if (m == NULL){
		cl_log(LOG_ERR, "creating message failed");
		return HA_FAIL;
	}
	rc = hb->llc_ops->sendclustermsg(hb, m);
	ha_msg_del(m);
	return(rc);
}

static int
ccm_send_minimum_clustermsg(ll_cluster_t* hb, ccm_info_t* info, int type)
{
	struct ha_msg *m = ccm_create_minimum_msg(info, type);
	int  rc;	
	
	if (m == NULL) {
		cl_log(LOG_ERR, "creating a new message failed");
		return(HA_FAIL);
	}
	
	rc = hb->llc_ops->sendclustermsg(hb, m);
	ha_msg_del(m);
	return(rc);	
	
	
}

static int
ccm_send_extra_clustermsg(ll_cluster_t* hb, ccm_info_t* info, int type,
			  const char* fieldname, const char* fieldvalue )
{
	struct ha_msg *m = ccm_create_msg(info, type);
	int rc;

	if (fieldname == NULL || fieldvalue == NULL){
		cl_log(LOG_ERR, "NULL argument");
		return HA_FAIL;
	}
	
	if (m == NULL){
		cl_log(LOG_ERR, "message creating failed");
		return HA_FAIL;
	}
	
	if ( ha_msg_add(m, fieldname, fieldvalue) == HA_FAIL){
		cl_log(LOG_ERR, "Adding a field failed");
		ha_msg_del(m);
		return HA_FAIL;
	} 
	
	rc = hb->llc_ops->sendclustermsg(hb, m);		
	ha_msg_del(m);
	return(rc);
	
}

static int
ccm_send_extra_nodemsg(ll_cluster_t* hb, ccm_info_t* info, int type,
		       const char* fieldname, const char* fieldvalue, 
		       const char* nodename )
{
	
	struct ha_msg *m = ccm_create_msg(info, type);
	int rc;

	if (fieldname == NULL || fieldvalue == NULL){
		cl_log(LOG_ERR, "NULL argument");
		return HA_FAIL;
	}
	
	if (m == NULL){
		cl_log(LOG_ERR, "message creating failed");
		return HA_FAIL;
	}
	
	if ( ha_msg_add(m, fieldname, fieldvalue) == HA_FAIL){
		cl_log(LOG_ERR, "Adding a field failed");
		ha_msg_del(m);
		return HA_FAIL;
	} 
	
	rc = hb->llc_ops->sendnodemsg(hb, m, nodename);		
	ha_msg_del(m);
	return(rc);

}

int
ccm_send_protoversion(ll_cluster_t *hb, ccm_info_t *info)
{
	return ccm_send_minimum_clustermsg(hb, info, CCM_TYPE_PROTOVERSION);
}


int
ccm_send_join(ll_cluster_t *hb, ccm_info_t *info)
{
	return ccm_send_standard_clustermsg(hb, info, CCM_TYPE_JOIN);
}

int
ccm_send_memlist_request(ll_cluster_t *hb, ccm_info_t *info)
{
	return ccm_send_standard_clustermsg(hb, info, CCM_TYPE_REQ_MEMLIST);
}

int
ccm_send_memlist_res(ll_cluster_t *hb, 
		     ccm_info_t *info,
		     const char *nodename, 
		     const char *memlist)
{
	struct ha_msg *m = ccm_create_msg(info, CCM_TYPE_RES_MEMLIST);
	char maxtrans[15]; 
	int  rc;
	snprintf(maxtrans, sizeof(maxtrans), "%d", 
		 info->ccm_max_transition);
	if (!memlist) {
		memlist= "";
	} 
	
	if ( (ha_msg_add(m, CCM_MAXTRANS, maxtrans) == HA_FAIL)
	     || (ha_msg_add(m, CCM_MEMLIST, memlist) == HA_FAIL)) {
		cl_log(LOG_ERR, "ccm_send_memlist_res: Cannot create "
		       "RES_MEMLIST message");
		rc = HA_FAIL;
		ha_msg_del(m);
		return HA_FAIL;
	} 
	
	rc = hb->llc_ops->sendnodemsg(hb, m, nodename);
	ha_msg_del(m);
	return(rc);
}
int
ccm_send_final_memlist(ll_cluster_t *hb, 
			ccm_info_t *info, 
			char *newcookie, 
			char *finallist,
			uint32_t max_tran)
{  
	struct ha_msg *m = ccm_create_msg(info, CCM_TYPE_FINAL_MEMLIST);
	char activeproto[3];
	char maxtrans[15]; 
	int rc;


	if (m == NULL){
		cl_log(LOG_ERR, "msg creation failure");
		return HA_FAIL;
	}

	snprintf(activeproto, sizeof(activeproto), "%d", 
		 info->ccm_active_proto);
	snprintf(maxtrans, sizeof(maxtrans), "%d", max_tran);
	assert(finallist);

	if (ha_msg_add(m, CCM_MAXTRANS, maxtrans) == HA_FAIL
	    || ha_msg_add(m, CCM_MEMLIST, finallist) == HA_FAIL
	    ||(!newcookie? FALSE: (ha_msg_add(m, CCM_NEWCOOKIE, newcookie)
				   ==HA_FAIL))) {
		cl_log(LOG_ERR, "ccm_send_final_memlist: Cannot create "
		       "FINAL_MEMLIST message");
		rc = HA_FAIL;
	} else {
		rc = hb->llc_ops->sendclustermsg(hb, m);
	}
	ha_msg_del(m);
	return(rc);
}


int 
ccm_send_one_join_reply(ll_cluster_t *hb, ccm_info_t *info, const char *joiner)
{
	struct ha_msg *m;
	char activeproto[3];
	char clsize[5];
	int rc;


	/*send the membership information to all the nodes of the cluster*/
	m=ccm_create_msg(info, CCM_TYPE_PROTOVERSION_RESP);
	if (m == NULL){
		cl_log(LOG_ERR, "%s: creating a message failed",
		       __FUNCTION__);
		return(HA_FAIL);
	}
	
	snprintf(activeproto, sizeof(activeproto), "%d", 
		 info->ccm_active_proto);
	snprintf(clsize, sizeof(clsize), "%d", 
		 info->ccm_nodeCount);
	if ( ha_msg_add(m, CCM_PROTOCOL, activeproto) == HA_FAIL 
	     || ha_msg_add(m, CCM_CLSIZE, clsize) == HA_FAIL){
		cl_log(LOG_ERR, "ccm_send_one_join_reply: Cannot create JOIN "
		       "reply message");
		rc = HA_FAIL;
	} else {
		rc = hb->llc_ops->sendnodemsg(hb, m, joiner);
	}
	ha_msg_del(m);
	return(rc);
}

int
ccm_send_abort(ll_cluster_t *hb, ccm_info_t *info, 
		const char *dest, 
		const int major, 
		const int minor)
{
	struct ha_msg *m = ccm_create_msg(info, CCM_TYPE_ABORT);
	int  rc;
	char majortrans[15]; 
	char minortrans[15]; 
	
	if (m == NULL){
		return HA_FAIL;
	}
	
	snprintf(majortrans, sizeof(majortrans), "%d", 
		 info->ccm_transition_major);
	snprintf(minortrans, sizeof(minortrans), "%d", 
		 info->ccm_transition_minor);
	
	if (ha_msg_mod(m,CCM_MAJORTRANS ,majortrans) != HA_OK
	    || ha_msg_mod(m,CCM_MINORTRANS ,majortrans) != HA_OK){
		cl_log(LOG_ERR, "modifying fields failed");
		ha_msg_del(m);
		return HA_FAIL;
	}
	
	rc = hb->llc_ops->sendnodemsg(hb, m, dest);
	ha_msg_del(m);
	return(rc);
}




/* Fake up a leave message. 
 * This is generally done when heartbeat informs ccm of the crash of 
 * a cluster member. 
 */
struct ha_msg *
ccm_create_leave_msg(ccm_info_t *info, int uuid)
{
	struct ha_msg *m = ccm_create_msg(info, CCM_TYPE_LEAVE);
	llm_info_t *llm;
	char *nodename;
	
	if (m == NULL){
		cl_log(LOG_ERR, "message creating failed");
		return NULL;
	}

	/* find the name of the node at index */
	llm = &(info->llm);
	nodename = llm_get_nodeid_from_uuid(llm, uuid);
	
	if(ha_msg_add(m, F_ORIG, nodename) == HA_FAIL) {
		cl_log(LOG_ERR, "adding field failed");
		ha_msg_del(m);
		return NULL;
	}

	return(m);
}

int
timeout_msg_init(ccm_info_t *info)
{
	return HA_OK;
}




static struct ha_msg * timeout_msg = NULL;

struct ha_msg  *
timeout_msg_mod(ccm_info_t *info)
{
	struct ha_msg *m = timeout_msg;
	char *hname;
	
	if (m !=NULL){
		if (ccm_mod_msg(m, info, CCM_TYPE_TIMEOUT) != HA_OK){
			cl_log(LOG_ERR, "mod message failed");
			ha_msg_del(timeout_msg);
			timeout_msg = NULL;
			return NULL;
		}
		return m;
	}
	
	m = ccm_create_minimum_msg(info, CCM_TYPE_TIMEOUT);
	if (m == NULL){
		cl_log(LOG_ERR, "creating a message failed");
		return NULL;
	}
	
	hname = info->llm.nodes[info->llm.myindex].nodename;
	
	if (ha_msg_add(m, F_ORIG, hname) == HA_FAIL
	    || ha_msg_add(m, CCM_COOKIE, "  ") == HA_FAIL
	    || ha_msg_add(m, CCM_MAJORTRANS, "0") == HA_FAIL
	    ||(ha_msg_add(m, CCM_MINORTRANS, "0") == HA_FAIL)){
		cl_log(LOG_ERR, "Adding field to a message failed");       
		ha_msg_del(m);
		return NULL;
	}
	timeout_msg = m;
	return m;
}	
	


/*broadcast CCM_TYPE_NODE_LEAVE_NOTICE */
int
ccm_bcast_node_leave_notice(ll_cluster_t* hb, 
			    ccm_info_t* info,
			    const char* node)
{
	return ccm_send_extra_clustermsg(hb, info, CCM_TYPE_NODE_LEAVE_NOTICE,
					 F_NODE, node);
}




int
send_node_leave_to_leader(ll_cluster_t *hb, ccm_info_t *info, const char *node)
{
	return ccm_send_extra_nodemsg(hb, info, CCM_TYPE_NODE_LEAVE,
				      F_NODE, node,
				      node);

}

int
ccm_send_to_all(ll_cluster_t *hb, ccm_info_t *info, 
		char *memlist, char *newcookie,
		int *uptime_list, size_t uptime_size)
{  
	struct ha_msg *m = ccm_create_msg(info, CCM_TYPE_MEM_LIST);
	char activeproto[3];
	int rc;

	if (m == NULL){
		cl_log(LOG_ERR, "creating msg failed");
		return HA_FAIL;
	}
	
	snprintf(activeproto, sizeof(activeproto), "%d", info->ccm_active_proto);

	if ( ha_msg_add(m, CCM_MEMLIST, memlist) == HA_FAIL
	     || cl_msg_add_list_int(m, CCM_UPTIMELIST, uptime_list, uptime_size)
	     == HA_FAIL
	     || !newcookie? FALSE: (ha_msg_add(m, CCM_NEWCOOKIE, newcookie)
				    ==HA_FAIL)) {
		cl_log(LOG_ERR, "ccm_send_final_memlist: Cannot create "
		       "FINAL_MEMLIST message");
		ha_msg_del(m);
		return HA_FAIL;
	}
	
	rc = hb->llc_ops->sendclustermsg(hb, m);
	ha_msg_del(m);
	return(rc);
}


int ccm_send_alive_msg(ll_cluster_t *hb, ccm_info_t *info)
{
	return ccm_send_standard_clustermsg(hb, info, CCM_TYPE_ALIVE);
}


int
ccm_send_newnode_to_leader(ll_cluster_t *hb, 
			   ccm_info_t *info, 
			   const char *node)
{
	
	return ccm_send_extra_nodemsg(hb, info, CCM_TYPE_NEW_NODE,
				      F_NODE, node,
				      info->llm.nodes[info->ccm_cluster_leader].nodename);
}



/* send a message to node 
 * the message contains my state informaton
 */

int
ccm_send_state_info(ll_cluster_t* hb, ccm_info_t* info, const char* node)
{
	
	return ccm_send_extra_nodemsg(hb, info, CCM_TYPE_STATE_INFO,
				      F_STATE, state2string(info->state),
				      node);
	
}



int
ccm_send_restart_msg(ll_cluster_t* hb, ccm_info_t* info)
{
	return ccm_send_minimum_clustermsg(hb, info, CCM_TYPE_RESTART);
}
