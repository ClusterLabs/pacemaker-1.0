/* $Id: ipfail.c,v 1.42 2005/08/11 20:41:31 gshi Exp $ */
/* ipfail: IP Failover plugin for Linux-HA
 *
 * Copyright (C) 2002-2004 Kevin Dwyer <kevin@pheared.net>
 *
 * This plugin uses ping nodes to determine a failure in an
 * interface's connectivity and forces a hb_standby. It is based on the
 * api_test.c program included with Linux-HA.
 * 
 * Setup: In your ha.cf file make sure you have a ping node setup for each
 *        interface.  Choosing something like the switch that you are connected
 *        to is a good idea.  Choosing your win95 reboot-o-matic is a bad idea.
 *        
 *        The way this works is by taking note of when a ping node dies.  
 *        When a death is detected, it communicates with the other side to see
 *        if the other side saw it die (sort of).  If it didn't, then we know
 *        who deserves to have the resources.
 *
 * There are ways to improve this, and I'm working on them.
 *
 */
/* 
 * api_test: Test program for testing the heartbeat API
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <libgen.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <hb_api.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/GSource.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/coredumps.h>
#include "ipfail.h"
#include <clplumbing/cl_misc.h>

/* ICK! global vars. */
const char *node_name;	   /* The node we are connected to            */
char other_node[SYS_NMLN]; /* The remote node in the pair             */
int node_stable;           /* Other node stable?                      */
int need_standby;          /* Are we waiting for stability?           */
int quitnow = 0;           /* Allows a signal to break us out of loop */
int auto_failback;         /* How is our auto_failback configured?    */
GMainLoop *mainloop;       /* Reference to the mainloop for events    */
guint delay_giveup_tag = 0;/* Our delay timer                         */

int
main(int argc, char **argv)
{
	unsigned fmask;
	ll_cluster_t *hb;
	char pid[10];
	char *bname, *parameter;
	IPC_Channel *apiIPC;

	cl_log_enable_stderr(TRUE);
	
	/* Get the name of the binary for logging purposes */
	bname = ha_strdup(argv[0]);
	cl_log_set_entity(basename(bname));

	cl_log_set_facility(DEFAULT_FACILITY);

	hb = ll_cluster_new("heartbeat");

	memset(other_node, 0, sizeof(other_node));
	need_standby = 0;

	memset(pid, 0, sizeof(pid));
	snprintf(pid, sizeof(pid), "%ld", (long)getpid());
	cl_log(LOG_DEBUG, "PID=%s", pid);

	open_api(hb);

	node_stable = is_stable(hb);
	if (node_stable == -1) {
		cl_log(LOG_ERR, "No managed resources");
		exit(100);
	}

	/* Obtain our local node name */
	node_name = hb->llc_ops->get_mynodeid(hb);
	if (node_name == NULL) {
		cl_log(LOG_ERR, "Cannot get my nodeid");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(19);
	}
	cl_log(LOG_DEBUG, "[We are %s]", node_name);

	/* Check to see if we should engage auto_failback tactics */
	parameter = hb->llc_ops->get_parameter(hb, "auto_failback");
	if (parameter) {
		/* This is equivalent to nice_failback off */
		if (!strcmp(parameter, "legacy")) {
			cl_log(LOG_ERR, "auto_failback set to "
			       "incompatible legacy option.");
			exit(100);
		}
		
		if(str_to_boolean(parameter, &auto_failback) != HA_OK){
			cl_log(LOG_ERR, " invalid auto_faiback value(%s)",
			       parameter);
			exit(100);
		}

		cl_log(LOG_DEBUG, "auto_failback -> %i (%s)", auto_failback,
		       parameter);
		cl_free(parameter);
	}else{
		cl_log(LOG_ERR, "Couldn't get auto_failback setting.");
	}
	/* See if we should drop cores somewhere odd... */
	parameter = hb->llc_ops->get_parameter(hb, KEY_COREROOTDIR);
	if (parameter) {
		cl_set_corerootdir(parameter);
		cl_cdtocoredir();
	}
	cl_cdtocoredir();


	set_callbacks(hb);

	fmask = LLC_FILTER_DEFAULT;

	cl_log(LOG_DEBUG, "Setting message filter mode");
	if (hb->llc_ops->setfmode(hb, fmask) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set filter mode");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(8);
	}

	node_walk(hb);

	set_signals(hb);

	cl_log(LOG_DEBUG, "Waiting for messages...");
	errno = 0;
	cl_log_enable_stderr(FALSE);


	/* We will sit in a glib loop waiting for inputs, or making decisions
	 * for failover
	 */
	mainloop = g_main_new(TRUE);
	
        apiIPC = hb->llc_ops->ipcchan(hb);

	/* Watch the API IPC for input */
        G_main_add_IPC_Channel(G_PRIORITY_HIGH, apiIPC, FALSE, 
                               ipfail_dispatch, (gpointer)hb,
                               ipfail_dispatch_destroy);

	Gmain_timeout_add_full(G_PRIORITY_DEFAULT, 1000, 
	                     ipfail_timeout_dispatch, (gpointer)hb, 
	                     ipfail_dispatch_destroy);
	
	g_main_run(mainloop);
	g_main_destroy(mainloop);
	
	if (!quitnow && errno != EAGAIN && errno != EINTR) {
		cl_perror("read_hb_msg returned NULL");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
	}

	close_api(hb);

	return 0;
}

int
is_stable(ll_cluster_t *hb)
{
	const char *resources = hb->llc_ops->get_resources(hb);
	if (!resources)
		/* Heartbeat is not providing resource management */
	        return -1;
	
	if (!strcmp(resources, "transition"))
		return 0;

	return 1;
}

void
node_walk(ll_cluster_t *hb)
{
	const char *node;
/*	const char *intf;  --Out until ifwalk is fixed */

	cl_log(LOG_DEBUG, "Starting node walk");
	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(9);
	}
	while((node = hb->llc_ops->nextnode(hb)) != NULL) {
		cl_log(LOG_DEBUG, "Cluster node: %s: status: %s", node
		,	hb->llc_ops->node_status(hb, node));

		/* Look for our partner */
		if (!strcmp("normal", hb->llc_ops->node_type(hb, node))
		    && strcmp(node, node_name)) {
			strcpy(other_node, node);
			cl_log(LOG_DEBUG, "[They are %s]", other_node);
		}

		/* ifwalking is broken for ping nodes.  I don't think we even
		   need it at this point.

		if (hb->llc_ops->init_ifwalk(hb, node) != HA_OK) {
			cl_log(LOG_ERR, "Cannot start if walk");
			cl_log(LOG_ERR, "REASON: %s"
			,	hb->llc_ops->errmsg(hb));
			exit(10);
		}
		while ((intf = hb->llc_ops->nextif(hb))) {
			cl_log(LOG_DEBUG, "\tnode %s: intf: %s ifstatus: %s"
			,	node, intf
			,	hb->llc_ops->if_status(hb, node, intf));
		}
		if (hb->llc_ops->end_ifwalk(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot end if walk");
			cl_log(LOG_ERR, "REASON: %s"
			,	hb->llc_ops->errmsg(hb));
			exit(11);
		}
		-END of ifwalkcode */
	}
	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(12);
	}
}

void
set_callbacks(ll_cluster_t *hb)
{
	/* Add each of the callbacks we use with the API */

	if (hb->llc_ops->set_msg_callback(hb, T_APICLISTAT, 
					  msg_ipfail_join, hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set msg_ipfail_join callback");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(2);
	}

	if (hb->llc_ops->set_msg_callback(hb, T_RESOURCES, 
					  msg_resources, hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set msg_resources callback");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(18);
	}

	if (hb->llc_ops->set_msg_callback(hb, "num_ping_nodes", 
					  msg_ping_nodes, hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set msg callback");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(3);
	}

	if (hb->llc_ops->set_msg_callback(hb, "abort_giveup", 
					  msg_abort_giveup, hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set msg_abort_giveup callback");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(4);
	}

	if (hb->llc_ops->set_msg_callback(hb, "you_are_dead", 
					  i_am_dead, hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set i_am_dead callback");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(5);
	}

	if (hb->llc_ops->set_nstatus_callback(hb, NodeStatus, hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set node status callback");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(6);
	}

	if (hb->llc_ops->set_ifstatus_callback(hb, LinkStatus, hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set if status callback");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(7);
	}
}

void
set_signals(ll_cluster_t *hb)
{
	/* Setup the various signals */

	CL_SIGINTERRUPT(SIGINT, 1);
	CL_SIGNAL(SIGINT, gotsig);
	CL_SIGINTERRUPT(SIGTERM, 1);
	CL_SIGNAL(SIGTERM, gotsig);

	cl_log(LOG_DEBUG, "Setting message signal");
	if (hb->llc_ops->setmsgsignal(hb, 0) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set message signal");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(13);
	}
}

void
NodeStatus(const char *node, const char *status, void *private)
{
	/* Callback for node status changes */

	cl_log(LOG_INFO, "Status update: Node %s now has status %s"
	,	node, status);
	if (strcmp(status, DEADSTATUS) == 0) {
		if (ping_node_status(private)) {
			cl_log(LOG_INFO, "NS: We are still alive!");
		} else {
			cl_log(LOG_INFO, "NS: We are dead. :<");
		}
	} else if (strcmp(status, PINGSTATUS) == 0) {
		/* A ping node just came up, if we died, request resources?
		 * If so, that would emulate the primary/secondary type of
		 * High-Availability, instead of nice_failback mode
		 */

		/* Lets make sure we weren't both down, and now half up. */
		int num_ping;

		cl_log(LOG_INFO, "A ping node just came up.");
		num_ping = ping_node_status(private);
		ask_ping_nodes(private, num_ping);
	}
}

void
LinkStatus(const char *node, const char *lnk, const char *status,
	   void *private)
{
	/* Callback for Link status changes */

	int num_ping=0;

	cl_log(LOG_INFO, "Link Status update: Link %s/%s now has status %s"
	,	node, lnk, status);

	if (strcmp(status, DEADSTATUS) == 0) {
		/* If we can still see pinging node, request resources */
		if ((num_ping = ping_node_status(private))) {
			ask_ping_nodes(private, num_ping);
			cl_log(LOG_INFO, "Checking remote count"
			       " of ping nodes.");
		} else {
			cl_log(LOG_INFO, "We are dead. :<");
                        ask_ping_nodes(private, num_ping);
		}
	}
}

int
ping_node_status(ll_cluster_t *hb)
{
	/* ping_node_status: Takes the hearbeat cluster as input, 
	 * returns number of ping nodes found to be in the cluster, 
	 * and therefore alive.
	 */

	const char *node;
	int found=0;       /* Number of ping nodes found */

	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(16);
	}
	while((node = hb->llc_ops->nextnode(hb))!= NULL) {
		if (!strcmp(PINGSTATUS, 
			    hb->llc_ops->node_status(hb, node))) {

			cl_log(LOG_DEBUG, "Found ping node %s!", node);
			found++;
		}
	}
	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(17);
	}

	return found;
}

gboolean
giveup(gpointer user_data)
{
	/* Giveup: Takes the heartbeat cluster as input and the type of
	 * resources to give up.  Returning FALSE causes the timer to die.
	 * Forces the local node to release a particular class of resources.
	 */

	struct giveup_data *gd = user_data;
	ll_cluster_t *hb = gd->hb;
	const char *res_type = gd->res_type;
	struct ha_msg *msg;
	char pid[10];

	cl_log(LOG_INFO, "giveup() called (timeout worked)");

	if (is_stable(hb)) {
		memset(pid, 0, sizeof(pid));
		snprintf(pid, sizeof(pid), "%ld", (long)getpid());

		msg = ha_msg_new(3);
		ha_msg_add(msg, F_TYPE, T_ASKRESOURCES);
		ha_msg_add(msg, F_RESOURCES, res_type);
		ha_msg_add(msg, F_ORIG, node_name);
		ha_msg_add(msg, F_COMMENT, "me");

		hb->llc_ops->sendclustermsg(hb, msg);
		cl_log(LOG_DEBUG, "Message [" T_ASKRESOURCES "] sent.");
		ha_msg_del(msg);
		need_standby = 0;
	} else {
		need_standby = 1;
	}

	return FALSE;
}

void
delay_giveup(ll_cluster_t *hb, const char *res_type, int mseconds)
{
	struct giveup_data *gd;

	gd = malloc(sizeof(struct giveup_data));
       
        if (gd == NULL) {
                cl_log(LOG_ERR, "Out of memory, can't giveup.");
                return;
        }

	gd->hb = hb;
	gd->res_type = res_type;

	/* Set mseconds to -1 to use default. (twice the keepalive) */
	if (mseconds < 0) {
		mseconds = hb->llc_ops->get_keepalive(hb) * 2;
	}

	cl_log(LOG_INFO, "Delayed giveup in %i seconds.", mseconds / 1000);

	if (delay_giveup_tag) {
		/* A timer exists already? */
		cl_log(LOG_DEBUG, "Detected existing delay timer, overriding");
		Gmain_timeout_remove(delay_giveup_tag);
		delay_giveup_tag = 0;
	}

	/* We are going to call giveup in mseconds/1000 Seconds. */
	delay_giveup_tag = Gmain_timeout_add_full(G_PRIORITY_DEFAULT, 
						  mseconds, 
						  giveup, (gpointer)gd, 
						  giveup_destroy);
}

void
giveup_destroy(gpointer user_data)
{
	/* Clean up the struct giveup_data that we were using */
	free(user_data);

	delay_giveup_tag = 0;
	cl_log(LOG_DEBUG, "giveup timeout has been destroyed.");
}

void
abort_giveup()
{
	if (delay_giveup_tag) {
		cl_log(LOG_INFO, "Aborted delayed giveup (%u)", 
		       delay_giveup_tag);
		Gmain_timeout_remove(delay_giveup_tag);
		delay_giveup_tag = 0;
	} else {
		cl_log(LOG_INFO, "No giveup timer to abort.");
	}
}

void
send_abort_giveup(ll_cluster_t *hb)
{
        struct ha_msg *msg;

        msg = ha_msg_new(2);
        ha_msg_add(msg, F_TYPE, "abort_giveup");
        ha_msg_add(msg, F_ORIG, node_name);

        hb->llc_ops->sendnodemsg(hb, msg, other_node);
        cl_log(LOG_DEBUG, "Abort message sent.");
        ha_msg_del(msg);
}

void
msg_abort_giveup(const struct ha_msg *msg, void *private)
{
        abort_giveup();
}


void
msg_ipfail_join(const struct ha_msg *msg, void *private)
{
	/* msg_ipfail_join: When another ipfail client sends a join 
	 * message, call ask_ping_nodes() to compare ping node counts.
	 * Callback for the T_APICLISTAT message. 
	 */

	/* If this is a join message from ipfail on a different node.... */
	if (!strcmp(ha_msg_value(msg, F_STATUS), JOINSTATUS) &&
	    !strcmp(ha_msg_value(msg, F_FROMID), "ipfail")   && 
	    strcmp(ha_msg_value(msg, F_ORIG),   node_name)) {
		cl_log(LOG_DEBUG, 
		       "Got join message from another ipfail client. (%s)",
		       ha_msg_value(msg, F_ORIG));
		ask_ping_nodes(private, ping_node_status(private));
	}
}

void
msg_resources(const struct ha_msg *msg, void *private)
{
	const char * orig = ha_msg_value(msg, F_ORIG);
	const char * isstable = ha_msg_value(msg, F_ISSTABLE);

	/* msg_resources: Catch T_RESOURCES messages, so that we can
	 * find out when stability is achieved among the cluster
	 */
	if (!orig || !isstable) {
		return;	/* No stability info in this message... */
	}

	/* Right now there are two stable messages sent out, we are
	 * only concerned with the one that has no info= line on it.
	 */
	if (!strcmp(orig, other_node) &&
	    !ha_msg_value(msg, F_COMMENT) &&
	    !strcmp(isstable, "1")) {

		cl_log(LOG_DEBUG, "Other side is now stable.");
		node_stable = 1;
		
		/* There may be a pending standby */
		if (need_standby) {
			/* Gratuitious ARPs take some time, is there a
			 * way to know when they're finished?  I don't
			 * want this sleep here, even if it only is during
			 * startup.
			 */

			/* This value is prone to be wrong for different
			 * situations.  We need the resource stability
			 * message to be delayed until the resource scripts
			 * finish, and then we can stop waiting.
			 */
			sleep(10);


			/* If the resource message stuff is solved, we could
			 * safely giveup() here.  However, since we're waiting
			 * for arbitrary amounts of time it may be wise to
			 * recheck the assumptions of the cluster and count
			 * ping nodes.
			 */
			ask_ping_nodes(private, ping_node_status(private));
			/* giveup(private); */

                        /* The ask_ping_nodes message will sort out whether
                         * a standby is necessary. */
                        need_standby = 0;
		}
	}

	else if (!strcmp(orig, other_node) &&
		 !strcmp(isstable, "0")) {

		cl_log(LOG_DEBUG, "Other side is unstable.");
		node_stable = 0;
	
	}
}

void
ask_ping_nodes(ll_cluster_t *hb, int num_ping)
{
	/* ask_ping_nodes: Takes the heartbeat cluster and the number of
	 * ping nodes we can see alive as input, returning nothing.
	 * It asks the other node for the number of ping nodes it can see.
	 */

	struct ha_msg *msg;
	char pid[10], np[5];

	cl_log(LOG_INFO, "Asking other side for ping node count.");
	memset(pid, 0, sizeof(pid));
	snprintf(pid, sizeof(pid), "%ld", (long)getpid());
	memset(np, 0, sizeof(np));
	snprintf(np, sizeof(np), "%d", num_ping);

	msg = ha_msg_new(3);
	ha_msg_add(msg, F_TYPE, "num_ping_nodes");
	ha_msg_add(msg, F_ORIG, node_name);
	ha_msg_add(msg, F_NUMPING, np);

	hb->llc_ops->sendnodemsg(hb, msg, other_node);
	cl_log(LOG_DEBUG, "Message [" F_NUMPING "] sent.");
	ha_msg_del(msg);
}

void
msg_ping_nodes(const struct ha_msg *msg, void *private)
{
	/* msg_ping_nodes: Takes the message and heartbeat cluster as input;
	 * returns nothing.  Callback for the num_ping_nodes message.
	 */

	int num_nodes=0;
	ll_cluster_t *hb = private;

	cl_log(LOG_DEBUG, "Got asked for num_ping.");
	num_nodes = ping_node_status(hb);
	if (num_nodes > atoi(ha_msg_value(msg, F_NUMPING))) {
                cl_log(LOG_INFO, 
                       "Telling other node that we have more visible ping "
                       "nodes.");
		you_are_dead(hb);
	}
	else if (num_nodes < atoi(ha_msg_value(msg, F_NUMPING))) {
		cl_log(LOG_INFO, 
                       "Giving up because we have less visible ping nodes.");
		delay_giveup(hb, HB_ALL_RESOURCES, -1);
	}
	else {
		cl_log(LOG_INFO, "Ping node count is balanced.");
		send_abort_giveup(hb);
		if (delay_giveup_tag) {
			/* We've got a delayed giveup, and we're now balanced*/
			/* BUG? We don't want to do this if we have an 
			   auto_failback pending, I think. */

			abort_giveup();
		} else if (auto_failback && is_stable(hb)) {
			/* We're balanced, so make sure we don't have foreign 
			 * stuff
			 */
			cl_log(LOG_INFO, 
                               "Giving up foreign resources (auto_failback).");
			delay_giveup(hb, HB_FOREIGN_RESOURCES, -1);
		}
	}
}

void
you_are_dead(ll_cluster_t *hb)
{
	/* you_are_dead: Takes the heartbeat cluster as input; returns nothing.
	 * Sends the you_are_dead message to the dead node.
	 */

	struct ha_msg *msg;
	char pid[10];

	cl_log(LOG_DEBUG, "Sending you_are_dead.");

	memset(pid, 0, sizeof(pid));
	snprintf(pid, sizeof(pid), "%ld", (long)getpid());

	msg = ha_msg_new(1);
	ha_msg_add(msg, F_TYPE, "you_are_dead");

	hb->llc_ops->sendnodemsg(hb, msg, other_node);
	cl_log(LOG_DEBUG, "Message [you_are_dead] sent.");
	ha_msg_del(msg);
}

void
i_am_dead(const struct ha_msg *msg, void *private)
{
	/* i_am_dead: Takes the you_are_dead message and the heartbeat cluster
	 * as input; returns nothing.
	 * Callback for the you_are_dead message.
	 */

	cl_log(LOG_INFO, 
               "Giving up because we were told that we have less ping nodes.");
	delay_giveup(private, HB_ALL_RESOURCES, -1);
}

void
gotsig(int nsig)
{
	(void)nsig;
	quitnow = 1;
}

/* Used to handle the API in the gmainloop */
gboolean 
ipfail_dispatch(IPC_Channel* ipc, gpointer user_data)
{
	struct ha_msg *reply;
	ll_cluster_t *hb = user_data;
	
	/*
	if (hb->llc_ops->msgready(hb))
		cl_log(LOG_DEBUG, "Msg ready!");
	cl_log(LOG_DEBUG, "Reading a message!");
	*/
	reply = hb->llc_ops->readmsg(hb, 0);

	if (reply != NULL) {
		/* cl_log_message(reply); */
		ha_msg_del(reply); reply=NULL;
		return TRUE;
	}
	/*
	else
	  return FALSE;
	*/
	
	return TRUE;
}

void
ipfail_dispatch_destroy(gpointer user_data)
{
	return;
}

gboolean
ipfail_timeout_dispatch(gpointer user_data)
{
	ll_cluster_t *hb = user_data;

	if (quitnow) {
		g_main_quit(mainloop);
		return FALSE;
	}

	if (hb->llc_ops->msgready(hb)) {
		/* cl_log(LOG_DEBUG, "Msg ready! [2]"); */
		return ipfail_dispatch(NULL, user_data);
	}
	return TRUE;
}

void
open_api(ll_cluster_t *hb)
{
	/* Sign in to the API and setup the log facility */
	int facility;

	cl_log(LOG_DEBUG, "Signing in with heartbeat");
	if (hb->llc_ops->signon(hb, "ipfail")!= HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(1);
	}
	if ((facility = hb->llc_ops->get_logfacility(hb)) <= 0) {
		facility = DEFAULT_FACILITY;
	}
	cl_log_set_facility(facility);
}

void
close_api(ll_cluster_t *hb)
{
	/* Log off of the API and clean up */
	if (hb->llc_ops->signoff(hb, FALSE) != HA_OK) {
		cl_log(LOG_ERR, "Cannot sign off from heartbeat.");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		exit(14);
	}
	if (hb->llc_ops->delete(hb) != HA_OK) {
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		cl_log(LOG_ERR, "Cannot delete API object.");
		exit(15);
	}
}
