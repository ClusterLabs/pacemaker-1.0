/* $Id: haclient.c,v 1.5 2004/02/17 22:12:01 lars Exp $ */
#include "haclient.h"

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>

static unsigned long hbInitialized = 0;
static ll_cluster_t * hb = NULL;
static GArray * gNodeTable;
static GArray * gIFTable;

struct hb_node_t {
	char * name;
	// char * type;
	// char * status;
	unsigned long ifcount;
};

struct hb_if_t {
	const char * name;
	const char * node;
	int32_t nodeid;
	int32_t id;
};


void NodeStatus(const char * node, const char * status, void * private);
void LinkStatus(const char * node, const char * lnk, const char * status 
,       void * private);

int init_hb_tables(void);
int walk_node_table(void);
int walk_if_table(void);
int clusterinfo_get_int32_value(unsigned index, ha_attribute_t attrib, int32_t * value);
int nodeinfo_get_int32_value(size_t index, ha_attribute_t attrib, int32_t * value);
int nodeinfo_get_str_value(size_t index, ha_attribute_t attrib, const char * * value);
int ifinfo_get_int32_value(size_t index, ha_attribute_t attrib, int32_t * value);
int ifinfo_get_str_value(size_t index, ha_attribute_t attrib, const char * * value);

void
NodeStatus(const char * node, const char * status, void * private)
{
        cl_log(LOG_NOTICE, "Status update: Node %s now has status %s\n"
        ,       node, status);
}

void
LinkStatus(const char * node, const char * lnk, const char * status
,       void * private)
{
        cl_log(LOG_NOTICE, "Link Status update: Link %s/%s now has status %s\n"
        ,       node, lnk, status);
}

int
init_heartbeat(void)
{
	hb = NULL;

	cl_log_set_entity("hasubagent");
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(LOG_USER);

	hb = ll_cluster_new("heartbeat");

	cl_log(LOG_DEBUG, "PID=%ld\n", (long)getpid());
	cl_log(LOG_DEBUG, "Signing in with heartbeat\n");

	if (hb->llc_ops->signon(hb, NULL)!= HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

	if (hb->llc_ops->set_nstatus_callback(hb, NodeStatus, NULL) !=HA_OK){
	        cl_log(LOG_ERR, "Cannot set node status callback\n");
	        cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

	if (hb->llc_ops->set_ifstatus_callback(hb, LinkStatus, NULL)!=HA_OK){
	        cl_log(LOG_ERR, "Cannot set if status callback\n");
	        cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

	// walk the node table
	if ( HA_OK != walk_node_table() || HA_OK != walk_if_table() ) {
		return HA_FAIL;
	}

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
		cl_log(LOG_ERR, "Cannot get inputfd\n");
		cl_log(LOG_ERR, "REASON, %s\n", hb->llc_ops->errmsg(hb));
	}
	return fd;
}

int
handle_heartbeat_msg(void)
{
	struct ha_msg *msg;
	const char * type;

	if (hb->llc_ops->msgready(hb)) {
		msg = hb->llc_ops->readmsg(hb, 0);
		if (msg) {
			type = ha_msg_value(msg, F_TYPE);
			if (!type) {
				// can't read type. log and ignore the msg.
				cl_log(LOG_DEBUG, "Can't read msg type.\n");
				return 0;
			}

			// we only handle the shutdown msg for now.
			if (strncmp(type, T_SHUTDONE, 20) == 0) {
				return -1;
			}
		}
	}
	return 0;
}

int
walk_node_table(void)
{
	const char *nname, *ntype;
	struct hb_node_t node;

	gNodeTable = g_array_new(TRUE, TRUE, sizeof (struct hb_node_t));

	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}
	while((nname = hb->llc_ops->nextnode(hb))!= NULL) {
		ntype = hb->llc_ops->node_type(hb, nname);

		cl_log(LOG_DEBUG, "Cluster node: %s: type: %s\n", nname 
		,	ntype);

		node.name =  g_strdup(nname);
		// node.type =  g_strdup(ntype);
		g_array_append_val(gNodeTable, node); 
	}
	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}
	return HA_OK;
}

int
walk_if_table(void)
{
	const char *ifname;
	struct hb_node_t * node;
	struct hb_if_t interface;
	int i, ifcount;

	gIFTable = g_array_new(TRUE, TRUE, sizeof (struct hb_if_t));

	for (i = 0; i < gNodeTable->len; i++) {
		node = &g_array_index(gNodeTable, struct hb_node_t, i);
		ifcount = 0;

		if (hb->llc_ops->init_ifwalk(hb, node->name) != HA_OK) {
			cl_log(LOG_ERR, "Cannot start if walk\n");
			cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
			return HA_FAIL;
		}

		while((ifname = hb->llc_ops->nextif(hb))!=NULL) {
			/*
			ifstatus = hb->llc_ops->if_status(hb, node->name, ifname);

			cl_log(LOG_DEBUG, "node interface: %s: status: %s\n", ifname 
			,	ifstatus);
			*/

			interface.name = g_strdup(ifname);
			interface.node = g_strdup(node->name);
			interface.nodeid= i;
			interface.id = ifcount++;
			g_array_append_val(gIFTable, interface);
		}

		if (hb->llc_ops->end_ifwalk(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot end if walk.\n");
			cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
			return HA_FAIL;
		}

		/* assign ifcount to the node */
		node->ifcount = ifcount;
	}
	return HA_OK;
}

/* 
 * functions specific for snmp request 
 */

int
get_count(ha_group_t group, size_t * count)
{
	*count = 0;

	switch (group) {
		case NODEINFO:
			*count = gNodeTable->len;
			break;

		case IFINFO:
			*count = gIFTable->len;
			break;

		case RESOURCEINFO:
			break;

		default:
			return HA_FAIL;
	}
	return HA_OK;
}

int 
get_int32_value(ha_group_t group, ha_attribute_t attrib, size_t index, int32_t * value)
{
	switch (group) {
		case CLUSTERINFO: 
			return clusterinfo_get_int32_value(index, attrib, value);

		case NODEINFO:
			return nodeinfo_get_int32_value(index, attrib, value);

		case IFINFO:
			return ifinfo_get_int32_value(index, attrib, value);

		case RESOURCEINFO:
			return HA_FAIL; // todo

		default:
			return HA_FAIL;
	}
}

int
get_str_value(ha_group_t group, ha_attribute_t attrib, size_t index, const char * * value)
{
	switch (group) {
		case NODEINFO:
			return nodeinfo_get_str_value(index, attrib, value);

		case IFINFO:
			return ifinfo_get_str_value(index, attrib, value);

		case RESOURCEINFO:
			return HA_FAIL; // todo

		default:
			return HA_FAIL;
	}
}

int
clusterinfo_get_int32_value(size_t index, ha_attribute_t attrib, int32_t * value)
{
	*value = 0;

	switch (attrib) {
		case NODE_COUNT:
			*value = gNodeTable->len;
			break;

		default:
			return HA_FAIL;
	}
	return HA_OK;
}

int 
nodeinfo_get_int32_value(size_t index, ha_attribute_t attrib, int32_t * value)
{
	*value = 0;

	if (index > gNodeTable->len) 
		return HA_FAIL;

	switch (attrib) {
		case NODE_IF_COUNT:
			*value = (g_array_index(gNodeTable, struct hb_node_t, index)).ifcount;
			break;
		default:
			return HA_FAIL;
	}
	return HA_OK;
}

int
nodeinfo_get_str_value(size_t index, ha_attribute_t attrib, const char * * value)
{
	const char * node;

	if (!value)
		return HA_FAIL;

	*value = NULL;
	if (index > gNodeTable->len) 
		return HA_FAIL;

	node = (g_array_index(gNodeTable, struct hb_node_t, index)).name;

	switch (attrib) {
		case NODE_NAME:
			*value = node;
			break;

		case NODE_TYPE:
			if ((*value = hb->llc_ops->node_type(hb, node)) == NULL) {
				cl_log(LOG_ERR, "Failed to get node type.\n");
				cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
				return HA_FAIL;
			}
			break;

		case NODE_STATUS:
			if ((*value = hb->llc_ops->node_status(hb, node)) == NULL) {
				cl_log(LOG_ERR, "Failed to get node status.\n");
				cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
				return HA_FAIL;
			}
			break;

		default:
			return HA_FAIL;
	}

	return HA_OK;
}

int
ifinfo_get_int32_value(size_t index, ha_attribute_t attrib, int32_t * value)
{
	*value = 0;

	if (index > gIFTable->len) 
		return HA_FAIL;

	switch (attrib) {
		case IF_NODE_ID:
			*value = (g_array_index(gIFTable, struct hb_if_t, index)).nodeid;
			break;

		case IF_ID:
			*value = (g_array_index(gIFTable, struct hb_if_t, index)).id;
			break;


		default:
			return HA_FAIL;
	}
	return HA_OK;
}

int
ifinfo_get_str_value(size_t index, ha_attribute_t attrib, const char * * value)
{
	const char * node, * ifname;

	if (!value)
		return HA_FAIL;

	*value = NULL;
	if (index > gNodeTable->len) 
		return HA_FAIL;

	node = (g_array_index(gIFTable, struct hb_if_t, index)).node;
	ifname = (g_array_index(gIFTable, struct hb_if_t, index)).name;

	switch (attrib) {
		case IF_NAME:
			*value = ifname;
			break;

		case IF_STATUS:
			if ((*value = hb->llc_ops->if_status(hb, node, ifname)) == NULL) {
				cl_log(LOG_ERR, "Failed to get if status.\n");
				cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
				return HA_FAIL;
			}
			break;

		default:
			return HA_FAIL;
	}

	return HA_OK;
}

