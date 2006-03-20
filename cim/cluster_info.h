
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

enum { HB_RUNNING = 1, HB_STOPED = 2, HB_UNKNOWN = 3 };
enum { START_HB   = 1, STOP_HB   = 2, RESTART_HB = 3 };

enum GET_FUNC_IDS {
	/* cluster */
	GET_CRM_CONFIG = 0,
	GET_HB_CONFIG,
	GET_DC,

	/* node */
	GET_NODE_INFO,
	GET_NODE_LIST,
	
	/* resource */
	GET_RSC_CLASSES,
	GET_RSC_PROVIDERS,
	GET_RSC_TYPES,
	GET_RSC_LIST,
	GET_RSC_TYPE,
	GET_RSC_STATUS,
	GET_RSC_HOST,
	GET_SUB_RSC,
	GET_PRIMITIVE,
	GET_CLONE,
	GET_MASTER,

	/* resource attribute */
	GET_RSC_OPERATIONS,
	GET_RSC_ATTRIBUTES,

	/* constriant */
	GET_ORDER_CONS_LIST,
	GET_LOCATION_CONS_LIST,
	GET_COLOCATION_CONS_LIST,
	GET_ORDER_CONSTRAINT,
	GET_LOCATION_CONSTRAINT,
	GET_COLOCATION_CONSTRAINT,

	GET_FUNC_ID_END
};

enum UPDATE_FUNC_IDS {
	/* resource */
	DEL_RESOURCE = 0,
	DEL_OPERATIONS,
	DEL_ATTRIBUTES,
	UPDATE_MASTER,
        UPDATE_CLONE,
	UPDATE_OPERATIONS,
	UPDATE_ATTRIBUTES,
	CREATE_RESOURCE,
	CREATE_RSC_GROUP,

	/* constraint */
	DEL_LOCATION_CONSTRAINT,
	DEL_ORDER_CONSTRAINT,
	DEL_COLOCATION_CONSTRAINT,
	UPDATE_LOCATION_CONSTRAINT,
	UPDATE_ORDER_CONSTRAINT,
	UPDATE_COLOCATION_CONSTRAINT,

	UPDATE_FUNC_ID_END
};

#define cim_get_str(i,p,o)	((char*)cim_get(i,p,o))
#define cim_get_array(i,p,o)	((CIMArray*)cim_get(i,p,o))
#define cim_get_table(i,p,o)	((CIMTable*)cim_get(i,p,o))

void *		cim_get(int func_id, const char * param, void * out);
int		cim_update(int func_id, const char * param, void * data, void *out);

int             cim_get_hb_status (void);
int             cim_change_hb_state(int state);
CIMTable *	cim_get_hacf_config (void);
CIMTable *	cim_get_cluster_auth (void);
CIMTable *	cim_get_software_identity(void);
int             cim_set_cluster_config(CIMTable * table);
int             cim_set_cluster_auth(CIMTable * table);
int		cim_get_resource_type(const char * rscid);

#endif    /* _CLUSTER_INFO_H */
