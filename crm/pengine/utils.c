/* $Id: utils.c,v 1.54 2005/02/18 10:32:37 andrew Exp $ */
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
#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/common/util.h>

#include <glib.h>

#include <pengine.h>
#include <pe_utils.h>

int action_id = 1;

void print_str_str(gpointer key, gpointer value, gpointer user_data);
gboolean ghash_free_str_str(gpointer key, gpointer value, gpointer user_data);
gboolean node_merge_weights(node_t *node, node_t *with);

/* only for rsc_dependancy constraints */
rsc_dependancy_t *
invert_constraint(rsc_dependancy_t *constraint) 
{
	rsc_dependancy_t *inverted_con = NULL;

	crm_verbose("Inverting constraint");
	if(constraint == NULL) {
		crm_err("Cannot invert NULL constraint");
		return NULL;
	}

	crm_malloc(inverted_con, sizeof(rsc_dependancy_t));

	if(inverted_con == NULL) {
		return NULL;
	}
	
	inverted_con->id = crm_strdup(constraint->id);
	inverted_con->strength = constraint->strength;

	/* swap the direction */
	inverted_con->rsc_lh = constraint->rsc_rh;
	inverted_con->rsc_rh = constraint->rsc_lh;

	crm_debug_action(
		print_rsc_dependancy("Inverted constraint", inverted_con, FALSE));
	
	return inverted_con;
}


/* are the contents of list1 and list2 equal 
 * nodes with weight < 0 are ignored if filter == TRUE
 *
 * slow but linear
 *
 */
gboolean
node_list_eq(GListPtr list1, GListPtr list2, gboolean filter)
{
	node_t *other_node;

	GListPtr lhs = list1;
	GListPtr rhs = list2;
	
	slist_iter(
		node, node_t, lhs, lpc,

		if(node == NULL || (filter && node->weight < 0)) {
			continue;
		}

		other_node = (node_t*)
			pe_find_node(rhs, node->details->uname);

		if(other_node == NULL || other_node->weight < 0) {
			return FALSE;
		}
		);
	
	lhs = list2;
	rhs = list1;

	slist_iter(
		node, node_t, lhs, lpc,

		if(node == NULL || (filter && node->weight < 0)) {
			continue;
		}

		other_node = (node_t*)
			pe_find_node(rhs, node->details->uname);

		if(other_node == NULL || other_node->weight < 0) {
			return FALSE;
		}
		);
  
	return TRUE;
}

/* the intersection of list1 and list2 
 */
GListPtr
node_list_and(GListPtr list1, GListPtr list2, gboolean filter)
{
	GListPtr result = NULL;
	int lpc = 0;

	for(lpc = 0; lpc < g_list_length(list1); lpc++) {
		node_t *node = (node_t*)g_list_nth_data(list1, lpc);
		node_t *new_node = node_copy(node);
		node_t *other_node = pe_find_node(list2, node->details->uname);

		if(node_merge_weights(new_node, other_node) == FALSE) {
			crm_free(new_node);

		} else if(filter && new_node->weight < 0) {
			crm_free(new_node);

		} else {
			result = g_list_append(result, new_node);
		}
	}

	return result;
}


gboolean
node_merge_weights(node_t *node, node_t *with)
{
	if(node == NULL || with == NULL) {
		return FALSE;
	} else if(node->weight < 0 || with->weight < 0) {
		node->weight = -1;
	} else if(node->weight < with->weight) {
		node->weight = with->weight;
	}
	return TRUE;
}


/* list1 - list2 */
GListPtr
node_list_minus(GListPtr list1, GListPtr list2, gboolean filter)
{
	GListPtr result = NULL;

	slist_iter(
		node, node_t, list1, lpc,
		node_t *other_node = pe_find_node(list2, node->details->uname);
		node_t *new_node = NULL;
		
		if(node == NULL || other_node != NULL
		   || (filter && node->weight < 0)) {
			continue;
			
		}
		new_node = node_copy(node);
		result = g_list_append(result, new_node);
		);
  
	crm_verbose("Minus result len: %d", g_list_length(result));

	return result;
}

/* list1 + list2 - (intersection of list1 and list2) */
GListPtr
node_list_xor(GListPtr list1, GListPtr list2, gboolean filter)
{
	GListPtr result = NULL;
	
	slist_iter(
		node, node_t, list1, lpc,
		node_t *new_node = NULL;
		node_t *other_node = (node_t*)
			pe_find_node(list2, node->details->uname);

		if(node == NULL || other_node != NULL
		   || (filter && node->weight < 0)) {
			continue;
		}
		new_node = node_copy(node);
		result = g_list_append(result, new_node);
		);
	
 
	slist_iter(
		node, node_t, list2, lpc,
		node_t *new_node = NULL;
		node_t *other_node = (node_t*)
			pe_find_node(list1, node->details->uname);

		if(node == NULL || other_node != NULL
		   || (filter && node->weight < 0)) {
			continue;
		}
		new_node = node_copy(node);
		result = g_list_append(result, new_node);
		);
  
	crm_verbose("Xor result len: %d", g_list_length(result));
	return result;
}

GListPtr
node_list_or(GListPtr list1, GListPtr list2, gboolean filter)
{
	node_t *other_node = NULL;
	GListPtr result = NULL;

	result = node_list_dup(list1, filter);

	slist_iter(
		node, node_t, list2, lpc,

		if(node == NULL) {
			continue;
		}

		other_node = (node_t*)pe_find_node(
			result, node->details->uname);
		
		if(other_node != NULL) {
			node_merge_weights(other_node, node);

			if(filter && node->weight < 0) {
				/* TODO: remove and free other_node */
			}
			
		} else if(filter && node->weight < 0) {
				  
		} else {
			node_t *new_node = node_copy(node);
			result = g_list_append(result, new_node);
		}
		);

	return result;
}

GListPtr 
node_list_dup(GListPtr list1, gboolean filter)
{
	GListPtr result = NULL;

	slist_iter(
		this_node, node_t, list1, lpc,
		node_t *new_node = NULL;
		if(filter && this_node->weight < 0) {
			continue;
		}
		
		new_node = node_copy(this_node);
		if(new_node != NULL) {
			result = g_list_append(result, new_node);
		}
		);

	return result;
}

node_t *
node_copy(node_t *this_node) 
{
	node_t *new_node  = NULL;

	if(this_node == NULL) {
		crm_err("Failed copy of <null> node.");
		return NULL;
	}
	crm_malloc(new_node, sizeof(node_t));

	if(new_node == NULL) {
		return NULL;
	}
	
	crm_trace("Copying %p (%s) to %p",
		  this_node, this_node->details->uname, new_node);
	new_node->weight  = this_node->weight; 
	new_node->fixed   = this_node->fixed;
	new_node->details = this_node->details; 
	
	return new_node;
}

static int color_id = 0;

/*
 * Create a new color with the contents of "nodes" as the list of
 *  possible nodes that resources with this color can be run on.
 *
 * Typically, when creating a color you will provide the node list from
 *  the resource you will first assign the color to.
 *
 * If "colors" != NULL, it will be added to that list
 * If "resources" != NULL, it will be added to every provisional resource
 *  in that list
 */
color_t *
create_color(GListPtr *colors, resource_t *resource, GListPtr node_list)
{
	color_t *new_color = NULL;
	
	crm_trace("Creating color");
	crm_malloc(new_color, sizeof(color_t));
	if(new_color == NULL) {
		return NULL;
	}
	
	new_color->id           = color_id++;
	new_color->local_weight = 1.0;
	
	crm_trace("Creating color details");
	crm_malloc(new_color->details, sizeof(struct color_shared_s));

	if(new_color->details == NULL) {
		crm_free(new_color);
		return NULL;
	}
		
	new_color->details->id                  = new_color->id;
	new_color->details->highest_priority    = -1;
	new_color->details->chosen_node         = NULL;
	new_color->details->candidate_nodes     = NULL;
	new_color->details->allocated_resources = NULL;
	new_color->details->pending             = TRUE;
	
	if(resource != NULL) {
		crm_trace("populating node list");
		new_color->details->highest_priority = resource->priority;
		new_color->details->candidate_nodes  =
			node_list_dup(node_list, TRUE);
	}
	
	crm_debug_action(print_color("Created color", new_color, TRUE));

	if(colors != NULL) {
		*colors = g_list_append(*colors, new_color);      
	}
	
	return new_color;
}

color_t *
copy_color(color_t *a_color) 
{
	color_t *color_copy = NULL;

	if(a_color == NULL) {
		crm_err("Cannot copy NULL");
		return NULL;
	}
	
	crm_malloc(color_copy, sizeof(color_t));
	if(color_copy != NULL) {
		color_copy->id      = a_color->id;
		color_copy->details = a_color->details;
		color_copy->local_weight = 1.0;
	}
	return color_copy;
}



resource_t *
pe_find_resource(GListPtr rsc_list, const char *id)
{
	int lpc = 0;
	resource_t *rsc = NULL;
	resource_t *child_rsc = NULL;

	for(lpc = 0; lpc < g_list_length(rsc_list); lpc++) {
		rsc = g_list_nth_data(rsc_list, lpc);
		if(rsc != NULL && safe_str_eq(rsc->id, id)){
			crm_debug("Found a match for %s", id);
			return rsc;
		}
	}
	for(lpc = 0; lpc < g_list_length(rsc_list); lpc++) {
		rsc = g_list_nth_data(rsc_list, lpc);

		child_rsc = rsc->fns->find_child(rsc, id);
		if(child_rsc != NULL) {
			crm_debug("Found a match for %s in %s",
				  id, rsc->id);
			
			return child_rsc;
		}
	}
	/* error */
	return NULL;
}


node_t *
pe_find_node(GListPtr nodes, const char *uname)
{
	int lpc = 0;
	node_t *node = NULL;
  
	for(lpc = 0; lpc < g_list_length(nodes); lpc++) {
		node = g_list_nth_data(nodes, lpc);
		if(node != NULL && safe_str_eq(node->details->uname, uname)) {
			return node;
		}
	}
	/* error */
	return NULL;
}

node_t *
pe_find_node_id(GListPtr nodes, const char *id)
{
	int lpc = 0;
	node_t *node = NULL;
  
	for(lpc = 0; lpc < g_list_length(nodes); lpc++) {
		node = g_list_nth_data(nodes, lpc);
		if(safe_str_eq(node->details->id, id)) {
			return node;
		}
	}
	/* error */
	return NULL;
}

gint gslist_color_compare(gconstpointer a, gconstpointer b);
color_t *
find_color(GListPtr candidate_colors, color_t *other_color)
{
	GListPtr tmp = g_list_find_custom(candidate_colors, other_color,
					    gslist_color_compare);
	if(tmp != NULL) {
		return (color_t *)tmp->data;
	}
	return NULL;
}


gint gslist_color_compare(gconstpointer a, gconstpointer b)
{
	const color_t *color_a = (const color_t*)a;
	const color_t *color_b = (const color_t*)b;

/*	crm_trace("%d vs. %d", a?color_a->id:-2, b?color_b->id:-2); */
	if(a == b) {
		return 0;
	} else if(a == NULL || b == NULL) {
		return 1;
	} else if(color_a->id == color_b->id) {
		return 0;
	}
	return 1;
}



gint sort_rsc_priority(gconstpointer a, gconstpointer b)
{
	const resource_t *resource1 = (const resource_t*)a;
	const resource_t *resource2 = (const resource_t*)b;

	if(a == NULL) { return 1; }
	if(b == NULL) { return -1; }
  
	if(resource1->priority > resource2->priority) {
		return -1;
	}
	
	if(resource1->priority < resource2->priority) {
		return 1;
	}

	return 0;
}

gint sort_cons_strength(gconstpointer a, gconstpointer b)
{
	const rsc_dependancy_t *rsc_constraint1 = (const rsc_dependancy_t*)a;
	const rsc_dependancy_t *rsc_constraint2 = (const rsc_dependancy_t*)b;

	if(a == NULL) { return 1; }
	if(b == NULL) { return -1; }
  
	if(rsc_constraint1->strength > rsc_constraint2->strength) {
		return 1;
	}
	
	if(rsc_constraint1->strength < rsc_constraint2->strength) {
		return -1;
	}
	return 0;
}

gint sort_color_weight(gconstpointer a, gconstpointer b)
{
	const color_t *color1 = (const color_t*)a;
	const color_t *color2 = (const color_t*)b;

	if(a == NULL) { return 1; }
	if(b == NULL) { return -1; }
  
	if(color1->local_weight > color2->local_weight) {
		return -1;
	}
	
	if(color1->local_weight < color2->local_weight) {
		return 1;
	}
	
	return 0;
}

gint sort_node_weight(gconstpointer a, gconstpointer b)
{
	const node_t *node1 = (const node_t*)a;
	const node_t *node2 = (const node_t*)b;

	if(a == NULL) { return 1; }
	if(b == NULL) { return -1; }
	
	if(node1->weight > node2->weight) {
		return -1;
	}
	
	if(node1->weight < node2->weight) {
		return 1;
	}

	return 0;
}


action_t *
action_new(resource_t *rsc, enum action_tasks task, node_t *on_node)
{
	action_t *action = NULL;

	GListPtr possible_matches = NULL;
	if(rsc != NULL) {
		possible_matches =
			find_actions(rsc->actions, task, on_node);
	}

	if(on_node != NULL && on_node->details->unclean) {
		crm_warn("Not creating action  %s for %s because %s is unclean",
			 task2text(task), rsc?rsc->id:"<NULL>",
			 on_node->details->id);
	}

	if(on_node != NULL && on_node->details->unclean) {
		crm_warn("Not creating action  %s for %s because %s is unclean",
			 task2text(task), rsc?rsc->id:"<NULL>",
			 on_node->details->id);
	}
	
	if(possible_matches != NULL) {

		if(g_list_length(possible_matches) > 1) {
			crm_err("Action %s for %s on %s exists %d times",
				task2text(task), rsc?rsc->id:"<NULL>",
				on_node?on_node->details->id:"<NULL>",
				g_list_length(possible_matches));
		}
		
		action = g_list_nth_data(possible_matches, 0);
		crm_debug("Returning existing action (%d) %s for %s on %s",
			  action->id,
			  task2text(task), rsc?rsc->id:"<NULL>",
			  on_node?on_node->details->id:"<NULL>");

		/* todo: free possible_matches */
		return action;
	}
	
	crm_debug("Creating action %s for %s on %s",
		  task2text(task), rsc?rsc->id:"<NULL>",
		  on_node?on_node->details->id:"<NULL>");

	crm_malloc(action, sizeof(action_t));
	if(action != NULL) {
		action->id   = action_id++;
		action->rsc  = rsc;
		action->task = task;
		action->node = on_node;
		action->actions_before   = NULL;
		action->actions_after    = NULL;
		action->failure_is_fatal = TRUE;
		action->pseudo = FALSE;
		action->dumped     = FALSE;
		action->discard    = FALSE;
		action->runnable   = TRUE;
		action->processed  = FALSE;
		action->optional   = FALSE;
		action->seen_count = 0;
		action->timeout    = NULL;
		action->args       = create_xml_node(NULL, "args");
		
		if(rsc != NULL) {
			crm_debug("Adding created action to its resource");
			rsc->actions = g_list_append(rsc->actions, action);
			if(task == start_rsc) {
				rsc->starting = TRUE;
				action->timeout = rsc->start_timeout;

			} else if(task == stop_rsc) {
				rsc->stopping = TRUE;
				action->timeout = rsc->stop_timeout;

			} else {
				action->timeout = rsc->def_timeout;
			}
			
		}

	}
	crm_debug("Action %d created", action->id);
	return action;
}

const char *
strength2text(enum con_strength strength)
{
	const char *result = "<unknown>";
	switch(strength)
	{
		case pecs_ignore:
			result = "ignore";
			break;
		case pecs_must:
			result = XML_STRENGTH_VAL_MUST;
			break;
		case pecs_must_not:
			result = XML_STRENGTH_VAL_MUSTNOT;
			break;
		case pecs_startstop:
			result = "start/stop";
			break;
	}
	return result;
}



const char *
task2text(enum action_tasks task)
{
	const char *result = "<unknown>";
	switch(task)
	{
		case no_action:
			result = "no_action";
			break;
		case stop_rsc:
			result = CRMD_RSCSTATE_STOP;
			break;
		case stopped_rsc:
			result = CRMD_RSCSTATE_STOP_OK;
			break;
		case start_rsc:
			result = CRMD_RSCSTATE_START;
			break;
		case started_rsc:
			result = CRMD_RSCSTATE_START_OK;
			break;
		case shutdown_crm:
			result = CRM_OP_SHUTDOWN;
			break;
		case stonith_node:
			result = XML_CIB_ATTR_STONITH;
			break;
	}
	
	return result;
}


void
print_node(const char *pre_text, node_t *node, gboolean details)
{ 
	if(node == NULL) {
		crm_debug("%s%s: <NULL>",
		       pre_text==NULL?"":pre_text,
		       pre_text==NULL?"":": ");
		return;
	}

	crm_debug("%s%s%sNode %s: (weight=%f, fixed=%s)",
	       pre_text==NULL?"":pre_text,
	       pre_text==NULL?"":": ",
	       node->details==NULL?"error ":node->details->online?"":"Unavailable/Unclean ",
	       node->details->uname, 
	       node->weight,
	       node->fixed?"True":"False"); 

	if(details && node->details != NULL) {
		char *pe_mutable = crm_strdup("\t\t");
		crm_debug("\t\t===Node Attributes");
		g_hash_table_foreach(node->details->attrs,
				     print_str_str, pe_mutable);
		crm_free(pe_mutable);
	}

	if(details) {
		crm_debug("\t\t===Node Attributes");
		slist_iter(
			rsc, resource_t, node->details->running_rsc, lpc,
			print_resource("\t\t", rsc, FALSE);
			);
	}
	
}

/*
 * Used by the HashTable for-loop
 */
void print_str_str(gpointer key, gpointer value, gpointer user_data)
{
	crm_debug("%s%s %s ==> %s",
	       user_data==NULL?"":(char*)user_data,
	       user_data==NULL?"":": ",
	       (char*)key,
	       (char*)value);
}

void
print_color_details(const char *pre_text,
		    struct color_shared_s *color,
		    gboolean details)
{ 
	if(color == NULL) {
		crm_debug("%s%s: <NULL>",
		       pre_text==NULL?"":pre_text,
		       pre_text==NULL?"":": ");
		return;
	}
	crm_debug("%s%sColor %d: node=%s (from %d candidates)",
	       pre_text==NULL?"":pre_text,
	       pre_text==NULL?"":": ",
	       color->id, 
	       color->chosen_node==NULL?"<unset>":color->chosen_node->details->uname,
	       g_list_length(color->candidate_nodes)); 
	if(details) {
		slist_iter(node, node_t, color->candidate_nodes, lpc,
			   print_node("\t", node, FALSE));
	}
}

void
print_color(const char *pre_text, color_t *color, gboolean details)
{ 
	if(color == NULL) {
		crm_debug("%s%s: <NULL>",
		       pre_text==NULL?"":pre_text,
		       pre_text==NULL?"":": ");
		return;
	}
	crm_debug("%s%sColor %d: (weight=%f, node=%s, possible=%d)",
	       pre_text==NULL?"":pre_text,
	       pre_text==NULL?"":": ",
	       color->id, 
	       color->local_weight,
		  safe_val5("<unset>",color,details,chosen_node,details,uname),
	       g_list_length(color->details->candidate_nodes)); 
	if(details) {
		print_color_details("\t", color->details, details);
	}
}

void
print_rsc_to_node(const char *pre_text, rsc_to_node_t *cons, gboolean details)
{ 
	if(cons == NULL) {
		crm_debug("%s%s: <NULL>",
		       pre_text==NULL?"":pre_text,
		       pre_text==NULL?"":": ");
		return;
	}
	crm_debug("%s%s%s Constraint %s (%p) - %d nodes:",
	       pre_text==NULL?"":pre_text,
	       pre_text==NULL?"":": ",
	       "rsc_to_node",
		  cons->id, cons,
		  g_list_length(cons->node_list_rh));

	if(details == FALSE) {
		crm_debug("\t%s %s run (score=%f : node placement rule)",
			  safe_val3(NULL, cons, rsc_lh, id), 
			  cons->can?"Can":"Cannot",
			  cons->weight);

		slist_iter(
			node, node_t, cons->node_list_rh, lpc,
			print_node("\t\t-->", node, FALSE)
			);
	}
}

void
print_rsc_dependancy(const char *pre_text, rsc_dependancy_t *cons, gboolean details)
{ 
	if(cons == NULL) {
		crm_debug("%s%s: <NULL>",
		       pre_text==NULL?"":pre_text,
		       pre_text==NULL?"":": ");
		return;
	}
	crm_debug("%s%s%s Constraint %s (%p):",
	       pre_text==NULL?"":pre_text,
	       pre_text==NULL?"":": ",
	       XML_CONS_TAG_RSC_DEPEND, cons->id, cons);

	if(details == FALSE) {

		crm_debug("\t%s --> %s, %s",
			  safe_val3(NULL, cons, rsc_lh, id), 
			  safe_val3(NULL, cons, rsc_rh, id), 
			  strength2text(cons->strength));
	}
} 

void
print_resource(const char *pre_text, resource_t *rsc, gboolean details)
{ 
	if(rsc == NULL) {
		crm_debug("%s%s: <NULL>",
		       pre_text==NULL?"":pre_text,
		       pre_text==NULL?"":": ");
		return;
	}
	rsc->fns->dump(rsc, pre_text, details);
}



void
print_action(const char *pre_text, action_t *action, gboolean details)
{ 
	if(action == NULL) {
		crm_debug("%s%s: <NULL>",
		       pre_text==NULL?"":pre_text,
		       pre_text==NULL?"":": ");
		return;
	}

	switch(action->task) {
		case stonith_node:
		case shutdown_crm:
			crm_debug("%s%s%sAction %d: %s @ %s",
			       pre_text==NULL?"":pre_text,
			       pre_text==NULL?"":": ",
			       action->discard?"Discarded ":action->optional?"Optional ":action->runnable?action->processed?"":"(Provisional) ":"!!Non-Startable!! ",
			       action->id,
			       task2text(action->task),
			       safe_val4(NULL, action, node, details, uname));
			break;
		default:
			crm_debug("%s%s%sAction %d: %s %s @ %s",
			       pre_text==NULL?"":pre_text,
			       pre_text==NULL?"":": ",
			       action->optional?"Optional ":action->runnable?action->processed?"":"(Provisional) ":"!!Non-Startable!! ",
			       action->id,
			       task2text(action->task),
			       safe_val3(NULL, action, rsc, id),
			       safe_val4(NULL, action, node, details, uname));
			
			break;
	}

	if(details) {
#if 1
		crm_debug("\t\t====== Preceeding Actions");
		slist_iter(
			other, action_wrapper_t, action->actions_before, lpc,
			print_action("\t\t", other->action, FALSE);
			);
		crm_debug("\t\t====== Subsequent Actions");
		slist_iter(
			other, action_wrapper_t, action->actions_after, lpc,
			print_action("\t\t", other->action, FALSE);
			);		
#else
		crm_debug("\t\t====== Subsequent Actions");
		slist_iter(
			other, action_wrapper_t, action->actions_after, lpc,
			print_action("\t\t", other->action, FALSE);
			);		
#endif
		crm_debug("\t\t====== End");

	} else {
		crm_debug("\t\t(seen=%d, before=%d, after=%d)",
		       action->seen_count,
		       g_list_length(action->actions_before),
		       g_list_length(action->actions_after));
	}
}


void
pe_free_nodes(GListPtr nodes)
{
	while(nodes != NULL) {
		GListPtr list_item = nodes;
		node_t *node = (node_t*)list_item->data;
		struct node_shared_s *details = node->details;
		nodes = nodes->next;

		crm_trace("deleting node");
		crm_trace("%s is being deleted", details->uname);
		print_node("delete", node, FALSE);
		
		if(details != NULL) {
			if(details->attrs != NULL) {
				g_hash_table_foreach_remove(details->attrs,
							    ghash_free_str_str,
							    NULL);

				g_hash_table_destroy(details->attrs);
			}
			
		}
		
	}
	if(nodes != NULL) {
		g_list_free(nodes);
	}
}

gboolean ghash_free_str_str(gpointer key, gpointer value, gpointer user_data)
{
	crm_free(key);
	crm_free(value);
	return TRUE;
}


void
pe_free_colors(GListPtr colors)
{
	while(colors != NULL) {
		GListPtr list_item = colors;
		color_t *color = (color_t *)list_item->data;
		struct color_shared_s *details = color->details;
		colors = colors->next;
		
		if(details != NULL) {
			pe_free_shallow(details->candidate_nodes);
			pe_free_shallow_adv(details->allocated_resources, FALSE);
			crm_free(details->chosen_node);
			crm_free(details);
		}
		crm_free(color);
	}
	if(colors != NULL) {
		g_list_free(colors);
	}
}

void
pe_free_shallow(GListPtr alist)
{
	pe_free_shallow_adv(alist, TRUE);
}

void
pe_free_shallow_adv(GListPtr alist, gboolean with_data)
{
	GListPtr item;
	GListPtr item_next = alist;
	while(item_next != NULL) {
		item = item_next;
		item_next = item_next->next;
		
		if(with_data) {
/*			crm_trace("freeing %p", item->data); */
			crm_free(item->data);
		}
		
		item->data = NULL;
		item->next = NULL;
		g_list_free(item);
	}
}

void
pe_free_resources(GListPtr resources)
{ 
	volatile GListPtr list_item = NULL;
	resource_t *rsc = NULL;
	
	while(resources != NULL) {
		list_item = resources;
		rsc = (resource_t *)list_item->data;
		resources = resources->next;

		pe_free_shallow_adv(rsc->candidate_colors, TRUE);
		rsc->fns->free(rsc);
	}
	if(resources != NULL) {
		g_list_free(resources);
	}
}


void
pe_free_actions(GListPtr actions) 
{
	while(actions != NULL) {
		GListPtr list_item = actions;
		action_t *action = (action_t *)list_item->data;
		actions = actions->next;

		pe_free_shallow(action->actions_before);/* action_warpper_t* */
		pe_free_shallow(action->actions_after); /* action_warpper_t* */
		action->actions_before = NULL;
		action->actions_after  = NULL;
		free_xml(action->args);
		crm_free(action);
	}
	if(actions != NULL) {
		g_list_free(actions);
	}
}



void
pe_free_rsc_dependancy(rsc_dependancy_t *cons)
{ 
	if(cons != NULL) {
		crm_debug("Freeing constraint %s (%p)", cons->id, cons);
		crm_free(cons);
	}
}

void
pe_free_rsc_to_node(rsc_to_node_t *cons)
{
	if(cons != NULL) {

		/* right now we dont make copies so this isnt required */
/*		pe_free_shallow(cons->node_list_rh); */ /* node_t* */
		crm_free(cons);
	}
}

GListPtr
find_actions(GListPtr input, enum action_tasks task, node_t *on_node)
{
	GListPtr result = NULL;
	
	slist_iter(
		action, action_t, input, lpc,
		if(action->task == task) {
			if(on_node == NULL) {
				result = g_list_append(result, action);

			} else if(action->node == NULL) {
				/* skip */
				crm_warn("While looking for %s action on %s, "
					  "found an unallocated one.  "
					  "This could end up creating dups..",
					  task2text(task),
					  on_node->details->id);
				
			} else if(safe_str_eq(on_node->details->id,
					      action->node->details->id)) {
				result = g_list_append(result, action);
			}
		}
		);

	return result;
}

void
set_id(crm_data_t * xml_obj, const char *prefix, int child) 
{
	int id_len = 0;
	gboolean use_prefix = TRUE;
	gboolean use_child = TRUE;

	char *new_id   = NULL;
	const char *id = crm_element_value(xml_obj, XML_ATTR_ID);
	
	id_len = 1 + strlen(id);

	if(child > 999) {
		crm_err("Are you insane?!?"
			" The CRM does not support > 1000 children per resource");
		return;
		
	} else if(child < 0) {
		use_child = FALSE;
		
	} else {
		id_len += 4; /* child */
	}
	
	if(prefix == NULL || safe_str_eq(id, prefix)) {
		use_prefix = FALSE;
	} else {
		id_len += (1 + strlen(prefix));
	}
	
	crm_malloc(new_id, id_len);

	if(use_child) {
		snprintf(new_id, id_len, "%s%s%s:%d",
			 use_prefix?prefix:"", use_prefix?":":"", id, child);
	} else {
		snprintf(new_id, id_len, "%s%s%s",
			 use_prefix?prefix:"", use_prefix?":":"", id);
	}
	
	set_xml_property_copy(xml_obj, XML_ATTR_ID, new_id);
	crm_free(new_id);
}
