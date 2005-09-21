/*
 * Client-side Linux HA Manager API.
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

#ifndef __HAM_H
#define __HAM_H 1

#define	STRLEN_CONST(conststr)  ((size_t)((sizeof(conststr)/sizeof(char))-1))
#define	STRNCMP_CONST(varstr, conststr) strncmp((varstr), conststr, STRLEN_CONST(conststr)+1)

#define PORT		5678

#define	MAX_MSGLEN	4096
#define	MAX_STRLEN	1024

#define MSG_LOGIN	"login"
#define MSG_LOGOUT	"logout"
#define MSG_ECHO	"echo"
#define MSG_TEST	"test"
#define MSG_REGEVT	"regevt"
#define MSG_STATUS	"status"
#define MSG_ACTIVENODES "activenodes"
#define MSG_OK		"ok"
#define MSG_FAIL	"fail"
#define MSG_ALLNODES	"allnodes"

extern int mgmt_connect(const char* server, const char* user, const char*  passwd);
extern char* mgmt_sendmsg(const char* msg);
extern char* mgmt_recvmsg(void);
extern int mgmt_inputfd(void);
extern int mgmt_disconnect(void);

typedef void* (*malloc_t)(size_t size);
typedef void* (*realloc_t)(void* oldval, size_t newsize);
typedef void (*free_t)(void *ptr);

extern void	mgmt_set_mem_funcs(malloc_t m, realloc_t r, free_t f);
extern char*	mgmt_new_msg(const char* type, ...);
extern char*	mgmt_msg_append(char* msg, const char* append);
extern char**	mgmt_msg_args(const char* msg, int* num);
extern void	mgmt_del_msg(char* msg);
extern void	mgmt_del_args(char** args);
extern int 	mgmt_session_sendmsg(void* s, const char* msg);
extern char* 	mgmt_session_recvmsg(void* s);

#endif /* __HAM_H */
