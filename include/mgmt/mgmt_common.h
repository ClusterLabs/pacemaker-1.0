/*
 *  Common library for Linux-HA management tool
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

#ifndef __MGMT_COMMON_H
#define __MGMT_COMMON_H 1


/*************************MESSAGES*******************************************/

#define MSG_OK  		"ok"
#define MSG_FAIL		"fail"

/*
description:
	login to daemon.
	the username/password must be one of the authorized account on the 
	server running the daemon
format:
	MSG_LOGIN username password
return:
	MSG_OK 
or
	MSG_FAIL
*/
#define MSG_LOGIN		"login"

/*
description:
	logout from daemon
format:
	MSG_LOGOUT
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_LOGOUT		"logout"

/*
description:
	do nothing except return the string ANYSTRING.
	using for test the sanity of daemon.
format:
	MSG_ECHO anystring
return:
	MSG_OK anystring
or
	MSG_FAIL
*/
#define MSG_ECHO		"echo"

/*
description:
	register event EVENTTYPE.
	when the event invoked, the client would be notified.
format:
	MSG_REGEVT EVENTTYPE
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_REGEVT		"regevt"

/*
description:
	return CIB version
format:
	MSG_CIB_VERSION
return:
	MSG_OK version
or
	MSG_FAIL
*/
#define MSG_CIB_VERSION		"cib_version"

/*
description:
	return CRM configuration
format:
	MSG_CRM_CONFIG
return:
	MSG_OK transition_idle_timeout symmetric_cluster(True|False)
 	  stonith_enabled(True|False) no_quorum_policy(freeze|stop|ignore)
 	  default_resource_stickiness have_quorum(True|False) 
 	  default_resource_failure_stickiness
or
	MSG_FAIL
*/
#define MSG_CRM_CONFIG		"crm_config"
#define F_MGMT_TRANSITION_IDEL_TIMEOUT		1
#define F_MGMT_SYMMETRIC_CLUSTER		2
#define F_MGMT_STONITH_ENABLED			3
#define F_MGMT_NO_QUORUM_POLICY			4
#define F_MGMT_DEFAULT_RESOURCE_STICKINESS	5
#define F_MGMT_HAVE_QUORUM			6
#define F_MGMT_RESOURCE_FAILURE_STICKINESS	7


/*
description:
	update CRM's configuration
format:
	MSG_UP_CRM_CONFIG config_id config_key config_value
return:
	MSG_OK 
or
	MSG_FAIL
*/
#define MSG_UP_CRM_CONFIG	"up_crm_config"

/*
description:
	return heartbeat configuration
format:
	MSG_HB_CONFIG
return:
	MSG_OK apiauth auto_failback baud debug debugfile deadping deadtime
	  hbversion hopfudge initdead keepalive logfacility logfile msgfmt
	  nice_failback node normalpoll stonith udpport warntime watchdog
or
	MSG_FAIL
*/
#define MSG_HB_CONFIG		"hb_config"
#define F_MGMT_APIAUTH			1
#define F_MGMT_AUTO_FAILBACK		2
#define F_MGMT_BAUD			3
#define F_MGMT_DEBUG			4
#define F_MGMT_DEBUGFILE		5
#define F_MGMT_DEADPING			6
#define F_MGMT_DEADTIME			7
#define F_MGMT_HBVERSION		8
#define F_MGMT_HOPFUDGE			9
#define F_MGMT_INITDEAD			10
#define F_MGMT_KEEPALIVE		11
#define F_MGMT_LOGFACILITY		12
#define F_MGMT_LOGFILE			13
#define F_MGMT_MSGFMT			14
#define F_MGMT_NICE_FAILBACK		15
#define F_MGMT_NODE			16
#define F_MGMT_NORMALPOLL		17
#define F_MGMT_STONITH			18
#define F_MGMT_UDPPORT			19
#define F_MGMT_WARNTIME			20
#define F_MGMT_WATCHDOG			21
#define F_MGMT_CLUSTER			22

/*
description:
	return the name of all nodes configured in cluster
format:
	MSG_ALLNODES
return:
	MSG_OK node1 node2 ... noden
or
	MSG_FAIL
*/
#define MSG_ALLNODES		"all_nodes"

/*
description:
	return active nodes configured in cluster
format:
	MSG_ACTIVENODES
return:
	MSG_OK node1 node2 ... noden
or
	MSG_FAIL
*/
#define MSG_ACTIVENODES 	"active_nodes"

/*
description:
	return nodes configured in crm
format:
	MSG_CRMNODES
return:
	MSG_OK node1 node2 ... noden
or
	MSG_FAIL
*/
#define MSG_CRMNODES 	"crm_nodes"

/*
description:
	return DC in cluster
format:
	MSG_DC
return:
	MSG_OK dc_node
or
	MSG_FAIL
*/
#define MSG_DC			"dc"

/*
description:
	return node's configured
format:
	MSG_NODE_CONFIG NODENAME
return:
	MSG_OK uname online(True|False) standby(True|False) unclean(True|False)
 	  shutdown(True|False) expected_up(True|False) is_dc(True|False)
	  node_ping("ping|member")
or
	MSG_FAIL
*/
#define MSG_NODE_CONFIG		"node_config"
#define F_MGMT_UNAME				1
#define F_MGMT_ONLINE				2
#define F_MGMT_STANDBY				3
#define F_MGMT_UNCLEAN				4
#define F_MGMT_SHUTDOWN			5
#define F_MGMT_EXPECTED_UP			6
#define F_MGMT_IS_DC				7
#define F_MGMT_NODE_PING			8

/*
description:
	set standby on a node
format:
	MSG_STANDBY node on|off
return:
	MSG_OK 
or
	MSG_FAIL reason
*/
#define MSG_STANDBY		"standby"


/*
description:
	return names of all running resources on a given node
format:
	MSG_RUNNING_RSC node
return:
	MSG_OK resource1 resource2 ...  resourcen
or
	MSG_FAIL
*/
#define MSG_RUNNING_RSC		"running_rsc"

/*
description:
	return all resources in the cluster
format:
	MSG_ALL_RSC
return:
	MSG_OK resource1 resource2 ...  resourcen
or
	MSG_FAIL
*/
#define MSG_ALL_RSC		"all_rsc"

/*
description:
	return the attributes of a given resource
format:
	MSG_RSC_ATTRS resource
return:
	MSG_OK id class provider type
or
	MSG_FAIL
*/
#define MSG_RSC_ATTRS		"rsc_attrs"

/*
description:
	return the type of a given resource
format:
	MSG_RSC_TYPE resource
return:
	MSG_OK type(unknown|native|group|clone|master)
or
	MSG_FAIL
*/
#define MSG_RSC_TYPE		"rsc_type"

/*
description:
	return the sub-resources of a given resource
format:
	MSG_SUB_RSC resource
return:
	MSG_OK sub-resource1 sub-resource2 ... sub-resourcen
or
	MSG_FAIL
*/
#define MSG_SUB_RSC		"sub_rsc"

/*
description:
	return the node on which the given resource is running on
format:
	MSG_RSC_RUNNING_ON resource
return:
	MSG_OK node
or
	MSG_FAIL
*/
#define MSG_RSC_RUNNING_ON	"rsc_running_on"

/*
description:
	return the status of a given resource
format:
	MSG_RSC_STATUS resource
return:
	MSG_OK status(unknown|unmanaged|failed|multi-running|running|group
			|clone|master)
or
	MSG_FAIL
*/
#define MSG_RSC_STATUS		"rsc_status"

/*
description:
	add a new resource
format:
	MSG_ADD_RSC rsc_id rsc_class rsc_type rsc_provider group("" for NONE)
		advance(""|"clone"|"master") advance_id clone_max
		clone_node_max master_max master_node_max
		param_id1 param_name1 param_value1
		param_id2 param_name2 param_value2
		...
		param_idn param_namen param_valuen
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_ADD_RSC		"add_rsc"
#define F_MGMT_RSC_ID			1
#define F_MGMT_RSC_CLASS		2
#define F_MGMT_RSC_TYPE			3
#define F_MGMT_RSC_PROVIDER		4
#define F_MGMT_GROUP			5
#define F_MGMT_ADVANCE			6
#define F_MGMT_ADVANCE_ID		7
#define F_MGMT_CLONE_MAX		8
#define F_MGMT_CLONE_NODE_MAX		9
#define F_MGMT_MASTER_MAX		10
#define F_MGMT_MASTER_NODE_MAX		11
#define F_MGMT_PARAM_ID1		12
#define F_MGMT_PARAM_NAME1		13
#define F_MGMT_PARAM_VALUE1		14

/*
description:
	add a new group
format:
	MSG_ADD_GRP group
		param_id1 param_name1 param_value1
		param_id2 param_name2 param_value2
		...
		param_idn param_namen param_valuen
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_ADD_GRP		"add_grp"

/*
description:
	delete a resource
	notice that the resoruce can be native, group, clone or master
format:
	MSG_DEL_RSC resource
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_DEL_RSC		"del_rsc"

/*
description:
	clean up a unmanaged resource
format:
	MSG_CLEANUP_RSC resource
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_CLEANUP_RSC		"cleanup_rsc"

/*
description:
	move a resource in group
format:
	MSG_MOVE_RSC resource up|down
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_MOVE_RSC		"move_rsc"

/*
description:
	update attribute of a given resource
format:
	MSG_UP_RSC_ATTR resource name value
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_UP_RSC_ATTR		"up_rsc_attr"

/*
description:
	return the params of a given resource
format:
	MSG_RSC_PARAMS resource
return:
	MSG_OK id1 name1 value1 id2 name2 value2 ... idn namen valuen
or
	MSG_FAIL
*/

#define MSG_RSC_PARAMS		"rsc_params"

/*
description:
	update params of a given resource
format:
	MSG_UP_RSC_PARAMS resource id1 name1 value1 id2 name2 value2 
		... idn namen valuen
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_UP_RSC_PARAMS	"up_rsc_params"

/*
description:
	delete the given param 
format:
	MSG_DEL_RSC_PARAM param_id
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_DEL_RSC_PARAM	"del_rsc_param"

/*
description:
	set the target_role of resource
format:
	MSG_SET_TARGET_ROLE resource "started"|"stopped"|"default"
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_SET_TARGET_ROLE	"set_target_role"

/*
description:
	return the operations of a given resource
format:
	MSG_RSC_OPS resource
return:
	MSG_OK id1 name1 interval1 timeout1 id2 name2 interval2 timeout2
		... idn namen intervaln timeoutn
or
	MSG_FAIL
*/
#define MSG_RSC_OPS		"rsc_ops"

/*
description:
	update operations of a given resource
format:
	MSG_UP_RSC_OPS resource id1 name1 interval1 timeout1
		id2 name2 interval2 timeout2 ...
		idn namen intervaln timeoutn
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_UP_RSC_OPS		"up_rsc_ops"

/*
description:
	delete the given operation 
format:
	MSG_DEL_RSC_OP operation
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_DEL_RSC_OP		"del_rsc_op"

/*
description:
	update the clone
format:
	MSG_UPDATE_CLONE clone_id clone_max clone_node_max
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_UPDATE_CLONE	"up_clone"

/*
description:
	get the clone
format:
	MSG_GET_CLONE clone_id 
return:
	MSG_OK clone_id clone_max clone_node_max
or
	MSG_FAIL
*/
#define MSG_GET_CLONE		"get_clone"

/*
description:
	update the master_slave
format:
	MSG_UPDATE_MASTER master_id clone_max clone_node_max
		master_max master_max_node
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_UPDATE_MASTER	"up_master"

/*
description:
	get the information of a given master_slave
format:
	MSG_GET_MASTER master_id
return:
	MSG_OK master_id  clone_max clone_node_max
		master_max master_max_node
or
	MSG_FAIL
*/
#define MSG_GET_MASTER		"get_master"

/*
description:
	get the name list of a given type of constraints
format:
	MSG_GET_CONSTRAINTS type(rsc_location|rsc_colocation|rsc_order)
return:
	MSG_OK constraint_id1 constraint_id2 ... constraint_idn
or
	MSG_FAIL
*/
#define MSG_GET_CONSTRAINTS	"get_cos"

/*
description:
	get the parameters of a given constraint
format:
	MSG_GET_CONSTRAINT (rsc_location|rsc_colocation|rsc_order) id
return:
	rsc_location:
		MSG_OK id resource score expr_id1 attribute1 operation1 value1
			expr_id2 attribute2 operation2 value2 ...
			expr_idn attributen operationn valuen
	rsc_order:
		MSG_OK id from_rsc type to_rsc
	rsc_colocation:
		MSG_OK id from_rsc to_rsc score
or
	MSG_FAIL
*/
#define MSG_GET_CONSTRAINT	"get_co"

/*
description:
	delete a given constraint
format:
	MSG_DEL_CONSTRAINT (rsc_location|rsc_colocation|rsc_order) id
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_DEL_CONSTRAINT	"del_co"

/*
description:
	update a constraint. if the constraint doesn't exist, create one
format:
	rsc_location:
		MSG_UP_CONSTRAINT "rsc_location" id resource score expr_id1 
			attribute1 operation1 value1 expr_id2 attribute2 
			operation2 value2 ... expr_idn attributen operationn
 			valuen
	rsc_order:
		MSG_UP_CONSTRAINT "rsc_order" id from_rsc type to_rsc
	rsc_colocation:
		MSG_UP_CONSTRAINT "rsc_colocation" id from_rsc to_rsc score
return:
	MSG_OK
or
	MSG_FAIL
*/
#define MSG_UP_CONSTRAINT	"up_co"

/*
description:
	return all resource classes of resource agents
format:
	MSG_RSC_CLASSES
return:
	MSG_OK class1 class2 ... classn
or
	MSG_FAIL
*/
#define MSG_RSC_CLASSES		"rsc_classes"

/*
description:
	return all resource type of a given class
format:
	MSG_RSC_TYPE class
return:
	MSG_OK type1 type2 ... typen
or
	MSG_FAIL
*/
#define MSG_RSC_TYPES		"rsc_types"

/*
description:
	return all provider of a given class and type
format:
	MSG_RSC_TYPE class type
return:
	MSG_OK provider1 provider2 ... providern
or
	MSG_FAIL
*/
#define MSG_RSC_PROVIDERS	"rsc_providers"

/*
description:
	return the metadata of a given resource type
format:
	MSG_RSC_METADATA RSC class type provider
return:
	MSG_OK LINE1 LINE2 ... LINEn
or
	MSG_FAIL
*/
#define MSG_RSC_METADATA	"rsc_metadata"


/*************************EVENTS*********************************************/

/*
description:
	when the cib changed, client which registered this event will be 
	notified with this event message
format:
	EVT_CIB_CHANGED
*/
#define EVT_CIB_CHANGED		"evt:cib_changed"

/*
description:
	when the management daemon losts connection with heartbeat, client 
	which registered this event will be notified with this event message
format:
	EVT_DISCONNECTED
*/
#define EVT_DISCONNECTED	"evt:disconnected"

#define EVT_TEST		"evt:test"

/*************************FUNTIONS*******************************************/
/*
mgmt_set_mem_funcs:
	set user own memory functions, like cl_malloc/cl_realloc/cl_free
 	for linux-ha 2
parameters:
	the three memory functions
return:
	none
*/
typedef void* 	(*malloc_t)(size_t size);
typedef void* 	(*realloc_t)(void* oldval, size_t newsize);
typedef void 	(*free_t)(void *ptr);
extern void	mgmt_set_mem_funcs(malloc_t m, realloc_t r, free_t f);
extern void* 	mgmt_malloc(size_t size);
extern void* 	mgmt_realloc(void* oldval, size_t newsize);
extern void 	mgmt_free(void *ptr);

/*
mgmt_new_msg:
	create a new message
parameters:
	type: should be the micro of MSG_XXX listed above
	... : the parameters listed above
return:
	a string as result, the format is described above
*/
extern char*	mgmt_new_msg(const char* type, ...);

/*
mgmt_msg_append:
	append a new parameter to an existing message
	the memory of the msg will be realloced.
parameters:
	msg: the original message
	append: the parameter to be appended
return:
	the new message
example:
	msg = mgmt_msg_append(msg, "new_param");
*/
extern char*	mgmt_msg_append(char* msg, const char* append);

/*
mgmt_del_msg:
	free a message
parameters:
	msg: the message to be free
return:
*/
extern void	mgmt_del_msg(char* msg);

/*
mgmt_result_ok:
	return whether the result is ok
parameters:
	msg: the message for determining
return:
	1: the result message is ok
	0: the result message is fail
*/
extern int	mgmt_result_ok(char* msg);

/*
mgmt_msg_args:
	parse the message to string arry
parameters:
	msg: the message to be parsed
	num: return the number of parameters
		(include type of message if the message has one)
return:
	the string arry, we should use mgmt_del_args() to free it
example:
	int i,num;
	char**	args = mgmt_msg_args(msg, &num);
	for(i=0; i<num; i++) {
		printf("%s\n",args[i]);
	}
	mgmt_del_args(args);
*/
extern char**	mgmt_msg_args(const char* msg, int* num);
extern void	mgmt_del_args(char** args);

#define	STRLEN_CONST(conststr)  ((size_t)((sizeof(conststr)/sizeof(char))-1))
#define	STRNCMP_CONST(varstr, conststr) strncmp((varstr), conststr, STRLEN_CONST(conststr)+1)

#define	MAX_MSGLEN	(256*1024)
#define	MAX_STRLEN	(64*1024)

#define INIT_SIZE	1024
#define INC_SIZE	512

#endif /* __MGMT_COMMON_H */
