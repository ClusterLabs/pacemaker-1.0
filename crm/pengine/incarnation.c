/* $Id: incarnation.c,v 1.9 2005/02/23 15:43:59 andrew Exp $ */
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

#include <pengine.h>
#include <pe_utils.h>
#include <crm/msg_xml.h>

extern gboolean rsc_colocation_new(
	const char *id, enum con_strength strength,
	resource_t *rsc_lh, resource_t *rsc_rh);

typedef struct incarnation_variant_data_s
{
		resource_t *self;

		int incarnation_max;
		int incarnation_max_node;

		int active_incarnation;

		gboolean interleave;
		gboolean ordered;

		GListPtr child_list; /* resource_t* */
} incarnation_variant_data_t;

#define get_incarnation_variant_data(data, rsc)				\
	if(rsc->variant == pe_incarnation) {				\
		data = (incarnation_variant_data_t *)rsc->variant_opaque; \
	} else {							\
		crm_err("Resource %s was not an \"incarnation\" variant", \
			rsc->id);					\
		return;							\
	}

void incarnation_unpack(resource_t *rsc)
{
	int lpc = 0;
	crm_data_t * xml_obj = rsc->xml;
	crm_data_t * xml_self = create_xml_node(NULL, XML_CIB_TAG_RESOURCE);
	incarnation_variant_data_t *incarnation_data = NULL;
	resource_t *self = NULL;

	const char *ordered         = crm_element_value(
		xml_obj, XML_RSC_ATTR_ORDERED);
	const char *interleave      = crm_element_value(
		xml_obj, XML_RSC_ATTR_INTERLEAVE);
	const char *max_incarn      = crm_element_value(
		xml_obj, XML_RSC_ATTR_INCARNATION_MAX);
	const char *max_incarn_node = crm_element_value(
		xml_obj, XML_RSC_ATTR_INCARNATION_NODEMAX);

	crm_verbose("Processing resource %s...", rsc->id);

	crm_malloc(incarnation_data, sizeof(incarnation_variant_data_t));
	incarnation_data->child_list           = NULL;
	incarnation_data->interleave           = FALSE;
	incarnation_data->ordered              = FALSE;
	incarnation_data->active_incarnation   = 0;
	incarnation_data->incarnation_max      = crm_atoi(max_incarn,     "1");
	incarnation_data->incarnation_max_node = crm_atoi(max_incarn_node,"1");

	/* this is a bit of a hack - but simplifies everything else */
	copy_in_properties(xml_self, xml_obj);
	if(common_unpack(xml_self, &self)) {
		incarnation_data->self = self;
		self->restart_type = pe_restart_restart;

	} else {
		crm_xml_err(xml_self, "Couldnt unpack dummy child");
		return;
	}

	if(safe_str_eq(interleave, XML_BOOLEAN_TRUE)) {
		incarnation_data->interleave = TRUE;
	}
	if(safe_str_eq(ordered, XML_BOOLEAN_TRUE)) {
		incarnation_data->ordered = TRUE;
	}
	
	xml_child_iter(
		xml_obj, xml_obj_child, XML_CIB_TAG_RESOURCE,

		char *inc_max = crm_itoa(incarnation_data->incarnation_max);
		for(lpc = 0; lpc < incarnation_data->incarnation_max; lpc++) {
			resource_t *child_rsc = NULL;
			crm_data_t * child_copy = copy_xml_node_recursive(
				xml_obj_child);

			set_id(child_copy, rsc->id, lpc);
			
			if(common_unpack(child_copy, &child_rsc)) {
				char *inc_num = crm_itoa(lpc);

				incarnation_data->child_list = g_list_append(
					incarnation_data->child_list, child_rsc);
				
				set_xml_property_copy(
					child_rsc->extra_attrs,
					XML_RSC_ATTR_INCARNATION, inc_num);
				set_xml_property_copy(
					child_rsc->extra_attrs,
					XML_RSC_ATTR_INCARNATION_MAX, inc_max);

				crm_xml_devel(child_rsc->extra_attrs,
					     "creating extra attributes");

				crm_devel_action(
					print_resource("Added", child_rsc, FALSE));

				crm_free(inc_num);
				
			} else {
				crm_err("Failed unpacking resource %s",
					crm_element_value(child_copy, XML_ATTR_ID));
			}
		}
		crm_free(inc_max);
		
		/* only count the first one */
		break;
		);
	
	crm_verbose("Added %d children to resource %s...",
		    incarnation_data->incarnation_max, rsc->id);
	
	rsc->variant_opaque = incarnation_data;
}



resource_t *
incarnation_find_child(resource_t *rsc, const char *id)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	if(rsc->variant == pe_incarnation) {
		incarnation_data = (incarnation_variant_data_t *)rsc->variant_opaque;
	} else {
		crm_err("Resource %s was not a \"incarnation\" variant", rsc->id);
		return NULL;
	}
	return pe_find_resource(incarnation_data->child_list, id);
}

int incarnation_num_allowed_nodes(resource_t *rsc)
{
	int num_nodes = 0;
	incarnation_variant_data_t *incarnation_data = NULL;
	if(rsc->variant == pe_incarnation) {
		incarnation_data = (incarnation_variant_data_t *)rsc->variant_opaque;
	} else {
		crm_err("Resource %s was not an \"incarnation\" variant",
			rsc->id);
		return 0;
	}

	/* what *should* we return here? */
	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,
		int tmp_num_nodes = child_rsc->fns->num_allowed_nodes(child_rsc);
		if(tmp_num_nodes > num_nodes) {
			num_nodes = tmp_num_nodes;
		}
		);

	return num_nodes;
}

void incarnation_color(resource_t *rsc, GListPtr *colors)
{
	int lpc = 0, lpc2 = 0, max_nodes = 0;
	resource_t *child_0  = NULL;
	resource_t *child_lh = NULL;
	resource_t *child_rh = NULL;
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	child_0 = g_list_nth_data(incarnation_data->child_list, 0);

	max_nodes = rsc->fns->num_allowed_nodes(rsc);

	/* generate up to max_nodes * incarnation_node_max constraints */
	lpc = 0;
	crm_info("Distributing %d incarnations over %d nodes",
		  incarnation_data->incarnation_max, max_nodes);

	for(; lpc < max_nodes && lpc < incarnation_data->incarnation_max; lpc++) {

		child_lh = child_0;
		incarnation_data->active_incarnation++;

		if(lpc != 0) {
			child_rh = g_list_nth_data(incarnation_data->child_list, lpc);
			
			crm_devel("Incarnation %d will run on a differnt node to 0",
				  lpc);
			
			rsc_colocation_new("pe_incarnation_internal_must_not",
					   pecs_must_not, child_lh, child_rh);
		} else {
			child_rh = child_0;
		}
		
		child_lh = child_rh;
		
		for(lpc2 = 1; lpc2 < incarnation_data->incarnation_max_node; lpc2++) {
			int offset = lpc + (lpc2 * max_nodes);
			if(offset >= incarnation_data->incarnation_max) {
				break;
			}
			crm_devel("Incarnation %d will run on the same node as %d",
				  offset, lpc);

			incarnation_data->active_incarnation++;

			child_rh = g_list_nth_data(
				incarnation_data->child_list, offset);

			rsc_colocation_new("pe_incarnation_internal_must",
					   pecs_must, child_lh, child_rh);
		}
	}

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,
		if(lpc < incarnation_data->active_incarnation) {
			crm_devel("Coloring Incarnation %d", lpc);
			child_rsc->fns->color(child_rsc, colors);
		} else {
			/* TODO: assign "no color"?  Doesnt seem to need it */
			crm_warn("Incarnation %d cannot be started", lpc);
		} 
		);
	crm_info("%d Incarnations are active", incarnation_data->active_incarnation);
}

void incarnation_create_actions(resource_t *rsc)
{
	gboolean child_starting = FALSE;
	gboolean child_stopping = FALSE;
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,
		child_rsc->fns->create_actions(child_rsc);
		child_starting = child_starting || child_rsc->starting;
		child_stopping = child_stopping || child_rsc->stopping;
		);

	if(child_starting) {
		rsc->starting = TRUE;
		action_new(incarnation_data->self, start_rsc, NULL);
		action_new(incarnation_data->self, started_rsc, NULL);
		
	}
	if(child_stopping) {
		rsc->stopping = TRUE;
		action_new(incarnation_data->self, stop_rsc, NULL);
		action_new(incarnation_data->self, stopped_rsc, NULL);
	}
	
	slist_iter(
		action, action_t, incarnation_data->self->actions, lpc,
		action->pseudo   = TRUE;
		);
}


void incarnation_internal_constraints(resource_t *rsc, GListPtr *ordering_constraints)
{
	resource_t *last_rsc = NULL;
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	/* global stop before start */
	order_new(incarnation_data->self, stop_rsc,  NULL,
		  incarnation_data->self, start_rsc, NULL,
		  pecs_startstop, ordering_constraints);
	
	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,

		/* child stop before start */
		order_new(child_rsc, stop_rsc,  NULL,
			  child_rsc, start_rsc, NULL,
			  pecs_startstop, ordering_constraints);
		
		if(incarnation_data->ordered && last_rsc != NULL) {
			crm_devel("Ordered version");
			if(lpc < incarnation_data->active_incarnation) {
				/* child/child relative start */
				order_new(last_rsc,  start_rsc, NULL,
					  child_rsc, start_rsc, NULL,
					  pecs_startstop, ordering_constraints);
			}
			
			/* child/child relative stop */
			order_new(child_rsc, stop_rsc, NULL,
				  last_rsc,  stop_rsc, NULL,
				  pecs_startstop, ordering_constraints);

		} else if(incarnation_data->ordered) {
			crm_devel("Ordered version (1st node)");

			/* child start before global started */
			order_new(child_rsc,              start_rsc, NULL,
				  incarnation_data->self, started_rsc, NULL,
				  pecs_startstop, ordering_constraints);
			
			/* first child stop before global stopped */
			order_new(child_rsc,              stop_rsc, NULL,
				  incarnation_data->self, stopped_rsc, NULL,
				  pecs_startstop, ordering_constraints);
			
			/* global start before first child start */
			order_new(incarnation_data->self, start_rsc, NULL,
				  child_rsc,              start_rsc, NULL,
				  pecs_startstop, ordering_constraints);

		} else {
			crm_devel("Un-ordered version");

			if(lpc < incarnation_data->active_incarnation) {
				/* child start before global started */
				order_new(child_rsc,              start_rsc, NULL,
					  incarnation_data->self, started_rsc, NULL,
					  pecs_startstop, ordering_constraints);
			}
		
			/* global start before child start */
			order_new(incarnation_data->self, start_rsc, NULL,
				  child_rsc,              start_rsc, NULL,
				  pecs_startstop, ordering_constraints);

			/* child stop before global stopped */
			order_new(child_rsc,              stop_rsc, NULL,
				  incarnation_data->self, stopped_rsc, NULL,
				  pecs_startstop, ordering_constraints);
			
			/* global stop before child stop */
			order_new(incarnation_data->self, stop_rsc, NULL,
				  child_rsc,               stop_rsc, NULL,
				  pecs_startstop, ordering_constraints);

		}

		if(lpc < incarnation_data->active_incarnation) {
			last_rsc = child_rsc;
		}
		
		);

	if(incarnation_data->ordered && last_rsc != NULL) {
		crm_devel("Ordered version (last node)");
		/* last child start before global started */
		order_new(last_rsc,         start_rsc, NULL,
			  incarnation_data->self, started_rsc, NULL,
			  pecs_startstop, ordering_constraints);

		/* global stop before first child stop */
		order_new(incarnation_data->self, stop_rsc, NULL,
			  last_rsc,         stop_rsc, NULL,
			  pecs_startstop, ordering_constraints);
	}
		
}

void incarnation_rsc_colocation_lh(rsc_colocation_t *constraint)
{
	resource_t *rsc = constraint->rsc_lh;
	incarnation_variant_data_t *incarnation_data = NULL;
	
	if(rsc == NULL) {
		crm_err("rsc_lh was NULL for %s", constraint->id);
		return;

	} else if(constraint->rsc_rh == NULL) {
		crm_err("rsc_rh was NULL for %s", constraint->id);
		return;
		
	} else if(constraint->strength != pecs_must_not) {
		crm_warn("rsc_dependancies other than \"must_not\" "
			 "are not supported for incarnation resources");
		return;
		
	} else {
		crm_devel("Processing constraints from %s", rsc->id);
	}
	
	get_incarnation_variant_data(incarnation_data, rsc);

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,
		
		crm_devel_action(print_resource("LHS", child_rsc, TRUE));
		child_rsc->fns->rsc_colocation_rh(child_rsc, constraint);
		);
}

void incarnation_rsc_colocation_rh(resource_t *rsc, rsc_colocation_t *constraint)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	
	crm_verbose("Processing RH of constraint %s", constraint->id);

	if(rsc == NULL) {
		crm_err("rsc_lh was NULL for %s", constraint->id);
		return;

	} else if(constraint->rsc_rh == NULL) {
		crm_err("rsc_rh was NULL for %s", constraint->id);
		return;
		
	} else if(constraint->strength != pecs_must_not) {
		crm_warn("rsc_dependancies other than \"must_not\" "
			 "are not supported for incarnation resources");
		return;
		
	} else {
		crm_devel_action(print_resource("LHS", rsc, FALSE));
	}
	
	get_incarnation_variant_data(incarnation_data, rsc);

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,
		
		crm_devel_action(print_resource("RHS", child_rsc, FALSE));
		child_rsc->fns->rsc_colocation_rh(child_rsc, constraint);
		);
}


void incarnation_rsc_order_lh(resource_t *rsc, order_constraint_t *order)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	crm_verbose("Processing LH of ordering constraint %d", order->id);

	if(order->lh_action_task == start_rsc) {
		order->lh_action_task = started_rsc;
		
	} else if(order->lh_action_task == stop_rsc) {
		order->lh_action_task = stopped_rsc;
	}
	
	incarnation_data->self->fns->rsc_order_lh(incarnation_data->self, order);
}

void incarnation_rsc_order_rh(
	action_t *lh_action, resource_t *rsc, order_constraint_t *order)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	crm_verbose("Processing RH of ordering constraint %d", order->id);

	incarnation_data->self->fns->rsc_order_rh(lh_action, incarnation_data->self, order);
}

void incarnation_rsc_location(resource_t *rsc, rsc_to_node_t *constraint)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	crm_verbose("Processing actions from %s", rsc->id);

	incarnation_data->self->fns->rsc_location(incarnation_data->self, constraint);
	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,

		child_rsc->fns->rsc_location(child_rsc, constraint);
		);
}

void incarnation_expand(resource_t *rsc, crm_data_t * *graph)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	crm_verbose("Processing actions from %s", rsc->id);

	incarnation_data->self->fns->expand(incarnation_data->self, graph);

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,

		child_rsc->fns->expand(child_rsc, graph);

		);
}

void incarnation_dump(resource_t *rsc, const char *pre_text, gboolean details)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	common_dump(rsc, pre_text, details);
	
	incarnation_data->self->fns->dump(
		incarnation_data->self, pre_text, details);

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,
		
		child_rsc->fns->dump(child_rsc, pre_text, details);
		);
}

void incarnation_free(resource_t *rsc)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	crm_verbose("Freeing %s", rsc->id);

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,

		crm_verbose("Freeing child %s", child_rsc->id);
		child_rsc->fns->free(child_rsc);
		);

	crm_verbose("Freeing child list");
	pe_free_shallow_adv(incarnation_data->child_list, FALSE);
	incarnation_data->self->fns->free(incarnation_data->self);

	common_free(rsc);
}


void
incarnation_agent_constraints(resource_t *rsc)
{
	incarnation_variant_data_t *incarnation_data = NULL;
	get_incarnation_variant_data(incarnation_data, rsc);

	slist_iter(
		child_rsc, resource_t, incarnation_data->child_list, lpc,
		
		child_rsc->fns->agent_constraints(child_rsc);
		);
}
