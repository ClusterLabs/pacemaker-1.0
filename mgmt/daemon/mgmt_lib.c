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
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <security/pam_appl.h>
#include <glib.h>


#include <heartbeat.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_syslog.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/cl_pidfile.h>

#include <mgmt/mgmt.h>
#include "mgmt_internal.h"



/* common daemon and debug functions */

/* the initial func for modules */
extern int init_general(void);
extern void final_general(void);
extern int init_crm(void);
extern void final_crm(void);
extern int init_heartbeat(void);
extern void final_heartbeat(void);
extern int init_lrm(void);
extern void final_lrm(void);

static GHashTable* msg_map = NULL;		
static GHashTable* event_map = NULL;		

int
init_mgmtd_lib()
{
	/* create the internal data structures */
	msg_map = g_hash_table_new_full(g_str_hash, g_str_equal, cl_free, NULL);
	event_map = g_hash_table_new_full(g_str_hash, g_str_equal, cl_free, NULL);

	/* init modules */
	init_heartbeat();
	init_lrm();
	init_crm();
	return 0;
}

int
final_mgmtd_lib()
{
	final_crm();
	final_lrm();
	final_heartbeat();
	g_hash_table_destroy(msg_map);
	g_hash_table_destroy(event_map);
	return 0;
}

int
reg_msg(const char* type, msg_handler fun)
{
	if (g_hash_table_lookup(msg_map, type) != NULL) {
		return -1;
	}
	g_hash_table_insert(msg_map, cl_strdup(type),(gpointer)fun);
	return 0;
}

int
fire_event(const char* event)
{
	event_handler func = NULL;
	
	char** args = mgmt_msg_args(event, NULL);
	if (args == NULL) {
		return -1;
	}
	
	func = g_hash_table_lookup(event_map, args[0]);
	if (func != NULL) {
		func(event);
	}
	mgmt_del_args(args);
	return 0;
}

char*
process_msg(const char* msg)
{
	msg_handler handler;
	char* ret;
	int num;
	char** args = mgmt_msg_args(msg, &num);
	if (args == NULL) {
		return NULL;
	}
	handler = (msg_handler)g_hash_table_lookup(msg_map, args[0]);
	if ( handler == NULL) {
		mgmt_del_args(args);
		return NULL;
	}
	ret = (*handler)(args, num);
	mgmt_del_args(args);
	return ret;
}
int
reg_event(const char* type, event_handler func)
{
	g_hash_table_replace(event_map, cl_strdup(type), (gpointer)func);
	return 0;
}
