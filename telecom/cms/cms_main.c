/*
 * cms_main.c: cms daemon main entry
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

#include <ha_config.h>
#include <config.h>
#include <portability.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>
#include <string.h>

#include <clplumbing/cl_log.h>
#include <clplumbing/GSource.h>
#include <hb_api.h>
#include <heartbeat.h>

#include "cms_data.h"
#include "cms_client.h"
#include "cms_membership.h"

#if DEBUG_MEMORY
#include <mcheck.h>
#endif


cms_data_t cms_data;
int option_debug;


/*
 * The callback function which is called when the status of a link
 * changes.
 */
static void
LinkStatus(const char * node, const char * lnk, const char * status,
		void * private)
{
	cl_log(LOG_DEBUG, "Link Status update: Link %s/%s "
			"now has status %s", node, lnk, status);
}

/*
 * The callback function which is called when the status of a cms
 * daemon in the cluster changes.
 */
static void
ClientStatus(const char * node, const char * client, const char * status,
		void * private)
{
	set_cms_status(node, status, private);
}

static int
cms_heartbeat_init(cms_data_t * cmsdata) 
{
	ll_cluster_t * heartbeat;
	const char * myhost;
	const char * host;
	IPC_Channel * hb_channel;
	size_t ncount = 0;

	heartbeat = ll_cluster_new("heartbeat");

	cl_log(LOG_INFO, "cms pid = %ld", (long) getpid());

	cl_log(LOG_INFO, "signing in with heartbeat");

	if (heartbeat->llc_ops->signon(heartbeat, CMSID) != HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat");
		cl_log(LOG_ERR, "Reason: %s"
		,	heartbeat->llc_ops->errmsg(heartbeat));
		return HA_FAIL;
	}

	if ((myhost = heartbeat->llc_ops->get_mynodeid(heartbeat)) == NULL) {
		cl_log(LOG_ERR, "Cannot get my node id");
		cl_log(LOG_ERR, "Reason: %s"
		,	heartbeat->llc_ops->errmsg(heartbeat));
		return HA_FAIL;
	}

	if (heartbeat->llc_ops->set_ifstatus_callback(heartbeat
	,	LinkStatus, NULL) != HA_OK) {

		cl_log(LOG_ERR, "Cannot set if status call back");
		cl_log(LOG_ERR, "Reason: %s"
		,	heartbeat->llc_ops->errmsg(heartbeat));
		return HA_FAIL;
	}

	if (heartbeat->llc_ops->init_nodewalk(heartbeat) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk");
		cl_log(LOG_ERR, "Reason: %s"
		,	heartbeat->llc_ops->errmsg(heartbeat));
		return HA_FAIL;
	}

	while ((host = heartbeat->llc_ops->nextnode(heartbeat)) != NULL) {

		/* ignore non normal nodes */
		if (strcmp(heartbeat->llc_ops->node_type(heartbeat, host)
		,	"normal") != 0) {
			continue;
		}
		ncount++;
	}

	if (heartbeat->llc_ops->end_nodewalk(heartbeat) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk");
		cl_log(LOG_ERR, "Reason: %s"
		,	heartbeat->llc_ops->errmsg(heartbeat));
		return HA_FAIL;
	}

	hb_channel = heartbeat->llc_ops->ipcchan(heartbeat);

	cmsdata->hb_handle = heartbeat;
	cmsdata->hb_channel = hb_channel;
	cmsdata->node_count = ncount;
	cmsdata->my_nodeid = ha_strdup(myhost);

	if (heartbeat->llc_ops->set_cstatus_callback(heartbeat
	,	ClientStatus, NULL) != HA_OK) {

		cl_log(LOG_ERR, "Cannot set client status callback");
		cl_log(LOG_ERR, "REASON: %s"
		,	heartbeat->llc_ops->errmsg(heartbeat));
		return HA_FAIL;
	}

	heartbeat->llc_ops->client_status(heartbeat, NULL, CMSID, 0);

	return HA_OK;
}

static void
hb_input_destroy(gpointer user_data)
{
	cms_data_t * cmsdata = (cms_data_t *) user_data;

	/* close connection to all the clients */
	cms_client_close_all(cmsdata->client_table);

	/* shut ourself down when there's no heartbeat. */
	g_main_quit(cmsdata->mainloop);
	return;
}

static gboolean
waitCh_input_dispatch(IPC_Channel *newclient, gpointer user_data)
{
	cms_data_t * cmsdata = (cms_data_t *) user_data;

	cms_client_add(&cmsdata->client_table, newclient);

	G_main_add_IPC_Channel(G_PRIORITY_LOW, newclient, FALSE
	,	client_input_dispatch, cmsdata, cms_client_input_destroy);

	cl_log(LOG_INFO, "%s: return TRUE", __FUNCTION__);
	return TRUE;
}

static void
waitCh_input_destroy(gpointer user_data)
{
	cms_data_t * cmsdata = (cms_data_t *) user_data;

	IPC_WaitConnection *wait_ch = cmsdata->wait_ch;

	wait_ch->ops->destroy(wait_ch);

	cl_log(LOG_INFO, "%s: return TRUE", __FUNCTION__);
	return;
}

static gboolean
clm_input_dispatch(int fd, gpointer user_data)
{
	cms_data_t * cmsdata = (cms_data_t *) user_data;
	gboolean ret;

	ret = cms_membership_dispatch(&cmsdata->clm_handle, SA_DISPATCH_ALL);

	return ret;
}

static void
clm_input_destroy(gpointer user_data)
{
	cms_data_t * cmsdata = (cms_data_t *) user_data;

	cl_log(LOG_WARNING, "Lost connection to membership service. Need to bail out.");

	cms_membership_finalize(&cmsdata->clm_handle);

	g_main_quit(cmsdata->mainloop);

	return;
}

static gboolean
hb_input_dispatch(IPC_Channel * channel, gpointer user_data)
{
	if (channel->ch_status == IPC_DISCONNECT) {
		cl_log(LOG_INFO, "Lost connection to heartbeat service. Need to bail out!");
		return FALSE;
	}

	return cluster_input_dispatch(channel, user_data);
}

int
main(int argc, char ** argv) 
{
	GMainLoop *mainloop;
	int c;

	while (1) {
		c = getopt(argc, argv, "d");

		if (c == -1)
			break;

		switch (c) {
			case 'd':
				option_debug++;
				break;
			default:
				fprintf(stderr, "Error: unknown option %c\n",c);
				return 1;
		}
	}

	mainloop = g_main_new(TRUE);

	cl_log_set_entity(argv[0]);
	cl_log_set_facility(LOG_USER);
	if (option_debug)
		cl_log_enable_stderr(TRUE);

	if (cms_heartbeat_init(&cms_data) != HA_OK) {
		cl_log(LOG_ERR, "cms_heartbeat_init failed");
		exit(1);
	}

	if (cms_membership_init(&cms_data) != HA_OK) {
		cl_log(LOG_ERR, "cms_membership_init failed");
		exit(2);
	}

	if (cluster_hash_table_init() != HA_OK) {
		cl_log(LOG_ERR, "cluster_hash_table_init failed");
		exit(3);
	}

	if (cms_client_init(&cms_data) != HA_OK) {
		cl_log(LOG_ERR, "cms_client_init failed");
		exit(4);
	}

	G_main_add_IPC_Channel(G_PRIORITY_HIGH, cms_data.hb_channel, FALSE
	,	hb_input_dispatch, &cms_data, hb_input_destroy);

	G_main_add_fd(G_PRIORITY_HIGH, cms_data.clm_fd, FALSE
	,	clm_input_dispatch, &cms_data, clm_input_destroy);

	G_main_add_IPC_WaitConnection(G_PRIORITY_LOW, cms_data.wait_ch, NULL
	,	FALSE, waitCh_input_dispatch, &cms_data, waitCh_input_destroy);

	cms_data.mainloop = mainloop;

#if DEBUG_MEMORY
	mtrace();
#endif
	/* NOTE: we send out a request for mqinfo_update 
	   if we are not the first or only node in the cluster. 
	   
	   There is a possible race condition here.  If this node
	   itself is really really slow compared to the rest of 
	   the nodes in the cluster, it might get the reply back
	   before the main loop get setup thus lost this message. */
	if (!cms_data.cms_ready) {
		request_mqinfo_update(&cms_data);
	}

	g_main_run(mainloop);
#if DEBUG_MEMORY
	muntrace();
#endif
	g_main_destroy(mainloop);

	if (cms_data.my_nodeid)
		ha_free(cms_data.my_nodeid);

	if (cms_data.clm_nbuf);
		ha_free(cms_data.clm_nbuf);

	return 1;
}

