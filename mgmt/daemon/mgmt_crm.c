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
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_syslog.h>
#include <clplumbing/lsb_exitcodes.h>

#include "mgmt_internal.h"

#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/pengine/status.h>

extern resource_t *group_find_child(resource_t *rsc, const char *id);
extern crm_data_t * do_calculations(
	pe_working_set_t *data_set, crm_data_t *xml_input, ha_time_t *now);

cib_t*	cib_conn = NULL;
int in_shutdown = FALSE;
int init_crm(int cache_cib);
void final_crm(void);

static void on_cib_diff(const char *event, HA_Message *msg);

static char* on_get_cib_version(char* argv[], int argc);

static char* on_get_crm_config(char* argv[], int argc);
static char* on_update_crm_config(char* argv[], int argc);
static char* on_get_activenodes(char* argv[], int argc);
static char* on_get_crmnodes(char* argv[], int argc);
static char* on_get_dc(char* argv[], int argc);

static char* on_set_node_standby(char* argv[], int argc);
static char* on_get_node_config(char* argv[], int argc);
static char* on_get_running_rsc(char* argv[], int argc);

static char* on_del_rsc(char* argv[], int argc);
static char* on_cleanup_rsc(char* argv[], int argc);
static char* on_add_rsc(char* argv[], int argc);
static char* on_move_rsc(char* argv[], int argc);
static char* on_add_grp(char* argv[], int argc);

static char* on_update_clone(char* argv[], int argc);
static char* on_get_clone(char* argv[], int argc);

static char* on_update_master(char* argv[], int argc);
static char* on_get_master(char* argv[], int argc);

static char* on_get_all_rsc(char* argv[], int argc);
static char* on_get_rsc_type(char* argv[], int argc);
static char* on_get_sub_rsc(char* argv[], int argc);
static char* on_get_rsc_attrs(char* argv[], int argc);
static char* on_update_rsc_attr(char* argv[], int argc);
static char* on_get_rsc_running_on(char* argv[], int argc);
static char* on_get_rsc_status(char* argv[], int argc);

static char* on_get_rsc_params(char* argv[], int argc);
static char* on_update_rsc_params(char* argv[], int argc);
static char* on_delete_rsc_param(char* argv[], int argc);
static char* on_set_target_role(char* argv[], int argc);

static char* on_get_rsc_ops(char* argv[], int argc);
static char* on_update_rsc_ops(char* argv[], int argc);
static char* on_delete_rsc_op(char* argv[], int argc);

static char* on_get_constraints(char* argv[], int argc);
static char* on_get_constraint(char* argv[], int argc);
static char* on_update_constraint(char* argv[], int argc);
static char* on_delete_constraint(char* argv[], int argc);

static void get_instance_attributes_id(const char* rsc_id, char* id);
static void get_attr_id(const char* rsc_id, const char* attr, char* id);
static int delete_object(const char* type, const char* entry, const char* id, crm_data_t** output);
static GList* find_xml_node_list(crm_data_t *root, const char *search_path);
static int refresh_lrm(IPC_Channel *crmd_channel, const char *host_uname);
static int delete_lrm_rsc(IPC_Channel *crmd_channel, const char *host_uname, const char *rsc_id);
static pe_working_set_t* get_data_set(void);
static void free_data_set(pe_working_set_t* data_set);
static void on_cib_connection_destroy(gpointer user_data);
static char* crm_failed_msg(crm_data_t* output, int rc);
static const char* uname2id(const char* node);
static resource_t* get_parent(resource_t* child);
static int get_fix(const char* rsc_id, char* prefix, char* suffix, char* real_id);
static const char* get_rsc_tag(resource_t* rsc);
static int cl_msg_swap_offset(struct ha_msg* msg, int offset1, int offset2);

pe_working_set_t* cib_cached = NULL;
int cib_cache_enable = FALSE;

#define GET_RESOURCE()	rsc = pe_find_resource(data_set->resources, argv[1]);	\
	if (rsc == NULL) {						\
		free_data_set(data_set);				\
		return cl_strdup(MSG_FAIL"\nno such resource");		\
	}

/* internal functions */
GList* find_xml_node_list(crm_data_t *root, const char *child_name)
{
	int i;
	GList* list = NULL;
	if (root == NULL) {
		return NULL;
	}
	for (i = 0; i < root->nfields; i++ ) {
		if (strncmp(root->names[i], child_name, MAX_STRLEN) == 0) {
			list = g_list_append(list, root->values[i]);
		}
	}
	return list;
}

int
delete_object(const char* type, const char* entry, const char* id, crm_data_t** output) 
{
	int rc;
	crm_data_t* cib_object = NULL;
	char xml[MAX_STRLEN];

	snprintf(xml, MAX_STRLEN, "<%s id=\"%s\">", entry, id);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return -1;
	}
	
	mgmt_log(LOG_INFO, "(delete)xml:%s",xml);

	rc = cib_conn->cmds->delete(
			cib_conn, type, cib_object, output, cib_sync_call);
	free_xml(cib_object);
	if (rc < 0) {
		return -1;
	}
	return 0;
}

pe_working_set_t*
get_data_set(void) 
{
	pe_working_set_t* data_set;
	
	if (cib_cache_enable) {
		if (cib_cached != NULL) {
			return cib_cached;
		}
	}
	
	data_set = (pe_working_set_t*)cl_malloc(sizeof(pe_working_set_t));
	if (data_set == NULL) {
		mgmt_log(LOG_ERR, "%s:Can't alloc memory for data set.",__FUNCTION__);
		return NULL;
	}
	set_working_set_defaults(data_set);
	data_set->input = get_cib_copy(cib_conn);
	data_set->now = new_ha_date(TRUE);

	cluster_status(data_set);
	
	if (cib_cache_enable) {
		cib_cached = data_set;
	}
	return data_set;
}

void 
free_data_set(pe_working_set_t* data_set)
{
	/* we only release the cib when cib is not cached.
	   the cached cib will be released in on_cib_diff() */
	if (!cib_cache_enable) {
		cleanup_calculations(data_set);
		cl_free(data_set);
	}
}	
char* 
crm_failed_msg(crm_data_t* output, int rc) 
{
	const char* reason = NULL;
	crm_data_t* failed_tag;
	char* ret;
	
	/* beekhof:
		you can pretend that the return code is success, 
		its an internal CIB thing*/
	if (rc == cib_diff_resync) {
		if (output != NULL) {
			free_xml(output);
		}
		return cl_strdup(MSG_OK);
	}
	
	ret = cl_strdup(MSG_FAIL);
	ret = mgmt_msg_append(ret, cib_error2string((enum cib_errors)rc));
	
	if (output == NULL) {
		return ret;
	}
	
	failed_tag = cl_get_struct(output, XML_FAIL_TAG_CIB);
	if (failed_tag != NULL) {
		reason = ha_msg_value(failed_tag, XML_FAILCIB_ATTR_REASON);
		if (reason != NULL) {
			ret = mgmt_msg_append(ret, reason);
		}
	}
	free_xml(output);
	
	return ret;
}
const char*
uname2id(const char* uname)
{
	node_t* node;
	GList* cur;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	cur = data_set->nodes;
	while (cur != NULL) {
		node = (node_t*) cur->data;
		if (strncmp(uname,node->details->uname,MAX_STRLEN) == 0) {
			return node->details->id;
		}
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return NULL;
}
static resource_t* 
get_parent(resource_t* child)
{
	GList* cur;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	cur = data_set->resources;
	while (cur != NULL) {
		resource_t* rsc = (resource_t*)cur->data;
		if(rsc->orphan == FALSE || rsc->role != RSC_ROLE_STOPPED) {
			GList* child_list = rsc->fns->children(rsc);
			if (g_list_find(child_list, child) != NULL) {
				free_data_set(data_set);
				return rsc;
			}
		}
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return NULL;
}
static const char* 
get_rsc_tag(resource_t* rsc)
{
	switch (rsc->variant) {
		case pe_native:
			return "primitive";
		case pe_group:
			return "group";
		case pe_clone:
			return "clone";
		case pe_master:
			return "master_slave";
		case pe_unknown:
		default:
			return "unknown";
	}
	
}
static int
get_fix(const char* rsc_id, char* prefix, char* suffix, char* real_id)
{
	resource_t* rsc;
	resource_t* parent;
	pe_working_set_t* data_set;
	char* colon;
	char parent_tag[MAX_STRLEN];
	char rsc_tag[MAX_STRLEN];
		
	data_set = get_data_set();
	rsc = pe_find_resource(data_set->resources, rsc_id);	
	if (rsc == NULL) {
		free_data_set(data_set);	
		return -1;
	}
	strncpy(rsc_tag, get_rsc_tag(rsc), MAX_STRLEN);
	strncpy(real_id, rsc_id, MAX_STRLEN);

	parent = get_parent(rsc);
	if (parent == NULL) {
		snprintf(prefix, MAX_STRLEN,"<%s id=\"%s\">",rsc_tag, rsc_id);
		snprintf(suffix, MAX_STRLEN,"</%s>", rsc_tag);
	}
	else {
		colon = strrchr(real_id, ':');
		if (colon != NULL) {
			*colon = '\0';
		}
		strncpy(parent_tag, get_rsc_tag(parent), MAX_STRLEN);
		
		snprintf(prefix, MAX_STRLEN,"<%s id=\"%s\"><%s id=\"%s\">"
		, 	parent_tag, parent->id, rsc_tag, real_id);
		snprintf(suffix, MAX_STRLEN,"</%s></%s>",rsc_tag, parent_tag);
	}
	free_data_set(data_set);
	return 0;
}
static void
get_instance_attributes_id(const char* rsc_id, char* id)
{
	resource_t* rsc;
	const char* cur_id;
	pe_working_set_t* data_set;
	struct ha_msg* attrs;
	
	data_set = get_data_set();
	rsc = pe_find_resource(data_set->resources, rsc_id);	
	if (rsc == NULL) {
		snprintf(id, MAX_STRLEN, "%s_instance_attrs", rsc_id);
		free_data_set(data_set);
		return;
	}
	attrs = cl_get_struct((struct ha_msg*)rsc->xml, "instance_attributes");
	if (attrs == NULL) {
		snprintf(id, MAX_STRLEN, "%s_instance_attrs", rsc_id);
		free_data_set(data_set);
		return;
	}
	cur_id = ha_msg_value(attrs, "id");
	if (cur_id == NULL) {
		snprintf(id, MAX_STRLEN, "%s_instance_attrs", rsc_id);
		free_data_set(data_set);
		return;
	}
	strncpy(id, cur_id, MAX_STRLEN);
	free_data_set(data_set);
	return;					

}
static void
get_attr_id(const char* rsc_id, const char* attr, char* id)
{
	int i;
	resource_t* rsc;
	const char * name_nvpair;
	const char * id_nvpair;
	struct ha_msg* attrs;
	struct ha_msg* nvpair;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	rsc = pe_find_resource(data_set->resources, rsc_id);	
	if (rsc == NULL) {
		snprintf(id, MAX_STRLEN,  "%s_%s", rsc_id, attr);
		free_data_set(data_set);
		return;
	}

	attrs = cl_get_struct((struct ha_msg*)rsc->xml, "instance_attributes");
	if(attrs == NULL) {
		snprintf(id, MAX_STRLEN,  "%s_%s", rsc_id, attr);
		free_data_set(data_set);
		return;
	}
	attrs = cl_get_struct(attrs, "attributes");
	if(attrs == NULL) {
		snprintf(id, MAX_STRLEN,  "%s_%s", rsc_id, attr);
		free_data_set(data_set);
		return;
	}
	for (i = 0; i < attrs->nfields; i++) {
		if (STRNCMP_CONST(attrs->names[i], "nvpair") == 0) {
			nvpair = (struct ha_msg*)attrs->values[i];
			name_nvpair = ha_msg_value(nvpair, "name");
			if ( strncmp(name_nvpair,attr,MAX_STRLEN) == 0 ) {
				id_nvpair = ha_msg_value(nvpair,"id");
				if (id_nvpair != NULL) {
					strncpy(id,id_nvpair,MAX_STRLEN);
					free_data_set(data_set);
					return;					
				}
			}
		}
	}
	snprintf(id, MAX_STRLEN,  "%s_%s", rsc_id, attr);
	free_data_set(data_set);
	return;
}

/* mgmtd functions */
int
init_crm(int cache_cib)
{
	int ret = cib_ok;
	int i, max_try = 5;
	
	mgmt_log(LOG_INFO,"init_crm");
	crm_log_level = LOG_ERR;
	cib_conn = cib_new();
	in_shutdown = FALSE;
	
	cib_cache_enable = cache_cib?TRUE:FALSE;
	cib_cached = NULL;
	
	for (i = 0; i < max_try ; i++) {
		ret = cib_conn->cmds->signon(cib_conn, client_name, cib_command);
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
	ret = cib_conn->cmds->set_connection_dnotify(cib_conn
			, on_cib_connection_destroy);

	reg_msg(MSG_CIB_VERSION, on_get_cib_version);
	reg_msg(MSG_CRM_CONFIG, on_get_crm_config);
	reg_msg(MSG_UP_CRM_CONFIG, on_update_crm_config);
	
	reg_msg(MSG_DC, on_get_dc);
	reg_msg(MSG_ACTIVENODES, on_get_activenodes);
	reg_msg(MSG_CRMNODES, on_get_crmnodes);
	reg_msg(MSG_NODE_CONFIG, on_get_node_config);
	reg_msg(MSG_RUNNING_RSC, on_get_running_rsc);
	reg_msg(MSG_STANDBY, on_set_node_standby);
	
	reg_msg(MSG_DEL_RSC, on_del_rsc);
	reg_msg(MSG_CLEANUP_RSC, on_cleanup_rsc);
	reg_msg(MSG_ADD_RSC, on_add_rsc);
	reg_msg(MSG_MOVE_RSC, on_move_rsc);
	reg_msg(MSG_ADD_GRP, on_add_grp);
	
	reg_msg(MSG_ALL_RSC, on_get_all_rsc);
	reg_msg(MSG_SUB_RSC, on_get_sub_rsc);
	reg_msg(MSG_RSC_ATTRS, on_get_rsc_attrs);
	reg_msg(MSG_RSC_RUNNING_ON, on_get_rsc_running_on);
	reg_msg(MSG_RSC_STATUS, on_get_rsc_status);
	reg_msg(MSG_RSC_TYPE, on_get_rsc_type);
	reg_msg(MSG_UP_RSC_ATTR, on_update_rsc_attr);
		
	
	reg_msg(MSG_RSC_PARAMS, on_get_rsc_params);
	reg_msg(MSG_UP_RSC_PARAMS, on_update_rsc_params);
	reg_msg(MSG_DEL_RSC_PARAM, on_delete_rsc_param);
	reg_msg(MSG_SET_TARGET_ROLE, on_set_target_role);
	
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
		in_shutdown = TRUE;
		cib_conn->cmds->signoff(cib_conn);
		cib_conn = NULL;
	}
}

/* event handler */
void
on_cib_diff(const char *event, HA_Message *msg)
{
	if (debug_level) {
		mgmt_debug(LOG_DEBUG,"update cib finished");
	}
	if (cib_cache_enable) {
		if (cib_cached != NULL) {
			cleanup_calculations(cib_cached);
			cl_free(cib_cached);
			cib_cached = NULL;
		}
	}
	
	fire_event(EVT_CIB_CHANGED);
}
void
on_cib_connection_destroy(gpointer user_data)
{
	fire_event(EVT_DISCONNECTED);
	cib_conn = NULL;
	if (!in_shutdown) {
		mgmt_log(LOG_ERR,"Connection to the CIB terminated... exiting");
		/*cib exits abnormally, mgmtd exits too and
		wait heartbeat	restart us in order*/
		exit(LSB_EXIT_OK);
	}
	return;
}

/* cluster  functions */
char* 
on_get_cib_version(char* argv[], int argc)
{
	const char* version = NULL;
	pe_working_set_t* data_set;
	char* ret;
	
	data_set = get_data_set();
	version = ha_msg_value(data_set->input, "num_updates");
	if (version != NULL) {
		ret = cl_strdup(MSG_OK);
		ret = mgmt_msg_append(ret, version);
	}
	else {
		ret = cl_strdup(MSG_FAIL);
	}	
	free_data_set(data_set);
	return ret;
}
char* 
on_get_crm_config(char* argv[], int argc)
{
	char buf [255];
	pe_working_set_t* data_set;
	char* ret = cl_strdup(MSG_OK);
	data_set = get_data_set();
	
	ret = mgmt_msg_append(ret, data_set->transition_idle_timeout);
	ret = mgmt_msg_append(ret, data_set->symmetric_cluster?"True":"False");
	ret = mgmt_msg_append(ret, data_set->stonith_enabled?"True":"False");
	
	switch (data_set->no_quorum_policy) {
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
	snprintf(buf, 255, "%d", data_set->default_resource_stickiness);
	ret = mgmt_msg_append(ret, buf);
	ret = mgmt_msg_append(ret, data_set->have_quorum?"True":"False");
	snprintf(buf, 255, "%d", data_set->default_resource_fail_stickiness);
	
	ret = mgmt_msg_append(ret, buf);
	free_data_set(data_set);
	return ret;
}
char*
on_update_crm_config(char* argv[], int argc)
{
	int rc;
	GList* cur;
	crm_data_t* attr;
	crm_data_t* attrs;
	const char* id = NULL;
	pe_working_set_t* data_set;
	const char* path[] = {"configuration","crm_config","cluster_property_set", "attributes"};
	
	ARGC_CHECK(3);
	data_set = get_data_set();
	attrs = find_xml_node_nested(data_set->input, path, 4);

	if (attrs != NULL) {
		cur = find_xml_node_list(attrs, "nvpair");
		while (cur != NULL) {
			attr = (crm_data_t*)cur->data;
			if(strncmp(ha_msg_value(attr,"name"),argv[1], MAX_STRLEN)==0) {
				id = ha_msg_value(attr,"id");
				break;
			}
			cur = g_list_next(cur);
		}
	}

	rc = update_attr(cib_conn, cib_sync_call, XML_CIB_TAG_CRMCONFIG, NULL
	, 		CIB_OPTIONS_FIRST, id, argv[1], argv[2]);
	
	free_data_set(data_set);
	if (rc == cib_ok) {
		return cl_strdup(MSG_OK);
	}
	else {
		return cl_strdup(MSG_FAIL);
	}
}

/* node functions */
char*
on_get_activenodes(char* argv[], int argc)
{
	node_t* node;
	GList* cur;
	char* ret;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	cur = data_set->nodes;
	ret = cl_strdup(MSG_OK);
	while (cur != NULL) {
		node = (node_t*) cur->data;
		if (node->details->online) {
			ret = mgmt_msg_append(ret, node->details->uname);
		}
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return ret;
}

char*
on_get_crmnodes(char* argv[], int argc)
{
	node_t* node;
	GList* cur;
	char* ret;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	cur = data_set->nodes;
	ret = cl_strdup(MSG_OK);
	while (cur != NULL) {
		node = (node_t*) cur->data;
		ret = mgmt_msg_append(ret, node->details->uname);
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return ret;
}

char* 
on_get_dc(char* argv[], int argc)
{
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	if (data_set->dc_node != NULL) {
		char* ret = cl_strdup(MSG_OK);
		ret = mgmt_msg_append(ret, data_set->dc_node->details->uname);
		free_data_set(data_set);
		return ret;
	}
	free_data_set(data_set);
	return cl_strdup(MSG_FAIL);
}


char*
on_get_node_config(char* argv[], int argc)
{
	node_t* node;
	GList* cur;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	cur = data_set->nodes;
	ARGC_CHECK(2);
	while (cur != NULL) {
		node = (node_t*) cur->data;
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
			free_data_set(data_set);
			return ret;
		}
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return cl_strdup(MSG_FAIL);
}

char*
on_get_running_rsc(char* argv[], int argc)
{
	node_t* node;
	GList* cur;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	cur = data_set->nodes;
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
				free_data_set(data_set);
				return ret;
			}
		}
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return cl_strdup(MSG_FAIL);
}
char*
on_set_node_standby(char* argv[], int argc)
{
	int rc;
	const char* id = NULL;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];

	ARGC_CHECK(3);
	id = uname2id(argv[1]);
	if (id == NULL) {
		return cl_strdup(MSG_FAIL"\nno such node");
	}
	
	snprintf(xml, MAX_STRLEN, 
		"<node id=\"%s\"><instance_attributes id=\"nodes-\"%s\">"
		"<attributes><nvpair id=\"standby-%s\" name=\"standby\" value=\"%s\"/>"
           	"</attributes></instance_attributes></node>", 
           	id, id, id, argv[2]);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}

	fragment = create_cib_fragment(cib_object, "nodes");

	mgmt_log(LOG_INFO, "(update)xml:%s",xml);

	rc = cib_conn->cmds->update(
			cib_conn, "nodes", fragment, &output, cib_sync_call);

	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);

}
/* resource functions */
/* add/delete resource */
char*
on_del_rsc(char* argv[], int argc)
{
	int rc;
	resource_t* rsc;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	snprintf(xml, MAX_STRLEN, "<%s id=\"%s\"/>",get_rsc_tag(rsc), rsc->id);
	free_data_set(data_set);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}

	mgmt_log(LOG_INFO, "(delete resources)xml:%s",xml);
	rc = cib_conn->cmds->delete(
			cib_conn, "resources", cib_object, &output, cib_sync_call);
	
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);
}
static int
delete_lrm_rsc(IPC_Channel *crmd_channel, const char *host_uname, const char *rsc_id)
{
	HA_Message *cmd = NULL;
	crm_data_t *msg_data = NULL;
	crm_data_t *rsc = NULL;
	crm_data_t *params = NULL;
	char our_pid[11];
	char *key = NULL; 
	
	snprintf(our_pid, 10, "%d", getpid());
	our_pid[10] = '\0';
	key = crm_concat(client_name, our_pid, '-');
	
	msg_data = create_xml_node(NULL, XML_GRAPH_TAG_RSC_OP);
	crm_xml_add(msg_data, XML_ATTR_TRANSITION_KEY, key);
	
	rsc = create_xml_node(msg_data, XML_CIB_TAG_RESOURCE);
	crm_xml_add(rsc, XML_ATTR_ID, rsc_id);

	params = create_xml_node(msg_data, XML_TAG_ATTRS);
	crm_xml_add(params, XML_ATTR_CRM_VERSION, CRM_FEATURE_SET);
	
	cmd = create_request(CRM_OP_LRM_DELETE, msg_data, host_uname,
			     CRM_SYSTEM_CRMD, client_name, our_pid);

	free_xml(msg_data);
	crm_free(key);

	if(send_ipc_message(crmd_channel, cmd)) {
		crm_msg_del(cmd);
		return 0;
	}
	crm_msg_del(cmd);
	return -1;
}

static int
refresh_lrm(IPC_Channel *crmd_channel, const char *host_uname)
{
	HA_Message *cmd = NULL;
	char our_pid[11];
	
	snprintf(our_pid, 10, "%d", getpid());
	our_pid[10] = '\0';
	
	cmd = create_request(CRM_OP_LRM_REFRESH, NULL, host_uname,
			     CRM_SYSTEM_CRMD, client_name, our_pid);
	
	if(send_ipc_message(crmd_channel, cmd)) {
		crm_msg_del(cmd);
		return 0;
	}
	crm_msg_del(cmd);
	return -1;
}

char*
on_cleanup_rsc(char* argv[], int argc)
{
	IPC_Channel *crmd_channel = NULL;
	char our_pid[11];
	char *now_s = NULL;
	time_t now = time(NULL);
	
	ARGC_CHECK(2);
	snprintf(our_pid, 10, "%d", getpid());
	our_pid[10] = '\0';
	
	init_client_ipc_comms(CRM_SYSTEM_CRMD, NULL,
				    NULL, &crmd_channel);

	send_hello_message(crmd_channel, our_pid, client_name, "0", "1");
	delete_lrm_rsc(crmd_channel, NULL, argv[1]);
	refresh_lrm(crmd_channel, NULL); 

	/* force the TE to start a transition */
	sleep(5); /* wait for the refresh */
	now_s = crm_itoa(now);
	update_attr(cib_conn, cib_sync_call,
		    NULL, NULL, NULL, NULL, "last-lrm-refresh", now_s);
	crm_free(now_s);

	
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
	crm_data_t* output = NULL;
	char xml[MAX_STRLEN];
	char buf[MAX_STRLEN];
	char inst_attrs_id[MAX_STRLEN];
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
		get_instance_attributes_id(argv[7], inst_attrs_id);
		snprintf(buf, MAX_STRLEN,
			 "<clone id=\"%s\"><instance_attributes id=\"%s\"><attributes>" \
			 "<nvpair id=\"%s_clone_max\" name=\"clone_max\" value=\"%s\"/>" \
			 "<nvpair id=\"%s_clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
			 "</attributes>	</instance_attributes> ",
			 argv[7], inst_attrs_id, argv[7], argv[8],argv[7], argv[9]);
		strncat(xml, buf, MAX_STRLEN);
	}
	if (master) {
		get_instance_attributes_id(argv[7], inst_attrs_id);
		snprintf(buf, MAX_STRLEN,
			 "<master_slave id=\"%s\"><instance_attributes id=\"%s\"><attributes>" \
			 "<nvpair id=\"%s_clone_max\" name=\"clone_max\" value=\"%s\"/>" \
			 "<nvpair id=\"%s_clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
			 "<nvpair id=\"%s_master_max\" name=\"master_max\" value=\"%s\"/>" \
			 "<nvpair id=\"%s_master_node_max\" name=\"master_node_max\" value=\"%s\"/>" \
			 "</attributes>	</instance_attributes>",
			 argv[7], inst_attrs_id, argv[7], argv[8], argv[7], argv[9],
			 argv[7], argv[10], argv[7], argv[11]);
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
			 "<instance_attributes id=\"%s_instance_attrs\"> <attributes>"
			 , argv[1],argv[2], argv[3],argv[4], argv[1]);
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
	mgmt_log(LOG_INFO, "on_add_rsc:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	if (in_group || clone || master) {
		rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, cib_sync_call);
	}
	else {
		rc = cib_conn->cmds->create(
			cib_conn, "resources", fragment, &output, cib_sync_call);
	}
	
	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);

}

int
cl_msg_swap_offset(struct ha_msg* msg, int offset1, int offset2)
{
	char* name;
	int nlen;
	void* value;
	int vlen;
	int type;
	
	name = msg->names[offset1];
	nlen = msg->nlens[offset1];
	value = msg->values[offset1];
	vlen = msg->vlens[offset1];
	type = msg->types[offset1];
		
	msg->names[offset1] = msg->names[offset2];
	msg->nlens[offset1] = msg->nlens[offset2];
	msg->values[offset1] = msg->values[offset2];
	msg->vlens[offset1] = msg->vlens[offset2];
	msg->types[offset1] = msg->types[offset2];
		
	msg->names[offset2] = name;
	msg->nlens[offset2] = nlen;
	msg->values[offset2] = value;
	msg->vlens[offset2] = vlen;
	msg->types[offset2] = type;
	
	return HA_OK;
}

char*
on_move_rsc(char* argv[], int argc)
{
	int i, rc, pos = -1;
	int first_child = -1;
	int last_child = -1;
	const char* child_id;
	struct ha_msg* child;
	resource_t* rsc;
	resource_t* parent;
	pe_working_set_t* data_set;
	crm_data_t* output = NULL;
	
	data_set = get_data_set();
	GET_RESOURCE()
	parent = get_parent(rsc);
	if (parent == NULL || parent->variant != pe_group) {
		free_data_set(data_set);
		return cl_strdup(MSG_FAIL);
	}
	for (i=0; i < parent->xml->nfields ; i++){
		if (STRNCMP_CONST(parent->xml->names[i], "primitive")!=0) {
			continue;
		}
		child = (struct ha_msg*)parent->xml->values[i];
		if (first_child == -1) {
			first_child = i;
		}
		last_child = i;
		child_id = ha_msg_value(child,"id");
		if (strcmp(child_id, argv[1]) == 0) {
			mgmt_log(LOG_INFO,"find %s !",child_id);
			pos = i;
		}
	}	
	if (STRNCMP_CONST(argv[2],"up")==0) {
		if (pos-1<first_child) {
			free_data_set(data_set);
			return cl_strdup(MSG_FAIL);
		}
		cl_msg_swap_offset(parent->xml, pos-1, pos);
	}
	else if (STRNCMP_CONST(argv[2],"down")==0) {
		if (pos+1>last_child) {
			free_data_set(data_set);
			return cl_strdup(MSG_FAIL);
		}
		cl_msg_swap_offset(parent->xml, pos, pos+1);
	}
	else {
		free_data_set(data_set);
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_move_rsc:%s",dump_xml_formatted(parent->xml));
	free_data_set(data_set);
	
	rc = cib_conn->cmds->variant_op(
			cib_conn, CIB_OP_REPLACE, NULL,"resources",
			parent->xml, &output, cib_sync_call);
	
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	
	return cl_strdup(MSG_OK);
}

char*
on_add_grp(char* argv[], int argc)
{
	int rc, i;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	char buf[MAX_STRLEN];
	
	snprintf(xml, MAX_STRLEN,"<group id=\"%s\">" \
		"<instance_attributes id=\"%s_instance_attrs\">" \
		"<attributes>", argv[1], argv[1]);
	for (i = 2; i < argc; i += 3) {
		snprintf(buf, MAX_STRLEN,
			 "<nvpair id=\"%s\" name=\"%s\" value=\"%s\"/>",
			 argv[i], argv[i+1],argv[i+2]);
		strncat(xml, buf, MAX_STRLEN);
	}
	strncat(xml,"</attributes></instance_attributes> ", MAX_STRLEN);
	strncat(xml,"</group>",MAX_STRLEN);
	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_add_grp:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");
	rc = cib_conn->cmds->create(cib_conn, "resources", fragment, &output, cib_sync_call);
	
	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);
}
/* get all resources*/
char*
on_get_all_rsc(char* argv[], int argc)
{
	GList* cur;
	char* ret;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	ret = cl_strdup(MSG_OK);
	cur = data_set->resources;
	while (cur != NULL) {
		resource_t* rsc = (resource_t*)cur->data;
		if(rsc->orphan == FALSE || rsc->role != RSC_ROLE_STOPPED) {
			ret = mgmt_msg_append(ret, rsc->id);
		}
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return ret;
}
/* basic information of resource */
char*
on_get_rsc_attrs(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	const char* value;
	struct ha_msg* attrs;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	attrs = (struct ha_msg*)rsc->xml;
	ret = mgmt_msg_append(ret, ha_msg_value(attrs, "id"));
	ret = mgmt_msg_append(ret, ha_msg_value(attrs, "description"));
	if (rsc->variant == pe_native) {
		ret = mgmt_msg_append(ret, ha_msg_value(attrs, "class"));
		ret = mgmt_msg_append(ret, ha_msg_value(attrs, "provider"));
		ret = mgmt_msg_append(ret, ha_msg_value(attrs, "type"));
	}
	value = ha_msg_value(attrs, "is_managed");
	ret = mgmt_msg_append(ret, value?value:"#default");
	value = ha_msg_value(attrs, "restart_type");
	ret = mgmt_msg_append(ret, value?value:"#default");
	value = ha_msg_value(attrs, "multiple_active");
	ret = mgmt_msg_append(ret, value?value:"#default");
	value = ha_msg_value(attrs, "resource_stickiness");
	ret = mgmt_msg_append(ret, value?value:"#default");
	
	switch (rsc->variant) {
		case pe_group:
			value = ha_msg_value(attrs, "ordered");
			ret = mgmt_msg_append(ret, value?value:"#default");
			value = ha_msg_value(attrs, "collocated");
			ret = mgmt_msg_append(ret, value?value:"#default");
			break;
		case pe_clone:
		case pe_master:
			value = ha_msg_value(attrs, "notify");
			ret = mgmt_msg_append(ret, value?value:"#default");
			value = ha_msg_value(attrs, "globally_unique");
			ret = mgmt_msg_append(ret, value?value:"#default");
			value = ha_msg_value(attrs, "ordered");
			ret = mgmt_msg_append(ret, value?value:"#default");
			value = ha_msg_value(attrs, "interleave");
			ret = mgmt_msg_append(ret, value?value:"#default");
			break;
		default:
			break;
	}
	free_data_set(data_set);
	return ret;
}
char*
on_get_rsc_running_on(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	GList* cur;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	cur = rsc->running_on;
	while (cur != NULL) {
		node_t* node = (node_t*)cur->data;
		ret = mgmt_msg_append(ret, node->details->uname);
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
	return ret;
}
char*
on_get_rsc_status(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	pe_working_set_t* data_set;
	
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
			if( rsc->failed ) {
				ret = mgmt_msg_append(ret, "failed");
				break;
			}
			if( g_list_length(rsc->running_on) == 0) {
				ret = mgmt_msg_append(ret, "not running");
				break;
			}
			if( g_list_length(rsc->running_on) > 1) {
				ret = mgmt_msg_append(ret, "multi-running");
				break;
			}
			if( rsc->role==RSC_ROLE_SLAVE ) {
				ret = mgmt_msg_append(ret, "running (Slave)");		
			}
			else if( rsc->role==RSC_ROLE_MASTER) {
				ret = mgmt_msg_append(ret, "running (Master)");		
			}
			else {
				ret = mgmt_msg_append(ret, "running");		
			}
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
	free_data_set(data_set);
	return ret;
}

char*
on_get_rsc_type(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	pe_working_set_t* data_set;
	
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
	free_data_set(data_set);
	return ret;
}

char*
on_get_sub_rsc(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	GList* cur = NULL;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()
		
	cur = rsc->fns->children(rsc);
	
	ret = cl_strdup(MSG_OK);
	while (cur != NULL) {
		resource_t* rsc = (resource_t*)cur->data;
		ret = mgmt_msg_append(ret, rsc->id);
		cur = g_list_next(cur);
	}
	free_data_set(data_set);
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
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	attrs = cl_get_struct((struct ha_msg*)rsc->xml, "instance_attributes");
	if(attrs == NULL) {
		free_data_set(data_set);
		return ret;
	}
	attrs = cl_get_struct(attrs, "attributes");
	if(attrs == NULL) {
		free_data_set(data_set);
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
	free_data_set(data_set);
	return ret;
}
char*
on_update_rsc_attr(char* argv[], int argc)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	char real_id[MAX_STRLEN];
	char parent_tag[MAX_STRLEN];
	char rsc_tag[MAX_STRLEN];
	pe_working_set_t* data_set;
	resource_t* rsc;
	resource_t* parent;
	
	data_set = get_data_set();
	rsc = pe_find_resource(data_set->resources, argv[1]);	
	if (rsc == NULL) {
		free_data_set(data_set);
		return cl_strdup(MSG_FAIL);;
	}
	parent = get_parent(rsc);
	
	strncpy(rsc_tag, get_rsc_tag(rsc), MAX_STRLEN);
	strncpy(real_id, argv[1], MAX_STRLEN);

	parent = get_parent(rsc);
	if (parent == NULL) {
		snprintf(xml, MAX_STRLEN,"<%s id=\"%s\" %s=\"%s\"/>"
		,	rsc_tag, argv[1], argv[2], argv[3]);
	}
	else {
		char* colon = strrchr(real_id, ':');
		if (colon != NULL) {
			*colon = '\0';
		}
		strncpy(parent_tag, get_rsc_tag(parent), MAX_STRLEN);
		
		snprintf(xml, MAX_STRLEN,"<%s id=\"%s\">" \
			"<%s id=\"%s\" %s=\"%s\"/></%s>" \
			,parent_tag, parent->id, rsc_tag, real_id
			, argv[2],argv[3],parent_tag);
	}

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_update_rsc_attr:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, cib_sync_call);

	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);
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
	char prefix[MAX_STRLEN];
	char suffix[MAX_STRLEN];
	char real_id[MAX_STRLEN];
	char inst_attrs_id[MAX_STRLEN];	
	
	if(get_fix(argv[1], prefix, suffix, real_id) == -1) {
		return cl_strdup(MSG_FAIL);
	}
	get_instance_attributes_id(argv[1],inst_attrs_id);
	snprintf(xml, MAX_STRLEN,
    		 "%s<instance_attributes id=\"%s\"><attributes>",
    		 prefix , inst_attrs_id);
	for (i = 2; i < argc; i += 3) {
		snprintf(buf, MAX_STRLEN,
			"<nvpair id=\"%s\" name=\"%s\" value=\"%s\"/>",
			argv[i], argv[i+1], argv[i+2]);
		strncat(xml, buf, MAX_STRLEN);
	}
	strncat(xml, "</attributes></instance_attributes>", MAX_STRLEN);
	strncat(xml, suffix, MAX_STRLEN);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_update_rsc_params:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, cib_sync_call);

	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);
}
char*
on_delete_rsc_param(char* argv[], int argc)
{
	crm_data_t * output;
	int rc;
	ARGC_CHECK(2)

	if ((rc=delete_object("resources", "nvpair", argv[1], &output)) < 0) {
		return crm_failed_msg(output, rc);
	}
	return cl_strdup(MSG_OK);
}

char*
on_set_target_role(char* argv[], int argc)
{
	int rc;
	crm_data_t* fragment = NULL;
	crm_data_t* cib_object = NULL;
	crm_data_t* output;
	char xml[MAX_STRLEN];
	char buf[MAX_STRLEN];
	char prefix[MAX_STRLEN];
	char suffix[MAX_STRLEN];
	char real_id[MAX_STRLEN];
	char inst_attrs_id[MAX_STRLEN];	
	char target_role_id[MAX_STRLEN];	
	
	if(get_fix(argv[1], prefix, suffix, real_id) == -1) {
		return cl_strdup(MSG_FAIL);
	}
	get_instance_attributes_id(argv[1], inst_attrs_id);
	get_attr_id(argv[1], "target_role", target_role_id);
	
	if (STRNCMP_CONST(argv[2],"#default") == 0) {
		snprintf(buf, MAX_STRLEN, "%s", target_role_id);
		rc = delete_object("resources", "nvpair", buf, &output);
		if (rc < 0) {
			return crm_failed_msg(output, rc);
		}
		return cl_strdup(MSG_OK);
	}
		

	snprintf(xml, MAX_STRLEN,
    		 "%s<instance_attributes id=\"%s\"><attributes>",
    		 prefix,inst_attrs_id);
	snprintf(buf, MAX_STRLEN,
		"<nvpair id=\"%s\" " \
		"name=\"target_role\" value=\"%s\"/>",
		target_role_id, argv[2]);
	strncat(xml, buf, MAX_STRLEN);
	
	strncat(xml, "</attributes></instance_attributes>", MAX_STRLEN);
	strncat(xml, suffix, MAX_STRLEN);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_set_target_role:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, cib_sync_call);

	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
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
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	ops = cl_get_struct((struct ha_msg*)rsc->xml, "operations");
	if (ops == NULL) {
		free_data_set(data_set);
		return ret;
	}
	for (i = 0; i < ops->nfields; i++) {
		if (STRNCMP_CONST(ops->names[i], "op") == 0) {
			if (ops->types[i] != FT_STRUCT) {
				continue;
			}
			op = (struct ha_msg*)ops->values[i];
			ret = mgmt_msg_append(ret, ha_msg_value(op, "id"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "name"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "interval"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "timeout"));
		}
	}
	free_data_set(data_set);
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
	char prefix[MAX_STRLEN];
	char suffix[MAX_STRLEN];
	char real_id[MAX_STRLEN];
	
	if(get_fix(argv[1], prefix, suffix,real_id) == -1) {
		return cl_strdup(MSG_FAIL);
	}
	
	snprintf(xml, MAX_STRLEN,
 		 "%s<operations>", prefix);
	for (i = 2; i < argc; i += 4) {
		snprintf(buf, MAX_STRLEN,
			"<op id=\"%s\" name=\"%s\" interval=\"%s\" timeout=\"%s\"/>",
			argv[i], argv[i+1], argv[i+2], argv[i+3]);
		strncat(xml, buf, MAX_STRLEN);
	}
	strncat(xml, "</operations>", MAX_STRLEN);
	strncat(xml, suffix, MAX_STRLEN);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_update_rsc_ops:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");

	rc = cib_conn->cmds->update(
			cib_conn, "resources", fragment, &output, cib_sync_call);

	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);
}
char*
on_delete_rsc_op(char* argv[], int argc)
{
	int rc;
	crm_data_t * output;
	ARGC_CHECK(2)

	if ((rc=delete_object("resources", "op", argv[1], &output)) < 0) {
		return crm_failed_msg(output, rc);
	}
	return cl_strdup(MSG_OK);
}
/* clone functions */
char*
on_get_clone(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	char* parameter=NULL;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	ret = mgmt_msg_append(ret, rsc->id);
	
	parameter = rsc->fns->parameter(rsc, NULL, FALSE
	,	XML_RSC_ATTR_INCARNATION_MAX, data_set);
	ret = mgmt_msg_append(ret, parameter);
	if (parameter != NULL) {
		cl_free(parameter);
	}
	
	parameter = rsc->fns->parameter(rsc, NULL, FALSE
	,	XML_RSC_ATTR_INCARNATION_NODEMAX, data_set);
	ret = mgmt_msg_append(ret, parameter);
	if (parameter != NULL) {
		cl_free(parameter);
	}

	free_data_set(data_set);
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
	char inst_attrs_id[MAX_STRLEN];	

	ARGC_CHECK(4);
	
	get_instance_attributes_id(argv[1], inst_attrs_id);
	snprintf(xml,MAX_STRLEN,
		 "<clone id=\"%s\"><instance_attributes id=\"%s\"><attributes>" \
		 "<nvpair id=\"%s_clone_max\" name=\"clone_max\" value=\"%s\"/>" \
		 "<nvpair id=\"%s_clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
		 "</attributes></instance_attributes></clone>",
		 argv[1],inst_attrs_id,argv[1],argv[2],argv[1],argv[3]);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_update_clone:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");
	rc = cib_conn->cmds->update(cib_conn, "resources", fragment, &output, cib_sync_call);
	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);
}
/* master functions */
char*
on_get_master(char* argv[], int argc)
{
	resource_t* rsc;
	char* ret;
	char* parameter=NULL;
	pe_working_set_t* data_set;
	
	data_set = get_data_set();
	GET_RESOURCE()
	
	ret = cl_strdup(MSG_OK);
	ret = mgmt_msg_append(ret, rsc->id);
	
	parameter = rsc->fns->parameter(rsc, NULL, FALSE
	,	XML_RSC_ATTR_INCARNATION_MAX, data_set);
	ret = mgmt_msg_append(ret, parameter);
	if (parameter != NULL) {
		cl_free(parameter);
	}

	parameter = rsc->fns->parameter(rsc, NULL, FALSE
	,	XML_RSC_ATTR_INCARNATION_NODEMAX, data_set);
	ret = mgmt_msg_append(ret, parameter);
	if (parameter != NULL) {
		cl_free(parameter);
	}

	parameter = rsc->fns->parameter(rsc, NULL, FALSE
	,	XML_RSC_ATTR_MASTER_MAX, data_set);
	ret = mgmt_msg_append(ret, parameter);
	if (parameter != NULL) {
		cl_free(parameter);
	}

	parameter = rsc->fns->parameter(rsc, NULL, FALSE
	,	XML_RSC_ATTR_MASTER_NODEMAX, data_set);
	ret = mgmt_msg_append(ret, parameter);
	if (parameter != NULL) {
		cl_free(parameter);
	}

	free_data_set(data_set);
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
	char inst_attrs_id[MAX_STRLEN];	

	ARGC_CHECK(6);
	get_instance_attributes_id(argv[1], inst_attrs_id);
	snprintf(xml,MAX_STRLEN,
		 "<master_slave id=\"%s\"><instance_attributes id=\"%s\"><attributes>" \
		 "<nvpair id=\"%s_clone_max\" name=\"clone_max\" value=\"%s\"/>" \
		 "<nvpair id=\"%s_clone_node_max\" name=\"clone_node_max\" value=\"%s\"/>" \
		 "<nvpair id=\"%s_master_max\" name=\"master_max\" value=\"%s\"/>" \
		 "<nvpair id=\"%s_master_node_max\" name=\"master_node_max\" value=\"%s\"/>" \
		 "</attributes></instance_attributes></master_slave>",
		 argv[1],inst_attrs_id,argv[1],argv[2],argv[1],
		 argv[3],argv[1],argv[4],argv[1],argv[5]);

	cib_object = string2xml(xml);
	if(cib_object == NULL) {
		return cl_strdup(MSG_FAIL);
	}
	mgmt_log(LOG_INFO, "on_update_master:%s",xml);
	fragment = create_cib_fragment(cib_object, "resources");
	rc = cib_conn->cmds->update(cib_conn, "resources", fragment, &output, cib_sync_call);
	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
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
	pe_working_set_t* data_set;
	const char* path[] = {"configuration","constraints"};
	
	ARGC_CHECK(2);
	
	data_set = get_data_set();
	cos = find_xml_node_nested(data_set->input, path, 2);
	if (cos == NULL) {
		free_data_set(data_set);
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
	free_data_set(data_set);
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
	pe_working_set_t* data_set;
	const char* path[] = {"configuration","constraints"};
	ARGC_CHECK(3); 
	
	data_set = get_data_set();
	cos = find_xml_node_nested(data_set->input, path, 2);
	if (cos == NULL) {
		free_data_set(data_set);
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
	free_data_set(data_set);
	return ret;
}
char*
on_delete_constraint(char* argv[], int argc)
{
	int rc;
	crm_data_t * output;
	ARGC_CHECK(3)

	if ((rc=delete_object("constraints", argv[1], argv[2], &output)) < 0) {
		return crm_failed_msg(output, rc);
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
	mgmt_log(LOG_INFO, "on_update_constraint:%s",xml);
	fragment = create_cib_fragment(cib_object, "constraints");

	rc = cib_conn->cmds->update(
			cib_conn, "constraints", fragment, &output, cib_sync_call);

	free_xml(fragment);
	free_xml(cib_object);
	if (rc < 0) {
		return crm_failed_msg(output, rc);
	}
	free_xml(output);
	return cl_strdup(MSG_OK);
}

