/* $Id: utils.c,v 1.13 2005/02/01 22:48:16 andrew Exp $ */
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
#include <sys/param.h>
#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/msg.h>
#include <crm/common/xml.h>
#include <tengine.h>
#include <heartbeat.h>
#include <clplumbing/Gmain_timeout.h>
#include <lrm/lrm_api.h>

extern cib_t *te_cib_conn;
extern int global_transition_timer;

void print_input(const char *prefix, action_t *input, gboolean to_file);
void print_action(const char *prefix, action_t *action, gboolean to_file);
gboolean timer_callback(gpointer data);

void
/* send_abort(const char *text, HA_Message *msg) */
send_abort(const char *text, crm_data_t *msg)
{	
	HA_Message *cmd = NULL;

	if(msg != NULL) {
		crm_info("Sending \"abort\" message... details follow");
		crm_xml_info(msg, text);
	} else {
		crm_info("Sending \"abort\" message... %s", text);
	}
	
	print_state(TRUE);
	initialize_graph();

	cmd = create_request(CRM_OP_TEABORT, NULL, NULL,
			     CRM_SYSTEM_DC, CRM_SYSTEM_TENGINE, NULL);
	ha_msg_add(cmd, "message", text);

#ifdef TESTING
	crm_log_message(LOG_ERR, cmd);
	g_main_quit(mainloop);
	return;
#else
	send_ipc_message(crm_ch, cmd);
#endif	

}

void
send_success(const char *text)
{	
	HA_Message *cmd = NULL;
	if(in_transition == FALSE) {
		crm_warn("Not in transition, not sending message");
		return;
	}
	in_transition = FALSE;

	crm_info("Transition \"complete\": %s", text);

	print_state(TRUE);
	initialize_graph();

	cmd = create_request(CRM_OP_TECOMPLETE, NULL, NULL,
			     CRM_SYSTEM_DC, CRM_SYSTEM_TENGINE, NULL);
	ha_msg_add(cmd, "message", text);

#ifdef TESTING
	crm_log_message(LOG_INFO, cmd);
	g_main_quit(mainloop);
	return;
#else
	send_ipc_message(crm_ch, cmd);
#endif	
}

void
print_state(int log_level)
{
	
	do_crm_log(log_level, __FUNCTION__, NULL, "###########");
	if(graph == NULL) {
		do_crm_log(log_level, __FUNCTION__, NULL,
			   "\tEmpty transition graph");
		do_crm_log(log_level, __FUNCTION__, NULL, "###########");
		return;
	}

	slist_iter(
		synapse, synapse_t, graph, lpc,

		do_crm_log(log_level, __FUNCTION__, NULL, "Synapse %d %s",
			  synapse->id,
			  synapse->complete?"has completed":"is pending");
		
		if(synapse->complete == FALSE) {
			slist_iter(
				input, action_t, synapse->inputs, lpc2,
				print_input("\t", input, log_level+1);
				);
		}
		
		slist_iter(
			action, action_t, synapse->actions, lpc2,
			print_action("\t", action, log_level);
			);
		);
	
	do_crm_log(log_level, __FUNCTION__, NULL, "###########");
}

void
print_input(const char *prefix, action_t *input, int log_level) 
{
	do_crm_log(log_level, __FUNCTION__, NULL, "%s[Input %d] %s (%s)",
		   prefix, input->id,
		   input->complete?"Satisfied":"Pending",
		   actiontype2text(input->type));

	if(input->complete == FALSE) {
		crm_log_xml(log_level, "\t  Raw input", input->xml);
	}
}

void
print_action(const char *prefix, action_t *action, int log_level) 
{
	do_crm_log(log_level, __FUNCTION__, NULL,
		   "%s[Action %d] %s (%d - %s fail)",
		   prefix,
		   action->id,
		   action->complete?"Completed":
			action->invoked?"In-flight":"Pending",
		   action->type,
		   action->can_fail?"can":"cannot");
	
	do_crm_log(log_level, __FUNCTION__, NULL, "%s  timeout=%d, timer=%d",
		   prefix,
		   action->timeout,
		   action->timer->source_id);

	if(action->complete == FALSE) {
		crm_log_xml(log_level, "\t  Raw action", action->xml);
	}
}

gboolean
do_update_cib(crm_data_t *xml_action, int status)
{
	char *code;
	char since_epoch[64];
	crm_data_t *fragment = NULL;
	crm_data_t *state    = NULL;
	crm_data_t *rsc      = NULL;

	enum cib_errors rc = cib_ok;
	
	const char *task   = crm_element_value(xml_action, XML_LRM_ATTR_TASK);
	const char *rsc_id = crm_element_value(xml_action, XML_LRM_ATTR_RSCID);
	const char *target = crm_element_value(xml_action, XML_LRM_ATTR_TARGET);
	const char *target_uuid =
		crm_element_value(xml_action, XML_LRM_ATTR_TARGET_UUID);

	int call_options = cib_scope_local|cib_discard_reply|cib_sync_call;
	call_options |= cib_inhibit_notify;
	
	if(status == LRM_OP_TIMEOUT) {
		if(crm_element_value(xml_action, XML_LRM_ATTR_RSCID) != NULL) {
			crm_warn("%s: %s %s on %s timed out",
				 crm_element_name(xml_action), task, rsc_id, target);
		} else {
			crm_warn("%s: %s on %s timed out",
				 crm_element_name(xml_action), task, target);
		}
	}
	
/*
  update the CIB

<node_state id="hadev">
      <lrm>
        <lrm_resources>
          <lrm_resource id="rsc2" last_op="start" op_code="0" target="hadev"/>
*/

	fragment = NULL;
	state    = create_xml_node(NULL, XML_CIB_TAG_STATE);

#ifdef TESTING

	/* turn the "pending" notification into a "op completed" notification
	 *  when testing... exercises more code this way.
	 */
	if(status == -1) {
		status = 0;
	}
#endif
	set_xml_property_copy(state,   XML_ATTR_UUID,  target_uuid);
	set_xml_property_copy(state,   XML_ATTR_UNAME, target);
	
	if(status != -1 && (safe_str_eq(task, CRM_OP_SHUTDOWN))) {
		sprintf(since_epoch, "%ld", (unsigned long)time(NULL));
		set_xml_property_copy(rsc, XML_CIB_ATTR_STONITH, since_epoch);
		
	} else {
		code = crm_itoa(status);
		
		rsc = create_xml_node(state, XML_CIB_TAG_LRM);
		rsc = create_xml_node(rsc,   XML_LRM_TAG_RESOURCES);
		rsc = create_xml_node(rsc,   XML_LRM_TAG_RESOURCE);
		
		set_xml_property_copy(rsc, XML_ATTR_ID,         rsc_id);
		set_xml_property_copy(rsc, XML_LRM_ATTR_TARGET, target);
		set_xml_property_copy(
			rsc, XML_LRM_ATTR_TARGET_UUID, target_uuid);

		if(safe_str_eq(CRMD_RSCSTATE_START, task)) {
			set_xml_property_copy(
				rsc, XML_LRM_ATTR_RSCSTATE,
				CRMD_RSCSTATE_START_PENDING);

		} else if(safe_str_eq(CRMD_RSCSTATE_STOP, task)) {
			set_xml_property_copy(
				rsc, XML_LRM_ATTR_RSCSTATE,
				CRMD_RSCSTATE_STOP_PENDING);

		} else {
			crm_warn("Using status \"pending\" for op \"%s\"..."
				 " this is still in the experimental stage.",
				 crm_str(task));
			set_xml_property_copy(
				rsc, XML_LRM_ATTR_RSCSTATE,
				CRMD_RSCSTATE_GENERIC_PENDING);
		}
		
		set_xml_property_copy(rsc, XML_LRM_ATTR_OPSTATUS, code);
		set_xml_property_copy(rsc, XML_LRM_ATTR_RC, code);
		set_xml_property_copy(rsc, XML_LRM_ATTR_LASTOP, task);

		crm_free(code);
	}

	fragment = create_cib_fragment(state, NULL);
	
	do_crm_log(LOG_DEBUG, __FUNCTION__, NULL,
		   "Updating CIB with \"%s\" (%s): %s %s on %s",
		   status<0?"new action":XML_ATTR_TIMEOUT,
		   crm_element_name(xml_action), crm_str(task), rsc_id, target);
	
	rc = te_cib_conn->cmds->modify(
		te_cib_conn, XML_CIB_TAG_STATUS, fragment, NULL, call_options);
	
	free_xml(fragment);
	free_xml(state);

	if(rc != cib_ok) {
		return FALSE;
	}

	return TRUE;
}

gboolean
timer_callback(gpointer data)
{
	te_timer_t *timer = NULL;
	
	if(data == NULL) {
		crm_err("Timer popped with no data");
		return FALSE;
	}
	
	timer = (te_timer_t*)data;
	if(timer->source_id > 0) {
		g_source_remove(timer->source_id);
	}
	timer->source_id = -1;
	
	if(timer->reason == timeout_fuzz) {
		crm_warn("Transition timeout reached..."
			 " marking transition complete.");
		send_success("success");
		return TRUE;

	} else if(timer->reason == timeout_timeout) {
		
		/* global timeout - abort the transition */
		crm_warn("Transition timeout reached..."
			 " marking transition complete.");
		
		crm_warn("Some actions may not have been executed.");
			
		send_success(XML_ATTR_TIMEOUT);
		
		return TRUE;
		
	} else if(timer->action == NULL) {
		crm_err("Action not present!");
		return FALSE;
		
	} else {
		/* fail the action
		 * - which may or may not abort the transition
		 */

		/* TODO: send a cancel notice to the LRM */
		/* TODO: use the ack from above to update the CIB */
		return do_update_cib(timer->action->xml, LRM_OP_TIMEOUT);
	}
}

gboolean
start_te_timer(te_timer_t *timer)
{
	if(((int)timer->source_id) < 0 && timer->timeout > 0) {
		timer->source_id = Gmain_timeout_add(
			timer->timeout, timer_callback, (void*)timer);
		return TRUE;

	} else if(timer->timeout < 0) {
		crm_err("Tried to start timer with -ve period");
		
	} else {
		crm_debug("#!!#!!# Timer already running (%d)",
			  timer->source_id);
	}
	return FALSE;		
}


gboolean
stop_te_timer(te_timer_t *timer)
{
	if(timer == NULL) {
		return FALSE;
	}
	
	if(((int)timer->source_id) > 0) {
		g_source_remove(timer->source_id);
		timer->source_id = -2;

	} else {
		return FALSE;
	}

	return TRUE;
}

const char *
actiontype2text(action_type_e type)
{
	switch(type) {
		case action_type_pseudo:
			return "pseduo";
		case action_type_rsc:
			return "rsc";
		case action_type_crm:
			return "crm";
			
	}
	return "<unknown>";
}

