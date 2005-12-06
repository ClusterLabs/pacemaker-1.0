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
#include <mgmt/mgmt_client.h>

int main (int argc, char* argv[])
{
	char* ret;
	mgmt_connect("127.0.0.1", "hacluster","hacluster");
	ret = mgmt_sendmsg(MSG_ECHO"\nhello");
	printf("%s\n",ret);
	mgmt_del_msg(ret);
	mgmt_disconnect();
	return 0;
}
