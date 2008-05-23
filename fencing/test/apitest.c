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

#include <crm_internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */
#include <glib.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/uids.h>
#include <fencing/stonithd_api.h>

static int g_rc = 0;

static void
stonith_ops_cb(stonith_ops_t * op)
{
	printf("optype=%d, node_name=%s, result=%d, node_list=%s\n",op->optype,
		op->node_name, op->op_result, (char *)op->node_list);
	if (atoi(op->private_data) != (int)op->op_result) {
		g_rc = -1;
	}
}

int main(int argc, char * argv[])
{	
	stonith_ops_t * st_op;

	cl_log_set_entity("STDAPI_TEST");
	cl_log_set_facility(LOG_USER);
	cl_log_enable_stderr(TRUE);
	if (argc != 5) {
		cl_log(LOG_ERR, "parameter error.");
		printf("%s optype target_node timeout expect_value.\n", argv[0]);
		return -1;
	}

	if (ST_OK != stonithd_signon("apitest")) {
		return -1;
	}

	st_op = g_new(stonith_ops_t, 1);
	st_op->optype = atoi(argv[1]);
	st_op->node_name = g_strdup(argv[2]);
	st_op->node_uuid = g_strdup(argv[2]);
	st_op->timeout = atoi(argv[3]);
	st_op->private_data = g_strdup(argv[4]);
	
	if (ST_OK!=stonithd_set_stonith_ops_callback(stonith_ops_cb)) {
		stonithd_signoff();
		return -1;
	}
	if (ST_OK == stonithd_node_fence( st_op )) {
		while (stonithd_op_result_ready() != TRUE) {
			;
		}

		if (ST_OK!=stonithd_receive_ops_result(TRUE)) {
			return -1;
		}
	} else {
		g_rc = -1;
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
	if (ST_OK!=stonithd_signoff()) {
		g_rc = -1;
	}

	return g_rc;
}
