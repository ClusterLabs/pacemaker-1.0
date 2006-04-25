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

#define S_RES_PRIMITIVE		"native"
#define S_RES_GROUP		"group"
#define S_RES_CLONE		"clone"
#define S_RES_MASTER		"master"
#define S_RES_UNKNOWN		"unknown"

#define	CIM_MSG_LIST		"__list__"
#define	CIM_MSG_TAG		"__tag__"
#define CIM_MSG_ATTR_ID		"id"	/* every node-msg should have this */
#define CIM_MSG_INST_ATTR	"instance_attributes"
#define CIM_MSG_ATTR		"attributes"


#define	HA_CIM_VARDIR		"/var/lib/heartbeat/cim"

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
	CLEANUP_RESOURCE	,

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

/* cluster */
struct ha_msg *	cim_get_software_identity(void);
int             cim_get_hb_status (void);
int             cim_change_hb_state(int state);
struct ha_msg *	cim_get_hacf_config (void);
struct ha_msg *	cim_get_authkeys (void);
int             cim_update_hacf(struct ha_msg *msg);
int             cim_update_authkeys(struct ha_msg *msg);

#define RESOURCE_ENABLED(rscid) (cim_rsc_is_in_cib(rscid))
#define RESOURCE_DISABLED(rscid) (!cim_rsc_is_in_cib(rscid))

/* get a resource's type: TID_RES_PRIMITIVE, etc */
int		cim_get_rsctype(const char * rscid);

/* resource list */
int		cim_rsc_is_in_cib(const char *rscid);

/* not include sub resources */
struct ha_msg*	cim_get_all_rsc_list(void);

/* retrive the closure of the list */
struct ha_msg* 	cim_traverse_allrsc(struct ha_msg* list);

/* resource operations */
struct ha_msg * cim_get_rscops(const char *rscid);
int		cim_add_rscop(const char *rscid, struct ha_msg *op);
int		cim_del_rscop(const char *rscid, const char *opid);
int		cim_update_rscop(const char*rscid, const char*, struct ha_msg*);

/* resource */
/* submit a resource to CIB */
int		cim_rsc_submit(const char *rscid);
/* store a resource image on the disk */
int		cim_rscdb_store(int type, const char *rscid, struct ha_msg *rsc);
/* cleanup a resource image from the disk, as well as its attributes, type ... */
int		cim_rscdb_cleanup(int type, const char * rscid);
struct ha_msg*	cim_find_rsc(int type, const char * rscid);
int		cim_update_rsc(int type, const char *rscid, struct ha_msg *);
int		cim_remove_rsc(const char * rscid);

/* sub resource */
struct ha_msg * cim_get_subrsc_list(const char *rscid);
int		cim_add_subrsc(struct ha_msg *rsc, struct ha_msg *subrsc);

/* resource attributes */
struct ha_msg *	cim_rscattrs_get(const char *rscid);
int		cim_rscattrs_del(const char *rscid);
int		cim_update_attrnvpair(const char*, const char*, struct ha_msg*);
int		cim_remove_attrnvpair(const char* rscid, const char* attrid);

#endif    /* _CLUSTER_INFO_H */
