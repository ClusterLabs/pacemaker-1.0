
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
#include <clplumbing/Gmain_timeout.h>

#include "mgmtd.h"

int init_general(void);
void final_general(void);

static gboolean on_timeout(gpointer data);
static char* on_echo(char* argv[], int argc, int client_id);
static char* on_reg_evt(char* argv[], int argc, int client_id);

gboolean
on_timeout(gpointer data)
{
	fire_evt(MSG_TEST);
	return TRUE;
}

char* 
on_echo(char* argv[], int argc, int client_id)
{
	char* ret = cl_strdup(MSG_OK);
	ret = mgmt_msg_append(ret, argv[1]);

	return ret;
}

char* 
on_reg_evt(char* argv[], int argc, int client_id)
{
	if (argc != 2) {
		return cl_strdup(MSG_FAIL);
	}
	reg_evt(argv[1], client_id);
	return cl_strdup(MSG_OK);
}

int
init_general(void)
{
	reg_msg(MSG_ECHO, on_echo);
	reg_msg(MSG_REGEVT, on_reg_evt);
	Gmain_timeout_add(5000,on_timeout,NULL);
	return 0;
}

void
final_general(void)
{
}
