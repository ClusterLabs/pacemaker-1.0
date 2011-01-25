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
#ifndef PENGINE_COMPLEX__H
#define PENGINE_COMPLEX__H

#define n_object_classes 3

/*#define PE_OBJ_F_	""*/

#define PE_OBJ_T_NATIVE		"native"
#define PE_OBJ_T_GROUP		"group"
#define PE_OBJ_T_INCARNATION	"clone"
#define PE_OBJ_T_MASTER		"master"

enum pe_obj_types 
{
	pe_unknown = -1,
	pe_native = 0,
	pe_group = 1,
	pe_clone = 2,
	pe_master = 3
};

extern enum pe_obj_types get_resource_type(const char *name);
extern const char *get_resource_typename(enum pe_obj_types type);


typedef struct resource_object_functions_s 
{
		gboolean (*unpack)(resource_t *, pe_working_set_t *);
		resource_t *(*find_rsc)(resource_t *parent, const char *search, node_t *node, int flags);
		/* parameter result must be free'd */
		char *(*parameter)(
			resource_t *, node_t *, gboolean, const char *,
			pe_working_set_t *);
		void (*print)(resource_t *, const char *, long, void *);
		gboolean (*active)(resource_t *,gboolean);
		enum rsc_role_e (*state)(const resource_t *, gboolean);
		node_t *(*location)(resource_t *, GListPtr*, gboolean);
		void (*free)(resource_t *);
} resource_object_functions_t;

extern void common_update_score(resource_t *rsc, const char *id, int score);

extern char *native_parameter(
	resource_t *rsc, node_t *node, gboolean create, const char *name,
	pe_working_set_t *data_set);

extern gboolean native_unpack(resource_t *rsc, pe_working_set_t *data_set);
extern gboolean group_unpack(resource_t *rsc, pe_working_set_t *data_set);
extern gboolean clone_unpack(resource_t *rsc, pe_working_set_t *data_set);
extern gboolean master_unpack(resource_t *rsc, pe_working_set_t *data_set);

extern resource_t *native_find_rsc(
    resource_t *rsc, const char *id, node_t *node, int flags);

extern gboolean native_active(resource_t *rsc, gboolean all);
extern gboolean group_active(resource_t *rsc, gboolean all);
extern gboolean clone_active(resource_t *rsc, gboolean all);
extern gboolean master_active(resource_t *rsc, gboolean all);

extern void native_print(
	resource_t *rsc, const char *pre_text, long options, void *print_data);
extern void group_print(
	resource_t *rsc, const char *pre_text, long options, void *print_data);
extern void clone_print(
	resource_t *rsc, const char *pre_text, long options, void *print_data);
extern void master_print(
	resource_t *rsc, const char *pre_text, long options, void *print_data);

extern void native_free(resource_t *rsc);
extern void group_free(resource_t *rsc);
extern void clone_free(resource_t *rsc);
extern void master_free(resource_t *rsc);

extern enum rsc_role_e native_resource_state(const resource_t *rsc, gboolean current);
extern enum rsc_role_e group_resource_state(const resource_t *rsc, gboolean current);
extern enum rsc_role_e clone_resource_state(const resource_t *rsc, gboolean current);
extern enum rsc_role_e master_resource_state(const resource_t *rsc, gboolean current);

extern node_t *native_location(resource_t *rsc, GListPtr *list, gboolean current);

extern resource_object_functions_t resource_class_functions[];
extern gboolean	common_unpack(xmlNode * xml_obj, resource_t **rsc,
			      resource_t *parent, pe_working_set_t *data_set);

extern void common_print(resource_t *rsc, const char *pre_text, long options, void *print_data);

extern void common_free(resource_t *rsc);
extern void native_add_running(
	resource_t *rsc, node_t *node, pe_working_set_t *data_set);
extern void get_meta_attributes(GHashTable *meta_hash, resource_t *rsc,
				node_t *node, pe_working_set_t *data_set);
extern void get_rsc_attributes(GHashTable *meta_hash, resource_t *rsc,
				node_t *node, pe_working_set_t *data_set);

typedef struct resource_alloc_functions_s resource_alloc_functions_t;
extern resource_t *uber_parent(resource_t *rsc);
extern node_t *rsc_known_on(resource_t *rsc, GListPtr *list);

#endif
