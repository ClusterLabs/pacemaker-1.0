/* $Id: callbacks.c,v 1.14 2005/02/25 10:32:08 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
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

#include <sys/stat.h>

#include <hb_api.h>

#include <crm/crm.h>
#include <crm/common/xml.h>
#include <crm/msg_xml.h>
#include <crm/cib.h>
#include <heartbeat.h>

#include <tengine.h>

void te_update_confirm(const char *event, HA_Message *msg);

void
te_update_confirm(const char *event, HA_Message *msg)
{
	int rc = -1;
	gboolean done = FALSE;
	const char *op = cl_get_string(msg, F_CIB_OPERATION);
	const char *type = cl_get_string(msg, F_CIB_OBJTYPE);
	crm_data_t *update = get_message_xml(msg, F_CIB_UPDATE);

	ha_msg_value_int(msg, F_CIB_RC, &rc);
	crm_debug("Processing %s...", event);
	crm_xml_debug(update, "Processing update");
	
	if (MSG_LOG) {
		struct stat buf;
		if(stat(DEVEL_DIR, &buf) != 0) {
			cl_perror("Stat of %s failed... exiting", DEVEL_DIR);
			exit(100);
		}
	}
	
	if(op == NULL) {
		crm_err(
			"Illegal CIB update, the operation must be specified");
		send_abort("Illegal update", update);
		done = TRUE;
		
	} else if(strcmp(op, CRM_OP_CIB_CREATE) == 0
		  || strcmp(op, CRM_OP_CIB_DELETE) == 0
		  || strcmp(op, CRM_OP_CIB_REPLACE) == 0
		  || strcmp(op, CRM_OP_SHUTDOWN_REQ) == 0
		  || strcmp(op, CRM_OP_CIB_ERASE) == 0) {
		
		/* these are always unexpected, trigger the PE */
		send_abort("Non-update change", update);
		done = TRUE;
		
	} else if(strcmp(op, CRM_OP_CIB_UPDATE) != 0) {
		crm_verbose("Ignoring %s op confirmation", op);
		done = TRUE;
	}

	if(done) {
		free_xml(update);
		return;
	}
	
	if(safe_str_eq(type, XML_CIB_TAG_CRMCONFIG)) {
		/* ignore - for the moment */
		crm_devel("Ignoring changes to the %s section", type);
		
	} else if(safe_str_eq(type, XML_CIB_TAG_STATUS)) {
		/* this _may_ not be un-expected */
		if(extract_event(update) == FALSE) {
			send_abort("Unexpected status update", update);
		}

	} else if(safe_str_eq(type, XML_CIB_TAG_NODES)
		|| safe_str_eq(type, XML_CIB_TAG_RESOURCES)
		|| safe_str_eq(type, XML_CIB_TAG_CONSTRAINTS)) {
		/* these are never expected	 */
		crm_debug("Aborting on changes to the %s section", type);
		send_abort("Non-status update", update);

	} else {
		crm_warn("Ignoring update confirmation for %s object", type);
	}

	free_xml(update);
}

gboolean
process_te_message(HA_Message *msg, crm_data_t *xml_data, IPC_Channel *sender)
{
	const char *sys_to = cl_get_string(msg, F_CRM_SYS_TO);
	const char *ref    = cl_get_string(msg, XML_ATTR_REFERENCE);
	const char *op     = cl_get_string(msg, F_CRM_TASK);

	crm_log_message(LOG_VERBOSE, msg);
	
	if(safe_str_eq(cl_get_string(msg, F_CRM_MSG_TYPE), XML_ATTR_RESPONSE)
	   && safe_str_neq(op, CRM_OP_EVENTCC)) {
		crm_info("Message was a response not a request.  Discarding");
		return TRUE;
	}

	crm_devel("Processing %s (%s) message", op, ref);
	
	if(op == NULL){
		/* error */
	} else if(strcmp(op, CRM_OP_HELLO) == 0) {
		/* ignore */

	} else if(sys_to == NULL || strcmp(sys_to, CRM_SYSTEM_TENGINE) != 0) {
		crm_verbose("Bad sys-to %s", crm_str(sys_to));
		return FALSE;
		
	} else if(strcmp(op, CRM_OP_TRANSITION) == 0) {

		crm_trace("Initializing graph...");
		initialize_graph();

		crm_trace("Unpacking graph...");
		unpack_graph(xml_data);
		crm_trace("Initiating transition...");

		in_transition = TRUE;

		if(initiate_transition() == FALSE) {
			/* nothing to be done.. means we're done. */
			crm_info("No actions to be taken..."
			       " transition compelte.");
		}
		crm_trace("Processing complete...");
		
	} else if(strcmp(op, CRM_OP_TEABORT) == 0) {
		initialize_graph();

	} else if(strcmp(op, CRM_OP_QUIT) == 0) {
		crm_info("Received quit message, terminating");
		exit(0);
		
	} else if(in_transition == FALSE) {
		crm_info("Received event_cc while not in a transition..."
			 "  Poking the Policy Engine");
		send_abort("Initiate a transition", NULL);
#ifdef TESTING
	} else if(strcmp(op, CRM_OP_EVENTCC) == 0) {
		crm_trace("Processing %s...", CRM_OP_EVENTCC);
		if(extract_event(msg) == FALSE) {
			send_abort("ttest loopback", msg);
		}
#endif
	}

	crm_devel("finished processing message");
	print_state(LOG_DEV);
	
	return TRUE;
}

void
tengine_stonith_callback(stonith_ops_t * op, void * private_data)
{
	action_t *te_action = private_data;

	if(op == NULL) {
		crm_err("Called with a NULL op!");
		return;
	}
	
	crm_info("optype=%d, node_name=%s, result=%d, node_list=%s\n",
		 op->optype, op->node_name, op->op_result,
		 (char *)op->node_list);

	if(op->op_result == STONITH_SUCCEEDED) {
		enum cib_errors rc = cib_ok;
		crm_data_t *action_args = find_xml_node(
			te_action->xml, "args", TRUE);
		const char *target = crm_element_value(action_args, "target");
		const char *uuid = crm_element_value(action_args,"target_uuid");
		
		/* zero out the node-status & remove all LRM status info */
		crm_data_t *update = NULL;
		crm_data_t *node_state = create_xml_node(
			NULL, XML_CIB_TAG_STATE);

		set_xml_property_copy(node_state, XML_ATTR_UUID, uuid);
		set_xml_property_copy(node_state, XML_ATTR_UNAME, target);
		set_xml_property_copy(
			node_state, XML_CIB_ATTR_HASTATE, DEADSTATUS);
		set_xml_property_copy(
			node_state, XML_CIB_ATTR_INCCM, XML_BOOLEAN_NO);
		set_xml_property_copy(
			node_state, XML_CIB_ATTR_CRMDSTATE, OFFLINESTATUS);
		set_xml_property_copy(
			node_state, XML_CIB_ATTR_JOINSTATE,CRMD_JOINSTATE_DOWN);
		set_xml_property_copy(
			node_state, XML_CIB_ATTR_EXPSTATE, CRMD_JOINSTATE_DOWN);
		set_xml_property_copy(
			node_state, XML_CIB_ATTR_REPLACE, XML_CIB_TAG_LRM);
		create_xml_node(node_state, XML_CIB_TAG_LRM);
		
		update = create_cib_fragment(node_state, NULL);
		free_xml(node_state);
	
		rc = te_cib_conn->cmds->modify(
			te_cib_conn, XML_CIB_TAG_STATUS,update,NULL,cib_none);	

		if(rc == cib_ok) {
			/* mark the action complete */
			te_action->complete = TRUE;
			
			process_trigger(te_action->id);
			check_for_completion();
		} else {
			send_abort("Couldnt update CIB after stonith", update);
		}
		free_xml(update);
		
	} else {
		send_abort("Fencing op failed", NULL);
	}
}


void
tengine_stonith_connection_destroy(gpointer user_data)
{
	crm_err("Fencing daemon has left us: Shutting down...NOW");
	/* shutdown properly later */
	CRM_DEV_ASSERT(FALSE/* fencing daemon died */);
	return;
}

gboolean
tengine_stonith_dispatch(IPC_Channel *sender, void *user_data)
{
	int lpc = 0;

	while(stonithd_op_result_ready()) {
		if (sender->ch_status == IPC_DISCONNECT) {
			/* The message which was pending for us is that
			 * the IPC status is now IPC_DISCONNECT */
			break;
		}
		if(ST_FAIL == stonithd_receive_ops_result(FALSE)) {
			crm_err("stonithd_receive_ops_result() failed");
		} else {
			lpc++;
		}
	}

	crm_verbose("Processed %d messages", lpc);
	if (sender->ch_status == IPC_DISCONNECT) {
		return FALSE;
	}
	return TRUE;
}
