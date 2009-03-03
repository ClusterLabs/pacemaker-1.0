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

#include <crm_internal.h>

#include <crm/crm.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/transition.h>
/* #include <sys/param.h> */
/*  */


extern crm_graph_functions_t *graph_fns;

static gboolean
pseudo_action_dummy(crm_graph_t *graph, crm_action_t *action) 
{
    static int fail = -1;
    if(fail < 0) {
	char *fail_s = getenv("PE_fail");
	if(fail_s) {
	    fail = crm_int_helper(fail_s, NULL);
	} else {
	    fail = 0;
	}
    }
    
	crm_debug_2("Dummy event handler: action %d executed", action->id);
	if(action->id == fail) {
	    crm_err("Dummy event handler: pretending action %d failed", action->id);
	    action->failed = TRUE;
	    graph->abort_priority = INFINITY;
	}
	action->confirmed = TRUE;
	update_graph(graph, action);
	return TRUE;
}

crm_graph_functions_t default_fns = {
	pseudo_action_dummy,
	pseudo_action_dummy,
	pseudo_action_dummy,
	pseudo_action_dummy
};

void
set_default_graph_functions(void) 
{
	graph_fns = &default_fns;
}

void
set_graph_functions(crm_graph_functions_t *fns) 
{
	crm_info("Setting custom graph functions");
	graph_fns = fns;

	CRM_ASSERT(graph_fns != NULL);
	CRM_ASSERT(graph_fns->rsc != NULL);
	CRM_ASSERT(graph_fns->crmd != NULL);
	CRM_ASSERT(graph_fns->pseudo != NULL);
	CRM_ASSERT(graph_fns->stonith != NULL);
}

const char *
transition_status(enum transition_status state) 
{
	switch(state) {
		case transition_active:
			return "active";
		case transition_pending:
			return "pending";
		case transition_complete:
			return "complete";
		case transition_stopped:
			return "stopped";
		case transition_terminated:
			return "terminated";
		case transition_action_failed:
			return "failed (action)";
		case transition_failed:
			return "failed";			
	}
	return "unknown";			
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

static void
print_elem(int log_level, const char *prefix, gboolean as_input, crm_action_t *action) 
{
	int priority = 0;
	const char *key = NULL;
	const char *host = NULL;
	const char *class = "Action";
	const char *state = "Pending";

	if(action->failed) {
		state = "Failed";

	} else if(action->confirmed) {
		state = "Completed";

	} else if(action->executed) {
		state = "In-flight";

	} else if(action->sent_update) {
		state = "Update sent";
	}

	if(as_input) {
		class = "Input";
	}

	if(as_input == FALSE) {
		priority = action->synapse->priority;
	}
	
	key = crm_element_value(action->xml, XML_LRM_ATTR_TASK_KEY);
	host = crm_element_value(action->xml, XML_LRM_ATTR_TARGET);
	
	switch(action->type) {
		case action_type_pseudo:
			do_crm_log(log_level,
				      "%s[%s %d]: %s (id: %s, type: %s, priority: %d)",
				      prefix, class, action->id, state, key,
				      actiontype2text(action->type),
				      priority);
			break;
		case action_type_rsc:
			do_crm_log(log_level,
				      "%s[%s %d]: %s (id: %s, loc: %s, priority: %d)",
				      prefix, class, action->id, state, key, host,
				      priority);
			break;
		case action_type_crm:	
			do_crm_log(log_level,
				      "%s[%s %d]: %s (id: %s, loc: %s, type: %s, priority: %d)",
				      prefix, class, action->id, state, key, host,
				      actiontype2text(action->type),
				      priority);
			break;
		default:
			crm_err("%s[%s %d]: %s (id: %s, loc: %s, type: %s (unhandled), priority: %d)",
				prefix, class, action->id, state, key, host,
				actiontype2text(action->type),
				priority);
	}

	if(as_input == FALSE) {
		return;
	}
	
	if(action->timer) {
		do_crm_log(log_level, "%s\ttimeout=%d, timer=%d", prefix,
			      action->timeout, action->timer->source_id);
	}
	
	if(action->confirmed == FALSE) {
		crm_log_xml(LOG_DEBUG_3, "\t\t\tRaw xml: ", action->xml);
	}
}

void
print_action(int log_level, const char *prefix, crm_action_t *action) 
{
	print_elem(log_level, prefix, FALSE, action);
}

void
print_graph(unsigned int log_level, crm_graph_t *graph)
{
	if(graph == NULL || graph->num_actions == 0) {
		if(log_level > LOG_DEBUG) {
			crm_debug("## Empty transition graph ##");
		}
		return;
	}
		
	do_crm_log(log_level, "Graph %d (%d actions in %d synapses):"
		   " batch-limit=%d jobs, network-delay=%dms",
		   graph->id, graph->num_actions, graph->num_synapses,
		   graph->batch_limit, graph->network_delay);
	
	slist_iter(
		synapse, synapse_t, graph->synapses, lpc,

		do_crm_log(log_level, "Synapse %d %s (priority: %d)",
			      synapse->id,
			      synapse->confirmed?"was confirmed":
			        synapse->executed?"was executed":
			      "is pending",
			      synapse->priority);
		
		if(synapse->confirmed == FALSE) {
			slist_iter(
				action, crm_action_t, synapse->actions, lpc2,
				print_elem(log_level, "    ", FALSE, action);
				);
		}
		if(synapse->executed == FALSE) {
			slist_iter(
				input, crm_action_t, synapse->inputs, lpc2,
				print_elem(log_level, "     * ", TRUE, input);
				);
		}
		
		);
}

static const char *abort2text(enum transition_action abort_action)
{
    switch(abort_action) {
	case tg_done:
	    return "done";
	case tg_stop:
	    return "stop";
	case tg_restart:
	    return "restart";
	case tg_shutdown:
	    return "shutdown";
    }
    return "unknown";
}

void
update_abort_priority(
	crm_graph_t *graph, int priority,
	enum transition_action action, const char *abort_reason)
{
	if(graph == NULL) {
		return;
	}
	
	if(graph->abort_priority < priority) {
		crm_info("Abort priority upgraded from %d to %d",
			 graph->abort_priority, priority);
		graph->abort_priority = priority;
		if(graph->abort_reason != NULL) {
			crm_info("'%s' abort superceeded",
				 graph->abort_reason);
		}
		graph->abort_reason = abort_reason;
	}

	if(graph->completion_action < action) {
		crm_info("Abort action %s superceeded by %s",
			 abort2text(graph->completion_action), abort2text(action));
		graph->completion_action = action;
	}
}

