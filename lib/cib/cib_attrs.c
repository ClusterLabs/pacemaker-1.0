
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

#include <crm_internal.h>

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


#define attr_msg(level, fmt, args...) do {	\
	if(to_console) {			\
	    printf(fmt"\n", ##args);		\
	} else {				\
	    do_crm_log(level, fmt , ##args);	\
	}					\
} while(0)

extern enum cib_errors
find_nvpair_attr(
    cib_t *the_cib, const char *attr, const char *section, const char *node_uuid, const char *set_name,
    const char *attr_id, const char *attr_name, gboolean to_console, char **value)
{
    int offset = 0;
    static int xpath_max = 1024;
    enum cib_errors rc = cib_ok;

    char *xpath_string = NULL;
    xmlNode *xml_search = NULL;
    const char *set_type = XML_TAG_ATTR_SETS;

    CRM_ASSERT(value != NULL);
    *value = NULL;
    
    if(safe_str_eq(section, XML_CIB_TAG_CRMCONFIG)) {
	node_uuid = NULL;
	set_type = XML_CIB_TAG_PROPSET;
	
    } else if(safe_str_eq(section, XML_CIB_TAG_OPCONFIG)
	      || safe_str_eq(section, XML_CIB_TAG_RSCCONFIG)) {
	node_uuid = NULL;
	set_type = XML_TAG_META_SETS;

    } else if(node_uuid == NULL) {
	return cib_missing_data;
    }
    
    crm_malloc0(xpath_string, xpath_max);
    offset += snprintf(xpath_string + offset, xpath_max - offset, "%s", get_object_path(section));

    if(node_uuid) {
	const char *node_type = XML_CIB_TAG_NODE;
	if(safe_str_eq(section, XML_CIB_TAG_STATUS)) {
	    node_type = XML_CIB_TAG_STATE;
	    set_type = XML_TAG_TRANSIENT_NODEATTRS;
	}
	offset += snprintf(xpath_string + offset, xpath_max - offset, "//%s[@id='%s']", node_type, node_uuid);
    }

    if(set_name) {
	offset += snprintf(xpath_string + offset, xpath_max - offset, "//%s[@id='%s']", set_type, set_name);
    }
    
    offset += snprintf(xpath_string + offset, xpath_max - offset, "//nvpair[");
    if(attr_id) {
	offset += snprintf(xpath_string + offset, xpath_max - offset, "@id='%s'", attr_id);
    }
    
    if(attr_name) {
	if(attr_id) {
	    offset += snprintf(xpath_string + offset, xpath_max - offset, " and ");
	}
	offset += snprintf(xpath_string + offset, xpath_max - offset, "@name='%s'", attr_name);
    }   
    offset += snprintf(xpath_string + offset, xpath_max - offset, "]");
    
    rc = the_cib->cmds->query(
	the_cib, xpath_string, &xml_search, cib_sync_call|cib_scope_local|cib_xpath);
	
    if(rc != cib_ok) {
	do_crm_log(LOG_DEBUG_2, "Query failed for attribute %s (section=%s, node=%s, set=%s, xpath=%s): %s",
		 attr_name, section, crm_str(node_uuid), crm_str(set_name), xpath_string,
		 cib_error2string(rc));
	goto done;
    }

    crm_log_xml_debug(xml_search, "Match");
    if(xml_has_children(xml_search)) {
	rc = cib_missing_data;
	attr_msg(LOG_WARNING, "Multiple attributes match name=%s", attr_name);
	
	xml_child_iter(xml_search, child,
		       attr_msg(LOG_INFO, "  Value: %s \t(id=%s)", 
				crm_element_value(child, XML_NVPAIR_ATTR_VALUE), ID(child));
	    );

    } else {
	const char *tmp = crm_element_value(xml_search, attr);
	if(tmp) {
	    *value = crm_strdup(tmp);
	}
    }

  done:
    crm_free(xpath_string);
    free_xml(xml_search);
    return rc;
}


enum cib_errors 
update_attr(cib_t *the_cib, int call_options,
	    const char *section, const char *node_uuid, const char *set_name,
	    const char *attr_id, const char *attr_name, const char *attr_value, gboolean to_console)
{
	const char *tag = NULL;
	enum cib_errors rc = cib_ok;
	xmlNode *xml_top = NULL;
	xmlNode *xml_obj = NULL;

	char *local_attr_id = NULL;
	char *local_set_name = NULL;
	gboolean use_attributes_tag = FALSE;
	
	CRM_CHECK(section != NULL, return cib_missing);
	CRM_CHECK(attr_value != NULL, return cib_missing);
	CRM_CHECK(attr_name != NULL || attr_id != NULL, return cib_missing);
	
	rc = find_nvpair_attr(the_cib, XML_ATTR_ID, section, node_uuid, set_name, attr_id, attr_name, FALSE, &local_attr_id);
	if(rc == cib_ok) {
	    attr_id = local_attr_id;
	    goto do_modify;

	} else if(rc != cib_NOTEXISTS) {
	    return rc;

	/* } else if(attr_id == NULL) { */
	/*     return cib_missing; */

	} else {
	    const char *value = NULL;
	    xmlNode *cib_top = NULL;
	    rc = the_cib->cmds->query(
		the_cib, "/cib", &cib_top, cib_sync_call|cib_scope_local|cib_xpath|cib_no_children);

	    value = crm_element_value(cib_top, "ignore_dtd");
	    if(value != NULL) {
		use_attributes_tag = TRUE;
		
	    } else {
		value = crm_element_value(cib_top, XML_ATTR_VALIDATION);
		if(value && strstr(value, "-0.6")) {
		    use_attributes_tag = TRUE;
		}
	    }
	    free_xml(cib_top);
	    
	    if(safe_str_eq(section, XML_CIB_TAG_NODES)) {
		tag = XML_CIB_TAG_NODE;
		if(node_uuid == NULL) {
		    return cib_missing;
		}

	    } else if(safe_str_eq(section, XML_CIB_TAG_STATUS)) {
		tag = XML_TAG_TRANSIENT_NODEATTRS;
		if(node_uuid == NULL) {
		    return cib_missing;
		}

		xml_obj = create_xml_node(xml_obj, XML_CIB_TAG_STATE);
		crm_xml_add(xml_obj, XML_ATTR_ID, node_uuid);
		if(xml_top == NULL) {
		    xml_top = xml_obj;
		}

	    } else {
		tag = section;
		node_uuid = NULL;
	    }

	    if(set_name == NULL) {
		if(safe_str_eq(section, XML_CIB_TAG_CRMCONFIG)) {
		    local_set_name = crm_strdup(CIB_OPTIONS_FIRST);

		} else if(node_uuid) {
		    local_set_name = crm_concat(section, node_uuid, '-');

		} else {
		    local_set_name = crm_concat(section, "options", '-');
		}
		set_name = local_set_name;
	    }

	    if(attr_id == NULL) {
		local_attr_id = crm_concat(set_name, attr_name, '-');
		attr_id = local_attr_id;

	    } else if(attr_name == NULL) {
		attr_name = attr_id;
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
		if(safe_str_eq(section, XML_CIB_TAG_CRMCONFIG)) {
		    xml_obj = create_xml_node(xml_obj, XML_CIB_TAG_PROPSET);
		} else {
		    xml_obj = create_xml_node(xml_obj, XML_TAG_META_SETS);
		}
		
	    } else {
		xml_obj = create_xml_node(xml_obj, XML_TAG_ATTR_SETS);
	    }
	    crm_xml_add(xml_obj, XML_ATTR_ID, set_name);
	    
	    if(xml_top == NULL) {
		xml_top = xml_obj;
	    }

	    if(use_attributes_tag) {
		xml_obj = create_xml_node(xml_obj, XML_TAG_ATTRS);
	    }
	}

  do_modify:
	xml_obj = create_xml_node(xml_obj, XML_CIB_TAG_NVPAIR);
	if(xml_top == NULL) {
		xml_top = xml_obj;
	}

	crm_xml_add(xml_obj, XML_ATTR_ID, attr_id);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_NAME, attr_name);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_VALUE, attr_value);
	
	crm_log_xml_debug_2(xml_top, "update_attr");
	rc = the_cib->cmds->modify(
	    the_cib, section, xml_top, call_options|cib_quorum_override);

	if(rc < cib_ok) {
		attr_msg(LOG_ERR, "Error setting %s=%s (section=%s, set=%s): %s",
			attr_name, attr_value, section, crm_str(set_name),
			cib_error2string(rc));
		crm_log_xml_info(xml_top, "Update");
	}
	
	crm_free(local_set_name);
	crm_free(local_attr_id);
	free_xml(xml_top);
	
	return rc;
}

enum cib_errors 
read_attr(cib_t *the_cib,
	  const char *section, const char *node_uuid, const char *set_name,
	  const char *attr_id, const char *attr_name, char **attr_value, gboolean to_console)
{
	enum cib_errors rc = cib_ok;

	CRM_ASSERT(attr_value != NULL);
	CRM_CHECK(section != NULL, return cib_missing);
	CRM_CHECK(attr_name != NULL || attr_id != NULL, return cib_missing);

	*attr_value = NULL;

	rc = find_nvpair_attr(the_cib, XML_NVPAIR_ATTR_VALUE, section, node_uuid, set_name, attr_id, attr_name, to_console, attr_value);
	if(rc != cib_ok) {
		do_crm_log(LOG_DEBUG_2, "Query failed for attribute %s (section=%s, node=%s, set=%s): %s",
			attr_name, section, crm_str(set_name), crm_str(node_uuid),
			cib_error2string(rc));
	}
	return rc;
}


enum cib_errors 
delete_attr(cib_t *the_cib, int options, 
	    const char *section, const char *node_uuid, const char *set_name,
	    const char *attr_id, const char *attr_name, const char *attr_value, gboolean to_console)
{
	enum cib_errors rc = cib_ok;
	xmlNode *xml_obj = NULL;
	char *local_attr_id = NULL;

	CRM_CHECK(section != NULL, return cib_missing);
	CRM_CHECK(attr_name != NULL || attr_id != NULL, return cib_missing);

	if(attr_id == NULL) {
	    rc = find_nvpair_attr(the_cib, XML_ATTR_ID, section, node_uuid, set_name, attr_id, attr_name, to_console, &local_attr_id);
	    if(rc != cib_ok) {
		return rc;
	    }
	    attr_id = local_attr_id;
	}
	
	xml_obj = create_xml_node(NULL, XML_CIB_TAG_NVPAIR);
	crm_xml_add(xml_obj, XML_ATTR_ID, attr_id);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_NAME, attr_name);
	crm_xml_add(xml_obj, XML_NVPAIR_ATTR_VALUE, attr_value);
	
	rc = the_cib->cmds->delete(
	    the_cib, section, xml_obj, options|cib_quorum_override);

	if(rc == cib_ok) {
	    attr_msg(LOG_DEBUG, "Deleted %s %s: id=%s%s%s%s%s\n",
		     section, node_uuid?"attribute":"option", local_attr_id,
		     set_name?" set=":"", set_name?set_name:"",
		     attr_name?" name=":"", attr_name?attr_name:"");
	}

	crm_free(local_attr_id);
	free_xml(xml_obj);
	return rc;
}

enum cib_errors 
query_node_uuid(cib_t *the_cib, const char *uname, char **uuid)
{
	enum cib_errors rc = cib_ok;
	xmlNode *xml_obj = NULL;
	xmlNode *fragment = NULL;
	const char *child_name = NULL;

	CRM_ASSERT(uname != NULL);
	CRM_ASSERT(uuid != NULL);
	
	rc = the_cib->cmds->query(the_cib, XML_CIB_TAG_NODES, &fragment,
				  cib_sync_call|cib_scope_local);
	if(rc != cib_ok) {
		return rc;
	}

	xml_obj = fragment;
	CRM_CHECK(safe_str_eq(crm_element_name(xml_obj), XML_CIB_TAG_NODES),
		  return cib_output_data);
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
	xmlNode *xml_obj = NULL;
	xmlNode *fragment = NULL;
	const char *child_name = NULL;

	CRM_ASSERT(uname != NULL);
	CRM_ASSERT(uuid != NULL);
	
	rc = the_cib->cmds->query(the_cib, XML_CIB_TAG_NODES, &fragment,
				  cib_sync_call|cib_scope_local);
	if(rc != cib_ok) {
		return rc;
	}

	xml_obj = fragment;
	CRM_CHECK(safe_str_eq(crm_element_name(xml_obj), XML_CIB_TAG_NODES),
		  return cib_output_data);
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

enum cib_errors 
set_standby(cib_t *the_cib, const char *uuid, const char *scope, const char *standby_value)
{
	enum cib_errors rc = cib_ok;
	int str_length = 3;
	char *attr_id  = NULL;
	char *set_name = NULL;
	const char *attr_name  = "standby";
	CRM_CHECK(standby_value != NULL, return cib_missing_data);
	if(scope == NULL) {
	    scope = XML_CIB_TAG_NODES;
	}
	

	CRM_CHECK(uuid != NULL, return cib_missing_data);
	str_length += strlen(attr_name);
	str_length += strlen(uuid);
	
	if(safe_str_eq(scope, "reboot") || safe_str_eq(scope, XML_CIB_TAG_STATUS)) {
	    const char *extra = "transient";
	    scope = XML_CIB_TAG_STATUS;
	    
	    str_length += strlen(extra);
	    crm_malloc0(attr_id, str_length);
	    sprintf(attr_id, "%s-%s-%s", extra, attr_name, uuid);
	    
	} else {
	    scope = XML_CIB_TAG_NODES;
	    crm_malloc0(attr_id, str_length);
	    sprintf(attr_id, "%s-%s", attr_name, uuid);
	}
	
	rc = update_attr(the_cib, cib_sync_call, scope, uuid, set_name,
			 attr_id, attr_name, standby_value, TRUE);

	crm_free(attr_id);
	crm_free(set_name);
	return rc;
}

