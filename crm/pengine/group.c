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

#include <pengine.h>
#include <lib/crm/pengine/utils.h>
#include <crm/msg_xml.h>
#include <clplumbing/cl_misc.h>
#include <allocate.h>
#include <utils.h>

#define VARIANT_GROUP 1
#include <lib/crm/pengine/variant.h>

void group_set_cmds(resource_t *rsc)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);
	group_data->self->cmds = &resource_class_alloc_functions[group_data->self->variant];
	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		child_rsc->cmds = &resource_class_alloc_functions[child_rsc->variant];
		child_rsc->cmds->set_cmds(child_rsc);
		);
}

int group_num_allowed_nodes(resource_t *rsc)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);
	if(group_data->colocated == FALSE) {
		crm_config_err("Cannot clone non-colocated group: %s", rsc->id);
		return 0;
	}
 	return group_data->self->cmds->num_allowed_nodes(group_data->self);
}

node_t *
group_color(resource_t *rsc, pe_working_set_t *data_set)
{
	resource_t *child = NULL;
	node_t *group_node = NULL;
	GListPtr child_iter = NULL;
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	if(rsc->provisional == FALSE) {
		return rsc->allocated_to;
	}
	/* combine the child weights */
	crm_debug("Processing %s", rsc->id);
	if(rsc->is_allocating) {
		crm_debug("Dependancy loop detected involving %s", rsc->id);
		return NULL;
	}
	rsc->is_allocating = TRUE;
	group_data->self->role = group_data->first_child->role;
	
	group_data->first_child->rsc_cons = g_list_concat(
		group_data->first_child->rsc_cons, rsc->rsc_cons);
	rsc->rsc_cons = NULL;

	/* process in reverse so that all scores are merged before allocation */
	child_iter = g_list_last(group_data->child_list);
	for(; child_iter != NULL; ) {
		child = child_iter->data;
		child_iter = g_list_previous(child_iter);
		group_node = child->cmds->color(child, data_set);
	}

	group_data->self->next_role = group_data->first_child->next_role;	
	rsc->is_allocating = FALSE;
	rsc->provisional = FALSE;

	if(group_data->colocated) {
		return group_node;
	} 
	return NULL;
}

void group_update_pseudo_status(resource_t *parent, resource_t *child);

void group_create_actions(resource_t *rsc, pe_working_set_t *data_set)
{
	action_t *op = NULL;
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug_2("Creating actions for %s", rsc->id);
	
	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		child_rsc->cmds->create_actions(child_rsc, data_set);
		group_update_pseudo_status(rsc, child_rsc);
		);

	op = start_action(group_data->self, NULL, !group_data->child_starting);
	op->pseudo = TRUE;
	op->runnable = TRUE;

	op = custom_action(group_data->self, started_key(group_data->self),
			   CRMD_ACTION_STARTED, NULL,
			   !group_data->child_starting, TRUE, data_set);
	op->pseudo = TRUE;
	op->runnable = TRUE;

	op = stop_action(group_data->self, NULL, !group_data->child_stopping);
	op->pseudo = TRUE;
	op->runnable = TRUE;
	
	op = custom_action(group_data->self, stopped_key(group_data->self),
			   CRMD_ACTION_STOPPED, NULL,
			   !group_data->child_stopping, TRUE, data_set);
	op->pseudo = TRUE;
	op->runnable = TRUE;

	rsc->actions = group_data->self->actions;
/* 	group_data->self->actions = NULL; */
}

void
group_update_pseudo_status(resource_t *parent, resource_t *child) 
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, parent);

	if(group_data->child_stopping && group_data->child_starting) {
		return;
	}
	slist_iter(
		action, action_t, child->actions, lpc,

		if(action->optional) {
			continue;
		}
		if(safe_str_eq(CRMD_ACTION_STOP, action->task) && action->runnable) {
			group_data->child_stopping = TRUE;
			crm_debug_3("Based on %s the group is stopping", action->uuid);

		} else if(safe_str_eq(CRMD_ACTION_START, action->task) && action->runnable) {
			group_data->child_starting = TRUE;
			crm_debug_3("Based on %s the group is starting", action->uuid);
		}
		
		);
}

void group_internal_constraints(resource_t *rsc, pe_working_set_t *data_set)
{
	resource_t *this_rsc = NULL;
	resource_t *last_rsc = NULL;

	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	this_rsc = group_data->self;
	group_data->self->cmds->internal_constraints(group_data->self, data_set);
	
	custom_action_order(
		group_data->self, stopped_key(group_data->self), NULL,
		group_data->self, start_key(group_data->self), NULL,
		pe_order_optional, data_set);

	custom_action_order(
		group_data->self, stop_key(group_data->self), NULL,
		group_data->self, stopped_key(group_data->self), NULL,
		pe_order_runnable_left, data_set);

	custom_action_order(
		group_data->self, start_key(group_data->self), NULL,
		group_data->self, started_key(group_data->self), NULL,
		pe_order_runnable_left, data_set);
	
	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,

		child_rsc->cmds->internal_constraints(child_rsc, data_set);

		if(group_data->colocated && last_rsc != NULL) {
			rsc_colocation_new(
				"group:internal_colocation", NULL, INFINITY,
				child_rsc, last_rsc, NULL, NULL);
		}

		custom_action_order(child_rsc, stop_key(child_rsc), NULL,
				    this_rsc,  stopped_key(this_rsc), NULL,
				    pe_order_optional, data_set);

		custom_action_order(child_rsc, start_key(child_rsc), NULL,
				    this_rsc, started_key(this_rsc), NULL,
				    pe_order_optional, data_set);
		
 		if(group_data->ordered == FALSE) {
			order_start_start(this_rsc, child_rsc, pe_order_implies_right);
			order_stop_stop(this_rsc, child_rsc, pe_order_implies_right);

		} else if(last_rsc != NULL) {
			order_start_start(last_rsc, child_rsc, pe_order_implies_right|pe_order_runnable_left);
			order_stop_stop(child_rsc, last_rsc, pe_order_implies_left);

			child_rsc->restart_type = pe_restart_restart;

		} else {
			/* If anyone in the group is starting, then
			 *  pe_order_implies_right will cause _everyone_ in the group
			 *  to be sent a start action
			 * But this is safe since starting something that is already
			 *  started is required to be "safe"
			 */
			order_start_start(this_rsc, child_rsc, pe_order_implies_right|pe_order_runnable_right);
		}
		
		last_rsc = child_rsc;
		);

	if(group_data->ordered && last_rsc != NULL) {
		order_stop_stop(this_rsc, last_rsc, pe_order_implies_right|pe_order_test);
	}		
}


void group_rsc_colocation_lh(
	resource_t *rsc_lh, resource_t *rsc_rh, rsc_colocation_t *constraint)
{
	group_variant_data_t *group_data = NULL;
	
	if(rsc_lh == NULL) {
		pe_err("rsc_lh was NULL for %s", constraint->id);
		return;

	} else if(rsc_rh == NULL) {
		pe_err("rsc_rh was NULL for %s", constraint->id);
		return;
	}
		
	crm_debug_4("Processing constraints from %s", rsc_lh->id);

	get_group_variant_data(group_data, rsc_lh);

	if(group_data->colocated) {
		group_data->first_child->cmds->rsc_colocation_lh(
			group_data->first_child, rsc_rh, constraint); 
		return;

	} else if(constraint->score >= INFINITY) {
		crm_config_err("%s: Cannot perform manditory colocation"
			       " between non-colocated group and %s",
			       rsc_lh->id, rsc_rh->id);
		return;
	} 

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		child_rsc->cmds->rsc_colocation_lh(
			child_rsc, rsc_rh, constraint); 
		);
}

void group_rsc_colocation_rh(
	resource_t *rsc_lh, resource_t *rsc_rh, rsc_colocation_t *constraint)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc_rh);
	CRM_CHECK(rsc_lh->variant == pe_native, return);

	crm_debug_3("Processing RH of constraint %s", constraint->id);
	print_resource(LOG_DEBUG_3, "LHS", rsc_lh, TRUE);

	if(rsc_rh->provisional) {
		return;
	
	} else if(group_data->colocated) {
		group_data->first_child->cmds->rsc_colocation_rh(
			rsc_lh, group_data->first_child, constraint); 
		return;

	} else if(constraint->score >= INFINITY) {
		crm_config_err("%s: Cannot perform manditory colocation with"
			       " non-colocated group: %s", rsc_lh->id, rsc_rh->id);
		return;
	} 

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		child_rsc->cmds->rsc_colocation_rh(
			rsc_lh, child_rsc, constraint); 
		);
}

void group_rsc_order_lh(resource_t *rsc, order_constraint_t *order, pe_working_set_t *data_set)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug("%s->%s", order->lh_action_task, order->rh_action_task);

	if(group_data->self == NULL) {
		return;
	}

	group_data->self->cmds->rsc_order_lh(group_data->self, order, data_set);
	convert_non_atomic_task(rsc, order);
	group_data->self->cmds->rsc_order_lh(group_data->self, order, data_set);
}

void group_rsc_order_rh(
	action_t *lh_action, resource_t *rsc, order_constraint_t *order)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug_2("%s->%s", lh_action->uuid, order->rh_action_task);

	if(group_data->self == NULL) {
		return;
	}

	group_data->self->cmds->rsc_order_rh(lh_action, group_data->self, order);
}

void group_rsc_location(resource_t *rsc, rsc_to_node_t *constraint)
{
	gboolean reset_scores = TRUE;
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug("Processing rsc_location %s for %s",
		  constraint->id, group_data->self->id);

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		child_rsc->cmds->rsc_location(child_rsc, constraint);
		if(group_data->colocated && reset_scores) {
			reset_scores = FALSE;
			slist_iter(node, node_t, constraint->node_list_rh, lpc2,
				   node->weight = 0;
				);
		}
		);
}


void group_expand(resource_t *rsc, pe_working_set_t *data_set)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug_3("Processing actions from %s", rsc->id);

	CRM_CHECK(group_data->self != NULL, return);
	group_data->self->cmds->expand(group_data->self, data_set);
	
	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,

		child_rsc->cmds->expand(child_rsc, data_set);
		);

}

void
group_agent_constraints(resource_t *rsc)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		
		child_rsc->cmds->agent_constraints(child_rsc);
		);
}

void
group_create_notify_element(resource_t *rsc, action_t *op,
			    notify_data_t *n_data, pe_working_set_t *data_set)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		
		child_rsc->cmds->create_notify_element(
			child_rsc, op, n_data, data_set);
		);
}

gboolean
group_create_probe(resource_t *rsc, node_t *node, action_t *complete,
		    gboolean force, pe_working_set_t *data_set) 
{
	gboolean any_created = FALSE;
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		
		any_created = child_rsc->cmds->create_probe(
			child_rsc, node, complete, force, data_set) || any_created;
		);
	return any_created;
}

void
group_stonith_ordering(
	resource_t *rsc,  action_t *stonith_op, pe_working_set_t *data_set)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		
		child_rsc->cmds->stonith_ordering(
			child_rsc, stonith_op, data_set);
		);
}

void
group_migrate_reload(resource_t *rsc, pe_working_set_t *data_set)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	slist_iter(
		child_rsc, resource_t, group_data->child_list, lpc,
		
		child_rsc->cmds->migrate_reload(child_rsc, data_set);
		);
}
