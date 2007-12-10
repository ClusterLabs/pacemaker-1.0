/*
 * hbagent.c:  heartbeat snmp subagnet code.
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
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

#include <crm_internal.h>

#include "hbagent.h"

#include "hb_api.h"
#include "heartbeat.h"
#include "clplumbing/cl_log.h"
#include "clplumbing/coredumps.h"

#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif
#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif
#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif
#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif
#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <sys/wait.h>

#include "LHAClusterInfo.h"
#include "LHANodeTable.h"
#include "LHAIFStatusTable.h"
#include "LHAMembershipTable.h"
#include "LHAResourceGroupTable.h"
#include "LHAHeartbeatConfigInfo.h"

#include <signal.h>

#include <sys/types.h> /* getpid() */
#include <unistd.h>

#include <errno.h>

#include "saf/ais.h"

#define DEFAULT_TIME_OUT 5 /* default timeout value for snmp in sec. */
#define LHAAGENTID "lha-snmpagent"

static unsigned long hbInitialized = 0;
static ll_cluster_t * hb = NULL; /* heartbeat handle */
static char * myid = NULL; /* my node id */
static SaClmHandleT clm = 0;
static unsigned long clmInitialized = 0;

static GPtrArray * gNodeTable = NULL;
static GPtrArray * gIFTable = NULL;
static GPtrArray * gMembershipTable = NULL;
static GPtrArray * gResourceTable = NULL;

static int keep_running;

int init_heartbeat(void);
int get_heartbeat_fd(void);
int handle_heartbeat_msg(void);

int init_membership(void);
int get_membership_fd(void);
int handle_membership_msg(void);

int init_resource_table(void);

int init_storage(void);

void free_storage(void);

int walk_nodetable(void);
int walk_iftable(void);

int nodestatus_trap(const char * node, const char * status);
int ifstatus_trap(const char * node, const char * lnk, const char * status);
int membership_trap(const char * node, SaClmClusterChangesT status);
int hbagent_trap(int online, const char * node);

int ping_membership(int * mem_fd);

/* LHAHeartbeatConfigInfo partial-mode */
#define DEFAULT_REFRESH_TIMING  (0)

int  init_hbconfig(void);
void free_hbconfig(void);
int  hbconfig_get_str_value(const char * attr, char * * value);

static int    hbconfig_partial = 0;
static char * hbconfig[LHA_CONF_END];
static struct hbconfig_map {
	lha_hbconfig_t  attr_no;
	const char *    attr;
} hbconfig_map[] =
{
	{LHA_CONF_HBVERSION,   KEY_HBVERSION},
	{LHA_CONF_KEEPALIVE,   KEY_KEEPALIVE},
	{LHA_CONF_DEADTIME,    KEY_DEADTIME},
	{LHA_CONF_DEADPING,    KEY_DEADPING},
	{LHA_CONF_WARNTIME,    KEY_WARNTIME},
	{LHA_CONF_INITDEAD,    KEY_INITDEAD},
	{LHA_CONF_BAUDRATE,    KEY_BAUDRATE},
	{LHA_CONF_AUTOFAIL,    KEY_AUTOFAIL},
	{LHA_CONF_STONITH,     KEY_STONITH},
	{LHA_CONF_STONITHHOST, KEY_STONITHHOST},
	{LHA_CONF_RESPAWN,     KEY_CLIENT_CHILD},
	{LHA_CONF_END,         NULL}
};


uint32_t get_status_value(const char * status, const char * * status_array, uint32_t * value_array);

const char * NODE_STATUS [] = 
{
	"",
	INITSTATUS,
	UPSTATUS,
	ACTIVESTATUS,
	DEADSTATUS,
	""
};

static uint32_t NODE_STATUS_VALUE[] = 
{	
	LHANODESTATUS_UNKNOWN, 
	LHANODESTATUS_INIT,
	LHANODESTATUS_UP,
	LHANODESTATUS_ACTIVE,
	LHANODESTATUS_DEAD,
};

const char * NODE_TYPE [] =
{
	UNKNOWNNODE,
	NORMALNODE,
	PINGNODE,
	""
};

static uint32_t NODE_TYPE_VALUE[] =
{
	LHANODETYPE_UNKNOWN,
	LHANODETYPE_NORMAL,
	LHANODETYPE_PING
};

const char * IF_STATUS[] = 
{
	"",
	LINKUP,
	DEADSTATUS,
	""
};

static uint32_t IF_STATUS_VALUE[] = 
{
	LHAIFSTATUS_UNKNOWN,
	LHAIFSTATUS_UP,
	LHAIFSTATUS_DOWN
};

static RETSIGTYPE
stop_server(int a) {
    	keep_running = 0;
}

uint32_t 
get_status_value(const char * status, const char * * status_array, uint32_t * value_array)
{
	int i = 1;
	int found = 0;

	while (strlen(status_array[i])) {
		if (strncmp(status, status_array[i], 
					strlen(status_array[i])) == 0) {
			found = 1;
			break;
		}
		i++;
	}

	if (found)
		return value_array[i];
	else 
		return value_array[0];
}

int 
get_int_value(lha_group_t group, lha_attribute_t attr, size_t index, uint32_t * value)
{
	switch (group) {
		case LHA_CLUSTERINFO: 
			return clusterinfo_get_int_value(attr, index, value);

		case LHA_RESOURCEINFO:
			return rsinfo_get_int_value(attr, index, value);

		case LHA_NODEINFO:

		case LHA_IFSTATUSINFO:


		default:
			return HA_FAIL;
	}
}

int 
get_str_value(lha_group_t group, lha_attribute_t attr, size_t index, char * * value)
{
	switch (group) {

	    	case LHA_HBCONFIGINFO:

		case LHA_NODEINFO:

		case LHA_IFSTATUSINFO:

		case LHA_RESOURCEINFO:

		default:
			return HA_FAIL;
	}
}

int
clusterinfo_get_int_value(lha_attribute_t attr, size_t index, uint32_t * value)
{
    int i;
    uint32_t status;
    size_t count;
    const struct hb_nodeinfo * node;

    *value = 0;

    if (!gNodeTable) 
	return HA_FAIL;

    switch (attr) {

	case TOTAL_NODE_COUNT:

      	    *value = gNodeTable->len;
	    break;

	case LIVE_NODE_COUNT:

	    count = 0;

	    for (i = 0; i < gNodeTable->len; i++) {
		status = ((struct hb_nodeinfo *) g_ptr_array_index(gNodeTable, i))->status;
		if (status != LHANODESTATUS_DEAD &&
			status != LHANODESTATUS_UNKNOWN ) {
		    count++;
		}
	    }

	    *value = count;
	    break;

	case RESOURCE_GROUP_COUNT:
	    *value = gResourceTable->len;

	    break;

	case CURRENT_NODE_ID:
	    for (i = 0; i < gNodeTable->len; i++) {
		node = (struct hb_nodeinfo *) g_ptr_array_index(gNodeTable, i);
		if (strcmp(node->name, myid) == 0) {
		    *value = node->id;
		    break;
		}
	    }

	    break;

	default:
	    return HA_FAIL;
    }

    return HA_OK;
}

/* LHAHeartbeatConfigInfo partial-mode */
int
init_hbconfig(void)
{
    lha_hbconfig_t      attr_no;
    char *              value;

    for (attr_no = LHA_CONF_HBVERSION; attr_no < LHA_CONF_END; ++attr_no) {

        hbconfig[attr_no] = NULL;

        if(!hbconfig_partial)
            continue;

        if (hbconfig_get_str_value(hbconfig_map[attr_no].attr, &value)
                                                                != HA_OK) {
            cl_log(LOG_INFO, "LHAHeartbeatConfigInfo initialization error.  "
                             "maybe get_parameter returned NULL.");
            continue;
        }

        hbconfig[attr_no] = value;
    }

    return HA_OK;
}

void
free_hbconfig(void)
{
    lha_hbconfig_t      attr_no;

    if (!hbconfig_partial)
        return;

    for (attr_no = LHA_CONF_HBVERSION; attr_no < LHA_CONF_END; ++attr_no) {

        if (hbconfig[attr_no] != NULL)
            cl_free(hbconfig[attr_no]);

    }
}

int
hbconfig_get_str(lha_hbconfig_t attr_no, char * * value)
{
    static char err[] = "N/A";

    if (hbconfig[attr_no] != NULL) {

        *value = cl_strdup(hbconfig[attr_no]);

    } else {

        if (hbconfig_get_str_value(hbconfig_map[attr_no].attr, value)
                                                                != HA_OK) {
            cl_log(LOG_INFO, "LHAHeartbeatConfigInfo getting parameter error.  "
                             "maybe get_parameter returned NULL.");
            *value  = cl_strdup(err);
        }

    }

    return HA_OK;
}


int
hbconfig_get_str_value(const char * attr, char * * value)
{
    char * ret;
    static char err[] = "N/A";

    if ((ret = hb->llc_ops->get_parameter(hb, attr)) == NULL) {
	/*
	cl_log(LOG_INFO, "getting parameter [%s] error.", attr);
	cl_log(LOG_INFO, "reason: %s.", hb->llc_ops->errmsg(hb));
	*/

	/* we have to return HA_OK here otherwise the 
	   agent code would not progress */
	*value  = cl_strdup(err);
	return HA_OK;
    };

    *value = ret;

    return HA_OK;
}

GPtrArray *
get_hb_info(lha_group_t group)
{
    switch (group) {
	case LHA_NODEINFO:
	    return gNodeTable;

	case LHA_IFSTATUSINFO:
	    return gIFTable;

	case LHA_RESOURCEINFO:
	    return gResourceTable;

	case LHA_MEMBERSHIPINFO:
	    return gMembershipTable;

	default:
	    return NULL;
    }
}

/* functions which talk to heartbeat */

static void
NodeStatus(const char * node, const char * status, void * private)
{
        cl_log(LOG_NOTICE, "Status update: Node %s now has status %s"
        ,       node, status);
	walk_nodetable();

	nodestatus_trap(node, status);
}

static void
LinkStatus(const char * node, const char * lnk, const char * status
,       void * private)
{
        cl_log(LOG_NOTICE, "Link Status update: Link %s/%s now has status %s"
        ,       node, lnk, status);
	walk_iftable();

	ifstatus_trap(node, lnk, status);
}

int
init_storage(void)
{
	gNodeTable = g_ptr_array_new();
	gIFTable = g_ptr_array_new();
	gResourceTable = g_ptr_array_new();
	gMembershipTable = g_ptr_array_new();

	if (!gNodeTable || !gIFTable || !gResourceTable || !gMembershipTable){
	    	cl_log(LOG_ERR, "Storage allocation failure.  Out of Memory.");
		return HA_FAIL;
	} 

	return HA_OK;
}

static void
free_nodetable(void)
{
	struct hb_nodeinfo * node;

	if (!gNodeTable) 
		return;

	while (gNodeTable->len) {

		node = (struct hb_nodeinfo *) g_ptr_array_remove_index_fast(gNodeTable, 0);

		free(node->name);
		cl_free(node);
	}

	return;
}

static void
free_iftable(void)
{
	struct hb_ifinfo * interface;

	if (!gIFTable) 
		return;

	while (gIFTable->len) {
		interface = (struct hb_ifinfo *) g_ptr_array_remove_index_fast(gIFTable, 0);

		free(interface->name);
		free(interface->node);
		cl_free(interface);
	}

	return;
}

static void
free_resourcetable(void)
{
	struct hb_rsinfo * resource;

	if (!gResourceTable) 
		return;

	while (gResourceTable->len) {

		resource = (struct hb_rsinfo *) g_ptr_array_remove_index_fast(gResourceTable, 0);

		free(resource->resource);
		cl_free(resource);

	}

	return;
}

static void
free_membershiptable(void)
{
    	if (!gMembershipTable)
	    return;

	/* the membership info buffer is provided by us when we 
	   initiated the session, so don't really need to free
	   any memory. */

	while (gMembershipTable->len) {
	    	g_ptr_array_remove_index_fast(gMembershipTable, 0);
	}

	return;
}

void
free_storage(void)
{
    	free_nodetable();
	g_ptr_array_free(gNodeTable, 1);
	gNodeTable = NULL;

	free_iftable();
	g_ptr_array_free(gIFTable, 1);
	gIFTable = NULL;

	free_resourcetable();
	g_ptr_array_free(gResourceTable, 1);
	gResourceTable = NULL;

	free_membershiptable();
	g_ptr_array_free(gMembershipTable, 1);
	gMembershipTable = NULL;

}

int
init_heartbeat(void)
{
	char * parameter;
	hb = NULL;

	cl_log_set_entity("lha-snmpagent");
	cl_log_set_facility(LOG_USER);

	hb = ll_cluster_new("heartbeat");

	cl_log(LOG_DEBUG, "PID=%ld", (long)getpid());
	cl_log(LOG_DEBUG, "Signing in with heartbeat");

	if (hb->llc_ops->signon(hb, LHAAGENTID)!= HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

	/* See if we should drop cores somewhere odd... */
	parameter = hb->llc_ops->get_parameter(hb, KEY_COREROOTDIR);
	if (parameter) {
		cl_set_corerootdir(parameter);
		cl_free(parameter);
	}
	cl_cdtocoredir();

	if (NULL == (myid = cl_strdup(hb->llc_ops->get_mynodeid(hb)))) {
		cl_log(LOG_ERR, "Cannot get mynodeid");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

	if (hb->llc_ops->set_nstatus_callback(hb, NodeStatus, NULL) !=HA_OK){
	        cl_log(LOG_ERR, "Cannot set node status callback");
	        cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

	if (hb->llc_ops->set_ifstatus_callback(hb, LinkStatus, NULL)!=HA_OK){
	        cl_log(LOG_ERR, "Cannot set if status callback");
	        cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

#if 1
	/* walk the node table */
	if ( HA_OK != walk_nodetable() || HA_OK != walk_iftable() ) {
		return HA_FAIL;
	}
#endif

	hbInitialized = 1;
	return HA_OK;
}

int
get_heartbeat_fd(void)
{
	int ret, fd;
	if(!hbInitialized) {
		ret = init_heartbeat();
		if (ret != HA_OK) {
			return ret;
		}
	}

	if ((fd = hb->llc_ops->inputfd(hb)) < 0) {
		cl_log(LOG_ERR, "Cannot get inputfd");
		cl_log(LOG_ERR, "REASON, %s", hb->llc_ops->errmsg(hb));
	}
	return fd;
}

int
walk_nodetable(void)
{
	const char *name, *type, *status;
	struct hb_nodeinfo * node;
	size_t id = 0;
	cl_uuid_t uuid;

	if (gNodeTable) {
		free_nodetable();
	}

	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}
	while((name = hb->llc_ops->nextnode(hb))!= NULL) {
		id++;

		status = hb->llc_ops->node_status(hb, name);
		type = hb->llc_ops->node_type(hb, name);

		cl_log(LOG_INFO, "node %ld: %s, type: %s, status: %s", (unsigned long)id, name 
		,	type, status);

		node = (struct hb_nodeinfo *) cl_malloc(sizeof(struct hb_nodeinfo));
		if (!node) {
			cl_log(LOG_CRIT, "malloc failed for node info.");
			return HA_FAIL;
		}

		node->name =  g_strdup(name);
		node->ifcount = 0;
		node->id = id;

		memset(&uuid, 0, sizeof(cl_uuid_t));

#ifdef HAVE_NEW_HB_API
		/* the get_uuid_by_name is not available for STABLE_1_2 branch. */
		if (hb->llc_ops->get_uuid_by_name(hb, name, &uuid) == HA_FAIL) {
			cl_log(LOG_DEBUG, "Cannot get the uuid for node: %s", name);
		}
#endif /* HAVE_NEW_HB_API */
		node->uuid = uuid;

		node->type = get_status_value(type, NODE_TYPE, NODE_TYPE_VALUE);
		node->status = get_status_value(status, NODE_STATUS,
				NODE_STATUS_VALUE);

		g_ptr_array_add(gNodeTable, (gpointer *) node); 
	}
	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}
	return HA_OK;
}

int
walk_iftable(void)
{
	const char *name, * status;
	struct hb_nodeinfo * node;
	struct hb_ifinfo * interface;
	int i; 
	size_t ifcount;

	if (gIFTable) {
		free_iftable();
	}

	for (i = 0; i < gNodeTable->len; i++) {
		node = (struct hb_nodeinfo *) g_ptr_array_index(gNodeTable, i);
		ifcount = 0;

		if (hb->llc_ops->init_ifwalk(hb, node->name) != HA_OK) {
			cl_log(LOG_ERR, "Cannot start if walk");
			cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			return HA_FAIL;
		}

		while((name = hb->llc_ops->nextif(hb))!=NULL) {
			status = hb->llc_ops->if_status(hb, node->name, name);

			cl_log(LOG_INFO, "node: %s, interface: %s, status: %s",
					node->name, name, status);

			interface = (struct hb_ifinfo *) cl_malloc(sizeof(struct hb_ifinfo));
			if (!interface) {
				cl_log(LOG_CRIT, "malloc failed for if info.");
				return HA_FAIL;
			}

			interface->name = g_strdup(name);
			interface->node = g_strdup(node->name);
			interface->nodeid = node->id;
			interface->id = ++ifcount;
			interface->status = get_status_value(status,
					IF_STATUS, IF_STATUS_VALUE);

			g_ptr_array_add(gIFTable, (gpointer *) interface);
		}

		node->ifcount = ifcount;

		if (hb->llc_ops->end_ifwalk(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot end if walk.");
			cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			return HA_FAIL;
		}
	}
	return HA_OK;
}

int
handle_heartbeat_msg(void)
{
	IPC_Channel * chan;
	struct ha_msg * msg;
	const char * type, * node;

	while (hb->llc_ops->msgready(hb)) {

		chan = hb->llc_ops->ipcchan(hb);
		/* this happens when the main heartbeat daemon is not there
		   any more */
		if (chan->ch_status == IPC_DISCONNECT) {
			return HA_FAIL;
		} 

		msg = hb->llc_ops->readmsg(hb, 0);
		if (!msg) {
			cl_log(LOG_DEBUG, "read_hb_msg returned NULL.");
			continue;
		}

		type = ha_msg_value(msg, F_TYPE);
		node = ha_msg_value(msg, F_ORIG);
		if (!type || !node) {
			/* can't read type. log and ignore the msg. */
			cl_log(LOG_DEBUG, "Can't read msg type.");
			return HA_OK;
		}

		/* we only handle the shutdown msg for now. */
		if (strcmp(myid, node) == 0
		&&		STRNCMP_CONST(type, T_SHUTDONE) == 0) {
			return HA_FAIL;
		}
		ha_msg_del(msg);
		msg = NULL;
	}
	return HA_OK;
}

static void
clm_track_cb(SaClmClusterNotificationT *nbuf, SaUint32T nitem,
	SaUint32T nmem, SaUint64T nview, SaErrorT error)
{
        int i;
	const char * node;
	SaClmClusterChangesT status;

        free_membershiptable();

        for (i = 0; i < nitem; i++) {

	    	cl_log(LOG_INFO, "adding %s in membership table", nbuf[i].clusterNode.nodeName.value);

                g_ptr_array_add(gMembershipTable, (gpointer *) &nbuf[i]);
        }

	if (clmInitialized) {

		for (i = 0; i < nitem; i++) {
		    	status = nbuf[i].clusterChanges;
			node = (char *)nbuf[i].clusterNode.nodeName.value;

			if (status == SA_CLM_NODE_NO_CHANGE) {
			    	continue;
			}

	    		membership_trap(node, status);
		}
	}
}

static void
clm_node_cb(SaInvocationT invocation, SaClmClusterNodeT *clusterNode,
	SaErrorT error)
{
    	return;
}

int
init_membership(void)
{
	SaErrorT ret;
	static SaClmClusterNotificationT * nbuf = NULL;
	SaClmHandleT handle;

	SaClmCallbacksT my_callbacks = {
	    .saClmClusterTrackCallback 
		= (SaClmClusterTrackCallbackT) clm_track_cb,
	    .saClmClusterNodeGetCallback 
		= (SaClmClusterNodeGetCallbackT) clm_node_cb,
	};

	if ((ret = saClmInitialize(&handle, &my_callbacks, NULL)) != SA_OK) {
	    cl_log(LOG_DEBUG, "Membership service currently not available.  Will try again later. errno [%d]",ret);
	    return HA_OK;
	}

	if (nbuf) {
		cl_free(nbuf);
	}

        nbuf = (SaClmClusterNotificationT *) cl_malloc(gNodeTable->len *
                                sizeof (SaClmClusterNotificationT));
	if (!nbuf) {
		cl_log(LOG_ERR, "%s: cl_malloc failed for SaClmClusterNotificationT.", __FUNCTION__);
		return HA_FAIL;
	}

        if (saClmClusterTrackStart(&handle, SA_TRACK_CURRENT, nbuf,
                gNodeTable->len) != SA_OK) {
                cl_log(LOG_ERR, "SA_TRACK_CURRENT error, errno [%d]", ret);
                cl_free(nbuf);
                return HA_FAIL;
        }

        /* Start to track cluster membership changes events */
        if (saClmClusterTrackStart(&handle, SA_TRACK_CHANGES, nbuf,
                gNodeTable->len) != SA_OK) {
                cl_log(LOG_ERR, "SA_TRACK_CURRENT error, errno [%d]", ret);
                cl_free(nbuf);
                return HA_FAIL;
        }

	clm = handle;

	clmInitialized = 1;
	cl_log(LOG_INFO, "Membership service initialized successfully.");

	return HA_OK;
}

int
get_membership_fd(void)
{
    	SaErrorT ret;
	SaSelectionObjectT st;

	if (!clmInitialized)
		return 0;

	if ((ret = saClmSelectionObjectGet(&clm, &st)) != SA_OK) {
	    	cl_log(LOG_ERR, "saClmSelectionObjectGet error, errno [%d]", ret);
		return -1;
	} 

	return (int) st;
}

int
handle_membership_msg(void)
{
    	SaErrorT ret;

	if ((ret = saClmDispatch(&clm, SA_DISPATCH_ALL)) != SA_OK) {
		if (ret == SA_ERR_LIBRARY) {

		    	cl_log(LOG_DEBUG, "I am evicted.");

			/* mark the membership as uninitialized and try 
			   again later. */
			clmInitialized = 0;
			free_membershiptable();
                        
                        /* finalize membership */
                        saClmFinalize(&clm);
			return HA_OK;
		} else {
		    cl_log(LOG_WARNING, "saClmDispatch error, ret = [%d]", ret);
		}
	}

    	return HA_OK;
}

int
ping_membership(int * mem_fd)
{
	int fd;

	if (clmInitialized) 
		return 0;

	init_membership();
	fd = get_membership_fd();

	*mem_fd = fd;

	return HA_OK;
}

int
init_resource_table(void)
{
    	int rc, i, count, found;
	FILE * rcsf;
	char buf[MAXLINE];
	char host[MAXLINE], pad[MAXLINE];
	struct hb_rsinfo * resource;
	struct hb_nodeinfo * node;
	size_t nodeid = 0;

	if (gResourceTable) {
	    	free_resourcetable();
	}

	if ((rcsf = fopen(RESOURCE_CFG, "r")) == NULL) {
	    	cl_log(LOG_ERR, "Cannot open file %s", RESOURCE_CFG);
		return HA_FAIL;
	}

	count = 0;
	for (;;) {
	    	errno = 0;
		if (fgets(buf, MAXLINE, rcsf) == NULL) {
		    	if (ferror(rcsf)) {
			    	cl_perror("fgets failure");
			}
			break;
		}
		/* remove the comments */
		if (buf[0] == '#') {
		    	continue;
		} 

		if (buf[strlen(buf) - 1] == '\n') 
		    buf[strlen(buf) - 1] = EOS;

		if ((rc = sscanf(buf, "%s %[^\n\t\r]", host, pad)) < 1) {
		    	cl_log(LOG_WARNING, "%s syntax error?", RESOURCE_CFG);
		};

		/* make sure that the host node is in the node list */
		found = 0;
		for (i = 0; i < gNodeTable->len; i++) {
		    	node = (struct hb_nodeinfo *) g_ptr_array_index(gNodeTable, i);
			if (strcmp(node->name, host) == 0) {
			    	found = 1;
				nodeid = node->id;
			    	break;
			}
		}
		if (!found)
		    continue;

		resource = (struct hb_rsinfo *) cl_malloc(sizeof(struct hb_rsinfo));
		if (!resource) {
			cl_log(LOG_CRIT, "malloc resource info failed.");
			fclose(rcsf);
			return HA_FAIL;
		}

		resource->id = ++count;
		resource->masternodeid = nodeid;
		resource->resource = g_strdup(pad);

		g_ptr_array_add(gResourceTable, (gpointer *) resource);
	}

	fclose(rcsf);
	return HA_OK;
}

int 
rsinfo_get_int_value(lha_attribute_t attr, size_t index, uint32_t * value)
{
    uint32_t rc = 0;
    char getcmd[MAXLINE];
    char * resource;

    *value = 0;

    if (!gResourceTable)
	return HA_FAIL;

    switch (attr) {
	case RESOURCE_STATUS:
	    resource = ((struct hb_rsinfo *) g_ptr_array_index(gResourceTable, index))->resource;
	    sprintf(getcmd, HA_NOARCHDATAHBDIR "/ResourceManager status %s", resource);
	    rc = system(getcmd);
	    /* cl_log(LOG_INFO, "resource [%s] status: [%d]", resource, WEXITSTATUS(rc)); */

	    *value = WEXITSTATUS(rc);
	    break;

	default: 
	    return HA_FAIL;
    }

    return HA_OK;
}


int 
nodestatus_trap(const char * node, const char * status)
{
    uint32_t svalue = 0;

    oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t objid_snmptrap_len = OID_LENGTH(objid_snmptrap);

    oid  trap_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 900, 1 };
    size_t trap_oid_len = OID_LENGTH(trap_oid);

    oid  nodename_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 2, 1, 2 };
    size_t nodename_oid_len = OID_LENGTH(nodename_oid);

    oid  nodestatus_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 2, 1, 4 };
    size_t nodestatus_oid_len = OID_LENGTH(nodestatus_oid);

    netsnmp_variable_list *notification_vars = NULL;

    svalue = get_status_value(status, NODE_STATUS, NODE_STATUS_VALUE);

    snmp_varlist_add_variable(&notification_vars,
                              /*
                               * the snmpTrapOID.0 variable
                               */
                              objid_snmptrap, objid_snmptrap_len,
                              /*
                               * value type is an OID
                               */
                              ASN_OBJECT_ID,
                              /*
                               * value contents is our notification OID
                               */
                              (u_char *) trap_oid,
                              /*
                               * size in bytes = oid length * sizeof(oid)
                               */
                              trap_oid_len * sizeof(oid));


    snmp_varlist_add_variable(&notification_vars,
                              nodename_oid, 
			      nodename_oid_len,
                              ASN_OCTET_STR,
                              (const u_char *) node,
                              strlen(node)); /* do NOT use strlen() +1 */

    snmp_varlist_add_variable(&notification_vars,
                              nodestatus_oid, 
			      nodestatus_oid_len,
                              ASN_INTEGER,
                              (const u_char *) & svalue,
                              sizeof(svalue)); 

    cl_log(LOG_INFO, "sending node status trap. node: %s: status %s, value:%d", 
	    node, status, svalue);
    send_v2trap(notification_vars);
    snmp_free_varbind(notification_vars);

    return HA_OK;
}

int 
ifstatus_trap(const char * node, const char * lnk, const char * status)
{
    uint32_t svalue = 0;

    oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t objid_snmptrap_len = OID_LENGTH(objid_snmptrap);

    oid  trap_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 900, 3 };
    size_t trap_oid_len = OID_LENGTH(trap_oid);

    oid  nodename_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 2, 1, 2 };
    size_t nodename_oid_len = OID_LENGTH(nodename_oid);

    oid  ifname_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 3, 1, 2 };
    size_t ifname_oid_len = OID_LENGTH(ifname_oid);

    oid  ifstatus_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 3, 1, 3 };
    size_t ifstatus_oid_len = OID_LENGTH(ifstatus_oid);

    netsnmp_variable_list *notification_vars = NULL;

    svalue = get_status_value(status, IF_STATUS, IF_STATUS_VALUE);

    snmp_varlist_add_variable(&notification_vars,
                              /*
                               * the snmpTrapOID.0 variable
                               */
                              objid_snmptrap, objid_snmptrap_len,
                              /*
                               * value type is an OID
                               */
                              ASN_OBJECT_ID,
                              /*
                               * value contents is our notification OID
                               */
                              (u_char *) trap_oid,
                              /*
                               * size in bytes = oid length * sizeof(oid)
                               */
                              trap_oid_len * sizeof(oid));

    snmp_varlist_add_variable(&notification_vars,
                              nodename_oid, 
			      nodename_oid_len,
                              ASN_OCTET_STR,
                              (const u_char *) node,
                              strlen(node)); /* do NOT use strlen() +1 */

    snmp_varlist_add_variable(&notification_vars,
                              ifname_oid, 
			      ifname_oid_len,
                              ASN_OCTET_STR,
                              (const u_char *) lnk,
                              strlen(lnk)); /* do NOT use strlen() +1 */

    snmp_varlist_add_variable(&notification_vars,
                              ifstatus_oid, 
			      ifstatus_oid_len,
                              ASN_INTEGER,
                              (const u_char *) & svalue,
                              sizeof(svalue)); 

    cl_log(LOG_INFO, "sending ifstatus trap. node:%s, lnk: %s, status:%s, value: %d", 
	    node, lnk, status, svalue);
    send_v2trap(notification_vars);
    snmp_free_varbind(notification_vars);

    return HA_OK;
}

int 
hbagent_trap(int online, const char * node)
{
    netsnmp_variable_list *notification_vars = NULL;

    oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t objid_snmptrap_len = OID_LENGTH(objid_snmptrap);

    oid  nodename_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 2, 1, 2 };
    size_t nodename_oid_len = OID_LENGTH(nodename_oid);

  /* this is the oid for the hbagent online trap */
    oid  trap_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 900, 7 };
    size_t trap_oid_len = OID_LENGTH(trap_oid);

    /* this is the oid for the offline trap */
    if (!online) {
	    trap_oid[trap_oid_len - 1] = 9;
    }


    snmp_varlist_add_variable(&notification_vars,
                              /*
                               * the snmpTrapOID.0 variable
                               */
                              objid_snmptrap, objid_snmptrap_len,
                              /*
                               * value type is an OID
                               */
                              ASN_OBJECT_ID,
                              /*
                               * value contents is our notification OID
                               */
                              (u_char *) trap_oid,
                              /*
                               * size in bytes = oid length * sizeof(oid)
                               */
                              trap_oid_len * sizeof(oid));

    snmp_varlist_add_variable(&notification_vars,
                              nodename_oid, 
			      nodename_oid_len,
                              ASN_OCTET_STR,
                              (const u_char *) node,
                              strlen(node)); /* do NOT use strlen() +1 */

    cl_log(LOG_INFO, "sending hbagent trap. status:%d", online);
    send_v2trap(notification_vars);
    snmp_free_varbind(notification_vars);

    return HA_OK;
}


int 
membership_trap(const char * node, SaClmClusterChangesT status)
{
    oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t objid_snmptrap_len = OID_LENGTH(objid_snmptrap);

    oid  trap_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 900, 5 };
    size_t trap_oid_len = OID_LENGTH(trap_oid);

    oid  nodename_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 2, 1, 2 };
    size_t nodename_oid_len = OID_LENGTH(nodename_oid);

    oid  membershipchange_oid[] = { 1, 3, 6, 1, 4, 1, 4682, 6, 1, 6 };
    size_t membershipchange_oid_len = OID_LENGTH(membershipchange_oid);

    netsnmp_variable_list *notification_vars = NULL;

    snmp_varlist_add_variable(&notification_vars,
                              /*
                               * the snmpTrapOID.0 variable
                               */
                              objid_snmptrap, objid_snmptrap_len,
                              /*
                               * value type is an OID
                               */
                              ASN_OBJECT_ID,
                              /*
                               * value contents is our notification OID
                               */
                              (u_char *) trap_oid,
                              /*
                               * size in bytes = oid length * sizeof(oid)
                               */
                              trap_oid_len * sizeof(oid));

    snmp_varlist_add_variable(&notification_vars,
                              nodename_oid, 
			      nodename_oid_len,
                              ASN_OCTET_STR,
                              (const u_char *) node,
                              strlen(node)); /* do NOT use strlen() +1 */

    snmp_varlist_add_variable(&notification_vars,
                              membershipchange_oid, 
			      membershipchange_oid_len,
                              ASN_INTEGER,
                              (u_char *) & status,
                              sizeof(status)); 

    cl_log(LOG_INFO, "sending membership trap. node:%s, status:%d", 
	    node, status);
    send_v2trap(notification_vars);
    snmp_free_varbind(notification_vars);

    return HA_OK;
}

static void
usage(void)
{
    	fprintf(stderr, "Usage: hbagent [Options]\n"
    			"Options:\n"
    			" -d         enable logging to STDERR\n"
    			" -r SECOND  set LHAHeartbeatConfigInfo to partial-mode\n"
    			"            SECOND -- time interval between update "
    			"LHAHeartbeatConfigInfo\n"
    			"                      o multiple of 5\n"
    			"                      o '0' keeps initial value (not update)\n"
    	);
}

int
main(int argc, char ** argv) 
{
	int ret;

	fd_set fdset;
	struct timeval tv, *tvp;
	int flag, block = 0, numfds, hb_fd = 0, mem_fd = 0, debug = 0;
	int hb_already_dead = 0;

	/* change this if you want to be a SNMP master agent */
	int agentx_subagent=1; 

	/* change this if you want to run in the background */
	int background = 0; 

	/* change this if you want to use syslog */
	int syslog = 1; 

	/* LHAHeartbeatConfigInfo partial-mode */
	int hbconfig_refresh_timing = 0;
	int hbconfig_refresh_cnt;

	while ((flag = getopt(argc, argv, "dr:h")) != EOF) {
	    	int i;

	    	switch (flag) {
		    case 'd':
			debug++ ;
			break;
		    case 'r': /* LHAHeartbeatConfigInfo partial-mode */
			hbconfig_partial = 1;
			i = atoi(optarg);
			if (i > 0)
				hbconfig_refresh_timing = i / DEFAULT_TIME_OUT;
			else
				hbconfig_refresh_timing = DEFAULT_REFRESH_TIMING;
			break;
		    case 'h':
			usage();
			exit(0);
		    default: 
			usage();
			exit(1);
		}
	}

	if (debug) 
		cl_log_enable_stderr(TRUE);
	else 
		cl_log_enable_stderr(FALSE);

	/* print log errors to syslog or stderr */
	if (syslog)
		snmp_enable_calllog();
	else
		snmp_enable_stderrlog();

	/* we're an agentx subagent? */
	if (agentx_subagent) {
		/* make us a agentx client. */
		netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
				NETSNMP_DS_AGENT_ROLE, 1);
	}

	/* run in background, if requested */
	if (background && netsnmp_daemonize(1, !syslog))
		exit(1);

	/* initialize the agent library */
	if (init_agent("LHA-agent")) {
		cl_log(LOG_ERR, "AgentX initialization failure.  You can only run this as root.  Also make sure that the master snmpd is started and is accepting AgentX connetions.  The subagent will not be respawned.");

		return 100;
	}

	/* initialize mib code here */

	if ((ret = init_storage()) != HA_OK) {
	    	return -2;
	}

	if ((ret = init_heartbeat()) != HA_OK ||
	    	(hb_fd = get_heartbeat_fd()) <=0) {
                return -1;
        }

	if ((ret = init_resource_table()) != HA_OK) {
	    	cl_log(LOG_ERR, "resource table initialization failure.");
	}

	ret = init_membership();
	mem_fd = get_membership_fd();

	if (ret != HA_OK) {
		cl_log(LOG_ERR, "fatal error during membership initialization. ");
	}  

	/*
	if ((ret = init_membership() != HA_OK) ||
		(mem_fd = get_membership_fd()) <= 0) {
	    	cl_log(LOG_DEBUG, "membership initialization failure.  You will not be able to view any membership information in this cluster.");
	} 
	*/

	/* LHAHeartbeatConfigInfo partial-mode */
	if ((ret = init_hbconfig()) != HA_OK) {
	    	return -3;
	}

	init_LHAClusterInfo();
	init_LHANodeTable();
	init_LHAIFStatusTable();
	init_LHAResourceGroupTable();
	init_LHAMembershipTable();
	init_LHAHeartbeatConfigInfo();

	/* LHA-agent will be used to read LHA-agent.conf files. */
	init_snmp("LHA-agent");

	/* If we're going to be a snmp master agent, initial the ports */
	if (!agentx_subagent)
		init_master_agent();  /* open the port to listen on (defaults to udp:161) */

	/* In case we recevie a request to stop (kill -TERM or kill -INT) */
	keep_running = 1;
	signal(SIGTERM, stop_server);
	signal(SIGINT, stop_server);

	snmp_log(LOG_INFO,"LHA-agent is up and running.\n");

	hbagent_trap(1, myid);

	hbconfig_refresh_cnt = 0;

	/* you're main loop here... */
	while(keep_running) {
	/* if you use select(), see snmp_select_info() in snmp_api(3) */
	/*     --- OR ---  */
		/* 0 == don't block */
		/* agent_check_and_process(1); */

		FD_ZERO(&fdset);
                FD_SET(hb_fd, &fdset);
		numfds = hb_fd + 1;

		if (clmInitialized) {
			FD_SET(mem_fd, &fdset);

			if (mem_fd > hb_fd)
				numfds = mem_fd + 1;
		}

		tv.tv_sec = DEFAULT_TIME_OUT;
		tv.tv_usec = 0;
		tvp = &tv;

		snmp_select_info(&numfds, &fdset, tvp, &block);

		if (block) {
			tvp = NULL;
		} else if (tvp->tv_sec == 0) {
		    tvp->tv_sec = DEFAULT_TIME_OUT;
		    tvp->tv_usec = 0;
		}

		ret = select(numfds, &fdset, 0, 0, tvp);

		if (ret < 0) {
			if (errno == EBADF) {
				struct stat	foo;
				/* Probably the membership layer shut down for some reason. */
				if (mem_fd >= 0 && fstat(mem_fd, &foo) < 0 && errno == EBADF) {
					cl_log(LOG_WARNING, "membership layer shut down.");
					FD_CLR(mem_fd, &fdset);
					if (numfds == mem_fd + 1) {
						numfds--;
					}
					mem_fd = -1;
					break;
				}
			}
				
			/* error */
			cl_perror("select() returned an error. shutting down");
			break;
		} else if (ret == 0) {
			/* timeout */
			ping_membership(&mem_fd);
			/* LHAHeartbeatConfigInfo partial-mode */
			if (hbconfig_refresh_timing
			&& ++hbconfig_refresh_cnt >= hbconfig_refresh_timing) {
				free_hbconfig();
				init_hbconfig();
				hbconfig_refresh_cnt = 0;
			}
			snmp_timeout();
			goto process_pending;
		} 

		if (FD_ISSET(hb_fd, &fdset)) {
			/* heartbeat */

			if ((ret = handle_heartbeat_msg()) == HA_FAIL) {
				cl_log(LOG_DEBUG, "no heartbeat. quit now.");
				hb_already_dead = 1;
				break;
			}
		} else  if (clmInitialized && FD_ISSET(mem_fd, &fdset)) {
		    	/* membership events */

		    	if ((ret = handle_membership_msg()) == HA_FAIL) {
			    	cl_log(LOG_DEBUG, "unrecoverable membership error. quit now.");
				break;
			}
		} else {

			/* snmp request */
			snmp_read(&fdset);
		}

process_pending:
		run_alarms();
		netsnmp_check_outstanding_agent_requests();

	}

	/* at shutdown time */
	
	hbagent_trap(0, myid);
	snmp_shutdown("LHA-agent");

	free_hbconfig();
	cl_free(myid);
	free_storage();

        if (!hb_already_dead && hb->llc_ops->signoff(hb, TRUE) != HA_OK) {
                cl_log(LOG_ERR, "Cannot sign off from heartbeat.");
                cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
                exit(10);
        }
        if (hb->llc_ops->delete(hb) != HA_OK) {
                cl_log(LOG_ERR, "Cannot delete API object.");
                cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
                exit(11);
        }

	return 0;
}


