/* $Id: pe_utils.h,v 1.10 2004/07/01 16:16:04 andrew Exp $ */
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
#ifndef PE_UTILS__H
#define PE_UTILS__H


// General utilities
extern resource_t *pe_find_resource(GListPtr rsc_list, const char *id_rh);

extern action_t *action_new(resource_t *rsc, enum action_tasks task);


// Constraint helper functions
extern rsc_to_rsc_t *invert_constraint(rsc_to_rsc_t *constraint);

extern rsc_to_node_t *copy_constraint(rsc_to_node_t *constraint);

// Color helper functions
extern void add_color_to_rsc(resource_t *rsc, color_t *color);

extern color_t *find_color(GListPtr candidate_colors, color_t *other_color);

extern color_t *create_color(
	GListPtr *colors, resource_t *resource, GListPtr resources);

// Node helper functions
extern gboolean filter_nodes(resource_t *rsc);

extern node_t *pe_find_node(GListPtr node_list, const char *uname);

extern node_t *pe_find_node_id(GListPtr node_list, const char *id);

extern node_t *node_copy(node_t *this_node) ;


// Binary like operators for lists of nodes
extern GListPtr node_list_dup(GListPtr list1, gboolean filter);

extern GListPtr node_list_and(GListPtr list1, GListPtr list2, gboolean filter);

extern GListPtr node_list_xor(GListPtr list1, GListPtr list2, gboolean filter);

extern GListPtr node_list_minus(GListPtr list1,GListPtr list2,gboolean filter);

extern gboolean node_list_eq(GListPtr list1, GListPtr list2, gboolean filter);

extern GListPtr node_list_or(GListPtr list1, GListPtr list2, gboolean filter);



// For creating the transition graph
extern xmlNodePtr action2xml(action_t *action);

// Printing functions for debug
extern void print_node(
	const char *pre_text, node_t *node, gboolean details);

extern void print_resource(
	const char *pre_text, resource_t *rsc, gboolean details);

extern void print_rsc_to_node(
	const char *pre_text, rsc_to_node_t *cons, gboolean details);

extern void print_rsc_to_rsc(
	const char *pre_text, rsc_to_rsc_t *cons, gboolean details);

extern void print_color(
	const char *pre_text, color_t *color, gboolean details);

extern void print_color_details(
	const char *pre_text, struct color_shared_s *color, gboolean details);

extern void print_action(
	const char *pre_text, action_t *action, gboolean details);

// Sorting functions
extern gint sort_rsc_priority(gconstpointer a, gconstpointer b);
extern gint sort_cons_strength(gconstpointer a, gconstpointer b);
extern gint sort_color_weight(gconstpointer a, gconstpointer b);
extern gint sort_node_weight(gconstpointer a, gconstpointer b);

// enum 2 text functions (mostly used by print_*)
extern const char *contype2text(enum con_type type);
extern const char *strength2text(enum con_strength strength);
extern const char *modifier2text(enum con_modifier modifier);
extern const char *task2text(enum action_tasks task);

// free the various structures
extern void pe_free_nodes(GListPtr nodes);
extern void pe_free_colors(GListPtr colors);
extern void pe_free_rsc_to_rsc(rsc_to_rsc_t *cons);
extern void pe_free_rsc_to_node(rsc_to_node_t *cons);
extern void pe_free_shallow(GListPtr alist);
extern void pe_free_shallow_adv(GListPtr alist, gboolean with_data);
extern void pe_free_resources(GListPtr resources);
extern void pe_free_actions(GListPtr actions);

// Helper macros to avoid NULL pointers
#define safe_val(def, x,y)          (x?x->y:def)
#define safe_val3(def, t,u,v)       (t?t->u?t->u->v:def:def)
#define safe_val4(def, t,u,v,w)     (t?t->u?t->u->v?t->u->v->w:def:def:def)
#define safe_val5(def, t,u,v,w,x)   (t?t->u?t->u->v?t->u->v->w?t->u->v->w->x:def:def:def:def)
#define safe_val6(def, t,u,v,w,x,y) (t?t->u?t->u->v?t->u->v->w?t->u->v->w->x?t->u->v->w->x->y:def:def:def:def:def)
#define safe_val7(def, t,u,v,w,x,y,z) (t?t->u?t->u->v?t->u->v->w?t->u->v->w->x?t->u->v->w->x->y?t->u->v->w->x->y->z:def:def:def:def:def:def)

#endif
