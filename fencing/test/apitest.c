/* File: apitest.c
 * Description: A program for testing stonithd client APIs.
 *
 * Author: Sun Jiang Dong <sunjd@cn.ibm.com>
 * Copyright (c) 2004 International Business Machines
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>
#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */
#include <errno.h>
#include <glib.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/uids.h>
#include <fencing/stonithd_api.h>

static void
stonith_ops_cb(stonith_ops_t * op, void * private_data)
{
	printf("optype=%d, node_name=%s, result=%d\n",op->optype, op->node_name,
		op->op_result);
}

int main(void)
{	
	cl_log_set_entity("STONITHD_API_TEST");
	cl_log_set_facility(LOG_USER);
	cl_log_enable_stderr(TRUE);
	stonithd_signon("test");
	//sleep(1);

	stonith_ops_t * st_op;

	st_op = g_new(stonith_ops_t, 1);
	st_op->optype = 1;
	st_op->node_name = g_strdup("hadev1");
	st_op->timeout = 10000;
	
	stonithd_set_stonith_ops_callback(stonith_ops_cb, NULL);
	if (ST_OK == stonithd_node_fence( st_op )) {
		while (stonithd_op_result_ready() != TRUE) {
			;
		}

		cl_log(LOG_DEBUG, "Will call stonithd_receive_ops_result.");
		stonithd_receive_ops_result(TRUE);
	}

	/*
	stonithRA_ops_t * stra_op;
	int call_id;
	stra_op = g_new(stonithRA_ops_t, 1);
	stra_op->ra_name = g_strdup("/root/test");
	stra_op->op_type = g_strdup("start");
	stra_op->params = NULL;

	stonithd_virtual_stonithRA_ops( stra_op, &call_id );
	*/
	stonithd_signoff();

	return 0;
}
