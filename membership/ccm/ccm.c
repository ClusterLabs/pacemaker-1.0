
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
	int 		uuid;
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
	
	uuid = llm_get_uuid(&info->llm, orig);
	if(uuid == -1) {
		return NULL;
	}
	
	return(ccm_create_leave_msg(info, uuid));
}

/* 
 * The callback function which is called when the status
 * of a node changes. 
 */

static void
nodelist_update(ll_cluster_t* hb, ccm_info_t* info, 
		const char *id, const char *status, int hbgen)
{
	llm_info_t *llm;
	int indx, uuid;
	char oldstatus[STATUSSIZE];
	/* update the low level membership of the node
	 * if the status moves from active to dead and if the member
	 * is already part of the ccm, then we have to mimic a
	 * leave message for us 
	 */
	if(global_debug)
		cl_log(LOG_DEBUG, 
		       "nodelist update: Node %s now has status %s gen=%d", 
		       id,  status, hbgen);
	llm = &info->llm;
	if(llm_status_update(llm, id, status,oldstatus)) {
		indx = ccm_get_membership_index(info,id);
		if(indx != -1) {
			uuid = llm_get_uuid(llm, id);
			leave_cache(uuid);
		}
	}
	
	if ( strncmp(LLM_GET_MYNODEID(llm), id,NODEIDSIZE ) == 0){
		return ;
	}
	if ( part_of_cluster(info->state)
	     && ( STRNCMP_CONST(oldstatus, DEADSTATUS) == 0
		  && STRNCMP_CONST(status, DEADSTATUS) != 0)){
		ccm_send_state_info(hb, info, id);
	}
		
	return;
}

int
ccm_control_process(ccm_info_t *info, ll_cluster_t * hb)
{
	struct ha_msg *msg, *newmsg;
	const char *type;
	int	ccm_msg_type;
	const char *orig=NULL;
	const char *status=NULL;

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
			
			int 	gen_val;
			const char *gen = ha_msg_value(msg, F_HBGENERATION);
			
			cl_log_message(LOG_INFO, msg);
			gen_val = atoi(gen?gen:"-1");
			nodelist_update(hb, info,orig, status, gen_val);

			ha_msg_del(msg);
			return TRUE;
			
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
