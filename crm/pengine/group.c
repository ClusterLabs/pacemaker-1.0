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

node_t *
group_color(resource_t *rsc, pe_working_set_t *data_set)
{
	node_t *node = NULL;
	node_t *group_node = NULL;
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	if(is_not_set(rsc->flags, pe_rsc_provisional)) {
		return rsc->allocated_to;
	}
	crm_debug_2("Processing %s", rsc->id);
	if(is_set(rsc->flags, pe_rsc_allocating)) {
		crm_debug("Dependancy loop detected involving %s", rsc->id);
		return NULL;
	}
	
	if(group_data->first_child == NULL) {
	    /* nothign to allocate */
	    clear_bit(rsc->flags, pe_rsc_provisional);
	    return NULL;
	}
	
	set_bit(rsc->flags, pe_rsc_allocating);
	rsc->role = group_data->first_child->role;
	
	group_data->first_child->rsc_cons = g_list_concat(
		group_data->first_child->rsc_cons, rsc->rsc_cons);
	rsc->rsc_cons = NULL;

	slist_iter(
		child_rsc, resource_t, rsc->children, lpc,
		node = child_rsc->cmds->color(child_rsc, data_set);
		if(group_node == NULL) {
		    group_node = node;
		}
		);

	rsc->next_role = group_data->first_child->next_role;	
	clear_bit(rsc->flags, pe_rsc_allocating);
	clear_bit(rsc->flags, pe_rsc_provisional);

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
		child_rsc, resource_t, rsc->children, lpc,
		child_rsc->cmds->create_actions(child_rsc, data_set);
		group_update_pseudo_status(rsc, child_rsc);
		);

	op = start_action(rsc, NULL, !group_data->child_starting);
	op->pseudo = TRUE;
	op->runnable = TRUE;

	op = custom_action(rsc, started_key(rsc),
			   CRMD_ACTION_STARTED, NULL,
			   !group_data->child_starting, TRUE, data_set);
	op->pseudo = TRUE;
	op->runnable = TRUE;

	op = stop_action(rsc, NULL, !group_data->child_stopping);
	op->pseudo = TRUE;
	op->runnable = TRUE;
	
	op = custom_action(rsc, stopped_key(rsc),
			   CRMD_ACTION_STOPPED, NULL,
			   !group_data->child_stopping, TRUE, data_set);
	op->pseudo = TRUE;
	op->runnable = TRUE;

	rsc->actions = rsc->actions;
/* 	rsc->actions = NULL; */
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
	resource_t *last_rsc = NULL;
	int stopstop = pe_order_shutdown;
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	native_internal_constraints(rsc, data_set);

	if(group_data->ordered == FALSE) {
	    stopstop |= pe_order_implies_right;
	}
	
	custom_action_order(
		rsc, stopped_key(rsc), NULL,
		rsc, start_key(rsc), NULL,
		pe_order_optional, data_set);

	custom_action_order(
		rsc, stop_key(rsc), NULL,
		rsc, stopped_key(rsc), NULL,
		pe_order_runnable_left, data_set);

	custom_action_order(
		rsc, start_key(rsc), NULL,
		rsc, started_key(rsc), NULL,
		pe_order_runnable_left, data_set);
	
	slist_iter(
		child_rsc, resource_t, rsc->children, lpc,

		child_rsc->cmds->internal_constraints(child_rsc, data_set);

		if(group_data->colocated && last_rsc != NULL) {
			rsc_colocation_new(
				"group:internal_colocation", NULL, INFINITY,
				child_rsc, last_rsc, NULL, NULL, data_set);
		}

		order_stop_stop(rsc, child_rsc, stopstop);
		
		custom_action_order(child_rsc, stop_key(child_rsc), NULL,
				    rsc,  stopped_key(rsc), NULL,
				    pe_order_optional, data_set);

		custom_action_order(child_rsc, start_key(child_rsc), NULL,
				    rsc, started_key(rsc), NULL,
				    pe_order_runnable_left, data_set);
		
 		if(group_data->ordered == FALSE) {
			order_start_start(rsc, child_rsc, pe_order_implies_right|pe_order_runnable_left);

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
			order_start_start(rsc, child_rsc,
					  pe_order_implies_right|pe_order_implies_left|pe_order_runnable_right|pe_order_runnable_left);
		}
		
		last_rsc = child_rsc;
		);

	if(group_data->ordered && last_rsc != NULL) {
		order_stop_stop(rsc, last_rsc, pe_order_implies_right);
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
		child_rsc, resource_t, rsc_lh->children, lpc,
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

	if(is_set(rsc_rh->flags, pe_rsc_provisional)) {
		return;
	
	} else if(group_data->colocated && group_data->first_child) {
		group_data->first_child->cmds->rsc_colocation_rh(
			rsc_lh, group_data->first_child, constraint); 
		return;

	} else if(constraint->score >= INFINITY) {
		crm_config_err("%s: Cannot perform manditory colocation with"
			       " non-colocated group: %s", rsc_lh->id, rsc_rh->id);
		return;
	} 

	slist_iter(
		child_rsc, resource_t, rsc_rh->children, lpc,
		child_rsc->cmds->rsc_colocation_rh(
			rsc_lh, child_rsc, constraint); 
		);
}

void group_rsc_order_lh(resource_t *rsc, order_constraint_t *order, pe_working_set_t *data_set)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug_2("%s->%s", order->lh_action_task, order->rh_action_task);

	if(order->rh_rsc != NULL
	   && (rsc == order->rh_rsc || rsc == order->rh_rsc->parent)) {
		native_rsc_order_lh(rsc, order, data_set);
		return;
	}

	if(order->type != pe_order_optional) {
		native_rsc_order_lh(rsc, order, data_set);
	}

	if(order->type & pe_order_implies_left) {
 		native_rsc_order_lh(group_data->first_child, order, data_set);
	}

	convert_non_atomic_task(rsc, order);
	native_rsc_order_lh(rsc, order, data_set);
}

void group_rsc_order_rh(
	action_t *lh_action, resource_t *rsc, order_constraint_t *order)
{
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug_2("%s->%s", lh_action->uuid, order->rh_action_task);

	if(rsc == NULL) {
		return;
	}

	native_rsc_order_rh(lh_action, rsc, order);
}

void group_rsc_location(resource_t *rsc, rsc_to_node_t *constraint)
{
	gboolean reset_scores = TRUE;
	group_variant_data_t *group_data = NULL;
	get_group_variant_data(group_data, rsc);

	crm_debug("Processing rsc_location %s for %s",
		  constraint->id, rsc->id);

	slist_iter(
		child_rsc, resource_t, rsc->children, lpc,
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

	CRM_CHECK(rsc != NULL, return);
	native_expand(rsc, data_set);
	
	slist_iter(
		child_rsc, resource_t, rsc->children, lpc,

		child_rsc->cmds->expand(child_rsc, data_set);
		);

}

GListPtr
group_merge_weights(
    resource_t *rsc, const char *rhs, GListPtr nodes, int factor, gboolean allow_rollback) 
{
    group_variant_data_t *group_data = NULL;
    get_group_variant_data(group_data, rsc);
    
    if(is_set(rsc->flags, pe_rsc_merging)) {
	crm_debug("Breaking dependancy loop with %s at %s", rsc->id, rhs);
	return nodes;

    } else if(is_not_set(rsc->flags, pe_rsc_provisional) || can_run_any(nodes) == FALSE) {
	return nodes;
    }

    set_bit(rsc->flags, pe_rsc_merging);
    nodes = group_data->first_child->cmds->merge_weights(
	group_data->first_child, rhs, nodes, factor, allow_rollback);

    slist_iter(
	constraint, rsc_colocation_t, rsc->rsc_cons_lhs, lpc,
	
	nodes = native_merge_weights(
	    constraint->rsc_lh, rsc->id, nodes,
	    constraint->score/INFINITY, allow_rollback);
	);

    clear_bit(rsc->flags, pe_rsc_merging);
    return nodes;
}
