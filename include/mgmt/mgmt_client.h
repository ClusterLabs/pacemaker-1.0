/*
 * Client-side Linux HA manager API.
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (C) 2005 International Business Machines
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __MGMT_CLIENT_H
#define __MGMT_CLIENT_H 1
#include <mgmt/mgmt_common.h>
/*************************USER GUIDE*****************************************/

/*
* daemon = management daemon
1. using mgmt_connect() to connect to the server running the daemon
   after connect, we should send MSG_LOGIN to daemon first.
2. using mgmt_sendmsg() to send message to daemon, the result will
   be returned by the same function
3. if we register some event by MSG_REGEVT message, we need watch the fd
   returned by mgmt_inputfd(), when event comes, call mgmt_recvmsg() to get
   the event message.
4. call mgmt_disconnect() to disconnect from daemon
5. the format of the message: an ASCII string using "\n" as separator
6. the message sent to daemon:
   MSG_XXXXX\nPARAMETER1\nPARAMETER2\n....\nPARAMETERN
7. the result message after you send a message to daemon:
   ok\nPARAMETER1\nPARAMETER2\n....\nPARAMETERN or
   fail\nPARAMETER1\nPARAMETER2\n....\nPARAMETERN
8. the event message from daemon:
   EVT_XXXXX\nPARAMETER1\nPARAMETER2\n....\nPARAMETERN
9. mgmt_new_msg(),mgmt_msg_append(),mgmt_msg_args(),mgmt_del_msg() and
   mgmt_del_args() are used for manipulating the message string
*/


/*
mgmt_connect:
	connect to server running of daemon. We need send MSG_LOGIN
	message for login after we connected.
parameters:
	server: ip address of server, like "192.168.30.12"
	user and passwd: the authorized user/password on the server.
return:
	0: success
	-1: fail
*/
extern int 	mgmt_connect(const char* server, const char* user
		, const char*  passwd);

/*
mgmt_sendmsg:
	send message to daemon.
parameters:
	msg: see the comment above
return: the result from daemon for success, see the comment above
	NULL for fail
	
*/
extern char* 	mgmt_sendmsg(const char* msg);

/*
mgmt_recvmsg:
	receive message from daemon when client get an event,
	see the  comment above
parameters:
return:
	the message recieved
*/
extern char* 	mgmt_recvmsg(void);

/*
mgmt_inputfd:
	return the socket for select, pull, or g_main_loop
parameters:
return:
	>0: success
	-1: fail
*/
extern int 	mgmt_inputfd(void);

/*
mgmt_disconnect:
	disconnect from server
parameters:
return:
	0: success
	-1: fail
*/
extern int 	mgmt_disconnect(void);

/*FIXME: We haven't apply the port 5560 yet*/
#define PORT		5560
extern int mgmt_session_sendmsg(void* session, const char* msg);
extern char* mgmt_session_recvmsg(void* session);

#endif /* __MGMT_CLIENT_H */
