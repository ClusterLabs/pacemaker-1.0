/*
 * Client-side Linux HA management library
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

#include <portability.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mgmt/mgmt_tls.h>
#include <mgmt/mgmt_client.h>

#define ISCONNECTED()	(session)


int 		sock = 0;
void*		session = NULL;
/*
 *	return value
 *	-1:can't connect to server
 *	-2:auth failed
 *	0 :success
 */
int
mgmt_connect(const char* server, const char* user, const char*  passwd)
{
	struct sockaddr_in addr;
	char* msg;
	char* ret;
	
	/* if it has already connected, return fail */
	if (ISCONNECTED()) {
		return -1;
	}

	/* create socket */
	sock=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1 ) {
		return -1;
	}

	/* connect to server*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(server);
	addr.sin_port = htons(PORT);
	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		close(sock);
		return -1;
	}

	/* initialize GnuTls lib*/
	if (tls_init_client() == -1) {
		return -1;
	}

	/* bind the socket to GnuTls lib */
	session = tls_attach_client(sock);
	if (session == NULL) {
		close(sock);
		tls_close_client();
		return -1;
	}

	/* login to server */
	msg = mgmt_new_msg(MSG_LOGIN, user, passwd, NULL);
	ret = mgmt_sendmsg(msg);
	if (ret == NULL || STRNCMP_CONST(ret,MSG_OK) != 0) {
		mgmt_del_msg(msg);
		mgmt_del_msg(ret);
		close(sock);
		tls_close_client();
		return -2;
	}
	
	mgmt_del_msg(msg);
	mgmt_del_msg(ret);
	return 0;
}

char* 
mgmt_sendmsg(const char* msg)
{
	/* send the msg */
	if (mgmt_session_sendmsg(session, msg) < 0) {
		return NULL;
	}
	/* get the result msg */
	return mgmt_session_recvmsg(session);
}

char*
mgmt_recvmsg(void)
{
	return mgmt_session_recvmsg(session);
}

int
mgmt_inputfd(void)
{
	if (!ISCONNECTED()) {
		return -1;
	}
	return sock;
}


int
mgmt_disconnect(void)
{
	if (!ISCONNECTED()) {
		return -1;
	}
	
	if (session != NULL) {
		mgmt_session_sendmsg(session, MSG_LOGOUT);
		tls_detach(session);
		session = NULL;
	}
	if (sock != 0) {
		close(sock);
		sock = 0;
	}
	tls_close_client();
	return 0;
}

int 
mgmt_session_sendmsg(void* session, const char* msg)
{
	int len;
	if (session == NULL) {
		return -1;
	}
	/* send the msg, with the last zero */
	len = strnlen(msg, MAX_MSGLEN)+1;
	if (len == MAX_MSGLEN + 1) {
		return -2;
	}
	if (len != tls_send(session, msg, len)) {
		return -1;
	}
	/* get the bytes sent */
	return len;
}

char*
mgmt_session_recvmsg(void* session)
{
	char c;
	int cur = 0;
	int len = 0;
	char* buf = NULL;
	if (session == NULL) {
		return NULL;
	}

	while(1) {
		int rd = tls_recv(session, &c, 1);
		if (rd <= 0 && buf == NULL) {
			/* no msg or something wrong */
			return NULL;
		}
		if (rd == 1) {
			/* read one char */
			if (buf == NULL) {
				/* malloc buf */
				buf = (char*)mgmt_malloc(INIT_SIZE);
				len = INIT_SIZE;
			}
			if (buf == NULL) {
				return NULL;
			}
			/* the buf is full, enlarge it */
			if (cur == len) {
				buf = mgmt_realloc(buf, len+INC_SIZE);
				if (buf == NULL) {
					return NULL;
				}
				len += INC_SIZE;
			}
			
			buf[cur] = c;
			cur++;
			if (c == 0) {
				return buf;
			}
		}
		/* something wrong */
		if (rd <= 0) {
			if(errno == EINTR) {
				continue;
			}
			mgmt_free(buf);
			return NULL;
		}
	}
}

