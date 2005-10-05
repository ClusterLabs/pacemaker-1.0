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

#define MSG_OK			"ok"
#define MSG_FAIL		"fail"

#define MSG_LOGIN		"login"
#define MSG_LOGOUT		"logout"
#define MSG_ECHO		"echo"
#define MSG_TEST		"test"
#define MSG_REGEVT		"regevt"
#define MSG_ALLNODES		"all_nodes"
#define MSG_ACTIVENODES 	"active_nodes"
#define MSG_DC			"dc"
#define MSG_CRM_CONFIG		"crm_config"
#define MSG_HB_CONFIG		"hb_config"
#define MSG_NODE_CONFIG		"node_config"
#define MSG_RUNNING_RSC		"running_rsc"
#define MSG_RSC_PARAMS		"rsc_params"
#define MSG_RSC_ATTRS		"rsc_attrs"
#define MSG_RSC_CONS		"rsc_cons"
#define MSG_RSC_RUNNING_ON	"rsc_running_on"
#define MSG_RSC_LOCATION	"rsc_location"
#define MSG_RSC_OPS		"rsc_ops"

#define MSG_ALL_RSC		"all_rsc"
#define MSG_RSC_TYPE		"rsc_type"
#define MSG_SUB_RSC		"sub_rsc"
#define MSG_UP_CRM_CONFIG	"up_crm_config"

#define MSG_DEL_RSC		"del_rsc"
#define MSG_ADD_RSC		"add_rsc"
#define MSG_ADD_GRP		"add_grp"

#define MSG_RSC_CLASSES		"rsc_classes"
#define MSG_RSC_TYPES		"rsc_types"
#define MSG_RSC_PROVIDERS	"rsc_providers"

#define MSG_UP_RSC_PARAMS	"up_rsc_params"
#define MSG_UP_RSC_OPS		"up_rsc_ops"
#define MSG_UP_RSC_CONS		"up_rsc_cons"

#define EVT_STATUS		"evt:status"

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
