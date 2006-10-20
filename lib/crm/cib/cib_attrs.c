/* $Id: cib_attrs.c,v 1.24 2006/04/18 11:28:56 andrew Exp $ */

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

#include <sys/param.h>

#include <crm/crm.h>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/cib.h>

#include <crm/dmalloc_wrapper.h>

#define attr_common_setup(section)					\
	gboolean is_crm_config = FALSE;					\
	gboolean is_node_transient = FALSE;				\
	char *local_set_name = NULL;					\
	if(attr_id == NULL && attr_name == NULL) {			\
		return cib_missing;					\
									\
	} else if(safe_str_eq(section, XML_CIB_TAG_CRMCONFIG)) {	\
		tag = NULL;						\
		is_crm_config = TRUE;					\
		if(set_name == NULL) {					\
			set_name = CIB_OPTIONS_FIRST;			\
		}							\
									\
	} else if(safe_str_eq(section, XML_CIB_TAG_NODES)) {		\
		tag = XML_CIB_TAG_NODE;					\
		if(node_uuid == NULL) {					\
			return cib_missing;				\
		}							\
		if(set_name == NULL) {					\
			local_set_name = crm_concat(section, node_uuid, '-'); \
			set_name = local_set_name;			\
		}							\
									\
	} else if(safe_str_eq(section, XML_CIB_TAG_STATUS)) {		\
		is_node_transient = TRUE;				\
		tag = XML_TAG_TRANSIENT_NODEATTRS;			\
		if(set_name == NULL) {					\
			local_set_name = crm_concat(section, node_uuid, '-'); \
			set_name = local_set_name;			\
		}							\
									\
	} else {							\
		return cib_bad_section;					\
	}								\
									\
	if(attr_id == NULL) {						\
		local_attr_id = crm_concat(set_name, attr_name, '-');	\
		attr_id = local_attr_id;				\
									\
	} else if(attr_name == NULL) {					\
		attr_name = attr_id;					\
	}								\

crm_data_t *
find_attr_details(crm_data_t *xml_search, const char *node_uuid,
		  const char *set_name, const char *attr_id, const char *attr_name)
{
	int matches = 0;
	crm_data_t *nv_children = NULL;
	crm_data_t *set_children = NULL;
	const char *set_type = XML_TAG_ATTR_SETS;

	if(node_uuid != NULL) {
		set_type = XML_CIB_TAG_PROPSET;

		/* filter by node */
		matches = find_xml_children(
			&set_children, xml_search, 
			NULL, XML_ATTR_ID, node_uuid, FALSE);
		crm_log_xml_debug(set_children, "search by node:");
		if(matches == 0) {
			crm_info("No node matching id=%s in %s", node_uuid, TYPE(xml_search));
			return NULL;
		}
	}

	/* filter by set name */
	if(set_name != NULL) {
		matches = find_xml_children(
			&set_children, set_children?set_children:xml_search, 
			XML_TAG_ATTR_SETS, XML_ATTR_ID, set_name, FALSE);
		crm_log_xml_debug(set_children, "search by set:");
		if(matches == 0) {
			crm_info("No set matching id=%s in %s", set_name, TYPE(xml_search));
			return NULL;
		}
	}

	matches = 0;
	if(attr_id == NULL) {
		matches = find_xml_children(
			&nv_children, set_children?set_children:xml_search,
			XML_CIB_TAG_NVPAIR, XML_NVPAIR_ATTR_NAME, attr_name, FALSE);
		crm_log_xml_debug(nv_children, "search by name:");

	} else if(attr_id != NULL) {
		matches = find_xml_children(
			&nv_children, set_children?set_children:xml_search,
			XML_CIB_TAG_NVPAIR, XML_ATTR_ID, attr_id, FALSE);
		crm_log_xml_debug(nv_children, "search by id:");
	}
	
		
	if(matches == 1) {
		crm_data_t *single_match = NULL;
		xml_child_iter(nv_children, child,
			       single_match = copy_xml(child);
			       break;
			);
		free_xml(nv_children);
		return single_match;
		
	} else if(matches > 1) {
		crm_err("Multiple attributes match name=%s in %s:\n",
			 attr_name, TYPE(xml_search));

		if(set_children == NULL) {
			free_xml(set_children);
			set_children = NULL;
			find_xml_children(
				&set_children, xml_search, 
				XML_TAG_ATTR_SETS, NULL, NULL, FALSE);
			xml_child_iter(
				set_children, set,
				free_xml(nv_children);
				nv_children = NULL;
				find_xml_children(
					&nv_children, set,
					XML_CIB_TAG_NVPAIR, XML_NVPAIR_ATTR_NAME, attr_name, FALSE);
				xml_child_iter(
					nv_children, child,
					crm_info("  Set: %s,\tValue: %s,\tID: %s\n",
						ID(set),
						crm_element_value(child, XML_NVPAIR_ATTR_VALUE),
						ID(child));
					);
				);
			
		} else {
			xml_child_iter(
				nv_children, child,
				crm_info("  ID: %s, Value: %s\n", ID(child),
					crm_element_value(child, XML_NVPAIR_ATTR_VALUE));
				);
		}
	}
	return NULL;
}


enum cib_errors 
update_attr(cib_t *the_cib, int call_options,
	    const char *section, const char *node_uuid, const char *set_name,
	    const char *attr_id, const char *attr_name, const char *attr_value)
{
	const char *tag = NULL;
	
	enum cib_errors rc = cib_ok;
	crm_data_t *xml_top = NULL;
	crm_data_t *xml_obj = NULL;
	crm_data_t *xml_search = NULL;

	char *local_attr_id = NULL;
	
	CRM_CHECK(section != NULL, return cib_missing);
	CRM_CHECK(attr_name != NULL || attr_id != NULL, return cib_missing);

	if(safe_str_eq(section, XML_CIB_TAG_NODES)) {
		CRM_CHECK(node_uuid != NULL, return cib_NOTEXISTS);
		
	} else if(safe_str_eq(section, XML_CIB_TAG_STATUS)) {
		CRM_CHECK(node_uuid != NULL, return cib_NOTEXISTS);
	}
	
	rc = the_cib->cmds->query(the_cib, section, &xml_search,
				  cib_sync_call|cib_scope_local);
	
	if(rc != cib_ok) {
		crm_err("Query failed for attribute %s (section=%s, node=%s, set=%s): %s",
			attr_name, section, crm_str(set_name), crm_str(node_uuid),
			cib_error2string(rc));
		return rc;
	}
		
	xml_obj = find_attr_details(
		xml_search, node_uuid, set_name, attr_id, attr_name);
	free_xml(xml_search);

	if(xml_obj != NULL) {
		local_attr_id = crm_strdup(ID(xml_obj));
		attr_id = local_attr_id;
	}
	
	if(attr_id == NULL || xml_obj == NULL) {
		attr_common_setup(section);	
		
		CRM_CHECK(attr_id != NULL, return cib_missing);
		CRM_CHECK(set_name != NULL, return cib_missing);
		
		if(attr_value == NULL) {
			return cib_missing_data;
		}
		
		if(is_node_transient) {
			xml_obj = create_xml_node(xml_obj, XML_CIB_TAG_STATE);
			crm_xml_add(xml_obj, XML_ATTR_ID, node_uuid);
			if(xml_top == NULL) {
				xml_top = xml_obj;
			}
		}
		
		crm_debug_2("Creating %s/%s", section, tag);
		if(tag != NULL) {
			xml_obj = create_xml_node(xml_obj, tag);
			crm_xml_add(xml_obj, XML_ATTR_ID, node_uuid);
			if(xml_top == NULL) {
				xml_top = xml_obj;
			}
		}
		
		if(node_uuid == NULL) {
			xml_obj = create_xml_node(xml_obj, XML_CIB_TAG_PROPSET);
		} else {
			xml_obj = create_xml_node(xml_obj, XML_TAG_ATTR_SETS);
		}
		crm_xml_add(xml_obj, XML_ATTR_ID, set_name);
		
		if(xml_top == NULL) {
			xml_top = xml_obj;
		}
		
		xml_obj = create_xml_node(xml_obj, XML_TAG_ATTRS);
		crm_free(local_set_name);
	} else {
		xml_obj = NULL;
	}

	xml_obj = create_xml_node(xml_obj, XML_CIB_TAG_NVPAIR);
	if(xml_top == NULL) {
		xml_top = xml_obj;
	}

	crm_xml_add(xml_obj, XML_ATTR_ID, attr_id);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_NAME, attr_name);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_VALUE, attr_value);
	
	crm_log_xml_debug_2(xml_top, "update_attr");
	
	rc = the_cib->cmds->modify(the_cib, section, xml_top, NULL,
				   call_options|cib_quorum_override);

	if(rc == cib_diff_resync) {
		/* this is an internal matter - the update succeeded */ 
		rc = cib_ok;
	}

	if(rc < cib_ok) {
		crm_err("Error setting %s=%s (section=%s, set=%s): %s",
			attr_name, attr_value, section, crm_str(set_name),
			cib_error2string(rc));
		crm_log_xml_info(xml_top, "Update");
	}
	
	crm_free(local_attr_id);
	free_xml(xml_top);
	
	return rc;
}

enum cib_errors 
read_attr(cib_t *the_cib,
	  const char *section, const char *node_uuid, const char *set_name,
	  const char *attr_id, const char *attr_name, char **attr_value)
{
	enum cib_errors rc = cib_ok;

	crm_data_t *xml_obj = NULL;
	crm_data_t *xml_next = NULL;
	crm_data_t *fragment = NULL;

	CRM_CHECK(section != NULL, return cib_missing);
	CRM_CHECK(attr_name != NULL || attr_id != NULL, return cib_missing);

	if(safe_str_eq(section, XML_CIB_TAG_NODES)) {
		CRM_CHECK(node_uuid != NULL, return cib_NOTEXISTS);
		
	} else if(safe_str_eq(section, XML_CIB_TAG_STATUS)) {
		CRM_CHECK(node_uuid != NULL, return cib_NOTEXISTS);
	}
	
	CRM_ASSERT(attr_value != NULL);
	*attr_value = NULL;

	crm_debug("Searching for attribute %s (section=%s, node=%s, set=%s)",
		  attr_name, section, crm_str(node_uuid), crm_str(set_name));

	rc = the_cib->cmds->query(
		the_cib, section, &fragment, cib_sync_call);

	if(rc != cib_ok) {
		crm_err("Query failed for attribute %s (section=%s, node=%s, set=%s): %s",
			attr_name, section, crm_str(set_name), crm_str(node_uuid),
			cib_error2string(rc));
		return rc;
	}

#if CRM_DEPRECATED_SINCE_2_0_4
	if(safe_str_eq(crm_element_name(fragment), section)) {
		xml_obj = fragment;
	} else {
		crm_data_t *a_node = NULL;
		a_node = find_xml_node(fragment, XML_TAG_CIB, TRUE);
		xml_obj = get_object_root(section, a_node);
	}
#else
	xml_obj = fragment;
	CRM_CHECK(safe_str_eq(crm_element_name(xml_obj), section),
		  return cib_output_data);
#endif
	CRM_ASSERT(xml_obj != NULL);
	crm_log_xml_debug_2(xml_obj, "Result section");
	
	xml_next = find_attr_details(
		xml_obj, node_uuid, set_name, attr_id, attr_name);

	if(xml_next != NULL) {
		*attr_value = crm_element_value_copy(
			xml_next, XML_NVPAIR_ATTR_VALUE);
	}
	free_xml(fragment);

	return xml_next == NULL?cib_NOTEXISTS:cib_ok;
}


enum cib_errors 
delete_attr(cib_t *the_cib, int options, 
	    const char *section, const char *node_uuid, const char *set_name,
	    const char *attr_id, const char *attr_name, const char *attr_value)
{
	enum cib_errors rc = cib_ok;
	crm_data_t *xml_obj = NULL;
	crm_data_t *xml_search = NULL;
	char *local_attr_id = NULL;

	CRM_CHECK(section != NULL, return cib_missing);
	CRM_CHECK(attr_name != NULL || attr_id != NULL, return cib_missing);

	if(safe_str_eq(section, XML_CIB_TAG_NODES)) {
		CRM_CHECK(node_uuid != NULL, return cib_NOTEXISTS);
		
	} else if(safe_str_eq(section, XML_CIB_TAG_STATUS)) {
		CRM_CHECK(node_uuid != NULL, return cib_NOTEXISTS);
	}
	
	if(attr_id == NULL || attr_value != NULL) {
		rc = the_cib->cmds->query(the_cib, section, &xml_search,
					  cib_sync_call|cib_scope_local);

		if(rc != cib_ok) {
			crm_err("Query failed for section=%s of the CIB: %s",
				section, cib_error2string(rc));
			return rc;
		}
		
		xml_obj = find_attr_details(
			xml_search, node_uuid, set_name, attr_id, attr_name);
		free_xml(xml_search);

		if(xml_obj != NULL) {
			if(attr_value != NULL) {
				const char *current = crm_element_value(xml_obj, XML_NVPAIR_ATTR_VALUE);
				if(safe_str_neq(attr_value, current)) {
					return cib_NOTEXISTS;
				}
			}
			local_attr_id = crm_strdup(ID(xml_obj));
			attr_id = local_attr_id;			
			xml_obj = NULL;
		}
	}

	if(attr_id == NULL) {
		return cib_NOTEXISTS;
	}
	
	xml_obj = create_xml_node(NULL, XML_CIB_TAG_NVPAIR);
	crm_xml_add(xml_obj, XML_ATTR_ID, attr_id);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_NAME, attr_name);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_VALUE, attr_value);
	
	rc = the_cib->cmds->delete(
		the_cib, section, xml_obj, NULL,
		options|cib_quorum_override);

	crm_free(local_attr_id);
	free_xml(xml_obj);
	return rc;
}

enum cib_errors 
query_node_uuid(cib_t *the_cib, const char *uname, char **uuid)
{
	enum cib_errors rc = cib_ok;
	crm_data_t *xml_obj = NULL;
	crm_data_t *fragment = NULL;
	const char *child_name = NULL;

	CRM_ASSERT(uname != NULL);
	CRM_ASSERT(uuid != NULL);
	
	rc = the_cib->cmds->query(the_cib, XML_CIB_TAG_NODES, &fragment,
				  cib_sync_call|cib_scope_local);
	if(rc != cib_ok) {
		return rc;
	}

#if CRM_DEPRECATED_SINCE_2_0_4
	if(safe_str_eq(crm_element_name(fragment), XML_CIB_TAG_NODES)) {
		xml_obj = fragment;
	} else {
		xml_obj = find_xml_node(fragment, XML_TAG_CIB, TRUE);
		xml_obj = get_object_root(XML_CIB_TAG_NODES, xml_obj);
	}
#else
	xml_obj = fragment;
	CRM_CHECK(safe_str_eq(crm_element_name(xml_obj), XML_CIB_TAG_NODES),
		  return cib_output_data);
#endif
	CRM_ASSERT(xml_obj != NULL);
	crm_log_xml_debug(xml_obj, "Result section");

	rc = cib_NOTEXISTS;
	*uuid = NULL;
	
	xml_child_iter_filter(
		xml_obj, a_child, XML_CIB_TAG_NODE,
		child_name = crm_element_value(a_child, XML_ATTR_UNAME);

		if(safe_str_eq(uname, child_name)) {
			child_name = ID(a_child);
			if(child_name != NULL) {
				*uuid = crm_strdup(child_name);
				rc = cib_ok;
			}
			break;
		}
		);
	free_xml(fragment);
	return rc;
}

enum cib_errors 
query_node_uname(cib_t *the_cib, const char *uuid, char **uname)
{
	enum cib_errors rc = cib_ok;
	crm_data_t *xml_obj = NULL;
	crm_data_t *fragment = NULL;
	const char *child_name = NULL;

	CRM_ASSERT(uname != NULL);
	CRM_ASSERT(uuid != NULL);
	
	rc = the_cib->cmds->query(the_cib, XML_CIB_TAG_NODES, &fragment,
				  cib_sync_call|cib_scope_local);
	if(rc != cib_ok) {
		return rc;
	}

#if CRM_DEPRECATED_SINCE_2_0_4
	if(safe_str_eq(crm_element_name(fragment), XML_CIB_TAG_NODES)) {
		xml_obj = fragment;
	} else {
		xml_obj = find_xml_node(fragment, XML_TAG_CIB, TRUE);
		xml_obj = get_object_root(XML_CIB_TAG_NODES, xml_obj);
	}
#else
	xml_obj = fragment;
	CRM_CHECK(safe_str_eq(crm_element_name(xml_obj), XML_CIB_TAG_NODES),
		  return cib_output_data);
#endif
	CRM_ASSERT(xml_obj != NULL);
	crm_log_xml_debug_2(xml_obj, "Result section");

	rc = cib_NOTEXISTS;
	*uname = NULL;
	
	xml_child_iter_filter(
		xml_obj, a_child, XML_CIB_TAG_NODE,
		child_name = ID(a_child);

		if(safe_str_eq(uuid, child_name)) {
			child_name = crm_element_value(a_child, XML_ATTR_UNAME);
			if(child_name != NULL) {
				*uname = crm_strdup(child_name);
				rc = cib_ok;
			}
			break;
		}
		);
	free_xml(fragment);
	return rc;
}

#define standby_common 	char *attr_id  = NULL;				\
	int str_length = 3;						\
	char *set_name = NULL;						\
	const char *attr_name  = "standby";				\
	const char *type = XML_CIB_TAG_NODES;				\
									\
	CRM_CHECK(uuid != NULL, return cib_missing_data);		\
	str_length += strlen(attr_name);				\
	str_length += strlen(uuid);					\
	if(safe_str_eq(scope, "reboot")					\
	   || safe_str_eq(scope, XML_CIB_TAG_STATUS)) {			\
		const char *extra = "transient";			\
 		type = XML_CIB_TAG_STATUS;				\
		str_length += strlen(extra);				\
		crm_malloc0(attr_id, str_length);			\
		sprintf(attr_id, "%s-%s-%s", extra, attr_name, uuid);	\
									\
	} else {							\
		crm_malloc0(attr_id, str_length);			\
		sprintf(attr_id, "%s-%s", attr_name, uuid);		\
	}

enum cib_errors 
query_standby(cib_t *the_cib, const char *uuid, const char *scope,
	      char **standby_value)
{
	enum cib_errors rc = cib_ok;
	CRM_CHECK(standby_value != NULL, return cib_missing_data);

	if(scope != NULL) {
		standby_common;
		rc = read_attr(the_cib, type, uuid, set_name,
			       attr_id, attr_name, standby_value);
		crm_free(attr_id);
		crm_free(set_name);

	} else {
		rc = query_standby(
			the_cib, uuid, XML_CIB_TAG_NODES, standby_value);

		if(rc == cib_NOTEXISTS) {
			crm_debug("No standby value found with "
				  "lifetime=forever, checking lifetime=reboot");
			rc = query_standby(the_cib, uuid,
					   XML_CIB_TAG_STATUS, standby_value);
		}
	}
	
	return rc;
}

enum cib_errors 
set_standby(cib_t *the_cib, const char *uuid, const char *scope,
	    const char *standby_value)
{
	enum cib_errors rc = cib_ok;
	CRM_CHECK(standby_value != NULL, return cib_missing_data);
	if(scope != NULL) {
		standby_common;
		rc = update_attr(the_cib, cib_sync_call, type, uuid, set_name,
				 attr_id, attr_name, standby_value);
		crm_free(attr_id);
		crm_free(set_name);

	} else {
		rc = set_standby(the_cib, uuid, XML_CIB_TAG_NODES, standby_value);
	}

	return rc;
}

enum cib_errors 
delete_standby(cib_t *the_cib, const char *uuid, const char *scope,
	       const char *standby_value)
{
	enum cib_errors rc = cib_ok;
	if(scope != NULL) {
		standby_common;
		rc = delete_attr(the_cib, cib_sync_call, type, uuid, set_name,
				 attr_id, attr_name, standby_value);
		crm_free(attr_id);
		crm_free(set_name);

	} else {
		rc = delete_standby(
			the_cib, uuid, XML_CIB_TAG_STATUS, standby_value);

		rc = delete_standby(
			the_cib, uuid, XML_CIB_TAG_NODES, standby_value);
	}

	return rc;
}

