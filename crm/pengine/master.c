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

#include <crm/msg_xml.h>
#include <allocate.h>
#include <lib/crm/pengine/utils.h>
#include <utils.h>

#define VARIANT_CLONE 1
#include <lib/crm/pengine/variant.h>

#define NO_MASTER_PREFS 0

extern gint sort_clone_instance(gconstpointer a, gconstpointer b);

extern void clone_create_notifications(
	resource_t *rsc, action_t *action, action_t *action_complete,
	pe_working_set_t *data_set);

static void
child_promoting_constraints(
	clone_variant_data_t *clone_data, enum pe_ordering type,
	resource_t *rsc, resource_t *child, resource_t *last, pe_working_set_t *data_set)
{
/* 	if(clone_data->ordered */
/* 	   || clone_data->self->restart_type == pe_restart_restart) { */
/* 		type = pe_order_implies_left; */
/* 	} */
	if(child == NULL) {
		if(clone_data->ordered && last != NULL) {
			crm_debug_4("Ordered version (last node)");
			/* last child promote before promoted started */
			custom_action_order(
				last, promote_key(last), NULL,
				rsc, promoted_key(rsc), NULL,
				type, data_set);
		}
		
	} else if(clone_data->ordered) {
		crm_debug_4("Ordered version");
		if(last == NULL) {
			/* global promote before first child promote */
			last = rsc;

		} /* else: child/child relative promote */

		order_start_start(last, child, type);
		custom_action_order(
			last, promote_key(last), NULL,
			child, promote_key(child), NULL,
			type, data_set);

	} else {
		crm_debug_4("Un-ordered version");
		
		/* child promote before global promoted */
		custom_action_order(
			child, promote_key(child), NULL,
			rsc, promoted_key(rsc), NULL,
			type, data_set);
                
		/* global promote before child promote */
		custom_action_order(
			rsc, promote_key(rsc), NULL,
			child, promote_key(child), NULL,
			type, data_set);

	}
}

static void
child_demoting_constraints(
	clone_variant_data_t *clone_data, enum pe_ordering type,
	resource_t *rsc, resource_t *child, resource_t *last, pe_working_set_t *data_set)
{
/* 	if(clone_data->ordered */
/* 	   || clone_data->self->restart_type == pe_restart_restart) { */
/* 		type = pe_order_implies_left; */
/* 	} */
	
	if(child == NULL) {
		if(clone_data->ordered && last != NULL) {
			crm_debug_4("Ordered version (last node)");
			/* global demote before first child demote */
			custom_action_order(
				rsc, demote_key(rsc), NULL,
				last, demote_key(last), NULL,
				pe_order_implies_left, data_set);
		}
		
	} else if(clone_data->ordered && last != NULL) {
		crm_debug_4("Ordered version");

		/* child/child relative demote */
		custom_action_order(child, demote_key(child), NULL,
				    last, demote_key(last), NULL,
				    type, data_set);

	} else if(clone_data->ordered) {
		crm_debug_4("Ordered version (1st node)");
		/* first child stop before global stopped */
		custom_action_order(
			child, demote_key(child), NULL,
			rsc, demoted_key(rsc), NULL,
			type, data_set);

	} else {
		crm_debug_4("Un-ordered version");

		/* child demote before global demoted */
		custom_action_order(
			child, demote_key(child), NULL,
			rsc, demoted_key(rsc), NULL,
			type, data_set);
                        
		/* global demote before child demote */
		custom_action_order(
			rsc, demote_key(rsc), NULL,
			child, demote_key(child), NULL,
			type, data_set);
	}
}

static void
master_update_pseudo_status(
	resource_t *child, gboolean *demoting, gboolean *promoting) 
{
	CRM_ASSERT(demoting != NULL);
	CRM_ASSERT(promoting != NULL);

	slist_iter(
		action, action_t, child->actions, lpc,

		if(*promoting && *demoting) {
			return;

		} else if(action->optional) {
			continue;

		} else if(safe_str_eq(CRMD_ACTION_DEMOTE, action->task)) {
			*demoting = TRUE;

		} else if(safe_str_eq(CRMD_ACTION_PROMOTE, action->task)) {
			*promoting = TRUE;
		}
		);

}

#define apply_master_location(list)					\
	slist_iter(							\
		cons, rsc_to_node_t, list, lpc2,			\
		cons_node = NULL;					\
		if(cons->role_filter == RSC_ROLE_MASTER) {		\
			crm_debug("Applying %s to %s",			\
				  cons->id, child_rsc->id);		\
			cons_node = pe_find_node_id(			\
				cons->node_list_rh, chosen->details->id); \
		}							\
		if(cons_node != NULL) {				\
			int new_priority = merge_weights(		\
				child_rsc->priority, cons_node->weight); \
			crm_debug("\t%s: %d->%d", child_rsc->id,	\
				  child_rsc->priority, new_priority);	\
			child_rsc->priority = new_priority;		\
		}							\
		);

#define apply_master_colocation(list)					\
	slist_iter(							\
		cons, rsc_colocation_t, list, lpc2,			\
		cons_node = cons->rsc_lh->allocated_to;			\
		if(cons->role_lh == RSC_ROLE_MASTER			\
		   && cons_node != NULL					\
		   && chosen->details == cons_node->details) {		\
			int new_priority = merge_weights(		\
				child_rsc->priority, cons->score);	\
			crm_debug("Applying %s to %s",			\
				  cons->id, child_rsc->id);		\
			crm_debug("\t%s: %d->%d", child_rsc->id,	\
				  child_rsc->priority, new_priority);	\
			child_rsc->priority = new_priority;		\
		}							\
		);

static node_t *
can_be_master(resource_t *rsc)
{
	node_t *node = NULL;
	node_t *local_node = NULL;
	clone_variant_data_t *clone_data = NULL;

	node = rsc->allocated_to;
	if(rsc->priority < 0) {
		crm_debug_2("%s cannot be master: preference",
			    rsc->id);
		return NULL;
	} else if(node == NULL) {
		crm_debug_2("%s cannot be master: not allocated",
			    rsc->id);
		return NULL;
	} else if(can_run_resources(node) == FALSE) {
		crm_debug_2("Node cant run any resources: %s",
			    node->details->uname);
		return NULL;
	}

	get_clone_variant_data(clone_data, rsc->parent);
	local_node = pe_find_node_id(
		rsc->parent->allowed_nodes, node->details->id);

	if(local_node == NULL) {
		crm_err("%s cannot run on %s: node not allowed",
			rsc->id, node->details->uname);
		return NULL;

	} else if(local_node->count < clone_data->master_node_max) {
		return local_node;

	} else {
		crm_debug_2("%s cannot be master on %s: node full",
			    rsc->id, node->details->uname);
	}

	return NULL;
}

static gint sort_master_instance(gconstpointer a, gconstpointer b)
{
	int rc;
	const resource_t *resource1 = (const resource_t*)a;
	const resource_t *resource2 = (const resource_t*)b;

	CRM_ASSERT(resource1 != NULL);
	CRM_ASSERT(resource2 != NULL);

	rc = sort_rsc_priority(a, b);
	if( rc != 0 ) {
		return rc;
	}
	
	if(resource1->role > resource2->role) {
		return -1;

	} else if(resource1->role < resource2->role) {
		return 1;
	}
	
	return sort_clone_instance(a, b);
}

node_t *
master_color(resource_t *rsc, pe_working_set_t *data_set)
{
	int len = 0;
	int promoted = 0;
	node_t *chosen = NULL;
	node_t *cons_node = NULL;

	char *attr_name = NULL;
	const char *attr_value = NULL;
	
	clone_variant_data_t *clone_data = NULL;
	get_clone_variant_data(clone_data, rsc);

	clone_color(rsc, data_set);
	
	/* count now tracks the number of masters allocated */
	slist_iter(node, node_t, rsc->allowed_nodes, lpc,
		   node->count = 0;
		);

	/*
	 * assign priority
	 */
	slist_iter(
		child_rsc, resource_t, clone_data->child_list, lpc,

		crm_debug_2("Assigning priority for %s", child_rsc->id);
		chosen = child_rsc->allocated_to;
		if(chosen == NULL) {
			continue;
			
		} else if(child_rsc->role == RSC_ROLE_STARTED) {
			child_rsc->role = RSC_ROLE_SLAVE;
		}
		
		switch(child_rsc->next_role) {
			case RSC_ROLE_STARTED:
				if(NO_MASTER_PREFS) {
					child_rsc->priority =
						clone_data->clone_max - lpc;
					break;
				}
				
				child_rsc->priority = -1;

				CRM_CHECK(chosen != NULL, break);

				len = 8 + strlen(child_rsc->id);
				crm_malloc0(attr_name, len);
				sprintf(attr_name, "master-%s", child_rsc->id);
				
				crm_debug_2("looking for %s on %s", attr_name,
					    chosen->details->uname);
				attr_value = g_hash_table_lookup(
					chosen->details->attrs, attr_name);

				if(attr_value == NULL) {
					crm_free(attr_name);
					len = 8 + strlen(child_rsc->long_name);
					crm_malloc0(attr_name, len);
					sprintf(attr_name, "master-%s", child_rsc->long_name);
					crm_debug_2("looking for %s on %s", attr_name,
						    chosen->details->uname);
					attr_value = g_hash_table_lookup(
						chosen->details->attrs, attr_name);
				}
				
				if(attr_value != NULL) {
					crm_debug("%s=%s for %s", attr_name,
						  crm_str(attr_value),
						  chosen->details->uname);
				
					child_rsc->priority = char2score(
						attr_value);
				}

				crm_free(attr_name);
				break;

			case RSC_ROLE_SLAVE:
			case RSC_ROLE_STOPPED:
				child_rsc->priority = -INFINITY;
				break;
			case RSC_ROLE_MASTER:
				/* the only reason we should be here is if
				 * we're re-creating actions after a stonith
				 */
				promoted++;
				break;
			default:
				CRM_CHECK(FALSE/* unhandled */,
					  crm_err("Unknown resource role: %d for %s",
						  child_rsc->next_role, child_rsc->id));
		}
		apply_master_location(child_rsc->rsc_location);
		apply_master_location(rsc->rsc_location);
		apply_master_colocation(rsc->rsc_cons);
		apply_master_colocation(child_rsc->rsc_cons);
		);
	
	/* sort based on the new "promote" priority */
	clone_data->child_list = g_list_sort(
		clone_data->child_list, sort_master_instance);

	/* mark the first N as masters */
	slist_iter(
		child_rsc, resource_t, clone_data->child_list, lpc,

		chosen = NULL;
		crm_debug_2("Processing %s", child_rsc->id);
		if(promoted < clone_data->master_max) {
			chosen = can_be_master(child_rsc);
		}

		if(chosen == NULL) {
			if(child_rsc->next_role == RSC_ROLE_STARTED) {
				child_rsc->next_role = RSC_ROLE_SLAVE;
			}
			continue;
		}

		chosen->count++;
		crm_info("Promoting %s", child_rsc->id);
		child_rsc->next_role = RSC_ROLE_MASTER;
		promoted++;
		
		add_hash_param(child_rsc->parameters, crm_meta_name("role"),
			       role2text(child_rsc->next_role));
		);
	
	crm_info("Promoted %d instances of a possible %d to master", promoted, clone_data->master_max);
	return NULL;
}

void master_create_actions(resource_t *rsc, pe_working_set_t *data_set)
{
	action_t *action = NULL;
	action_t *action_complete = NULL;
	gboolean any_promoting = FALSE;
	gboolean any_demoting = FALSE;
	resource_t *last_promote_rsc = NULL;
	resource_t *last_demote_rsc = NULL;

	clone_variant_data_t *clone_data = NULL;
	get_clone_variant_data(clone_data, rsc);
	
	crm_debug("Creating actions for %s", rsc->id);

	/* create actions as normal */
	clone_create_actions(rsc, data_set);

	slist_iter(
		child_rsc, resource_t, clone_data->child_list, lpc,
		gboolean child_promoting = FALSE;
		gboolean child_demoting = FALSE;

		crm_debug_2("Creating actions for %s", child_rsc->id);
		child_rsc->cmds->create_actions(child_rsc, data_set);
		master_update_pseudo_status(
			child_rsc, &child_demoting, &child_promoting);

		any_demoting = any_demoting || child_demoting;
		any_promoting = any_promoting || child_promoting;
		);
	
	/* promote */
	action = promote_action(rsc, NULL, !any_promoting);
	action_complete = custom_action(
		rsc, promoted_key(rsc),
		CRMD_ACTION_PROMOTED, NULL, !any_promoting, TRUE, data_set);

	action->pseudo = TRUE;
	action->runnable = TRUE;
	action_complete->pseudo = TRUE;
	action_complete->runnable = TRUE;
	action_complete->priority = INFINITY;
	
	child_promoting_constraints(clone_data, pe_order_optional, 
				    rsc, NULL, last_promote_rsc, data_set);

	clone_create_notifications(rsc, action, action_complete, data_set);	


	/* demote */
	action = demote_action(rsc, NULL, !any_demoting);
	action_complete = custom_action(
		rsc, demoted_key(rsc),
		CRMD_ACTION_DEMOTED, NULL, !any_demoting, TRUE, data_set);
	action_complete->priority = INFINITY;

	action->pseudo = TRUE;
	action->runnable = TRUE;
	action_complete->pseudo = TRUE;
	action_complete->runnable = TRUE;
	
	child_demoting_constraints(clone_data, pe_order_optional,
				   rsc, NULL, last_demote_rsc, data_set);

	clone_create_notifications(rsc, action, action_complete, data_set);	
}

void
master_internal_constraints(resource_t *rsc, pe_working_set_t *data_set)
{
	resource_t *last_rsc = NULL;	
	clone_variant_data_t *clone_data = NULL;
	get_clone_variant_data(clone_data, rsc);

	clone_internal_constraints(rsc, data_set);
	
	/* global demoted before start */
	custom_action_order(
		rsc, demoted_key(rsc), NULL,
		rsc, start_key(rsc), NULL,
		pe_order_optional, data_set);

	/* global started before promote */
	custom_action_order(
		rsc, started_key(rsc), NULL,
		rsc, promote_key(rsc), NULL,
		pe_order_optional, data_set);

	/* global demoted before stop */
	custom_action_order(
		rsc, demoted_key(rsc), NULL,
		rsc, stop_key(rsc), NULL,
		pe_order_optional, data_set);

	/* global demote before demoted */
	custom_action_order(
		rsc, demote_key(rsc), NULL,
		rsc, demoted_key(rsc), NULL,
		pe_order_optional, data_set);

	/* global demoted before promote */
	custom_action_order(
		rsc, demoted_key(rsc), NULL,
		rsc, promote_key(rsc), NULL,
		pe_order_optional, data_set);

	slist_iter(
		child_rsc, resource_t, clone_data->child_list, lpc,

		/* child demote before promote */
		custom_action_order(
			child_rsc, demote_key(child_rsc), NULL,
			child_rsc, promote_key(child_rsc), NULL,
			pe_order_optional, data_set);
		
		child_promoting_constraints(clone_data, pe_order_optional,
					    rsc, child_rsc, last_rsc, data_set);

		child_demoting_constraints(clone_data, pe_order_optional,
					   rsc, child_rsc, last_rsc, data_set);

		last_rsc = child_rsc;
		
		);
	
}

void master_rsc_colocation_rh(
	resource_t *rsc_lh, resource_t *rsc_rh, rsc_colocation_t *constraint)
{
	clone_variant_data_t *clone_data = NULL;
	get_clone_variant_data(clone_data, rsc_rh);

	if(rsc_rh->provisional) {
		return;

	} else if(constraint->role_rh == RSC_ROLE_UNKNOWN) {
		crm_debug_3("Handling %s as a clone colocation", constraint->id);
		clone_rsc_colocation_rh(rsc_lh, rsc_rh, constraint);
		return;
	}
	
	CRM_CHECK(rsc_lh != NULL, return);
	CRM_CHECK(rsc_lh->variant == pe_native, return);
	crm_info("Processing constraint %s: %d", constraint->id, constraint->score);

	if(constraint->score < INFINITY) {
		slist_iter(
			child_rsc, resource_t, clone_data->child_list, lpc,
			
			child_rsc->cmds->rsc_colocation_rh(rsc_lh, child_rsc, constraint);
			);

	} else {
		GListPtr lhs = NULL, rhs = NULL;
		lhs = rsc_lh->allowed_nodes;

		slist_iter(
			child_rsc, resource_t, clone_data->child_list, lpc,
			crm_info("Processing: %s", child_rsc->id);
			if(child_rsc->allocated_to != NULL
			   && child_rsc->next_role == constraint->role_rh) {
				crm_info("Applying: %s %s", child_rsc->id, role2text(child_rsc->next_role));
				rhs = g_list_append(rhs, child_rsc->allocated_to);
			}
			);
	
		rsc_lh->allowed_nodes = node_list_and(lhs, rhs, FALSE);
		
		pe_free_shallow_adv(rhs, FALSE);
		pe_free_shallow(lhs);
	}

	return;
}
