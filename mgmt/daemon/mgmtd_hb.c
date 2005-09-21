
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

#include <hb_api.h>

#include "mgmtd.h"

int init_heartbeat(void);
void final_heartbeat(void);

static ll_cluster_t * hb = NULL;
static char* on_get_allnodes(const char* msg, int id);

char* 
on_get_allnodes(const char* msg, int id)
{
	const char* name = NULL;
	char* ret = cl_strdup(MSG_OK);
	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		mgmtd_log(LOG_ERR, "Cannot start node walk");
		mgmtd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		cl_free(ret);
		return cl_strdup(MSG_FAIL);
	}
	while((name = hb->llc_ops->nextnode(hb))!= NULL) {
		mgmtd_log(LOG_ERR, "name:%s",name);
		mgmt_msg_append(ret, name);
	}
	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		mgmtd_log(LOG_ERR, "Cannot end node walk");
		mgmtd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		cl_free(ret);
		return cl_strdup(MSG_FAIL);
	}
	
	return ret;
}

int
init_heartbeat(void)
{
	hb = ll_cluster_new("heartbeat");
	if (hb->llc_ops->signon(hb, mgmtd_name)!= HA_OK) {
		mgmtd_log(LOG_ERR, "Cannot sign on with heartbeat");
		mgmtd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		hb->llc_ops->delete(hb);
		hb = NULL;
		return HA_FAIL;
	}
	
	reg_msg(MSG_ALLNODES, on_get_allnodes);
	return 0;
}

void
final_heartbeat(void)
{
	hb->llc_ops->delete(hb);
	hb = NULL;
}

