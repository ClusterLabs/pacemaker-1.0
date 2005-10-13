/*
 * hbagent.c:  heartbeat snmp subagnet code.
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 *         Jia Ming Pan (jmltc@cn.ibm.com)
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

#include <portability.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "linuxha_info.h"
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

#include <sys/wait.h>
#include <sys/types.h> /* getpid() */
#include <unistd.h>
#include <errno.h>

#include "saf/ais.h"


/* enums for column LHANodeType */
#define LHANODETYPE_UNKNOWN             0
#define LHANODETYPE_NORMAL              1
#define LHANODETYPE_PING                2

/* enums for column LHANodeStatus */
#define LHANODESTATUS_UNKNOWN           0
#define LHANODESTATUS_INIT              1
#define LHANODESTATUS_UP                2
#define LHANODESTATUS_ACTIVE            3
#define LHANODESTATUS_DEAD              4

#define LHAIFSTATUS_UNKNOWN             0
#define LHAIFSTATUS_UP                  1
#define LHAIFSTATUS_DOWN                2


static char * hb_client_id = NULL; 
static int hb_initialized = 0;

unsigned long clm_initialized = 0;

static ll_cluster_t * hb = NULL;        /* heartbeat handle */
static char * myid = NULL;              /* my node id */
static SaClmHandleT clm = 0;

static GPtrArray * gNodeTable = NULL;
static GPtrArray * gIFTable = NULL;
static GPtrArray * gMembershipTable = NULL;
static GPtrArray * gResourceTable = NULL;

static node_event_hook_t node_event_hook = NULL;
static if_event_hook_t   if_event_hook   = NULL;
static membership_event_hook_t membership_event_hook = NULL;


uint32_t get_status_value(const char * status, const char * * status_array, 
                          uint32_t * value_array);


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


const char *
get_status(uint32_t status_value)
{
        int i;
        
        for (i = 0; i < sizeof(NODE_STATUS_VALUE); i++) {
                if (status_value == NODE_STATUS_VALUE[i]){
                        return NODE_STATUS[i];
                }
        }
        return NULL;
}

uint32_t 
get_status_value(const char * status, const char ** status_array, 
                 uint32_t * value_array)
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
get_int_value(lha_group_t group, lha_attribute_t attr, size_t index, 
              uint32_t * value)
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
get_str_value(lha_group_t group, lha_attribute_t attr, size_t index, 
              char ** value)
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
                        status = ((struct hb_nodeinfo *) 
                                     g_ptr_array_index(gNodeTable, i))->status;

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
                        node = (struct hb_nodeinfo *) 
                                        g_ptr_array_index(gNodeTable, i);
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

int
hbconfig_get_str_value(const char * attr, char ** value)
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
                *value  = ha_strdup(err);
                return HA_OK;
        };

        *value = ha_strdup(ret);

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
                ,           node, status);
        walk_nodetable();

        /* user hook handler */
        if ( node_event_hook ){
                node_event_hook(node, status);
        }
}

static void
LinkStatus(const char * node, const char * lnk, const char * status
,           void * private)
{
        cl_log(LOG_NOTICE, "Link Status update: Link %s/%s now has status %s"
                ,           node, lnk, status);
        walk_iftable();

        /* user hook handler */
        if ( if_event_hook ){
                if_event_hook(node, lnk, status);
        }
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

                node = (struct hb_nodeinfo *) 
                        g_ptr_array_remove_index_fast(gNodeTable, 0);

                free(node->name);
                ha_free(node);
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
                interface = (struct hb_ifinfo *) 
                        g_ptr_array_remove_index_fast(gIFTable, 0);

                free(interface->name);
                free(interface->node);
                ha_free(interface);
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

                resource = (struct hb_rsinfo *) 
                        g_ptr_array_remove_index_fast(gResourceTable, 0);

                free(resource->resource);
                ha_free(resource);

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
        g_ptr_array_free(gNodeTable, 0);
        gNodeTable = NULL;

        free_iftable();
        g_ptr_array_free(gIFTable, 0);
        gIFTable = NULL;

        free_resourcetable();
        g_ptr_array_free(gResourceTable, 0);
        gResourceTable = NULL;

        free_membershiptable();
        g_ptr_array_free(gMembershipTable, 0);
        gResourceTable = NULL;

}

int
init_heartbeat(void)
{
        const char * parameter;
        hb = NULL;

        hb = ll_cluster_new("heartbeat");

        cl_log(LOG_DEBUG, "hb_client_id = %s", hb_client_id);
        cl_log(LOG_DEBUG, "PID = %ld", (long)getpid());
        cl_log(LOG_DEBUG, "Signing in with heartbeat");

        if (hb->llc_ops->signon(hb, hb_client_id)!= HA_OK) {
                cl_log(LOG_ERR, "Cannot sign on with heartbeat");
                cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
                return HA_FAIL;
        }

        /* See if we should drop cores somewhere odd... */
        parameter = hb->llc_ops->get_parameter(hb, KEY_COREROOTDIR);
        if (parameter) {
                cl_set_corerootdir(parameter);
        }
        cl_cdtocoredir();

        if (NULL == (myid = ha_strdup(hb->llc_ops->get_mynodeid(hb)))) {
                cl_log(LOG_ERR, "Cannot get mynodeid");
                cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));

                free_heartbeat();
                return HA_FAIL;
        }

        cl_log(LOG_INFO, "%s: ready to walk_nodetable and walk_iftable",
                         __FUNCTION__);

        /* walk the node table */
        if ( HA_OK != walk_nodetable() || HA_OK != walk_iftable() ) {
                free_heartbeat();
                return HA_FAIL;
        }

        return HA_OK;
}

int hb_set_callbacks() { 

        if ( hb == NULL ) {
                cl_log(LOG_ERR, "%s: heartbeat not initialized", __FUNCTION__);
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

        return HA_OK;
}

int free_heartbeat(){
        ha_free(myid);

        if (hb->llc_ops->signoff(hb, TRUE) != HA_OK) {
                cl_log(LOG_ERR, "Cannot sign off from heartbeat.");
                cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
                return HA_FAIL;
        }
        
        if (hb->llc_ops->delete(hb) != HA_OK) {
                cl_log(LOG_ERR, "Cannot delete API object.");
                cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
                return HA_FAIL;
        }

        hb = NULL;
        return HA_OK;
}

int
get_heartbeat_fd(void)
{
        int fd;

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

                cl_log(LOG_INFO, "node %ld: %s, type: %s, status: %s", 
                        (unsigned long)id, name, type, status); 

                node = (struct hb_nodeinfo *) 
                                ha_malloc(sizeof(struct hb_nodeinfo));

                if (!node) {
                        cl_log(LOG_CRIT, "malloc failed for node info.");
                        return HA_FAIL;
                }

                node->name =  g_strdup(name);
                node->ifcount = 0;
                node->id = id;

                memset(&uuid, 0, sizeof(cl_uuid_t));

                /* the get_uuid_by_name is not available for STABLE_1_2 branch. */
                if (hb->llc_ops->get_uuid_by_name(hb, name, &uuid) == HA_FAIL) {
                        cl_log(LOG_DEBUG, "Cannot get the uuid for node: %s", name);
                }
                
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

                        interface = (struct hb_ifinfo *) 
                                ha_malloc(sizeof(struct hb_ifinfo));
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
                &&                STRNCMP_CONST(type, T_SHUTDONE) == 0) {
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
                cl_log(LOG_INFO, "adding %s in membership table", 
                                        nbuf[i].clusterNode.nodeName.value);
                g_ptr_array_add(gMembershipTable, (gpointer *) &nbuf[i]);
        }

        if (clm_initialized) {
                for (i = 0; i < nitem; i++) {
                        status = nbuf[i].clusterChanges;
                        node = (char *)nbuf[i].clusterNode.nodeName.value;

                        if (status == SA_CLM_NODE_NO_CHANGE) {
                                        continue;
                        }

                        /* user hook handler */
                        if ( membership_event_hook ){
                                membership_event_hook (node, status);
                        }
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
                cl_log(LOG_DEBUG, "Membership service currently not available."
                                  "Will try again later. errno [%d]",ret);
                return HA_OK;
        }

        if (nbuf) {
                ha_free(nbuf);
        }

        nbuf = (SaClmClusterNotificationT *) 
                        ha_malloc(gNodeTable->len * 
                                  sizeof (SaClmClusterNotificationT));
        if (!nbuf) {
                cl_log(LOG_ERR, 
                        "%s: ha_malloc failed for SaClmClusterNotificationT.", 
                        __FUNCTION__);
                return HA_FAIL;
        }

        if (saClmClusterTrackStart(&handle, SA_TRACK_CURRENT, nbuf,
                gNodeTable->len) != SA_OK) {
                cl_log(LOG_ERR, 
                               "SA_TRACK_CURRENT error, errno [%d]", ret);
                ha_free(nbuf);
                return HA_FAIL;
        }

        /* Start to track cluster membership changes events */
        if (saClmClusterTrackStart(&handle, SA_TRACK_CHANGES, nbuf,
                        gNodeTable->len) != SA_OK) {
                cl_log(LOG_ERR, 
                        "SA_TRACK_CURRENT error, errno [%d]", ret);
                ha_free(nbuf);
                return HA_FAIL;
        }

        clm = handle;

        clm_initialized = 1;
        cl_log(LOG_INFO, "Membership service initialized successfully.");

        return HA_OK;
}

int
get_membership_fd(void)
{
                SaErrorT ret;
        SaSelectionObjectT st;

        if (!clm_initialized)
                return 0;

        if ((ret = saClmSelectionObjectGet(&clm, &st)) != SA_OK) {
                cl_log(LOG_ERR, 
                       "saClmSelectionObjectGet error, errno [%d]", ret);
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
                        clm_initialized = 0;
                        free_membershiptable();

                        return HA_OK;
                } else {
                        cl_log(LOG_WARNING, 
                               "saClmDispatch error, ret = [%d]", ret);
                }
        }

        return HA_OK;
}

int
ping_membership(int * mem_fd)
{
        int fd;

        if (clm_initialized) 
                return 0;

        init_membership();
        fd = get_membership_fd();

        *mem_fd = fd;

        return HA_OK;
}

int
init_resource_table(void)
{

#if 0
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

                resource = (struct hb_rsinfo *) ha_malloc(sizeof(struct hb_rsinfo));
                if (!resource) {
                        cl_log(LOG_CRIT, "malloc resource info failed.");
                        return HA_FAIL;
                }

                resource->id = ++count;
                resource->masternodeid = nodeid;
                resource->resource = g_strdup(pad);

                g_ptr_array_add(gResourceTable, (gpointer *) resource);
        }
#endif
        return HA_OK;
}

int 
rsinfo_get_int_value(lha_attribute_t attr, size_t index, uint32_t * value)
{

#if 0
        uint32_t rc = 0;
        char getcmd[MAXLINE];
        char * resource;

        *value = 0;

        if (!gResourceTable)
                return HA_FAIL;

        switch (attr) {
        case RESOURCE_STATUS:
                resource = ((struct hb_rsinfo *) 
                        g_ptr_array_index(gResourceTable, index))->resource;
                sprintf(getcmd, HALIB "/ResourceManager status %s", resource);
                rc = system(getcmd);
                cl_log(LOG_INFO, "resource [%s] status: [%d]", 
                                  resource, WEXITSTATUS(rc));

                *value = WEXITSTATUS(rc);

                break;
        default: 
                return HA_FAIL;
        }

#endif

        return HA_OK;
}


int ha_set_event_hooks(node_event_hook_t node_hook, if_event_hook_t if_hook, 
                       membership_event_hook_t membership_hook)
{

        node_event_hook = node_hook;
        if_event_hook = if_hook;
        membership_event_hook = membership_hook;

        return HA_OK;
}

int ha_unset_event_hooks ()
{

        node_event_hook = NULL;
        if_event_hook = NULL;
        membership_event_hook = NULL;

        return HA_OK;
}


static int logger_initialized = 0;

int init_logger(const char * entity)
{
        char * inherit_debuglevel;
        int debug_level;
 
        if ( logger_initialized ){
                return HA_OK;
        }

	inherit_debuglevel = getenv(HADEBUGVAL);
	if (inherit_debuglevel != NULL) {
		debug_level = atoi(inherit_debuglevel);
		if (debug_level > 2) {
			debug_level = 2;
		}
	}

	cl_log_set_entity(entity);
	cl_log_enable_stderr(debug_level?TRUE:FALSE);
	cl_log_set_facility(LOG_DAEMON);
        return HA_OK;
}



#define DEBUG_ENTER() cl_log(LOG_INFO, "%s: --- ENTER ---", __FUNCTION__)
#define DEBUG_LEAVE() cl_log(LOG_INFO, "%s: --- LEAVE ---", __FUNCTION__)

int
get_hb_initialized () 
{
        return hb_initialized;
}


char *
get_hb_client_id ()
{
        return hb_client_id;
}       

int 
linuxha_initialize(const char * client_id, int force)
{

        int ret = 0;

        cl_log(LOG_INFO, "%s: PID = %ld", 
                __FUNCTION__, (long)getpid());

        /* casual client not allowed */
        if ( client_id == NULL ) {
                ret = HA_FAIL;
                goto out;
        }

        if ( hb_client_id ){
                if ( force ){
                        cl_log(LOG_WARNING, 
                                "%s: re-initialize heartbeat", __FUNCTION__);

                        linuxha_finalize();

                } else {
                        cl_log(LOG_WARNING, 
                                "%s: heartbeat has been initialized",
                                __FUNCTION__);
                        ret = HA_OK;
                        goto out;
                }
        }

        if ( client_id ) {
                hb_client_id = strdup(client_id);

                if ( hb_client_id == NULL )  {
                        ret = HA_FAIL;
                        goto out;
                }
        } else {
                hb_client_id = NULL;
        }

        if (init_storage() != HA_OK) {
                cl_log(LOG_ERR, 
                        "%s: failed to init storage", __FUNCTION__);

                ret = HA_FAIL;
                
                free(hb_client_id);
                hb_client_id = NULL;

                goto out;
        }

        if (init_heartbeat() != HA_OK ||
                        get_heartbeat_fd() <= 0) {

                cl_log(LOG_ERR, 
                        "%s: failed to init heartbeat", __FUNCTION__); 

                free_storage();
                free(hb_client_id);
                hb_client_id = NULL;

                ret = HA_FAIL;
                goto out;
        }

        hb_initialized = 1;
        ret = HA_OK;
out:
        return ret;

}


int linuxha_finalize () {

        cl_log(LOG_INFO, "%s: PID = %ld", 
                __FUNCTION__, (long)getpid());

        free_heartbeat();
        free_storage();

        cl_log(LOG_INFO, 
                "%s: free hb_client_id", __FUNCTION__);

        if ( hb_client_id ) {
                free(hb_client_id);
                hb_client_id = NULL;
        }

        hb_initialized = 0;
        return HA_OK;
}

