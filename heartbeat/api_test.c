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
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <syslog.h>
#include <hb_api_core.h>
#include <hb_api.h>

/*
 * A heartbeat API test program...
 */

void NodeStatus(const char * node, const char * status, void * private);
void LinkStatus(const char * node, const char *, const char *, void*);
void gotsig(int nsig);

void
NodeStatus(const char * node, const char * status, void * private)
{
	cl_log(LOG_NOTICE, "Status update: Node %s now has status %s\n"
	,	node, status);
}

void
LinkStatus(const char * node, const char * lnk, const char * status
,	void * private)
{
	cl_log(LOG_NOTICE, "Link Status update: Link %s/%s now has status %s\n"
	,	node, lnk, status);
}

int quitnow = 0;
void gotsig(int nsig)
{
	(void)nsig;
	quitnow = 1;
}

const char * mandparms[] =
{	KEY_HBVERSION
,	KEY_HOPS
,	KEY_KEEPALIVE
,	KEY_DEADTIME
,	KEY_DEADPING
,	KEY_WARNTIME
,	KEY_INITDEAD
,	KEY_BAUDRATE
,	KEY_UDPPORT
,	KEY_AUTOFAIL
,	KEY_GEN_METH
,	KEY_REALTIME
,	KEY_DEBUGLEVEL
,	KEY_NORMALPOLL};

const char * optparms[] =
{	KEY_LOGFILE
,	KEY_DBGFILE
,	KEY_FACILITY
,	KEY_RT_PRIO
,	KEY_WATCHDOG};


int
main(int argc, char ** argv)
{
	struct ha_msg*	reply;
	struct ha_msg*	pingreq = NULL;
	unsigned	fmask;
	ll_cluster_t*	hb;
	const char *	node;
	const char *	intf;
	int		msgcount=0;
	char *		ctmp;
	const char *	cval;
	int		j;

	(void)_ha_msg_h_Id;

	cl_log_set_entity(argv[0]);
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(LOG_USER);
	hb = ll_cluster_new("heartbeat");
	cl_log(LOG_INFO, "PID=%ld\n", (long)getpid());
	cl_log(LOG_INFO, "Signing in with heartbeat\n");
	if (hb->llc_ops->signon(hb, "ping")!= HA_OK) {
		cl_log(LOG_ERR, "Cannot sign on with heartbeat\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(1);
	}

	if (hb->llc_ops->set_nstatus_callback(hb, NodeStatus, NULL) !=HA_OK){
		cl_log(LOG_ERR, "Cannot set node status callback\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(2);
	}

	if (hb->llc_ops->set_ifstatus_callback(hb, LinkStatus, NULL)!=HA_OK){
		cl_log(LOG_ERR, "Cannot set if status callback\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(3);
	}

#if 0
	fmask = LLC_FILTER_RAW;
#else
	fmask = LLC_FILTER_DEFAULT;
#endif
	/* This isn't necessary -- you don't need this call - it's just for testing... */
	cl_log(LOG_INFO, "Setting message filter mode\n");
	if (hb->llc_ops->setfmode(hb, fmask) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set filter mode\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(4);
	}

	for (j=0; j < DIMOF(mandparms); ++j) {
		if ((ctmp = hb->llc_ops->get_parameter(hb, mandparms[j])) != NULL) {
			cl_log(LOG_INFO, "Parameter %s is [%s]"
			,	mandparms[j]
			,	ctmp);
			free(ctmp); ctmp = NULL;
		}else{
			cl_log(LOG_ERR, "Mandantory Parameter %s is not available!"
			,	mandparms[j]);
		}
	}
	for (j=0; j < DIMOF(optparms); ++j) {
		if ((ctmp = hb->llc_ops->get_parameter(hb, optparms[j])) != NULL) {
			cl_log(LOG_INFO, "Optional Parameter %s is [%s]"
			,	optparms[j]
			,	ctmp);
			free(ctmp); ctmp = NULL;
		}
	}
	if ((cval = hb->llc_ops->get_resources(hb)) == NULL) {
		cl_perror("Cannot get resource status");
		cl_log(LOG_ERR, "REASON: %s\n"
		,	hb->llc_ops->errmsg(hb));
	}else{
		cl_log(LOG_INFO, "Current resource status: %s", cval);
	}


	cl_log(LOG_INFO, "Starting node walk\n");
	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(5);
	}
	while((node = hb->llc_ops->nextnode(hb))!= NULL) {
		cl_log(LOG_INFO, "Cluster node: %s: status: %s\n", node
		,	hb->llc_ops->node_status(hb, node));
		if (hb->llc_ops->init_ifwalk(hb, node) != HA_OK) {
			cl_log(LOG_ERR, "Cannot start if walk\n");
			cl_log(LOG_ERR, "REASON: %s\n"
			,	hb->llc_ops->errmsg(hb));
			exit(6);
		}
		while ((intf = hb->llc_ops->nextif(hb))) {
			cl_log(LOG_INFO, "\tnode %s: intf: %s ifstatus: %s\n"
			,	node, intf
			,	hb->llc_ops->if_status(hb, node, intf));
		}
		if (hb->llc_ops->end_ifwalk(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot end if walk\n");
			cl_log(LOG_ERR, "REASON: %s\n"
			,	hb->llc_ops->errmsg(hb));
			exit(7);
		}
	}
	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(8);
	}

	CL_SIGINTERRUPT(SIGINT, 1);
	CL_SIGNAL(SIGINT, gotsig);

#if 0
	/* This is not necessary either ;-) */
	cl_log(LOG_INFO, "Setting message signal\n");
	if (hb->llc_ops->setmsgsignal(hb, 0) != HA_OK) {
		cl_log(LOG_ERR, "Cannot set message signal\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(9);
	}

#endif
	pingreq = ha_msg_new(0);
	ha_msg_add(pingreq, F_TYPE, "ping");
	cl_log(LOG_INFO, "Sleeping...\n");
	sleep(5);
	if (hb->llc_ops->sendclustermsg(hb, pingreq) == HA_OK) {
		cl_log(LOG_INFO, "Sent ping request to cluster\n");
	}else{
		cl_log(LOG_ERR, "PING request FAIL to cluster\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
	}
	cl_log(LOG_INFO, "Waiting for messages...\n");
	errno = 0;
	for(; !quitnow && (reply=hb->llc_ops->readmsg(hb, 1)) != NULL;) {
		const char *	type;
		const char *	orig;
		++msgcount;
		if ((type = ha_msg_value(reply, F_TYPE)) == NULL) {
			type = "?";
		}
		if ((orig = ha_msg_value(reply, F_ORIG)) == NULL) {
			orig = "?";
		}
		cl_log(LOG_NOTICE, "Got message %d of type [%s] from [%s]\n"
		,	msgcount, type, orig);
		if (strcasecmp(type, T_APICLISTAT) == 0) {
			ha_log_message(reply);
			cl_log(LOG_NOTICE, "%s", hb->llc_ops->errmsg(hb));
		}
#if 0
		else {
			ha_log_message(reply);
			cl_log(LOG_NOTICE, "%s", hb->llc_ops->errmsg(hb));
		}
#endif
		if (strcmp(type, "ping") ==0) {
			struct ha_msg*	pingreply = ha_msg_new(4);
			int	count;

			ha_msg_add(pingreply, F_TYPE, "pingreply");

			for (count=0; count < 10; ++count) {
				if (hb->llc_ops->sendnodemsg(hb, pingreply, orig)
				==	HA_OK) {
					cl_log(LOG_INFO
					,	"Sent ping reply(%d) to [%s]\n"
					,	count, orig);
				}else{
					cl_log(LOG_ERR, "PING %d FAIL to [%s]\n"
					,	count, orig);
				}
			}
			ha_msg_del(pingreply); pingreply=NULL;
		}
		ha_msg_del(reply); reply=NULL;
	}

	if (!quitnow) {
		cl_log(LOG_ERR, "read_hb_msg returned NULL");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
	}
	if (hb->llc_ops->signoff(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot sign off from heartbeat.\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(10);
	}
	if (hb->llc_ops->delete(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot delete API object.\n");
		cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
		exit(11);
	}
	return 0;
}
