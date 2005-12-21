/*
 * Linux HA management library
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <unistd.h>
#include <glib.h>

#include <heartbeat.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_syslog.h>
#include "mgmt_internal.h"

#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/pengine/pengine.h>

extern resource_t *group_find_child(resource_t *rsc, const char *id);

cib_t*	cib_conn = NULL;
int init_crm(void);
void final_crm(void);

static void on_cib_diff(const char *event, HA_Message *msg);

static char* on_get_crm_config(char* argv[], int argc);
static char* on_update_crm_config(char* argv[], int argc);
static char* on_get_activenodes(char* argv[], int argc);
static char* on_get_dc(char* argv[], int argc);

static char* on_get_node_config(char* argv[], int argc);
static char* on_get_running_rsc(char* argv[], int argc);

static char* on_del_rsc(char* argv[], int argc);
static char* on_add_rsc(char* argv[], int argc);
static char* on_add_grp(char* argv[], int argc);

static char* on_update_clone(char* argv[], int argc);
static char* on_get_clone(char* argv[], int argc);

static char* on_update_master(char* argv[], int argc);
static char* on_get_master(char* argv[], int argc);

static char* on_get_all_rsc(char* argv[], int argc);
static char* on_get_rsc_type(char* argv[], int argc);
static char* on_get_sub_rsc(char* argv[], int argc);
static char* on_get_rsc_attrs(char* argv[], int argc);
static char* on_get_rsc_running_on(char* argv[], int argc);
static char* on_get_rsc_status(char* argv[], int argc);

static char* on_get_rsc_params(char* argv[], int argc);
static char* on_update_rsc_params(char* argv[], int argc);
static char* on_delete_rsc_param(char* argv[], int argc);

static char* on_get_rsc_ops(char* argv[], int argc);
static char* on_update_rsc_ops(char* argv[], int argc);
static char* on_delete_rsc_op(char* argv[], int argc);

static char* on_get_constraints(char* argv[], int argc);
static char* on_get_constraint(char* argv[], int argc);
static char* on_update_constraint(char* argv[], int argc);
static char* on_delete_constraint(char* argv[], int argc);

static resource_t* find_resource(GList* rsc_list, const char* id);
static int delete_object(const char* type, const char* entry, const char* id);
static GList* find_xml_node_list(crm_data_t *root, const char *search_path);
static pe_working_set_t get_data_set(void);

#define GET_RESOURCE()	if (argc != 2) { 					\
				return cl_strdup(MSG_FAIL); 			\
			} 							\
			rsc = find_resource(data_set.resources, argv[1]); 	\
			if (rsc == NULL) {					\
				return cl_strdup(MSG_FAIL); 			\
			}
			
/* internal functions */
GList* find_xml_node_list(crm_data_t *root, const char *child_name)
{
	int i;
	GList* list = NULL;
	for (i = 0; i < root->nfields; i++ ) {
		if (strncmp(root->names[i], child_name, MAX_STRLEN) == 0) {
			list = g_list_append(list, root->values[i]);
		}
	}
	return list;
}

resource_t*
find_resource(GList* rsc_list, const char* id)
{
	resource_t* ret;
	GList* cur = rsc_list;
	while (cur != NULL) {
		resource_t* rsc = (resource_t*)cur->data;
		if (strcmp(id, rsc->id) == 0) {
			return rsc;
		}
		ret = rsc->fns->find_child(rsc, id);
		if (ret != NULL) {
			return ret;
		}
		cur = g_list_next(cur);
	}
	return NULL;
}

int
delete_object(const char* type, const char* entry, const char* id) 
{
	int rc;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];

	snprintf(xml, MAX_STRLEN, "<%s id=\"%s\">", entry, id);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return -1;
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);

	rc = cib_conn->cmds->delete(
			cib_conn, type, cib_object, &output, 0);

	if (rc < 0) {
		return -1;
	}
	return 0;
}

pe_working_set_t 
get_data_set(void) {
	static crm_data_t* cib_xml_copy = NULL;
	pe_working_set_t data_set;
	
	set_working_set_defaults(&data_set);
	if (cib_xml_copy) {
		ha_msg_del(cib_xml_copy);
	}
	cib_xml_copy = get_cib_copy(cib_conn);
	data_set.input = cib_xml_copy;
	data_set.now = new_ha_date(TRUE);
	stage0(&data_set);
	return data_set;
}


/* mgmtd functions */
int
init_crm(void)
{
	int ret = cib_ok;
	int i, max_try = 5;

	mgmt_log(LOG_INFO,"init_crm");
	cib_conn = cib_new();
	for (i = 0; i < max_try ; i++) {
		ret = cib_conn->cmds->signon(cib_conn, client_name, cib_query);
		if (ret == cib_ok) {
			break;
		}
		mgmt_log(LOG_INFO,"login to cib: %d, ret:%d",i,ret);
		sleep(1);
	}
	if (ret != cib_ok) {
		mgmt_log(LOG_INFO,"login to cib failed");
		cib_conn = NULL;
		return -1;
	}

	ret = cib_conn->cmds->add_notify_callback(cib_conn, T_CIB_DIFF_NOTIFY
						  , on_cib_diff);

	reg_msg(MSG_CRM_CONFIG, on_get_crm_config);
	reg_msg(MSG_UP_CRM_CONFIG, on_update_crm_config);
	
	reg_msg(MSG_DC, on_get_dc);
	reg_msg(MSG_ACTIVENODES, on_get_activenodes);
	reg_msg(MSG_NODE_CONFIG, on_get_node_config);
	reg_msg(MSG_RUNNING_RSC, on_get_running_rsc);

	reg_msg(MSG_DEL_RSC, on_del_rsc);
	reg_msg(MSG_ADD_RSC, on_add_rsc);
	reg_msg(MSG_ADD_GRP, on_add_grp);
	
	reg_msg(MSG_ALL_RSC, on_get_all_rsc);
	reg_msg(MSG_SUB_RSC, on_get_sub_rsc);
	reg_msg(MSG_RSC_ATTRS, on_get_rsc_attrs);
	reg_msg(MSG_RSC_RUNNING_ON, on_get_rsc_running_on);
	reg_msg(MSG_RSC_STATUS, on_get_rsc_status);
	reg_msg(MSG_RSC_TYPE, on_get_rsc_type);
	
	reg_msg(MSG_RSC_PARAMS, on_get_rsc_params);
	reg_msg(MSG_UP_RSC_PARAMS, on_update_rsc_params);
	reg_msg(MSG_DEL_RSC_PARAM, on_delete_rsc_param);
	
	reg_msg(MSG_RSC_OPS, on_get_rsc_ops);
	reg_msg(MSG_UP_RSC_OPS, on_update_rsc_ops);
	reg_msg(MSG_DEL_RSC_OP, on_delete_rsc_op);

	reg_msg(MSG_UPDATE_CLONE, on_update_clone);
	reg_msg(MSG_GET_CLONE, on_get_clone);
	reg_msg(MSG_UPDATE_MASTER, on_update_master);
	reg_msg(MSG_GET_MASTER, on_get_master);

	reg_msg(MSG_GET_CONSTRAINTS, on_get_constraints);
	reg_msg(MSG_GET_CONSTRAINT, on_get_constraint);
	reg_msg(MSG_DEL_CONSTRAINT, on_delete_constraint);
	reg_msg(MSG_UP_CONSTRAINT, on_update_constraint);
	
	return 0;
}	
void
final_crm(void)
{
	if(cib_conn != NULL) {
		cib_conn->cmds->signoff(cib_conn);
		cib_conn = NULL;
	}
}

/* event handler */
void
on_cib_diff(const char *event, HA_Message *msg)
{
	mgmt_log(LOG_INFO,"update cib finished");
	fire_event(EVT_CIB_CHANGED);
}

/* cluster  functions */
char* 
on_get_crm_config(char* argv[], int argc)
{
	char buf [255];
	pe_working_set_t data_set;
	char* ret = cl_strdup(MSG_OK);
	data_set = get_data_set();
	
	ret = mgmt_msg_append(ret, data_set.transition_idle_timeout);
	ret = mgmt_msg_append(ret, data_set.symmetric_cluster?"True":"False");
	ret = mgmt_msg_append(ret, data_set.stonith_enabled?"True":"False");
	
	switch (data_set.no_quorum_policy) {
		case no_quorum_freeze:
			ret = mgmt_msg_append(ret, "freeze");
			break;
		case no_quorum_stop:
			ret = mgmt_msg_append(ret, "stop");
			break;
		case no_quorum_ignore:
			ret = mgmt_msg_append(ret, "ignore");
			break;
	}
	snprintf(buf, 255, "%d", data_set.default_resource_stickiness);
	ret = mgmt_msg_append(ret, buf);
	ret = mgmt_msg_append(ret, data_set.have_quorum?"True":"False");
	return ret;
}
char*
on_update_crm_config(char* argv[], int argc)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];

	ARGC_CHECK(3);
	snprintf(xml, MAX_STRLEN, "<nvpair id=\"%s\" name=\"%s\" value=\"%s\"/>", argv[1],argv[1],argv[2]);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}

	fragment = create_cib_fragment(cib_object, "crm_config");

	rc = cib_conn->cmds->update(
			cib_conn, "crm_config", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}

	return cl_strdup(MSG_OK);
}

/* node functions */
char*
on_get_activenodes(char* argv[], int argc)
{
	node_t* node;
	GList* cur;
	char* ret;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	cur = data_set.nodes;
	ret = cl_strdup(MSG_OK);
	while (cur != NULL) {
		node = (node_t*) cur->data;
		if (node->details->online) {
			ret = mgmt_msg_append(ret, node->details->uname);
		}
		cur = g_list_next(cur);
	}
	return ret;
}

char* 
on_get_dc(char* argv[], int argc)
{
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	if (data_set.dc_node != NULL) {
		char* ret = cl_strdup(MSG_OK);
		ret = mgmt_msg_append(ret, data_set.dc_node->details->uname);
		return ret;
	}
	return cl_strdup(MSG_FAIL);
}


char*
on_get_node_config(char* argv[], int argc)
{
	node_t* node;
	GList* cur;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	cur = data_set.nodes;
	ARGC_CHECK(2);
	while (cur != NULL) {
		node = (node_t*) cur->data;
		if (node->details->online) {
			if (strncmp(argv[1],node->details->uname,MAX_STRLEN) == 0) {
				char* ret = cl_strdup(MSG_OK);
				ret = mgmt_msg_append(ret, node->details->uname);
				ret = mgmt_msg_append(ret, node->details->online?"True":"False");
				ret = mgmt_msg_append(ret, node->details->standby?"True":"False");
				ret = mgmt_msg_append(ret, node->details->unclean?"True":"False");
				ret = mgmt_msg_append(ret, node->details->shutdown?"True":"False");
				ret = mgmt_msg_append(ret, node->details->expected_up?"True":"False");
				ret = mgmt_msg_append(ret, node->details->is_dc?"True":"False");
				ret = mgmt_msg_append(ret, node->details->type==node_ping?"ping":"member");

				return ret;
			}
		}
		cur = g_list_next(cur);
	}
	return cl_strdup(MSG_FAIL);
}

char*
on_get_running_rsc(char* argv[], int argc)
{
	node_t* node;
	GList* cur;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	cur = data_set.nodes;
	ARGC_CHECK(2);
	while (cur != NULL) {
		node = (node_t*) cur->data;
		if (node->details->online) {
			if (strncmp(argv[1],node->details->uname,MAX_STRLEN) == 0) {
				GList* cur_rsc;
				char* ret = cl_strdup(MSG_OK);
				cur_rsc = node->details->running_rsc;
				while(cur_rsc != NULL) {
					resource_t* rsc = (resource_t*)cur_rsc->data;
					ret = mgmt_msg_append(ret, rsc->id);
					cur_rsc = g_list_next(cur_rsc);
				}
				return ret;
			}
		}
		cur = g_list_next(cur);
	}
	return cl_strdup(MSG_FAIL);
}
/* resource functions */
/* add/delete resource */
char*
on_del_rsc(char* argv[], int argc)
{
	int rc;
	resource_t* rsc;
	const char* rsc_id;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	switch (rsc->variant) {
		case pe_native:
			/* if the resource is in group, remove the prefix */
			rsc_id = strrchr(rsc->id, ':');
			if (rsc_id == NULL) {
				rsc_id = rsc->id;
			}
			else {
				rsc_id ++;
			}	
			snprintf(xml, MAX_STRLEN, "<primitive id=\"%s\"/>", rsc_id);
			break;
		case pe_group:
			snprintf(xml, MAX_STRLEN, "<group id=\"%s\"/>", rsc->id);
			break;
		case pe_clone:
			snprintf(xml, MAX_STRLEN, "<clone id=\"%s\"/>", rsc->id);
			break;
		case pe_master:
			snprintf(xml, MAX_STRLEN, "<master_slave id=\"%s\"/>", rsc->id);
			break;
		default:
			return cl_strdup(MSG_FAIL);
	}
			

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}

	rc = cib_conn->cmds->delete(
			cib_conn, "resources", cib_object, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}

	
	return cl_strdup(MSG_OK);
}
/*
	0	cmd = "add_rsc"
	1	cmd += "\n"+rsc["id"]
	2	cmd += "\n"+rsc["class"]
	3	cmd += "\n"+rsc["type"]
	4	cmd += "\n"+rsc["provider"]
	5	cmd += "\n"+rsc["group"]
	6	cmd += "\n"+rsc["advance"]
	7	cmd += "\n"+rsc["advance_id"]
	8	cmd += "\n"+rsc["clone_max"]
	9	cmd += "\n"+rsc["clone_node_max"]
	10	cmd += "\n"+rsc["master_max"]
	11	cmd += "\n"+rsc["master_node_max"]
		for param in rsc["params"] :
	12,15,18...	cmd += "\n"+param["id"]
	13,16,19...	cmd += "\n"+param["name"]
	14,17,20...	cmd += "\n"+param["value"]
*/
char*
on_add_rsc(char* argv[], int argc)
{
	int rc, i, in_group;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	char buf[MAX_STRLEN];
	int clone, master, has_param;
		
	if (argc < 11) {
		return cl_strdup(MSG_FAIL);
	}
	xml[0]=0;
	in_group = (strlen(argv[5]) != 0);
	clone = (STRNCMP_CONST(argv[6], "clone") == 0);
	master = (STRNCMP_CONST(argv[6], "master") == 0);
	has_param = (argc > 11);
	if (in_group) {
		snprintf(buf, MAX_STRLEN, "<group id=\"%s\">", argv[5]);
		strncat(xml, buf, MAX_STRLEN);
	}
	if (clone) {
		snprintf(buf, MAX_STRLEN,
			 "<clone id=\"%s\"><instance_attributes><attributes>" \
			 "<nvpair id=\"clone_max\" name=\"clone_max\" value=\"%s\"/>" \
			 "<nvpair id=\"clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
			 "</attributes>	</instance_attributes> ",
			 argv[7], argv[8], argv[9]);
		strncat(xml, buf, MAX_STRLEN);
	}
	if (master) {
		snprintf(buf, MAX_STRLEN,
			 "<master_slave id=\"%s\"><instance_attributes><attributes>" \
			 "<nvpair id=\"clone_max\" name=\"clone_max\" value=\"%s\"/>" \
			 "<nvpair id=\"clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
			 "<nvpair id=\"master_max\" name=\"master_max\" value=\"%s\"/>" \
			 "<nvpair id=\"master_node_max\" name=\"master_node_max\" value=\"%s\"/>" \
			 "</attributes>	</instance_attributes>",
			 argv[7], argv[8], argv[9], argv[10], argv[11]);
		strncat(xml, buf, MAX_STRLEN);
	}
	
	if (!has_param) {
		snprintf(buf, MAX_STRLEN,
			 "<primitive id=\"%s\" class=\"%s\" type=\"%s\" provider=\"%s\"/>"
					 , argv[1],argv[2], argv[3],argv[4]);
		strncat(xml, buf, MAX_STRLEN);
	}
	else {
		snprintf(buf, MAX_STRLEN,
			 "<primitive id=\"%s\" class=\"%s\" type=\"%s\" provider=\"%s\">" \
			 "<instance_attributes> <attributes>"
			 , argv[1],argv[2], argv[3],argv[4]);
		strncat(xml, buf, MAX_STRLEN);
	
		for (i = 12; i < argc; i += 3) {
			snprintf(buf, MAX_STRLEN,
				 "<nvpair id=\"%s\" name=\"%s\" value=\"%s\"/>",
				 argv[i], argv[i+1],argv[i+2]);
			strncat(xml, buf, MAX_STRLEN);
		}
		strncat(xml, "</attributes></instance_attributes></primitive>", MAX_STRLEN);
	}
	if (master) {
		strncat(xml, "</master_slave>", MAX_STRLEN);
	}
	if (clone) {
		strncat(xml, "</clone>", MAX_STRLEN);
	}
	
	if (in_group) {
		strncat(xml, "</group>", MAX_STRLEN);
	}
	
	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	if (in_group) {
		rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, 0);
	}
	else {
		rc = cib_conn->cmds->create(
			cib_conn, "resources", fragment, &output, 0);
	}
	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);

}
char*
on_add_grp(char* argv[], int argc)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	
	ARGC_CHECK(2);
	snprintf(xml, MAX_STRLEN,"<group id=\"%s\"/>", argv[1]);
	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");
	rc = cib_conn->cmds->create(cib_conn, "resources", fragment, &output, 0);
	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);

}
/* get all resources*/
char*
on_get_all_rsc(char* argv[], int argc)
{
	GList* cur;
	char* ret;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	ret = cl_strdup(MSG_OK);
	cur = data_set.resources;
	while (cur != NULL) {
		resource_t* rsc = (resource_t*)cur->data;
		ret = mgmt_msg_append(ret, rsc->id);
		cur = g_list_next(cur);
	}
	return ret;
}
/* basic information of resource */
char*
on_get_rsc_attrs(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	struct ha_msg* attrs;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	attrs = (struct ha_msg*)rsc->xml;
	ret = mgmt_msg_append(ret, ha_msg_value(attrs, "id"));
	ret = mgmt_msg_append(ret, ha_msg_value(attrs, "class"));
	ret = mgmt_msg_append(ret, ha_msg_value(attrs, "provider"));
	ret = mgmt_msg_append(ret, ha_msg_value(attrs, "type"));
	return ret;
}

char*
on_get_rsc_running_on(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	GList* cur;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	cur = rsc->running_on;
	while (cur != NULL) {
		node_t* node = (node_t*)cur->data;
		ret = mgmt_msg_append(ret, node->details->uname);
		cur = g_list_next(cur);
	}
	return ret;
}
char*
on_get_rsc_status(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	switch (rsc->variant) {
		case pe_unknown:
			ret = mgmt_msg_append(ret, "unknown");
			break;
		case pe_native:
			if(rsc->is_managed == FALSE) {
				ret = mgmt_msg_append(ret, "unmanaged");
				break;
			}
			if( rsc->failed 
			  ||g_list_length(rsc->running_on) == 0) {
				ret = mgmt_msg_append(ret, "failed");
				break;
			}
			if( g_list_length(rsc->running_on) > 1) {
				ret = mgmt_msg_append(ret, "multi-running");
				break;
			}
			ret = mgmt_msg_append(ret, "running");		
			break;
		case pe_group:
			ret = mgmt_msg_append(ret, "group");
			break;
		case pe_clone:
			ret = mgmt_msg_append(ret, "clone");
			break;
		case pe_master:
			ret = mgmt_msg_append(ret, "master");
			break;
	}
	return ret;
}

char*
on_get_rsc_type(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);

	switch (rsc->variant) {
		case pe_unknown:
			ret = mgmt_msg_append(ret, "unknown");
			break;
		case pe_native:
			ret = mgmt_msg_append(ret, "native");
			break;
		case pe_group:
			ret = mgmt_msg_append(ret, "group");
			break;
		case pe_clone:
			ret = mgmt_msg_append(ret, "clone");
			break;
		case pe_master:
			ret = mgmt_msg_append(ret, "master");
			break;
	}

	return ret;
}
/* FIXME: following two structures is copied from CRM */
typedef struct group_variant_data_s
{
		int num_children;
		GListPtr child_list; /* resource_t* */
		resource_t *self;
		resource_t *first_child;
		resource_t *last_child;

		gboolean child_starting;
		gboolean child_stopping;
		
} group_variant_data_t;

typedef struct clone_variant_data_s
{
	resource_t *self;

	int clone_max;
	int clone_node_max;

	int active_clones;
	int max_nodes;
		
	gboolean interleave;
	gboolean ordered;

	crm_data_t *xml_obj_child;
		
	gboolean notify_confirm;
		
	GListPtr child_list; /* resource_t* */
		
} clone_variant_data_t;

char*
on_get_sub_rsc(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	GList* cur = NULL;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	if (rsc->variant == pe_group) {
		cur = ((group_variant_data_t*)rsc->variant_opaque)->child_list;
	}
	if (rsc->variant == pe_clone || rsc->variant == pe_master) {
		cur = ((clone_variant_data_t*)rsc->variant_opaque)->child_list;
	}

	ret = cl_strdup(MSG_OK);
	while (cur != NULL) {
		resource_t* rsc = (resource_t*)cur->data;
		ret = mgmt_msg_append(ret, rsc->id);
		cur = g_list_next(cur);
	}
	return ret;
}

/* resource params */
char*
on_get_rsc_params(char* argv[], int argc)
{
	int i;
	resource_t* rsc;
	char* ret;
	struct ha_msg* attrs;
	struct ha_msg* nvpair;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	attrs = cl_get_struct((struct ha_msg*)rsc->xml, "instance_attributes");
	if(attrs == NULL) {
		return ret;
	}
	attrs = cl_get_struct(attrs, "attributes");
	if(attrs == NULL) {
		return ret;
	}
	for (i = 0; i < attrs->nfields; i++) {
		if (STRNCMP_CONST(attrs->names[i], "nvpair") == 0) {
			nvpair = (struct ha_msg*)attrs->values[i];
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "id"));
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "name"));
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "value"));
		}
	}
	return ret;
}
char*
on_update_rsc_params(char* argv[], int argc)
{
	int rc, i;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	char buf[MAX_STRLEN];

	snprintf(xml, MAX_STRLEN,
 		 "<primitive id=\"%s\">"
    		 "<instance_attributes><attributes>", argv[1]);
	for (i = 2; i < argc; i += 3) {
		snprintf(buf, MAX_STRLEN,
			"<nvpair id=\"%s\" name=\"%s\" value=\"%s\"/>",
			argv[i], argv[i+1], argv[i+2]);
		strncat(xml, buf, MAX_STRLEN);
	}
	strncat(xml, "</attributes></instance_attributes></primitive>", MAX_STRLEN);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
char*
on_delete_rsc_param(char* argv[], int argc)
{
	ARGC_CHECK(2)

	if (delete_object("resources", "nvpair", argv[1]) < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
/* resource operations */
char*
on_get_rsc_ops(char* argv[], int argc)
{
	int i;
	resource_t* rsc;
	char* ret;
	struct ha_msg* ops;
	struct ha_msg* op;
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	ops = cl_get_struct((struct ha_msg*)rsc->xml, "operations");
	if (ops == NULL) {
		return ret;
	}
	for (i = 0; i < ops->nfields; i++) {
		if (STRNCMP_CONST(ops->names[i], "op") == 0) {
			op = (struct ha_msg*)ops->values[i];
			ret = mgmt_msg_append(ret, ha_msg_value(op, "id"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "name"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "interval"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "timeout"));
		}
	}
	return ret;
}
char*
on_update_rsc_ops(char* argv[], int argc)
{
	int rc, i;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	char buf[MAX_STRLEN];

	snprintf(xml, MAX_STRLEN,
 		 "<primitive id=\"%s\">"
    		 " <operations>", argv[1]);
	for (i = 2; i < argc; i += 4) {
		snprintf(buf, MAX_STRLEN,
			"<op id=\"%s\" name=\"%s\" interval=\"%s\" timeout=\"%s\"/>",
			argv[i], argv[i+1], argv[i+2], argv[i+3]);
		strncat(xml, buf, MAX_STRLEN);
	}
	strncat(xml, "</operations></primitive>", MAX_STRLEN);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
char*
on_delete_rsc_op(char* argv[], int argc)
{
	ARGC_CHECK(2)

	if (delete_object("resources", "op", argv[1]) < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
/* clone functions */
char*
on_get_clone(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	clone_variant_data_t* clone_data;
	char buf[MAX_STRLEN];
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	clone_data = (clone_variant_data_t*)rsc->variant_opaque;
	
	ret = mgmt_msg_append(ret, rsc->id);
	snprintf(buf, MAX_STRLEN, "%d", clone_data->clone_max);
	ret = mgmt_msg_append(ret, buf);
	snprintf(buf, MAX_STRLEN, "%d", clone_data->clone_node_max);
	ret = mgmt_msg_append(ret, buf);
	return ret;
}
char*
on_update_clone(char* argv[], int argc)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];

	ARGC_CHECK(4);
	snprintf(xml,MAX_STRLEN,
		 "<clone id=\"%s\"><instance_attributes><attributes>" \
		 "<nvpair id=\"clone_max\" name=\"clone_max\" value=\"%s\"/>" \
		 "<nvpair id=\"clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
		 "</attributes></instance_attributes></clone>",
		 argv[1],argv[2],argv[3]);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");
	rc = cib_conn->cmds->update(cib_conn, "resources", fragment, &output, 0);
	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
/* master functions */
char*
on_get_master(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	clone_variant_data_t* clone_data;
	const char * master_max_s;
	const char * master_node_max_s;
	char buf[MAX_STRLEN];
	pe_working_set_t data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	master_max_s = get_rsc_param(rsc, XML_RSC_ATTR_MASTER_MAX);
	master_node_max_s = get_rsc_param(rsc, XML_RSC_ATTR_MASTER_NODEMAX);

	ret = cl_strdup(MSG_OK);
	clone_data = (clone_variant_data_t*)rsc->variant_opaque;
	
	ret = mgmt_msg_append(ret, rsc->id);
	snprintf(buf, MAX_STRLEN, "%d", clone_data->clone_max);
	ret = mgmt_msg_append(ret, buf);
	snprintf(buf, MAX_STRLEN, "%d", clone_data->clone_node_max);
	ret = mgmt_msg_append(ret, buf);
	ret = mgmt_msg_append(ret, master_max_s);
	ret = mgmt_msg_append(ret, master_node_max_s);
	return ret;
}
char*
on_update_master(char* argv[], int argc)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];

	ARGC_CHECK(6);
	snprintf(xml,MAX_STRLEN,
		 "<master_slave id=\"%s\"><instance_attributes><attributes>" \
		 "<nvpair id=\"clone_max\" name=\"clone_max\" value=\"%s\"/>" \
		 "<nvpair id=\"clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
		 "<nvpair id=\"master_max\" name=\"master_max\" value=\"%s\"/>" \
		 "<nvpair id=\"master_node_max\" name=\"master_node_max\" value=\"%s\"/>" \
		 "</attributes></instance_attributes></master_slave>",
		 argv[1],argv[2],argv[3],argv[4],argv[5]);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");
	rc = cib_conn->cmds->update(cib_conn, "resources", fragment, &output, 0);
	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);

}

/* constraints functions */
char*
on_get_constraints(char* argv[], int argc)
{
	char* ret;
	GList* list;
	GList* cur;
	crm_data_t* cos = NULL;
	pe_working_set_t data_set;
	const char* path[] = {"configuration","constraints"}
	
	ARGC_CHECK(2);
	
	data_set = get_data_set();
	cos = find_xml_node_nested(data_set.input, path, 2);
	if (cos == NULL) {
		return  cl_strdup(MSG_FAIL);
	}
	ret = cl_strdup(MSG_OK);
	list = find_xml_node_list(cos, argv[1]);
	cur = list;
	while (cur != NULL) {
		crm_data_t* location = (crm_data_t*)cur->data;
		ret = mgmt_msg_append(ret, ha_msg_value(location, "id"));
		
		cur = g_list_next(cur);
	}
	g_list_free(list);
	return ret;
}

char*
on_get_constraint(char* argv[], int argc)
{
	char* ret;
	GList* list;
	GList* cur;
	crm_data_t* rule;
	
	GList* expr_list, *expr_cur;
	crm_data_t* cos = NULL;
	pe_working_set_t data_set;
	const char* path[] = {"configuration","constraints"}
	ARGC_CHECK(3);
	
	data_set = get_data_set();
	cos = find_xml_node_nested(data_set.input, path, 2);
	if (cos == NULL) {
		return  cl_strdup(MSG_FAIL);
	}
	ret = cl_strdup(MSG_OK);
	list = find_xml_node_list(cos, argv[1]);
	cur = list;
	while (cur != NULL) {
		crm_data_t* constraint = (crm_data_t*)cur->data;
		if (strncmp(argv[2],ha_msg_value(constraint, "id"), MAX_STRLEN)==0) {
			if (STRNCMP_CONST(argv[1],"rsc_location")==0) {
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "id"));
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "rsc"));
				rule = find_xml_node(constraint,"rule",TRUE);
				ret = mgmt_msg_append(ret, ha_msg_value(rule, "score"));
				expr_list = find_xml_node_list(rule, "expression");
				expr_cur = expr_list;
				while(expr_cur) {
					crm_data_t* expr = (crm_data_t*)expr_cur->data;
					ret = mgmt_msg_append(ret, ha_msg_value(expr, "id"));
					ret = mgmt_msg_append(ret, ha_msg_value(expr, "attribute"));
					ret = mgmt_msg_append(ret, ha_msg_value(expr, "operation"));
					ret = mgmt_msg_append(ret, ha_msg_value(expr, "value"));
					expr_cur = g_list_next(expr_cur);
				}
				g_list_free(expr_list);
			}
			else if (STRNCMP_CONST(argv[1],"rsc_order")==0) {
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "id"));
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "from"));
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "type"));
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "to"));
			}
			else if (STRNCMP_CONST(argv[1],"rsc_colocation")==0) {
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "id"));
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "from"));
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "to"));
				ret = mgmt_msg_append(ret, ha_msg_value(constraint, "score"));
			}
			break;
		}
		cur = g_list_next(cur);
	}
	g_list_free(list);
	return ret;
}
char*
on_delete_constraint(char* argv[], int argc)
{
	ARGC_CHECK(3)

	if (delete_object("constraints", argv[1], argv[2]) < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}

char*
on_update_constraint(char* argv[], int argc)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	int i;
	char xml[MAX_STRLEN];

	if (STRNCMP_CONST(argv[1],"rsc_location")==0) {
		snprintf(xml, MAX_STRLEN,
			 "<rsc_location id=\"%s\" rsc=\"%s\">" \
				"<rule id=\"prefered_%s\" score=\"%s\">",
		 	 argv[2], argv[3], argv[2], argv[4]);
		for (i = 0; i < (argc-5)/4; i++) {
			char expr[MAX_STRLEN];
			snprintf(expr, MAX_STRLEN,
				 "<expression attribute=\"%s\" id=\"%s\" operation=\"%s\" value=\"%s\"/>",
			 	 argv[5+i*4+1],argv[5+i*4],argv[5+i*4+2],argv[5+i*4+3]);
			strncat(xml, expr, MAX_STRLEN);
		}
		strncat(xml, "</rule></rsc_location>", MAX_STRLEN);
	}
	else if (STRNCMP_CONST(argv[1],"rsc_order")==0) {
		snprintf(xml, MAX_STRLEN,
			 "<rsc_order id=\"%s\" from=\"%s\" type=\"%s\" to=\"%s\"/>",
			 argv[2], argv[3], argv[4], argv[5]);
	}
	else if (STRNCMP_CONST(argv[1],"rsc_colocation")==0) {
		snprintf(xml, MAX_STRLEN,
			 "<rsc_colocation id=\"%s\" from=\"%s\" to=\"%s\" score=\"%s\"/>",
			 argv[2], argv[3], argv[4], argv[5]);
	}
	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "constraints");

	rc = cib_conn->cmds->update(
			cib_conn, "constraints", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}

