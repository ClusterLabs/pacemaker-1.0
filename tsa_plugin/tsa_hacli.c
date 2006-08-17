/*
 * tsa_hacli.c: command line tool
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_pidfile.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>
#include <ha_msg.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <mgmt/mgmt.h>
#include "ha_tsa_common.h"

#define PID_FILE 	HA_VARRUNDIR"/tsa_hacli.pid"
#define MAX_CMD_LEN 1024

static char*  
process_command(int argc, char * argv[])
{
	char *msg = NULL, *result = NULL;
	int i;
	char *buf = NULL;

	msg = mgmt_new_msg(argv[1], NULL);
	for(i = 2; i < argc; i++ ) {
		msg = mgmt_msg_append(msg, argv[i]);
	}

	cl_log(LOG_DEBUG, "msg sent: %s", msg);
	result = process_msg(msg);
	mgmt_del_msg(msg);
	if ( result == NULL ) {
		return NULL;
	}
	buf = cl_strdup(result);
	mgmt_del_msg(result);
	return buf;
}

int main(int argc, char *argv[])
{
	char *result = NULL;

	init_logger("tsa_cli");
	if ( argc < 2 ) {
		return 1;
	}

	if(cl_lock_pidfile(PID_FILE) < 0 ){
		exit(100);
	}

        init_mgmt_lib("tsa", ENABLE_LRM|ENABLE_CRM|ENABLE_HB|CACHE_CIB);
	result = process_command(argc, argv);
        final_mgmt_lib(); 
	if (result) {
		printf("%s\n", result);
	}
	return 0;
}

