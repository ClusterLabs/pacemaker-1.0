
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

static char* on_get_activenodes(const char* msg, int id);
static char* on_get_dc(const char* msg, int id);
static char* on_get_crm_config(const char* msg, int id);
static char* on_get_node_config(const char* msg, int id);
static char* on_get_running_rsc(const char* msg, int id);
static char* on_get_rsc_params(const char* msg, int id);
static char* on_get_rsc_attrs(const char* msg, int id);
static char* on_get_rsc_cons(const char* msg, int id);
static char* on_get_rsc_running_on(const char* msg, int id);
static char* on_get_rsc_location(const char* msg, int id);
static char* on_get_rsc_ops(const char* msg, int id);


static resource_t* find_resource(GList* rsc_list, const char* id);
#define GET_RESOURCE()	char** args = mgmt_msg_args(msg, &num);			\
			if (num != 2) { 					\
				mgmt_del_args(args); 				\
				return cl_strdup(MSG_FAIL); 			\
			} 							\
			rsc = find_resource(data_set.resources, args[1]); 	\
			mgmt_del_args(args);					\
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
		fire_evt(MSG_STATUS);
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
	reg_msg(MSG_RSC_CONS, on_get_rsc_cons);
	reg_msg(MSG_RSC_RUNNING_ON, on_get_rsc_running_on);
	reg_msg(MSG_RSC_LOCATION, on_get_rsc_location);
	reg_msg(MSG_RSC_OPS, on_get_rsc_ops);

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
char* 
on_get_activenodes(const char* msg, int id)
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
on_get_dc(const char* msg, int id)
{
	if (data_set.dc_node != NULL) {
		char* ret = cl_strdup(MSG_OK);
		ret = mgmt_msg_append(ret, data_set.dc_node->details->uname);
		return ret;
	}
	return cl_strdup(MSG_FAIL);
}

char* 
on_get_crm_config(const char* msg, int id)
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
on_get_node_config(const char* msg, int id)
{
	int num;
	char** args = mgmt_msg_args(msg, &num);
	if (num != 2) {
		mgmt_del_args(args);
		return cl_strdup(MSG_FAIL);
	}

	node_t* node;
	GList* cur = data_set.nodes;
	while (cur != NULL) {
		node = (node_t*) cur->data;
		if (node->details->online) {
			if (strncmp(args[1],node->details->uname,MAX_STRLEN) == 0) {
				char* ret = cl_strdup(MSG_OK);
				mgmt_del_args(args);
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
	mgmt_del_args(args);
	return cl_strdup(MSG_FAIL);
}

char*
on_get_running_rsc(const char* msg, int id)
{
	int num;
	node_t* node;

	char** args = mgmt_msg_args(msg, &num);
	if (num != 2) {
		mgmt_del_args(args);
		return cl_strdup(MSG_FAIL);
	}

	GList* cur = data_set.nodes;
	while (cur != NULL) {
		node = (node_t*) cur->data;
		if (node->details->online) {
			if (strncmp(args[1],node->details->uname,MAX_STRLEN) == 0) {
				GList* cur_rsc;
				char* ret = cl_strdup(MSG_OK);
				mgmt_del_args(args); 
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
	mgmt_del_args(args);
	return cl_strdup(MSG_FAIL);
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
on_get_rsc_params(const char* msg, int id)
{
	int num, i;
	resource_t* rsc;
	char* ret;
	struct ha_msg* attrs;
	struct ha_msg* nvpair;

	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	attrs = cl_get_struct((struct ha_msg*)rsc->xml, "instance_attributes");
	attrs = cl_get_struct(attrs, "attributes");
	for (i = 0; i < attrs->nfields; i++) {
		if (strncmp(attrs->names[i], "nvpair", sizeof("nvpair")) == 0) {
			nvpair = (struct ha_msg*)attrs->values[i];
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "name"));
			ret = mgmt_msg_append(ret, ha_msg_value(nvpair, "value"));
		}
	}
	return ret;
}

char*
on_get_rsc_attrs(const char* msg, int id)
{
	int num;
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
on_get_rsc_cons(const char* msg, int id)
{
	int num;
	resource_t* rsc;
	char* ret;
	GList* cur;

	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	cur = rsc->rsc_cons;
	while (cur != NULL) {
		rsc_colocation_t* con = (rsc_colocation_t*)cur->data;
		ret = mgmt_msg_append(ret, con->id);
		ret = mgmt_msg_append(ret, con->rsc_lh->id);
		ret = mgmt_msg_append(ret, con->rsc_rh->id);
		ret = mgmt_msg_append(ret, con->state_lh);
		ret = mgmt_msg_append(ret, con->state_rh);
		switch (con->strength) {
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
		}

		cur = g_list_next(cur);
	}
	return ret;
}

char*
on_get_rsc_running_on(const char* msg, int id)
{
	int num;
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
on_get_rsc_location(const char* msg, int id)
{
	int num;
	resource_t* rsc;
	char* ret;
	GList* cur;
	GList* cur_node;
	char buf[MAX_STRLEN];

	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	cur = rsc->rsc_location;
	while (cur != NULL) {
		rsc_to_node_t* location = (rsc_to_node_t*)cur->data;
		ret = mgmt_msg_append(ret, location->id);
		ret = mgmt_msg_append(ret, location->rsc_lh->id);
		switch (location->role_filter) {
			case RSC_ROLE_UNKNOWN:
				ret = mgmt_msg_append(ret, "unknown");
				break;		
			case RSC_ROLE_STOPPED:
				ret = mgmt_msg_append(ret, "stopped");
				break;		
			case RSC_ROLE_STARTED:
				ret = mgmt_msg_append(ret, "started");
				break;		
			case RSC_ROLE_SLAVE:
				ret = mgmt_msg_append(ret, "slave");
				break;		
			case RSC_ROLE_MASTER:
				ret = mgmt_msg_append(ret, "master");
				break;		
		}
		memset(buf, 0, MAX_STRLEN);
		cur_node = location->node_list_rh;
		while(cur_node != NULL) {
			node_t* node = (node_t*)cur_node->data;
			strncat(buf, node->details->uname, MAX_STRLEN);
			strncat(buf, " ", MAX_STRLEN);
			cur_node = g_list_next(cur_node);
		}
		ret = mgmt_msg_append(ret, buf);
		cur = g_list_next(cur);
	}
	return ret;
}

char*
on_get_rsc_ops(const char* msg, int id)
{
	int num, i;
	resource_t* rsc;
	char* ret;
	struct ha_msg* ops;
	struct ha_msg* op;

	GET_RESOURCE()

	ret = cl_strdup(MSG_OK);
	ops = cl_get_struct((struct ha_msg*)rsc->xml, "operations");
	for (i = 0; i < ops->nfields; i++) {
		if (strncmp(ops->names[i], "op", sizeof("op")) == 0) {
			op = (struct ha_msg*)ops->values[i];
			ret = mgmt_msg_append(ret, ha_msg_value(op, "id"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "interval"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "name"));
			ret = mgmt_msg_append(ret, ha_msg_value(op, "timeout"));
		}
	}
	return ret;
}
