/* $Id: xml.h,v 1.13 2005/02/07 11:41:17 andrew Exp $ */
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
#ifndef CRM_COMMON_XML__H
#define CRM_COMMON_XML__H

#include <portability.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <ha_msg.h>
#include <clplumbing/cl_log.h> 

#define USE_LIBXML 1

#ifdef USE_LIBXML
#  include <libxml/tree.h> 
   typedef xmlNode crm_data_t;
#else
   typedef struct ha_msg crm_data_t;
#endif

gboolean add_message_xml(HA_Message *msg, const char *field, crm_data_t *xml);

/*
 * Replacement function for xmlCopyPropList which at the very least,
 * doesnt work the way *I* would expect it to.
 *
 * Copy all the attributes/properties from src into target.
 *
 * Not recursive, does not return anything. 
 *
 */
extern void copy_in_properties(crm_data_t *target, crm_data_t *src);

/*
 * Find a child named search_path[i] at level i in the XML fragment where i=0
 * is an immediate child of <i>root</i>.
 *
 * Terminate with success if i == len, or search_path[i] == NULL.
 *
 * On success, returns the sub-fragment described by search_path.
 * On failure, returns NULL.
 */
extern crm_data_t *find_xml_node_nested(
	crm_data_t *root, const char **search_path, int len);


/*
 * Find a child named search_path[i] at level i in the XML fragment where i=0
 * is an immediate child of <i>root</i>.
 *
 * Once the last child specified by node_path is found, find the value
 * of attr_name.
 *
 * If <i>error<i> is set to TRUE, then it is an error for the attribute not
 * to be found and the function will log accordingly.
 *
 * On success, returns the value of attr_name.
 * On failure, returns NULL.
 */
extern const char *get_xml_attr_nested(crm_data_t *parent,
				       const char **node_path, int length,
				       const char *attr_name, gboolean error);

/*
 * Free the XML "stuff" associated with a_node
 *
 * If a_node is part of a document, free the whole thing
 *
 * Otherwise, unlink it from its current location and free everything
 * from there down.
 *
 * Wont barf on NULL.
 *
 */
extern void free_xml_fn(crm_data_t *a_node);
#if 1
#  define free_xml(xml_obj) free_xml_fn(xml_obj); xml_obj = NULL
#else
#  define free_xml(xml_obj) xml_obj = NULL
#endif

/*
 * Create a node named "name" as a child of "parent"
 * If parent is NULL, creates an unconnected node.
 *
 * Returns the created node
 *
 */
extern crm_data_t *create_xml_node(crm_data_t *parent, const char *name);

/*
 * Make a copy of name and value and use the copied memory to create
 * an attribute for node.
 *
 * If node, name or value are NULL, nothing is done.
 *
 * If name or value are an empty string, nothing is done.
 *
 * Returns FALSE on failure and TRUE on success.
 *
 */
extern gboolean set_xml_property_copy(
	crm_data_t *node, const char *name, const char *value);

/*
 * Unlink the node and set its doc pointer to NULL so free_xml()
 * will act appropriately
 */
extern void unlink_xml_node(crm_data_t *node);

/*
 * Set a timestamp attribute on a_node
 */
extern void set_node_tstamp(crm_data_t *a_node);

/*
 * Returns a deep copy of src_node
 *
 * Either calls xmlCopyNode() or a home grown alternative (based on
 * XML_TRACE being defined) that does more logging...
 * helpful when part of the XML document has been freed :)
 */
extern crm_data_t *copy_xml_node_recursive(crm_data_t *src_node);

/*
 * Add a copy of xml_node to new_parent
 */
extern crm_data_t *add_node_copy(
	crm_data_t *new_parent, crm_data_t *xml_node);


/*
 * Read in the contents of a pre-opened file descriptor (until EOF) and
 * produce an XML fragment (it will have an attached document).
 *
 * input will need to be closed on completion.
 *
 * Whitespace between tags is discarded.
 *
 */
extern crm_data_t *file2xml(FILE *input);

/*
 * Read in the contents of a string and produce an XML fragment (it will
 * have an attached document).
 *
 * input will need to be freed on completion.
 *
 * Whitespace between tags is discarded.
 *
 */
extern crm_data_t *string2xml(const char *input);


/* convience "wrapper" functions */
extern crm_data_t *find_xml_node(
	crm_data_t *cib, const char * node_path, gboolean must_find);

extern crm_data_t *find_entity(crm_data_t *parent,
			      const char *node_name,
			      const char *id,
			      gboolean siblings);

extern int write_xml_file(crm_data_t *xml_node, const char *filename);

extern char *dump_xml_formatted(crm_data_t *msg);
extern char *dump_xml_unformatted(crm_data_t *msg);

extern void print_xml_formatted(
	int log_level, const char *function,
	crm_data_t *an_xml_node, const char *text);

#ifdef USE_LIBXML
#   define xml_remove_prop(obj, name) xmlUnsetProp(obj, name)
#   define xml_has_children(root) (root->children ? TRUE : FALSE)
#   define crm_element_value(data, name) xmlGetProp(data, name)
#   define crm_element_name(data) (data ? data->name : NULL)
#   define crm_element_parent(data, parent) parent=(data?data->parent:NULL)
#   define xml_child_iter(a,b,c,d) if(a != NULL) {			\
		crm_data_t *b = NULL;					\
		crm_data_t *__crm_xml_iter = a->children;		\
		while(__crm_xml_iter != NULL) {			\
			b = __crm_xml_iter;				\
			__crm_xml_iter = __crm_xml_iter->next;		\
			if(c == NULL || safe_str_eq(c, b->name)) {	\
				d;					\
			} else {					\
				crm_trace("Skipping <%s../>", b->name);	\
			}						\
		}							\
	} else {							\
		crm_trace("Parent of loop was NULL");			\
	}
#define xml_prop_iter(parent, prop_name, prop_value, code) if(parent != NULL) { \
		xmlAttrPtr prop_iter = parent->properties;		\
		while(prop_iter != NULL) {				\
			const char *prop_name = prop_iter->name;	\
			const char *prop_value =			\
				xmlGetProp(parent, prop_name);		\
			code;						\
			prop_iter = prop_iter->next;			\
		}							\
	} else {							\
		crm_trace("Parent of loop was NULL");			\
	}

#else
extern void crm_update_parents(crm_data_t *root);
extern gboolean crm_xml_has_children(crm_data_t *root);
#   define xml_remove_prop(obj, name) cl_msg_remove(obj, name); \
	 		crm_validate_data(obj)
#   define xml_has_children(root) crm_xml_has_children(root)
#   define xmlGetNodePath(data) cl_get_string(data, XML_ATTR_TAGNAME)
#   define crm_element_parent(data, parent) ha_msg_value_int(data, XML_ATTR_PARENT, (int*)parent)
#   define crm_set_element_parent(data, parent) if(parent != NULL) {	\
		ha_msg_mod_int(data, XML_ATTR_PARENT, (int)parent);	\
	} else {							\
		cl_msg_remove(data, XML_ATTR_PARENT);			\
	}
#   define crm_element_value(data, name) cl_get_string(data, name)
#   define crm_element_name(data) cl_get_string(data, XML_ATTR_TAGNAME) 
#   define xml_child_iter(a, b, c, d) if(a != NULL) {			\
		int __counter = 0;					\
		crm_data_t *b = NULL;					\
		for (__counter = 0; __counter < a->nfields; __counter++) { \
			if(a->types[__counter] != FT_STRUCT) {		\
				continue;				\
			}						\
			b = a->values[__counter];			\
			if(b == NULL) {				\
				crm_trace("Skipping %s == NULL", a->names[__counter]); \
			} else if(c == NULL || safe_str_eq(c, a->names[__counter])) { \
				d;					\
			} else {					\
				crm_trace("Skipping <%s../>", a->names[__counter]); \
			}						\
		}							\
	} else {							\
		crm_trace("Parent of loop was NULL");			\
	}
#define xml_prop_iter(parent, prop_name, prop_value, code) if(parent != NULL) { \
		const char *prop_name = NULL;				\
		const char *prop_value = NULL;				\
		int __counter = 0;					\
		crm_insane("Searching %d fields", parent->nfields);	\
		for (__counter = 0; __counter < parent->nfields; __counter++) { \
			crm_insane("Searching field %d", __counter);	\
			if(parent->types[__counter] != FT_STRING) {	\
				continue;				\
			}						\
			prop_name = parent->names[__counter];		\
			prop_value = parent->values[__counter];		\
			code;						\
		}							\
	} else {							\
		crm_trace("Parent of loop was NULL");			\
	}

extern void crm_validate_data(crm_data_t *root);
#endif


#endif
