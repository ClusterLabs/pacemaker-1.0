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

#include <lha_internal.h>

#include <sys/param.h>
#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/common/msg.h>

#include <glib.h>

#include <allocate.h>
#include <lib/crm/pengine/utils.h>
#include <utils.h>

gboolean update_action(action_t *action);

gboolean
update_action_states(GListPtr actions)
{
	crm_debug_2("Updating %d actions", g_list_length(actions));
	slist_iter(
		action, action_t, actions, lpc,

		update_action(action);
		);

	return TRUE;
}

gboolean
update_action(action_t *action)
{
	int local_type = 0;
	int log_level = LOG_INFO;
	gboolean changed = FALSE;
	
	do_crm_log(log_level, "Processing action %s: %s",
		    action->uuid, action->optional?"optional":"required");

	slist_iter(
		other, action_wrapper_t, action->actions_before, lpc,

		gboolean other_changed = FALSE;
		do_crm_log(log_level, "   Checking action %s: %s 0x%.6x",
			   other->action->uuid,
			   other->action->optional?"optional":"required",
			   other->type);

		local_type = other->type;
/* 		local_type |= pe_order_optional; */
/* 		local_type ^= pe_order_optional; */
		
		if((local_type & pe_order_runnable_left)
			&& other->action->runnable == FALSE) {
			if(other->action->pseudo) {
				do_crm_log(log_level, "Ignoring un-runnable - pseudo");

			} else if(action->runnable == FALSE) {
				do_crm_log(log_level+1, "Already un-runnable");
				
			} else {
				action->runnable = FALSE;
				do_crm_log(log_level-1,
					   "   * Marking action %s un-runnable because of %s",
					   action->uuid, other->action->uuid);
				changed = TRUE;
			}
		}

		if((local_type & pe_order_runnable_right)
			&& action->runnable == FALSE) {
			if(action->pseudo) {
				do_crm_log(log_level, "Ignoring un-runnable - pseudo");

			} else if(other->action->runnable == FALSE) {
				do_crm_log(log_level+1, "Already un-runnable");
				
			} else {
				other->action->runnable = FALSE;
				do_crm_log(log_level-1,
					   "   * Marking action %s un-runnable because of %s",
					   other->action->uuid, action->uuid);
				other_changed = TRUE;
			}
		}
		
		if(other->type & pe_order_implies_left) {
			if(other->action->optional == FALSE) {
				/* nothing to do */
				do_crm_log(log_level+1, "      Ignoring implies left - redundant");
				
			} else if(safe_str_eq(other->action->task, CRMD_ACTION_STOP)
				  && other->action->rsc->fns->state(
					  other->action->rsc, TRUE) == RSC_ROLE_STOPPED) {
				do_crm_log(log_level-1, "      Ignoring implies left - %s already stopped",
					other->action->rsc->id);

			} else if(action->optional == FALSE) {
				other->action->optional = FALSE;
				do_crm_log(log_level-1,
					   "   * (implies left) Marking action %s mandatory because of %s",
					   other->action->uuid, action->uuid);
				other_changed = TRUE;
				
			} else {
				do_crm_log(log_level, "      Ignoring implies left");
			}
		}
		
		if(other->type & pe_order_implies_right) {
			if(action->optional == FALSE) {
				/* nothing to do */
				do_crm_log(log_level+1, "      Ignoring implies right - redundant");
			} else if(other->action->optional == FALSE) {
				action->optional = FALSE;
				do_crm_log(log_level-1,
					   "   * (implies right) Marking action %s mandatory because of %s",
					   action->uuid, other->action->uuid);
				changed = TRUE;
				
			} else {
				do_crm_log(log_level, "      Ignoring implies right");
			}
		}

		if(other_changed) {
			crm_info("%s changed, processing after list", other->action->uuid);
			update_action(other->action);
			slist_iter(
				before_other, action_wrapper_t, other->action->actions_after, lpc2,
				crm_info("%s changed, processing %s", other->action->uuid, before_other->action->uuid);
				update_action(before_other->action);
				);
		}
		
		);

	if(changed) {
		crm_info("%s changed, processing after list", action->uuid);
		slist_iter(
			other, action_wrapper_t, action->actions_after, lpc,
			crm_info("%s changed, processing %s", action->uuid, other->action->uuid);
			update_action(other->action);
			);
	}
	
	return FALSE;
}


gboolean
shutdown_constraints(
	node_t *node, action_t *shutdown_op, pe_working_set_t *data_set)
{
	/* add the stop to the before lists so it counts as a pre-req
	 * for the shutdown
	 */
	slist_iter(
		rsc, resource_t, node->details->running_rsc, lpc,

		if(rsc->is_managed == FALSE) {
			continue;
		}
		
		custom_action_order(
			rsc, stop_key(rsc), NULL,
			NULL, crm_strdup(CRM_OP_SHUTDOWN), shutdown_op,
			pe_order_implies_left, data_set);

		);	

	return TRUE;	
}

gboolean
stonith_constraints(
	node_t *node, action_t *stonith_op, pe_working_set_t *data_set)
{
	CRM_CHECK(stonith_op != NULL, return FALSE);
	
	/*
	 * Make sure the stonith OP occurs before we start any shared resources
	 */
	if(stonith_op != NULL) {
		slist_iter(
			rsc, resource_t, data_set->resources, lpc,
			rsc->cmds->stonith_ordering(rsc, stonith_op, data_set);
			);
	}
	
	/* add the stonith OP as a stop pre-req and the mark the stop
	 * as a pseudo op - since its now redundant
	 */
	
	return TRUE;
}

static void dup_attr(gpointer key, gpointer value, gpointer user_data)
{
	g_hash_table_replace(user_data, crm_strdup(key), crm_strdup(value));
}

crm_data_t *
action2xml(action_t *action, gboolean as_input)
{
	gboolean needs_node_info = TRUE;
	crm_data_t * action_xml = NULL;
	crm_data_t * args_xml = NULL;
	char *action_id_s = NULL;
	
	if(action == NULL) {
		return NULL;
	}

	crm_debug_4("Dumping action %d as XML", action->id);
	if(safe_str_eq(action->task, CRM_OP_FENCE)) {
		action_xml = create_xml_node(NULL, XML_GRAPH_TAG_CRM_EVENT);
/* 		needs_node_info = FALSE; */
		
	} else if(safe_str_eq(action->task, CRM_OP_SHUTDOWN)) {
		action_xml = create_xml_node(NULL, XML_GRAPH_TAG_CRM_EVENT);

	} else if(safe_str_eq(action->task, CRM_OP_LRM_REFRESH)) {
		action_xml = create_xml_node(NULL, XML_GRAPH_TAG_CRM_EVENT);

/* 	} else if(safe_str_eq(action->task, CRMD_ACTION_PROBED)) { */
/* 		action_xml = create_xml_node(NULL, XML_GRAPH_TAG_CRM_EVENT); */

	} else if(action->pseudo) {
		action_xml = create_xml_node(NULL, XML_GRAPH_TAG_PSEUDO_EVENT);
		needs_node_info = FALSE;

	} else {
		action_xml = create_xml_node(NULL, XML_GRAPH_TAG_RSC_OP);
	}

	action_id_s = crm_itoa(action->id);
	crm_xml_add(action_xml, XML_ATTR_ID, action_id_s);
	crm_free(action_id_s);
	
	crm_xml_add(action_xml, XML_LRM_ATTR_TASK, action->task);
	if(action->rsc != NULL && action->rsc->clone_name != NULL) {
		char *clone_key = NULL;
		const char *interval_s = g_hash_table_lookup(action->meta, "interval");
		int interval = crm_parse_int(interval_s, "0");

		if(safe_str_eq(action->task, CRMD_ACTION_NOTIFY)) {			
			const char *n_type = g_hash_table_lookup(
				action->extra, crm_meta_name("notify_type"));
			const char *n_task = g_hash_table_lookup(
				action->extra, crm_meta_name("notify_operation"));
			CRM_CHECK(n_type != NULL, ;);
			CRM_CHECK(n_task != NULL, ;);
			clone_key = generate_notify_key(action->rsc->clone_name, n_type, n_task);
			
		} else {
			clone_key = generate_op_key(action->rsc->clone_name, action->task, interval);
		}
		
		crm_xml_add(action_xml, XML_LRM_ATTR_TASK_KEY, clone_key);
		crm_xml_add(action_xml, "internal_"XML_LRM_ATTR_TASK_KEY, action->uuid);
		crm_free(clone_key);
		
	} else {
		crm_xml_add(action_xml, XML_LRM_ATTR_TASK_KEY, action->uuid);
	}
	
	if(needs_node_info && action->node != NULL) {
		crm_xml_add(action_xml, XML_LRM_ATTR_TARGET,
			    action->node->details->uname);

		crm_xml_add(action_xml, XML_LRM_ATTR_TARGET_UUID,
			    action->node->details->id);		
	}

	if(action->failure_is_fatal == FALSE) {
		add_hash_param(action->meta,
			       XML_ATTR_TE_ALLOWFAIL, XML_BOOLEAN_TRUE);
	}
	
	if(as_input) {
		return action_xml;
	}

	if(action->notify_keys != NULL) {
		g_hash_table_foreach(
			action->notify_keys, dup_attr, action->meta);
	}
	if(action->rsc != NULL && action->pseudo == FALSE) {
		int lpc = 0;
		
		crm_data_t *rsc_xml = create_xml_node(
			action_xml, crm_element_name(action->rsc->xml));

		const char *attr_list[] = {
			XML_AGENT_ATTR_CLASS,
			XML_AGENT_ATTR_PROVIDER,
			XML_ATTR_TYPE
		};

		if(action->rsc->clone_name != NULL) {
			crm_debug("Using clone name %s for %s", action->rsc->clone_name, action->rsc->id);
			crm_xml_add(rsc_xml, XML_ATTR_ID, action->rsc->clone_name);
			crm_xml_add(rsc_xml, XML_ATTR_ID_LONG, action->rsc->id);

		} else {
			crm_xml_add(rsc_xml, XML_ATTR_ID, action->rsc->id);
			crm_xml_add(rsc_xml, XML_ATTR_ID_LONG, action->rsc->long_name);
		}
		
		for(lpc = 0; lpc < DIMOF(attr_list); lpc++) {
			crm_xml_add(rsc_xml, attr_list[lpc],
				    g_hash_table_lookup(action->rsc->meta, attr_list[lpc]));
		}
	}

	args_xml = create_xml_node(action_xml, XML_TAG_ATTRS);
	crm_xml_add(args_xml, XML_ATTR_CRM_VERSION, CRM_FEATURE_SET);

	g_hash_table_foreach(action->extra, hash2field, args_xml);
	if(action->rsc != NULL && safe_str_neq(action->task, CRMD_ACTION_STOP)) {
		g_hash_table_foreach(action->rsc->parameters, hash2field, args_xml);
	}

	g_hash_table_foreach(action->meta, hash2metafield, args_xml);
	if(action->rsc != NULL) {
		int lpc = 0;
		const char *key = NULL;
		const char *value = NULL;
		const char *meta_list[] = {
			XML_RSC_ATTR_UNIQUE,
			XML_RSC_ATTR_INCARNATION,
			XML_RSC_ATTR_INCARNATION_MAX,
			XML_RSC_ATTR_INCARNATION_NODEMAX,
			XML_RSC_ATTR_MASTER_MAX,
			XML_RSC_ATTR_MASTER_NODEMAX,
		};
		
		for(lpc = 0; lpc < DIMOF(meta_list); lpc++) {
			key = meta_list[lpc];
			value = g_hash_table_lookup(action->rsc->meta, key);
			if(value != NULL) {
				char *crm_name = crm_concat(CRM_META, key, '_');
				crm_xml_add(args_xml, crm_name, value);
				crm_free(crm_name);
			}
		}
	}
	
	crm_log_xml_debug_4(action_xml, "dumped action");
	
	return action_xml;
}

static gboolean
should_dump_action(action_t *action) 
{
	const char * interval = NULL;

	CRM_CHECK(action != NULL, return FALSE);

	interval = g_hash_table_lookup(action->meta, XML_LRM_ATTR_INTERVAL);
	if(action->optional) {
		crm_debug_5("action %d (%s) was optional",
			    action->id, action->uuid);
		return FALSE;

	} else if(action->runnable == FALSE) {
		crm_debug_5("action %d (%s) was not runnable",
			    action->id, action->uuid);
		return FALSE;

	} else if(action->dumped) {
		crm_debug_5("action %d (%s) was already dumped",
			    action->id, action->uuid);
		return FALSE;

	} else if(action->rsc != NULL
		  && action->rsc->is_managed == FALSE) {

		/* make sure probes go through */
		if(safe_str_neq(action->task, CRMD_ACTION_STATUS)) {
			pe_warn("action %d (%s) was for an unmanaged resource (%s)",
				action->id, action->uuid, action->rsc->id);
			return FALSE;
		}
		
		if(interval != NULL && safe_str_neq(interval, "0")) {
			pe_warn("action %d (%s) was for an unmanaged resource (%s)",
				action->id, action->uuid, action->rsc->id);
			return FALSE;
		}
	}
	
	if(action->pseudo
	   || safe_str_eq(action->task,  CRM_OP_FENCE)
	   || safe_str_eq(action->task,  CRM_OP_SHUTDOWN)) {
		/* skip the next checks */
		crm_info("Printing %s", action->uuid);
		return TRUE;
	}

	if(action->node == NULL) {
		pe_err("action %d (%s) was not allocated",
		       action->id, action->uuid);
		log_action(LOG_DEBUG, "Unallocated action", action, FALSE);
		return FALSE;
		
	} else if(action->node->details->online == FALSE) {
		pe_err("action %d was (%s) scheduled for offline node",
		       action->id, action->uuid);
		log_action(LOG_DEBUG, "Action for offline node", action, FALSE);
		return FALSE;
#if 0
		/* but this would also affect resources that can be safely
		 *  migrated before a fencing op
		 */
	} else if(action->node->details->unclean == FALSE) {
		pe_err("action %d was (%s) scheduled for unclean node",
		       action->id, action->uuid);
		log_action(LOG_DEBUG, "Action for unclean node", action, FALSE);
		return FALSE;
#endif
	}
	return TRUE;
}

/* lowest to highest */
static gint sort_action_id(gconstpointer a, gconstpointer b)
{
	const action_wrapper_t *action_wrapper2 = (const action_wrapper_t*)a;
	const action_wrapper_t *action_wrapper1 = (const action_wrapper_t*)b;

	if(a == NULL) { return 1; }
	if(b == NULL) { return -1; }
  
	if(action_wrapper1->action->id > action_wrapper2->action->id) {
		return -1;
	}
	
	if(action_wrapper1->action->id < action_wrapper2->action->id) {
		return 1;
	}
	return 0;
}

void
graph_element_from_action(action_t *action, pe_working_set_t *data_set)
{
	int last_action = -1;
	int synapse_priority = 0;
	crm_data_t * syn = NULL;
	crm_data_t * set = NULL;
	crm_data_t * in  = NULL;
	crm_data_t * input = NULL;
	crm_data_t * xml_action = NULL;

	if(should_dump_action(action) == FALSE) {
		return;
	}
	
	action->dumped = TRUE;
	
	syn = create_xml_node(data_set->graph, "synapse");
	set = create_xml_node(syn, "action_set");
	in  = create_xml_node(syn, "inputs");

	crm_xml_add_int(syn, XML_ATTR_ID, data_set->num_synapse);
	data_set->num_synapse++;

	if(action->rsc != NULL) {
		synapse_priority = action->rsc->priority;
	}
	if(action->priority > synapse_priority) {
		synapse_priority = action->priority;
	}
	if(synapse_priority > 0) {
		crm_xml_add_int(syn, XML_CIB_ATTR_PRIORITY, synapse_priority);
	}
	
	xml_action = action2xml(action, FALSE);
	add_node_copy(set, xml_action);
	free_xml(xml_action);

	action->actions_before = g_list_sort(
		action->actions_before, sort_action_id);
	
	slist_iter(wrapper,action_wrapper_t,action->actions_before,lpc,

		   if(last_action == wrapper->action->id) {
			   crm_debug_2("Input (%d) %s duplicated",
				       wrapper->action->id,
				       wrapper->action->uuid);
			   continue;
			   
		   } else if(wrapper->action->optional == TRUE) {
			   crm_debug_2("Input (%d) %s optional",
				       wrapper->action->id,
				       wrapper->action->uuid);
			   continue;

		   } else if(wrapper->action->runnable == FALSE
			     && wrapper->type == pe_order_optional) {
			   crm_debug("Input (%d) %s optional (ordering)",
				     wrapper->action->id,
				     wrapper->action->uuid);
			   continue;
		   }

		   CRM_CHECK(last_action < wrapper->action->id, ;);
		   last_action = wrapper->action->id;
		   input = create_xml_node(in, "trigger");
		   
		   xml_action = action2xml(wrapper->action, TRUE);
		   add_node_copy(input, xml_action);
		   free_xml(xml_action);
		   
		);
}

