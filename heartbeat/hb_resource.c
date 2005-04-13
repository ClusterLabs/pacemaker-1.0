/* $Id: hb_resource.c,v 1.75 2005/04/13 18:04:46 gshi Exp $ */
/*
 * hb_resource: Linux-HA heartbeat resource management code
 *
 * Copyright (C) 2001-2002 Luis Claudio R. Goncalves
 *				<lclaudio@conectiva.com.br>
 * Copyright (C) 1999-2002 Alan Robertson <alanr@unix.sh>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <portability.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <hb_proc.h>
#include <hb_resource.h>
#include <heartbeat_private.h>
#include <hb_api_core.h>
#include <setproctitle.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/realtime.h>

/**************************************************************************
 *
 * This file contains almost all the resource management code for
 * heartbeat.
 *
 * It contains code to handle:
 *	resource takeover
 *	standby processing
 *	STONITH operations.
 *	performing notify_world() type notifications of status changes.
 *
 * We're planning on replacing it with an external process
 * to perform resource management functions as a heartbeat client.
 *
 * In the mean time, we're planning on disentangling it from the main
 * heartbeat code and cleaning it up some.
 *
 * Here are my favorite cleanup tasks:
 *
 * Get rid of the "standby_running" timer, and replace it with a gmainloop
 *	timer.
 *
 * Make hooks for processing incoming messages (in heartbeat.c) cleaner
 *	and probably hook them in through a hash table callback hook
 *	or something.
 *
 * Make registration hooks to allow notify_world to be called by pointer.
 *
 * Reduce the dependency on global variables shared between heartbeat.c
 *	and here.
 *
 * Generally Reduce the number of interactions between this code and
 *	heartbeat.c as evidenced by heartbeat_private.h and hb_resource.h
 *
 **************************************************************************/

extern struct node_info *	curnode;
int				DoManageResources = TRUE;
int 				nice_failback = FALSE;
int 				auto_failback = FALSE;
int 				failback_in_progress = FALSE;
static gboolean			rsc_needs_failback = FALSE;
/*
 * These are true when all our initial work for taking over local
 * or foreign resources is completed, or found to be unnecessary. 
 */
static gboolean			local_takeover_work_done = FALSE;
static gboolean			foreign_takeover_work_done = FALSE;

static gboolean			rsc_needs_shutdown = FALSE;
int				other_holds_resources = HB_NO_RSC;
int				other_is_stable = FALSE; /* F_ISSTABLE */
int				takeover_in_progress = FALSE;
enum hb_rsc_state		resourcestate = HB_R_INIT;
enum standby			going_standby = NOT;
longclock_t			standby_running = 0L;
static int			standby_rsctype = HB_ALL_RSC;

#define	INITMSG			"Initial resource acquisition complete"

/*
 * A helper to allow us to pass things into the anonproc
 * environment without any warnings about passing const strings
 * being passed into a plain old (non-const) gpointer.
 */
struct hb_const_string {
	const char * str;
};

#define	HB_RSCMGMTPROC(p, s)					\
	{							\
	 	static struct hb_const_string cstr = {(s)};	\
		NewTrackedProc((p), 1				\
		,	(debug ? PT_LOGVERBOSE : PT_LOGNORMAL)	\
		,	&cstr, &hb_rsc_RscMgmtProcessTrackOps);	\
	}


/*
 * A helper function which points at a malloced string.
 */
struct StonithProcHelper {
	char *		nodename;
};
extern ProcTrack_ops ManagedChildTrackOps;

static int	ResourceMgmt_child_count = 0;

static void	StartNextRemoteRscReq(void);
static void	InitRemoteRscReqQueue(void);
static int	send_standby_msg(enum standby state);
static void 	send_stonith_msg(const char *, const char *);
static void	go_standby(enum standby who, int resourceset);
static int	send_local_starting(void);

static	void	RscMgmtProcessRegistered(ProcTrack* p);
static	void	RscMgmtProcessDied(ProcTrack* p, int status, int signo
,				int exitcode, int waslogged);
static	const char * RscMgmtProcessName(ProcTrack* p);

static	void StonithProcessDied(ProcTrack* p, int status, int signo
,		int exitcode, int waslogged);
static	const char * StonithProcessName(ProcTrack* p);
static	void StonithStatProcessDied(ProcTrack* p, int status, int signo
,		int exitcode, int waslogged);
static	const char * StonithStatProcessName(ProcTrack* p);
void	Initiate_Reset(Stonith* s, const char * nodename, gboolean doreset);
static int FilterNotifications(const char * msgtype);
static int countbystatus(const char * status, int matchornot);
static gboolean hb_rsc_isstable(void);
static void shutdown_if_needed(void);

ProcTrack_ops hb_rsc_RscMgmtProcessTrackOps = {
	RscMgmtProcessDied,
	RscMgmtProcessRegistered,
	RscMgmtProcessName
};

static ProcTrack_ops StonithProcessTrackOps = {
	StonithProcessDied,
	NULL,
	StonithProcessName
};

static ProcTrack_ops StonithStatProcessTrackOps = {
	StonithStatProcessDied,
	NULL,
	StonithStatProcessName
};



static void
HBDoMsg_T_STARTING_or_RESOURCES(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	/*
	 * process_resources() will deal with T_STARTING
	 * and T_RESOURCES messages appropriately.
	 */
	process_resources(type, msg, fromnode);
	heartbeat_monitor(msg, KEEPIT, iface);
}

/* Someone wants to go standby!!! */
static void
HBDoMsg_T_ASKRESOURCES(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	heartbeat_monitor(msg, KEEPIT, iface);
	ask_for_resources(msg);
}

static void
HBDoMsg_T_ASKRELEASE(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	heartbeat_monitor(msg, KEEPIT, iface);
	if (fromnode != curnode) {
		/*
		 * Queue for later handling...
		 */
		QueueRemoteRscReq(PerformQueuedNotifyWorld, msg);
	}
}

static void
HBDoMsg_T_ACKRELEASE(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	/* Ignore this if we're shutting down! */
	if (shutdown_in_progress) {
		return;
	}
	heartbeat_monitor(msg, KEEPIT, iface);
	QueueRemoteRscReq(PerformQueuedNotifyWorld, msg);
}
/* Process a message no one recognizes */
static void
HBDoMsg_default(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	heartbeat_monitor(msg, KEEPIT, iface);
	QueueRemoteRscReq(PerformQueuedNotifyWorld, msg);
}

/* Received a "SHUTDONE" message from someone... */
static void 
HBDoMsg_T_SHUTDONE(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	if (heartbeat_comm_state == COMM_LINKSUP) {
		process_resources(type, msg, fromnode);
	}
	heartbeat_monitor(msg, KEEPIT, iface);
	if (fromnode == curnode) {
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			,	"Received T_SHUTDONE from us.");
		}
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			,	"Calling hb_mcp_final_shutdown"
			" in a second.");
		}
		/* Trigger next phase of final shutdown process in a second */
		Gmain_timeout_add(1000, hb_mcp_final_shutdown, NULL); /* phase 0 - normal */
	}else{
		fromnode->has_resources = FALSE;
		other_is_stable = 0;
		other_holds_resources= HB_NO_RSC;

		cl_log(LOG_INFO
		,	"Received shutdown notice from '%s'."
		,	fromnode->nodename);
		takeover_from_node(fromnode->nodename);
	}
}

void
init_resource_module(void)
{
	hb_register_msg_callback(T_SHUTDONE, HBDoMsg_T_SHUTDONE);
	hb_register_comm_up_callback(comm_up_resource_action);
}

static const char *	rsc_msg[] =	{HB_NO_RESOURCES, HB_LOCAL_RESOURCES
,	HB_FOREIGN_RESOURCES, HB_ALL_RESOURCES};

/*
 * We look at the directory /etc/ha.d/rc.d to see what
 * scripts are there to avoid trying to run anything
 * which isn't there.
 */
static GHashTable* RCScriptNames = NULL;

static void
CreateInitialFilter(void)
{
	DIR*	dp;
	struct dirent*	dep;
	static char foo[] = "bar";
	RCScriptNames = g_hash_table_new(g_str_hash, g_str_equal);

	if ((dp = opendir(HB_RC_DIR)) == NULL) {
		cl_perror("Cannot open directory " HB_RC_DIR);
		return;
	}
	while((dep = readdir(dp)) != NULL) {
		if (dep->d_name[0] == '.') {
			continue;
		}
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			,	"CreateInitialFilter: %s", dep->d_name);
		}
		g_hash_table_insert(RCScriptNames, g_strdup(dep->d_name),foo);
	}
	closedir(dp);
}
static int
FilterNotifications(const char * msgtype)
{
	int		rc;
	if (RCScriptNames == NULL) {
		CreateInitialFilter();
	}
	rc = g_hash_table_lookup(RCScriptNames, msgtype) != NULL;

	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"FilterNotifications(%s) => %d"
		,	msgtype, rc);
	}

	return rc;
}

static void
PerformAutoFailback(void)
{
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "Calling PerformAutoFailback()");
	}
	if ((procinfo->i_hold_resources & HB_FOREIGN_RSC) == 0
	||	!rsc_needs_failback || !auto_failback) {
		rsc_needs_failback = FALSE;
		shutdown_if_needed();
		return;
	}

	if (going_standby != NOT) {
		cl_log(LOG_ERR, "Auto failback ignored.");
		rsc_needs_failback = FALSE;
		return;
	}
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "Auto failback triggered.");
	}

	failback_in_progress = TRUE;
	standby_rsctype = HB_FOREIGN_RSC;
	send_standby_msg(ME);
	rsc_needs_failback = FALSE;
}

/* Notify the (external) world of an HA event */
void
notify_world(struct ha_msg * msg, const char * ostatus)
{
/*
 *	We invoke our "rc" script with the following arguments:
 *
 *	0:	RC_ARG0	(always the same)
 *	1:	lowercase version of command ("type" field)
 *
 *	All message fields get put into environment variables
 *
 *	The rc script, in turn, runs the scripts it finds in the rc.d
 *	directory (or whatever we call it... ) with the same arguments.
 *
 *	We set the following environment variables for the RC script:
 *	HA_CURHOST:	the node name we're running on
 *	HA_OSTATUS:	Status of node (before this change)
 *
 */
	struct sigaction sa;
	/* We only run one of these commands at a time */
	static char	command[STATUSLENG];
	char 		rc_arg0 [] = RC_ARG0;
	char *	const	argv[MAXFIELDS+3] = {rc_arg0, command, NULL};
	const char *	fp;
	char *		tp;
	int		pid;
#if WAITFORCOMMANDS
	int		status;
#endif

	if (!DoManageResources) {
		return;
	}

	tp = command;

	fp  = ha_msg_value(msg, F_TYPE);
	ASSERT(fp != NULL && strlen(fp) < STATUSLENG);

	if (fp == NULL || strlen(fp) >= STATUSLENG
	||	 !FilterNotifications(fp)) {
		return;
	}

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"notify_world: invoking %s: OLD status: %s"
		,	RC_ARG0,	(ostatus ? ostatus : "(none)"));
	}


	/* FIXME: No check on length of command */
	while (*fp) {
		if (isupper((unsigned int)*fp)) {
			*tp = tolower((unsigned int)*fp);
		}else{
			*tp = *fp;
		}
		++fp; ++tp;
	}
	*tp = EOS;

	switch ((pid=fork())) {

		case -1:	cl_perror("Can't fork to notify world!");
				break;


		case 0:	{	/* Child */
				int	j;
				hb_setup_child();
				set_proc_title("%s: notify_world()", cmdname);
				setpgid(0,0);
				CL_SIGACTION(SIGCHLD, NULL, &sa);
				if (sa.sa_handler != SIG_DFL) {
					cl_log(LOG_DEBUG
					,	"notify_world: setting SIGCHLD"
					" Handler to SIG_DFL");
					CL_SIGNAL(SIGCHLD,SIG_DFL);
				}
				for (j=0; j < msg->nfields; ++j) {
					char ename[64];
					snprintf(ename, sizeof(ename), "HA_%s"
					,	msg->names[j]);
					setenv(ename, msg->values[j], 1);
				}
				if (ostatus) {
					setenv(OLDSTATUS, ostatus, 1);
				}
				if (nice_failback) {
					setenv(HANICEFAILBACK, "yes", 1);
				}
				
				/*should we use logging daemon or not in script*/
				setenv(HALOGD, cl_log_get_uselogd()?
				       "yes":"no", 1);
				
				if (ANYDEBUG) {
					cl_log(LOG_DEBUG
					,	"notify_world: Running %s %s"
					,	argv[0], argv[1]);
				}
				execv(RCSCRIPT, argv);

				cl_log(LOG_ERR, "cannot exec %s", RCSCRIPT);
				cleanexit(1);
				/*NOTREACHED*/
				break;
			}


		default:	/* We're the Parent. */
				/* We run these commands at a time */
				/* So this use of "command" is OK */
				HB_RSCMGMTPROC(pid, command);
				if (ANYDEBUG) {
					cl_log(LOG_DEBUG
					,	"Starting notify process [%s]"
					,	command);
				}
	}

#if WAITFORCOMMANDS
	waitpid(pid, &status, 0);
#endif
}


/*
 * Node 'hip' has died.  Take over its resources (if any)
 * This may mean we have to STONITH them.
 */

void
hb_rsc_recover_dead_resources(struct node_info* hip)
{
	gboolean	need_stonith = TRUE;
	struct ha_msg *	hmsg;
	char		timestamp[16];
	standby_running = zero_longclock;
	going_standby	= NOT;

	
	if ((hmsg = ha_msg_new(6)) == NULL) {
		cl_log(LOG_ERR, "no memory to takeover_from_node");
		return;
	}
	
	sprintf(timestamp, TIME_X, (TIME_T) time(NULL));

	if (	ha_msg_add(hmsg, F_TYPE, T_STATUS) != HA_OK
	||	ha_msg_add(hmsg, F_SEQ, "1") != HA_OK
	||	ha_msg_add(hmsg, F_TIME, timestamp) != HA_OK
	||	ha_msg_add(hmsg, F_ORIG, hip->nodename) != HA_OK
	||	ha_msg_add(hmsg, F_STATUS, DEADSTATUS) != HA_OK) {
		cl_log(LOG_ERR, "no memory to takeover_from_node");
		ha_msg_del(hmsg);
		return;
	}
	
	if (hip->nodetype == PINGNODE_I) {
		if (ha_msg_add(hmsg, F_COMMENT, "ping") != HA_OK) {
			cl_log(LOG_ERR, "no memory to mark ping node dead");
			ha_msg_del(hmsg);
			return;
		}
	}
	
	/*deliver this message to clients*/
	heartbeat_monitor(hmsg, KEEPIT, "<internal>");
	ha_msg_del(hmsg);
	hmsg = NULL;

	if (!DoManageResources) {
		return;
	}

	if (hip->nodetype == PINGNODE_I) {
		takeover_from_node(hip->nodename);
		return;
	}
	/*
	 * We can get confused by a dead node when we're 
	 * not fully started, unless we're careful.
	 */
	if (shutdown_in_progress) {
		switch(resourcestate) {


		case HB_R_SHUTDOWN:
		case HB_R_STABLE:	return;
					
		default:	
				cl_log(LOG_ERR
				,	"recover_dead_resources()"
				" during shutdown"
				": state %d", resourcestate);
				/* FALL THROUGH! */
		case HB_R_INIT:	
		case HB_R_BOTHSTARTING:
		case HB_R_RSCRCVD:
		case HB_R_STARTING:	hb_giveup_resources();
					return;
		}
	}
	rsc_needs_failback = TRUE;

	/*
	 * If we haven't heard anything from them - they might be holding
	 * resources - we have no way of knowing.
	 */
	if (hip->anypacketsyet) {
		if (nice_failback) {
			if (other_holds_resources == HB_NO_RSC) {
				need_stonith = FALSE;
			}
		}else if (!hip->has_resources) {
			need_stonith = FALSE;
		}
	}

	if (need_stonith) {
		/* We have to Zap them before we take the resources */
		/* This often takes a few seconds. */
		if (config->stonith) {
			Initiate_Reset(config->stonith, hip->nodename, TRUE);
			/* It will call takeover_from_node() later */
			return;
		}else{
			send_stonith_msg(hip->nodename, T_STONITH_NOTCONFGD);
			cl_log(LOG_WARNING, "No STONITH device configured.");
			cl_log(LOG_WARNING, "Shared disks are not protected.");
			/* nice_failback needs us to do this anyway... */
			takeover_from_node(hip->nodename);
		}
	}else{
		cl_log(LOG_INFO, "Dead node %s gave up resources."
		,	hip->nodename);
		send_stonith_msg(hip->nodename, T_STONITH_UNNEEDED);
		if (nice_failback) {
			/* These might happen due to timing weirdnesses */
			if (! (procinfo->i_hold_resources & HB_LOCAL_RSC)){
				req_our_resources(TRUE);
			}
			if (! (procinfo->i_hold_resources & HB_FOREIGN_RSC)){
				takeover_from_node(hip->nodename);
			}
		}else{
			/* With nice_failback disabled, we always
			 * own our own (local) resources
			 */
			takeover_from_node(hip->nodename);
		}
	}
}

static gboolean
hb_rsc_isstable(void)
{
	/* Is this the "legacy" case? */
	if (!nice_failback) {
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			,	"hb_rsc_isstable"
			": ResourceMgmt_child_count: %d"
			,	ResourceMgmt_child_count);
		}
		return ResourceMgmt_child_count == 0;
	}

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"hb_rsc_isstable: ResourceMgmt_child_count: %d"
		", other_is_stable: %d"
		", takeover_in_progress: %d, going_standby: %d"
		", standby running(ms): %ld, resourcestate: %d" 
		,	ResourceMgmt_child_count, other_is_stable
		,	takeover_in_progress, going_standby
		,	longclockto_ms(standby_running)
		,	resourcestate);
	}
	/* Special case for early shutdown requests */
	if (shutdown_in_progress && resourcestate == HB_R_INIT) {
		return TRUE;
	}
	return	other_is_stable
	&&	!takeover_in_progress
	&&	going_standby == NOT
	&&	standby_running == 0L
	&&	ResourceMgmt_child_count == 0
	&&	(resourcestate == HB_R_STABLE||resourcestate==HB_R_SHUTDOWN
	||	 resourcestate == HB_R_INIT);
}


const char *
hb_rsc_resource_state(void)
{
	return (hb_rsc_isstable()
	?	decode_resources(procinfo->i_hold_resources)
	:	"transition");
}

/*
 * Here starts the nice_failback thing. The main purpouse of
 * nice_failback is to create a controlled failback. This
 * means that when the primary comes back from an outage it
 * stays quiet and acts as a secondary/backup server.
 * There are some more comments about it in nice_failback.txt
 */

/*
 * At this point nice failback deals with two nodes and is
 * an interim measure. The new version using the API is coming soon!
 *
 * This piece of code treats five different situations:
 *
 * 1. Node1 is starting and Node2 is down (or vice-versa)
 *    Take the resources. req_our_resources(), mark_node_dead()
 *
 * 2. Node1 and Node2 are starting at the same time
 *    Let both machines req_our_resources().
 *
 * 3. Node1 is starting and Node2 holds no resources
 *    Just like #2
 *
 * 4. Node1 is starting and Node2 has (his) local resources
 *    Let's ask for our local resources. req_our_resources()
 *
 * 5. Node1 is starting and Node2 has both local and foreign
 *	resources (all resources)
 *    Do nothing :)
 *
 */
/*
 * About the nice_failback resource takeover model:
 *
 * There are two principles that seem to guarantee safety:
 *
 *      1) Take all unclaimed resources if the other side is stable.
 *	      [Once you do this, you are also stable].
 *
 *      2) Take only unclaimed local resources when a timer elapses
 *		without things becoming stable by (1) above.
 *	      [Once this occurs, you're stable].
 *
 * Stable means that we have taken the resources we think we ought to, and
 * won't take any more without another transition ocurring.
 *
 * The other side is stable whenever it says it is (in its RESOURCE
 * message), or if it is dead.
 *
 * The nice thing about the stable bit in the resources message is that it
 * enables you to tell if the other side is still messing around, or if
 * they think they're done messing around.  If they're done, then it's safe
 * to proceed.  If they're not, then you need to wait until they say
 * they're done, or until a timeout occurs (because no one has become stable).
 *
 * When the timeout occurs, you're both deadlocked each waiting for the
 * other to become stable.  Then it's safe to take your local resources
 * (unless, of course, for some unknown reason, the other side has taken
 * them already).
 *
 * If a node dies die, then they'll be marked dead, and its resources will
 * be marked unclaimed.  In this case, you'll take over everything - whether
 * local resources through mark_node_dead() or remote resources through
 * mach_down.
 */

#define	HB_UPD_RSC(full, cur, up)	((full) ? up : (up == HB_NO_RSC) ? HB_NO_RSC : ((up)|(cur)))

void
comm_up_resource_action(void)
{
	static int	resources_requested_yet = 0;
	int		deadcount = countbystatus(DEADSTATUS, TRUE);


	hb_register_msg_callback(T_STARTING,		HBDoMsg_T_STARTING_or_RESOURCES);
	hb_register_msg_callback(T_RESOURCES,		HBDoMsg_T_STARTING_or_RESOURCES);
	hb_register_msg_callback(T_ASKRESOURCES,	HBDoMsg_T_ASKRESOURCES);
	hb_register_msg_callback(T_ASKRELEASE,		HBDoMsg_T_ASKRELEASE);
	hb_register_msg_callback(T_ACKRELEASE,		HBDoMsg_T_ACKRELEASE);
	hb_register_msg_callback("",			HBDoMsg_default);

	if (deadcount == 0) {
		/*
		 * If all nodes are up, we won't have to acquire
                 * anyone else's resources.  We're done with that.
		 */
		foreign_takeover_work_done = TRUE;
	}
	if (nice_failback) {
		send_local_starting();
	}else{
		/* Original ("normal") starting behavior */
		if (!WeAreRestarting && !resources_requested_yet) {
			resources_requested_yet=1;
			req_our_resources(FALSE);
		}
	}
	if (config->stonith) {
		/* This will get called every hour from now on... */
		Initiate_Reset(config->stonith, NULL, FALSE);
	}

}
static void
AnnounceTakeover(const char * reason)
{
	static gboolean		init_takeover_announced = FALSE;

	if (ANYDEBUG) {
		cl_log(LOG_INFO
		,	"AnnounceTakeover(local %d, foreign %d"
		", reason '%s' (%d))"
		,	local_takeover_work_done
		,	foreign_takeover_work_done
		,	reason
		,	init_takeover_announced);
	}
		

	if (init_takeover_announced
	||	!local_takeover_work_done || !foreign_takeover_work_done) {
		return;
	}
	cl_log(LOG_INFO, INITMSG " (%s)", reason);
	init_takeover_announced = TRUE;
}
void
process_resources(const char * type, struct ha_msg* msg
,	struct node_info * thisnode)
{

	enum hb_rsc_state	newrstate = resourcestate;
	static int			first_time = 1;

	shutdown_if_needed();
	if (!DoManageResources || !nice_failback) {
		return;
	}

	/* Otherwise, we're in the nice_failback case */

	/* This first_time switch might still be buggy -- FIXME */

	if (first_time && WeAreRestarting) {
		resourcestate = newrstate = HB_R_STABLE;
	}


	/*
	 * Deal with T_STARTING messages coming from the other side.
	 *
	 * These messages are a request for resource usage information.
	 * The appropriate reply is a T_RESOURCES message.
	 */

	 if (strcasecmp(type, T_STARTING) == 0 && (thisnode != curnode)) {

		switch(resourcestate) {

		case HB_R_RSCRCVD:
		case HB_R_STABLE:
		case HB_R_SHUTDOWN:
			break;
		case HB_R_STARTING:
			newrstate = HB_R_BOTHSTARTING;
			foreign_takeover_work_done = TRUE;
			AnnounceTakeover("HB_R_BOTHSTARTING");
			/* ??? req_our_resources(); ??? */
			break;

		default:
			cl_log(LOG_ERR, "Received '%s' message in state %d"
			,	T_STARTING, resourcestate);
			return;

		}
		other_is_stable = FALSE;
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			, "process_resources: other now unstable");
		}
		if (takeover_in_progress) {
			cl_log(LOG_WARNING
			,	"T_STARTING received during takeover.");
		}
		hb_send_resources_held(resourcestate == HB_R_STABLE, NULL);
	}

	/* Manage resource related messages... */

	if (strcasecmp(type, T_RESOURCES) == 0) {
		const char *p;
		int	fullupdate = FALSE;
		int n;
		/*
		 * There are four possible resource answers:
		 *
		 * "I don't hold any resources"			HB_NO_RSC
		 * "I hold only LOCAL resources"		HB_LOCAL_RSC
		 * "I hold only FOREIGN resources"		HB_FOREIGN_RSC
		 * "I hold ALL resources" (local+foreign)	HB_ALL_RSC
		 */

		p=ha_msg_value(msg, F_RESOURCES);
		if (p == NULL) {
			cl_log(LOG_ERR
			,	T_RESOURCES " message without " F_RESOURCES
			" field.");
			return;
		}
		n = encode_resources(p);


		if ((p = ha_msg_value(msg, F_RTYPE))
		&&	strcmp(p, "full") == 0) {
			fullupdate = TRUE;
		}

		switch (resourcestate) {

		case HB_R_BOTHSTARTING: 
		case HB_R_STARTING:	newrstate = HB_R_RSCRCVD;
					if (nice_failback
					&&	!auto_failback) {
			
						foreign_takeover_work_done
						=	TRUE;
						AnnounceTakeover
						("T_RESOURCES");
					}

		case HB_R_RSCRCVD:
		case HB_R_STABLE:
		case HB_R_SHUTDOWN:
					break;

		default:		cl_log(LOG_ERR,	T_RESOURCES
					" message received in state %d"
					,	resourcestate);
					return;
		}


		if (thisnode != curnode) {
			/*
			 * This T_RESOURCES message is from the other side.
			 */

			const char *	f_stable;

                        other_holds_resources
                        =       HB_UPD_RSC(fullupdate, other_holds_resources, n);

			/* f_stable is NULL when msg from takeover script */
			if ((f_stable = ha_msg_value(msg, F_ISSTABLE)) != NULL){
				if (strcmp(f_stable, "1") == 0) {
					if (!other_is_stable) {
						cl_log(LOG_INFO
						,	"remote resource"
						" transition completed.");
						other_is_stable = TRUE;
					 	hb_send_resources_held(resourcestate == HB_R_STABLE, NULL);	
						PerformAutoFailback();
					}
				}else{
					other_is_stable = FALSE;
					if (ANYDEBUG) {
						cl_log(LOG_DEBUG
						, "process_resources(2): %s"
						, " other now unstable");
					}
				}
			}

			if (ANYDEBUG) {
				cl_log(LOG_INFO
				,	"other_holds_resources: %d"
				,	other_holds_resources);
			}

			if ((resourcestate != HB_R_STABLE
			&&   resourcestate != HB_R_SHUTDOWN)
			&&	other_is_stable) {
				cl_log(LOG_INFO
				,	"remote resource transition completed."
				);
				req_our_resources(FALSE);
				newrstate = HB_R_STABLE;
				hb_send_resources_held(TRUE, NULL);
				PerformAutoFailback();
				foreign_takeover_work_done = TRUE;
				if (!auto_failback) {
					if (other_holds_resources
					& HB_FOREIGN_RSC) {
						local_takeover_work_done
						=	TRUE;
					}
				}
				AnnounceTakeover("T_RESOURCES(them)");
			}
		}else{	/* This message is from us... */
			const char * comment = ha_msg_value(msg, F_COMMENT);

			/*
			 * This T_RESOURCES message is from us.  It might be
			 * from the "mach_down" script or our own response to
			 * the other side's T_STARTING message.  The mach_down
			 * script sets the info (F_COMMENT) field to "mach_down"
			 * We set it to "shutdown" in giveup_resources().
			 *
			 * We do this so the audits work cleanly AND we can
			 * avoid a potential race condition.
			 *
			 * Also, we could now time how long a takeover is
			 * taking to occur, and complain if it takes "too long"
			 * 	[ whatever *that* means ]
			 */
				/* Probably unnecessary */
			procinfo->i_hold_resources
			=	HB_UPD_RSC(fullupdate
			,	procinfo->i_hold_resources, n);
			if (procinfo->i_hold_resources & HB_LOCAL_RSC) {
				/* This may sometimes be slightly premature.
				 * The problem is that if the machine has
				 * no local resources we will receive no
				 * ip-addr-resp messages for resource
				 * releases from the far side, so we
				 * have to do something to cover that case.
				 */
				local_takeover_work_done = TRUE;
				AnnounceTakeover("T_RESOURCES(us)");
			}

			if (comment) {
				if (strcmp(comment, "mach_down") == 0) {
					cl_log(LOG_INFO
					,	"mach_down takeover complete.");
					takeover_in_progress = FALSE;
					/* FYI: This also got noted earlier */
					procinfo->i_hold_resources
					|=	HB_FOREIGN_RSC;
					rsc_needs_failback = TRUE;
					other_is_stable = TRUE;
					if (ANYDEBUG) {
						cl_log(LOG_DEBUG
						, "process_resources(3): %s"
						, " other now stable");
					}
					foreign_takeover_work_done = TRUE;
					AnnounceTakeover("mach_down");
				}else if (strcmp(comment, "shutdown") == 0) {
					resourcestate = newrstate = HB_R_SHUTDOWN;
				}
			}
		}
	}
	if (strcasecmp(type, T_SHUTDONE) == 0) {
		if (thisnode != curnode) {
			/*
			 * It seems other_is_stable should be set to TRUE
			 * when we come here because the other side
			 * declared they are shutting down and no longer
			 * own any resources.
			 */
			other_is_stable = TRUE;
			other_holds_resources = HB_NO_RSC;
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				, "process_resources(4): %s"
				, " other now stable - T_SHUTDONE");
			}
			if ((procinfo->i_hold_resources != HB_ALL_RSC)
			&&	!shutdown_in_progress) {
				int	rtype;
			
				switch (procinfo->i_hold_resources) {
					case HB_FOREIGN_RSC:
						rtype = HB_LOCAL_RSC;	break;
					case HB_LOCAL_RSC:
						rtype = HB_FOREIGN_RSC;	break;
					default:
					case HB_NO_RSC:
						rtype = HB_ALL_RSC;	break;
				}
				
				/* Take over resources immediately */
				going_standby = DONE;
				go_standby(OTHER, rtype);
			}

		}else{
			resourcestate = newrstate = HB_R_SHUTDOWN;
			procinfo->i_hold_resources = 0;
		}
	}

	if (resourcestate != newrstate) {
		if (ANYDEBUG) {
			cl_log(LOG_INFO
			,	"STATE %d => %d", resourcestate, newrstate);
		}
	}

	resourcestate = newrstate;

	if (resourcestate == HB_R_RSCRCVD && local_takeover_time == 0L) {
		local_takeover_time =	add_longclock(time_longclock()
		,	secsto_longclock(RQSTDELAY));
	}

	AuditResources();
	shutdown_if_needed();
}

void
AuditResources(void)
{
	if (!nice_failback) {
		return;
	}

	/*******************************************************
	 *	Look for for duplicated or orphaned resources
	 *******************************************************/

	/*
	 *	Do both nodes own our local resources?
	 */

	if ((procinfo->i_hold_resources & HB_LOCAL_RSC) != 0
	&&	(other_holds_resources & HB_FOREIGN_RSC) != 0) {
		cl_log(LOG_ERR, "Both machines own our resources!");
	}

	/*
	 *	Do both nodes own foreign resources?
	 */

	if ((other_holds_resources & HB_LOCAL_RSC) != 0
	&&	(procinfo->i_hold_resources & HB_FOREIGN_RSC) != 0) {
		cl_log(LOG_ERR, "Both machines own foreign resources!");
	}

	/*
	 *	If things are stable, look for orphaned resources...
	 */

	if (hb_rsc_isstable() && !shutdown_in_progress
	&&	(resourcestate != HB_R_SHUTDOWN))  {
		/*
		 *	Does someone own local resources?
		 */

		if ((procinfo->i_hold_resources & HB_LOCAL_RSC) == 0
		&&	(other_holds_resources & HB_FOREIGN_RSC) == 0) {
			cl_log(LOG_ERR, "No one owns our local resources!");
		}

		/*
		 *	Does someone own foreign resources?
		 */

		if ((other_holds_resources & HB_LOCAL_RSC) == 0
		&&	(procinfo->i_hold_resources & HB_FOREIGN_RSC) == 0) {
			cl_log(LOG_ERR, "No one owns foreign resources!");
		}
	}
}

const char *
decode_resources(int i)
{
	return (i < 0 || i >= DIMOF(rsc_msg))?  "(undefined)" : rsc_msg[i];
}

int
encode_resources(const char *p)
{
	int i;

	for (i=0; i < DIMOF(rsc_msg); i++) {
		if (strcmp(rsc_msg[i], p) == 0) {
			return i;
			break;
		}
	}
	cl_log(LOG_ERR, "encode_resources: bad resource type [%s]", p);
	return 0;
}

/* Send the "I hold resources" or "I don't hold" resource messages */
int
hb_send_resources_held(int stable, const char * comment)
{
	struct ha_msg * m;
	int		rc = HA_OK;
	char		timestamp[16];
	const char *	str;

	if (!nice_failback) {
		return HA_OK;
	}
	str = rsc_msg[procinfo->i_hold_resources];
	sprintf(timestamp, TIME_X, (TIME_T) time(NULL));

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"Sending hold resources msg: %s, stable=%d # %s"
		,	str, stable, (comment ? comment : "<none>"));
	}
	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send local starting msg");
		return(HA_FAIL);
	}
	if ((ha_msg_add(m, F_TYPE, T_RESOURCES) != HA_OK)
	||  (ha_msg_add(m, F_RESOURCES, str) != HA_OK)
	||  (ha_msg_add(m, F_RTYPE, "full") != HA_OK)
	||  (ha_msg_add(m, F_ISSTABLE, (stable ? "1" : "0")) != HA_OK)) {
		cl_log(LOG_ERR, "hb_send_resources_held: Cannot create local msg");
		rc = HA_FAIL;
	}else if (comment) {
		rc = ha_msg_add(m, F_COMMENT, comment);
	}
	if (rc == HA_OK) {
		rc = send_cluster_msg(m); m = NULL;
	}else{
		ha_msg_del(m); m = NULL;
	}

	return(rc);
}


/* Send the starting msg out to the cluster */
static int
send_local_starting(void)
{
	struct ha_msg * m;
	int		rc;

	if (!nice_failback) {
		return HA_OK;
	}
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"Sending local starting msg: resourcestate = %d"
		,	resourcestate);
	}
	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send local starting msg");
		return(HA_FAIL);
	}
	if ((ha_msg_add(m, F_TYPE, T_STARTING) != HA_OK)) {
		cl_log(LOG_ERR, "send_local_starting: "
		"Cannot create local starting msg");
		rc = HA_FAIL;
		ha_msg_del(m); m = NULL;
	}else{
		rc = send_cluster_msg(m); m = NULL;
	}

	resourcestate = HB_R_STARTING;
	return(rc);
}
/* We take all resources over from a given node */
void
takeover_from_node(const char * nodename)
{
	struct node_info *	hip = lookup_node(nodename);
	struct ha_msg *	hmsg;
	char		timestamp[16];


	if (hip == 0) {
		return;
	}
	if (shutdown_in_progress) {
		cl_log(LOG_INFO
		,	"Resource takeover cancelled - shutdown in progress.");
		shutdown_if_needed();
		return;
	}else if (hip->nodetype != PINGNODE_I) {
		cl_log(LOG_INFO
		,	"Resources being acquired from %s."
		,	hip->nodename);
	}
	if ((hmsg = ha_msg_new(6)) == NULL) {
		cl_log(LOG_ERR, "no memory to takeover_from_node");
		return;
	}

	sprintf(timestamp, TIME_X, (TIME_T) time(NULL));

	if (	ha_msg_add(hmsg, F_TYPE, T_STATUS) != HA_OK
	||	ha_msg_add(hmsg, F_SEQ, "1") != HA_OK
	||	ha_msg_add(hmsg, F_TIME, timestamp) != HA_OK
	||	ha_msg_add(hmsg, F_ORIG, hip->nodename) != HA_OK
	||	ha_msg_add(hmsg, F_STATUS, DEADSTATUS) != HA_OK) {
		cl_log(LOG_ERR, "no memory to takeover_from_node");
		ha_msg_del(hmsg);
		return;
	}

	if (hip->nodetype == PINGNODE_I) {
		if (ha_msg_add(hmsg, F_COMMENT, "ping") != HA_OK) {
			cl_log(LOG_ERR, "no memory to mark ping node dead");
			ha_msg_del(hmsg);
			return;
		}
	}

	/* Sending this message triggers the "mach_down" script */
	
	/*heartbeat_monitor(hmsg, KEEPIT, "<internal>");*/
	QueueRemoteRscReq(PerformQueuedNotifyWorld, hmsg);

	/*
	 * STONITH has already successfully completed, or wasn't needed...
	 */
	if (hip->nodetype != PINGNODE_I) {
		if (nice_failback) {

			/* mach_down is out there acquiring foreign resources */
			/* So, make a note of it... */
			procinfo->i_hold_resources |= HB_FOREIGN_RSC;

			other_holds_resources = HB_NO_RSC;
			other_is_stable = TRUE;	/* Not going anywhere */
			takeover_in_progress = TRUE;
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				,	"takeover_from_node: other now stable");
			}
			/*
			 * We MUST do this now, or the other side might come
			 * back up and think they can own their own resources
			 * when we do due to receiving an interim
			 * T_RESOURCE message from us.
			 */
			/* case 1 - part 1 */
			/* part 2 is done by the mach_down script... */
		}
		/* This is here because we might not have gotten our
		 * resources yet - waiting for the other side to give them
		 * up.  Fortunately, req_our_resources() won't cause a
		 * race condition because it queues its work.
		 */
		req_our_resources(TRUE);
		/* req_our_resources turns on the HB_LOCAL_RSC bit */

	}
	hip->anypacketsyet = 1;
	ha_msg_del(hmsg);
}

void
req_our_resources(int getthemanyway)
{
	FILE *	rkeys;
	char	cmd[MAXLINE];
	char	getcmd[MAXLINE];
	char	buf[MAXLINE];
	int	finalrc = HA_OK;
	int	rc;
	int	rsc_count = 0;
	int	pid;
	int	upcount;

	if (!DoManageResources || shutdown_in_progress) {
		return;
	}

	if (nice_failback) {

		if (((other_holds_resources & HB_FOREIGN_RSC) != 0
		||	(procinfo->i_hold_resources & HB_LOCAL_RSC) != 0)
		&&	!getthemanyway) {

			if (going_standby == NOT) {
				/* Someone already owns our resources */
				cl_log(LOG_INFO
				,   "Local Resource acquisition completed"
				". (none)");
				return;
			}
		}

		/*
		 * We MUST do this now, or the other side might think they
		 * can have our resources, due to an interim T_RESOURCE
		 * message
		 */
		procinfo->i_hold_resources |= HB_LOCAL_RSC;
	}
	upcount = countbystatus(ACTIVESTATUS, TRUE);

	/* Our status update is often not done yet */
	if (strcmp(curnode->status, ACTIVESTATUS) != 0) {
		upcount++;
	}

	/* We need to fork so we can make child procs not real time */
	switch(pid=fork()) {

		case -1:	cl_log(LOG_ERR, "Cannot fork.");
				return;
		default:
				if (upcount < 2) {
					HB_RSCMGMTPROC(pid
					,	"req_our_resources");
				}else{
					HB_RSCMGMTPROC(pid
					,	"req_our_resources(ask)");
				}
				return;

		case 0:		/* Child */
				break;
	}

	hb_setup_child();
	set_proc_title("%s: req_our_resources()", cmdname);
	setpgid(0,0);
	CL_SIGNAL(SIGCHLD, SIG_DFL);
	alarm(0);
	CL_IGNORE_SIG(SIGALRM);
	CL_SIGINTERRUPT(SIGALRM, 0);
 
	/* Are we all alone in the world? */
	if (upcount < 2) {
		setenv(HADONTASK, "yes", 1);
	}

	if (nice_failback) {
		setenv(HANICEFAILBACK, "yes", 1);
	}
	sprintf(cmd, HALIB "/ResourceManager listkeys %s", curnode->nodename);
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "req_our_resources(%s)"
		,	cmd);
	}

	if ((rkeys = popen(cmd, "r")) == NULL) {
		cl_log(LOG_ERR, "Cannot run command %s", cmd);
		exit(1);
	}


	for (;;) {
		if (DEBUGDETAILS) {
			cl_log(LOG_DEBUG, "req_our_resources() before fgets()");
		}
		errno = 0;
		if (fgets(buf, MAXLINE, rkeys) == NULL) {
			if (DEBUGDETAILS) {
				cl_log(LOG_DEBUG
				,	"req_our_resources() fgets => NULL");
			}
			if (ferror(rkeys)) {
				cl_perror("req_our_resources: fgets failure");
			}
			break;
		}
		++rsc_count;

		if (buf[strlen(buf)-1] == '\n') {
			buf[strlen(buf)-1] = EOS;
		}
		sprintf(getcmd, HALIB "/req_resource %s", buf);
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "req_our_resources()"
			": running [%s]",	getcmd);
		}
		/*should we use logging daemon or not in script*/
		setenv(HALOGD, cl_log_get_uselogd()?
		       "yes":"no", 1);				

		if ((rc=system(getcmd)) != 0) {
			cl_perror("%s returned %d", getcmd, rc);
			finalrc=HA_FAIL;
		}
	}
	rc=pclose(rkeys);
	rkeys = NULL;
	if (rc < 0 && errno != ECHILD) {
		cl_perror("pclose(%s) returned %d", cmd, rc);
	}else if (rc > 0) {
		cl_log(LOG_ERR, "[%s] exited with 0x%x", cmd, rc);
	}

	if (rsc_count == 0) {
		cl_log(LOG_INFO, "No local resources [%s] to acquire.", cmd);
	}else{
		if (ANYDEBUG) {
			cl_log(LOG_INFO, "%d local resources from [%s]"
			,	rsc_count, cmd);
		}
		cl_log(LOG_INFO, "Local Resource acquisition completed.");
	}
	hb_send_resources_held(TRUE, "req_our_resources()");
	exit(0);
}

/* Send "standby" related msgs out to the cluster */
static int
send_standby_msg(enum standby state)
{
	const char * standby_msg[] = { "not", "me", "other", "done"};
	struct ha_msg * m;
	int		rc;
	char		timestamp[16];

	sprintf(timestamp, TIME_X, (TIME_T) time(NULL));

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "Sending standby [%s] msg"
		,			standby_msg[state]);
	}
	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send standby [%s] msg"
		,			standby_msg[state]);
		return(HA_FAIL);
	}
	if (ha_msg_add(m, F_TYPE, T_ASKRESOURCES) != HA_OK
	||  ha_msg_add(m, F_RESOURCES, decode_resources(standby_rsctype))
	!= HA_OK
	||  ha_msg_add(m, F_COMMENT, standby_msg[state]) != HA_OK) {
		cl_log(LOG_ERR, "send_standby_msg: "
		"Cannot create standby reply msg");
		rc = HA_FAIL;
		ha_msg_del(m); m = NULL;
	}else{
		rc = send_cluster_msg(m); m = NULL;
	}

	return(rc);
}

void
send_stonith_msg(const char *nodename, const char *result)
{
	struct ha_msg*	hmsg;

	if ((hmsg = ha_msg_new(6)) == NULL) {
		cl_log(LOG_ERR, "no memory for " T_STONITH);
	}

	if (	hmsg != NULL
	&& 	ha_msg_add(hmsg, F_TYPE, T_STONITH)    == HA_OK
	&&	ha_msg_add(hmsg, F_NODE, nodename) == HA_OK
	&&	ha_msg_add(hmsg, F_APIRESULT, result) == HA_OK) {
		if (send_cluster_msg(hmsg) != HA_OK) {
			cl_log(LOG_ERR, "cannot send " T_STONITH
			" request for %s", nodename);
		}
		hmsg = NULL;
	}else{
		cl_log(LOG_ERR
		,	"Cannot send reset reply message [%s] for %s", result
		,	nodename);
		if (hmsg != NULL) {
			ha_msg_del(hmsg);
			hmsg = NULL;
		}
	}
	return;
}

#define	STANDBY_INIT_TO_MS	10000L		/* ms timeout for initial reply */
#define	HB_STANDBY_RSC_TO_MS	60L*(60L*1000L)	/* resource handling timeout */
						/* (An hour in ms)*/

void
ask_for_resources(struct ha_msg *msg)
{

	const char *	info;
	const char *	from;
	int 		msgfromme;
	longclock_t 	now = time_longclock();
	int		message_ignored = 0;
	const enum standby	orig_standby = going_standby;
	const longclock_t	standby_rsc_to
	=			msto_longclock(HB_STANDBY_RSC_TO_MS);
	const longclock_t	init_to =  msto_longclock(STANDBY_INIT_TO_MS);
	const char *	rsctype;
	int		rtype;

	if (!nice_failback) {
		cl_log(LOG_INFO
		,	"Standby mode only implemented when nice_failback on");
		return;
	}
	if (resourcestate == HB_R_SHUTDOWN) {
		if (ANYDEBUG){
			cl_log(LOG_DEBUG
			,	"standby message ignored during shutdown");
		}
		return;
	}
	info = ha_msg_value(msg, F_COMMENT);
	from = ha_msg_value(msg, F_ORIG);
	rsctype=ha_msg_value(msg, F_RESOURCES);
	if (rsctype == NULL) {
		rtype = HB_ALL_RSC;
	}else{
		rtype = encode_resources(rsctype);
	}


	if (info == NULL || from == NULL) {
		cl_log(LOG_ERR, "Received standby message without info/from");
		return;
	}
	msgfromme = strcmp(from, curnode->nodename) == 0;

	if (ANYDEBUG){
		cl_log(LOG_DEBUG
		,	"Received standby message %s from %s in state %d "
		,	info, from, going_standby);
	}

	if (cmp_longclock(standby_running, zero_longclock) != 0
	&&	cmp_longclock(now, standby_running) < 0
	&&	strcasecmp(info, "me") == 0) {
		unsigned long	secs_left;

		secs_left = longclockto_ms(sub_longclock(standby_running, now));

		secs_left = (secs_left+999)/1000;

		cl_log(LOG_WARNING
		,	"Standby in progress"
		"- new request from %s ignored [%ld seconds left]"
		,	from, secs_left);
		return;
	}


	/* Starting the STANDBY 3-phased protocol */

	switch(going_standby) {
	case NOT:
		if (!other_is_stable) {
			cl_log(LOG_WARNING, "standby message [%s] from %s"
			" ignored.  Other side is in flux.", info, from);
			return;
		}
		if (resourcestate != HB_R_STABLE) {
			cl_log(LOG_WARNING, "standby message [%s] from %s"
			" ignored.  local resources in flux.", info, from);
			return;
		}
		standby_rsctype = rtype;
		if (strcasecmp(info, "me") == 0) {

			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				, "ask_for_resources: other now unstable");
			}
			other_is_stable = FALSE;
			cl_log(LOG_INFO, "%s wants to go standby [%s]"
			,	from, decode_resources(rtype));
			if (msgfromme) {
				/* We want to go standby */
				if (ANYDEBUG) {
					cl_log(LOG_INFO
					,	"i_hold_resources: %d"
					,	procinfo->i_hold_resources);
				}
				standby_running = add_longclock(now, init_to);
				going_standby = ME;
			}else{
				if (ANYDEBUG) {
					cl_log(LOG_INFO
					,	"standby"
					": other_holds_resources: %d"
					,	other_holds_resources);
				}
				/* Other node wants to go standby */
				going_standby = OTHER;
				send_standby_msg(going_standby);
				standby_running = add_longclock(now
				,	standby_rsc_to);
			}
		}else{
			message_ignored = 1;
		}
		break;

	case ME:
		/* Other node is alive, so give up our resources */
		if (!msgfromme) {
			standby_rsctype = rtype;
			standby_running = add_longclock(now, standby_rsc_to);
			if (strcasecmp(info,"other") == 0) {
				cl_log(LOG_INFO
				,	"standby: %s can take our %s resources"
				,	from, decode_resources(rtype));
				go_standby(ME, rtype);
				/* Our child proc sends a "done" message */
				/* after all the resources are released	*/
			}else{
				message_ignored = 1;
			}
		}else if (strcasecmp(info, "done") == 0) {
			/*
			 * The "done" message came from our child process
			 * indicating resources are completely released now.
			 */
			cl_log(LOG_INFO
			,	"Local standby process completed [%s]."
			,	decode_resources(rtype));
			going_standby = DONE;
			procinfo->i_hold_resources &= ~standby_rsctype;
			standby_running = add_longclock(now, standby_rsc_to);
		}else{
			message_ignored = 1;
		}
		break;

	case OTHER:
		standby_rsctype = rtype;
		if (strcasecmp(info, "done") == 0) {
			standby_running = add_longclock(now, standby_rsc_to);
			if (!msgfromme) {
				/* It's time to acquire resources */

				cl_log(LOG_INFO
				,	"standby: acquire [%s] resources"
				" from %s"
				,	decode_resources(rtype), from);
				/* go_standby gets requested resources */
				go_standby(OTHER, standby_rsctype);
				going_standby = DONE;
			}else{
				message_ignored = 1;
			}
		}else if (!msgfromme || strcasecmp(info, "other") != 0) {
			/* We expect an "other" message from us */
			/* But, that's not what this one is ;-) */
			message_ignored = 1;
		}
		break;

	case DONE:
		standby_rsctype = rtype;
		if (strcmp(info, "done")== 0) {
			standby_running = zero_longclock;
			going_standby = NOT;
			if (msgfromme) {
				int	rup = HB_NO_RSC;
				cl_log(LOG_INFO
				,	"Standby resource"
				" acquisition done [%s]."
				,	decode_resources(rtype));
				if (auto_failback) {
					local_takeover_work_done = TRUE;
					AnnounceTakeover("auto_failback");
				}
				switch(rtype) {
				case HB_LOCAL_RSC:	rup=HB_FOREIGN_RSC;
							break;
				case HB_FOREIGN_RSC:	rup=HB_LOCAL_RSC;
							break;
				case HB_ALL_RSC:	rup=HB_ALL_RSC;
							break;
				}

				procinfo->i_hold_resources |= rup;
			}else{
				cl_log(LOG_INFO
				,	"Other node completed standby"
				" takeover of %s resources."
				,	decode_resources(rtype));
			}
			hb_send_resources_held(TRUE, NULL);
			going_standby = NOT;
		}else{
			message_ignored = 1;
		}
		break;
	}
	if (message_ignored){
		cl_log(LOG_ERR
		,	"Ignored standby message '%s' from %s in state %d"
		,	info, from, orig_standby);
	}
	if (ANYDEBUG) {
		cl_log(LOG_INFO, "New standby state: %d", going_standby);
	}
	shutdown_if_needed();
}

static int
countbystatus(const char * status, int matchornot)
{
	int	count = 0;
	int	matches;
	int	j;

	matchornot = (matchornot ? TRUE : FALSE);

	for (j=0; j < config->nodecount; ++j) {
		if (config->nodes[j].nodetype == PINGNODE_I) {
			continue;
		}
		matches = (strcmp(config->nodes[j].status, status) == 0);
		if (matches == matchornot) {
			++count;
		}
	}
	return count;
}


static void
go_standby(enum standby who, int resourceset) /* Which resources to give up */
{
	FILE *			rkeys;
	char			cmd[MAXLINE];
	char			buf[MAXLINE];
	int			finalrc = HA_OK;
	int			rc = 0;
	pid_t			pid;
	int			actresources;	/* Resources to act on */
	const char *		querycmd = "allkeys";

#define	ACTION_ACQUIRE	0
#define	ACTION_GIVEUP	1
	int			action;		/* Action to take */
	static const char*	actionnames[2] = {"acquire", "give up"};
	static const char*	actioncmds [2] = {"takegroup", "givegroup"};

	/*
	 * We consider them unstable because they're about to pick up
	 * our resources.
	 */
	if (who == ME) {
		other_is_stable = FALSE;
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "go_standby: other is unstable");
		}
		/* Make sure they know what we're doing and that we're
		 * not done yet (not stable)
		 * Since heartbeat doesn't guarantee message ordering
		 * this could theoretically have problems, but all that
		 * happens if it gets out of order is that we get
		 * a funky warning message (or maybe two).
		 */
		procinfo->i_hold_resources &= ~resourceset;
		hb_send_resources_held(FALSE, "standby");
		action = ACTION_GIVEUP;
	}else{
		action = ACTION_ACQUIRE;
	}

	/* We need to fork so we can make child procs not real time */

	switch((pid=fork())) {

		case -1:	cl_log(LOG_ERR, "Cannot fork.");
				return;

				/*
				 * We can't block here, because then we
				 * aren't sending heartbeats out...
				 */
		default:	
				HB_RSCMGMTPROC(pid, "go_standby");
				return;

		case 0:		/* Child */
				break;
	}

	hb_setup_child();
	setpgid(0,0);
	CL_SIGNAL(SIGCHLD, SIG_DFL);



	/* Figure out which resources to inquire about */
	switch(resourceset) {

		case HB_FOREIGN_RSC:	
		actresources = (who == ME ? HB_FOREIGN_RSC : HB_LOCAL_RSC);
		break;

		case HB_LOCAL_RSC:
		actresources = (who == ME ? HB_LOCAL_RSC : HB_FOREIGN_RSC);
		break;

		case HB_ALL_RSC:
		actresources = HB_ALL_RSC;
		break;

		default:
			cl_log(LOG_ERR, "no resources to %s"
			,	actionnames[action]);
			exit(10);
	}

	/* Figure out what command to issue to get resource list... */
	switch (actresources) {
		case HB_FOREIGN_RSC:
			querycmd =  "otherkeys";
			break;
		case HB_LOCAL_RSC:
			querycmd =  "ourkeys";
			break;
		case HB_ALL_RSC:
			querycmd =  "allkeys";
			break;
	}

	cl_log(LOG_INFO
	,	"%s %s HA resources (standby)."
	,	actionnames[action]
	,	rsc_msg[actresources]);

	if (ANYDEBUG) {
		cl_log(LOG_INFO, "go_standby: who: %d resource set: %s"
		,	who, rsc_msg[actresources]);
		cl_log(LOG_INFO, "go_standby: (query/action): (%s/%s)"
		,	querycmd, actioncmds[action]);
	}

	/*
	 *	We could do this ourselves fairly easily...
	 */

	sprintf(cmd, HALIB "/ResourceManager %s", querycmd);

	if ((rkeys = popen(cmd, "r")) == NULL) {
		cl_log(LOG_ERR, "Cannot run command %s", cmd);
		return;
	}

	while (fgets(buf, MAXLINE, rkeys) != NULL) {
		if (buf[strlen(buf)-1] == '\n') {
			buf[strlen(buf)-1] = EOS;
		}
		sprintf(cmd, HALIB "/ResourceManager %s %s"
		,	actioncmds[action], buf);

		/*should we use logging daemon or not in script*/
		setenv(HALOGD, cl_log_get_uselogd()?
		       "yes":"no", 1);
		
		if ((rc=system(cmd)) != 0) {
			cl_log(LOG_ERR, "%s returned %d", cmd, rc);
			finalrc=HA_FAIL;
		}
	}
	pclose(rkeys);
	cl_log(LOG_INFO, "%s HA resource %s completed (standby)."
	,	rsc_msg[actresources]
	,	action == ACTION_ACQUIRE ? "acquisition" : "release");

	send_standby_msg(DONE);
	exit(rc);

}

static void
shutdown_if_needed(void)
{
	if (rsc_needs_shutdown) {
		hb_giveup_resources();
	}
}

/*
 * This is the first part of the graceful shutdown process
 *
 * We cannot shut down right now if resource actions are pending...
 *
 * Examples:
 *   - initial resource acquisition
 *   - hb_standby in progress
 *   - req_our_resources() in progress
 *   - notify_world() in progress
 *
 *   All these ideas are encapsulated by hb_rsc_isstable()
 */
void
hb_giveup_resources(void)
{
	FILE *		rkeys;
	char		cmd[MAXLINE];
	char		buf[MAXLINE];
	int		finalrc = HA_OK;
	int		rc;
	pid_t		pid;
	struct ha_msg *	m;
	static int	resource_shutdown_in_progress = FALSE;
	
	if (!DoManageResources){
		if (!shutdown_in_progress) {
			hb_initiate_shutdown(FALSE);
		}
		return;
	}

	if (!hb_rsc_isstable()) {
		/* Try again later... */
		/* (through shutdown_if_needed()) */
		if (!rsc_needs_shutdown) {
			cl_log(LOG_WARNING
			,	"Shutdown delayed until current"
			" resource activity finishes.");
			rsc_needs_shutdown = TRUE;
		}
		return;
	}
	rsc_needs_shutdown = FALSE;
	shutdown_in_progress = TRUE;


	if (resource_shutdown_in_progress) {
		cl_log(LOG_INFO, "Heartbeat shutdown already underway.");
		return;
	}
	resource_shutdown_in_progress = TRUE;
	if (ANYDEBUG) {
		cl_log(LOG_INFO, "hb_giveup_resources(): "
			"current status: %s", curnode->status);
	}
	hb_close_watchdog();
	DisableProcLogging();	/* We're shutting down */
	procinfo->i_hold_resources = HB_NO_RSC ;
	resourcestate = HB_R_SHUTDOWN; /* or we'll get a whiny little comment
				out of the resource management code */
	if (nice_failback) {
		hb_send_resources_held(FALSE, "shutdown");
	}
	cl_log(LOG_INFO, "Heartbeat shutdown in progress. (%d)"
	,	(int) getpid());

	/* We need to fork so we can make child procs not real time */

	switch((pid=fork())) {

		case -1:	cl_log(LOG_ERR, "Cannot fork.");
				return;

		default:
				HB_RSCMGMTPROC(pid
				,	"hb_giveup_resources");
				return;

		case 0:		/* Child */
				break;
	}

	hb_setup_child();
	setpgid(0,0);
	set_proc_title("%s: hb_signal_giveup_resources()", cmdname);

	/* We don't want to be interrupted while shutting down */

	CL_SIGNAL(SIGCHLD, SIG_DFL);
	CL_SIGINTERRUPT(SIGCHLD, 0);

	alarm(0);
	CL_IGNORE_SIG(SIGALRM);
	CL_SIGINTERRUPT(SIGALRM, 0);

	CL_IGNORE_SIG(SIGTERM);
	/* CL_SIGINTERRUPT(SIGTERM, 0); */

	cl_log(LOG_INFO, "Giving up all HA resources.");
	/*
	 *	We could do this ourselves fairly easily...
	 */

	sprintf(cmd, HALIB "/ResourceManager listkeys '.*'");

	if ((rkeys = popen(cmd, "r")) == NULL) {
		cl_log(LOG_ERR, "Cannot run command %s", cmd);
		exit(1);
	}

	while (fgets(buf, MAXLINE, rkeys) != NULL) {
		if (buf[strlen(buf)-1] == '\n') {
			buf[strlen(buf)-1] = EOS;
		}

		/*should we use logging daemon or not in script*/
		setenv(HALOGD, cl_log_get_uselogd()?
		       "yes":"no", 1);
		
		sprintf(cmd, HALIB "/ResourceManager givegroup %s", buf);
		if ((rc=system(cmd)) != 0) {
			cl_log(LOG_ERR, "%s returned %d", cmd, rc);
			finalrc=HA_FAIL;
		}
	}
	pclose(rkeys);

	cl_log(LOG_INFO, "All HA resources relinquished.");

	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send final shutdown msg");
		exit(1);
	}
	if ((ha_msg_add(m, F_TYPE, T_SHUTDONE) != HA_OK
	||	ha_msg_add(m, F_STATUS, DEADSTATUS) != HA_OK)) {
		cl_log(LOG_ERR, "hb_signal_giveup_resources: "
			"Cannot create local msg");
		ha_msg_del(m);
	}else{
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "Sending T_SHUTDONE.");
		}
		rc = send_cluster_msg(m); m = NULL;
	}

	exit(0);
}


void
Initiate_Reset(Stonith* s, const char * nodename, gboolean doreset)
{
	const char*	result = "bad";
	int		pid;
	int		exitcode = 0;
	struct StonithProcHelper *	h;
	int		rc;
	ProcTrack_ops * track;
	/*
	 * We need to fork because the stonith operations block for a long
	 * time (10 seconds in common cases)
	 */
	track = (doreset ?  &StonithProcessTrackOps : &StonithStatProcessTrackOps);
	switch((pid=fork())) {

		case -1:	cl_log(LOG_ERR, "Cannot fork.");
				return;
		default:
				h = g_new(struct StonithProcHelper, 1);
				h->nodename = g_strdup(nodename);
				NewTrackedProc(pid, 1, PT_LOGVERBOSE, h, track);
				/* StonithProcessDied is called when done */
				return;

		case 0:		/* Child */
				break;

	}
	/* Guard against possibly hanging Stonith code, etc... */
	hb_setup_child();
	setpgid(0,0);
	set_proc_title("%s: Initiate_Reset()", cmdname);
	CL_SIGNAL(SIGCHLD,SIG_DFL);

	if (doreset) {
		cl_log(LOG_INFO
		,	"Resetting node %s with [%s]"
		,	nodename
		,	stonith_get_info(s, ST_DEVICEID));
	}else{
		cl_log(LOG_INFO
		,	"Checking status of STONITH device [%s]"
		,	stonith_get_info(s, ST_DEVICEID));
	}

	if (doreset) {
		rc = stonith_req_reset(s, ST_GENERIC_RESET, nodename);
	}else{
		rc = stonith_get_status(s);
	}
	switch (rc) {

	case S_OK:
		result=T_STONITH_OK;
		if (doreset) {
			cl_log(LOG_INFO
			,	"node %s now reset.", nodename);
		}
		exitcode = 0;
		break;

	case S_BADHOST:
		cl_log(LOG_ERR
		,	"Device %s cannot reset host %s."
		,	stonith_get_info(s, ST_DEVICEID)
		,	nodename);
		exitcode = 100;
		result = T_STONITH_BADHOST;
		break;

	default:
		if (doreset) {
			cl_log(LOG_ERR, "Host %s not reset!", nodename);
		}else{
			cl_log(LOG_ERR, "STONITH device %s not operational!"
			,	stonith_get_info(s, ST_DEVICEID));
		}
		exitcode = 1;
		result = T_STONITH_BAD;
	}

	if (doreset) {	
		send_stonith_msg(nodename, result);
	}
	exit (exitcode);
}


static void
RscMgmtProcessRegistered(ProcTrack* p)
{
	ResourceMgmt_child_count ++;
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "Process [%s] started pid %d"
		,	p->ops->proctype(p)
		,	p->pid
		);
	}
}
/* Handle the death of a resource management process */
static void
RscMgmtProcessDied(ProcTrack* p, int status, int signo, int exitcode
,	int waslogged)
{
	const char *	pname = RscMgmtProcessName(p);
	ResourceMgmt_child_count --;

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "RscMgmtProc '%s' exited code %d"
		,	pname, exitcode);
	}
	if (strcmp(pname, "req_our_resources") == 0
	||	 strcmp(pname, "ip-request-resp") == 0) {
		local_takeover_work_done = TRUE;
		AnnounceTakeover(pname);
	}else if (!nice_failback && strcmp(pname, "status") == 0) {
		int	deadcount = countbystatus(DEADSTATUS, TRUE);
		if (deadcount > 0) {
			/* Must be our partner is dead...
			 * Status would have invoked mach_down
			 * and now all their resource are belong to us
			 */
			foreign_takeover_work_done = TRUE;
			AnnounceTakeover(pname);
		}
	}

	p->privatedata = NULL;
	StartNextRemoteRscReq();
	shutdown_if_needed();
}

static const char *
RscMgmtProcessName(ProcTrack* p)
{
	struct hb_const_string * s = p->privatedata;

	return (s && s->str ? s->str : "heartbeat resource child");
}

/***********************************************************************
 *
 * RemoteRscRequests are resource management requests from other nodes
 *
 * Our "privatedata" is a GHook.  This GHook points back to the
 * queue entry for this object. Its "data" element points to the message
 * which we want to give to the function which the hook points to...
 * QueueRemoteRscReq is the function which sets up the hook, then queues
 * it for later execution.
 *
 * StartNextRemoteRscReq() is the function which runs the hook,
 * when the time is right.  Basically, we won't run the hook if any
 * other asynchronous resource management operations are going on.
 * This solves the problem of a remote request coming in and conflicting
 * with a different local resource management request.  It delays
 * it until the local startup/takeover/etc. operations are complete.
 * At this time, it has a clear picture of what's going on, and
 * can safely do its thing.
 *
 * So, we queue the job to do in a Ghook.  When the Ghook runs, it
 * will create a ProcTrack object to track the completion of the process.
 *
 * When the process completes, it will clean up the ProcTrack, which in
 * turn will remove the GHook from the queue, destroying it and the
 * associated struct ha_msg* from the original message.
 *
 ***********************************************************************/

static GHookList	RemoteRscReqQueue = {0,0,0};
static GHook*		RunningRemoteRscReq = NULL;

/* Initialized the remote resource request queue */
static void
InitRemoteRscReqQueue(void)
{
	if (RemoteRscReqQueue.is_setup) {
		return;
	}
	g_hook_list_init(&RemoteRscReqQueue, sizeof(GHook));
}

/* Queue a remote resource request */
void
QueueRemoteRscReq(RemoteRscReqFunc func, struct ha_msg* msg)
{
	GHook*		hook;
	const char *	fp;

	if (!DoManageResources) {
		return;
	}
	InitRemoteRscReqQueue();
	hook = g_hook_alloc(&RemoteRscReqQueue);

	fp  = ha_msg_value(msg, F_TYPE);

	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"Queueing remote resource request (hook = 0x%p) %s"
		,	(void *)hook, fp);
		cl_log_message(LOG_DEBUG, msg);
	}

	if (fp == NULL || !FilterNotifications(fp)) {
		if (DEBUGDETAILS) {
			cl_log(LOG_DEBUG
			,	"%s: child process unneeded.", fp);
			cl_log_message(LOG_DEBUG, msg);
		}
		g_hook_free(&RemoteRscReqQueue, hook);
		return;
	}

	hook->func = func;
	hook->data = ha_msg_copy(msg);
	hook->destroy = (GDestroyNotify)(ha_msg_del);
	g_hook_append(&RemoteRscReqQueue, hook);
	StartNextRemoteRscReq();
}

/* If the time is right, start the next remote resource request */
static void
StartNextRemoteRscReq(void)
{
	GHook*		hook;
	RemoteRscReqFunc	func;

	/* We can only run one of these at a time... */
	if (ResourceMgmt_child_count != 0) {
		cl_log(LOG_DEBUG, "StartNextRemoteRscReq(): child count %d"
		,	ResourceMgmt_child_count);
		return;
	}

	RunningRemoteRscReq = NULL;

	/* Run the first hook in the list... */

	hook = g_hook_first_valid(&RemoteRscReqQueue, FALSE);
	if (hook == NULL) {
		ResourceMgmt_child_count = 0;
		shutdown_if_needed();
		return;
	}

	RunningRemoteRscReq = hook;
	func = hook->func;

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "StartNextRemoteRscReq() - calling hook");
	}
	/* Call the hook... */
	func(hook);
	g_hook_destroy_link(&RemoteRscReqQueue, hook);
	g_hook_unref(&RemoteRscReqQueue, hook);
}


/*
 * Perform a queued notify_world() call
 *
 * The Ghook and message are automatically destroyed by our
 * caller.
 */

void
PerformQueuedNotifyWorld(GHook* hook)
{
	struct ha_msg* m = hook->data;
	/*
	 * We have been asked to run a notify_world() which
	 * we would like to have done earlier...
	 */
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG, "PerformQueuedNotifyWorld() msg follows");
		cl_log_message(LOG_DEBUG, m);
	}
	notify_world(m, curnode->status);
	/* "m" is automatically destroyed when "hook" is */
}

static gboolean
StonithProc(gpointer gph)
{
	struct StonithProcHelper* h	= gph;
	Initiate_Reset(config->stonith, h->nodename, TRUE);
	return FALSE;
}

/* Handle the death of a STONITH process */
static void
StonithProcessDied(ProcTrack* p, int status, int signo, int exitcode, int waslogged)
{
	struct StonithProcHelper*	h = p->privatedata;

	if (signo != 0 || exitcode != 0) {
		cl_log(LOG_ERR, "STONITH of %s failed.  Retrying..."
		,	h->nodename);

		Gmain_timeout_add(5*1000, StonithProc, h);
		/* Don't free 'h' - we still need it */
		p->privatedata = NULL;
		return;
	}else{
		/* We need to finish taking over the other side's resources */
		takeover_from_node(h->nodename);
	}
	g_free(h->nodename);	h->nodename=NULL;
	g_free(p->privatedata);	p->privatedata = NULL;
}

static const char *
StonithProcessName(ProcTrack* p)
{
	static char buf[100];
	struct StonithProcHelper *	h = p->privatedata;
	snprintf(buf, sizeof(buf), "STONITH %s", h->nodename);
	return buf;
}

static gboolean
StonithStatProc(gpointer dummy)
{
	Initiate_Reset(config->stonith, "?", FALSE);
	return FALSE;
}
static void
StonithStatProcessDied(ProcTrack* p, int status, int signo, int exitcode, int waslogged)
{
	struct StonithProcHelper*	h = p->privatedata;

	if ((signo != 0 && signo != SIGTERM) || exitcode != 0) {
		cl_log(LOG_ERR, "STONITH status operation failed.");
		cl_log(LOG_INFO, "This may mean that the STONITH device has failed!");
	}
	g_free(h->nodename);	h->nodename=NULL;
	g_free(p->privatedata);	p->privatedata = NULL;
	Gmain_timeout_add(3600*1000, StonithStatProc, NULL);
}

static const char *
StonithStatProcessName(ProcTrack* p)
{
	static char buf[100];
	snprintf(buf, sizeof(buf), "STONITH-stat");
	return buf;
}


/*
 * $Log: hb_resource.c,v $
 * Revision 1.75  2005/04/13 18:04:46  gshi
 * bug 442:
 *
 * Enable logging daemon  by default
 * use static variables in cl_log and export interfaces to get/set variables
 *
 * Revision 1.74  2005/03/28 15:41:49  alan
 * BUG 49: Some of the STONITH logic (and other things) shouldn't be
 * executed if heartbeat isn't managing resources.
 *
 * Revision 1.73  2005/03/03 16:22:07  andrew
 * Fix from gshi for bug #302
 *  - seems to work well
 *
 * Revision 1.72  2005/02/28 21:23:30  gshi
 * giveup_resources() servers as two purposes in 1.x series:
 * 1) give up all resources
 * 2) shutdown heartbeat
 *
 * since in V2 all resources are managed in crm/lrm, this function
 * should not do any thing with the first purpose,therefore it should
 * start shutdown phase if it is in version 2
 *
 * Revision 1.71  2005/01/18 20:33:03  andrew
 * Appologies for the top-level commit, one change necessitated another which
 *   exposed some bugs... etc etc
 *
 * Remove redundant usage of XML in the CRM
 * - switch to "struct ha_msg" aka. HA_Message for everything except data
 * Make sure the expected type of all FSA input data is verified before processing
 * Fix a number of bugs including
 * - looking in the wrong place for the API result data in the CIB API
 *   (hideous that this actually worked).
 * - not overwriting error codes when sending the result to the client in the CIB API
 *   (this lead to some error cases being treated as successes later in the code)
 * Add PID to log messages sent to files (not to syslog)
 * Add a log level to calls for cl_log_message()
 * - convert existing calls, sorry if I got the level wrong
 * Add some checks in cl_msg.c code to prevent NULL pointer exceptions
 * - usually when NULL is passed to strlen() or similar
 *
 * Revision 1.70  2005/01/03 18:12:10  alan
 * Stonith version 2.
 * Some compatibility with old versions is still missing.
 * Basically, you can't (yet) use files for config information.
 * But, we'll fix that :-).
 * Right now ssh, null and baytech all work.
 *
 * Revision 1.69  2004/11/17 21:00:14  msoffen
 * Moved initialization of standby_running and going_standby to compile on BSD
 *
 * Revision 1.68  2004/11/16 05:47:23  zhenh
 * 1.Make the ordering shutdown work. 2.Move HBDoMsg_T_SHUTDONE() to hb_resource.c
 *
 * Revision 1.67  2004/11/11 20:27:03  gshi
 * node dead notice should be called after the node is really decared dead
 * instead of after receving a shutdown message from it
 *
 * Revision 1.66  2004/11/08 20:48:36  gshi
 * implemented logging daemon
 *
 * The logging daemon is to double-buffer log messages to protect us from blocking
 * writes to syslog / logfiles.
 *
 * Revision 1.65  2004/10/20 15:31:00  alan
 * Fixed a bug in STONITH retries where it frees something
 * prematurely.
 *
 * Revision 1.64  2004/09/10 22:47:40  alan
 * BEAM FIXES:  various minor fixes related to running out of memory.
 *
 * Revision 1.63  2004/09/10 07:23:50  alan
 * BEAM fixes:  Fixed a couple of resource leaks, and a couple of use-after-free errors.
 *
 * Revision 1.62  2004/09/10 01:12:23  alan
 * BEAM CHANGES: Fixed a couple of very minor bugs, and cleaned up some BEAM warnings.
 *
 * Revision 1.61  2004/09/08 22:19:42  alan
 * Bringing forward changes from the 1.2.x branch.
 *
 * Revision 1.60  2004/09/08 18:57:30  gshi
 * fixed a bug: api_test cannot receive messages
 * the default handler gets all messages
 * we need to check if any client need that message in the default handler
 *
 * Revision 1.59  2004/08/14 14:42:22  alan
 * Put in lots of comments about resource-work tieins in heartbeat.c, plus put in
 * a short term workaround for dealing with one particular case:
 * req_our_resources().
 *
 * Revision 1.58  2004/08/10 04:55:24  alan
 * Completed first pass of -M flag reorganization.
 * It passes BasicSanityCheck.
 * But, I haven't actually tried -M yet ;-)
 *
 * Revision 1.57  2004/07/02 03:34:21  sunjd
 * Fix cannot stop two nodes simultaneously, pls help to test more
 *
 * Revision 1.56  2004/05/17 15:12:08  lars
 * Reverting over-eager approach to disabling old resource manager code.
 *
 * Revision 1.55  2004/05/15 09:28:08  andrew
 * Disable ALL legacy resource management iff configured with --enable-crm
 * Possibly I have been a little over-zealous but likely the feature(s)
 *  would need to be re-written to use the new design anyway.
 *
 * Revision 1.54  2004/04/27 18:29:03  gshi
 * send a stability message back to the sender
 * unon recving that kind of message
 * this is to fix a bug that after a node re-read config file and restart
 * the other node will think that node is unstable.
 *
 * Revision 1.53  2004/04/14 00:31:00  alan
 * Added to code to check STONITH device every hour.
 *
 * Revision 1.52  2004/04/08 20:53:44  alan
 * Put in code to make STONITH repeat after 1 second's delay - instead of continually...
 *
 * Revision 1.51  2004/03/30 19:38:48  alan
 * Applied a fix for a bug.  Fix supplied by
 * Kurosawa Takahiro <kurosawa@valinux.co.jp>.
 *
 * Revision 1.50  2004/03/25 07:55:39  alan
 * Moved heartbeat libraries to the lib directory.
 *
 * Revision 1.49  2004/02/17 22:11:57  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.48  2004/02/13 17:57:31  alan
 * Changed the code so in comm_up_resource_action, so that when
 * no other nodes are down when we first start up, that we
 * note that foreign resource acquisition is complete.
 * This will make the CTS tests happier, since otherwise
 * a message they rely on won't come out sometimes.
 *
 * Revision 1.47  2004/02/13 07:20:48  alan
 * Change hb_resource so that we get exactly one message when local+remote
 * takeovers complete.  This is hard, and under some circumstances, we may
 * send out a message before the takeovers are 100% done.
 *
 * Revision 1.46  2004/02/12 09:30:05  alan
 * Put in a very small change to allow CTS to work with
 * auto_failback 'legacy' configurations.
 * We didn't print a message when we took over resources from a machine which was
 * dead when we first came up.  CTS was triggering off that message.
 * So, it thought things were broken.
 *
 * Revision 1.45  2004/02/12 08:55:44  alan
 * Neatend up the code for printing a takeover complete message for the
 * CTS tests to look for.
 *
 * Revision 1.44  2004/02/10 22:44:02  alan
 * Found and got rid of a direct call to notify world, and replaced it by
 * a queued call to notify world.
 *
 * Revision 1.43  2004/02/10 19:16:24  alan
 * Print fewer messages with minimum level of debug turned on.
 * This will help realtime performance when running tests.
 *
 * Revision 1.42  2004/02/03 07:10:51  alan
 * Put in yet another early shutdown special case.
 * I can't seem to hang it any more this way in hundreds of attempts :-)
 *
 * Revision 1.40  2004/01/30 22:45:17  alan
 * Fixed a hole where very early shutdown requests with the other node
 * dead could cause problems.
 *
 * Revision 1.39  2004/01/20 15:23:22  alan
 * Fixed a dumb bug where we correctly (and somewhat laboriously) computed the
 * resource state of the "other side", and then immediately (on the next line)
 * set it to the wrong value.
 * This is probably the result of not doing the update correctly when we
 * put the more laborious (and correct) code in a while back (oops!).
 *
 * Revision 1.38  2003/09/26 05:48:19  alan
 * Fixed a few undefined variable complaints.
 *
 * Revision 1.37  2003/09/26 04:34:13  alan
 * Fine tuning the auditing code so it doesn't bitch inappropriately.
 *
 * Revision 1.36  2003/09/24 23:04:41  alan
 * Put in some small amount of debugging, and eliminated a little of the
 * checking for auto_failback legacy case during shutdown.
 *
 * Revision 1.35  2003/09/24 05:53:13  alan
 * Put in a STONITH message for one case where it was missing...
 *
 * Revision 1.34  2003/09/19 19:57:31  alan
 * When auto_failback = legacy, 1.1.2 wouldn't shut down properly.
 * The code was testing some nice_failback only variables.  Now it doesn't ;-)
 *
 * Revision 1.33  2003/09/19 19:21:14  alan
 * Fixed the bug where we ran resource scripts twice.
 * The fix consisted of causing the resource requests to be queued, so that they aren't run simultaneously.
 *
 * Revision 1.32  2003/07/14 04:12:12  alan
 * Changed heartbeat so that the resource acquisition messages come out at
 * the right time.
 *
 * Revision 1.31  2003/07/13 12:54:32  alan
 * Added a (warning) message indicating shutdown has been delayed until
 * current resource activities are completed.
 *
 * Revision 1.30  2003/07/13 12:45:53  alan
 * Put in two changes:
 *     Determine when we are done with initial takeover after we first
 * 	start and print unique messages for those cases to help the tests
 * 	know when our resource takeover is really complete.
 *
 *     Changed heartbeat so that it will not shut down immediately when
 * 	requested.  Instead, it will wait until existing resource actions
 * 	have been completed before beginning the shutdown.  This will
 * 	keep it from leaving resources in a bad state.
 *
 * Revision 1.29  2003/07/03 23:09:42  alan
 * Added a new API call to return the current state of cluster
 * resources: all, local, foreign, or transition.
 *
 * Revision 1.28  2003/07/01 10:12:26  horms
 * Use defines for node types rather than arbitary strings
 *
 * Revision 1.27  2003/06/28 04:47:51  alan
 * Fixed some terrible, horrible, no good very bad reload bugs -- especially
 * with nice_failback turned on.  Yuck!
 * Also fixed a STONITH bug.  The previous code wouldn't STONTIH a node
 * we hadn't heard from yet -- but we really need to.
 * Decreased debugging verbosity a bit...
 *
 * Revision 1.26  2003/06/24 06:40:49  alan
 * Removed a redundant include of <ha_config.h>
 *
 * Revision 1.25  2003/06/02 12:39:23  lars
 * Cleaned up a corrupted log message.
 *
 * Revision 1.24  2003/05/29 06:54:31  ram
 * the managed clients are now killed after all the resource scripts have
 * completed, while shutting down heartbeat.
 *
 * Revision 1.23  2003/05/22 23:13:26  alan
 * Changed the code to fix a bug in resource auditing code.
 * We now indicate if an update to the resource set is incremental or full.
 *
 * Revision 1.22  2003/05/22 05:17:42  alan
 * Added the auto_failback option to heartbeat.
 *
 * Revision 1.21  2003/05/19 14:42:57  alan
 * Put in some code to fix some resource audit problems which showed up
 * with the new standby enhancements.
 * Fixed some long lines to be a little shorter.
 *
 * Revision 1.20  2003/05/17 05:28:46  alan
 * Implemented a modified version of nice_failback allowing it to
 * move subsets of resources around - not just all of them...
 *
 * Revision 1.19  2003/05/17 01:43:25  alan
 * Changed the standby code to allow for the possibility of failing
 * back to a "balanced" mode...
 *
 * Revision 1.18  2003/04/16 22:31:22  alan
 * Fixed a timing window bug for resource acquisition with !nice_failback.
 * Sometimes if the other node goes down just as the current node is coming up,
 * we can fail to take over our own resources because we sent the "mother may
 * I" query to the other side, but it died before answering.
 * Then, we'll never bring our own resources up because we'll wait forever.
 *
 * Revision 1.17  2003/04/15 23:06:53  alan
 * Lots of new code to support the semi-massive process restructuriing.
 *
 * Revision 1.16  2003/03/18 11:39:10  lars
 * Back out overly eager experimental change from hb_resource.c again which
 * sneaked into the last commit.
 *
 * Revision 1.15  2003/03/18 11:36:25  lars
 * Patch by Adam Li <adam.li@intel.com>:
 *
 * - man page updated for apphbd
 * - Sample apphbd configure file: doc/apphbd.cf
 * - Updated telecom/apphbd/apphbtest.c to make the output more understandable.
 *   New test cases added. However, the new test cases are not enabled by default
 *   (yet).
 * - Updated configure.in and other a few Makefile.am to enable compile of apphbd
 *   plugin recmgr and recovery manager.
 * - Bug fixes for recovery manager:
 *   - recoverymgrd cannot read more than one recovery script from configuration
 *     file.
 *   - recoverymgrd cannot set euid and egid for recovery scripts according to
 *     config file; order of setuid/setgid corrected.
 *   - Make sure that the specified user and group exists in the system by using
 *     getpwnam() and getgrnam()
 *   - Read configuration from file instead of using stdin in recoverymgrd
 *
 * Revision 1.14  2003/03/05 16:51:08  alan
 * Fixed a problem reported by Paul Porcelli and others.
 *
 * The problem is that the standby function used the wrong timeout on the receiving
 * end of the resources, so that if it took more than 10 seconds to release a resource
 * that it wouldn't work - because the receiving end timed out.
 *
 * Revision 1.13  2003/02/07 08:37:16  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.12  2003/02/05 09:06:33  horms
 * Lars put a lot of work into making sure that portability.h
 * is included first, everywhere. However this broke a few
 * things when building against heartbeat headers that
 * have been installed (usually somewhere under /usr/include or
 * /usr/local/include).
 *
 * This patch should resolve this problem without undoing all of
 * Lars's hard work.
 *
 * As an asside: I think that portability.h is a virus that has
 * infected all of heartbeat's code and now must also infect all
 * code that builds against heartbeat. I wish that it didn't need
 * to be included all over the place. Especially in headers to
 * be installed on the system. However, I respect Lars's opinion
 * that this is the best way to resolve some weird build problems
 * in the current tree.
 *
 * Revision 1.11  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.10  2003/01/25 01:19:52  ram
 * 1. Fixed a bug in heartbeat. Heartbeat does not send out a stonith completion
 * 	event if stonith device is not configured. Fixed it and made it
 * 	to send a Stonith compeletion event with result as T_STONITH_NOTCONFGD.
 * 2. Changed ccm to depend on STONITH COMPLETION event to trigger a new
 * 	membership computation. Earlier it depended on connectivity status,
 * 	to trigger a new membership. This is not entirely right.
 * 	CCM by depending on stonith completion device can not only
 * 	determine the new membership correctly, but also can deterministically
 * 	conclude that it is the only partition in the cluster, and hence
 * 	can ease computation of quorum(in a two node cluster).
 * 3. Finally fixed a memory related bug in the ccm library reported during
 * 	EVMS testing.
 *
 * Revision 1.9  2003/01/15 19:14:18  alan
 * fixed an incorrect log message when a STONITH message couldn't be created.
 *
 * Revision 1.8  2002/11/28 17:10:05  alan
 * We had a problem with local status updates getting all hosed sometimes
 * (depending on timing).  This greatly simplifies the management of
 * local status, and even takes a field out of the heartbeat packet.
 *
 * A fix like this was suggested by Horms.
 *
 * Revision 1.7  2002/11/22 07:04:39  horms
 * make lots of symbols static
 *
 * Revision 1.6  2002/11/19 19:23:48  lclaudio
 * Variable going_standby was declared twice in this file. Fixed.
 *
 * Revision 1.5  2002/11/08 15:49:39  alan
 * Fixed a bug in STONITH for the true cluster partition case.
 * When we came up, and didn't see the other node, we just took over
 * resources w/o STONITH.
 * Now we STONITH the node first, then take the data over.
 *
 * Revision 1.4  2002/10/30 17:17:40  alan
 * Added some debugging, and changed one message from an ERROR to a WARNING.
 *
 * Revision 1.3  2002/10/22 17:41:58  alan
 * Added some documentation about deadtime, etc.
 * Switched one of the sets of FIFOs to IPC channels.
 * Added msg_from_IPC to ha_msg.c make that easier.
 * Fixed a few compile errors that were introduced earlier.
 * Moved hb_api_core.h out of the global include directory,
 * and back into a local directory.  I also make sure it doesn't get
 * installed.  This *shouldn't* cause problems.
 * Added a ipc_waitin() function to the IPC code to allow you to wait for
 * input synchronously if you really want to.
 * Changes the STONITH test to default to enabled.
 *
 */
