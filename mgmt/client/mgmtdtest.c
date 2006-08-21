/*
 * Test client for Linux HA management daemon
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
#include <stdio.h>
#include <string.h>
#include <mgmt/mgmt_client.h>

int main (int argc, char* argv[])
{
	char* ret;
	int num;
	char** args;
	if(mgmt_connect("127.0.0.1", "hacluster","hacluster") != 0) {
		printf("can't conenct to mgmtd\n");
		return 1;
	}
	ret = mgmt_sendmsg(MSG_ECHO"\nhello");
	if (ret == NULL) {
		printf("can't process message\n");
		return 1;
	}
	args = mgmt_msg_args(ret, &num);
	if (ret == NULL) {
		printf("can't parse the return message\n");
		return 1;
	}
	if (num != 2) {
		printf("the return message has wrong field number\n");
		return 1;
	}
	if (strncmp(args[0],"ok",strlen("ok")) != 0) {
		printf("the return message is not \"ok\"\n");
		return 1;
	}
	if (strncmp(args[1],"hello",strlen("hello")) != 0) {
		printf("the echo string is not same as we sent\"ok\"\n");
		return 1;
	}
	mgmt_del_args(args);
	mgmt_del_msg(ret);
	mgmt_disconnect();
	return 0;
}
