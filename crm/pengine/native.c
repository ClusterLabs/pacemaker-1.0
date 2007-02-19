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

#include <pengine.h>
#include <crm/pengine/rules.h>
#include <lib/crm/pengine/utils.h>
#include <crm/msg_xml.h>
#include <allocate.h>
#include <utils.h>

#define DELETE_THEN_REFRESH 1

#define VARIANT_NATIVE 1
#include <lib/crm/pengine/variant.h>

resource_t *ultimate_parent(resource_t *rsc);

void node_list_update(GListPtr list1, GListPtr list2, int factor);

void native_rsc_colocation_rh_must(resource_t *rsc_lh, gboolean update_lh,
				   resource_t *rsc_rh, gboolean update_rh);

void native_rsc_colocation_rh_mustnot(resource_t *rsc_lh, gboolean update_lh,
				      resource_t *rsc_rh, gboolean update_rh);

void create_notifications(resource_t *rsc, pe_working_set_t *data_set);
void Recurring(resource_t *rsc, action_t *start, node_t *node,
			      pe_working_set_t *data_set);
void RecurringOp(resource_t *rsc, action_t *start, node_t *node,
		 crm_data_t *operation, pe_working_set_t *data_set);
void pe_pre_notify(
	resource_t *rsc, node_t *node, action_t *op, 
	notify_data_t *n_data, pe_working_set_t *data_set);
void pe_post_notify(
	resource_t *rsc, node_t *node, action_t *op, 
	notify_data_t *n_data, pe_working_set_t *data_set);

gboolean DeleteRsc(resource_t *rsc, node_t *node, pe_working_set_t *data_set);
void NoRoleChange(resource_t *rsc, node_t *current, node_t *next, pe_working_set_t *data_set);
gboolean StopRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set);
gboolean StartRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set);
extern gboolean DemoteRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set);
gboolean PromoteRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set);
gboolean RoleError(resource_t *rsc, node_t *next, pe_working_set_t *data_set);
gboolean NullOp(resource_t *rsc, node_t *next, pe_working_set_t *data_set);

enum rsc_role_e rsc_state_matrix[RSC_ROLE_MAX][RSC_ROLE_MAX] = {
/* Current State */	
/*    Next State:  Unknown 	    Stopped	      Started	        Slave	          Master */
/* Unknown */	{ RSC_ROLE_UNKNOWN, RSC_ROLE_STOPPED, RSC_ROLE_STOPPED, RSC_ROLE_STOPPED, RSC_ROLE_STOPPED, },
/* Stopped */	{ RSC_ROLE_STOPPED, RSC_ROLE_STOPPED, RSC_ROLE_STARTED, RSC_ROLE_SLAVE,   RSC_ROLE_SLAVE, },
/* Started */	{ RSC_ROLE_STOPPED, RSC_ROLE_STOPPED, RSC_ROLE_STARTED, RSC_ROLE_SLAVE,   RSC_ROLE_MASTER, },
/* Slave */	{ RSC_ROLE_STOPPED, RSC_ROLE_STOPPED, RSC_ROLE_UNKNOWN, RSC_ROLE_SLAVE,   RSC_ROLE_MASTER, },
/* Master */	{ RSC_ROLE_STOPPED, RSC_ROLE_SLAVE,   RSC_ROLE_UNKNOWN, RSC_ROLE_SLAVE,   RSC_ROLE_MASTER, },
};

gboolean (*rsc_action_matrix[RSC_ROLE_MAX][RSC_ROLE_MAX])(resource_t*,node_t*,pe_working_set_t*) = {
/* Current State */	
/*    Next State: Unknown	Stopped		Started		Slave		Master */
/* Unknown */	{ RoleError,	StopRsc,	RoleError,	RoleError,	RoleError,  },
/* Stopped */	{ RoleError,	NullOp,		StartRsc,	StartRsc,	RoleError,  },
/* Started */	{ RoleError,	StopRsc,	NullOp,		NullOp,	        PromoteRsc,  },
/* Slave */	{ RoleError,	StopRsc,	RoleError,	NullOp,		PromoteRsc, },
/* Master */	{ RoleError,	RoleError,	RoleError,	DemoteRsc,	NullOp,     },
};


static gboolean
native_choose_node(resource_t *rsc)
{
	/*
	  1. Sort by weight
	  2. color.chosen_node = the node (of those with the highest wieght)
				   with the fewest resources
	  3. remove color.chosen_node from all other colors
	*/
	GListPtr nodes = NULL;
	node_t *chosen = NULL;

	if(rsc->provisional == FALSE) {
		return rsc->allocated_to?TRUE:FALSE;
	}
	
	crm_debug_3("Choosing node for %s from %d candidates",
		    rsc->id, g_list_length(rsc->allowed_nodes));

	if(rsc->allowed_nodes) {
		rsc->allowed_nodes = g_list_sort(
			rsc->allowed_nodes, sort_node_weight);
		nodes = rsc->allowed_nodes;
		chosen = g_list_nth_data(nodes, 0);
	}
	
	return native_assign_node(rsc, nodes, chosen);
}

void native_set_cmds(resource_t *rsc)
{
}

int native_num_allowed_nodes(resource_t *rsc)
{
	int num_nodes = 0;

	if(rsc->next_role == RSC_ROLE_STOPPED) {
		return 0;
	}
	
	crm_debug_4("Default case");
	slist_iter(
		this_node, node_t, rsc->allowed_nodes, lpc,
		crm_debug_3("Rsc %s Checking %s: %d",
			    rsc->id, this_node->details->uname,
			    this_node->weight);
		if(this_node->details->shutdown
		   || this_node->details->online == FALSE) {
			this_node->weight = -INFINITY;
		}
		if(this_node->weight < 0) {				
			continue;
/* 			} else if(this_node->details->unclean) { */
/* 				continue; */
		}
		
		num_nodes++;
		);

	crm_debug_2("Resource %s can run on %d nodes", rsc->id, num_nodes);
	return num_nodes;
}

resource_t *
ultimate_parent(resource_t *rsc)
{
	resource_t *parent = rsc;
	while(parent->parent) {
		parent = parent->parent;
	}
	return parent;
}

node_t *
native_color(resource_t *rsc, pe_working_set_t *data_set)
{
	if(rsc->parent && rsc->parent->is_allocating == FALSE) {
		/* never allocate children on their own */
		crm_debug("Escalating allocation of %s to its parent: %s",
			  rsc->id, rsc->parent->id);
		rsc->parent->cmds->color(rsc->parent, data_set);
	}
	
	print_resource(LOG_DEBUG_2, "Allocating: ", rsc, FALSE);
	if(rsc->provisional == FALSE) {
		return rsc->allocated_to;
	}

	if(rsc->is_allocating) {
		crm_debug("Dependancy loop detected involving %s", rsc->id);
		return NULL;
	}

	rsc->is_allocating = TRUE;
	rsc->rsc_cons = g_list_sort(rsc->rsc_cons, sort_cons_strength);

	slist_iter(
		constraint, rsc_colocation_t, rsc->rsc_cons, lpc,

		crm_debug_3("%s: Pre-Processing %s", rsc->id, constraint->id);

		if(rsc->provisional && constraint->rsc_rh->provisional) {
			crm_info("Combine scores from %s and %s",
				 rsc->id, constraint->rsc_rh->id);
			node_list_update(constraint->rsc_rh->allowed_nodes,
					 rsc->allowed_nodes,
					 constraint->score/INFINITY);
		}
		
		constraint->rsc_rh->cmds->color(
			constraint->rsc_rh, data_set);
		rsc->cmds->rsc_colocation_lh(
			rsc, constraint->rsc_rh, constraint);
		
		);

	print_resource(LOG_DEBUG, "Allocating: ", rsc, FALSE);
	if(rsc->next_role == RSC_ROLE_STOPPED) {
		crm_debug_2("Making sure %s doesn't get allocated", rsc->id);
		/* make sure it doesnt come up again */
		resource_location(
			rsc, NULL, -INFINITY, "target_role", data_set);
	}
	
	if(rsc->provisional && native_choose_node(rsc) ) {
		crm_debug_3("Allocated resource %s to %s",
			    rsc->id, rsc->allocated_to->details->uname);

	} else if(rsc->allocated_to == NULL) {
		if(rsc->orphan == FALSE) {
			pe_warn("Resource %s cannot run anywhere", rsc->id);
		} else {
			crm_info("Stopping orphan resource %s", rsc->id);
		}
		
	} else {
		crm_debug("Pre-Allocated resource %s to %s",
			  rsc->id, rsc->allocated_to->details->uname);
	}
	
	rsc->is_allocating = FALSE;
	print_resource(LOG_DEBUG_3, "Allocated ", rsc, TRUE);

	return rsc->allocated_to;
}

void
RecurringOp(resource_t *rsc, action_t *start, node_t *node,
	    crm_data_t *operation, pe_working_set_t *data_set) 
{
	char *key = NULL;
	const char *name = NULL;
	const char *value = NULL;
	const char *interval = NULL;
	const char *node_uname = NULL;

	int interval_ms = 0;
	action_t *mon = NULL;
	gboolean is_optional = TRUE;
	GListPtr possible_matches = NULL;
	
	crm_debug_2("Creating recurring actions for %s", rsc->id);
	if(node != NULL) {
		node_uname = node->details->uname;
	}

	interval = crm_element_value(operation, XML_LRM_ATTR_INTERVAL);
	interval_ms = crm_get_msec(interval);
	
	if(interval_ms <= 0) {
		return;
	}
	
	value = crm_element_value(operation, "disabled");
	if(crm_is_true(value)) {
		return;
	}
	
	name = crm_element_value(operation, "name");
	key = generate_op_key(rsc->id, name, interval_ms);
	if(start != NULL) {
		crm_debug_3("Marking %s %s due to %s",
			    key, start->optional?"optional":"manditory",
			    start->uuid);
		is_optional = start->optional;
	} else {
		crm_debug_2("Marking %s optional", key);
		is_optional = TRUE;
	}
	
	/* start a monitor for an already active resource */
	possible_matches = find_actions_exact(rsc->actions, key, node);
	if(possible_matches == NULL) {
		is_optional = FALSE;
		crm_debug_3("Marking %s manditory: not active", key);
	}
	
	value = crm_element_value(operation, "role");
	if((rsc->next_role == RSC_ROLE_MASTER && value == NULL)
	   || (value != NULL && text2role(value) != rsc->next_role)) {
		int log_level = LOG_DEBUG_2;
		const char *result = "Ignoring";
		if(is_optional) {
			char *local_key = crm_strdup(key);
			log_level = LOG_INFO;
			result = "Cancelling";
			/* its running : cancel it */
			
			mon = custom_action(
				rsc, local_key, CRMD_ACTION_CANCEL, node,
				FALSE, TRUE, data_set);
			
			mon->task = CRMD_ACTION_CANCEL;
			add_hash_param(mon->meta, XML_LRM_ATTR_INTERVAL, interval);
			add_hash_param(mon->meta, XML_LRM_ATTR_TASK, name);
			
			custom_action_order(
				rsc, NULL, mon,
				rsc, promote_key(rsc), NULL,
				pe_order_optional, data_set);
			
			mon = NULL;
		}
		
		do_crm_log(log_level, "%s action %s (%s vs. %s)",
			   result , key, value?value:role2text(RSC_ROLE_SLAVE),
			   role2text(rsc->next_role));

		crm_free(key);
		key = NULL;
		return;
	}		
		
	mon = custom_action(rsc, key, name, node,
			    is_optional, TRUE, data_set);
	key = mon->uuid;
	if(is_optional) {
		crm_debug("%s\t   %s (optional)",
			  crm_str(node_uname), mon->uuid);
	}
	
	if(start == NULL || start->runnable == FALSE) {
		crm_debug("%s\t   %s (cancelled : start un-runnable)",
			  crm_str(node_uname), mon->uuid);
		mon->runnable = FALSE;
		
	} else if(node == NULL
		  || node->details->online == FALSE
		  || node->details->unclean) {
		crm_debug("%s\t   %s (cancelled : no node available)",
			  crm_str(node_uname), mon->uuid);
		mon->runnable = FALSE;
		
	} else if(mon->optional == FALSE) {
		crm_notice("%s\t   %s", crm_str(node_uname),mon->uuid);
	}
	
	custom_action_order(rsc, start_key(rsc), NULL,
			    NULL, crm_strdup(key), mon,
			    pe_order_internal_restart, data_set);
	
	if(rsc->next_role == RSC_ROLE_MASTER) {
		char *running_master = crm_itoa(EXECRA_RUNNING_MASTER);
		add_hash_param(mon->meta, XML_ATTR_TE_TARGET_RC, running_master);
		custom_action_order(
			rsc, promote_key(rsc), NULL,
			rsc, NULL, mon,
			pe_order_optional, data_set);
		crm_free(running_master);
	}		
}

void
Recurring(resource_t *rsc, action_t *start, node_t *node,
			 pe_working_set_t *data_set) 
{
	
	xml_child_iter_filter(
		rsc->ops_xml, operation, "op",
		RecurringOp(rsc, start, node, operation, data_set);		
		);	
}

void native_create_actions(resource_t *rsc, pe_working_set_t *data_set)
{
	action_t *start = NULL;
	node_t *chosen = NULL;
	enum rsc_role_e role = RSC_ROLE_UNKNOWN;
	enum rsc_role_e next_role = RSC_ROLE_UNKNOWN;

	crm_debug_2("Creating actions for %s", rsc->id);
	
	chosen = rsc->allocated_to;
	if(chosen != NULL) {
		CRM_CHECK(rsc->next_role != RSC_ROLE_UNKNOWN, rsc->next_role = RSC_ROLE_STARTED);
	}

	unpack_instance_attributes(
		rsc->xml, XML_TAG_ATTR_SETS,
		chosen?chosen->details->attrs:NULL,
		rsc->parameters, NULL, data_set->now);

	crm_debug_2("%s: %s->%s", rsc->id,
		    role2text(rsc->role), role2text(rsc->next_role));
	
	if(g_list_length(rsc->running_on) > 1) {
 		if(rsc->recovery_type == recovery_stop_start) {
			pe_proc_err("Attempting recovery of resource %s", rsc->id);
			StopRsc(rsc, NULL, data_set);
			rsc->role = RSC_ROLE_STOPPED;
		}
		
	} else if(rsc->running_on != NULL) {
		node_t *current = rsc->running_on->data;
		NoRoleChange(rsc, current, chosen, data_set);

	} else if(rsc->role == RSC_ROLE_STOPPED && rsc->next_role == RSC_ROLE_STOPPED) {
		char *key = start_key(rsc);
		GListPtr possible_matches = find_actions(rsc->actions, key, NULL);
		slist_iter(
			action, action_t, possible_matches, lpc,
			action->optional = TRUE;
/*			action->pseudo = TRUE; */
			);
		crm_debug_2("Stopping a stopped resource");
		crm_free(key);
		return;
	} 

	role = rsc->role;

	while(role != rsc->next_role) {
		next_role = rsc_state_matrix[role][rsc->next_role];
		crm_debug_2("Executing: %s->%s (%s)",
			  role2text(role), role2text(next_role), rsc->id);
		if(rsc_action_matrix[role][next_role](
			   rsc, chosen, data_set) == FALSE) {
			break;
		}
		role = next_role;
	}

	if(rsc->next_role != RSC_ROLE_STOPPED && rsc->is_managed) {
		start = start_action(rsc, chosen, TRUE);
		Recurring(rsc, start, chosen, data_set);
	}
}

void native_internal_constraints(resource_t *rsc, pe_working_set_t *data_set)
{
	order_restart(rsc);

	custom_action_order(rsc, demote_key(rsc), NULL,
			    rsc, stop_key(rsc), NULL,
			    pe_order_implies_left, data_set);

	custom_action_order(rsc, start_key(rsc), NULL,
			    rsc, promote_key(rsc), NULL,
			    pe_order_optional, data_set);

	custom_action_order(
		rsc, stop_key(rsc), NULL, rsc, delete_key(rsc), NULL, 
		pe_order_optional, data_set);

	custom_action_order(
		rsc, delete_key(rsc), NULL, rsc, start_key(rsc), NULL, 
		pe_order_implies_left, data_set);	

	if(rsc->notify) {
		char *key1 = NULL;
		char *key2 = NULL;

		key1 = generate_op_key(rsc->id, "confirmed-post_notify_start", 0);
		key2 = generate_op_key(rsc->id, "pre_notify_promote", 0);
		custom_action_order(
			rsc, key1, NULL, rsc, key2, NULL, 
			pe_order_optional, data_set);	

		key1 = generate_op_key(rsc->id, "confirmed-post_notify_demote", 0);
		key2 = generate_op_key(rsc->id, "pre_notify_stop", 0);
		custom_action_order(
			rsc, key1, NULL, rsc, key2, NULL, 
			pe_order_optional, data_set);	
	}
}

void native_rsc_colocation_lh(
	resource_t *rsc_lh, resource_t *rsc_rh, rsc_colocation_t *constraint)
{
	if(rsc_lh == NULL) {
		pe_err("rsc_lh was NULL for %s", constraint->id);
		return;

	} else if(constraint->rsc_rh == NULL) {
		pe_err("rsc_rh was NULL for %s", constraint->id);
		return;
	}
	
	crm_debug_2("Processing colocation constraint between %s and %s",
		    rsc_lh->id, rsc_rh->id);
	
	rsc_rh->cmds->rsc_colocation_rh(rsc_lh, rsc_rh, constraint);		
}

static gboolean
filter_colocation_constraint(
	resource_t *rsc_lh, resource_t *rsc_rh, rsc_colocation_t *constraint)
{
	if(constraint->score == 0){
		return FALSE;
	}

	if(constraint->role_lh != RSC_ROLE_UNKNOWN
	   && constraint->role_lh != rsc_lh->next_role) {
		crm_debug_4("RH: Skipping constraint: \"%s\" state filter",
			    role2text(constraint->role_rh));
		return FALSE;
	}
	
	if(constraint->role_rh != RSC_ROLE_UNKNOWN
	   && constraint->role_rh != rsc_rh->next_role) {
		crm_debug_4("RH: Skipping constraint: \"%s\" state filter",
			    role2text(constraint->role_rh));
		return FALSE;
	}
	return TRUE;
}

static void
colocation_match(
	resource_t *rsc_lh, resource_t *rsc_rh, rsc_colocation_t *constraint) 
{
	const char *tmp = NULL;
	const char *value = NULL;
	gboolean do_check = FALSE;
	const char *attribute = "#id";

	if(constraint->node_attribute != NULL) {
		attribute = constraint->node_attribute;
	}

	if(rsc_rh->allocated_to) {
		value = g_hash_table_lookup(
			rsc_rh->allocated_to->details->attrs, attribute);
		do_check = TRUE;
	}
	
	slist_iter(
		node, node_t, rsc_lh->allowed_nodes, lpc,
		tmp = g_hash_table_lookup(node->details->attrs, attribute);
		if(do_check && safe_str_eq(tmp, value)) {
			crm_debug_2("%s: %s.%s += %d", constraint->id, rsc_lh->id,
				  node->details->uname, constraint->score);
			node->weight = merge_weights(
				constraint->score, node->weight);

		} else if(do_check == FALSE || constraint->score >= INFINITY) {
			crm_debug_2("%s: %s.%s = -INFINITY (%s)", constraint->id, rsc_lh->id,
				  node->details->uname, do_check?"failed":"unallocated");
			node->weight = -INFINITY;
		}
		
		);
}

void native_rsc_colocation_rh(
	resource_t *rsc_lh, resource_t *rsc_rh, rsc_colocation_t *constraint)
{
	crm_debug_2("%sColocating %s with %s (%s, weight=%d)",
		    constraint->score >= 0?"":"Anti-",
		    rsc_lh->id, rsc_rh->id, constraint->id, constraint->score);
	
	if(filter_colocation_constraint(rsc_lh, rsc_rh, constraint) == FALSE) {
		return;
	}
	
	if(rsc_rh->provisional) {
		return;

	} else if(rsc_lh->provisional == FALSE) {
		/* error check */
		struct node_shared_s *details_lh;
		struct node_shared_s *details_rh;
		if((constraint->score > -INFINITY) && (constraint->score < INFINITY)) {
			return;
		}

		details_rh = rsc_rh->allocated_to?rsc_rh->allocated_to->details:NULL;
		details_lh = rsc_lh->allocated_to?rsc_lh->allocated_to->details:NULL;
		
		if(constraint->score == INFINITY && details_lh != details_rh) {
			crm_err("%s and %s are both allocated"
				" but to different nodes: %s vs. %s",
				rsc_lh->id, rsc_rh->id,
				details_lh?details_lh->uname:"n/a",
				details_rh?details_rh->uname:"n/a");

		} else if(constraint->score == -INFINITY && details_lh == details_rh) {
			crm_err("%s and %s are both allocated"
				" but to the SAME node: %s",
				rsc_lh->id, rsc_rh->id,
				details_rh?details_rh->uname:"n/a");
		}
		
		return;
		
	} else {
		colocation_match(rsc_lh, rsc_rh, constraint);
	}
}

void
node_list_update(GListPtr list1, GListPtr list2, int factor)
{
	node_t *other_node = NULL;

	slist_iter(
		node, node_t, list1, lpc,

		if(node == NULL) {
			continue;
		}

		other_node = (node_t*)pe_find_node_id(
			list2, node->details->id);

		if(other_node != NULL) {
			crm_debug_2("%s: %d + %d",
				    node->details->uname, 
				    node->weight, other_node->weight);
			node->weight = merge_weights(
				factor*other_node->weight, node->weight);
		}
		);	
}

void native_rsc_order_lh(resource_t *lh_rsc, order_constraint_t *order)
{
	GListPtr lh_actions = NULL;
	action_t *lh_action = order->lh_action;

	crm_debug_3("Processing LH of ordering constraint %d", order->id);

	if(lh_action != NULL) {
		lh_actions = g_list_append(NULL, lh_action);

	} else if(lh_action == NULL && lh_rsc != NULL) {
		lh_actions = find_actions(
			lh_rsc->actions, order->lh_action_task, NULL);

		if(lh_actions == NULL) {
			crm_debug_4("No LH-Side (%s/%s) found for constraint",
				  lh_rsc->id, order->lh_action_task);

			if(lh_rsc->next_role == RSC_ROLE_STOPPED) {
				resource_t *rh_rsc = order->rh_rsc;
				if(order->rh_action
				   && (order->type & pe_order_internal_restart)) {
					crm_debug_3("No LH(%s/%s) found for RH(%s)...",
						    lh_rsc->id, order->lh_action_task,
						    order->rh_action->uuid);
					order->rh_action->runnable = FALSE;
					return;
				
				} else if(rh_rsc != NULL) {
					crm_debug_3("No LH(%s/%s) found for RH(%s/%s)...",
						    lh_rsc->id, order->lh_action_task,
						    rh_rsc->id, order->rh_action_task);
					rh_rsc->cmds->rsc_order_rh(NULL, rh_rsc, order);
					return;
				}
			}
			
			return;
		}

	} else {
		pe_warn("No LH-Side (%s) specified for constraint",
			 order->lh_action_task);
		if(order->rh_rsc != NULL) {
			crm_debug_4("RH-Side was: (%s/%s)",
				  order->rh_rsc->id,
				  order->rh_action_task);
				  
		} else if(order->rh_action != NULL
			  && order->rh_action->rsc != NULL) {
			crm_debug_4("RH-Side was: (%s/%s)",
				  order->rh_action->rsc->id,
				  order->rh_action_task);
				  
		} else if(order->rh_action != NULL) {
			crm_debug_4("RH-Side was: %s",
				  order->rh_action_task);
		} else {
			crm_debug_4("RH-Side was NULL");
		}		
		
		return;
	}

	slist_iter(
		lh_action_iter, action_t, lh_actions, lpc,

		resource_t *rh_rsc = order->rh_rsc;
		if(rh_rsc == NULL && order->rh_action) {
			rh_rsc = order->rh_action->rsc;
		}
		
		if(rh_rsc) {
			rh_rsc->cmds->rsc_order_rh(
				lh_action_iter, rh_rsc, order);

		} else if(order->rh_action) {
			order_actions(lh_action_iter, order->rh_action, order->type); 

		}
		);

	pe_free_shallow_adv(lh_actions, FALSE);
}

void native_rsc_order_rh(
	action_t *lh_action, resource_t *rsc, order_constraint_t *order)
{
	GListPtr rh_actions = NULL;
	action_t *rh_action = NULL;

	CRM_CHECK(rsc != NULL, return);
	CRM_CHECK(order != NULL, return);

	rh_action = order->rh_action;
	crm_debug_3("Processing RH of ordering constraint %d", order->id);

	if(rh_action != NULL) {
		rh_actions = g_list_append(NULL, rh_action);

	} else if(rsc != NULL) {
		rh_actions = find_actions(
			rsc->actions, order->rh_action_task, NULL);
	}

	if(rh_actions == NULL) {
		crm_debug_4("No RH-Side (%s/%s) found for constraint..."
			    " ignoring", rsc->id,order->rh_action_task);
		if(lh_action) {
			crm_debug_4("LH-Side was: %s", lh_action->uuid);
		}
		return;
	}
	
	slist_iter(
		rh_action_iter, action_t, rh_actions, lpc,

		if(lh_action) {
			order_actions(lh_action, rh_action_iter, order->type); 

		} else if(order->type & pe_order_internal_restart) {
			rh_action_iter->runnable = FALSE;
		}
		
		);

	pe_free_shallow_adv(rh_actions, FALSE);
}

void native_rsc_location(resource_t *rsc, rsc_to_node_t *constraint)
{
	GListPtr or_list;

	crm_debug_2("Applying %s (%s) to %s", constraint->id,
		    role2text(constraint->role_filter), rsc->id);

	/* take "lifetime" into account */
	if(constraint == NULL) {
		pe_err("Constraint is NULL");
		return;

	} else if(rsc == NULL) {
		pe_err("LHS of rsc_to_node (%s) is NULL", constraint->id);
		return;

	} else if(constraint->role_filter > 0
		  && constraint->role_filter != rsc->next_role) {
		crm_debug("Constraint (%s) is not active (role : %s)",
			    constraint->id, role2text(constraint->role_filter));
		return;
		
	} else if(is_active(constraint) == FALSE) {
		crm_debug_2("Constraint (%s) is not active", constraint->id);
		return;
	}
    
	if(constraint->node_list_rh == NULL) {
		crm_debug_2("RHS of constraint %s is NULL", constraint->id);
		return;
	}
	or_list = node_list_or(
		rsc->allowed_nodes, constraint->node_list_rh, FALSE);
		
	pe_free_shallow(rsc->allowed_nodes);
	rsc->allowed_nodes = or_list;
	slist_iter(node, node_t, or_list, lpc,
		   crm_debug_3("%s + %s : %d", rsc->id, node->details->uname, node->weight);
		);
}

void native_expand(resource_t *rsc, pe_working_set_t *data_set)
{
	slist_iter(
		action, action_t, rsc->actions, lpc,
		crm_debug_4("processing action %d for rsc=%s",
			  action->id, rsc->id);
		graph_element_from_action(action, data_set);
		);
}



void
native_agent_constraints(resource_t *rsc)
{
}

void
create_notifications(resource_t *rsc, pe_working_set_t *data_set)
{
	if(rsc->notify == FALSE) {
		return;
	}
	
/* 	slist_iter( */
/* 		action, action_t, rsc->actions, lpc, */
		
/* 		); */

}

static void
register_activity(resource_t *rsc, enum action_tasks task, node_t *node, notify_data_t *n_data)
{
	notify_entry_t *entry = NULL;
	crm_malloc0(entry, sizeof(notify_entry_t));
	entry->rsc = rsc;
	entry->node = node;
	switch(task) {
		case start_rsc:
			n_data->start = g_list_append(n_data->start, entry);
			break;
		case stop_rsc:
			n_data->stop = g_list_append(n_data->stop, entry);
			break;
		case action_promote:
			n_data->promote = g_list_append(n_data->promote, entry);
			break;
		case action_demote:
			n_data->demote = g_list_append(n_data->demote, entry);
			break;
		default:
			crm_err("Unsupported notify action: %s", task2text(task));
			crm_free(entry);
			break;
	}
	
}


static void
register_state(resource_t *rsc, node_t *on_node, notify_data_t *n_data)
{
	notify_entry_t *entry = NULL;
	crm_malloc0(entry, sizeof(notify_entry_t));
	entry->rsc = rsc;
	entry->node = on_node;

	crm_debug_2("%s state: %s", rsc->id, role2text(rsc->next_role));

	switch(rsc->next_role) {
		case RSC_ROLE_STOPPED:
/* 			n_data->inactive = g_list_append(n_data->inactive, entry); */
			crm_free(entry);
			break;
		case RSC_ROLE_STARTED:
			n_data->active = g_list_append(n_data->active, entry);
			break;
		case RSC_ROLE_SLAVE:
 			n_data->slave = g_list_append(n_data->slave, entry); 
			break;
		case RSC_ROLE_MASTER:
			n_data->master = g_list_append(n_data->master, entry);
			break;
		default:
			crm_err("Unsupported notify role");
			crm_free(entry);
			break;
	}
}

void
native_create_notify_element(resource_t *rsc, action_t *op,
			     notify_data_t *n_data, pe_working_set_t *data_set)
{
	node_t *next_node = NULL;
	gboolean registered = FALSE;
	char *op_key = NULL;
	GListPtr possible_matches = NULL;
	enum action_tasks task = text2task(op->task);
	
	if(op->pre_notify == NULL || op->post_notify == NULL) {
		/* no notifications required */
		crm_debug_4("No notificaitons required for %s", op->task);
		return;
	}
	next_node = rsc->allocated_to;
	op_key = generate_op_key(rsc->id, op->task, 0);
	possible_matches = find_actions(rsc->actions, op_key, NULL);
	
	crm_debug_2("Creating notificaitons for: %s (%s->%s)",
		    op->uuid, role2text(rsc->role), role2text(rsc->next_role));

	if(rsc->role == rsc->next_role) {
		register_state(rsc, next_node, n_data);
	}
	
	slist_iter(
		local_op, action_t, possible_matches, lpc,

		local_op->notify_keys = n_data->keys;
		if(local_op->optional == FALSE) {
			registered = TRUE;
			register_activity(rsc, task, local_op->node, n_data);
		}		
		);

	/* stop / demote */
	if(rsc->role != RSC_ROLE_STOPPED) {
		if(task == stop_rsc || task == action_demote) {
			slist_iter(
				current_node, node_t, rsc->running_on, lpc,
				pe_pre_notify(rsc, current_node, op, n_data, data_set);
				if(task == action_demote || registered == FALSE) {
					pe_post_notify(rsc, current_node, op, n_data, data_set);
				}
				);
		}
	}
	
	/* start / promote */
	if(rsc->next_role != RSC_ROLE_STOPPED) {	
		CRM_CHECK(next_node != NULL,;);

		if(next_node == NULL) {
			pe_proc_err("next role: %s", role2text(rsc->next_role));
			
		} else if(task == start_rsc || task == action_promote) {
			if(task != start_rsc || registered == FALSE) {
				pe_pre_notify(rsc, next_node, op, n_data, data_set);
			}
			pe_post_notify(rsc, next_node, op, n_data, data_set);
		}
	}
	
	crm_free(op_key);
	pe_free_shallow_adv(possible_matches, FALSE);
}


static void dup_attr(gpointer key, gpointer value, gpointer user_data)
{
	char *meta_key = crm_concat(CRM_META, key, '_');
	g_hash_table_replace(user_data, meta_key, crm_strdup(value));
}

static action_t *
pe_notify(resource_t *rsc, node_t *node, action_t *op, action_t *confirm,
	  notify_data_t *n_data, pe_working_set_t *data_set)
{
	char *key = NULL;
	action_t *trigger = NULL;
	const char *value = NULL;
	const char *task = NULL;
	
	if(op == NULL || confirm == NULL) {
		crm_debug_2("Op=%p confirm=%p", op, confirm);
		return NULL;
	}

	CRM_CHECK(node != NULL, return NULL);

	if(node->details->online == FALSE) {
		crm_info("Skipping notification for %s", rsc->id);
		return NULL;
	}
	
	value = g_hash_table_lookup(op->meta, "notify_type");
	task = g_hash_table_lookup(op->meta, "notify_operation");

	crm_debug_2("Creating actions for %s: %s (%s-%s)",
		    op->uuid, rsc->id, value, task);
	
	key = generate_notify_key(rsc->id, value, task);
	trigger = custom_action(rsc, key, op->task, node,
				op->optional, TRUE, data_set);
	g_hash_table_foreach(op->meta, dup_attr, trigger->extra);
	trigger->notify_keys = n_data->keys;

	/* pseudo_notify before notify */
	crm_debug_3("Ordering %s before %s (%d->%d)",
		op->uuid, trigger->uuid, trigger->id, op->id);

	order_actions(op, trigger, pe_order_implies_left);
	
	value = g_hash_table_lookup(op->meta, "notify_confirm");
	if(crm_is_true(value)) {
		/* notify before pseudo_notified */
		crm_debug_3("Ordering %s before %s (%d->%d)",
			    trigger->uuid, confirm->uuid,
			    confirm->id, trigger->id);

		order_actions(trigger, confirm, pe_order_implies_left);
	}	
	return trigger;
}

void
pe_pre_notify(resource_t *rsc, node_t *node, action_t *op,
	      notify_data_t *n_data, pe_working_set_t *data_set)
{
	crm_debug_2("%s: %s", rsc->id, op->uuid);
	pe_notify(rsc, node, op->pre_notify, op->pre_notified,
		  n_data, data_set);
}

void
pe_post_notify(resource_t *rsc, node_t *node, action_t *op, 
	       notify_data_t *n_data, pe_working_set_t *data_set)
{
	action_t *notify = NULL;

	CRM_CHECK(op != NULL, return);
	CRM_CHECK(rsc != NULL, return);
	
	crm_debug_2("%s: %s", rsc->id, op->uuid);
	notify = pe_notify(rsc, node, op->post_notify, op->post_notified,
			   n_data, data_set);

	if(notify != NULL) {
/* 		crm_err("Upgrading priority for %s to INFINITY", notify->uuid); */
		notify->priority = INFINITY;
	}

	notify = op->post_notified;
	if(notify != NULL) {
		slist_iter(
			mon, action_t, rsc->actions, lpc,

			const char *interval = g_hash_table_lookup(mon->meta, "interval");
			if(interval == NULL || safe_str_eq(interval, "0")) {
				crm_debug_3("Skipping %s: interval", mon->uuid); 
				continue;
			} else if(safe_str_eq(mon->task, "cancel")) {
				crm_debug_3("Skipping %s: cancel", mon->uuid); 
				continue;
			}

			order_actions(notify, mon, pe_order_optional);
			);
	}
}


void
NoRoleChange(resource_t *rsc, node_t *current, node_t *next,
	     pe_working_set_t *data_set)
{
	action_t *start = NULL;
	action_t *stop = NULL;
	GListPtr possible_matches = NULL;

	crm_debug("Executing: %s (role=%s)",rsc->id, role2text(rsc->next_role));

	if(current == NULL || next == NULL) {
		return;
	}

	/* use StartRsc/StopRsc */
	
	if(safe_str_neq(current->details->id, next->details->id)) {
		crm_notice("Move  resource %s\t(%s -> %s)", rsc->id,
			   current->details->uname, next->details->uname);

		stop = stop_action(rsc, current, FALSE);
		start = start_action(rsc, next, FALSE);

		if(rsc->role == RSC_ROLE_MASTER) {
			demote_action(rsc, current, FALSE);
			promote_action(rsc, next, FALSE);
		}

		possible_matches = find_recurring_actions(rsc->actions, next);
		slist_iter(match, action_t, possible_matches, lpc,
			   if(match->optional == FALSE) {
				   crm_err("Found bad recurring action: %s",
					   match->uuid);
				   match->optional = TRUE;
			   }
			);
			
		if(data_set->remove_after_stop) {
			DeleteRsc(rsc, current, data_set);
		}
		
	} else {
		if(rsc->failed) {
			crm_notice("Recover resource %s\t(%s)",
				   rsc->id, next->details->uname);
			stop = stop_action(rsc, current, FALSE);
			start = start_action(rsc, next, FALSE);
/* 			/\* make the restart required *\/ */
/* 			order_stop_start(rsc, rsc, pe_order_implies_left); */
			
		} else if(rsc->start_pending) {
			start = start_action(rsc, next, TRUE);
			if(start->runnable) {
				/* wait for StartRsc() to be called */
				rsc->role = RSC_ROLE_STOPPED;
			} else {
				/* wait for StopRsc() to be called */
				rsc->next_role = RSC_ROLE_STOPPED;
			}
			
		} else {
			stop = stop_action(rsc, current, TRUE);
			start = start_action(rsc, next, TRUE);
			stop->optional = start->optional;
			
			if(start->runnable == FALSE) {
				rsc->next_role = RSC_ROLE_STOPPED;

			} else if(start->optional) {
				crm_notice("Leave resource %s\t(%s)",
					   rsc->id, next->details->uname);

			} else {
				crm_notice("Restart resource %s\t(%s)",
					   rsc->id, next->details->uname);
			}
		}
	}
}


gboolean
StopRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set)
{
	action_t *stop = NULL;
	
	crm_debug_2("Executing: %s", rsc->id);
	
	slist_iter(
		current, node_t, rsc->running_on, lpc,
		crm_notice("  %s\tStop %s", current->details->uname, rsc->id);
		stop = stop_action(rsc, current, FALSE);

		if(data_set->remove_after_stop) {
			DeleteRsc(rsc, current, data_set);
		}
		);
	
	return TRUE;
}


gboolean
StartRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set)
{
	action_t *start = NULL;
	
	crm_debug_2("Executing: %s", rsc->id);
	start = start_action(rsc, next, TRUE);
	if(start->runnable) {
		crm_notice(" %s\tStart %s", next->details->uname, rsc->id);
		start->optional = FALSE;
	}		
	return TRUE;
}

gboolean
PromoteRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set)
{
	char *key = NULL;
	gboolean runnable = TRUE;
	GListPtr action_list = NULL;
	crm_debug_2("Executing: %s", rsc->id);

	CRM_CHECK(rsc->next_role == RSC_ROLE_MASTER, return FALSE);

	key = start_key(rsc);
	action_list = find_actions_exact(rsc->actions, key, next);
	crm_free(key);

	slist_iter(start, action_t, action_list, lpc,
		   if(start->runnable == FALSE) {
			   runnable = FALSE;
		   }
		);

	if(runnable) {
		promote_action(rsc, next, FALSE);
		crm_notice("%s\tPromote %s", next->details->uname, rsc->id);
		return TRUE;
	} 

	crm_debug("%s\tPromote %s (canceled)", next->details->uname, rsc->id);

	key = promote_key(rsc);
	action_list = find_actions_exact(rsc->actions, key, next);
	crm_free(key);

	slist_iter(promote, action_t, action_list, lpc,
		   promote->runnable = FALSE;
		);
	
	return TRUE;
}

gboolean
DemoteRsc(resource_t *rsc, node_t *next, pe_working_set_t *data_set)
{
	crm_debug_2("Executing: %s", rsc->id);

/* 	CRM_CHECK(rsc->next_role == RSC_ROLE_SLAVE, return FALSE); */
	slist_iter(
		current, node_t, rsc->running_on, lpc,
		crm_notice("%s\tDeomote %s", current->details->uname, rsc->id);
		demote_action(rsc, current, FALSE);
		);
	return TRUE;
}

gboolean
RoleError(resource_t *rsc, node_t *next, pe_working_set_t *data_set)
{
	crm_debug("Executing: %s", rsc->id);
	CRM_CHECK(FALSE, return FALSE);
	return FALSE;
}

gboolean
NullOp(resource_t *rsc, node_t *next, pe_working_set_t *data_set)
{
	crm_debug("Executing: %s", rsc->id);
	return FALSE;
}

gboolean
DeleteRsc(resource_t *rsc, node_t *node, pe_working_set_t *data_set)
{
	action_t *delete = NULL;
 	action_t *refresh = NULL;

	if(rsc->failed) {
		crm_debug_2("Resource %s not deleted from %s: failed",
			    rsc->id, node->details->uname);
		return FALSE;
		
	} else if(node == NULL) {
		crm_debug_2("Resource %s not deleted: NULL node", rsc->id);
		return FALSE;
		
	} else if(node->details->unclean || node->details->online == FALSE) {
		crm_debug_2("Resource %s not deleted from %s: unrunnable",
			    rsc->id, node->details->uname);
		return FALSE;
	}
	
	crm_notice("Removing %s from %s",
		 rsc->id, node->details->uname);
	
	delete = delete_action(rsc, node);

#if DELETE_THEN_REFRESH
	refresh = custom_action(
		NULL, crm_strdup(CRM_OP_LRM_REFRESH), CRM_OP_LRM_REFRESH,
		node, FALSE, TRUE, data_set);

	add_hash_param(refresh->meta, XML_ATTR_TE_NOWAIT, XML_BOOLEAN_TRUE);

	order_actions(delete, refresh, pe_order_optional);
#endif
	
	return TRUE;
}

gboolean
native_create_probe(resource_t *rsc, node_t *node, action_t *complete,
		    gboolean force, pe_working_set_t *data_set) 
{
	char *key = NULL;
	char *target_rc = NULL;
	action_t *probe = NULL;
	node_t *running = NULL;

	CRM_CHECK(node != NULL, return FALSE);

	if(rsc->orphan) {
		crm_debug_2("Skipping orphan: %s", rsc->id);
		return FALSE;
	}
	
	running = pe_find_node_id(rsc->known_on, node->details->id);
	if(force == FALSE && running != NULL) {
		/* we already know the status of the resource on this node */
		crm_debug_3("Skipping active: %s", rsc->id);
		return FALSE;
	}

	key = generate_op_key(rsc->id, CRMD_ACTION_STATUS, 0);
	probe = custom_action(rsc, key, CRMD_ACTION_STATUS, node,
			      FALSE, TRUE, data_set);
	probe->priority = INFINITY;

	running = pe_find_node_id(rsc->running_on, node->details->id);
	if(running == NULL) {
		target_rc = crm_itoa(EXECRA_NOT_RUNNING);
		add_hash_param(probe->meta, XML_ATTR_TE_TARGET_RC, target_rc);
		crm_free(target_rc);
	}
	
	crm_debug_2("%s: Created probe for %s", node->details->uname, rsc->id);
	
	custom_action_order(rsc, NULL, probe, rsc, NULL, complete,
			    pe_order_implies_left, data_set);

	return TRUE;
}

static void
native_start_constraints(
	resource_t *rsc,  action_t *stonith_op, gboolean is_stonith,
	pe_working_set_t *data_set)
{
	gboolean is_unprotected = FALSE;
	gboolean run_unprotected = TRUE;

	if(is_stonith) {
		char *key = start_key(rsc);
		crm_debug_2("Ordering %s action before stonith events", key);
		custom_action_order(
			rsc, key, NULL,
			NULL, crm_strdup(CRM_OP_FENCE), stonith_op,
			pe_order_optional, data_set);

	} else {
		slist_iter(action, action_t, rsc->actions, lpc2,
			   if(action->needs != rsc_req_stonith) {
				   crm_debug_3("%s doesnt need to wait for stonith events", action->uuid);
				   continue;
			   }
			   crm_debug_2("Ordering %s after stonith events", action->uuid);
			   if(stonith_op != NULL) {
				   custom_action_order(
					   NULL, crm_strdup(CRM_OP_FENCE), stonith_op,
					   rsc, NULL, action,
					   pe_order_implies_left, data_set);
				   
			   } else if(run_unprotected == FALSE) {
				   /* mark the start unrunnable */
				   action->runnable = FALSE;
				   
			   } else {
				   is_unprotected = TRUE;
			   }   
			);
	}
	
	if(is_unprotected) {
		pe_err("SHARED RESOURCE %s IS NOT PROTECTED:"
		       " Stonith disabled", rsc->id);
	}

}

static void
native_stop_constraints(
	resource_t *rsc,  action_t *stonith_op, gboolean is_stonith,
	pe_working_set_t *data_set)
{
	char *key = NULL;
	GListPtr action_list = NULL;
	node_t *node = stonith_op->node;

	key = stop_key(rsc);
	action_list = find_actions(rsc->actions, key, node);
	crm_free(key);

	/* add the stonith OP as a stop pre-req and the mark the stop
	 * as a pseudo op - since its now redundant
	 */
	
	slist_iter(
		action, action_t, action_list, lpc2,
		if(node->details->online == FALSE || rsc->failed) {
			resource_t *parent = NULL;
			crm_warn("Stop of failed resource %s is"
				 " implicit after %s is fenced",
				 action->uuid, node->details->uname);
			/* the stop would never complete and is
			 * now implied by the stonith operation
			 */
			action->pseudo = TRUE;
			action->runnable = TRUE;
			if(is_stonith) {
				/* do nothing */
				
			} else {
				custom_action_order(
					NULL, crm_strdup(CRM_OP_FENCE),stonith_op,
					rsc, start_key(rsc), NULL,
					pe_order_implies_left, data_set);
			}
			
			/* find the top-most resource */
			parent = rsc->parent;
			while(parent != NULL && parent->parent != NULL) {
				parent = parent->parent;
			}
			
			if(parent) {
				crm_info("Re-creating actions for %s",
					 parent->id);
				parent->cmds->create_actions(parent, data_set);

				/* make sure we dont mess anything up in create_actions */
				CRM_CHECK(action->pseudo, action->pseudo = TRUE);
				CRM_CHECK(action->runnable, action->runnable = TRUE);
			}
			
		} else if(is_stonith == FALSE) {
			crm_info("Moving healthy resource %s"
				 " off %s before fencing",
				 rsc->id, node->details->uname);
			
			/* stop healthy resources before the
			 * stonith op
			 */
			custom_action_order(
				rsc, stop_key(rsc), NULL,
				NULL,crm_strdup(CRM_OP_FENCE),stonith_op,
				pe_order_implies_left, data_set);
		}
		);
	
	key = demote_key(rsc);
	action_list = find_actions(rsc->actions, key, node);
	crm_free(key);
	
	slist_iter(
		action, action_t, action_list, lpc2,
		if(node->details->online == FALSE || rsc->failed) {
			crm_info("Demote of failed resource %s is"
				 " implict after %s is fenced",
				 rsc->id, node->details->uname);
			/* the stop would never complete and is
			 * now implied by the stonith operation
			 */
			action->pseudo = TRUE;
			action->runnable = TRUE;
			if(is_stonith == FALSE) {
				custom_action_order(
					NULL, crm_strdup(CRM_OP_FENCE), stonith_op,
					rsc, demote_key(rsc), NULL,
					pe_order_implies_left, data_set);
			}
		}
		);	
}

void
native_stonith_ordering(
	resource_t *rsc,  action_t *stonith_op, pe_working_set_t *data_set)
{
	gboolean is_stonith = FALSE;
	const char *class = crm_element_value(rsc->xml, XML_AGENT_ATTR_CLASS);

	if(rsc->is_managed == FALSE) {
		crm_debug_3("Skipping fencing constraints for unmanaged resource: %s", rsc->id);
		return;
	} 

	if(stonith_op != NULL && safe_str_eq(class, "stonith")) {
		is_stonith = TRUE;
	}
	
	/* Start constraints */
	native_start_constraints(rsc,  stonith_op, is_stonith, data_set);
 
	/* Stop constraints */
	native_stop_constraints(rsc,  stonith_op, is_stonith, data_set);
}

void
native_migrate_reload(resource_t *rsc, pe_working_set_t *data_set)
{
	char *key = NULL;
	GListPtr action_list = NULL;
	
	const char *value = NULL;
	action_t *stop = NULL;
	action_t *start = NULL;
	action_t *other = NULL;
	action_t *action = NULL;

	if(rsc->variant != pe_native) {
		return;
	}
	
	if(rsc->is_managed == FALSE
	   || rsc->failed
	   || rsc->start_pending
	   || rsc->next_role != RSC_ROLE_STARTED		   
	   || g_list_length(rsc->running_on) != 1) {
		crm_debug_3("%s: resource", rsc->id);
		return;
	}
	
	key = start_key(rsc);
	action_list = find_actions(rsc->actions, key, NULL);
	crm_free(key);
	
	if(action_list == NULL) {
		crm_debug_3("%s: no start action", rsc->id);
		return;
	}
	
	start = action_list->data;
	
	value = g_hash_table_lookup(rsc->meta, "allow_migrate");
	if(crm_is_true(value)) {
		rsc->can_migrate = TRUE;	
	}
	
	if(rsc->can_migrate == FALSE
	   && start->allow_reload_conversion == FALSE) {
		crm_debug_3("%s: no need to continue", rsc->id);
		return;
	}
	
	key = stop_key(rsc);
	action_list = find_actions(rsc->actions, key, NULL);
	crm_free(key);
	
	if(action_list == NULL) {
		crm_debug_3("%s: no stop action", rsc->id);
		return;
	}
	
	stop = action_list->data;
	
	action = start;
	if(action->pseudo
	   || action->optional
	   || action->node == NULL
	   || action->runnable == FALSE) {
		crm_debug_3("Skipping: %s", action->task);
		return;
	}
	
	action = stop;
	if(action->pseudo
	   || action->optional
	   || action->node == NULL
	   || action->runnable == FALSE) {
		crm_debug_3("Skipping: %s", action->task);
		return;
	}
	
	slist_iter(
		other_w, action_wrapper_t, start->actions_before, lpc,
		other = other_w->action;
		if(other->optional == FALSE
		   && other->rsc != NULL
		   && other->rsc != start->rsc) {
			crm_debug_2("Skipping: start depends");
			return;
		}
		);
	
	slist_iter(
		other_w, action_wrapper_t, stop->actions_after, lpc,
		other = other_w->action;
		if(other->optional == FALSE
		   && other->rsc != NULL
		   && other->rsc != stop->rsc) {
			crm_debug_2("Skipping: stop depends");
			return;
		}
		);
	
	if(rsc->can_migrate && stop->node->details != start->node->details) {
		crm_info("Migrating %s from %s to %s", rsc->id,
			 stop->node->details->uname,
			 start->node->details->uname);
		
		crm_free(stop->uuid);
		stop->task = CRMD_ACTION_MIGRATE;
		stop->uuid = generate_op_key(rsc->id, stop->task, 0);
		add_hash_param(stop->meta, "migrate_source",
			       stop->node->details->uname);
		add_hash_param(stop->meta, "migrate_target",
			       start->node->details->uname);
		
		crm_free(start->uuid);
		start->task = CRMD_ACTION_MIGRATED;
		start->uuid = generate_op_key(rsc->id, start->task, 0);
		add_hash_param(start->meta, "migrate_source_uuid",
			       stop->node->details->id);
		add_hash_param(start->meta, "migrate_source",
			       stop->node->details->uname);
		add_hash_param(start->meta, "migrate_target",
			       start->node->details->uname);
		
	} else if(start->allow_reload_conversion
		  && stop->node->details == start->node->details) {
		crm_info("Rewriting restart of %s on %s as a reload",
			 rsc->id, start->node->details->uname);
		crm_free(start->uuid);
		start->task = "reload";
		start->uuid = generate_op_key(rsc->id, start->task, 0);
		
		stop->pseudo = TRUE; /* easier than trying to delete it from the graph */
		
	} else {
		crm_debug_3("%s nothing to do", rsc->id);
	}
}
