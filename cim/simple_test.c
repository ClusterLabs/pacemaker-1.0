/*
 * Tests 
 * 
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include "cluster_info.h"
#include <ha_msg.h>
#include <hb_api.h>
#include <unistd.h>
/* #include <clplumbing/tracer.h> */
#include "utils.h"
#include "cluster_info.h"
#include "mgmt_client.h"

#if 0
static int testfunc1(void)
{
	unsigned int i;
	i = call0(unsigned int, sleep, 1);
	return 0;
}

static void testfunc2(int i)
{
	sleep(i);
}

static int testfunc3(int k)
{
	sleep(k);
}

#endif

int main(int argc, char * argv[])
{

#if 0
	CIMTable *   t;
	CIMArray *	a;
	char test[1024]="hello\n";
	int i;
	
	t = cim_table_new();
	a = cim_table_lookup_v(t, "nosuchthing").v.array;
	split_string(test, &i, "\n");
	printf("len = %d\n", i);
	time_trace_init(NULL, dump_current_cb);
	void_call_full("printf", printf("%d", call_full("testfunc1", int, testfunc1())));
        void_call(testfunc2(2));
        call(int, testfunc3(1));
	time_trace_destroy();
#endif

	struct ha_msg *msg;
	cim_init_logger("test");
	msg = ha_msg_new(1);
	cl_msg_list_add_string(msg, "key", "hello");
	cl_msg_remove(msg, "key");
	cl_msg_list_add_string(msg, "key", "hello2");
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));
#if 0
	ha_msg_del(msg);	
	msg = cim_get_msg(GET_CRM_CONFIG, NULL, NULL);
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));
	msg = cim_get_msg(GET_DC, NULL, NULL);
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));
	msg = cim_get_msg(GET_HB_CONFIG, NULL, NULL);
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));
	msg = cim_get_msg(GET_RSC_LIST, NULL, NULL);
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));
	msg = cim_get_msg(GET_RSC_ATTRIBUTES, "DcIPaddr", NULL);
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));

	msg = cim_get_msg(GET_RSC_OPERATIONS, "DcIPaddr", NULL);
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));

	msg = string2msg(msg2string(msg), strlen(msg2string(msg)));
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));

	msg = cl_get_struct(msg, "op.0");
	cl_log(LOG_INFO, "msg: %s", msg2string(msg));
#endif
        return 0;
}
