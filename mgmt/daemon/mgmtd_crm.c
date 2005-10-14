
/*
 * Linux HA Management Daemon
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

#include <unistd.h>
#include <glib.h>

#include <heartbeat.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_syslog.h>
#include "mgmtd.h"

#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/pengine/pengine.h>

extern resource_t *group_find_child(resource_t *rsc, const char *id);

cib_t*	cib_conn = NULL;
pe_working_set_t data_set;
int init_crm(void);
void final_crm(void);

static void on_cib_diff(const char *event, HA_Message *msg);
static void on_cib_query_done(const HA_Message* msg, int call_id, int rc,
	   	  crm_data_t* output, void* user_data);

static char* on_get_activenodes(char* argv[], int argc, int client_id);
static char* on_get_dc(char* argv[], int argc, int client_id);
static char* on_get_crm_config(char* argv[], int argc, int client_id);
static char* on_get_node_config(char* argv[], int argc, int client_id);
static char* on_get_running_rsc(char* argv[], int argc, int client_id);
static char* on_get_rsc_params(char* argv[], int argc, int client_id);
static char* on_get_rsc_attrs(char* argv[], int argc, int client_id);
static char* on_get_rsc_colos(char* argv[], int argc, int client_id);
static char* on_get_rsc_running_on(char* argv[], int argc, int client_id);
static char* on_get_rsc_ops(char* argv[], int argc, int client_id);

static char* on_get_all_rsc(char* argv[], int argc, int client_id);
static char* on_get_rsc_type(char* argv[], int argc, int client_id);
static char* on_get_sub_rsc(char* argv[], int argc, int client_id);

static char* on_del_rsc(char* argv[], int argc, int client_id);
static char* on_add_rsc(char* argv[], int argc, int client_id);
static char* on_add_grp(char* argv[], int argc, int client_id);

static char* on_update_crm_config(char* argv[], int argc, int client_id);
static char* on_update_rsc_params(char* argv[], int argc, int client_id);
static char* on_update_rsc_ops(char* argv[], int argc, int client_id);
static char* on_update_rsc_colo(char* argv[], int argc, int client_id);

static char* on_delete_rsc_param(char* argv[], int argc, int client_id);
static char* on_delete_rsc_op(char* argv[], int argc, int client_id);
static char* on_delete_rsc_colo(char* argv[], int argc, int client_id);

static resource_t* find_resource(GList* rsc_list, const char* id);
static int delete_object(const char* type, const char* entry, const char* id);

#define GET_RESOURCE()	if (argc != 2) { 					\
				return cl_strdup(MSG_FAIL); 			\
			} 							\
			rsc = find_resource(data_set.resources, argv[1]); 	\
			if (rsc == NULL) {					\
				return cl_strdup(MSG_FAIL); 			\
			}


void
on_cib_query_done(const HA_Message* msg, int call_id, int rc,
	   	  crm_data_t* output, void* user_data)
{
	static crm_data_t *save = NULL;

	if(rc == cib_ok) {
		crm_data_t *cib = NULL;
		cib = find_xml_node(output,XML_TAG_CIB,TRUE);
		/* FIXME: dealing with the situation with libxml is missing*/
		if (save != NULL) {
			ha_msg_del(save);
		}
		save = ha_msg_copy(cib);
		set_working_set_defaults(&data_set);
		data_set.input = save;
		stage0(&data_set);

		mgmtd_log(LOG_INFO,"update cib finished");
		fire_evt(EVT_CIB_CHANGED);
	}
}

int
init_crm(void)
{
	int ret = cib_ok;
	int i, max_try = 5;

	mgmtd_log(LOG_INFO,"init_crm");
	cib_conn = cib_new();
	for (i = 0; i < max_try ; i++) {
		ret = cib_conn->cmds->signon(cib_conn, mgmtd_name, cib_query);
		if (ret == cib_ok) {
			break;
		}
		mgmtd_log(LOG_INFO,"login to cib: %d, ret:%d",i,ret);
		sleep(1);
	}
	if (ret != cib_ok) {
		mgmtd_log(LOG_INFO,"login to cib failed");
		cib_conn = NULL;
		return -1;
	}

	ret = cib_conn->cmds->query(cib_conn, NULL, NULL, cib_scope_local);
	add_cib_op_callback(ret, FALSE, NULL, on_cib_query_done);

	ret = cib_conn->cmds->add_notify_callback(cib_conn, T_CIB_DIFF_NOTIFY
						  , on_cib_diff);

	reg_msg(MSG_ACTIVENODES, on_get_activenodes);
	reg_msg(MSG_DC, on_get_dc);
	reg_msg(MSG_CRM_CONFIG, on_get_crm_config);
	reg_msg(MSG_NODE_CONFIG, on_get_node_config);
	reg_msg(MSG_RUNNING_RSC, on_get_running_rsc);

	reg_msg(MSG_RSC_PARAMS, on_get_rsc_params);
	reg_msg(MSG_RSC_ATTRS, on_get_rsc_attrs);
	reg_msg(MSG_RSC_COLOS, on_get_rsc_colos);
	reg_msg(MSG_RSC_RUNNING_ON, on_get_rsc_running_on);
	reg_msg(MSG_RSC_OPS, on_get_rsc_ops);

	reg_msg(MSG_ALL_RSC, on_get_all_rsc);
	reg_msg(MSG_RSC_TYPE, on_get_rsc_type);
	reg_msg(MSG_SUB_RSC, on_get_sub_rsc);

	reg_msg(MSG_UP_CRM_CONFIG, on_update_crm_config);
	reg_msg(MSG_DEL_RSC, on_del_rsc);
	reg_msg(MSG_ADD_RSC, on_add_rsc);
	reg_msg(MSG_ADD_GRP, on_add_grp);

	reg_msg(MSG_UP_RSC_PARAMS, on_update_rsc_params);
	reg_msg(MSG_DEL_RSC_PARAM, on_delete_rsc_param);
	reg_msg(MSG_UP_RSC_OPS, on_update_rsc_ops);
	reg_msg(MSG_DEL_RSC_OP, on_delete_rsc_op);
	reg_msg(MSG_UP_RSC_COLO, on_update_rsc_colo);
	reg_msg(MSG_DEL_RSC_COLO, on_delete_rsc_colo);
	return 0;
}	

void
on_cib_diff(const char *event, HA_Message *msg)
{
	
	int ret = cib_conn->cmds->query(cib_conn, NULL, NULL, cib_scope_local);
	add_cib_op_callback(ret, FALSE, NULL, on_cib_query_done);
}

void
final_crm(void)
{
	if(cib_conn != NULL) {
		cib_conn->cmds->signoff(cib_conn);
		cib_conn = NULL;
	}
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

char* 
on_get_activenodes(char* argv[], int argc, int client_id)
{
	node_t* node;
	GList* cur = data_set.nodes;
	char* ret = cl_strdup(MSG_OK);
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
on_get_dc(char* argv[], int argc, int client_id)
{
	if (data_set.dc_node != NULL) {
		char* ret = cl_strdup(MSG_OK);
		ret = mgmt_msg_append(ret, data_set.dc_node->details->uname);
		return ret;
	}
	return cl_strdup(MSG_FAIL);
}

char* 
on_get_crm_config(char* argv[], int argc, int client_id)
{
	char buf [255];
	char* ret = cl_strdup(MSG_OK);
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
	return ret;
}

char*
on_get_node_config(char* argv[], int argc, int client_id)
{
	node_t* node;
	GList* cur = data_set.nodes;
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
on_get_running_rsc(char* argv[], int argc, int client_id)
{
	node_t* node;
	GList* cur = data_set.nodes;
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

char*
on_get_rsc_params(char* argv[], int argc, int client_id)
{
	int i;
	resource_t* rsc;
	char* ret;
	struct ha_msg* attrs;
	struct ha_msg* nvpair;

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
		if (strncmp(attrs->names[i], "nvpair", sizeof("nvpair")) == 0) {
			nvpair = (struct ha_msg*)attrs->values[i];
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "id"));
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "name"));
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "value"));
		}
	}
	return ret;
}

char*
on_get_rsc_attrs(char* argv[], int argc, int client_id)
{
	resource_t* rsc;
	char* ret;
	struct ha_msg* attrs;

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
on_get_rsc_colos(char* argv[], int argc, int client_id)
{
	resource_t* rsc;
	char* ret;
	GList* cur;

	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	cur = rsc->rsc_cons;
	while (cur != NULL) {
		rsc_colocation_t* colo = (rsc_colocation_t*)cur->data;
		ret = mgmt_msg_append(ret, colo->id);
		ret = mgmt_msg_append(ret, colo->rsc_lh->id);
		ret = mgmt_msg_append(ret, colo->rsc_rh->id);
		ret = mgmt_msg_append(ret, colo->state_lh);
		ret = mgmt_msg_append(ret, colo->state_rh);
		switch (colo->strength) {
			case pecs_ignore:
				ret = mgmt_msg_append(ret, "ignore");
				break;
			case pecs_must:
				ret = mgmt_msg_append(ret, "must");
				break;
			case pecs_must_not:
				ret = mgmt_msg_append(ret, "mustnot");
				break;
			case pecs_startstop:
				ret = mgmt_msg_append(ret, "startstop");
				break;
			default :
				ret = mgmt_msg_append(ret, "unknown");
				break;
		}

		cur = g_list_next(cur);
	}
	return ret;
}

char*
on_get_rsc_running_on(char* argv[], int argc, int client_id)
{
	resource_t* rsc;
	char* ret;
	GList* cur;

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
on_get_rsc_ops(char* argv[], int argc, int client_id)
{
	int i;
	resource_t* rsc;
	char* ret;
	struct ha_msg* ops;
	struct ha_msg* op;

	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	ops = cl_get_struct((struct ha_msg*)rsc->xml, "operations");
	if (ops == NULL) {
		return ret;
	}
	for (i = 0; i < ops->nfields; i++) {
		if (strncmp(ops->names[i], "op", sizeof("op")) == 0) {
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
on_get_all_rsc(char* argv[], int argc, int client_id)
{
	GList* cur;
	char* ret = cl_strdup(MSG_OK);
	cur = data_set.resources;
	while (cur != NULL) {
		resource_t* rsc = (resource_t*)cur->data;
		ret = mgmt_msg_append(ret, rsc->id);
		cur = g_list_next(cur);
	}
	return ret;
}
char*
on_get_rsc_type(char* argv[], int argc, int client_id)
{
	resource_t* rsc;
	char* ret;

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
		int clone_max_node;

		int active_clones;
		int max_nodes;
		
		gboolean interleave;
		gboolean ordered;

		gboolean notify_confirm;
		
		GListPtr child_list; /* resource_t* */
		
} clone_variant_data_t;

char*
on_get_sub_rsc(char* argv[], int argc, int client_id)
{
	resource_t* rsc;
	char* ret;
	GList* cur = NULL;
	GET_RESOURCE()

	if (rsc->variant == pe_group) {
		cur = ((group_variant_data_t*)rsc->variant_opaque)->child_list;
	}
	if (rsc->variant == pe_clone) {
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
char*
on_update_crm_config(char* argv[], int argc, int client_id)
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

	rc = cib_conn->cmds->modify(
			cib_conn, "crm_config", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}

	return cl_strdup(MSG_OK);
}
char*
on_del_rsc(char* argv[], int argc, int client_id)
{
	int rc;
	resource_t* rsc;
	const char* rsc_id;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];

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
char*
on_add_rsc(char* argv[], int argc, int client_id)
{
	int rc, i, in_group;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	char buf[MAX_STRLEN];
	/* argv[5]: group */
	if (argc < 6) {
		return cl_strdup(MSG_FAIL);
	}
	in_group = (strlen(argv[5]) != 0);
	if (!in_group) {
		snprintf(xml, MAX_STRLEN,
			"<primitive id=\"%s\" class=\"%s\" type=\"%s\" provider=\"%s\">"
    			"<instance_attributes> <attributes>", argv[1],argv[2],
			argv[3],argv[4]);
	}
	else {
		snprintf(xml, MAX_STRLEN,
			"<group id=\"%s\">"
			"<primitive id=\"%s\" class=\"%s\" type=\"%s\" provider=\"%s\">"
    			"<instance_attributes> <attributes>", argv[5], argv[1], argv[2],
			argv[3],argv[4]);
	}	
	for (i = 6; i < argc; i += 3) {
		snprintf(buf, MAX_STRLEN,
			"<nvpair id=\"%s\" name=\"%s\" value=\"%s\"/>",
			argv[i], argv[i+1],argv[i+2]);
		strncat(xml, buf, MAX_STRLEN);
	}
	strncat(xml, "</attributes></instance_attributes></primitive>", MAX_STRLEN);
	if (!in_group) {
		strncat(xml, "</group>", MAX_STRLEN);
	}


	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmtd_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	if (!in_group) {
		rc = cib_conn->cmds->create(
			cib_conn, "resources", fragment, &output, 0);
	}
	else {
		rc = cib_conn->cmds->modify(
			cib_conn, "resources", fragment, &output, 0);
	}
	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);

}
char*
on_add_grp(char* argv[], int argc, int client_id)
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
	mgmtd_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->create(
			cib_conn, "resources", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);

}
char*
on_update_rsc_params(char* argv[], int argc, int client_id)
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
	mgmtd_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->modify(
			cib_conn, "resources", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
char*
on_update_rsc_ops(char* argv[], int argc, int client_id)
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
	mgmtd_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->modify(
			cib_conn, "resources", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
char*
on_update_rsc_colo(char* argv[], int argc, int client_id)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];

	snprintf(xml, MAX_STRLEN,
 		 "<rsc_colocation id=\"%s\" from=\"%s\" to=\"%s\" score=\"%s\"/>",
		 argv[2], argv[1], argv[3], argv[4]);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmtd_log(LOG_INFO, "xml:%s",xml);
	fragment = create_cib_fragment(cib_object, "constraints");

	rc = cib_conn->cmds->modify(
			cib_conn, "constraints", fragment, &output, 0);

	if (rc < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
char*
on_delete_rsc_param(char* argv[], int argc, int client_id)
{
	ARGC_CHECK(2)

	if (delete_object("resources", "nvpair", argv[1]) < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
char*
on_delete_rsc_op(char* argv[], int argc, int client_id)
{
	ARGC_CHECK(2)

	if (delete_object("resources", "op", argv[1]) < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
}
char*
on_delete_rsc_colo(char* argv[], int argc, int client_id)
{
	ARGC_CHECK(2)

	if (delete_object("constraints", "rsc_colocation", argv[1]) < 0) {
		return cl_strdup(MSG_FAIL);
	}
	return cl_strdup(MSG_OK);
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
	mgmtd_log(LOG_INFO, "xml:%s",xml);

	rc = cib_conn->cmds->delete(
			cib_conn, type, cib_object, &output, 0);

	if (rc < 0) {
		return -1;
	}
	return 0;
}
