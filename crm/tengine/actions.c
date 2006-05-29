/* $Id: actions.c,v 1.33 2006/05/29 13:20:43 andrew Exp $ */
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

#include <portability.h>

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
#include <clplumbing/lsb_exitcodes.h>

char *te_uuid = NULL;
IPC_Channel *crm_ch = NULL;

void send_rsc_command(crm_action_t *action);
extern crm_action_timer_t *transition_timer;

static void
te_start_action_timer(crm_action_t *action) 
{
	crm_malloc0(action->timer, sizeof(crm_action_timer_t));
	action->timer->timeout   = action->timeout;
	action->timer->reason    = timeout_action_warn;
	action->timer->action    = action;
	action->timer->source_id = Gmain_timeout_add(
		action->timer->timeout,
		action_timer_callback, (void*)action->timer);

	CRM_ASSERT(action->timer->source_id != 0);
}


static gboolean
te_pseudo_action(crm_graph_t *graph, crm_action_t *pseudo) 
{
	crm_info("Pseudo action %d confirmed", pseudo->id);
	pseudo->confirmed = TRUE;
	update_graph(graph, pseudo);
	trigger_graph();
	return TRUE;
}

void
send_stonith_update(stonith_ops_t * op)
{
	enum cib_errors rc = cib_ok;
	const char *target = op->node_name;
	const char *uuid   = op->node_uuid;
	
	/* zero out the node-status & remove all LRM status info */
	crm_data_t *node_state = create_xml_node(NULL, XML_CIB_TAG_STATE);
	
	CRM_CHECK(op->node_name != NULL, return);
	CRM_CHECK(op->node_uuid != NULL, return);
	
	crm_xml_add(node_state, XML_ATTR_UUID,  uuid);
	crm_xml_add(node_state, XML_ATTR_UNAME, target);
	crm_xml_add(node_state, XML_CIB_ATTR_HASTATE,   DEADSTATUS);
	crm_xml_add(node_state, XML_CIB_ATTR_INCCM,     XML_BOOLEAN_NO);
	crm_xml_add(node_state, XML_CIB_ATTR_CRMDSTATE, OFFLINESTATUS);
	crm_xml_add(node_state, XML_CIB_ATTR_JOINSTATE, CRMD_JOINSTATE_DOWN);
	crm_xml_add(node_state, XML_CIB_ATTR_EXPSTATE,  CRMD_JOINSTATE_DOWN);
	crm_xml_add(node_state, XML_CIB_ATTR_REPLACE,   XML_CIB_TAG_LRM);
	crm_xml_add(node_state, XML_ATTR_ORIGIN,   __FUNCTION__);
	
	rc = te_cib_conn->cmds->update(
		te_cib_conn, XML_CIB_TAG_STATUS, node_state, NULL,
		cib_quorum_override|cib_scope_local);	
	
	if(rc < cib_ok) {
		crm_err("CIB update failed: %s", cib_error2string(rc));
		abort_transition(
			INFINITY, tg_shutdown, "CIB update failed", node_state);
		
	} else {
		/* delay processing the trigger until the update completes */
		add_cib_op_callback(rc, FALSE, NULL, cib_fencing_updated);
	}
	
	free_xml(node_state);
	return;
}

static gboolean
te_fence_node(crm_graph_t *graph, crm_action_t *action)
{
	char *key = NULL;
	const char *id = NULL;
	const char *uuid = NULL;
	const char *target = NULL;
	const char *type = NULL;
	stonith_ops_t * st_op = NULL;
	
	id = ID(action->xml);
	target = crm_element_value(action->xml, XML_LRM_ATTR_TARGET);
	uuid = crm_element_value(action->xml, XML_LRM_ATTR_TARGET_UUID);
	type = g_hash_table_lookup(action->params, crm_meta_name("stonith_action"));
	
	CRM_CHECK(id != NULL,
		  crm_log_xml_warn(action->xml, "BadAction");
		  return FALSE);
	CRM_CHECK(uuid != NULL,
		  crm_log_xml_warn(action->xml, "BadAction");
		  return FALSE);
	CRM_CHECK(type != NULL,
		  crm_log_xml_warn(action->xml, "BadAction");
		  return FALSE);
	CRM_CHECK(target != NULL,
		  crm_log_xml_warn(action->xml, "BadAction");
		  return FALSE);

	te_log_action(LOG_INFO,
		      "Executing %s fencing operation (%s) on %s (timeout=%d)",
		      type, id, target,
		      transition_graph->transition_timeout / 2);

	crm_malloc0(st_op, sizeof(stonith_ops_t));
	if(safe_str_eq(type, "poweroff")) {
		st_op->optype = POWEROFF;
	} else {
		st_op->optype = RESET;
	}
	
	st_op->timeout = transition_graph->transition_timeout / 2;
	st_op->node_name = crm_strdup(target);
	st_op->node_uuid = crm_strdup(uuid);
	
	key = generate_transition_key(transition_graph->id, te_uuid);
	st_op->private_data = crm_concat(id, key, ';');
	crm_free(key);
	
	CRM_ASSERT(stonithd_input_IPC_channel() != NULL,
		   crm_err("Cannot fence %s: stonith not available.  Exiting", target));
		
	if (ST_OK != stonithd_node_fence( st_op )) {
		crm_err("Cannot fence %s: stonithd_node_fence() call failed ",
			target);
	}
	return TRUE;
}

static gboolean
te_crm_command(crm_graph_t *graph, crm_action_t *action)
{
	char *value = NULL;
	char *counter = NULL;
	HA_Message *cmd = NULL;		

	const char *id = NULL;
	const char *task = NULL;
	const char *on_node = NULL;

	gboolean ret = TRUE;

	id      = ID(action->xml);
	task    = crm_element_value(action->xml, XML_LRM_ATTR_TASK);
	on_node = crm_element_value(action->xml, XML_LRM_ATTR_TARGET);

	CRM_CHECK(on_node != NULL && strlen(on_node) != 0,
		  te_log_action(LOG_ERR, "Corrupted command (id=%s) %s: no node",
				crm_str(id), crm_str(task));
		  return FALSE);
	
	te_log_action(LOG_INFO, "Executing crm-event (%s): %s on %s",
		      crm_str(id), crm_str(task), on_node);
	
	cmd = create_request(task, NULL, on_node, CRM_SYSTEM_CRMD,
			     CRM_SYSTEM_TENGINE, NULL);
	
	counter = generate_transition_key(transition_graph->id, te_uuid);
	crm_xml_add(cmd, XML_ATTR_TRANSITION_KEY, counter);
	ret = send_ipc_message(crm_ch, cmd);
	crm_free(counter);
	
	value = g_hash_table_lookup(action->params, crm_meta_name(XML_ATTR_TE_NOWAIT));
	if(ret == FALSE) {
		crm_err("Action %d failed: send", action->id);
		return FALSE;
		
	} else if(crm_is_true(value)) {
		crm_info("Skipping wait for %d", action->id);
		action->confirmed = TRUE;
		update_graph(graph, action);
		trigger_graph();
		
	} else if(ret && action->timeout > 0) {
		crm_debug("Setting timer for action %d",action->id);
		action->timer->reason = timeout_action_warn;
		te_start_action_timer(action);
	}
	
	return TRUE;
}

static gboolean
te_rsc_command(crm_graph_t *graph, crm_action_t *action) 
{
	/* never overwrite stop actions in the CIB with
	 *   anything other than completed results
	 *
	 * Writing pending stops makes it look like the
	 *   resource is running again
	 */
	const char *task = NULL;
	const char *on_node = NULL;
	action->executed = FALSE;

	on_node = crm_element_value(action->xml, XML_LRM_ATTR_TARGET);
	CRM_CHECK(on_node != NULL && strlen(on_node) != 0,
		  te_log_action(LOG_ERR, "Corrupted command(id=%s) %s: no node",
				ID(action->xml), crm_str(task));
		  return FALSE);
	
	send_rsc_command(action);
	return TRUE;
}

gboolean
cib_action_update(crm_action_t *action, int status)
{
	char *code = NULL;
	char *digest = NULL;
	crm_data_t *params   = NULL;
	crm_data_t *state    = NULL;
	crm_data_t *rsc      = NULL;
	crm_data_t *xml_op   = NULL;
	crm_data_t *action_rsc = NULL;
	char *op_id = NULL;

	enum cib_errors rc = cib_ok;

	const char *name = NULL;
	const char *value = NULL;
	const char *rsc_id = NULL;
	const char *task   = crm_element_value(action->xml, XML_LRM_ATTR_TASK);
	const char *target = crm_element_value(action->xml, XML_LRM_ATTR_TARGET);
	const char *task_uuid = crm_element_value(
		action->xml, XML_LRM_ATTR_TASK_KEY);
	
	const char *target_uuid = crm_element_value(
		action->xml, XML_LRM_ATTR_TARGET_UUID);

	int call_options = cib_quorum_override|cib_scope_local;

	crm_warn("%s %d: %s on %s timed out",
		 crm_element_name(action->xml), action->id, task_uuid, target);

	action_rsc = find_xml_node(action->xml, XML_CIB_TAG_RESOURCE, TRUE);
	if(action_rsc == NULL) {
		return FALSE;
	}
	
	rsc_id = ID(action_rsc);
	CRM_CHECK(rsc_id != NULL,
		  crm_log_xml_err(action->xml, "Bad:action");
		  return FALSE);
	
	code = crm_itoa(status);
	
/*
  update the CIB

<node_state id="hadev">
      <lrm>
        <lrm_resources>
          <lrm_resource id="rsc2" last_op="start" op_code="0" target="hadev"/>
*/

	state    = create_xml_node(NULL, XML_CIB_TAG_STATE);

	crm_xml_add(state, XML_ATTR_UUID,  target_uuid);
	crm_xml_add(state, XML_ATTR_UNAME, target);
	
	rsc = create_xml_node(state, XML_CIB_TAG_LRM);
	crm_xml_add(rsc, XML_ATTR_ID, target_uuid);

	rsc = create_xml_node(rsc,   XML_LRM_TAG_RESOURCES);
	rsc = create_xml_node(rsc,   XML_LRM_TAG_RESOURCE);
	crm_xml_add(rsc, XML_ATTR_ID, rsc_id);

	name = XML_ATTR_TYPE;
	value = crm_element_value(action_rsc, name);
	crm_xml_add(rsc, name, value);
	name = XML_AGENT_ATTR_CLASS;
	value = crm_element_value(action_rsc, name);
	crm_xml_add(rsc, name, value);
	name = XML_AGENT_ATTR_PROVIDER;
	value = crm_element_value(action_rsc, name);
	crm_xml_add(rsc, name, value);

	xml_op = create_xml_node(rsc, XML_LRM_TAG_RSC_OP);	
	crm_xml_add(xml_op, XML_ATTR_ID, task);
	
	op_id = generate_op_key(rsc_id, task, action->interval);
	crm_xml_add(xml_op, XML_ATTR_ID, op_id);
	crm_free(op_id);
	
	crm_xml_add(xml_op, XML_LRM_ATTR_TASK, task);
	crm_xml_add(xml_op, XML_ATTR_CRM_VERSION, CRM_FEATURE_SET);
	crm_xml_add(xml_op, XML_LRM_ATTR_OPSTATUS, code);
	crm_xml_add(xml_op, XML_LRM_ATTR_CALLID, "-1");
	crm_xml_add_int(xml_op, XML_LRM_ATTR_INTERVAL, action->interval);
	crm_xml_add(xml_op, XML_LRM_ATTR_RC, code);
	crm_xml_add(xml_op, XML_ATTR_ORIGIN, __FUNCTION__);

	crm_free(code);

	code = generate_transition_key(transition_graph->id, te_uuid);
	crm_xml_add(xml_op, XML_ATTR_TRANSITION_KEY, code);
	crm_free(code);

	code = generate_transition_magic(
		crm_element_value(xml_op, XML_ATTR_TRANSITION_KEY), status, status);
	crm_xml_add(xml_op,  XML_ATTR_TRANSITION_MAGIC, code);
	crm_free(code);

	params = find_xml_node(action->xml, "attributes", TRUE);
	params = copy_xml(params);
	filter_action_parameters(params, CRM_FEATURE_SET);
	digest = calculate_xml_digest(params, TRUE);
	crm_xml_add(xml_op, XML_LRM_ATTR_OP_DIGEST, digest);
	crm_free(digest);
	free_xml(params);
	
	crm_debug_3("Updating CIB with \"%s\" (%s): %s %s on %s",
		  status<0?"new action":XML_ATTR_TIMEOUT,
		  crm_element_name(action->xml), crm_str(task), rsc_id, target);
	
	rc = te_cib_conn->cmds->update(
		te_cib_conn, XML_CIB_TAG_STATUS, state, NULL, call_options);

	crm_debug("Updating CIB with %s action %d: %s %s on %s (call_id=%d)",
		  op_status2text(status), action->id, task_uuid, rsc_id, target, rc);

	add_cib_op_callback(rc, FALSE, NULL, cib_action_updated);
	free_xml(state);

	action->sent_update = TRUE;
	
	if(rc < cib_ok) {
		return FALSE;
	}

	return TRUE;
}

void
send_rsc_command(crm_action_t *action) 
{
	HA_Message *cmd = NULL;
	crm_data_t *rsc_op  = NULL;
	char *counter = crm_itoa(transition_graph->id);

	const char *task    = NULL;
	const char *value   = NULL;
	const char *on_node = NULL;
	const char *task_uuid = NULL;

	CRM_ASSERT(action != NULL);
	CRM_ASSERT(action->xml != NULL);

	rsc_op  = action->xml;
	task    = crm_element_value(rsc_op, XML_LRM_ATTR_TASK);
	task_uuid = crm_element_value(action->xml, XML_LRM_ATTR_TASK_KEY);
	on_node = crm_element_value(rsc_op, XML_LRM_ATTR_TARGET);
	counter = generate_transition_key(transition_graph->id, te_uuid);
	crm_xml_add(rsc_op, XML_ATTR_TRANSITION_KEY, counter);

	crm_info("Initiating action %d: %s on %s",
		 action->id, task_uuid, on_node);

	crm_free(counter);
	
	if(rsc_op != NULL) {
		crm_log_xml_debug_2(rsc_op, "Performing");
	}
	cmd = create_request(CRM_OP_INVOKE_LRM, rsc_op, on_node,
			     CRM_SYSTEM_LRMD, CRM_SYSTEM_TENGINE, NULL);
	
#if 1
	send_ipc_message(crm_ch, cmd);
#else
	/* test the TE timer/recovery code */
	if((action->id % 11) == 0) {
		crm_err("Faking lost action %d: %s", action->id, task_uuid);
	} else {
		send_ipc_message(crm_ch, cmd);
	}
#endif
	
	action->executed = TRUE;
	value = g_hash_table_lookup(action->params, crm_meta_name(XML_ATTR_TE_NOWAIT));
	if(crm_is_true(value)) {
		crm_debug("Skipping wait for %d", action->id);
		action->confirmed = TRUE;
		update_graph(transition_graph, action);
		trigger_graph();

	} else if(action->timeout > 0) {
		int action_timeout = 2 * action->timeout;
		crm_debug_3("Setting timer for action %s", task_uuid);
		if(transition_graph->transition_timeout < action_timeout) {
			crm_debug("Action %d:"
				  " Increasing transition %d timeout to %d",
				  action->id, transition_graph->id,
				  transition_graph->transition_timeout);
			transition_graph->transition_timeout = action_timeout;
		}
		te_start_action_timer(action);
	}
}

crm_graph_functions_t te_graph_fns = {
	te_pseudo_action,
	te_rsc_command,
	te_crm_command,
	te_fence_node
};

void
notify_crmd(crm_graph_t *graph)
{	
	HA_Message *cmd = NULL;
	int log_level = LOG_DEBUG;
	const char *op = CRM_OP_TEABORT;
	int pending_callbacks = num_cib_op_callbacks();
	

	stop_te_timer(transition_timer);
	
	if(pending_callbacks != 0) {
		crm_warn("Delaying completion until all CIB updates complete");
		return;
	}

	CRM_CHECK(graph->complete, graph->complete = TRUE);

	switch(graph->completion_action) {
		case tg_stop:
			op = CRM_OP_TECOMPLETE;
			log_level = LOG_INFO;
			break;

		case tg_abort:
		case tg_restart:
			op = CRM_OP_TEABORT;
			break;

		case tg_shutdown:
			crm_info("Exiting after transition");
			exit(LSB_EXIT_OK);
	}

	te_log_action(log_level, "Transition %d status: %s - %s",
		      graph->id, op, graph->abort_reason);

	print_graph(LOG_DEBUG_3, graph);
	
	cmd = create_request(
		op, NULL, NULL, CRM_SYSTEM_DC, CRM_SYSTEM_TENGINE, NULL);

	if(graph->abort_reason != NULL) {
		ha_msg_add(cmd, "message", graph->abort_reason);
	}

	send_ipc_message(crm_ch, cmd);

	graph->abort_reason = NULL;
	graph->completion_action = tg_restart;	

}
