#include <portability.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hbagent.h"

#include "hb_api.h"
#include "heartbeat.h"
#include "clplumbing/cl_log.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "LHAClusterInfo.h"
#include "LHANodeTable.h"
#include "LHAIFTable.h"
#include "LHAMembershipTable.h"
#include "LHAResourceGroupTable.h"
#include "LHAHeartbeatConfigInfo.h"

#include <signal.h>

#include <sys/types.h> /* getpid() */
#include <unistd.h>

#include <errno.h>

#include "saf/ais.h"

#define DEFAULT_TIME_OUT 5 // default timeout value for snmp in sec.
#define LHAAGENTID "lha-snmpagent"

static unsigned long hbInitialized = 0;
static unsigned long clmInitialized = 0;
static ll_cluster_t * hb = NULL; // heartbeat handle
static const char * myid = NULL; // my node id
static SaClmHandleT clm = 0;

static GArray * gNodeTable = NULL;
static GArray * gIFTable = NULL;
static GArray * gMembershipTable = NULL;
static GArray * gResourceTable = NULL;

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

int walk_node_table(void);
int walk_if_table(void);

static RETSIGTYPE
stop_server(int a) {
    keep_running = 0;
}

int 
get_int_value(lha_group_t group, lha_attribute_t attr, size_t index, int * value)
{
	switch (group) {
		case LHA_CLUSTERINFO: 
			return clusterinfo_get_int_value(attr, index, value);

		case LHA_RESOURCEINFO:
			return rsinfo_get_int_value(attr, index, value);

		case LHA_NODEINFO:

		case LHA_IFINFO:


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

		case LHA_IFINFO:

		case LHA_RESOURCEINFO:

		default:
			return HA_FAIL;
	}
}

int
clusterinfo_get_int_value(lha_attribute_t attr, size_t index, int32_t * value)
{
    int i;
    const char * status;
    size_t count;

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
		status = g_array_index(gNodeTable, struct hb_nodeinfo, i).status;
		if (strcmp(status, DEADSTATUS) != 0) {
		    count++;
		}
	    }

	    *value = count;
	    break;

	default:
	    return HA_FAIL;
    }

    return HA_OK;
}

int
hbconfig_get_str_value(const char * attr, char * * value)
{
    char * ret;
    static char err[] = "N/A";

    *value  = err;

    if ((ret = hb->llc_ops->get_parameter(hb, attr)) == NULL) {
	cl_log(LOG_ERR, "getting parameter [%s] error.", attr);
	cl_log(LOG_INFO, "reason: %s.", hb->llc_ops->errmsg(hb));

	/* we have to return HA_OK here otherwise the 
	   agent code would not progress */
	return HA_OK;
    };

    *value = ret;

    return HA_OK;
}

GArray *
get_hb_info(lha_group_t group)
{
    switch (group) {
	case LHA_NODEINFO:
	    return gNodeTable;

	case LHA_IFINFO:
	    return gIFTable;

	case LHA_RESOURCEINFO:
	    return gResourceTable;

	case LHA_MEMBERSHIPINFO:
	    return gMembershipTable;

	default:
	    return NULL;
    }
}

int
main(int argc, char ** argv) 
{
	int ret;

	fd_set fdset;
	struct timeval tv, *tvp;
	int block, numfds, hb_fd = 0, mem_fd = 0;

	/* change this if you want to be a SNMP master agent */
	int agentx_subagent=1; 

	/* change this if you want to run in the background */
	int background = 0; 

	/* change this if you want to use syslog */
	int syslog = 0; 

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
	init_agent("LHA-agent");

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

	if ((ret = init_membership() != HA_OK) ||
		(mem_fd = get_membership_fd()) <= 0) {
	    	cl_log(LOG_ERR, "membership initialization failure.  You will not be able to view any membership information in this cluster.");
	} 

	init_LHAClusterInfo();
	init_LHANodeTable();
	init_LHAIFTable();
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

	/* you're main loop here... */
	while(keep_running) {
	/* if you use select(), see snmp_select_info() in snmp_api(3) */
	/*     --- OR ---  */
		// agent_check_and_process(1); /* 0 == don't block */

		FD_ZERO(&fdset);
                FD_SET(hb_fd, &fdset);

		if (clmInitialized) {
			FD_SET(mem_fd, &fdset);
		}

		tv.tv_sec = DEFAULT_TIME_OUT;
		tv.tv_usec = 0;
		tvp = &tv;

		numfds = hb_fd+1;

		snmp_select_info(&numfds, &fdset, tvp, &block);
		if (block) {
			tvp = NULL;
		}

		ret = select(numfds, &fdset, 0, 0, tvp);

		if (ret < 0) {
			/* error */
			cl_log(LOG_ERR, "select() returned with an error.");
			break;
		} else if (ret == 0) {
			/* timeout */
			snmp_timeout();
			continue;
		} 

		if (FD_ISSET(hb_fd, &fdset)) {
			/* heartbeat */

			if ((ret = handle_heartbeat_msg()) == HA_FAIL) {
				cl_log(LOG_ERR, "heartbeat stopped.");
				break;
			}
		} else if (clmInitialized && FD_ISSET(mem_fd, &fdset)) {
		    	/* membership events */

		    	if ((ret = handle_membership_msg()) == HA_FAIL) {
			    	cl_log(LOG_ERR, "memebership error.");
				break;
			}
		} else {
			/* snmp request */
			snmp_read(&fdset);
		}
	}

	/* at shutdown time */
	snmp_shutdown("example-demon");
	free_storage();

	return 0;
}

/* functions which talk to heartbeat */

static void
NodeStatus(const char * node, const char * status, void * private)
{
        cl_log(LOG_NOTICE, "Status update: Node %s now has status %s\n"
        ,       node, status);
	walk_node_table();
}

static void
LinkStatus(const char * node, const char * lnk, const char * status
,       void * private)
{
        cl_log(LOG_NOTICE, "Link Status update: Link %s/%s now has status %s\n"
        ,       node, lnk, status);
	walk_if_table();
}

int
init_storage(void)
{
	gNodeTable = g_array_new(TRUE, TRUE, sizeof (struct hb_nodeinfo));
	gIFTable = g_array_new(TRUE, TRUE, sizeof (struct hb_ifinfo));
	gResourceTable = g_array_new(TRUE, TRUE, sizeof (struct hb_rsinfo));
	gMembershipTable = g_array_new(TRUE, TRUE, 
			sizeof (SaClmClusterNotificationT));

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

	    	node = &g_array_index(gNodeTable, struct hb_nodeinfo, 0);

		free(node->name);
		free(node->type);
		free(node->status);

		gNodeTable = g_array_remove_index_fast(gNodeTable, 0);
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
		interface = &g_array_index(gIFTable, struct hb_ifinfo, 0);

		free(interface->name);
		free(interface->node);
		free(interface->status);

		gIFTable = g_array_remove_index_fast(gIFTable, 0);
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

		resource = & g_array_index(gResourceTable, 
			struct hb_rsinfo, 0);

		free(resource->master);
		free(resource->resource);

		gResourceTable = g_array_remove_index_fast(gResourceTable, 0);
	}

	return;
}

static void
free_membershiptable(void)
{
	SaClmClusterNotificationT * buf;

    	if (!gMembershipTable)
	    return;

	while (gMembershipTable->len) {

	    	buf = &g_array_index(gMembershipTable, 
			SaClmClusterNotificationT, 0);
	    	free(buf);

		gMembershipTable = 
		    g_array_remove_index_fast(gMembershipTable, 0);
	}
	
	return;
}

void
free_storage(void)
{
    	free_nodetable();
	g_array_free(gNodeTable, 1);
	gNodeTable = NULL;

	free_iftable();
	g_array_free(gIFTable, 1);
	gIFTable = NULL;

	free_resourcetable();
	g_array_free(gResourceTable, 1);
	gResourceTable = NULL;

	free_membershiptable();
	g_array_free(gMembershipTable, 1);
	gResourceTable = NULL;

}

int
init_heartbeat(void)
{
	hb = NULL;

	cl_log_set_entity("lha-snmpagent");
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(LOG_USER);

	hb = ll_cluster_new("heartbeat");

	cl_log(LOG_DEBUG, "PID=%ld\n", (long)getpid());
	cl_log(LOG_DEBUG, "Signing in with heartbeat\n");

	if (hb->llc_ops->signon(hb, LHAAGENTID)!= HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}

	if (NULL == (myid = hb->llc_ops->get_mynodeid(hb))) {
		cl_log(LOG_ERR, "Cannot get mynodeid\n");
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

#if 1
	// walk the node table
	if ( HA_OK != walk_node_table() || HA_OK != walk_if_table() ) {
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
		cl_log(LOG_ERR, "Cannot get inputfd\n");
		cl_log(LOG_ERR, "REASON, %s\n", hb->llc_ops->errmsg(hb));
	}
	return fd;
}

int
walk_node_table(void)
{
	const char *name, *type, *status;
	struct hb_nodeinfo node;

	if (gNodeTable) {
		free_nodetable();
	}

	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		return HA_FAIL;
	}
	while((name = hb->llc_ops->nextnode(hb))!= NULL) {
		status = hb->llc_ops->node_status(hb, name);

		cl_log(LOG_DEBUG, "Cluster node: %s: status: %s\n", name 
		,	status);

		type = hb->llc_ops->node_type(hb, name);

		cl_log(LOG_DEBUG, "Cluster node: %s: type: %s\n", name 
		,	type);

		node.name =  g_strdup(name);
		node.type =  g_strdup(type);
		node.status = g_strdup(status);
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
	const char *name, * status;
	struct hb_nodeinfo * node;
	struct hb_ifinfo interface;
	int i, ifcount;

	if (gIFTable) {
		free_iftable();
	}

	for (i = 0; i < gNodeTable->len; i++) {
		node = &g_array_index(gNodeTable, struct hb_nodeinfo, i);
		ifcount = 0;

		if (hb->llc_ops->init_ifwalk(hb, node->name) != HA_OK) {
			cl_log(LOG_ERR, "Cannot start if walk\n");
			cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
			return HA_FAIL;
		}

		while((name = hb->llc_ops->nextif(hb))!=NULL) {
			status = hb->llc_ops->if_status(hb, node->name, name);

			cl_log(LOG_DEBUG, "node interface: %s: status: %s\n",
					name,	status);

			interface.name = g_strdup(name);
			interface.node = g_strdup(node->name);
			interface.status = g_strdup(status);
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

int
handle_heartbeat_msg(void)
{
	struct ha_msg *msg;
	const char *type, *node;

	fprintf(stderr, "handling heartbeat msgs... \n");

	while (hb->llc_ops->msgready(hb)) {

		msg = hb->llc_ops->readmsg(hb, 0);
		if (!msg)
		    	break;

		type = ha_msg_value(msg, F_TYPE);
		node = ha_msg_value(msg, F_ORIG);
		fprintf(stderr, "node = %s, type = %s, \n", node, type);
		if (!type || !node) {
			// can't read type. log and ignore the msg.
			cl_log(LOG_DEBUG, "Can't read msg type.\n");
			return HA_OK;
		}

		// we only handle the shutdown msg for now.
		if (strcmp(myid, node) == 0 && strncmp(type, T_SHUTDONE, 20) == 0) {
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

        free_membershiptable();

        for (i = 0; i < nitem; i++) {
	    	cl_log(LOG_INFO, "adding %s in membership table", nbuf[i].clusterNode.nodeName.value);
                g_array_append_val(gMembershipTable, nbuf[i]);
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
	static SaClmClusterNotificationT * nbuf;
	SaClmHandleT handle;

	SaClmCallbacksT my_callbacks = {
	    .saClmClusterTrackCallback 
		= (SaClmClusterTrackCallbackT) clm_track_cb,
	    .saClmClusterNodeGetCallback 
		= (SaClmClusterNodeGetCallbackT) clm_node_cb,
	};

	if ((ret = saClmInitialize(&handle, &my_callbacks, NULL)) != SA_OK) {
	    cl_log(LOG_ERR, "saClmInitialize error, errno [%d]\n",ret);
	    return HA_FAIL;
	}

        nbuf = (SaClmClusterNotificationT *) ha_malloc(gNodeTable->len *
                                sizeof (SaClmClusterNotificationT));

        if (saClmClusterTrackStart(&handle, SA_TRACK_CURRENT, nbuf,
                gNodeTable->len) != SA_OK) {
                cl_log(LOG_ERR, "SA_TRACK_CURRENT error, errno [%d]\n", ret);
                ha_free(nbuf);
                return HA_FAIL;
        }

        /* Start to track cluster membership changes events */
        if (saClmClusterTrackStart(&handle, SA_TRACK_CHANGES, nbuf,
                gNodeTable->len) != SA_OK) {
                cl_log(LOG_ERR, "SA_TRACK_CURRENT error, errno [%d]\n", ret);
                ha_free(nbuf);
                return HA_FAIL;
        }

	clm = handle;

	clmInitialized = 1;

	return HA_OK;
}

int
get_membership_fd(void)
{
    	SaErrorT ret;
	SaSelectionObjectT st;

	if ((ret = saClmSelectionObjectGet(&clm, &st)) != SA_OK) {
	    	cl_log(LOG_ERR, "saClmSelectionObjectGet error, errno [%d]\n", ret);
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
		    	cl_log(LOG_ERR, "I am evicted.");
			return HA_FAIL;
		} else {
		    cl_log(LOG_WARNING, "saClmDispatch error, ret = [%d]", ret);
		}
	}

    	return HA_OK;
}

int
init_resource_table(void)
{
    	int rc, i, mcount, found;
	FILE * rcsf;
	char buf[MAXLINE];
	char host[MAXLINE], pad[MAXLINE];
	struct hb_rsinfo resource;
	char * node;

	if (gResourceTable) {
	    	free_resourcetable();
	}

	if ((rcsf = fopen(RESOURCE_CFG, "r")) == NULL) {
	    	cl_log(LOG_ERR, "Cannot open file %s", RESOURCE_CFG);
		return HA_FAIL;
	}

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

		resource.master = g_strdup(host);
		resource.resource = g_strdup(pad);

		/* make sure that the master node is in the node list */
		found = 0;
		for (i = 0; i < gNodeTable->len; i++) {
		    	node = g_array_index(gNodeTable, 
				struct hb_nodeinfo, i).name;
			if (strcmp(node, host) == 0) {
			    	found = 1;
			    	break;
			}
		}
		if (!found)
		    continue;

		mcount = 0;
		for (i = 0; i < gResourceTable->len; i++) {
		    	node = g_array_index(gResourceTable, 
				struct hb_rsinfo, i).master;

			if (strcmp(node, host) == 0) {
			    mcount++;
			}
		}

		resource.index = mcount + 1;

		g_array_append_val(gResourceTable, resource);
	}

	return HA_OK;
}

int 
rsinfo_get_int_value(lha_attribute_t attr, size_t index, int32_t * value)
{
    int rc = 0;
    char getcmd[MAXLINE];
    char * resource;

    *value = 0;

    if (!gResourceTable)
	return HA_FAIL;

    switch (attr) {
	case RESOURCE_STATUS:
	    resource = g_array_index(gResourceTable, struct hb_rsinfo, index).resource;
	    sprintf(getcmd, HALIB "/ResourceManager status %s", resource);
	    rc = system(getcmd);
	    cl_log(LOG_INFO, "resource [%s] status: [%d]", resource, WEXITSTATUS(rc));

	    *value = WEXITSTATUS(rc);
	    break;

	default: 
	    return HA_FAIL;
    }

    return HA_OK;
}





