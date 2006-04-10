/*
 * cluster_info.h: Cluster information agent
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

#ifndef _CLUSTER_INFO_H
#define _CLUSTER_INFO_H

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <hb_api.h>
#include <saf/ais.h>
#include <ha_msg.h>
#include "utils.h"
#include "mgmt_client.h"

#define TID_UNKNOWN          0x0
#define TID_RES_PRIMITIVE    0x51
#define TID_RES_GROUP        0x52
#define TID_RES_CLONE        0x53
#define TID_RES_MASTER       0x54
#define TID_CONS_ORDER       0x61
#define TID_CONS_LOCATION    0x62
#define TID_CONS_COLOCATION  0x63
#define TID_ARRAY            0x100

#define	CIM_MSG_LIST		"__list__"
#define	CIM_MSG_TAG		"__tag__"
#define CIM_MSG_ATTR_ID		"id"	/* every node-msg should have this */

enum { HB_RUNNING = 1, HB_STOPED = 2, HB_UNKNOWN = 3 };
enum { START_HB   = 1, STOP_HB   = 2, RESTART_HB = 3 };

enum QUERY_CTXIDS {
	/* cluster */
	GET_CRM_CONFIG = 0	,	/* get crm config values */
	GET_HB_CONFIG		,	/* get heartbeat config values */
	GET_DC			,	/* get cluster DC */

	/* node */
	GET_NODE_INFO		,	/* get node status, etc. */
	GET_NODE_LIST		, 	/* get all node names */
					/* 4 */
	/* resource */			
	GET_RSC_CLASSES		,	/* get resource's classes */
	GET_RSC_PROVIDERS	,	/* get resource's providers */
	GET_RSC_TYPES		,	/* get resource's types */
	GET_RSC_LIST		,	/* get all resource names in CIB*/
	GET_RSC_TYPE		,	/* get a resource's type */
	GET_RSC_STATUS		,	/* get a resource's status */
	GET_RSC_HOST		,	/* get the node name which a resource
						is running on*/
	GET_SUB_RSC		,	/* get the sub resources of a resource*/
	GET_PRIMITIVE		,	/* get a primitive resource */
	GET_CLONE		,	/* get a clone resource */
	GET_MASTER		,	/* get a master-slave resource */
					/* 15 */
	/* resource attribute */
	GET_RSC_OPERATIONS	,	/* get operations of a resource */
	GET_RSC_ATTRIBUTES	,	/* get attributes of a resource */
					/* 17 */
	/* constriant */
	GET_ORDER_CONS_LIST	,	/* get all order constraint names*/
	GET_LOCATION_CONS_LIST	,	/* get all location cons names */
	GET_COLOCATION_CONS_LIST,	/* get all colocation cons names*/
	GET_ORDER_CONSTRAINT	,	/* get an order constraint */
	GET_LOCATION_CONSTRAINT	,	/* get an location cons */
	GET_COLOCATION_CONSTRAINT,	/* get a colocation cons */
					/* 23 */
	/* end */
	QUERY_CTXID_END			/* END of list */
};

enum UPDATE_CTXIDS {
	/* resource */
	DEL_RESOURCE = 0	,	/* delete a resource */
	DEL_OPERATION		,	/* delete an operation */
	DEL_ATTRIBUTES		,	/* delete an attribute */
	UPDATE_MASTER		,	/* update a master-slave resource */
        UPDATE_CLONE		,	/* update a clone resource */
	UPDATE_OPERATIONS	,	/* update operations */
	UPDATE_ATTRIBUTES	,	/* update attributes */
	CREATE_RESOURCE		,	/* create a resource */
	CREATE_RSC_GROUP	,	/* create a resource group */

	/* constraint */
	DEL_LOCATION_CONSTRAINT	,	/* delete a location cons */
	DEL_ORDER_CONSTRAINT	,	/* delete an order cons */
	DEL_COLOCATION_CONSTRAINT,	/* delete a colocation cons */
	UPDATE_LOCATION_CONSTRAINT,	/* update loationn constraint */
	UPDATE_ORDER_CONSTRAINT	,	/* update order constraint */
	UPDATE_COLOCATION_CONSTRAINT,	/* update colocation constraint */

	/* end */
	UPDATE_CTXID_END		/* END of list */
};

struct ha_msg *	cim_query_dispatch(int id, const char* param, void* out);
int		cim_update_dispatch(int id,const char*param, void*data, void*);

int             cim_get_hb_status (void);
int             cim_change_hb_state(int state);
struct ha_msg *	cim_get_hacf_config (void);
struct ha_msg *	cim_get_authkeys (void);
struct ha_msg *	cim_get_software_identity(void);

int             cim_update_hacf(struct ha_msg *msg);
int             cim_update_authkeys(struct ha_msg *msg);

struct ha_msg * cim_get_disabled_rsc_list(void);
int		cim_update_disabled_rsc_list(int add, const char *rscid);
int		cim_is_rsc_disabled(const char *rscid);
struct ha_msg*	cim_get_rsc_list(void);
struct ha_msg* 	cim_traverse_allrsc(struct ha_msg* list);

int		cim_store_rsc(int type, const char *rscid, struct ha_msg *rsc);
struct ha_msg*	cim_find_rsc(int type, const char * rscid);
int		cim_store_rsc_type(const char* rscid, struct ha_msg *type);
int		cim_get_rsc_type(const char * rscid);

int	cim_store_operation(const char* rscid,const char* opid,struct ha_msg*);
struct ha_msg*	cim_load_operation(const char* rscid, const char *opid);
int		cim_update_rsc(int type, const char *rscid, struct ha_msg *);
struct ha_msg * cim_get_subrsc_list(const char *rscid);
int		cim_add_subrsc(struct ha_msg *rsc, struct ha_msg *subrsc);

struct ha_msg * cim_get_rscops(const char *rscid);
int		cim_add_rscop(const char *rscid, struct ha_msg *op);
int		cim_del_rscop(const char *rscid, const char *opid);
int		cim_update_rscop(const char*rscid, const char*, struct ha_msg*);
int		cim_cib_addrsc(const char *rscid);

#define cim_list_length(msg) 		cl_msg_list_length(msg, CIM_MSG_LIST)
#define cim_list_index(msg,index) 					\
	((char *)cl_msg_list_nth_data(msg, CIM_MSG_LIST, index))

#define cim_list_add(msg, value)					\
	cl_msg_list_add_string(msg, CIM_MSG_LIST, value)		

#define cim_msg_add_child(parent,id, child)			\
	ha_msg_addstruct(parent, id, child)
#define cim_msg_find_child(parent, id)		cl_get_struct(parent,id)
#define cim_msg_remove_child(parent, id)	cl_msg_remove(parent,id)

int		cim_msg_children_count(struct ha_msg *parent);
const char *	cim_msg_child_name(struct ha_msg * parent, int index);
struct ha_msg * cim_msg_child_index(struct ha_msg *parent, int index);

#endif    /* _CLUSTER_INFO_H */
