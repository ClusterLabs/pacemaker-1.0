/*
 * cms_membership.c: cms daemon membership event handlers
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 * Author: Zhu Yi (yi.zhu@intel.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <clplumbing/cl_log.h>
#include <heartbeat.h>

#include "cms_data.h"
#include "cms_common.h"
#include "cms_membership.h"

GList *mqmember_list = NULL;
GList *clm_member_list = NULL;
static cms_data_t * gcms_data = NULL;

#define CS_TIMEOUT 1000 /* 1 second */


static gint
comp_clm_member(gconstpointer data, gconstpointer user_data)
{
	const SaClmClusterNotificationT * member = data;
	const char * node = user_data;

	if (member->clusterNode.nodeName.length != strlen(node))
		return 1;

	return strncmp(member->clusterNode.nodeName.value, node, strlen(node));
}

static gint
comp_mqmember(gconstpointer data, gconstpointer user_data)
{
	const char * member = data;
	const char * node = user_data;

	return strcmp(member, node);	
}

static void
dump_cluster_member(gpointer data, gpointer user_data)
{
	SaClmClusterNotificationT * member = data;

	cl_log(LOG_INFO, "%s: nodeid = %ld nodename = %s", __FUNCTION__, 
		member->clusterNode.nodeId, member->clusterNode.nodeName.value);
}

static void
dump_mq_member(gpointer data, gpointer user_data)
{
	cl_log(LOG_INFO, "%s: nodename = %s", __FUNCTION__, (char *) data);
}

static void
mqname_dump_membership(void)
{
	g_list_foreach(clm_member_list, dump_cluster_member, NULL);
	g_list_foreach(mqmember_list, dump_mq_member, NULL);
}

int
is_cms_online(const char * node)
{
	ll_cluster_t * hb = gcms_data->hb_handle;
	const char * status;

	assert(hb);

	status = hb->llc_ops->client_status(hb, node, CMSID, CS_TIMEOUT);
	if (!status) {
		cl_log(LOG_ERR, "llc_ops->client_status error for node %s"
				"Reason: %s", node, hb->llc_ops->errmsg(hb));
		return 0;
	}
	dprintf("client status on %s = %s\n", node, status);

	if (strcmp(status, ONLINESTATUS) == 0) {
		return 1;
	}

	return 0;
}

int 
set_cms_status(const char * node, const char * status, void * private)
{
	GList * element;
	char * host = ha_strdup(node);

	dprintf("set_cms_status: node = %s, status = %s\n", node, status);

	if (!g_list_find_custom(clm_member_list, host, comp_clm_member)){
		return HA_OK;
	}

	/* is_cms_online(node); */

	element = g_list_find_custom(mqmember_list, host, comp_mqmember);
	if (element) {
		if (strcmp(status, LEAVESTATUS) == 0 ||
		    strcmp(status, OFFLINESTATUS) == 0) {
			/*
			 * cms server on that node is off line, 
			 * take it off from the mqmember list
			 */
			mqmember_list = g_list_remove_link(mqmember_list
			,	element);

			/*
			 * close all mqueues belongs to this node
			 */
			mqueue_close_node(element->data);

			ha_free(element->data);
			g_list_free_1(element);

			/*
			 * If I am now the only one in the list and 
			 * I am still waiting for the update, then set
			 * myself to ready.
			 */
			if (g_list_length(mqmember_list) == 1) {
				gcms_data->cms_ready = 1;
			}
		}
	} else {
		if (strcmp(status, JOINSTATUS) == 0 ||
		    strcmp(status, ONLINESTATUS) == 0) {

			mqmember_list = g_list_insert_sorted(mqmember_list
			,	host, comp_mqmember);

		}
	}

	mqname_dump_membership();
	return HA_OK;
}

static void
free_mqmember(gpointer data, gpointer user_data)
{
	char * node = data;

	ha_free(node);
}

static void
free_clm_member(gpointer data, gpointer user_data)
{
	SaClmClusterNotificationT * nbuf = data;

	ha_free(nbuf);
}

static int
free_member_lists(void)
{
	if (g_list_length(mqmember_list)) {
		g_list_foreach(mqmember_list, free_mqmember, NULL);
		g_list_free(mqmember_list);
		mqmember_list = NULL;
	}

	if (g_list_length(clm_member_list)) {
		g_list_foreach(clm_member_list, free_clm_member, NULL);
		g_list_free(clm_member_list);
		clm_member_list = NULL;
	}

	return HA_OK;
}

static void
dump_nodeinfo(SaClmClusterNodeT *cn)
{
	dprintf("Dump information from SaClmClusterNodeGet\n");
	dprintf("\n");
	dprintf("nodeId = %ld\n", cn->nodeId);
	dprintf("nodeAddress = %s\n"
	,	cn->nodeAddress.length > 0
	?	(char *)cn->nodeAddress.value : "N/A");
	dprintf("nodeName = %s\n"
	,	cn->nodeName.length > 0
	?	(char *)cn->nodeName.value : "N/A");
	dprintf("clusterName = %s\n"
	,	cn->clusterName.length > 0
	?	(char *)cn->clusterName.value : "N/A");
	dprintf("member = %d\n", cn->member);
	dprintf("bootTimestamp = %lld\n", cn->bootTimestamp);
	dprintf("\n");
}

static void
getnode_callback(SaInvocationT invocation, SaClmClusterNodeT *clusterNode
,	SaErrorT error)
{
	if (error != SA_OK) {
		cl_log(LOG_ERR, "Get Node Callback failed [%d]", error);
		/* exit(1); */
	}
	dprintf("Invocation [%d]\n", invocation);
	dump_nodeinfo(clusterNode);
}

static void
mqclm_track(SaClmClusterNotificationT *nbuf, SaUint32T nitem
,	SaUint32T nmem, SaUint64T nview, SaErrorT error)
{
	int i;
	SaClmClusterNotificationT *buffer;
	char * node;

	free_member_lists();

	for (i = 0; i < nitem; i++) {
		if (nbuf[i].clusterChanges == SA_CLM_NODE_LEFT) {
			if (!is_cms_online(nbuf[i].clusterNode.nodeName.value))
				continue;

			mqueue_close_node(nbuf[i].clusterNode.nodeName.value);
		}

		buffer = (SaClmClusterNotificationT *)
				ha_malloc(sizeof(SaClmClusterNotificationT));

		*buffer = nbuf[i];
		clm_member_list = g_list_append(clm_member_list, buffer);

		if (!is_cms_online(nbuf[i].clusterNode.nodeName.value)) {
			continue;
		}

		node = ha_strdup(nbuf[i].clusterNode.nodeName.value);
		dprintf("insert node [%s]\n", node);
		mqmember_list = g_list_insert_sorted(mqmember_list, node,
					comp_mqmember);
	}

	mqname_dump_membership();
}


int
cms_membership_init(cms_data_t * cmsdata)
{
	SaErrorT ret;
	SaVersionT version; 
	SaClmHandleT handle;
	SaSelectionObjectT st;
	SaClmClusterNotificationT * nbuf;

	SaClmCallbacksT my_callbacks = {
		.saClmClusterTrackCallback
		=	(SaClmClusterTrackCallbackT)mqclm_track,
		.saClmClusterNodeGetCallback
		=	(SaClmClusterNodeGetCallbackT)getnode_callback
	};

	version.major = 0x01;
	version.minor = 0x01;
	version.releaseCode = 'A';

	cl_log(LOG_INFO, "initializing with clm...");
	gcms_data = cmsdata;

	cmsdata->clm_nbuf = NULL;

	if ((ret = saClmInitialize(&handle, &my_callbacks, &version)) != SA_OK) {
		cl_log(LOG_ERR, "saClmInitialize error, errno [%d]", ret);
		return HA_FAIL;
	}

	nbuf = (SaClmClusterNotificationT *) ha_malloc(cmsdata->node_count * 
				sizeof (SaClmClusterNotificationT));

#if 0
	/* Get current cluster membership map. */
	if ((ret = saClmClusterTrackStart(&handle, SA_TRACK_CURRENT, nbuf, 
	    cmsdata->node_count) != SA_OK)) {
		cl_log(LOG_ERR, "SA_TRACK_CURRENT error, errno [%d]", ret);
		ha_free(nbuf);
		return HA_FAIL; 
	}
#endif

	ret = saClmClusterTrackStart(&handle, SA_TRACK_CURRENT, nbuf, cmsdata->node_count);

	cl_log(LOG_INFO, "SA_TRACK_CURRENT: ret = %d", ret);

	if (ret != SA_OK) {
		cl_log(LOG_ERR, "SA_TRACK_CURRENT error, errno [%d]", ret);
		ha_free(nbuf);
		return HA_FAIL; 
	}

	if ((ret = saClmSelectionObjectGet(&handle, &st)) != SA_OK) {
		cl_log(LOG_ERR, "saClmSelectionObjectGet error, errno [%d]", ret);
		ha_free(nbuf);
		return HA_FAIL;
	}

	/* Start to track cluster membership changes events */
	if ((ret = saClmClusterTrackStart(&handle, SA_TRACK_CHANGES, nbuf, 
		cmsdata->node_count)) != SA_OK) {
		cl_log(LOG_ERR, "SA_TRACK_CURRENT error, errno [%d]", ret);
		ha_free(nbuf);
		return HA_FAIL; 
	}

	cmsdata->clm_handle = handle;
	cmsdata->clm_fd = st;
	cmsdata->clm_nbuf = nbuf;

	if (g_list_length(mqmember_list) == 1) {
		cmsdata->cms_ready = 1;
	} else {
		cmsdata->cms_ready = 0;
		/* ask for the mqinfoupdate later */
	}

	return HA_OK;
}

void
cms_membership_finalize(SaClmHandleT * handle)
{
	saClmFinalize(handle);
	return;
}

int
cms_membership_dispatch(SaClmHandleT * handle, SaDispatchFlagsT flags)
{
	SaErrorT ret;

	if ((ret = saClmDispatch(handle, flags)) != SA_OK) {
		if (ret == SA_ERR_LIBRARY) {
			cl_log(LOG_ERR, "cms: evicted by membership!");
			return FALSE;
		} else if (ret == SA_ERR_BAD_HANDLE) {
			cl_log(LOG_ERR, "cms: Membership Service not "
				"available, errno [%d]", ret);
			return FALSE;
		}
		cl_log(LOG_ERR, "cms: saClmDispatch error, errno [%d]", ret);
		return FALSE;
	}

	return TRUE;
}


