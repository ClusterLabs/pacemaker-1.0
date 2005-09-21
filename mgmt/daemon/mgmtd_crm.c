
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


cib_t*	cib_conn = NULL;
pe_working_set_t data_set;

int init_crm(void);
void final_crm(void);

static void on_cib_diff(const char *event, HA_Message *msg);
static void on_cib_query_done(const HA_Message* msg, int call_id, int rc,
	   	  crm_data_t* output, void* user_data);
static char* on_get_activenodes(const char* msg, int id);

void
on_cib_query_done(const HA_Message* msg, int call_id, int rc,
	   	  crm_data_t* output, void* user_data)
{
	if(rc == cib_ok) {
		/*FIXME: where do we release cib?*/
		crm_data_t *cib = NULL;
		cib = find_xml_node(output,XML_TAG_CIB,TRUE);

		set_working_set_defaults(&data_set);
		data_set.input = cib;
		stage0(&data_set);

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
	set_working_set_defaults(&data_set);

	reg_msg(MSG_ACTIVENODES, on_get_activenodes);

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
			mgmt_msg_append(ret, node->details->uname);
		}
		cur = g_list_next(cur);
	}
	return ret;
}
