
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

extern state_msg_handler_t	state_msg_handler[];
extern int global_debug;

struct ha_msg * ccm_readmsg(ccm_info_t *info, ll_cluster_t *hb);

static struct ha_msg*
ccm_handle_hbapiclstat(ccm_info_t *info,  
		       const char *orig, 
		       const char *status)
{
	int 		index;
	enum ccm_state 	state = info->state;
	
	if(state == CCM_STATE_NONE ||
		state == CCM_STATE_VERSION_REQUEST) {
		return NULL;
	}

	assert(status);
	if(strncmp(status, JOINSTATUS, 5) == 0) {
		cl_log(LOG_INFO,
		       "ccm from %s started", orig);
		return NULL;
	}

	if(!orig){
		return NULL;
	}
	
	index = llm_get_index(&info->llm, orig);
	if(index == -1) {
		return NULL;
	}
	
	return(ccm_create_leave_msg(info, index));
}

/* 
 * The callback function which is called when the status
 * of a node changes. 
 */

static int
nodelist_update(ll_cluster_t* hb, ccm_info_t* info, 
		const char *id, const char *status)
{
	llm_info_t *llm;
	char oldstatus[STATUSSIZE];
	/* update the low level membership of the node
	 * if the status moves from active to dead and if the member
	 * is already part of the ccm, then we have to mimic a
	 * leave message for us 
	 */
	if(global_debug)
		cl_log(LOG_DEBUG, 
		       "nodelist update: Node %s now has status %s", 
		       id,  status);
	llm = &info->llm;
	
	if (llm_status_update(llm, id, status,oldstatus) != HA_OK) {
		cl_log(LOG_ERR, "%s: updating status for node %s failed",
		       __FUNCTION__, id);
		return HA_FAIL;
	}
	
	if (STRNCMP_CONST(status, DEADSTATUS) == 0){
		
		if(node_is_member(info, id)){
			leave_cache(llm_get_index(llm, id));
		}
		
	}
	
	if ( strncmp(llm_get_mynodename(llm), id,NODEIDSIZE ) == 0){
		return HA_OK;
	}
	if ( part_of_cluster(info->state)
	     && ( STRNCMP_CONST(oldstatus, DEADSTATUS) == 0
		  && STRNCMP_CONST(status, DEADSTATUS) != 0)){
		ccm_send_state_info(hb, info, id);
	}
	
	return HA_OK;
}

int
ccm_control_process(ccm_info_t *info, ll_cluster_t * hb)
{
	struct ha_msg*	msg;
	struct ha_msg*	newmsg;
	const char*	type;
	int		ccm_msg_type;
	const char*	orig=NULL;
	const char*	status=NULL;
	llm_info_t*	llm= &info->llm;
	const char*	mynode = llm_get_mynodename(llm);
	const char*	numnodes;
	int		numnodes_val;
	
 repeat:
	/* read the next available message */
	msg = ccm_readmsg(info, hb); /* this is non-blocking */

	if (msg) {
		type = ha_msg_value(msg, F_TYPE);
		orig = ha_msg_value(msg, F_ORIG);
		status = ha_msg_value(msg, F_STATUS);
		if(strcmp(type, T_APICLISTAT) == 0){
			/* handle ccm status of on other nodes of the cluster */
		       	if((newmsg = ccm_handle_hbapiclstat(info, orig, 
				status)) == NULL) {
				ha_msg_del(msg);
				return TRUE;
			}
			ha_msg_del(msg);
			msg = newmsg;
		} else if(strcasecmp(type, T_STATUS) == 0){
			
			cl_log_message(LOG_DEBUG, msg);
			if (llm_is_valid_node(&info->llm, orig)){

				if (nodelist_update(hb, info,orig, status) != HA_OK){
					cl_log(LOG_ERR, "%s: updating node status for "
					       "nodelist failed(%s-%s)",
					       __FUNCTION__, orig, status);
					return FALSE;
				}
				
				ha_msg_del(msg);
				return TRUE;
			}
			
			if (llm_add(llm, orig, status, mynode) != HA_OK){
				cl_log(LOG_ERR, "%s: adding node(%s) to llm failed",
				       __FUNCTION__,orig);
				return FALSE;
			}
			
			cl_log(LOG_INFO, "after adding node %s", orig);
			llm_display(llm);
			
			jump_to_joining_state(hb, info, msg);
			
		} else if (strcasecmp(type, T_DELNODE) ==0){
			const char* node = ha_msg_value(msg, F_NODE);
			if (node == NULL){
				cl_log(LOG_ERR, "%s: field node not found",
				       __FUNCTION__);
				cl_log_message(LOG_ERR, msg);
				return FALSE;
			}
			
			if (llm_del(llm, node) != HA_OK){
				cl_log(LOG_ERR, "%s: deleting node %s failed",
				       __FUNCTION__, node);
				return FALSE;
			}
			
			jump_to_joining_state(hb, info, msg);
			
		}

	} else {
		msg = timeout_msg_mod(info);
	}

	type = ha_msg_value(msg, F_TYPE);
	ccm_msg_type = ccm_string2type(type);
	if(global_debug){
		if(ccm_msg_type != CCM_TYPE_TIMEOUT){
			cl_log(LOG_DEBUG, "received message %s orig=%s", 
			       type, ha_msg_value(msg, F_ORIG));
		}
	}
	
	if (ccm_msg_type < 0){
		goto out;
	}
	
	if (ANYDEBUG){	
		if (ccm_msg_type != CCM_TYPE_TIMEOUT){
			cl_log(LOG_INFO, "get a message type=%s", type);
			cl_log_message(LOG_DEBUG, msg);
		}
	}

	numnodes = ha_msg_value(msg, F_NUMNODES);
	if(numnodes != NULL){
		numnodes_val = atoi(numnodes);
		if (numnodes_val != info->llm.nodecount){
			if (ANYDEBUG){
				cl_log(LOG_WARNING, " node count does not agree: "
				       "local count=%d,"
				       "count in message =%d",
				       info->llm.nodecount, numnodes_val);
				cl_log_message(LOG_DEBUG, msg);
			}
			jump_to_joining_state(hb, info, msg);
		}
	}
	state_msg_handler[info->state](ccm_msg_type, msg, hb, info);
	

 out:
	if(ccm_msg_type != CCM_TYPE_TIMEOUT) {
		ha_msg_del(msg);
	}
	
	/* If there is another message in the channel,
	 * process it now. 
	 */
	if (hb->llc_ops->msgready(hb))
		goto repeat;
	
	return TRUE;
}
