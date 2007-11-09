/*
 * heartbeat: Linux-HA heartbeat code
 *
 * Copyright (C) 1999-2002 Alan Robertson <alanr@unix.sh>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *
 *	The basic facilities for heartbeats and intracluster communication
 *	are contained within.
 *
 *	There is a master configuration file which we open to tell us
 *	what to do.
 *
 *	It has lines like this in it:
 *
 *	serial	/dev/cua0, /dev/cua1
 *	udp	eth0
 *
 *	node	amykathy, kathyamy
 *	node	dralan
 *	keepalive 2
 *	deadtime  10
 *	hopfudge 2
 *	baud 19200
 *	udpport 694
 *
 *	"Serial" lines tell us about our heartbeat configuration.
 *	If there is more than one serial port configured, we are in a "ring"
 *	configuration, every message not originated on our node is echoed
 *	to the other serial port(s)
 *
 *	"Node" lines tell us about the cluster configuration.
 *	We had better find our uname -n nodename here, or we won't start up.
 *
 *	We complain if we find extra nodes in the stream that aren't
 *	in the master configuration file.
 *
 *	keepalive lines specify the keepalive interval
 *	deadtime lines specify how long we wait before declaring
 *		a node dead
 *	hopfudge says how much larger than #nodes we allow hopcount
 *		to grow before dropping the message
 *
 *	I need to separate things into a "global" configuration file,
 *	and a "local" configuration file, so I can double check
 *	the global against the cluster when changing configurations.
 *	Things like serial port assignments may be node-specific...
 *
 *	This has kind of happened over time.  Haresources and authkeys are
 *	decidely global, whereas ha.cf has remained more local.
 *
 */

/*
 *	Here's our process structure:
 *
 *
 *		Master Status process - manages protocol and controls
			everything.
 *
 *		hb channel read processes - each reads a hb channel, and
 *			copies messages to the master status process.  The tty
 *			version of this cross-echos to the other ttys
 *			in the ring (ring passthrough)
 *
 *		hb channel write processes - one per hb channel, each reads
 *		its own IPC channel and send the result to its medium
 *
 *	The result of all this hoorah is that we have the following procs:
 *
 *	One Master Control process
 *	One FIFO reader process
 *		"n" hb channel read processes
 *		"n" hb channel write processes
 *
 *	For the usual 2 ttys in a ring configuration, this is 6 processes
 *
 *	For a system using only UDP for heartbeats this is 4 processes.
 *
 *	For a system using 2 ttys and UDP, this is 8 processes.
 *
 *	If every second, each node writes out 150 chars of status,
 *	and we have 8 nodes, and the data rate would be about 1200 chars/sec.
 *	This would require about 12000 bps.  Better run faster than that.
 *
 *	for such a cluster...  With good UARTs and CTS/RTS, and good cables,
 *	you should be able to.  Maybe 56K would be a good choice...
 *
 *
 ****** Wish List: **********************************************************
 *	[not necessarily in priority order]
 *
 *	Heartbeat API conversion to unix domain sockets:
 *		We ought to convert to UNIX domain sockets because we get
 *		better verification of the user, and we would get notified
 *		when they die.  This should use the now-written IPC libary.
 *		(NOTE:  this is currently in progress)
 *
 *	Fuzzy heartbeat timing
 *		Right now, the code works in such a way that it
 *		systematically gets everyone heartbeating on the same time
 *		intervals, so that they happen at precisely the same time.
 *		This isn't too good for non-switched ethernet (CSMA/CD)
 *		environments, where it generates gobs of collisions, packet
 *		losses and retransmissions.  It's especially bad if all the
 *		clocks are in sync, which of course, every good system
 *		administrator strives to do ;-) This description is due to
 *		Alan Cox who pointed out section 3.3 "Timers" in RFC 1058,
 *		which it states:
 *
 *       	  "It is undesirable for the update messages to become
 *		   synchronized, since it can lead to unnecessary collisions
 *		   on broadcast networks."
 *
 *		In particular, on Linux, if you set your all the clocks in
 *		your cluster via NTP (as you should), and heartbeat every
 *		second, then all the machines in the world will all try and
 *		heartbeat at precisely the same time, because alarm(2) wakes
 *		up on even second boundaries, which combined with the use of
 *		NTP (recommended), will systematically cause LOTS of
 *		unnecessary collisions.
 *
 *		Martin Lichtin suggests:
 *		Could you skew the heartbeats, based on the interface IP#?
 *
 *		AlanR replied:
 *
 *		I thought that perhaps I could set each machine to a
 *		different interval in a +- 0.25 second range.  For example,
 *		one machine might heartbeat at 0.75 second interval, and
 *		another at a 1.25 second interval.  The tendency would be
 *		then for the timers to wander across second boundaries,
 *		and even if they started out in sync, they would be unlikely
 *		to stay in sync.  [but in retrospect, I'm not 100% sure
 *		about this approach]
 *
 *		This would keep me from having to generate a random number
 *		for every single heartbeat as the RFC suggests.
 *
 *		Of course, there are only 100 ticks/second, so if the clocks
 *		get closely synchronized, you can only have 100 different
 *		times to heartbeat.  I suppose if you have something like
 *		50-100 nodes, you ought to use a switch, and not a hub, and
 *		this would likely mitigate the problems.
 *
 *	Nearest Neighbor heartbeating (? maybe?)
 *		This is a candidate to replace the current policy of
 *		full-ring heartbeats In this policy, each machine only
 *		heartbeats to it's nearest neighbors.  The nearest neighbors
 *		only forward on status CHANGES to their neighbors.
 *		This means that the total ring traffic in the non-error
 *		case is reduced to the same as a 3-node cluster.
 *		This is a huge improvement.  It probably means that
 *		19200 would be fast enough for almost any size
 *		network. Non-heartbeat admin traffic would need to be
 *		forwarded to all members of the ring as it was before.
 *
 *	IrDA heartbeats
 *		This is a near-exact replacement for ethernet with lower
 *		bandwidth, low costs and fewer points of failure.
 *		The role of an ethernet hub is replaced by a mirror, which
 *		is less likely to fail.  But if it does, it might mean
 *		seven years of bad luck :-)  On the other hand, the "mirror"
 *		could be a white painted board ;-)
 *
 *		The idea would be to make a bracket with the IrDA
 *		transceivers on them all facing the same way, then mount
 *		the bracket with the transceivers all facing the mirror.
 *		Then each of the transceivers would be able to "see" each
 *		other.
 *
 *		I do kind of wonder if the kernel's IrDA stacks would be up
 *		to so much contention as it seems unlikely that they'd ever
 *		been tested in such a stressful environment.  But, it seems
 *		really cool to me, and it only takes one port per machine
 *		rather than two like we need for serial rings.
 *
 */

#include <lha_internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dirent.h>
#include <netdb.h>
#include <ltdl.h>
#ifdef _POSIX_MEMLOCK
#	include <sys/mman.h>
#endif
#ifdef _POSIX_PRIORITY_SCHEDULING
#	include <sched.h>
#endif

#if HAVE_LINUX_WATCHDOG_H
#	include <sys/ioctl.h>
#	include <linux/types.h>
#	include <linux/watchdog.h>
#endif

#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/longclock.h>
#include <clplumbing/proctrack.h>
#include <clplumbing/ipc.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/timers.h>
#include <clplumbing/cl_poll.h>
#include <clplumbing/realtime.h>
#include <clplumbing/uids.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/cpulimits.h>
#include <clplumbing/netstring.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/cl_random.h>
#include <clplumbing/cl_reboot.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <hb_api.h>
#include <hb_api_core.h>
#include <test.h>
#include <hb_proc.h>
#include <pils/plugin.h>
#include <hb_module.h>
#include <HBcomm.h>
#include <heartbeat_private.h>
#include <hb_signal.h>
#include <hb_config.h>
#include <hb_resource.h>
#include <apphb.h>
#include <clplumbing/cl_uuid.h>
#include "clplumbing/setproctitle.h"
#include <clplumbing/cl_pidfile.h>

#define OPTARGS			"dDkMrRsvWlC:V"
#define	ONEDAY			(24*60*60)	/* Seconds in a day */
#define REAPER_SIG		0x0001UL
#define TERM_SIG		0x0002UL
#define DEBUG_USR1_SIG		0x0004UL
#define DEBUG_USR2_SIG		0x0008UL
#define PARENT_DEBUG_USR1_SIG	0x0010UL
#define PARENT_DEBUG_USR2_SIG	0x0020UL
#define REREAD_CONFIG_SIG	0x0040UL
#define FALSE_ALARM_SIG		0x0080UL
#define MAX_MISSING_PKTS	20

#define	ALWAYSRESTART_ON_SPLITBRAIN	1

#define	FLOWCONTROL_LIMIT	 ((seqno_t)(MAXMSGHIST/2))


static char 			hbname []= "heartbeat";
const char *			cmdname = hbname;
char *				localnodename = NULL;
static int			Argc = -1;
extern int			optind;
void				(*localdie)(void);
extern PILPluginUniv*		PluginLoadingSystem;
struct hb_media*		sysmedia[MAXMEDIA];
struct msg_xmit_hist		msghist;
extern struct hb_media_fns**	hbmedia_types;
extern int			num_hb_media_types;
int				nummedia = 0;
struct sys_config  		config_init_value;
struct sys_config *		config  = &config_init_value;
struct node_info *		curnode = NULL;
pid_t				processes[MAXPROCS];
volatile struct pstat_shm *	procinfo = NULL;
volatile struct process_info *	curproc = NULL;
struct TestParms *		TestOpts;

extern int			debug_level;
gboolean			verbose = FALSE;
int				timebasedgenno = FALSE;
int				parse_only = FALSE;
static gboolean			killrunninghb = FALSE;
static gboolean			rpt_hb_status = FALSE;
int				RestartRequested = FALSE;
int				hb_realtime_prio = -1;

int	 			UseApphbd = FALSE;
static gboolean			RegisteredWithApphbd = FALSE;

char *				watchdogdev = NULL;
static int			watchdogfd = -1;
static int			watchdog_timeout_ms = 0L;

int				shutdown_in_progress = FALSE;
int				startup_complete = FALSE;
int				WeAreRestarting = FALSE;
enum comm_state			heartbeat_comm_state = COMM_STARTING;
static gboolean			get_reqnodes_reply = FALSE;
static int			CoreProcessCount = 0;
static int			managed_child_count= 0;
int				UseOurOwnPoll = FALSE;
static longclock_t		NextPoll = 0UL;
static int			ClockJustJumped = FALSE;
longclock_t			local_takeover_time = 0L;
static int 			deadtime_tmpadd_count = 0;
gboolean			enable_flow_control = TRUE;
static	int			send_cluster_msg_level = 0;
static int			live_node_count = 1; /* namely us... */
static void print_a_child_client(gpointer childentry, gpointer unused);
static seqno_t			timer_lowseq = 0;
static	gboolean		init_deadtime_passed = FALSE;		
static int			PrintDefaults = FALSE;
static int			WikiOutput = FALSE;
GTRIGSource*			write_hostcachefile = NULL;
GTRIGSource*			write_delcachefile = NULL;
extern GSList*			del_node_list;


#undef DO_AUDITXMITHIST
#ifdef DO_AUDITXMITHIST
#	define AUDITXMITHIST audit_xmit_hist()
void		audit_xmit_hist(void);
#else
#	define AUDITXMITHIST /* Nothing */
#endif
static void	restart_heartbeat(void);
static void	usage(void);
static void	init_procinfo(void);
static int	initialize_heartbeat(void);
static
const char*	core_proc_name(enum process_type t);

static void	CoreProcessRegistered(ProcTrack* p);
static void	CoreProcessDied(ProcTrack* p, int status, int signo
,			int exitcode, int waslogged);
static
const char*	CoreProcessName(ProcTrack* p);

void		hb_kill_managed_children(int nsig);
void		hb_kill_rsc_mgmt_children(int nsig);
void		hb_kill_core_children(int nsig);
gboolean	hb_mcp_final_shutdown(gpointer p);


static void	ManagedChildRegistered(ProcTrack* p);
static void	ManagedChildDied(ProcTrack* p, int status
,			int signo, int exitcode, int waslogged);
static
const char*	ManagedChildName(ProcTrack* p);
static void	check_for_timeouts(void);
static void	check_comm_isup(void);
static int	send_local_status(void);
static int	set_local_status(const char * status);
static void	check_rexmit_reqs(void);
static void	mark_node_dead(struct node_info* hip);
static void	change_link_status(struct node_info* hip, struct link *lnk
,			const char * new);
static void	comm_now_up(void);
static void	make_daemon(void);
static void	hb_del_ipcmsg(IPC_Message* m);
static
IPC_Message*	hb_new_ipcmsg(const void* data, int len, IPC_Channel* ch
,			int refcnt);
static void	send_to_all_media(const char * smsg, int len);
static int	should_drop_message(struct node_info* node
,		const struct ha_msg* msg, const char *iface, int *);
static int	is_lost_packet(struct node_info * thisnode, seqno_t seq);
static void	cause_shutdown_restart(void);
static gboolean	CauseShutdownRestart(gpointer p);
static void	add2_xmit_hist (struct msg_xmit_hist * hist
,			struct ha_msg* msg, seqno_t seq);
static void	init_xmit_hist (struct msg_xmit_hist * hist);
static void	process_rexmit(struct msg_xmit_hist * hist
,			struct ha_msg* msg);
static void	update_ackseq(seqno_t new_ackseq) ;
static void	process_clustermsg(struct ha_msg* msg, struct link* lnk);
extern void	process_registerevent(IPC_Channel* chan,  gpointer user_data);
static void	nak_rexmit(struct msg_xmit_hist * hist, 
			   seqno_t seqno, const char*, const char * reason);
static int	IncrGeneration(seqno_t * generation);
static int	GetTimeBasedGeneration(seqno_t * generation);
static int	process_outbound_packet(struct msg_xmit_hist* hist
,			struct ha_msg * msg);
static void	start_a_child_client(gpointer childentry, gpointer dummy);
static gboolean	shutdown_last_client_child(int nsig);
static void	LookForClockJumps(void);
static void	get_localnodeinfo(void);
static gboolean EmergencyShutdown(gpointer p);
static void	hb_check_mcp_alive(void);
static gboolean hb_reregister_with_apphbd(gpointer dummy);

static void	hb_add_deadtime(int increment);
static gboolean	hb_pop_deadtime(gpointer p);
static void	dump_missing_pkts_info(void);
static int	write_hostcachedata(gpointer ginfo);
static int	write_delcachedata(gpointer ginfo);

static GHashTable*	message_callbacks = NULL;
static gboolean	HBDoMsgCallback(const char * type, struct node_info* fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg);
static void HBDoMsg_T_REXMIT(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg);
static void HBDoMsg_T_STATUS(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg);
static void HBDoMsg_T_QCSTATUS(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg);

static void (*comm_up_callback)(void) = NULL;
static gboolean set_init_deadtime_passed_flag(gpointer p);
/*
 * Glib Mainloop Source functions...
 */
static gboolean	polled_input_prepare(GSource* source,
				     gint* timeout);
static gboolean	polled_input_check(GSource* source);
static gboolean	polled_input_dispatch(GSource* source,
				      GSourceFunc callback,
				      gpointer user_data);

static gboolean	APIregistration_dispatch(IPC_Channel* chan, gpointer user_data);
static gboolean	FIFO_child_msg_dispatch(IPC_Channel* chan, gpointer udata);
static gboolean	read_child_dispatch(IPC_Channel* chan, gpointer user_data);
static gboolean hb_update_cpu_limit(gpointer p);


static int	SetupFifoChild(void);
/*
 * The biggies
 */
static void	read_child(struct hb_media* mp);
static void	write_child(struct hb_media* mp);
static void	fifo_child(IPC_Channel* chan);		/* Reads from FIFO */
		/* The REAL biggie ;-) */
static void	master_control_process(void);

extern void	dellist_destroy(void);
extern int	dellist_add(const char* nodename);


#define	CHECK_HA_RESOURCES()	(DoManageResources 		\
		 ?	(parse_ha_resources(RESOURCE_CFG) == HA_OK) : TRUE)

/*
 * Structures initialized to function pointer values...
 */

ProcTrack_ops			ManagedChildTrackOps = {
	ManagedChildDied,
	ManagedChildRegistered,
	ManagedChildName
};

static ProcTrack_ops		CoreProcessTrackOps = {
	CoreProcessDied,
	CoreProcessRegistered,
	CoreProcessName
};

static GSourceFuncs		polled_input_SourceFuncs = {
	polled_input_prepare,
	polled_input_check,
	polled_input_dispatch,
	NULL,
};

static void
init_procinfo()
{
	int	ipcid;
	struct pstat_shm *	shm;

	if ((ipcid = shmget(IPC_PRIVATE, sizeof(*procinfo), 0600)) < 0) {
		cl_perror("Cannot shmget for process status");
		return;
	}

	/*
	 * Casting this address into a long stinks, but there's no other
	 * way because of the way the shared memory API is designed.
	 */
	if (((long)(shm = shmat(ipcid, NULL, 0))) == -1L) {
		cl_perror("Cannot shmat for process status");
		shm = NULL;
		return;
	}
	if (shm) {
		procinfo = shm;
		memset(shm, 0, sizeof(*procinfo));
	}

	/*
	 * Go ahead and "remove" our shared memory now...
	 *
	 * This is cool because the manual says:
	 *
	 *	IPC_RMID    is used to mark the segment as destroyed. It
	 *	will actually be destroyed after the last detach.
	 *
	 * Not all the Shared memory implementations have as clear a
	 * description of this fact as Linux, but they all work this way
	 * anyway (for all we've tested).
	 */
	if (shmctl(ipcid, IPC_RMID, NULL) < 0) {
		cl_perror("Cannot IPC_RMID proc status shared memory id");
	}
	/* THIS IS RESOURCE WORK!  FIXME */
	procinfo->giveup_resources = 1;
	procinfo->i_hold_resources = HB_NO_RSC;
}




void
hb_versioninfo(void)
{
	cl_log(LOG_INFO, "%s: version %s", cmdname, VERSION);
}

/*
 *	Look up the interface in the node struct,
 *	returning the link info structure
 */
struct link *
lookup_iface(struct node_info * hip, const char *iface)
{
	struct link *	lnk;
	int		j;

	for (j=0; (lnk = &hip->links[j], lnk->name); ++j) {
		if (strcmp(lnk->name, iface) == 0) {
			return lnk;
		}
	}
	return NULL;
}

/*
 *	Look up the node in the configuration, returning the node
 *	info structure
 */
struct node_info *
lookup_node(const char * h)
{
	int			j;
	char	*shost;

	if ( (shost = cl_strdup(h)) == NULL) {
		return NULL;
	}
	g_strdown(shost);
	for (j=0; j < config->nodecount; ++j) {
		if (strcmp(shost, config->nodes[j].nodename) == 0)
			break;
	}
	cl_free(shost);
	if (j == config->nodecount) {
		return NULL;
	} else {
		return (config->nodes+j);
	}
}

static int
write_hostcachedata(gpointer notused)
{
	hb_setup_child();
	return write_cache_file(config);
}

static int
write_delcachedata(gpointer notused)
{
	hb_setup_child();
	return write_delnode_file(config);
}

void
hb_setup_child(void)
{

	close(watchdogfd);
	cl_make_normaltime();
	cl_cpu_limit_disable();
}

static void
change_logfile_ownership(void)
{
	struct passwd * entry;
	const char* apiuser = HA_CCMUSER;

	entry = getpwnam(apiuser);
	if (entry == NULL){
		cl_log(LOG_ERR, "%s: entry for user %s not found",
		  __FUNCTION__,  apiuser);
		return;
	}
	
	if (config->use_logfile){
		if (chown(config->logfile, entry->pw_uid, entry->pw_gid) < 0) {
			cl_log(LOG_WARNING, "%s: failed to chown logfile: %s",
			  __FUNCTION__, strerror(errno));
		}
	}
	if (config->use_dbgfile){
		if (chown(config->dbgfile, entry->pw_uid, entry->pw_gid) < 0) {
			cl_log(LOG_WARNING, "%s: failed to chown dbgfile: %s",
			  __FUNCTION__, strerror(errno));
		}
	}
	
}

/*
 * We can call this function once when we first start up and we can
 * also be called later to restart the FIFO process if it dies.
 * For R1-style clusters, the FIFO process is necessary for graceful
 * shutdown and restart.
 */
static int
SetupFifoChild(void) {
	static IPC_Channel*	fifochildipc[2] = {NULL, NULL};
	static GCHSource*	FifoChildSource = NULL;
	static int		fifoproc = -1;
	int			pid;

	if (FifoChildSource != NULL) {
		/* Not sure if this is really right... */
		G_main_del_IPC_Channel(FifoChildSource);
		fifochildipc[P_READFD] = NULL;
	}
	if (fifochildipc[P_WRITEFD] != NULL) {
		IPC_Channel* ch = fifochildipc[P_WRITEFD];
		ch->ops->destroy(ch);
		fifochildipc[P_WRITEFD] = NULL;
	}
			
	if (ipc_channel_pair(fifochildipc) != IPC_OK) {
		cl_perror("cannot create FIFO ipc channel");
		return HA_FAIL;
	}
	/* Encourage better real-time behavior */
	fifochildipc[P_READFD]->ops->set_recv_qlen(fifochildipc[P_READFD], 0); 
	/* Fork FIFO process... */
	if (fifoproc < 0) {
		fifoproc = procinfo->nprocs;
	}
	switch ((pid=fork())) {
		case -1:	cl_perror("Can't fork FIFO process!");
				return HA_FAIL;
				break;

		case 0:		/* Child */
				close(watchdogfd);
				curproc = &procinfo->info[fifoproc];
				cl_malloc_setstats(&curproc->memstats);
				cl_msg_setstats(&curproc->msgstats);
				curproc->type = PROC_HBFIFO;
				while (curproc->pid != getpid()) {
					sleep(1);
				}
				fifo_child(fifochildipc[P_WRITEFD]);
				cl_perror("FIFO child process exiting!");
				cleanexit(1);
		default:
				fifochildipc[P_READFD]->farside_pid = pid;

	}
	NewTrackedProc(pid, 0, PT_LOGVERBOSE, GINT_TO_POINTER(fifoproc)
	,	&CoreProcessTrackOps);

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "FIFO process pid: %d", pid);
	}
	/* We only read from this source, we never write to it */
	FifoChildSource = G_main_add_IPC_Channel(PRI_FIFOMSG
	,	fifochildipc[P_READFD]
	,	FALSE, FIFO_child_msg_dispatch, NULL, NULL);
	G_main_setmaxdispatchdelay((GSource*)FifoChildSource, config->heartbeat_ms);
	G_main_setmaxdispatchtime((GSource*)FifoChildSource, 50);
	G_main_setdescription((GSource*)FifoChildSource, "FIFO");
	
	return HA_OK;
}

/*
 *	This routine starts everything up and kicks off the heartbeat
 *	process.
 */
static int
initialize_heartbeat()
{
/*
 * Things we have to do:
 *
 *	Create all our pipes
 *	Open all our heartbeat channels
 *	fork all our children, and start the old ticker going...
 *
 *	Everything is forked from the parent process.  That's easier to
 *	monitor, and easier to shut down.
 */

	int		j;
	struct stat	buf;
	int		pid;
	int		ourproc = 0;
	int	(*getgen)(seqno_t * generation) = IncrGeneration;

	localdie = NULL;


	change_logfile_ownership();
	
	if (timebasedgenno) {
		getgen = GetTimeBasedGeneration;
	}
	if (getgen(&config->generation) != HA_OK) {
		cl_perror("Cannot get/increment generation number");
		return HA_FAIL;
	}
	cl_log(LOG_INFO, "Heartbeat generation: %lu", config->generation);
	
	if(GetUUID(config, curnode->nodename, &config->uuid) != HA_OK){
		cl_log(LOG_ERR, "getting uuid for the local node failed");
		return HA_FAIL;
	}
	
	if (ANYDEBUG){
		char uuid_str[UU_UNPARSE_SIZEOF];
		cl_uuid_unparse(&config->uuid, uuid_str);
		cl_log(LOG_DEBUG, "uuid is:%s", uuid_str);
	}
	
	add_uuidtable(&config->uuid, curnode);
	cl_uuid_copy(&curnode->uuid, &config->uuid);


	if (stat(FIFONAME, &buf) < 0 ||	!S_ISFIFO(buf.st_mode)) {
		cl_log(LOG_INFO, "Creating FIFO %s.", FIFONAME);
		unlink(FIFONAME);
		if (mkfifo(FIFONAME, FIFOMODE) < 0) {
			cl_perror("Cannot make fifo %s.", FIFONAME);
			return HA_FAIL;
		}
	}else{
		chmod(FIFONAME, FIFOMODE);
	}

	if (stat(FIFONAME, &buf) < 0) {
		cl_log(LOG_ERR, "FIFO %s does not exist", FIFONAME);
		return HA_FAIL;
	}else if (!S_ISFIFO(buf.st_mode)) {
		cl_log(LOG_ERR, "%s is not a FIFO", FIFONAME);
		return HA_FAIL;
	}



	/* THIS IS RESOURCE WORK!  FIXME */
	/* Clean up tmp files from our resource scripts */
	if (system("rm -fr " RSC_TMPDIR) <= 0) {
		cl_log(LOG_INFO, "Removing %s failed, recreating.", RSC_TMPDIR);
	}

	/* Remake the temporary directory ... */
	mkdir(RSC_TMPDIR
	,	S_IRUSR|S_IWUSR|S_IXUSR
	|	S_IRGRP|S_IWGRP|S_IXGRP	
	|	S_IROTH|S_IWOTH|S_IXOTH	|	S_ISVTX /* sticky bit */);

	/* Open all our heartbeat channels */

	for (j=0; j < nummedia; ++j) {
		struct hb_media* smj = sysmedia[j];

		if (ipc_channel_pair(smj->wchan) != IPC_OK) {
			cl_perror("cannot create hb write channel IPC");
			return HA_FAIL;
		}
		if (ipc_channel_pair(smj->rchan) != IPC_OK) {
			cl_perror("cannot create hb read channel IPC");
			return HA_FAIL;
		}
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "opening %s %s (%s)", smj->type
			,	smj->name, smj->description);
		}
		
	}

 	PILSetDebugLevel(PluginLoadingSystem, NULL, NULL, debug_level);
	CoreProcessCount = 0;
	procinfo->nprocs = 0;
	ourproc = procinfo->nprocs;
	curproc = &procinfo->info[ourproc];
	curproc->type = PROC_MST_CONTROL;
	cl_malloc_setstats(&curproc->memstats);
	cl_msg_setstats(&curproc->msgstats);
	NewTrackedProc(getpid(), 0, PT_LOGVERBOSE, GINT_TO_POINTER(ourproc)
	,	&CoreProcessTrackOps);

	curproc->pstat = RUNNING;

	/* We need to at least ignore SIGINTs early on */
	hb_signal_set_common(NULL);


	/* Now the fun begins... */
/*
 *	Optimal starting order:
 *		fifo_child();
 *		write_child();
 *		read_child();
 *		master_control_process();
 *
 */

	SetupFifoChild();

	ourproc = procinfo->nprocs;

	for (j=0; j < nummedia; ++j) {
		struct hb_media* mp = sysmedia[j];
		
		ourproc = procinfo->nprocs;
		
		if (mp->vf->open(mp) != HA_OK){
			cl_log(LOG_ERR, "cannot open %s %s",
			       mp->type,
			       mp->name);
			return HA_FAIL;
		}

		switch ((pid=fork())) {
			case -1:	cl_perror("Can't fork write proc.");
					return HA_FAIL;
					break;

			case 0:		/* Child */
					close(watchdogfd);
					curproc = &procinfo->info[ourproc];
					cl_malloc_setstats(&curproc->memstats);
					cl_msg_setstats(&curproc->msgstats);
					curproc->type = PROC_HBWRITE;
					while (curproc->pid != getpid()) {
						sleep(1);
					}
					write_child(mp);
					cl_perror("write process exiting");
					cleanexit(1);
			default:
				mp->wchan[P_WRITEFD]->farside_pid = pid;
				

		}
		NewTrackedProc(pid, 0, PT_LOGVERBOSE
		,	GINT_TO_POINTER(ourproc)
		,	&CoreProcessTrackOps);

		ourproc = procinfo->nprocs;

		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "write process pid: %d", pid);
		}

		switch ((pid=fork())) {
			case -1:	cl_perror("Can't fork read process");
					return HA_FAIL;
					break;

			case 0:		/* Child */
					close(watchdogfd);
					curproc = &procinfo->info[ourproc];
					cl_malloc_setstats(&curproc->memstats);
					cl_msg_setstats(&curproc->msgstats);
					curproc->type = PROC_HBREAD;
					while (curproc->pid != getpid()) {
						sleep(1);
					}
					read_child(mp);
					cl_perror("read_child() exiting");
					cleanexit(1);
			default:
					mp->rchan[P_WRITEFD]->farside_pid = pid;
		}
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "read child process pid: %d", pid);
		}
		NewTrackedProc(pid, 0, PT_LOGVERBOSE, GINT_TO_POINTER(ourproc)
		,	&CoreProcessTrackOps);

		
		if (mp->vf->close(mp) != HA_OK){
			cl_log(LOG_ERR, "cannot close %s %s",
			       mp->type,
			       mp->name);
			return HA_FAIL;
		}
	}




	ourproc = procinfo->nprocs;

	master_control_process();

	/*NOTREACHED*/
	cl_log(LOG_ERR, "master_control_process exiting?");
	cleanexit(LSB_EXIT_GENERIC);
	/*NOTREACHED*/
	return HA_FAIL;
}

/* Create a read child process (to read messages from hb medium) */
static void
read_child(struct hb_media* mp)
{
	IPC_Channel* ourchan =	mp->rchan[P_READFD];
	int		nullcount=0;
	const int	maxnullcount=10000;

	if (hb_signal_set_read_child(NULL) < 0) {
		cl_log(LOG_ERR, "read_child(): hb_signal_set_read_child(): "
		"Soldiering on...");
	}

	cl_make_realtime(-1
	,	(hb_realtime_prio > 1 ? hb_realtime_prio-1 : hb_realtime_prio)
	,	16, 64);
	set_proc_title("%s: read: %s %s", cmdname, mp->type, mp->name);
	cl_cdtocoredir();
	cl_set_all_coredump_signal_handlers();
	drop_privs(0, 0);	/* Become nobody */

	hb_signal_process_pending();
	curproc->pstat = RUNNING;

	if (ANYDEBUG) {
		/* Limit ourselves to 10% of the CPU */
		cl_cpu_limit_setpercent(10);
	}
	for (;;) {
		void		*pkt;
		IPC_Message     *imsg;
		int		rc;
		int		rc2;
		int		pktlen;

		hb_signal_process_pending();
		if ((pkt=mp->vf->read(mp, &pktlen)) == NULL) {
			++nullcount;
			if (nullcount > maxnullcount) {
				cl_perror("%d NULL vf->read() returns in a"
				" row. Exiting."
				,	maxnullcount);
				exit(10);
			}
			continue;
		}
		hb_signal_process_pending();
		
		imsg = wirefmt2ipcmsg(pkt, pktlen, ourchan);
		if (NULL == imsg) {
			++nullcount;
			if (nullcount > maxnullcount) {
				cl_perror("%d NULL wirefmt2ipcmsg() returns"
				" in a row. Exiting.", maxnullcount);
				exit(10);
			}
		}else{
			nullcount = 0;
			/* Send frees "imsg" "at the right time" */
			rc = ourchan->ops->send(ourchan, imsg);
			rc2 = ourchan->ops->waitout(ourchan);
			if (rc != IPC_OK || rc2 != IPC_OK) {
				cl_log(LOG_ERR, "read_child send: RCs: %d %d"
				,	rc, rc2);
			}
			if (ourchan->ch_status != IPC_CONNECT) {
				cl_log(LOG_ERR
				,	"read_child channel status: %d"
				" - returning.", ourchan->ch_status);
				return;
			}
		}
		cl_cpu_limit_update();
		cl_realtime_malloc_check();
	}
}


/* Create a write child process (to write messages to hb medium) */
static void
write_child(struct hb_media* mp)
{
	IPC_Channel* ourchan =	mp->wchan[P_READFD];

	if (hb_signal_set_write_child(NULL) < 0) {
		cl_perror("write_child(): hb_signal_set_write_child(): "
			"Soldiering on...");
	}

	set_proc_title("%s: write: %s %s", cmdname, mp->type, mp->name);
	cl_make_realtime(-1
	,	hb_realtime_prio > 1 ? hb_realtime_prio-1 : hb_realtime_prio
	,	16, 64);
	cl_cdtocoredir();
	cl_set_all_coredump_signal_handlers();
	drop_privs(0, 0);	/* Become nobody */
	curproc->pstat = RUNNING;

	if (ANYDEBUG) {
		/* Limit ourselves to 40% of the CPU */
		/* This seems like a lot, but pings are expensive :-( */
		cl_cpu_limit_setpercent(40);
	}
	for (;;) {
		IPC_Message*	ipcmsg = ipcmsgfromIPC(ourchan);
		hb_signal_process_pending();
		if (ipcmsg == NULL) {
			continue;
		}

		cl_cpu_limit_update();
		
		if (mp->vf->write(mp, ipcmsg->msg_body, ipcmsg->msg_len) != HA_OK) {
			cl_perror("write failure on %s %s."
			,	mp->type, mp->name);
		}

		if(ipcmsg->msg_done) { 
			 ipcmsg->msg_done(ipcmsg); 
		}

		hb_signal_process_pending();
		cl_cpu_limit_update();
		cl_realtime_malloc_check();
	}
}


/*
 * Read FIFO stream messages and translate to IPC msgs
 * Maybe in the future after all is merged together, we'll just poll for 
 * these every second or so.  Once we only use them for messages from
 * shell scripts, that would be good enough.
 * But, for now, we'll create this extra process...
 */
static void
fifo_child(IPC_Channel* chan)
{
	int		fiforfd;
	FILE *		fifo;
	int		flags;
	struct ha_msg *	msg = NULL;

	if (hb_signal_set_fifo_child(NULL) < 0) {
		cl_perror("fifo_child(): hb_signal_set_fifo_child()"
		": Soldiering on...");
	}
	set_proc_title("%s: FIFO reader", cmdname);
	fiforfd = open(FIFONAME, O_RDONLY|O_NDELAY|O_NONBLOCK);
	if (fiforfd < 0) {
		cl_perror("FIFO open (O_RDONLY) failed.");
		exit(1);
	}
	open(FIFONAME, O_WRONLY);	/* Keep reads from getting EOF */
	
	flags = fcntl(fiforfd, F_GETFL);
	flags &= ~(O_NONBLOCK|O_NDELAY);
	fcntl(fiforfd, F_SETFL, flags);
	fifo = fdopen(fiforfd, "r");
	if (fifo == NULL) {
		cl_perror("FIFO fdopen failed.");
		exit(1);
	}

	cl_make_realtime(-1
	,	(hb_realtime_prio > 1 ? hb_realtime_prio-1 : hb_realtime_prio)
	,	16, 8);
	cl_cdtocoredir();
	cl_set_all_coredump_signal_handlers();
	drop_privs(0, 0);	/* Become nobody */
	curproc->pstat = RUNNING;

	if (ANYDEBUG) {
		/* Limit ourselves to 10% of the CPU */
		cl_cpu_limit_setpercent(10);
	}

	/* Make sure we check for death of parent every so often... */
	for (;;) {

		setmsalarm(1000L);
		msg = msgfromstream(fifo);
		setmsalarm(0L);
		hb_check_mcp_alive();
		hb_signal_process_pending();

		if (msg) {
			IPC_Message*	m;
			if (DEBUGDETAILS) {
				cl_log(LOG_DEBUG, "fifo_child message:");
				cl_log_message(LOG_DEBUG, msg);
			}
			m = hamsg2ipcmsg(msg, chan);
			if (m) {
				/* Send frees "m" "at the right time" */
				chan->ops->send(chan, m);
				hb_check_mcp_alive();
				hb_signal_process_pending();
				chan->ops->waitout(chan);
				hb_check_mcp_alive();
				hb_signal_process_pending();
			}
			ha_msg_del(msg);
			msg = NULL;
			
		}else if (feof(fifo)) {
			if (ANYDEBUG) {
				return_to_orig_privs();
				cl_log(LOG_DEBUG
				,	"fifo_child: EOF on FIFO");
			}
			hb_check_mcp_alive();
			exit(2);
		}
		cl_cpu_limit_update();
		cl_realtime_malloc_check();
		hb_check_mcp_alive();
		hb_signal_process_pending();
	}
	/*notreached*/
}

static gboolean
Gmain_hb_signal_process_pending(void *unused)
{
	hb_signal_process_pending();
	return TRUE;
}


/*
 * We read a packet from the fifo (via fifo_child() process)
 */
static gboolean
FIFO_child_msg_dispatch(IPC_Channel* source, gpointer user_data)
{
	struct ha_msg*	msg;

	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG, "FIFO_child_msg_dispatch() {");
	}
	if (!source->ops->is_message_pending(source)) {
		return TRUE;
	}
	msg = msgfromIPC(source, 0);
	if (msg != NULL) {
		/* send_cluster_msg disposes of "msg" */
		send_cluster_msg(msg);
	}
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG, "}/*FIFO_child_msg_dispatch*/;");
	}
	return TRUE;
}

/*
 * We read a packet from a read child 
 */
static gboolean
read_child_dispatch(IPC_Channel* source, gpointer user_data)
{
	struct ha_msg*	msg = NULL;
	struct hb_media** mp = user_data;
	int	media_idx = mp - &sysmedia[0];

	if (media_idx < 0 || media_idx >= MAXMEDIA) {
		cl_log(LOG_ERR, "read child_dispatch: media index is %d"
		,	media_idx);
		ha_msg_del(msg); msg = NULL;
		return TRUE;
	}
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"read_child_dispatch() {");
	}
	if (!source->ops->is_message_pending(source)) {
		if (DEBUGDETAILS) {
			cl_log(LOG_DEBUG
			,	"}/*read_child_dispatch(0)*/;");
		}
		return TRUE;
	}
	msg = msgfromIPC(source, MSG_NEEDAUTH);
	if (msg != NULL) {
		const char * from = ha_msg_value(msg, F_ORIG);
		struct link* lnk = NULL;
		struct node_info* nip;

		if (from != NULL && (nip=lookup_node(from)) != NULL) {
			lnk = lookup_iface(nip, (*mp)->name);
		}

		process_clustermsg(msg, lnk);
		ha_msg_del(msg);  msg = NULL;
	}
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"}/*read_child_dispatch*/;");
	}
	return TRUE;
}

#define SEQARRAYCOUNT 5
static gboolean
Gmain_update_msgfree_count(void *unused)
{
	static int seqarray[SEQARRAYCOUNT]= {0,0,0,0,0};
	static int lastcount = -1;
	
	lastcount = (lastcount + 1) % SEQARRAYCOUNT;
	
	timer_lowseq = seqarray[lastcount];
	
	seqarray[lastcount] = msghist.hiseq;

	return TRUE;
}



/*
 * What are our abstract event sources?
 *
 *	Queued signals to be handled ("polled" high priority)
 *
 *	Sending a heartbeat message (timeout-based) (high priority)
 *
 *	Retransmitting packets for the protocol (timed medium priority)
 *
 *	Timing out on heartbeats from other nodes (timed low priority)
 *
 *		We currently combine all our timed/polled events together.
 *		The only one that has critical timing needs is sending
 *		out heartbeat messages
 *
 *	Messages from the network (file descriptor medium-high priority)
 *
 *	API requests from clients (file descriptor medium-low priority)
 *
 *	Registration requests from clients (file descriptor low priority)
 *
 */

static void
master_control_process(void)
{
/*
 *	Create glib sources for:
 *	  - API clients
 *	  - communication with read/write_children
 *	  - various signals ala polled_input_dispatch
 *
 *	Create timers for:
 *	  - sending out local status
 *	  - checking for dead nodes (one timer per node?)
 *	  - checking for dead links (one timer per link?)
 *	  - initial "comm is up" timer
 *	  - retransmission request timers (?)
 *		(that is, timers for requesting that nodes
 *		 try retransmitting to us again)
 *
 *	Set up signal handling for:
 *	  SIGINT	termination
 *	  SIGUSR1	increment debugging
 *	  SIGUSR2	decrement debugging
 *	  SIGCHLD	process termination
 *	  SIGHUP	reread configuration
 *			(should this propagate to client children?)
 *
 */
	volatile struct process_info *	pinfo;
	int			allstarted;
	int			j;
	GMainLoop*		mainloop;
	long			memstatsinterval;
	guint			id;

	write_hostcachefile = G_main_add_tempproc_trigger(PRI_WRITECACHE
	,	write_hostcachedata, "write_hostcachedata"
	,	NULL, NULL, NULL, NULL);

	write_delcachefile = G_main_add_tempproc_trigger(PRI_WRITECACHE
	,	write_delcachedata, "write_delcachedata"
	,	NULL, NULL, NULL, NULL);
	/*
	 * We _really_ only need to write out the uuid file if we're not yet
	 * in the host cache file on disk.
	 */

	G_main_set_trigger(write_hostcachefile);
	init_xmit_hist (&msghist);

	hb_init_watchdog();
	
	/*add logging channel into mainloop*/
	cl_log_set_logd_channel_source(NULL, NULL);

	if (hb_signal_set_master_control_process(NULL) < 0) {
		cl_log(LOG_ERR, "master_control_process(): "
			"hb_signal_set_master_control_process(): "
			"Soldiering on...");
	}

	if (ANYDEBUG) {
		/* Limit ourselves to 70% of the CPU */
		cl_cpu_limit_setpercent(70);
		/* Update our CPU limit periodically */
		id=Gmain_timeout_add_full(G_PRIORITY_HIGH-5
		,	cl_cpu_limit_ms_interval()
		,	hb_update_cpu_limit, NULL, NULL);
		G_main_setall_id(id, "cpu limit", 50, 20);
	}
	cl_make_realtime(-1, hb_realtime_prio, 32, config->memreserve);

	set_proc_title("%s: master control process", cmdname);



	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "Waiting for child processes to start");
	}
	/* Wait until all the child processes are really running */
	do {
		allstarted = 1;
		for (pinfo=procinfo->info; pinfo < curproc; ++pinfo) {
			if (pinfo->pstat != RUNNING) {
				if (ANYDEBUG) {
					cl_log(LOG_DEBUG
					, "Wait for pid %d type %d stat %d"
					, (int) pinfo->pid, pinfo->type
					, pinfo->pstat);
				}
				allstarted=0;
				sleep(1);
			}
		}
	}while (!allstarted);
	
	hb_add_deadtime(2000);
	id = Gmain_timeout_add(5000, hb_pop_deadtime, NULL);
	G_main_setall_id(id, "hb_pop_deadtime", 500, 100);

	set_local_status(UPSTATUS);	/* We're pretty sure we're up ;-) */
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"All your child process are belong to us");
	}

	send_local_status();

	if (G_main_add_input(G_PRIORITY_HIGH, FALSE, 
			     &polled_input_SourceFuncs) ==NULL){
		cl_log(LOG_ERR, "master_control_process: G_main_add_input failed");
	}



	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"Starting local status message @ %ld ms intervals"
		,	config->heartbeat_ms);
	}

	/* Child I/O processes */
	for(j = 0; j < nummedia; j++) {
		GCHSource*	s;
		/*
		 * We cannot share a socket between the write and read
		 * children, though it might sound like it would work ;-)
		 */

		/* Connect up the write child IPC channel... */
		s = G_main_add_IPC_Channel(PRI_SENDPKT
		,	sysmedia[j]->wchan[P_WRITEFD], FALSE
		,	NULL, sysmedia+j, NULL);
		G_main_setmaxdispatchdelay((GSource*)s, config->heartbeat_ms/4);
		G_main_setmaxdispatchtime((GSource*)s, 50);
		G_main_setdescription((GSource*)s, "write child");

		
		/* Connect up the read child IPC channel... */
		s = G_main_add_IPC_Channel(PRI_READPKT
		,	sysmedia[j]->rchan[P_WRITEFD], FALSE
		,	read_child_dispatch, sysmedia+j, NULL);
		/* Encourage better real-time behavior */
		sysmedia[j]->rchan[P_WRITEFD]->ops->set_recv_qlen
		(	sysmedia[j]->rchan[P_WRITEFD], 0); 
		G_main_setmaxdispatchdelay((GSource*)s, config->heartbeat_ms/4);
		G_main_setmaxdispatchtime((GSource*)s, 50);
		G_main_setdescription((GSource*)s, "read child");

}	
	

	/*
	 * Things to do on a periodic basis...
	 */
	
	/* Send local status at the "right time" */
	id=Gmain_timeout_add_full(PRI_SENDSTATUS, config->heartbeat_ms
	,	hb_send_local_status, NULL, NULL);
	G_main_setall_id(id, "send local status", 10+config->heartbeat_ms/2, 50);

	id=Gmain_timeout_add_full(PRI_AUDITCLIENT
	,	config->initial_deadtime_ms
	,	set_init_deadtime_passed_flag
	,	NULL
	,	NULL);
	G_main_setall_id(id, "init deadtime passed", config->warntime_ms, 50);

	/* Dump out memory stats periodically... */
	memstatsinterval = (debug_level ? 10*60*1000 : ONEDAY*1000);
	id=Gmain_timeout_add_full(PRI_DUMPSTATS, memstatsinterval
	,	hb_dump_all_proc_stats, NULL, NULL);
	G_main_setall_id(id, "memory stats", 5000, 100);

	/* Audit clients for liveness periodically */
	id=Gmain_timeout_add_full(PRI_AUDITCLIENT, 9*1000
	,	api_audit_clients, NULL, NULL);
	G_main_setall_id(id, "client audit", 5000, 100);

	/* Reset timeout times to "now" */
	for (j=0; j < config->nodecount; ++j) {
		struct node_info *	hip;
		hip= &config->nodes[j];
		hip->local_lastupdate = time_longclock();
	}

	/* Check for pending signals */
	id=Gmain_timeout_add_full(PRI_CHECKSIGS, config->heartbeat_ms
	,       Gmain_hb_signal_process_pending, NULL, NULL);
	G_main_setall_id(id, "check for signals", 10+config->heartbeat_ms/2, 50);
	
	id=Gmain_timeout_add_full(PRI_FREEMSG, 500
	,	Gmain_update_msgfree_count, NULL, NULL);
	G_main_setall_id(id, "update msgfree count", config->deadtime_ms, 50);
	
	if (UseApphbd) {
		Gmain_timeout_add_full(PRI_DUMPSTATS
		,	60*(1000-10) /* Not quite on a minute boundary */
		,	hb_reregister_with_apphbd
		,	NULL, NULL);
	}

	if (UseOurOwnPoll) {
		g_main_set_poll_func(cl_glibpoll);
		ipc_set_pollfunc(cl_poll);
	}
	mainloop = g_main_new(TRUE);
	g_main_run(mainloop);
}


static void
hb_del_ipcmsg(IPC_Message* m)
{
	/* this is perfectly safe in our case - reference counts are small ints */
	int	refcnt = POINTER_TO_SIZE_T(m->msg_private); /*pointer cast as int*/

	if (DEBUGPKTCONT) {
		cl_log(LOG_DEBUG
		,	"Message 0x%lx: refcnt %d"
		,	(unsigned long)m, refcnt);

	}
	if (refcnt <= 1) {
		if (DEBUGPKTCONT) {
			cl_log(LOG_DEBUG, "Message 0x%lx freed."
			,	(unsigned long)m);
		}
		memset(m->msg_body, 0, m->msg_len);
		cl_free(m->msg_buf);
		memset(m, 0, sizeof(*m));
		cl_free(m);
	}else{
		refcnt--;
		m->msg_private = GINT_TO_POINTER(refcnt);
	}
}

static IPC_Message*
hb_new_ipcmsg(const void* data, int len, IPC_Channel* ch, int refcnt)
{
	IPC_Message*	hdr;
	char*	copy;
	
	if (ch == NULL){
		cl_log(LOG_ERR, "hb_new_ipcmsg:"
		       " invalid parameter");
		return NULL;
	}
	
	if (ch->msgpad > MAX_MSGPAD){
		cl_log(LOG_ERR, "hb_new_ipcmsg: too many pads "
		       "something is wrong");
		return NULL;
	}


	if ((hdr = (IPC_Message*)cl_malloc(sizeof(*hdr)))  == NULL) {
		return NULL;
	}
	
	memset(hdr, 0, sizeof(*hdr));

	if ((copy = (char*)cl_malloc(ch->msgpad + len))
	    == NULL) {
		cl_free(hdr);
		return NULL;
	}
	memcpy(copy + ch->msgpad, data, len);
	hdr->msg_len = len;
	hdr->msg_buf = copy;
	hdr->msg_body = copy + ch->msgpad;
	hdr->msg_ch = ch;
	hdr->msg_done = hb_del_ipcmsg;
	hdr->msg_private = GINT_TO_POINTER(refcnt);
	
	if (DEBUGPKTCONT) {
		cl_log(LOG_DEBUG, "Message allocated: 0x%lx: refcnt %d"
		,	(unsigned long)hdr, refcnt);
	}
	return hdr;
}



/* Send this message to all of our heartbeat media */
static void
send_to_all_media(const char * smsg, int len)
{
	int	j;
	IPC_Message*	outmsg = NULL;
	
	/* Throw away some packets if testing is enabled */
	if (TESTSEND) {
		if (TestRand(send_loss_prob)) {
			if( '\0' == TestOpts->allow_nodes[0] 
			|| ';' == TestOpts->allow_nodes[0] ) {
				return;
			}
		}
	}


	/* Send the message to all our heartbeat interfaces */
	for (j=0; j < nummedia; ++j) {
		IPC_Channel*	wch = sysmedia[j]->wchan[P_WRITEFD];
		int	wrc;
		
		/*take the first media write channel as this msg's chan
		  assumption all channel's msgpad is the same
		*/
		
		if (outmsg == NULL){
			outmsg = hb_new_ipcmsg(smsg, len, wch,
					       nummedia);
		}

		if (outmsg == NULL) {
			cl_log(LOG_ERR, "Out of memory. Shutting down.");
			hb_initiate_shutdown(FALSE);
			return ;
		}
		
		outmsg->msg_ch = wch;
		wrc=wch->ops->send(wch, outmsg);
		if (wrc != IPC_OK) {
			cl_perror("Cannot write to media pipe %d"
				  ,	j);
			cl_log(LOG_ERR, "Shutting down.");
			hb_initiate_shutdown(FALSE);
		}
		alarm(0);
	}
}



static void
LookForClockJumps(void)
{
	static TIME_T	lastnow = 0L;
	TIME_T		now = time(NULL);

	/* Check for clock jumps */
	if (now < lastnow) {
		cl_log(LOG_INFO
		,	"Clock jumped backwards. Compensating.");
		ClockJustJumped = 1;
	}else{
		ClockJustJumped = 0;
	}
	lastnow = now;
}


#define	POLL_INTERVAL	250 /* milliseconds */

static gboolean
polled_input_prepare(GSource* source,
		     gint* timeout)
{

	if (DEBUGPKT){
		cl_log(LOG_DEBUG,"polled_input_prepare(): timeout=%d"
		,	*timeout);
	}
	LookForClockJumps();
	
	return ((hb_signal_pending() != 0)
	||	ClockJustJumped);
}


static gboolean
polled_input_check(GSource* source)
{
	longclock_t		now = time_longclock();
	
	LookForClockJumps();
	
	if (DEBUGPKT) {
		cl_log(LOG_DEBUG,"polled_input_check(): result = %d"
		,	cmp_longclock(now, NextPoll) >= 0);
	}
	
	/* FIXME:?? should this say pending_handlers || cmp...? */
	return (cmp_longclock(now, NextPoll) >= 0);
}

static gboolean
polled_input_dispatch(GSource* source,
		      GSourceFunc callback,
		      gpointer	user_data)
{
	longclock_t	now = time_longclock();

	if (DEBUGPKT){
		cl_log(LOG_DEBUG,"polled_input_dispatch() {");
	}
	NextPoll = add_longclock(now, msto_longclock(POLL_INTERVAL));


	LookForClockJumps();
	cl_realtime_malloc_check();

	hb_signal_process_pending();

	/* Scan nodes and links to see if any have timed out */
	if (!ClockJustJumped) {
		/* We'll catch it again next time around... */
		/* I'm not sure we really need to check for clock jumps
		 * any more since we now use longclock_t for everything
		 * and don't use time_t or clock_t for anything critical.
		 */

		check_for_timeouts();
	}

	/* Check to see we need to resend any rexmit requests... */
	(void)check_rexmit_reqs;
 
	/* See if our comm channels are working yet... */
	if (heartbeat_comm_state != COMM_LINKSUP) {
		check_comm_isup();
	}

	/* THIS IS RESOURCE WORK!  FIXME */
	/* Check for "time to take over local resources */
	if (nice_failback && resourcestate == HB_R_RSCRCVD
	&&	cmp_longclock(now, local_takeover_time) > 0) {
		resourcestate = HB_R_STABLE;
		req_our_resources(0);
		cl_log(LOG_INFO,"local resource transition completed.");
		hb_send_resources_held(TRUE, NULL);
		AuditResources();
	}
	if (DEBUGPKT){
		cl_log(LOG_DEBUG,"}/*polled_input_dispatch*/;");
	}

	return TRUE;
}

/*
 *	This should be something the code can register for.
 *	and a nice set of hooks to call, etc...
 */
static void
comm_now_up()
{
	static int	linksupbefore = 0;
	char			regsock[] = API_REGSOCK;
	char			path[] = IPC_PATH_ATTR;
	GHashTable*		wchanattrs;
	GWCSource*		regsource;
	IPC_WaitConnection*	regwchan = NULL;


	if (linksupbefore) {
		return;
	}
	linksupbefore = 1;

	cl_log(LOG_INFO
	       ,	"Comm_now_up(): updating status to " ACTIVESTATUS);
	
	/* Update our local status... */
	set_local_status(ACTIVESTATUS);

	if (comm_up_callback) {
		comm_up_callback();
	}
	
	/* Start to listen to the socket for clients*/
	wchanattrs = g_hash_table_new(g_str_hash, g_str_equal);
	
        g_hash_table_insert(wchanattrs, path, regsock);
	
	regwchan = ipc_wait_conn_constructor(IPC_DOMAIN_SOCKET, wchanattrs);

	if (regwchan == NULL) {
		cl_log(LOG_DEBUG
		,	"Cannot open registration socket at %s"
		,	regsock);
		cleanexit(LSB_EXIT_EPERM);
	}

	regsource = G_main_add_IPC_WaitConnection(PRI_APIREGISTER, regwchan
	,	NULL, FALSE, APIregistration_dispatch, NULL, NULL);
	G_main_setmaxdispatchdelay((GSource*)regsource, config->deadtime_ms);
	G_main_setmaxdispatchtime((GSource*)regsource, 20);
	G_main_setdescription((GSource*)regsource, "client registration");
	

	if (regsource == NULL) {
		cl_log(LOG_DEBUG
		,	"Cannot create registration source from IPC");
		cleanexit(LSB_EXIT_GENERIC);
	}

	/* Start each of our known child clients */
	if (!shutdown_in_progress) {
		g_list_foreach(config->client_list
		,	start_a_child_client, NULL);
	}
	if (!startup_complete) {
		startup_complete = TRUE;
		if (shutdown_in_progress) {
			shutdown_in_progress = FALSE;
			hb_initiate_shutdown(FALSE);
		}
	}
}



static gboolean
APIregistration_dispatch(IPC_Channel* chan,  gpointer user_data)
{
	/* 
	 * This channel must be non-blocking as
	 * we don't want to block for a client
	 */
	chan->should_send_block = FALSE;

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "APIregistration_dispatch() {");
	}
	process_registerevent(chan, user_data);
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "}/*APIregistration_dispatch*/;");
	}
	return TRUE;
}

void
hb_kill_managed_children(int nsig)
{
	/* Kill our managed children... */
	ForEachProc(&ManagedChildTrackOps
	, 	hb_kill_tracked_process
	,	GINT_TO_POINTER(nsig));
}

void
hb_kill_rsc_mgmt_children(int nsig)
{
	extern ProcTrack_ops hb_rsc_RscMgmtProcessTrackOps;

	ForEachProc(&hb_rsc_RscMgmtProcessTrackOps
	,	hb_kill_tracked_process
	,	GINT_TO_POINTER(nsig));
}

void
hb_kill_core_children(int nsig)
{
	ForEachProc(&CoreProcessTrackOps
	,	hb_kill_tracked_process
	,	GINT_TO_POINTER(nsig));
}

/*
 * Shutdown sequence:
 *   If non-quick shutdown:
 * 	Giveup resources (if requested)
 * 	Wait for resources to be released
 *	delay
 *
 *   Final shutdown sequence:
 *	Kill managed client children with SIGTERM
 *	If non-quick, kill rsc_mgmt children with SIGTERM
 *	Delay
 *	If non-quick, kill rsc_mgmt children with SIGKILL
 *	Kill core processes (except self) with SIGTERM
 *	Delay
 *	Kill core processes (except self) with SIGKILL
 *	Wait for all children to die.
 *
 */
void
hb_initiate_shutdown(int quickshutdown)
{
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "hb_initiate_shutdown() called.");
	}
	if (shutdown_in_progress) {
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "hb_initiate_shutdown"
			"(): shutdown already in progress");
			return;
		}
	}
		
	/* THINK maybe even add "need_shutdown", because it is not yet in
	 * progress; or do a Gmain_timeout_add, or something like that.
	 * A cleanexit(LSB_EXIT_OK) won't do, out children will continue
	 * without us.
	 */
	shutdown_in_progress = TRUE;
	if (!startup_complete) {
		cl_log(LOG_WARNING
		,	"Shutdown delayed until Communication is up.");
		return;
	}
	send_local_status();
	if (!quickshutdown && DoManageResources) {
		/* THIS IS RESOURCE WORK!  FIXME */
		procinfo->giveup_resources = TRUE;
		hb_giveup_resources();
		/* Do something more drastic in 60 minutes */
		Gmain_timeout_add(1000*60*60, EmergencyShutdown, NULL);
		return;
	}
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		, "hb_initiate_shutdown(): calling hb_mcp_final_shutdown()");
	}
	/* Trigger initial shutdown process for quick shutdown */
	hb_mcp_final_shutdown(NULL); /* phase 0 (quick) */
}

/*
 *	The general idea of this code is that we go through several shutdown phases:
 *
 *	0: We've given up release 1 style local resources
 *    		Action:  we shut down our client children
 *		each one in reverse start order
 *
 *	1: We've shut down all our client children
 *		Action: delay one second to let
 *		messages be received
 *
 *	2: We have delayed one second after phase 1
 *		Action: we kill all our "core" children
 *			(read, write, fifo)
 *
 *	We exit/restart after the last of our core children
 *	dies.
 */


gboolean
hb_mcp_final_shutdown(gpointer p)
{
	static int	shutdown_phase = 0;
	guint		id;

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "hb_mcp_final_shutdown() phase %d"
		,	shutdown_phase);
	}
	DisableProcLogging();	/* We're shutting down */

	CL_IGNORE_SIG(SIGTERM);
	switch (shutdown_phase) {

	case 0:		/* From hb_initiate_shutdown -- quickshutdown*/
			/* OR HBDoMsg_T_SHUTDONE -- long shutdown*/
		shutdown_phase = 1;
		send_local_status();
		if (!shutdown_last_client_child(SIGTERM)) {
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				, "hb_mcp_final_shutdown()"
				"- immediate completion.");
			}
			return hb_mcp_final_shutdown(p); /* phase 1 (no children) */
		}
		return FALSE;

	case 1:		/* From ManagedChildDied() (or above) */
		if (NULL != config->last_client) {
			g_list_foreach(config->client_list
			,	print_a_child_client, NULL);
			abort();
		}
		shutdown_phase = 2;
		if (procinfo->restart_after_shutdown) {
                	hb_add_deadtime(30000);
                }
		send_local_status();
		/* THIS IS RESOURCE WORK!  FIXME */
		if (procinfo->giveup_resources) {
			/* Shouldn't *really* need this */
			hb_kill_rsc_mgmt_children(SIGTERM);
		}
		id=Gmain_timeout_add(1000, hb_mcp_final_shutdown /* phase 2 */
		,	NULL);
		G_main_setall_id(id, "shutdown phase 2", 500, 100);
		return FALSE;

	case 2: /* From 1-second delay above */
		shutdown_phase = 3;
		if (procinfo->giveup_resources) {
			/* THIS IS RESOURCE WORK!  FIXME */
			/* Shouldn't *really* need this either ;-) */
			hb_kill_rsc_mgmt_children(SIGKILL);
		}
		/* Kill any lingering processes in our process group */
		CL_KILL(-getpid(), SIGTERM);
		hb_kill_core_children(SIGTERM); /* Is this redundant? */
		hb_tickle_watchdog();
		/* Ought to go down fast now... */
		Gmain_timeout_add(30*1000, EmergencyShutdown, NULL);
		return FALSE;

	default:	/* This should also never be reached */
		hb_emergency_shutdown();
		break;
	}
	hb_close_watchdog();

	/* Whack 'em */
	hb_kill_core_children(SIGKILL);
	cl_log(LOG_INFO,"%s Heartbeat shutdown complete.", localnodename);
	cl_flush_logs();

	if (procinfo->restart_after_shutdown) {
		cl_log(LOG_INFO, "Heartbeat restart triggered.");
		restart_heartbeat();
	} else{
		cleanuptable();
	}

	/*NOTREACHED*/
	cleanexit(0);
	/* NOTREACHED*/
	return FALSE;
}


static void
hb_remove_msg_callback(const char * mtype)
{
	if (message_callbacks == NULL) {
		return;
	}
	g_hash_table_remove(message_callbacks, mtype);
}
void
hb_register_msg_callback(const char * mtype, HBmsgcallback callback)
{
	char * msgtype = g_strdup(mtype);
	
	if (message_callbacks == NULL) {
		message_callbacks = g_hash_table_new(g_str_hash, g_str_equal);
	}
	g_hash_table_insert(message_callbacks, msgtype, callback);
}

void
hb_register_comm_up_callback(void (*callback)(void))
{
	comm_up_callback = callback;
}

static gboolean
HBDoMsgCallback(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	HBmsgcallback cb;

	if ((cb = g_hash_table_lookup(message_callbacks, type))) {
		cb(type, fromnode, msgtime, seqno, iface, msg);
		return TRUE;
	}
	/* It's OK to register for "no one else wants it" with "" */
	if ((cb = g_hash_table_lookup(message_callbacks, ""))) {
		cb(type, fromnode, msgtime, seqno, iface, msg);
		return TRUE;
	}
	return FALSE;
}

static void
free_one_hist_slot(struct msg_xmit_hist* hist, int slot )
{
	struct ha_msg* msg;
	
	msg = hist->msgq[slot];
	if (msg){
		hist->lowseq = hist->seqnos[slot];
		hist->msgq[slot] = NULL;
		if (!cl_is_allocated(msg)) {
			cl_log(LOG_CRIT,
			       "Unallocated slotmsg in %s",
			       __FUNCTION__);
			return;
		}else{
			ha_msg_del(msg);				
		}
	} 	
	
	return;
}



static void 
hist_display(struct msg_xmit_hist * hist)
{
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "hist->ackseq =%ld",     hist->ackseq);
		cl_log(LOG_DEBUG, "hist->lowseq =%ld, hist->hiseq=%ld", 
		       hist->lowseq, hist->hiseq);
		dump_missing_pkts_info();
		
		if (hist->lowest_acknode){
			cl_log(LOG_DEBUG,"expecting from %s",hist->lowest_acknode->nodename);
			cl_log(LOG_DEBUG,"it's ackseq=%ld", hist->lowest_acknode->track.ackseq);
		}
		cl_log(LOG_DEBUG, " ");	
	}
}

static void
reset_lowest_acknode(void)
{
	struct msg_xmit_hist* hist = &msghist;	

	hist->lowest_acknode = NULL;	

	return;
}

static void
HBDoMsg_T_ACKMSG(const char * type, struct node_info * fromnode,
	      TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	const char*		ackseq_str = ha_msg_value(msg, F_ACKSEQ);
	seqno_t			ackseq;
	struct msg_xmit_hist*	hist = &msghist;	
	const char*		to =  (const char*)ha_msg_value(msg, F_TO);
	struct node_info*	tonode;
	seqno_t			new_ackseq = hist->ackseq;
	
	if (!to || (tonode = lookup_tables(to, NULL)) == NULL
	||	tonode != curnode){
		return;
	}

	if (ackseq_str == NULL
	||	sscanf(ackseq_str, "%lx", &ackseq) != 1){
		goto out;
	}

	
	if (ackseq == fromnode->track.ackseq){
		/*dup message*/
		goto out;
	}
	
	if (ackseq <= new_ackseq){
		/* late or dup ack
		 * ignored
		 */
		goto out;
	}else if (ackseq > hist->hiseq){
		cl_log(LOG_ERR, "HBDoMsg_T_ACK"
		       ": corrupted ackseq"
		       " current hiseq = %ld"
		       " ackseq =%ld in this message",
		       hist->hiseq, ackseq);			
		goto out;
	}
	
	if (ackseq < fromnode->track.ackseq) {
		/* late or dup ack
		 * ignored
		 */
		goto out;
	}
	
	fromnode->track.ackseq = ackseq;
	
	if (hist->lowest_acknode != NULL
	&&	STRNCMP_CONST(hist->lowest_acknode->status,DEADSTATUS)==0){
		/* the lowest acked node is dead
		 * we cannot count on that node 
		 * to update our ackseq
		 */
		hist->lowest_acknode = NULL;
	}

	
	if (hist->lowest_acknode == NULL
	||	hist->lowest_acknode == fromnode){
		/*find the new lowest and update hist->ackseq*/
		seqno_t	minseq;
		int	minidx;
		int	i;
		
		hist->lowest_acknode = NULL;
		minidx = -1;
		minseq = 0;
		for (i = 0; i < config->nodecount; i++){
			struct node_info* hip = &config->nodes[i];
			
			if (hip->nodetype == PINGNODE_I
			||	STRNCMP_CONST(hip->status, DEADSTATUS) == 0) {
				continue;
			}
			
			if (minidx == -1
			||	hip->track.ackseq < minseq){
				minseq = hip->track.ackseq;
				minidx = i;
			}
		}
		
		if (minidx == -1) {
			/* Every node is DEADSTATUS */
			goto out;
		}
		if (live_node_count < 2) {
			/*
			 * Update hist->ackseq so we don't hang onto
			 * messages indefinitely and flow control clients
			 */
			if ((hist->hiseq - new_ackseq) >= FLOWCONTROL_LIMIT) {
				new_ackseq = hist->hiseq - (FLOWCONTROL_LIMIT-1);
			}
			hist->lowest_acknode = NULL;
			goto cleanupandout;
		}
		if (minidx >= config->nodecount) {
			cl_log(LOG_ERR, "minidx out of bound"
			       "minidx=%d",minidx );
			goto out;
		}


		if (minseq > 0){
			new_ackseq = minseq;
		}
		hist->lowest_acknode = &config->nodes[minidx];
	}
	
cleanupandout:
	update_ackseq(new_ackseq);
out:
	return;
}

static void
update_ackseq(seqno_t new_ackseq) 
{
	struct msg_xmit_hist*	hist = &msghist;	
	long			count;
	seqno_t			start;
	seqno_t			old_ackseq = hist->ackseq;

#if 0	
	cl_log(LOG_INFO, "new_ackseq = %ld, old_ackseq=%ld"
	,	new_ackseq, old_ackseq);
#endif
	if (new_ackseq <= old_ackseq){
		return;
	}
	hist->ackseq = new_ackseq;

	if ((hist->hiseq - hist->ackseq) < FLOWCONTROL_LIMIT){
		all_clients_resume();
	}

	count = hist->ackseq - hist->lowseq - send_cluster_msg_level;
	if (old_ackseq == 0){
		start = 0;
		count = count - 1;
	}else{
		start = hist->lowseq;
	}
	
	while(count -- > 0){
		/*
		 * If the seq number is greater than the lowseq number
		 * the timer set, we should not free any more messages
		 */
		if (start > timer_lowseq){
			break;
		}
	
		free_one_hist_slot(hist, start%MAXMSGHIST);
		start++;

		if (hist->lowseq > hist->ackseq){
			cl_log(LOG_ERR, "lowseq cannnot be greater than ackseq");
			cl_log(LOG_INFO, "hist->ackseq =%ld, old_ackseq=%ld"
			,	hist->ackseq, old_ackseq);
			cl_log(LOG_INFO, "hist->lowseq =%ld, hist->hiseq=%ld"
			", send_cluster_msg_level=%d"
			,	hist->lowseq, hist->hiseq, send_cluster_msg_level);
			abort();
		}
	}
	(void)dump_missing_pkts_info;

#ifdef DEBUG_FOR_GSHI
	if (ANYDEBUG){
		cl_log(LOG_DEBUG, "hist->ackseq =%ld, node %s's ackseq=%ld",
		       hist->ackseq, fromnode->nodename,
		       fromnode->track.ackseq);
		cl_log(LOG_DEBUG, "hist->lowseq =%ld, hist->hiseq=%ld", 
		       hist->lowseq, hist->hiseq);
		dump_missing_pkts_info();
		
		if (hist->lowest_acknode){
			cl_log(LOG_DEBUG,"expecting from %s",hist->lowest_acknode->nodename);
		}
		cl_log(LOG_DEBUG, " ");
	}

#endif 
}

static int
getnodes(const char* nodelist, char** nodes, int* num){
	
	const char* p;
	int i;
	int j;
	

	memset(nodes, 0, *num);
	i = 0;
	p =  nodelist ; 
	while(*p != 0){
		
		int nodelen;
		
		while(*p == ' ') {
			p++;
		}
		if (*p == 0){
			break;
		}
		nodelen = strcspn(p, " \0") ;
		
		if (i >= *num){
			cl_log(LOG_ERR, "%s: more memory needed(%d given but require %d)",
			       __FUNCTION__, *num, i+1);
			goto errexit;
		}
		
		nodes[i] = cl_malloc(nodelen + 1);
		if (nodes[i] == NULL){
			cl_log(LOG_ERR, "%s: malloc failed", __FUNCTION__);
			goto errexit;
		}

		memcpy(nodes[i], p, nodelen);
		nodes[i][nodelen] = 0;
		p += nodelen;
		i++;
	}	
	
	*num = i;
	return HA_OK;
	
	
 errexit:
	for (j = 0; j < i ; j++){
		if (nodes[j]){
			cl_free(nodes[j]);
			nodes[j] =NULL;
		}
	}
	return HA_FAIL;
}

static int
hb_add_one_node(const char* node)
{
	struct node_info*	thisnode = NULL;	

	cl_log(LOG_INFO,
	       "%s: Adding new node[%s] to configuration.",
	       __FUNCTION__, node);
	
	thisnode = lookup_node(node);
	if (thisnode){
		cl_log(LOG_ERR, "%s: node(%s) already exists",
		       __FUNCTION__, node);
		return HA_FAIL;
	}
       
	add_node(node, NORMALNODE_I);
	thisnode = lookup_node(node);
	if (thisnode == NULL) {
		cl_log(LOG_ERR, "%s: adding node(%s) failed",
		       __FUNCTION__, node);
		return HA_FAIL;
	}
	
	return HA_OK;
	
}

/*
 * Process a request to add a node to the cluster.
 * This can _only_ come from a manual addnode request.
 */
static void
HBDoMsg_T_ADDNODE(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface
,	struct ha_msg * msg)
{
	const char*	nodelist;
	char*		nodes[MAXNODE];
	int		num = MAXNODE;
	int		i;

	nodelist =  ha_msg_value(msg, F_NODELIST);
	if (nodelist == NULL){
		cl_log(LOG_ERR, "%s: nodelist not found in msg",		       
		       __FUNCTION__);
		cl_log_message(LOG_INFO, msg);
		return ;
	}
	
	if (getnodes(nodelist, nodes, &num) != HA_OK){
		cl_log(LOG_ERR, "%s: parsing failed",
		       __FUNCTION__);
		return;
	}
	
	
	for (i = 0; i < num; i++){
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "%s: adding node %s"
			,	__FUNCTION__, nodes[i]);
		}
		if (hb_add_one_node(nodes[i])!= HA_OK){
			cl_log(LOG_ERR, "Add node %s failed", nodes[i]);
		}
		cl_free(nodes[i]);
		nodes[i]=NULL;
	}
	G_main_set_trigger(write_hostcachefile);
	return;
}

/*
 * Process a request to set the quorum vote weight for a node.
 * This can only come from a manual setweight command.
 */
static void
HBDoMsg_T_SETWEIGHT(const char * type, struct node_info * fromnode,
		  TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	const char*	node;
	int		weight;

	node =  ha_msg_value(msg, F_NODE);
	if (node == NULL){
		cl_log(LOG_ERR, "%s: node not found in msg",		       
		       __FUNCTION__);
		cl_log_message(LOG_INFO, msg);
		return ;
	}
	if (ha_msg_value_int(msg, F_WEIGHT, &weight) != HA_OK){			
		cl_log(LOG_ERR, "%s: weight not found in msg",		       
		       __FUNCTION__);
		cl_log_message(LOG_INFO, msg);
		return ;
	}
	if (set_node_weight(node, weight) == HA_OK) {
		G_main_set_trigger(write_hostcachefile);
	}
	return;
}

/*
 * Process a request to set the site for a node.
 * This can only come from a manual setsite command.
 */
static void
HBDoMsg_T_SETSITE(const char * type, struct node_info * fromnode,
		  TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	const char*	node;
	const char*	site;

	node =  ha_msg_value(msg, F_NODE);
	if (node == NULL){
		cl_log(LOG_ERR, "%s: node not found in msg",		       
		       __FUNCTION__);
		cl_log_message(LOG_INFO, msg);
		return ;
	}
	site =  ha_msg_value(msg, F_SITE);
	if (node == NULL){
		cl_log(LOG_ERR, "%s: site not found in msg",		       
		       __FUNCTION__);
		cl_log_message(LOG_INFO, msg);
		return ;
	}
	if (set_node_site(node, site) == HA_OK) {
		G_main_set_trigger(write_hostcachefile);
	}
	return;
}

/*
 * Remove a single node from the configuration - for whatever reason
 * "deletion" is TRUE if it is to be permanently deleted from the
 * configuration and not allowed to autojoin back again.
 */
static int
hb_remove_one_node(const char* node, int deletion)
{
	struct node_info* thisnode = NULL;
	struct ha_msg* removemsg;
	
	cl_log(LOG_INFO,
	       "Removing node [%s] from configuration.",
	       node);
	
	thisnode = lookup_node(node);
	if (thisnode == NULL){
		cl_log(LOG_ERR, "%s: node %s not found in config",
		       __FUNCTION__, node);
		return HA_FAIL;
	}
	
	if (remove_node(node, deletion) != HA_OK){
		cl_log(LOG_ERR, "%s: Deleting node(%s) failed",
		       __FUNCTION__, node);
		return HA_FAIL;
	}
	
	removemsg = ha_msg_new(0);
	if (removemsg == NULL){
		cl_log(LOG_ERR, "%s: creating new message failed",__FUNCTION__);
		return HA_FAIL;
	}

	/*
	 * This message only goes to the CCM, etc. NOT to the network.
	 */
	
	if ( ha_msg_add(removemsg, F_TYPE, T_DELNODE)!= HA_OK
	     || ha_msg_add(removemsg, F_NODE, node) != HA_OK){
		cl_log(LOG_ERR, "%s: adding fields to msg failed", __FUNCTION__);
		return HA_FAIL;
	}

	heartbeat_monitor(removemsg, KEEPIT, NULL);
	reset_lowest_acknode();
	return HA_OK;
	
}



/*
 *	Process a message requesting a node deletion.
 *	This can come ONLY from a manual node deletion.
 */
static void
HBDoMsg_T_DELNODE(const char * type, struct node_info * fromnode,
		  TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	const char*    nodelist;
	char*		nodes[MAXNODE];
	int		num = MAXNODE;
	int		i;
	int		j;		
	
	nodelist =  ha_msg_value(msg, F_NODELIST);
	if (nodelist == NULL){
		cl_log(LOG_ERR, "%s: node not found in msg",
		       __FUNCTION__);
		cl_log_message(LOG_INFO, msg);
		return ;
	}
	
	if (getnodes(nodelist, nodes, &num) != HA_OK){
		cl_log(LOG_ERR, "%s: parsing failed",
		       __FUNCTION__);
		return ;
	}
	
	for (i = 0; i < config->nodecount ;i++){
		gboolean isdelnode =FALSE;
		for (j = 0 ; j < num; j++){
			if (strncmp(config->nodes[i].nodename, 
				    nodes[j],HOSTLENG)==0){
				isdelnode = TRUE;
				break;
			}
		}
		
		if (isdelnode){
			if (STRNCMP_CONST(config->nodes[i].status, DEADSTATUS) != 0){
				cl_log(LOG_WARNING, "deletion failed: node %s is not dead", 
					config->nodes[i].nodename);
				goto out;
			}
	
		}	
	
		if (!isdelnode){
			if ( STRNCMP_CONST(config->nodes[i].status,UPSTATUS) != 0
			     && STRNCMP_CONST(config->nodes[i].status, ACTIVESTATUS) !=0
			     && config->nodes[i].nodetype == NORMALNODE_I){
				cl_log(LOG_ERR, "%s: deletion failed. We don't have"
				       " all required nodes alive (%s is dead)",
				       __FUNCTION__, config->nodes[i].nodename);
				goto out;
			}
		}		
	}
	
	for (i = 0; i < num; i++){
		if (strncmp(nodes[i], curnode->nodename, HOSTLENG) == 0){
			cl_log(LOG_ERR, "I am being deleted from the cluster."
			       " This should not happen");
			hb_initiate_shutdown(FALSE);
			return;
		}

		if (hb_remove_one_node(nodes[i], TRUE)!= HA_OK){
			cl_log(LOG_ERR, "Deleting node %s failed", nodes[i]);
		}
	}
 out:
	for (i = 0; i < num; i++){
		cl_free(nodes[i]);
		nodes[i]= NULL;	
	}
	
	G_main_set_trigger(write_hostcachefile);
	G_main_set_trigger(write_delcachefile);
	
	return ;
	
}

static int
get_nodelist( char* nodelist, int len)
{
	int i;
	char* p;
	int numleft = len;
	
	p = nodelist;
	for (i = 0; i< config->nodecount; i++){
		int tmplen;
		if (config->nodes[i].nodetype != NORMALNODE_I) {
			continue;
		}
		tmplen= snprintf(p, numleft, "%s ", config->nodes[i].nodename);
		p += tmplen;
		numleft -= tmplen;
		if (tmplen <= 0){
			cl_log(LOG_ERR, "%s: not enough buffer", 
			       __FUNCTION__);
			return HA_FAIL;
		}
	}
	
	return HA_OK;

}

static int
get_delnodelist(char* delnodelist, int len)
{
	char* p = delnodelist;
	int numleft = len;
	GSList* list = NULL;

	if (del_node_list == NULL){
		delnodelist[0]= ' ';
		delnodelist[1]=0;
		goto out;
	}
	
	list = del_node_list;

	while( list){
		struct node_info* hip;
		int tmplen; 
		
		hip = (struct node_info*)list->data;
		if (hip == NULL){
			cl_log(LOG_ERR, "%s: null data in del node list",
			       __FUNCTION__);
			return HA_FAIL;
		}
		
		tmplen = snprintf(p, numleft,  "%s ", hip->nodename);
		if (tmplen <= 0){
			cl_log(LOG_ERR, "%s: not enough buffer", 
			       __FUNCTION__);
			return HA_FAIL;
		}
		
		p += tmplen;
		numleft -=tmplen;

		list = list->next;
	}
	
 out: 
	cl_log(LOG_DEBUG, "%s: delnodelist=%s", __FUNCTION__,  delnodelist);
	
	return HA_OK;
}
/*
 * Someone has joined the cluster and asked us for the current set of nodes
 * as modified by addnode and delnode commands or the autojoin option
 * (if enabled), and also the set of semi-permanently deleted nodes.
 *
 * We send them a T_REPNODES message in response - containing that information.
 *
 * We allow dynamic node configuration even if autojoin is disabled.  In that
 * case you need to use the addnode and delnode commands.
 */

static void
HBDoMsg_T_REQNODES(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface
,	struct ha_msg * msg)
{
	char nodelist[MAXLINE];
	char delnodelist[MAXLINE];
	struct ha_msg* repmsg;
	
	if (fromnode == curnode){
		cl_log(LOG_ERR,  "%s: get reqnodes msg from myself!", 
		       __FUNCTION__);
		return;
	}

	if (ANYDEBUG){
		cl_log(LOG_DEBUG, "Get a reqnodes message from %s"
		,	fromnode->nodename);
	}
	
	if (get_nodelist(nodelist, MAXLINE) != HA_OK
	    || get_delnodelist(delnodelist, MAXLINE) != HA_OK){
		cl_log(LOG_ERR, "%s: get node list or del node list from config failed",
		       __FUNCTION__);
		return;
	}

	
	repmsg = ha_msg_new(0);
	if ( repmsg == NULL
	|| ha_msg_add(repmsg, F_TO, fromnode->nodename) != HA_OK
	|| ha_msg_add(repmsg, F_TYPE, T_REPNODES) != HA_OK
	|| ha_msg_add(repmsg, F_NODELIST, nodelist) != HA_OK
	|| ha_msg_add(repmsg, F_DELNODELIST, delnodelist) != HA_OK){
		cl_log(LOG_ERR, "%s: constructing REPNODES msg failed",
		       __FUNCTION__);
		ha_msg_del(repmsg);
		return;	
	} 

	send_cluster_msg(repmsg);
	return;
} 

/*
 * Got our requested reply (T_REPNODES) to our T_REQNODES request.
 * It has the current membership as modified by addnode/delnode commands and
 * autojoin options
 *
 * We allow dynamic node configuration even if autojoin is disabled.  In that
 * case you need to use the addnode and delnode commands.
 */
static void
HBDoMsg_T_REPNODES(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	const char* nodelist = ha_msg_value(msg, F_NODELIST);
	const char* delnodelist = ha_msg_value(msg, F_DELNODELIST);
	char*	nodes[MAXNODE];
	char*   delnodes[MAXNODE];
	int	num =  MAXNODE;
	int	delnum = MAXNODE;
	int	i;
	int 	j;

	if (ANYDEBUG){
		cl_log(LOG_DEBUG,"Get a repnodes msg from %s", fromnode->nodename);
	}
	
	if (fromnode == curnode){
		/*our own REPNODES msg*/
		return;
	}
	
	/* process nodelist*/
	/* our local node list needs to be updated...
	 * Any node that is in nodelist but not in local node list should be
	 * added
	 * Any node that is in local node list but not in nodelist should be
	 * removed (but not deleted)
	 */
	
	/* term definition*/
	/* added: a node in config->nodes[] 
	   deleted: a node in del_node_list
	   removed: remove a node either from config->nodes[] or del_node_list
	*/

	if (nodelist != NULL){
		memset(nodes, 0, MAXNODE);
		if (ANYDEBUG){
			cl_log(LOG_DEBUG, "nodelist received:%s", nodelist);
		}
		if (getnodes(nodelist, nodes, &num) != HA_OK){
			cl_log(LOG_ERR, "%s: get nodes from nodelist failed",
				__FUNCTION__);
			return;
		}
		for (i=0; i < num; i++){
			for (j = 0; j < config->nodecount; j++){
				if (strncmp(nodes[i], config->nodes[j].nodename
				,	HOSTLENG) == 0){
					break;
				}
			}
			if (j == config->nodecount){
				/*
				 * This node is not found in config -
				 * we need to add it...
				 */
				if (ANYDEBUG) {
					cl_log(LOG_DEBUG
					,	"%s: adding node %s"
					,	__FUNCTION__, nodes[i]);
				}
				hb_add_one_node(nodes[i]);		
			}else if (config->nodes[j].nodetype != NORMALNODE_I){
				cl_log(LOG_ERR
				,	"%s: Incoming %s node list contains %s"
				,	__FUNCTION__
				,	T_REPNODES
				,	config->nodes[i].nodename);
			}
		}
		
		for (i=0; i < config->nodecount; i++){
			if (config->nodes[i].nodetype != NORMALNODE_I){
				continue;
			}
			for (j=0; j < num; j++){
				if (strncmp(config->nodes[i].nodename
				,	nodes[j], HOSTLENG) == 0){
					break;
				}	
			}
			if (j == num) {
				/*
				 * This node is not found in incoming nodelist,
				 * therefore, we need to remove it from
				 * config->nodes[]
				 *
				 * This assumes everyone the partner node we
				 * sent the reqnodes message to has the current
				 * configuration.
				 *
				 * The moral of the story is that you need to
				 * not add and delete nodes by ha.cf on live
				 * systems.
				 *
				 * If you use addnode and delnode commands then
				 * everything should be OK here.
				 */
				hb_remove_one_node(config->nodes[i].nodename
				,	FALSE);
			}
		}
		for (i = 0; i< num; i++){
			if (nodes[i]) {
				cl_free(nodes[i]);
				nodes[i] = NULL;
			}
		}
		get_reqnodes_reply = TRUE;
		G_main_set_trigger(write_hostcachefile);
	}

	if (delnodelist != NULL) {	
		memset(delnodes, 0, MAXNODE);
		if (getnodes(delnodelist, delnodes, &delnum) != HA_OK){	       
			cl_log(LOG_ERR, "%s: get del nodes from nodelist failed",
				__FUNCTION__);
			return;
		}
		/* process delnodelist*/
		/* update our del node list to be the exact same list as the received one
		*/
		dellist_destroy();
		for (i = 0; i < delnum; i++){
			dellist_add(delnodes[i]);
		}	
	
		for (i = 0; i < delnum; i++){
			if (delnodes[i]){
				cl_free(delnodes[i]);
				delnodes[i] = NULL;
			}
		}
		get_reqnodes_reply = TRUE;
		G_main_set_trigger(write_delcachefile);
	}
	comm_now_up();
        return;
}


static void
HBDoMsg_T_REXMIT(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	heartbeat_monitor(msg, PROTOCOL, iface);
	if (fromnode != curnode) {
		process_rexmit(&msghist, msg);
	}
}

/* Process status update (i.e., "heartbeat") message? */
static void
HBDoMsg_T_STATUS(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{

	const char *	status;
	longclock_t		messagetime = time_longclock();
	const char	*tmpstr;
	long		deadtime;
	int		protover;

	status = ha_msg_value(msg, F_STATUS);
	if (status == NULL)  {
		cl_log(LOG_ERR, "HBDoMsg_T_STATUS: "
		"status update without "
		F_STATUS " field");
		return;
	}
	
	/* Does it contain F_PROTOCOL field?*/
	

	/* Do we already have a newer status? */
	if (msgtime < fromnode->rmt_lastupdate
	&&		seqno < fromnode->status_seqno) {
		return;
	}

	/* Have we seen an update from here before? */
	if (fromnode->nodetype != PINGNODE_I
	    && enable_flow_control 
	    && ha_msg_value_int(msg, F_PROTOCOL, &protover) != HA_OK){		
		cl_log(LOG_INFO, "flow control disabled due to different version heartbeat");
		enable_flow_control = FALSE;
		hb_remove_msg_callback(T_ACKMSG);
	}

	if (fromnode->local_lastupdate) {
		long		heartbeat_ms;
		heartbeat_ms = longclockto_ms(sub_longclock
		(	messagetime, fromnode->local_lastupdate));

		if (heartbeat_ms > config->warntime_ms) {
			cl_log(LOG_WARNING
			,	"Late heartbeat: Node %s:"
			" interval %ld ms"
			,	fromnode->nodename
			,	heartbeat_ms);
		}
	}


	/* Is this a second status msg from a new node? */
	if (fromnode->status_suppressed && fromnode->saved_status_msg) {
		fromnode->status_suppressed = FALSE;
		QueueRemoteRscReq(PerformQueuedNotifyWorld
		,	fromnode->saved_status_msg);
		heartbeat_monitor(fromnode->saved_status_msg, KEEPIT, iface);
		ha_msg_del(fromnode->saved_status_msg);
		fromnode->saved_status_msg = NULL;
	}
	/* Is the node status the same? */
	if (strcasecmp(fromnode->status, status) != 0
	&&	fromnode != curnode) {
		cl_log(LOG_INFO
		       ,	"Status update for node %s: status %s"
		,	fromnode->nodename
		,	status);
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			,	"Status seqno: %ld msgtime: %ld"
			,	seqno, msgtime);
		}
		/*
		 * If the restart of a node is faster than deadtime,
		 * the previous status of node would be still ACTIVE 
		 * while current status is INITSTATUS.
		 * So we reduce the live_node_count here.
		 */
		if (fromnode->nodetype == NORMALNODE_I
		&&	fromnode != curnode
		&&	( STRNCMP_CONST(fromnode->status, ACTIVESTATUS) == 0
		||	  STRNCMP_CONST(fromnode->status, UPSTATUS) == 0)
		&&	( STRNCMP_CONST(status, INITSTATUS) == 0)) {
			--live_node_count;
			if (live_node_count < 1) {
				cl_log(LOG_ERR
				,	"live_node_count too small (%d)"
				,	live_node_count);
			}
		}
		/*
		 *   IF
		 *	It's from a normal node
		 *	It isn't from us
		 *	The node's old status was dead or init
		 *	The node's new status is up or active
		 *   THEN
		 *	increment the count of live nodes.
		 */
		if (fromnode->nodetype == NORMALNODE_I
		&&	fromnode != curnode
		&&	( STRNCMP_CONST(fromnode->status, DEADSTATUS) == 0
		||	  STRNCMP_CONST(fromnode->status, INITSTATUS) == 0)
		&&	( STRNCMP_CONST(status, UPSTATUS) == 0
		||	  STRNCMP_CONST(status, ACTIVESTATUS) == 0)) {
			++live_node_count;
			if (live_node_count > config->nodecount) {
				cl_log(LOG_ERR
				,	"live_node_count too big (%d)"
				,	live_node_count);
			}
		}
		
		strncpy(fromnode->status, status, sizeof(fromnode->status));
		if (!fromnode->status_suppressed) {
			QueueRemoteRscReq(PerformQueuedNotifyWorld, msg);
			heartbeat_monitor(msg, KEEPIT, iface);
		}else{
			/* We know we don't already have a saved msg */
			fromnode->saved_status_msg = ha_msg_copy(msg);
		}
	}else{
		heartbeat_monitor(msg, NOCHANGE, iface);
	}
	if ((tmpstr = ha_msg_value(msg, F_DT)) != NULL
	&&	sscanf(tmpstr, "%lx", (unsigned long*)&deadtime) == 1) {
		fromnode->dead_ticks = msto_longclock(deadtime);	
	}
	
	/* Did we get a status update on ourselves? */
	if (fromnode == curnode) {
		hb_tickle_watchdog();
	}

	fromnode->rmt_lastupdate = msgtime;
	fromnode->local_lastupdate = messagetime;
	fromnode->status_seqno = seqno;

}

static void /* This is a client status query from remote client */
HBDoMsg_T_QCSTATUS(const char * type, struct node_info * fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	const char * clientid;
	const char * fromclient;
	struct ha_msg * m = NULL;
	int ret = HA_FAIL;
	
	if ((clientid = ha_msg_value(msg, F_CLIENTNAME)) == NULL
	    || (fromclient = ha_msg_value(msg, F_FROMID)) == NULL) {
		cl_log(LOG_ERR, "%s ha_msg_value failed", __FUNCTION__);
		return;
	}
	if ((m = ha_msg_new(0)) == NULL){
		cl_log(LOG_ERR, "%s Cannot add field", __FUNCTION__);
		return;
	}
	if (ha_msg_add(m, F_TYPE, T_RCSTATUS) != HA_OK
	||	ha_msg_add(m, F_TO, fromnode->nodename) != HA_OK
	||	ha_msg_add(m, F_APIRESULT, API_OK) != HA_OK
	||	ha_msg_add(m, F_CLIENTNAME, clientid) != HA_OK
	||	ha_msg_add(m, F_TOID, fromclient) != HA_OK) {
		cl_log(LOG_ERR, "Cannot create clent status msg");
		return;
	}
	if (find_client(clientid, NULL) != NULL) {
		ret = ha_msg_add(m, F_CLIENTSTATUS, ONLINESTATUS);
	}else{
		ret = ha_msg_add(m, F_CLIENTSTATUS, OFFLINESTATUS);
	}

	if (ret != HA_OK) {
		cl_log(LOG_ERR, "Cannot create clent status msg");
		return;
	}
	send_cluster_msg(m);
}




static void
update_client_status_msg_list(struct node_info* thisnode)
{
	struct seqtrack *	t = &thisnode->track;
		
	if(t->client_status_msg_queue){			
		
		struct ha_msg*		msg ;
		GList*			listrunner;
		seqno_t			seq;
		const char *		cseq;
		
		
		while ((listrunner = g_list_first(t->client_status_msg_queue))
		       != NULL){
			
			msg = (struct ha_msg*) listrunner->data;
			
			cseq = ha_msg_value(msg, F_SEQ);
			if (cseq  == NULL 
			    || sscanf(cseq, "%lx", &seq) != 1 
			    ||	seq <= 0) {
				cl_log(LOG_ERR, "bad sequence number");
				if (cseq){
					cl_log(LOG_INFO, "cseq =%s", cseq);
				}
				return;
			}
			
			if ( t->first_missing_seq == 0 
			     || seq < t->first_missing_seq){
				/* deliver the message to client*/
				
				cl_log(LOG_DEBUG, "delivering client status "
				       "message to a client"				       
				       " from queue");
				heartbeat_monitor(msg, KEEPIT, NULL);
				
				ha_msg_del(msg);
				
				t->client_status_msg_queue = 
					g_list_delete_link(t->client_status_msg_queue,
							   listrunner);				
				
			}else{
				break;
			}	
			
		}
		
		if (g_list_length(t->client_status_msg_queue) == 0){
			g_list_free(t->client_status_msg_queue);
			t->client_status_msg_queue = NULL;
			
			cl_log(LOG_DEBUG,"client_status_msg_queue"
			       "for node %s destroyed",
			       thisnode->nodename);
		}
	}

	return;
}
static void
send_ack(struct node_info* thisnode, seqno_t seq)
{
	struct ha_msg*	hmsg;
	char		seq_str[32];
	
	if ((hmsg = ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "no memory for " T_ACKMSG);
		return;
	}
	
	sprintf(seq_str, "%lx",seq);
	
	if (ha_msg_add(hmsg, F_TYPE, T_ACKMSG) == HA_OK &&
	    ha_msg_add(hmsg, F_TO, thisnode->nodename) == HA_OK &&
	    ha_msg_add(hmsg, F_ACKSEQ,seq_str) == HA_OK) {
		
		if (send_cluster_msg(hmsg) != HA_OK) {
			cl_log(LOG_ERR, "cannot send " T_ACKMSG
			       " request to %s", thisnode->nodename);
		}

	}else{
		ha_msg_del(hmsg);
		cl_log(LOG_ERR, "Cannot create " T_REXMIT " message.");
	}
	
	return;
}



static void
send_ack_if_needed(struct node_info* thisnode, seqno_t seq)
{
	struct seqtrack* t = &thisnode->track;
        seqno_t		fm_seq = t->first_missing_seq;
	
	if (!enable_flow_control){
		return;
	}
	
	if ( (fm_seq != 0 && seq > fm_seq) ||
	     seq % ACK_MSG_DIV != thisnode->track.ack_trigger){
		/*no need to send ACK */
		return;
	}	
	
	send_ack(thisnode, seq);
	return;
}




static void
send_ack_if_necessary(const struct ha_msg* m)
{
	const char*	fromnode = ha_msg_value(m, F_ORIG);
	cl_uuid_t	fromuuid;
	const char*	seq_str = ha_msg_value(m, F_SEQ);
	seqno_t		seq;
	struct	node_info*	thisnode = NULL;

	if (!enable_flow_control){
		return;
	}
	
	if ( cl_get_uuid(m, F_ORIGUUID, &fromuuid) != HA_OK){
		cl_uuid_clear(&fromuuid);
	}
	
	if (fromnode == NULL ||
	    seq_str == NULL ||
	    sscanf( seq_str, "%lx", &seq) != 1){		
		return;
	}
	
	thisnode = lookup_tables(fromnode, &fromuuid);
	if (thisnode == NULL){
		
		cl_log(LOG_ERR, "node %s not found "
		       "bad message",
		       fromnode);
		return;		
	}
	
	send_ack_if_needed(thisnode, seq);
	
}

/*
 * Process an incoming message from our read child processes
 * That is, packets coming from other nodes.
 */
static void
process_clustermsg(struct ha_msg* msg, struct link* lnk)
{
	struct node_info *	thisnode = NULL;
	const char*		iface;
	TIME_T			msgtime = 0;
	longclock_t		now = time_longclock();
	const char *		from;
	cl_uuid_t		fromuuid;
	const char *		ts;
	const char *		type;
	int			action;
	const char *		cseq;
	seqno_t			seqno = 0;
	longclock_t		messagetime = now;
	int			missing_packet =0 ;


	if (lnk == NULL) {
		iface = "?";
	}else{
		iface = lnk->name;
	}

	/* FIXME: We really ought to use gmainloop timers for this */
	if (cmp_longclock(standby_running, zero_longclock) != 0) {
		if (DEBUGDETAILS) {
			unsigned long	msleft;
			msleft = longclockto_ms(sub_longclock(standby_running
			,	now));
			cl_log(LOG_WARNING, "Standby timer has %ld ms left"
			,	msleft);
		}

		/*
                 * If there's a standby timer running, verify if it's
                 * time to enable the standby messages again...
                 */
		if (cmp_longclock(now, standby_running) >= 0) {
			standby_running = zero_longclock;
			other_is_stable = 1;
			going_standby = NOT;
			cl_log(LOG_WARNING, "No reply to standby request"
			".  Standby request cancelled.");
			hb_shutdown_if_needed();
		}
	}

	/* Extract message type, originator, timestamp, auth */
	type = ha_msg_value(msg, F_TYPE);
	from = ha_msg_value(msg, F_ORIG);
	
	if ( cl_get_uuid(msg, F_ORIGUUID, &fromuuid) != HA_OK){
		cl_uuid_clear(&fromuuid);
	}
	ts = ha_msg_value(msg, F_TIME);
	cseq = ha_msg_value(msg, F_SEQ);

	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		       ,       "process_clustermsg: node [%s]"
		       ,	from ? from :"?");
	}

	if (from == NULL || ts == NULL || type == NULL) {
		cl_log(LOG_ERR
		,	"process_clustermsg: %s: iface %s, from %s"
		,	"missing from/ts/type"
		,	iface
		,	(from? from : "<?>"));
		cl_log_message(LOG_ERR, msg);
		return;
	}
	if (cseq != NULL) {
		if (sscanf(cseq, "%lx", &seqno) <= 0) {
			cl_log(LOG_ERR
			,	"process_clustermsg: %s: iface %s, from %s"
			,	"has bad cseq"
			,	iface
			,	(from? from : "<?>"));
			cl_log_message(LOG_ERR, msg);
			return;
		}
	}else{
		seqno = 0L;
		if (strncmp(type, NOSEQ_PREFIX, STRLEN_CONST(NOSEQ_PREFIX)) != 0) {
			cl_log(LOG_ERR
			,	"process_clustermsg: %s: iface %s, from %s"
			,	"missing seqno"
			,	iface
			,	(from? from : "<?>"));
			cl_log_message(LOG_ERR, msg);
			return;
		}
	}


	

	if (sscanf(ts, TIME_X, &msgtime) != 1 || ts == 0 || msgtime == 0) {
		return;
	}
	
	thisnode = lookup_tables(from, &fromuuid);
	
	if (thisnode == NULL) {
		if (config->rtjoinconfig == HB_JOIN_NONE) {
			/* If a node isn't in our config - whine */
			cl_log(LOG_ERR
			,   "process_status_message: bad node [%s] in message"
			,	from);
			cl_log_message(LOG_ERR, msg);
			return;
		}else{
			/* If a node isn't in our config, then add it... */
			cl_log(LOG_INFO
			,   "%s: Adding new node [%s] to configuration."
			,	__FUNCTION__, from);
			add_node(from, NORMALNODE_I);
			thisnode = lookup_node(from);
			if (thisnode == NULL) {
				return;
			}
			/*
			 * Suppress status updates to our clients until we
			 * hear the second heartbeat from the new node.
			 *
			 * We've already updated the node table and we will
			 * report its status if asked...
			 *
			 * This may eliminate an extra round of the membership
			 * protocol.
			 */
			thisnode->status_suppressed = TRUE;
			update_tables(from, &fromuuid);
			G_main_set_trigger(write_hostcachefile);
			return;
		}
	}

	/* Throw away some incoming packets if testing is enabled */
	if (TESTRCV) {
		if (thisnode != curnode &&  TestRand(rcv_loss_prob)) {
			char* match = strstr(TestOpts->allow_nodes,from);
			if ( NULL == match || ';' != *(match+strlen(from)) ) {
				return;
			}
		}
	}
	thisnode->anypacketsyet = 1;

	lnk = lookup_iface(thisnode, iface);

	/* Is this message a duplicate, or destined for someone else? */

	action=should_drop_message(thisnode, msg, iface, &missing_packet);
	switch (action) {
		case DROPIT:
		/* Ignore it */
		heartbeat_monitor(msg, action, iface);
		return;
		
		case DUPLICATE:
		heartbeat_monitor(msg, action, iface);
		/* fall through */
		case KEEPIT:

		/* Even though it's a DUP, it could update link status*/
		if (lnk) {
			lnk->lastupdate = messagetime;
			/* Is this from a link which was down? */
			if (strcasecmp(lnk->status, LINKUP) != 0) {
				change_link_status(thisnode, lnk
				,	LINKUP);
			}
		}
		if (action == DUPLICATE) {
			return;
		}
		break;
	}
	
	
	thisnode->track.last_iface = iface;

	if (HBDoMsgCallback(type, thisnode, msgtime, seqno, iface, msg)) {
		/* See if our comm channels are working yet... */
		if (heartbeat_comm_state != COMM_LINKSUP) {
			check_comm_isup();
		}	
	}else{
		/* Not a message anyone wants (yet) */
		if (heartbeat_comm_state != COMM_LINKSUP) {
			check_comm_isup();
			/* Make sure we don't lose this one message... */
			if (heartbeat_comm_state == COMM_LINKSUP) {
				/* Someone may have registered for this one */
				if (!HBDoMsgCallback(type, thisnode, msgtime
					,	seqno, iface,msg)) {
					heartbeat_monitor(msg, action, iface);
				}
			}
		}else{
			heartbeat_monitor(msg, action, iface);
		}
	}

	/* if this packet is a missing packet, 
	 * need look at 
	 * client status message list to see
	 * if we can deliver any
	 */
	
	if (missing_packet){		
		update_client_status_msg_list(thisnode);
		
	}
}


void
check_auth_change(struct sys_config *conf)
{
	if (conf->rereadauth) {
		return_to_orig_privs();
		/* parse_authfile() resets 'rereadauth' */
		if (parse_authfile() != HA_OK) {
			/* OOPS.  Sayonara. */
			cl_log(LOG_ERR
			,	"Authentication reparsing error, exiting.");
			hb_initiate_shutdown(FALSE);
			cleanexit(1);
		}
		return_to_dropped_privs();
		conf->rereadauth = FALSE;
	}
}
static int
hb_compute_authentication(int authindex, const void * data, size_t datalen
,	char * authstr, size_t authlen)
{
	struct HBAuthOps *	at;

	check_auth_change(config);
	if (authindex < 0) {
		authindex = config->authnum;
	}
	if (authindex < 0 || authindex >= MAXAUTH
	||	 ((at = config->auth_config[authindex].auth)) == NULL) {
		return HA_FAIL;
	}
	if (!at->auth(config->authmethod, data, datalen, authstr, authlen)) {
		ha_log(LOG_ERR 
		,	"Cannot compute message auth string [%s/%s/%s]"
		,	config->authmethod->authname
		,	config->authmethod->key
		,	(const char *)data);
		return -2;
	}
	return authindex;
}


/***********************************************************************
 * Track the core heartbeat processes
 ***********************************************************************/

/* Log things about registered core processes */
static void
CoreProcessRegistered(ProcTrack* p)
{
	++CoreProcessCount;

	if (p->pid > 0) {
		processes[procinfo->nprocs] = p->pid;
		procinfo->info[procinfo->nprocs].pstat = FORKED;
		procinfo->info[procinfo->nprocs].pid = p->pid;
		procinfo->nprocs++;
	}

}

/* Handle the death of a core heartbeat process */
static void
CoreProcessDied(ProcTrack* p, int status, int signo
,	int exitcode, int waslogged)
{
	-- CoreProcessCount;

	if (shutdown_in_progress) {
		p->privatedata = NULL;
		cl_log(LOG_INFO,"Core process %d exited. %d remaining"
		,	(int) p->pid, CoreProcessCount);

		if (CoreProcessCount <= 1) {
			cl_log(LOG_INFO,"%s Heartbeat shutdown complete.",
			       localnodename);
			if (procinfo->restart_after_shutdown) {
				cl_log(LOG_INFO
				,	"Heartbeat restart triggered.");
				restart_heartbeat();
			}
			cl_flush_logs();
			cleanexit(0);
		}
		return;
	}
	/* UhOh... */
	cl_log(LOG_ERR
	,	"Core heartbeat process died! Restarting.");
	cause_shutdown_restart();
	p->privatedata = NULL;
	return;
}

static const char *
CoreProcessName(ProcTrack* p)
{
	/* This is perfectly safe - procindex is a small int */
	int	procindex = POINTER_TO_SIZE_T(p->privatedata);/*pointer cast as int*/
	volatile struct process_info *	pi = procinfo->info+procindex;

	return (pi ? core_proc_name(pi->type) : "Core heartbeat process");
	
}

/***********************************************************************
 * Track our managed child processes...
 ***********************************************************************/

static void
ManagedChildRegistered(ProcTrack* p)
{
	struct client_child*	managedchild = p->privatedata;

	managed_child_count++;
	managedchild->pid = p->pid;
	managedchild->proctrack = p;
}

/* Handle the death of one of our managed child processes */
static void
ManagedChildDied(ProcTrack* p, int status, int signo, int exitcode
,	int waslogged)
{
	struct client_child*	managedchild = p->privatedata;
	
	/*remove the child from API client table*/
	api_remove_client_pid(p->pid, "died");

	managedchild->pid = 0;
	managedchild->proctrack = NULL;
	managed_child_count --;


	/* Log anything out of the ordinary... */
	if ((!shutdown_in_progress && !waslogged) || (ANYDEBUG)) {
		if (0 != exitcode) {
			cl_log(shutdown_in_progress ? LOG_DEBUG : LOG_ERR
			,	"Client %s exited with return code %d."
			,	managedchild->command
			,	exitcode);
		}
		if (0 != signo) {
			cl_log(shutdown_in_progress ? LOG_DEBUG : LOG_ERR
			,	"Client %s (pid=%d) killed by signal %d."
			,	managedchild->command
			,	(int)p->pid
			,	signo);
		}
	}
	if (managedchild->rebootifitdies) {
		if (signo != 0 || ((exitcode != 0 && !shutdown_in_progress))) {
			/* Fail fast and safe - reboot this machine.
			 * I'm not 100% sure whether we should do this for all
			 * exits outside of shutdown intervals, but it's
			 * clear that we should reboot in case of abnormal
			 * exits...
 		 	 */
			cl_reboot(config->heartbeat_ms, managedchild->command);
		}
	}

	/* If they exit 100 we won't restart them */
	if (managedchild->respawn && !shutdown_in_progress
	&&	exitcode != 100) {
		longclock_t	now = time_longclock();
		longclock_t	minticks = msto_longclock(30000);
		longclock_t	shorttime
		=	add_longclock(p->startticks, minticks);

		++managedchild->respawncount;

		if (cmp_longclock(now, shorttime) < 0) {
			++managedchild->shortrcount;
		}else{
			managedchild->shortrcount = 0;
		}
		if (managedchild->shortrcount > 10) {
			cl_log(LOG_ERR
			,	"Client %s \"%s\""
			,	managedchild->command
			,	"respawning too fast");
			managedchild->shortrcount = 0;
		}else{
			cl_log(LOG_ERR
			,	"Respawning client \"%s\":"
			,	managedchild->command);
			start_a_child_client(managedchild, NULL);
		}
	}
	p->privatedata = NULL;
	if (shutdown_in_progress) {
                if (g_list_find(config->client_list, managedchild)
		!=	config->last_client){
			/* Child died prematurely, ignore it and return */
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				,	"client \"%s\" died early during"
				" shutdown."
				,	managedchild->command);
			}
			return; 
		}
		config->last_client = config->last_client->prev;
		if (!shutdown_last_client_child(SIGTERM)) {
			if (config->last_client) {
				cl_log(LOG_ERR
				,	"ManagedChildDied()"
				": config->last_client != NULL");
			}
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				,	"Final client \"%s\" died."
				,	managedchild->command);
			}
                       /* Trigger next shutdown phase */
			hb_mcp_final_shutdown(NULL); /* phase 1 -	*/
						     /* last child died	*/
		}
	}
}

/* Handle the death of one of our managed child processes */

static const char *
ManagedChildName(ProcTrack* p)
{
		struct client_child*	managedchild = p->privatedata;
		return managedchild->command;
}


void
hb_kill_tracked_process(ProcTrack* p, void * data)
{
	/* This is perfectly safe - procindex is a small int */
	int	nsig = POINTER_TO_SIZE_T(data); /*pointer cast as int*/
	int	pid = p->pid;
	const char *	porg;
	const char * pname;

	pname = p->ops->proctype(p);

	if (p->isapgrp) {
		pid = -p->pid;
		porg = "process group";
	}else{
		pid =  p->pid;
		porg = "process";
		/* We never signal ourselves */
		if (pid == getpid()) {
			return;
		}
	}
	cl_log(LOG_INFO, "killing %s %s %d with signal %d", pname, porg
	,	(int) p->pid, nsig);
	/* Suppress logging this process' death */
	p->loglevel = PT_LOGNONE;
	return_to_orig_privs();
	CL_KILL(pid, nsig);
	return_to_dropped_privs();
}


static void
print_a_child_client(gpointer childentry, gpointer unused)
{
	struct client_child*	centry = childentry;

	if (centry->proctrack) {
		cl_log(LOG_DEBUG
		,	"RUNNING Child client \"%s\" (%d,%d) pid %d"
		,	centry->command, (int) centry->u_runas
		,	(int) centry->g_runas
		,	centry->pid);
	}else{
		cl_log(LOG_DEBUG
		,	"Idle Child client \"%s\" (%d,%d)"
		,	centry->command, (int) centry->u_runas
		,	(int) centry->g_runas);
	}
}
static void
start_a_child_client(gpointer childentry, gpointer dummy)
{
	struct client_child*	centry = childentry;
	pid_t			pid;
	struct passwd*		pwent;

	cl_log(LOG_INFO, "Starting child client \"%s\" (%d,%d)"
	,	centry->command, (int) centry->u_runas
	,	(int) centry->g_runas);

	if (centry->pid != 0) {
		cl_log(LOG_ERR, "OOPS! client %s already running as pid %d"
		,	centry->command, (int) centry->pid);
	}

	/*
	 * We need to ensure that the exec will succeed before
	 * we bother forking.  We don't want to respawn something that
	 * won't exec in the first place.
	 */

	if (access(centry->path, F_OK|X_OK) < 0) {
		cl_perror("Cannot exec %s", centry->command);
		return;
	}
	hb_add_deadtime(2000);

	/* We need to fork so we can make child procs not real time */
	switch(pid=fork()) {

		case -1:	cl_log(LOG_ERR
				,	"start_a_child_client: Cannot fork.");
				return;

		default:	/* Parent */
				NewTrackedProc(pid, 1, PT_LOGVERBOSE
				,	centry, &ManagedChildTrackOps);
				hb_pop_deadtime(NULL);
				return;

		case 0:		/* Child */
				break;
	}

	/* Child process:  start the managed child */
	hb_setup_child();
	setpgid(0,0);

	/* Limit peak resource usage, maximize success chances */
	if (centry->shortrcount > 0) {
		alarm(0);
		sleep(1);
	}

	cl_log(LOG_INFO, "Starting \"%s\" as uid %d  gid %d (pid %d)"
	,	centry->command, (int) centry->u_runas
	,	(int) centry->g_runas, (int) getpid());

	if (	(pwent = getpwuid(centry->u_runas)) == NULL
	||	initgroups(pwent->pw_name, centry->g_runas) < 0
	||	setgid(centry->g_runas) < 0
	||	setuid(centry->u_runas) < 0
	||	CL_SIGINTERRUPT(SIGALRM, 0) < 0) {

		cl_perror("Cannot setup child process %s"
		,	centry->command);
	}else{
		const char *	devnull = "/dev/null";
		unsigned int	j;
		struct rlimit		oflimits;
		char *cmdexec = NULL;
		size_t		cmdsize;
#define		CMDPREFIX	"exec "

		CL_SIGNAL(SIGCHLD, SIG_DFL);
		alarm(0);
		CL_IGNORE_SIG(SIGALRM);

		/* A precautionary measure */
		getrlimit(RLIMIT_NOFILE, &oflimits);
		for (j=0; j < oflimits.rlim_cur; ++j) {
			close(j);
		}
		(void)open(devnull, O_RDONLY);	/* Stdin:  fd 0 */
		(void)open(devnull, O_WRONLY);	/* Stdout: fd 1 */
		(void)open(devnull, O_WRONLY);	/* Stderr: fd 2 */
		cmdsize = STRLEN_CONST(CMDPREFIX)+strlen(centry->command)+1;

		cmdexec = cl_malloc(cmdsize);
		if (cmdexec != NULL) {
			strlcpy(cmdexec, CMDPREFIX, cmdsize);
			strlcat(cmdexec, centry->command, cmdsize);
			(void)execl("/bin/sh", "sh", "-c", cmdexec
			, (const char *)NULL); 
		}

		/* Should not happen */
		cl_perror("Cannot exec %s", centry->command);
	}
	/* Suppress respawning */
	exit(100);
}

static gboolean		/* return TRUE if any child was signalled */
shutdown_last_client_child(int nsig)
{
	GList*			last;
	struct client_child*	lastclient;

	if (NULL == (last = config->last_client)) {
		return FALSE;
	}
	lastclient = last->data;
	if (lastclient) {
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "Shutting down client %s"
			,	lastclient->command);
		}
		lastclient->respawn = FALSE;
		if (lastclient->proctrack) {
			hb_kill_tracked_process(lastclient->proctrack
			,	GINT_TO_POINTER(nsig));
			return TRUE;
		}
		cl_log(LOG_INFO, "client [%s] is not running."
		,	lastclient->command);
	}else{
		cl_log(LOG_ERR, "shutdown_last_clent_child(NULL client)");
	}
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "shutdown_last_client_child: Try next one.");
	}
	/* OOPS! Couldn't kill a process this time... Try the next one... */
	config->last_client = config->last_client->prev;
	return shutdown_last_client_child(nsig);
}

static const char *
core_proc_name(enum process_type t)
{
	const char *	ct = "huh?";
	switch(t) {
		case PROC_UNDEF:	ct = "UNDEF";		break;
		case PROC_MST_CONTROL:	ct = "MST_CONTROL";	break;
		case PROC_HBREAD:	ct = "HBREAD";		break;
		case PROC_HBWRITE:	ct = "HBWRITE";		break;
		case PROC_HBFIFO:	ct = "HBFIFO";		break;
		case PROC_PPP:		ct = "PPP";		break;
		default:		ct = "core process??";	break;
	}
	return ct;
}

void
hb_dump_proc_stats(volatile struct process_info * proc)
{
	const char *	ct;
	unsigned long	curralloc;
	volatile cl_mem_stats_t	*ms;

	if (!proc) {
		return;
	}

	ct = core_proc_name(proc->type);

	cl_log(LOG_INFO, "MSG stats: %ld/%ld ms age %ld [pid%d/%s]"
	,	proc->msgstats.allocmsgs, proc->msgstats.totalmsgs
	,	longclockto_ms(sub_longclock(time_longclock()
	,		proc->msgstats.lastmsg))
	,	(int) proc->pid, ct);

	
	ms = &proc->memstats;
	if (ms->numalloc > ms->numfree) {
		curralloc = ms->numalloc - ms->numfree;
	}else{
		curralloc = 0;
	}

	cl_log(LOG_INFO, "cl_malloc stats: %lu/%lu  %lu/%lu [pid%d/%s]"
	,	curralloc, ms->numalloc
	,	ms->nbytes_alloc, ms->nbytes_req, (int) proc->pid, ct);

	cl_log(LOG_INFO, "RealMalloc stats: %lu total malloc bytes."
	" pid [%d/%s]", ms->mallocbytes, (int) proc->pid, ct);

#ifdef HAVE_MALLINFO
	cl_log(LOG_INFO, "Current arena value: %lu", ms->arena);
#endif
}


/*
 *	Restart heartbeat - we never return from this...
 */
static void
restart_heartbeat(void)
{
	unsigned int		j;
	struct rlimit		oflimits;
	int			quickrestart;

	shutdown_in_progress = 1;
	cl_make_normaltime();
	return_to_orig_privs();	/* Remain privileged 'til the end */
	cl_log(LOG_INFO, "Restarting heartbeat.");
	/* THIS IS RESOURCE WORK!  FIXME */
	quickrestart = (procinfo->giveup_resources ? FALSE : TRUE);

	cl_log(LOG_INFO, "Performing heartbeat restart exec.");

	hb_close_watchdog();

	getrlimit(RLIMIT_NOFILE, &oflimits);
	for (j=3; j < oflimits.rlim_cur; ++j) {
		close(j);
	}

	
	if (quickrestart) {
		/* THIS IS RESOURCE WORK!  FIXME */
		if (nice_failback) {
			cl_log(LOG_INFO, "Current resources: -R -C %s"
			,	decode_resources(procinfo->i_hold_resources));
			execl(HA_LIBHBDIR "/heartbeat", "heartbeat", "-R"
			,	"-C"
			,	decode_resources(procinfo->i_hold_resources)
			,	(const char *)NULL);
		}else{
			execl(HA_LIBHBDIR "/heartbeat", "heartbeat", "-R"
			,	(const char *)NULL);
		}
	}else{
		/* Make sure they notice we're dead */
		sleep((config->deadtime_ms+999)/1000+1);
		/* "Normal" restart (not quick) */
		cl_unlock_pidfile(PIDFILE);
		execl(HA_LIBHBDIR "/heartbeat", "heartbeat", (const char *)NULL);
	}
	cl_log(LOG_ERR, "Could not exec " HA_LIBHBDIR "/heartbeat");
	cl_log(LOG_ERR, "Shutting down...");
	hb_emergency_shutdown();
}

/* See if any nodes or links have timed out */
static void
check_for_timeouts(void)
{
	longclock_t		now = time_longclock();
	struct node_info *	hip;
	longclock_t		dead_ticks;
	longclock_t		TooOld = msto_longclock(0);
	int			j;


	for (j=0; j < config->nodecount; ++j) {
		hip= &config->nodes[j];

		if (heartbeat_comm_state != COMM_LINKSUP) {
			/*
			 * Compute alternative dead_ticks value for very first
			 * dead interval.
			 *
			 * We do this because for some unknown reason
			 * sometimes the network is slow to start working.
			 * Experience indicates that 30 seconds is generally
			 * enough.  It would be nice to have a better way to
			 * detect that the network isn't really working, but
			 * I don't know any easy way.
			 * Patches are being accepted ;-)
			 */
			dead_ticks
			=       msto_longclock(config->initial_deadtime_ms);
		}else{
			dead_ticks = hip->dead_ticks;
		}

               if (cmp_longclock(now, dead_ticks) <= 0) {
                       TooOld  = zero_longclock;
               }else{
                       TooOld = sub_longclock(now, dead_ticks);
               }

		/* If it's recently updated, or already dead, ignore it */
		if (cmp_longclock(hip->local_lastupdate, TooOld) >= 0
		||	strcmp(hip->status, DEADSTATUS) == 0 ) {
			continue;
		}
		mark_node_dead(hip);
	}

	/* Check all links status of all nodes */

	for (j=0; j < config->nodecount; ++j) {
		struct link *	lnk;
		int		i;
		hip = &config->nodes[j];

		if (hip == curnode) {
			continue;
		}

		for (i=0; (lnk = &hip->links[i], lnk->name); i++) {
			if (lnk->lastupdate > now) {
					lnk->lastupdate = 0L;
			}
			if (cmp_longclock(lnk->lastupdate, TooOld) >= 0
			||  strcmp(lnk->status, DEADSTATUS) == 0 ) {
				continue;
			}
			change_link_status(hip, lnk, DEADSTATUS);
		}
	}
}




/*
 * Pick a machine, and ask it what the current ha.cf configuration is.
 * This is needed because of autojoin and also because of addnode/delnode
 * commands
 *
 * We allow dynamic node configuration even if autojoin is disabled.  In that
 * case you need to use the addnode and delnode commands to update the
 * configuration.
 */
static gboolean
send_reqnodes_msg(gpointer data){
	struct ha_msg*	msg;
	const char*	destnode = NULL;
	unsigned long	i;
	unsigned long	startindex = POINTER_TO_ULONG(data);
	guint		id;
	
	
	if (get_reqnodes_reply){
		return FALSE;
	}
	
	if (startindex >= config->nodecount){
		startindex = 0;
	}
	

	for (i = startindex; i< config->nodecount; i++){
		if (STRNCMP_CONST(config->nodes[i].status, DEADSTATUS) != 0
		    && (&config->nodes[i]) != curnode
		    && config->nodes[i].nodetype == NORMALNODE_I){
			destnode = config->nodes[i].nodename;
			break;
		}
		
	}
	
	if (destnode == NULL){
		get_reqnodes_reply = TRUE;
		comm_now_up();
		return FALSE;
	}
	

	msg = ha_msg_new(0);
	if (msg == NULL){
		cl_log(LOG_ERR, "%s: creating msg failed",
		       __FUNCTION__);		
		return FALSE;
	}
	
	if (ANYDEBUG){
		cl_log(LOG_DEBUG, "sending reqnodes msg to node %s", destnode);
	}
	
	if (ha_msg_add(msg, F_TYPE, T_REQNODES) != HA_OK
	    || ha_msg_add(msg, F_TO, destnode)!= HA_OK){
		cl_log(LOG_ERR, "%s: Adding filed failed",
		       __FUNCTION__);
		ha_msg_del(msg);
		return FALSE;		
	}
	
	send_cluster_msg(msg);
	
	id = Gmain_timeout_add(1000, send_reqnodes_msg, (gpointer)i);
	G_main_setall_id(id, "send_reqnodes_msg", config->heartbeat_ms, 100);
	return FALSE;
}

static void
check_comm_isup(void)
{
	struct node_info *	hip;
	int	j;
	int	heardfromcount = 0;


	if (heartbeat_comm_state == COMM_LINKSUP) {
		return;
	}
	
	if (config->rtjoinconfig != HB_JOIN_NONE 
	    && !init_deadtime_passed){
		return;
	}
	
	for (j=0; j < config->nodecount; ++j) {
		hip= &config->nodes[j];

		if (hip->anypacketsyet || strcmp(hip->status, DEADSTATUS) ==0){
			++heardfromcount;
		}
	}

	if (heardfromcount >= config->nodecount) {
		heartbeat_comm_state = COMM_LINKSUP;
		if (enable_flow_control){
			send_reqnodes_msg(0);
		}else{
		/*we have a mixed version of heartbeats
		 *Disable request/reply node list feature and mark comm up now
		 */  
			comm_now_up();
		}
	}
}

/* Set our local status to the given value, and send it out */
static int
set_local_status(const char * newstatus)
{
	if (strcmp(newstatus, curnode->status) != 0
	&&	strlen(newstatus) > 1 && strlen(newstatus) < STATUSLENG) {

		/*
		 * We can't do this because of conflicts between the two
		 * paths the updates otherwise arrive through...
		 * (Is this still true? ? ?)
		 */

		strncpy(curnode->status, newstatus, sizeof(curnode->status));
		send_local_status();
		cl_log(LOG_INFO, "Local status now set to: '%s'", newstatus);
		return HA_OK;
	}

	cl_log(LOG_INFO, "Unable to set local status to: %s", newstatus);
	return HA_FAIL;
}

/*
 * send_cluster_msg: sends out a message to the cluster
 * First we add some necessary fields to the message, then
 * we "send it out" via process_outbound_packet.
 *
 * send_cluster_msg disposes of the message
 *
 */
int
send_cluster_msg(struct ha_msg* msg)
{
	const char *	type;
	int		rc = HA_OK;
	pid_t		ourpid = getpid();

	send_cluster_msg_level ++;
	
	if (msg == NULL || (type = ha_msg_value(msg, F_TYPE)) == NULL) {
		cl_perror("Invalid message in send_cluster_msg");
		if (msg != NULL) {
			ha_msg_del(msg);
		}
		rc = HA_FAIL;
		goto out;
	}

	/*
	 * Only the parent process can send messages directly to the cluster.
	 *
	 * Everyone else needs to write to the FIFO instead.
	 * Sometimes we get called from the parent process, and sometimes
	 * from child processes.
	 */

	if (ourpid == processes[0]) {
		/* Parent process... Write message directly */

		if ((msg = add_control_msg_fields(msg)) != NULL) {
			rc = process_outbound_packet(&msghist, msg);
		}
	}else{
		/* We're a child process - copy it to the FIFO */
		int	ffd = -1;
		char *	smsg = NULL;
		int	needprivs = !cl_have_full_privs();
		size_t	len;
		ssize_t	writerc = -2;

		if (needprivs) {
			return_to_orig_privs();
		}
		if (DEBUGDETAILS) {
			cl_log(LOG_INFO, "Writing type [%s] message to FIFO"
			,	type);
		}

		/*
		 * Convert the message to a string, and write it to the FIFO
		 * It will then get written to the cluster properly.
		 */

		if ((smsg = msg2wirefmt_noac(msg, &len)) == NULL) {
			cl_log(LOG_ERR
			,	"send_cluster_msg: cannot convert"
			" message to wire format (pid %d)", (int)getpid());
			rc = HA_FAIL;
		}else if ((ffd = open(FIFONAME,O_WRONLY|O_APPEND)) < 0) {
			cl_perror("send_cluster_msg: cannot open " FIFONAME);
			rc = HA_FAIL;
		}else if ((writerc=write(ffd, smsg, len-1))
		!=		(ssize_t)(len -1)){
			cl_perror("send_cluster_msg: cannot write to "
			FIFONAME " [rc = %d]", (int)writerc);
			cl_log_message(LOG_ERR, msg);
			rc = HA_FAIL;
		}
		if (smsg) {
			if (ANYDEBUG) { /* FIXME - ANYDEBUG! */
				cl_log(LOG_INFO
				,	"FIFO message [type %s] written rc=%ld"
				, type, (long) writerc);
			}
			cl_free(smsg);
		}
		if (ffd > 0) {
			if (close(ffd) < 0) {
				cl_perror("%s close failure", FIFONAME);
			}
		}
		/* Dispose of the original message */
		ha_msg_del(msg);
		if (needprivs) {
			return_to_dropped_privs();
		}
	}
	
 out:
	send_cluster_msg_level --;
	return rc;
}




/* Send our local status out to the cluster */
static int
send_local_status()
{
	struct ha_msg *	m;
	int		rc;
	char		deadtime[64];
	long		cur_deadtime;

	if (DEBUGDETAILS){
		cl_log(LOG_DEBUG, "PID %d: Sending local status"
		" curnode = %lx status: %s"
		,	(int) getpid(), (unsigned long)curnode
		,	curnode->status);
	}
	if ((m=ha_msg_new(0)) == NULL) {
		cl_log(LOG_ERR, "Cannot send local status.");
		return HA_FAIL;
	}
	cur_deadtime = longclockto_ms(curnode->dead_ticks);
	snprintf(deadtime, sizeof(deadtime), "%lx", cur_deadtime);
	
	if (ha_msg_add(m, F_TYPE, T_STATUS) != HA_OK
	||	ha_msg_add(m, F_STATUS, curnode->status) != HA_OK
	||	ha_msg_add(m, F_DT, deadtime) != HA_OK) {
		cl_log(LOG_ERR, "send_local_status: "
		       "Cannot create local status msg");
		rc = HA_FAIL;
		ha_msg_del(m);
	}else{
		if (enable_flow_control 
		    && ha_msg_add_int(m, F_PROTOCOL, PROTOCOL_VERSION) != HA_OK){
			cl_log(LOG_ERR, "send_local_status: "
			       "Adding protocol number failed");
		}
		rc = send_cluster_msg(m);
	}

	return rc;
}

gboolean
hb_send_local_status(gpointer p)
{
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG, "hb_send_local_status() {");
	}
	send_local_status();
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG, "}/*hb_send_local_status*/;");
	}
	return TRUE;
}

static gboolean
set_init_deadtime_passed_flag(gpointer p)
{
	init_deadtime_passed =TRUE;
	return FALSE;
}
static gboolean
hb_update_cpu_limit(gpointer p)
{
	cl_cpu_limit_update();
	return TRUE;
}

gboolean
hb_dump_all_proc_stats(gpointer p)
{
	int	j;

	cl_log(LOG_INFO, "Daily informational memory statistics");

	hb_add_deadtime(2000);

	for (j=0; j < procinfo->nprocs; ++j) {
		hb_dump_proc_stats(procinfo->info+j);
	}
	
	hb_pop_deadtime(NULL);
	
	cl_log(LOG_INFO, "These are nothing to worry about.");
	return TRUE;
}

static gboolean
EmergencyShutdown(gpointer p)
{
	hb_emergency_shutdown();
	return TRUE;	/* Shouldn't get called twice, but... */
}

/* Mark the given link dead */
static void
change_link_status(struct node_info *hip, struct link *lnk
,	const char * newstat)
{
	struct ha_msg *	lmsg;

	if ((lmsg = ha_msg_new(8)) == NULL) {
		cl_log(LOG_ERR, "no memory to mark link dead");
		return;
	}

	strncpy(lnk->status, newstat, sizeof(lnk->status));
	cl_log(LOG_INFO, "Link %s:%s %s.", hip->nodename
	,	lnk->name, lnk->status);

	if (	ha_msg_add(lmsg, F_TYPE, T_IFSTATUS) != HA_OK
	||	ha_msg_add(lmsg, F_NODE, hip->nodename) != HA_OK
	||	ha_msg_add(lmsg, F_IFNAME, lnk->name) != HA_OK
	||	ha_msg_add(lmsg, F_STATUS, lnk->status) != HA_OK) {
		cl_log(LOG_ERR, "no memory to change link status");
		ha_msg_del(lmsg);
		return;
	}
	heartbeat_monitor(lmsg, KEEPIT, "<internal>");
	QueueRemoteRscReq(PerformQueuedNotifyWorld, lmsg);
	ha_msg_del(lmsg); lmsg = NULL;
}

/* Mark the given node dead */
static void
mark_node_dead(struct node_info *hip)
{
	cl_log(LOG_WARNING, "node %s: is dead", hip->nodename);

	if (hip == curnode) {
		/* Uh, oh... we're dead! */
		cl_log(LOG_ERR, "No local heartbeat. Forcing restart.");
		cl_log(LOG_INFO, "See URL: %s"
		,	HAURL("FAQ#no_local_heartbeat"));

		if (!shutdown_in_progress) {
			cause_shutdown_restart();
		}
		return;
	}

	if (hip->nodetype == NORMALNODE_I
	&&	STRNCMP_CONST(hip->status, DEADSTATUS) != 0
	&&	STRNCMP_CONST(hip->status, INITSTATUS) != 0) {
		--live_node_count;
	}
	strncpy(hip->status, DEADSTATUS, sizeof(hip->status));
	

	/* THIS IS RESOURCE WORK!  FIXME */
	hb_rsc_recover_dead_resources(hip);
	
	hip->rmt_lastupdate = 0L;
	hip->anypacketsyet  = 0;
	hip->track.nmissing = 0;
	hip->track.last_seq = NOSEQUENCE;
	hip->track.ackseq = 0;	

}


static gboolean
CauseShutdownRestart(gpointer p)
{
	cause_shutdown_restart();
	return FALSE;
}

static void
cause_shutdown_restart()
{
	/* Give up our resources, and restart ourselves */
	/* This is cleaner than lots of other options. */
	/* And, it really should work every time... :-) */

	procinfo->restart_after_shutdown = 1;
	/* THIS IS RESOURCE WORK!  FIXME */
	procinfo->giveup_resources = 1;
	hb_giveup_resources();
	/* Do something more drastic in 60 minutes */
	Gmain_timeout_add(1000*60*60, EmergencyShutdown, NULL);
}


/*
 * Values of msgtype:
 *	KEEPIT
 *	DROPIT
 *	DUPLICATE
 */
void
heartbeat_monitor(struct ha_msg * msg, int msgtype, const char * iface)
{
	api_heartbeat_monitor(msg, msgtype, iface);
}


static void
printversion(void)
{	
	printf("%s\n", VERSION);
	return;
}
/*
 * Print our usage statement.
 */
static void
usage(void)
{
	const char *	optionargs = OPTARGS;
	const char *	thislet;

	fprintf(stderr, "\nUsage: %s [-", cmdname);
	for (thislet=optionargs; *thislet; ++thislet) {
		if (thislet[0] != ':' &&  thislet[1] != ':') {
			fputc(*thislet, stderr);
		}
	}
	fputc(']', stderr);
	for (thislet=optionargs; *thislet; ++thislet) {
		if (thislet[1] == ':') {
			const char *	desc = "unknown-flag-argument";

			/* THIS IS RESOURCE WORK!  FIXME */
			/* Put a switch statement here eventually... */
			switch(thislet[0]) {
			case 'C':	desc = "Current-resource-state";
					break;
			}

			fprintf(stderr, " [-%c %s]", *thislet, desc);
		}
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "\t-C only valid with -R\n");
	fprintf(stderr, "\t-r is mutually exclusive with -R\n");
	cleanexit(LSB_EXIT_EINVAL);
}


int
main(int argc, char * argv[], char **envp)
{
	int		flag;
	unsigned	j;
	struct rlimit	oflimits;
	int		argerrs = 0;
	char *		CurrentStatus=NULL;
	char *		tmp_cmdname;
	long		running_hb_pid =  cl_read_pidfile(PIDFILE);
	int		generic_error = LSB_EXIT_GENERIC;

	cl_malloc_forced_for_glib();
	num_hb_media_types = 0;
	/* A precautionary measure */
	getrlimit(RLIMIT_NOFILE, &oflimits);
	for (j=FD_STDERR+1; j < oflimits.rlim_cur; ++j) {
		close(j);
	}

	
	/* Redirect messages from glib functions to our handler */
	g_log_set_handler(NULL
	,	G_LOG_LEVEL_ERROR	| G_LOG_LEVEL_CRITICAL
	|	G_LOG_LEVEL_WARNING	| G_LOG_LEVEL_MESSAGE
	|	G_LOG_LEVEL_INFO	| G_LOG_LEVEL_DEBUG
	|	G_LOG_FLAG_RECURSION	| G_LOG_FLAG_FATAL
	,	cl_glib_msg_handler, NULL);

	cl_log_enable_stderr(TRUE);
	/* Weird enum (bitfield) */
	g_log_set_always_fatal((GLogLevelFlags)0); /*value out of range*/  

	if ((tmp_cmdname = cl_strdup(argv[0])) == NULL) {
		cl_perror("Out of memory in main.");
		exit(1);
	}
	if ((cmdname = strrchr(tmp_cmdname, '/')) != NULL) {
		++cmdname;
	}else{
		cmdname = tmp_cmdname;
	}
	cl_log_set_entity(cmdname);
	if (module_init() != HA_OK) {
		cl_log(LOG_ERR, "Heartbeat not started: module init error.");
		cleanexit(generic_error);
	}
	init_procinfo();
	cl_set_oldmsgauthfunc(isauthentic);
	cl_set_authentication_computation_method(hb_compute_authentication);

	Argc = argc;

	while ((flag = getopt(argc, argv, OPTARGS)) != EOF) {

		switch(flag) {

			case 'C':
				/* THIS IS RESOURCE WORK!  FIXME */
				CurrentStatus = optarg;
				procinfo->i_hold_resources
				=	encode_resources(CurrentStatus);
				if (ANYDEBUG) {
					cl_log(LOG_DEBUG
					,	"Initializing resource state to %s"
					,	decode_resources(procinfo->i_hold_resources));
				}
				break;
			case 'd':
				++debug_level;
				break;
			case 'D':
				++PrintDefaults;
				break;
			case 'k':
				++killrunninghb;
				break;
			case 'M':
				DoManageResources=0;
				break;
			case 'r':
				++RestartRequested;
				break;
			case 'R':
				++WeAreRestarting;
				cl_log_enable_stderr(FALSE);
				break;
			case 's':
				++rpt_hb_status;
				generic_error = LSB_STATUS_UNKNOWN;
				break;
			case 'l':
				cl_disable_realtime();
				break;

			case 'v':
				verbose=TRUE;
				break;
			case 'V':
				printversion();
				cleanexit(LSB_EXIT_OK);
			case 'W':
				++WikiOutput;
				break;
				
			default:
				++argerrs;
				break;
		}
	}

	if (optind > argc) {
		++argerrs;
	}
	if (argerrs || (CurrentStatus && !WeAreRestarting)) {
		usage();
	}
	if (PrintDefaults) {
		dump_default_config(WikiOutput);
		cleanexit(LSB_EXIT_OK);
	}

	get_localnodeinfo();
	SetParameterValue(KEY_HBVERSION, VERSION);

	/* Default message handling... */
	hb_register_msg_callback(T_REXMIT,	HBDoMsg_T_REXMIT);
	hb_register_msg_callback(T_STATUS,	HBDoMsg_T_STATUS);
	hb_register_msg_callback(T_NS_STATUS,	HBDoMsg_T_STATUS);
	hb_register_msg_callback(T_QCSTATUS,	HBDoMsg_T_QCSTATUS);
	hb_register_msg_callback(T_ACKMSG,	HBDoMsg_T_ACKMSG);
	hb_register_msg_callback(T_ADDNODE,	HBDoMsg_T_ADDNODE);
	hb_register_msg_callback(T_SETWEIGHT,	HBDoMsg_T_SETWEIGHT);
	hb_register_msg_callback(T_SETSITE,	HBDoMsg_T_SETSITE);
	hb_register_msg_callback(T_DELNODE,	HBDoMsg_T_DELNODE);
	hb_register_msg_callback(T_REQNODES,    HBDoMsg_T_REQNODES);
	hb_register_msg_callback(T_REPNODES, 	HBDoMsg_T_REPNODES);

	if (init_set_proc_title(argc, argv, envp) < 0) {
		cl_log(LOG_ERR, "Allocation of proc title failed.");
		cleanexit(generic_error);
	}
	set_proc_title("%s", cmdname);

	hbmedia_types = cl_malloc(sizeof(struct hbmedia_types **));

	if (hbmedia_types == NULL) {
		cl_log(LOG_ERR, "Allocation of hbmedia_types failed.");
		cleanexit(generic_error);
	}



	if (debug_level > 0) {
		static char cdebug[8];
		snprintf(cdebug, sizeof(debug_level), "%d", debug_level);
		setenv(HADEBUGVAL, cdebug, TRUE);
	}

	/*
	 *	We've been asked to shut down the currently running heartbeat
	 *	process
	 */
	if (killrunninghb) {
		int	err;

		if (running_hb_pid < 0) {
			fprintf(stderr
			,	"INFO: Heartbeat already stopped.\n");
			cleanexit(LSB_EXIT_OK);
		}

		if (CL_KILL((pid_t)running_hb_pid, SIGTERM) >= 0) {
			/* Wait for the running heartbeat to die */
			alarm(0);
			do {
				sleep(1);
				continue;
			}while (CL_KILL((pid_t)running_hb_pid, 0) >= 0);
			cleanexit(LSB_EXIT_OK);
		}
		err = errno;
		fprintf(stderr, "ERROR: Could not kill pid %ld",
			running_hb_pid);
		perror(" ");
		cleanexit((err == EPERM || err == EACCES)
		?	LSB_EXIT_EPERM
		:	LSB_EXIT_GENERIC);
	}

	/*
	 *	Report status of heartbeat processes, etc.
	 *	We report in both Red Hat and SuSE formats...
	 */
	if (rpt_hb_status) {

		if (running_hb_pid < 0) {
			printf("%s is stopped. No process\n", cmdname);
			cleanexit(-running_hb_pid);
		}else{
			struct utsname u;

			if (uname(&u) < 0) {
				cl_perror("uname(2) call failed");
				cleanexit(LSB_EXIT_EPERM);
			}
			g_strdown(u.nodename);
			printf("%s OK [pid %ld et al] is running on %s [%s]...\n"
			,	cmdname, running_hb_pid, u.nodename, localnodename);
			cleanexit(LSB_STATUS_OK);
		}
		/*NOTREACHED*/
	}

	/*init table for nodename/uuid lookup*/
	inittable();


	/*
	 *	We think we just performed an "exec" of ourselves to restart.
	 */
	if (WeAreRestarting) {

		if (init_config(CONFIG_NAME) != HA_OK
			/* THIS IS RESOURCE WORK!  FIXME */
		||	! CHECK_HA_RESOURCES()){
			int err = errno;
			cl_log(LOG_INFO
			,	"Config errors: Heartbeat"
			" NOT restarted");
			cleanexit((err == EPERM || err == EACCES)
			?	LSB_EXIT_EPERM
			:	LSB_EXIT_GENERIC);
		}
		if (running_hb_pid < 0) {
			fprintf(stderr, "ERROR: %s is not running.\n"
			,	cmdname);
			cleanexit(LSB_EXIT_NOTCONFIGED);
		}
		if (running_hb_pid != getpid()) {
			fprintf(stderr
			,	"ERROR: Heartbeat already running"
			" [pid %ld].\n"
			,	running_hb_pid);
			cleanexit(LSB_EXIT_GENERIC);
		}

	/* LOTS OF RESOURCE WORK HERE!  FIXME */
		/*
		 * Nice_failback complicates things a bit here...
		 * We need to allow for the possibility that the user might
		 * have changed nice_failback options in the config file
		 */
		if (CurrentStatus && ANYDEBUG) {
			cl_log(LOG_INFO, "restart: i_hold_resources = %s"
			,	decode_resources(procinfo->i_hold_resources));
		}

		if (nice_failback) {
			/* nice_failback is currently ON */
			if (CurrentStatus == NULL) {
				/* From !nice_failback to nice_failback */
				procinfo->i_hold_resources = HB_LOCAL_RSC;
				hb_send_resources_held(TRUE, NULL);
				cl_log(LOG_INFO
				,	"restart: assuming HB_LOCAL_RSC");
			}else{
				/*
				 * From nice_failback to nice_failback.
				 * Cool. Nothing special to do.
				 */
			}
		}else{
			/* nice_failback is currently OFF */

			if (CurrentStatus == NULL) {
				/*
				 * From !nice_failback to !nice_failback.
				 * Cool. Nothing special to do.
				 */
			}else{
				/* From nice_failback to not nice_failback */
				if ((procinfo->i_hold_resources
				&		HB_LOCAL_RSC)) {
					/* We expect to have those */
					cl_log(LOG_INFO, "restart: acquiring"
					" local resources.");
					req_our_resources(0);
				}else{
					cl_log(LOG_INFO, "restart: "
					" local resources already acquired.");
				}
			}
		}
	}



	/*
	 *	We've been asked to restart currently running heartbeat
	 *	process (or at least get it to reread it's configuration
	 *	files)
	 */

	if (RestartRequested) {
		if (running_hb_pid < 0) {
			goto StartHeartbeat;
		}

		errno = 0;
		if (init_config(CONFIG_NAME)
			/* THIS IS RESOURCE WORK!  FIXME */
		&&	CHECK_HA_RESOURCES()){
			cl_log(LOG_INFO
			,	"Signalling heartbeat pid %ld to reread"
			" config files", running_hb_pid);

			if (CL_KILL(running_hb_pid, SIGHUP) >= 0) {
				cleanexit(0);
			}
			cl_perror("Unable to send SIGHUP to pid %ld"
			,	running_hb_pid);
		}else{
			int err = errno;
			cl_log(LOG_INFO
			,	"Config errors: Heartbeat pid %ld"
			" NOT restarted"
			,	running_hb_pid);
			cleanexit((err == EPERM || err == EACCES)
			?	LSB_EXIT_EPERM
			:	LSB_EXIT_GENERIC);
		}
		cleanexit(LSB_EXIT_GENERIC);
	}

StartHeartbeat:

	
	
        /* We have already initialized configs in case WeAreRestarting. */
        if (WeAreRestarting
        ||      (init_config(CONFIG_NAME)
			/* THIS IS RESOURCE WORK!  FIXME */
                &&      CHECK_HA_RESOURCES())) {
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			,	"HA configuration OK.  Heartbeat starting.");
		}
		if (verbose) {
			dump_config();
		}
		make_daemon();	/* Only child processes returns. */
		setenv(LOGFENV, config->logfile, 1);
		setenv(DEBUGFENV, config->dbgfile, 1);
		if (config->log_facility >= 0) {
			char	facility[40];
			snprintf(facility, sizeof(facility)
			,	"%s", config->facilityname);
			setenv(LOGFACILITY, facility, 1);
		}
		ParseTestOpts();
		hb_versioninfo();
		if (initialize_heartbeat() != HA_OK) {
			cleanexit((errno == EPERM || errno == EACCES)
			?	LSB_EXIT_EPERM
			:	LSB_EXIT_GENERIC);
		}
	}else{
		int err = errno;
		cl_log(LOG_ERR
		,	"Configuration error, heartbeat not started.");

		cleanexit((err == EPERM || err == EACCES)
		?	LSB_EXIT_EPERM
		:	LSB_EXIT_NOTCONFIGED);
	}

	/*NOTREACHED*/
	return generic_error;
}

void
cleanexit(rc)
	int	rc;
{
	hb_close_watchdog();

	if (localdie) {
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "Calling localdie() function");
		}
		(*localdie)();
	}
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "Exiting from pid %d [rc=%d]"
		,	(int) getpid(), rc);
	}
	if (config && config->log_facility >= 0) {
		closelog();
	}
	exit(rc);
}


void
hb_emergency_shutdown(void)
{
	cl_make_normaltime();
	return_to_orig_privs();
	CL_IGNORE_SIG(SIGTERM);
	cl_log(LOG_CRIT, "Emergency Shutdown: "
			"Attempting to kill everything ourselves");
	CL_KILL(-getpgrp(), SIGTERM);
	hb_kill_rsc_mgmt_children(SIGKILL);
	hb_kill_managed_children(SIGKILL);
	hb_kill_core_children(SIGKILL);
	sleep(2);
	CL_KILL(-getpgrp(), SIGKILL);
	/*NOTREACHED*/
	cleanexit(100);
}

static void
hb_check_mcp_alive(void)
{
	pid_t		ourpid = getpid();
	int		j;

	if (CL_PID_EXISTS(procinfo->info[0].pid)) {
		return;
	}
	return_to_orig_privs();
	cl_log(LOG_CRIT, "Emergency Shutdown: Master Control process died.");

	for (j=0; j < procinfo->nprocs; ++j) {
		if (procinfo->info[j].pid == ourpid) {
			continue;
		}
		cl_log(LOG_CRIT, "Killing pid %d with SIGTERM"
		,	(int)procinfo->info[j].pid);
		CL_KILL(procinfo->info[j].pid, SIGTERM);
	}
	/* We saved the best for last :-) */
	cl_log(LOG_CRIT, "Emergency Shutdown(MCP dead): Killing ourselves.");
	CL_KILL(ourpid, SIGTERM);
}

extern pid_t getsid(pid_t);

static void
make_daemon(void)
{
	long			pid;
	const char *		devnull = "/dev/null";

	/* See if heartbeat is already running... */

	if ((pid=cl_read_pidfile(PIDFILE)) > 0 && pid != getpid()) {
		cl_log(LOG_INFO, "%s: already running [pid %ld]."
		,	cmdname, pid);
		exit(LSB_EXIT_OK);
	}

	/* Guess not. Go ahead and start things up */

	if (!WeAreRestarting) {
#if 1
		pid = fork();
#else
		pid = 0;
#endif
		if (pid < 0) {
			cl_log(LOG_ERR, "%s: could not start daemon\n"
			,	cmdname);
			cl_perror("fork");
			exit(LSB_EXIT_GENERIC);
		}else if (pid > 0) {
			exit(LSB_EXIT_OK);
		}
	}

	
	if ( cl_lock_pidfile(PIDFILE) < 0){
		cl_log(LOG_ERR,"%s: could not create pidfile [%s]\n",
			cmdname, PIDFILE);
		exit(LSB_EXIT_EPERM);
	}



	cl_log_enable_stderr(FALSE);

	setenv(HADIRENV, HA_HBCONF_DIR, TRUE);
	setenv(DATEFMT, HA_DATEFMT, TRUE);
	setenv(HAFUNCENV, HA_FUNCS, TRUE);
	setenv("OCF_ROOT", OCF_ROOT_DIR, TRUE);
	umask(022);
	close(FD_STDIN);
	(void)open(devnull, O_RDONLY);		/* Stdin:  fd 0 */
	close(FD_STDOUT);
	(void)open(devnull, O_WRONLY);		/* Stdout: fd 1 */
	close(FD_STDERR);
	(void)open(devnull, O_WRONLY);		/* Stderr: fd 2 */
	cl_cdtocoredir();
	/* We need to at least ignore SIGINTs early on */
	hb_signal_set_common(NULL);
	if (getsid(0) != pid) {
		if (setsid() < 0) {
			cl_perror("setsid() failure.");
		}
	}
}

#define	APPHBINSTANCE	"master_control_process"

static void
hb_init_register_with_apphbd(void)
{
	static int	failcount = 0;
	if (!UseApphbd || RegisteredWithApphbd) {
		return;
	}
		
	if (apphb_register(hbname, APPHBINSTANCE) != 0) {
		/* Log attempts once an hour or so... */
		if ((failcount % 60) ==  0) {
			cl_perror("Unable to register with apphbd.");
			cl_log(LOG_INFO, "Continuing to try and register.");
		}
		++failcount;
		return;
	}

	RegisteredWithApphbd = TRUE;
	cl_log(LOG_INFO, "Registered with apphbd as %s/%s."
	,	hbname, APPHBINSTANCE); 

	if (apphb_setinterval(config->deadtime_ms) < 0
	||	apphb_setwarn(config->warntime_ms) < 0) {
		cl_perror("Unable to setup with apphbd.");
		apphb_unregister();
		RegisteredWithApphbd = FALSE;
		++failcount;
	}else{
		failcount = 0;
	}
}

static gboolean
hb_reregister_with_apphbd(gpointer dummy)
{
	if (UseApphbd) {
		hb_init_register_with_apphbd();
	}
	return UseApphbd;
}

static void
hb_unregister_from_apphb(void)
{
	if (RegisteredWithApphbd == TRUE ) {
		UseApphbd = FALSE;
		apphb_unregister();
	}
}

static void
hb_apphb_hb(void)
{
	if (UseApphbd) {
		if (RegisteredWithApphbd) {
			if (apphb_hb() < 0) {
				/* apphb_hb() will fail if apphbd exits */
				cl_perror("apphb_hb() failed.");
				apphb_unregister();
				RegisteredWithApphbd = FALSE;
			}
		}	
		/*
		 * Our timeout job (hb_reregister_with_apphbd) will
		 * reregister us if we become unregistered somehow...
		 */
	}
}


static void
hb_init_watchdog_interval(void)
{
	if (watchdogfd < 0) {
		return;
	}

	if (watchdog_timeout_ms == 0L) {
		watchdog_timeout_ms = config->deadtime_ms + 10;
	}
#ifdef WDIOC_SETTIMEOUT
	{
		int timeout_secs;

		timeout_secs = (watchdog_timeout_ms+999)/1000;

		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "Set watchdog timer to %d seconds."
			,	timeout_secs);
		}

		if (ioctl(watchdogfd, WDIOC_SETTIMEOUT, &timeout_secs) < 0) {
			cl_perror( "WDIOC_SETTIMEOUT"
			": Failed to set watchdog timer to %d seconds."
			,	timeout_secs);
		}
	}
#endif
}
void
hb_init_watchdog(void)
{
	if (watchdogfd < 0 && watchdogdev != NULL) {
		watchdogfd = open(watchdogdev, O_WRONLY);
		if (watchdogfd >= 0) {
			if (fcntl(watchdogfd, F_SETFD, FD_CLOEXEC)) {
				cl_perror("Error setting the "
				"close-on-exec flag for watchdog");
			}
			cl_log(LOG_NOTICE, "Using watchdog device: %s"
			,	watchdogdev);
			hb_init_watchdog_interval();
			hb_tickle_watchdog();
		}else{
			cl_log(LOG_ERR, "Cannot open watchdog device: %s"
			,	watchdogdev);
		}
	}
	if ( UseApphbd == TRUE ) {
		hb_init_register_with_apphbd();
	}
}

void
hb_tickle_watchdog(void)
{
	if (watchdogfd >= 0) {
		if (write(watchdogfd, "", 1) != 1) {
			cl_perror("Watchdog write failure: closing %s!"
			,	watchdogdev);
			hb_close_watchdog();
			watchdogfd=-1;
		}
	}
	hb_apphb_hb();
}

void
hb_close_watchdog(void)
{
	if (watchdogfd >= 0) {
		if (write(watchdogfd, "V", 1) != 1) {
			cl_perror(
			"Watchdog write magic character failure: closing %s!"
			,	watchdogdev);
		}else{
			if (ANYDEBUG) {
				cl_log(LOG_INFO, "Successful watchdog 'V' write");
			}
		}
		if (close(watchdogfd) < 0) {
			cl_perror("Watchdog close(2) failed.");
		}else{
			if (ANYDEBUG) {
				cl_log(LOG_INFO, "Successful watchdog close");
			}
		}
		watchdogfd=-1;
	}
	if (RegisteredWithApphbd) {
		hb_unregister_from_apphb();
	}
}

void
ha_assert(const char * assertion, int line, const char * file)
{
	cl_log(LOG_ERR, "Assertion \"%s\" failed on line %d in file \"%s\""
	,	assertion, line, file);
	cleanexit(1);
}

/*
 *	Check to see if we should copy this packet further into the ring
 */
int
should_ring_copy_msg(struct ha_msg *m)
{
	const char *	us = curnode->nodename;
	const char *	from;	/* Originating Node name */
	const char *	ttl;	/* Time to live */

	/* Get originator and time to live field values */
	if ((from = ha_msg_value(m, F_ORIG)) == NULL
	||	(ttl = ha_msg_value(m, F_TTL)) == NULL) {
			cl_log(LOG_ERR
			,	"bad packet in should_copy_ring_pkt");
			return 0;
	}
	/* Is this message from us? */
	if (strcmp(from, us) == 0 || ttl == NULL || atoi(ttl) <= 0) {
		/* Avoid infinite loops... Ignore this message */
		return 0;
	}

	/* Must be OK */
	return 1;
}

/*
 *	From here to the end is protocol code.  It implements our reliable
 *	multicast protocol.
 *
 *	This protocol is called from master_control_process().
 */

static void
client_status_msg_queue_cleanup(GList* list)
{
	struct ha_msg*	msg;
	GList*		list_runner;
	
	if (list == NULL){
		return;
	}
	
	while((list_runner =  g_list_first(list))!= NULL) {
		msg = (struct ha_msg*) list_runner->data;
		if (msg){
			ha_msg_del(msg);
		}
		list = g_list_delete_link(list, list_runner);
	}	
	
	g_list_free(list);
	
	return;
}


/*
 *	Right now, this function is a little too simple.  There is no
 *	provision for sequence number wraparounds.  But, it will take a very
 *	long time to wrap around (~ 100 years)
 *
 *	I suspect that there are better ways to do this, but this will
 *	do for now...
 */
#define	SEQGAP	500	/* A heuristic number */

/*
 *	Should we ignore this packet, or pay attention to it?
 */
static int
should_drop_message(struct node_info * thisnode, const struct ha_msg *msg,
		    const char *iface, int* is_missing_packet)
{
	struct seqtrack *	t = &thisnode->track;
	const char *		cseq = ha_msg_value(msg, F_SEQ);
	const char *		to = ha_msg_value(msg, F_TO);
	cl_uuid_t		touuid;
	const char *		from= ha_msg_value(msg, F_ORIG);
	cl_uuid_t		fromuuid;
	const char *		type = ha_msg_value(msg, F_TYPE);
	const char *		cgen = ha_msg_value(msg, F_HBGENERATION);
	seqno_t			seq;
	seqno_t			gen = 0;
	int			IsToUs;
	int			j;
	int			isrestart = 0;
	int			ishealedpartition = 0;
	int			is_status = 0;
	
	
	if ( cl_get_uuid(msg, F_ORIGUUID, &fromuuid) != HA_OK){
		cl_uuid_clear(&fromuuid);
	}
	
	if (from && !cl_uuid_is_null(&fromuuid)){
		/* We didn't know their uuid before, but now we do... */
		if (update_tables(from, &fromuuid)){
			G_main_set_trigger(write_hostcachefile);
		}
	}
	
	if (is_missing_packet == NULL){
		cl_log(LOG_ERR, "should_drop_message: "
		       "NULL input is_missing_packet");
		return DROPIT;
	}
	/* Some packet types shouldn't have sequence numbers */
	if (type != NULL
	&&	strncmp(type, NOSEQ_PREFIX, sizeof(NOSEQ_PREFIX)-1) ==	0) {
		/* Is this a sequence number rexmit NAK? */
		if (strcasecmp(type, T_NAKREXMIT) == 0) {
			const char *	cnseq = ha_msg_value(msg, F_FIRSTSEQ);
			seqno_t		nseq;

			if (cnseq  == NULL || sscanf(cnseq, "%lx", &nseq) != 1
			    ||	nseq <= 0) {
				cl_log(LOG_ERR
				, "should_drop_message: bad nak seq number");
				return DROPIT;
			}

			if (to == NULL){
				cl_log(LOG_WARNING,"should_drop_message: tonodename not found "
				       "heartbeat version not matching?");				       
			}
			if (to == NULL || strncmp(to, curnode->nodename, HOSTLENG ) == 0){				
				cl_log(LOG_ERR , "%s: node %s seq %ld",
				       "Irretrievably lost packet",
				       thisnode->nodename, nseq);
			}

			is_lost_packet(thisnode, nseq);
			return DROPIT;
			
		}else if (to == NULL || strncmp(to, curnode->nodename, HOSTLENG ) == 0){			
			return KEEPIT;
		}else{
			return DROPIT;
		}
		
	}
	if (strcasecmp(type, T_STATUS) == 0) {
		is_status = 1;
	}
	
	if (cseq  == NULL || sscanf(cseq, "%lx", &seq) != 1 ||	seq <= 0) {
		cl_log(LOG_ERR, "should_drop_message: bad sequence number");
		cl_log_message(LOG_ERR, msg);
		return DROPIT;
	}

	/* Extract the heartbeat generation number */
	if (cgen != NULL && sscanf(cgen, "%lx", &gen) <= 0) {
		cl_log(LOG_ERR, "should_drop_message: bad generation number");
		cl_log_message(LOG_ERR, msg);
		return DROPIT;
	}
	
	if ( cl_get_uuid(msg, F_TOUUID, &touuid) != HA_OK){
		cl_uuid_clear(&touuid);
	}
	
	
	if(!cl_uuid_is_null(&touuid)){
		IsToUs = (cl_uuid_compare(&touuid, &config->uuid) == 0);
	}else{
		IsToUs = (to == NULL) || (strcmp(to, curnode->nodename) == 0);
	}
	
	/*
	 * We need to do sequence number processing on every
	 * packet, even those that aren't sent to us.
	 */
	/* Does this looks like a replay attack... */
	if (gen < t->generation) {
		cl_log(LOG_ERR
		,	"should_drop_message: attempted replay attack"
		" [%s]? [gen = %ld, curgen = %ld]"
		,	thisnode->nodename, gen, t->generation);
		return DROPIT;

	}else if (is_status) {
		/* Look for apparent restarts/healed partitions */
		if (gen == t->generation && gen > 0) {
			/* Is this a message from a node that was dead? */
			if (strcmp(thisnode->status, DEADSTATUS) == 0) {
				/* Is this stale data? */
				if (seq <= thisnode->status_seqno) {
					return DROPIT;
				}

				/* They're now alive, but were dead. */
				/* No restart occured. UhOh. */

				cl_log(LOG_CRIT
				,	"Cluster node %s"
				" returning after partition."
				,	thisnode->nodename);
				cl_log(LOG_INFO
				,	"For information on cluster"
				" partitions, See URL: %s"
				,	HAURL("SplitBrain"));
				cl_log(LOG_WARNING
				,	"Deadtime value may be too small.");
				cl_log(LOG_INFO
				,	"See FAQ for information"
				" on tuning deadtime.");
				cl_log(LOG_INFO
				,	"URL: %s"
				,	HAURL("FAQ#heavy_load"));

				/* THIS IS RESOURCE WORK!  FIXME */
				/* IS THIS RIGHT??? FIXME ?? */
				if (DoManageResources) {
					guint id;
					send_local_status();
					(void)CauseShutdownRestart;
					id = Gmain_timeout_add(2000
					,	CauseShutdownRestart,NULL);
					G_main_setall_id(id, "shutdown restart", 1000, 50);
				}
				ishealedpartition=1;
			}
		}else if (gen > t->generation) {
			isrestart = 1;
			if (t->generation > 0) {
				cl_log(LOG_INFO, "Heartbeat restart on node %s"
				,	thisnode->nodename);
			}
			thisnode->rmt_lastupdate = 0L;
			thisnode->local_lastupdate = 0L;
			thisnode->status_seqno = 0L;
			/* THIS IS RESOURCE WORK!  FIXME */
			thisnode->has_resources = TRUE;
		}
		t->generation = gen;
	}

	/* Is this packet in sequence? */
	if (t->last_seq == NOSEQUENCE || seq == (t->last_seq+1)) {
		
		t->last_seq = seq;
		t->last_iface = iface;
		send_ack_if_necessary(msg);
		return (IsToUs ? KEEPIT : DROPIT);
	}else if (seq == t->last_seq) {
		/* Same as last-seen packet -- very common case */
		if (DEBUGPKT) {
			cl_log(LOG_DEBUG
			,	"should_drop_message: Duplicate packet(1)");
		}
		return DUPLICATE;
	}
	/*
	 * Not in sequence... Hmmm...
	 *
	 * Is it newer than the last packet we got?
	 */
	if (seq > t->last_seq) {
		seqno_t	k;
		seqno_t	nlost;
		nlost = ((seqno_t)(seq - (t->last_seq+1)));
		cl_log(LOG_WARNING, "%lu lost packet(s) for [%s] [%lu:%lu]"
		,	nlost, thisnode->nodename, t->last_seq, seq);

		if (nlost > SEQGAP) {
			/* Something bad happened.  Start over */
			/* This keeps the loop below from going a long time */
			t->nmissing = 0;
			t->last_seq = seq;
			t->last_iface = iface;
			t->first_missing_seq = 0;
			if ( t->client_status_msg_queue){
				GList* mq =  t->client_status_msg_queue;
				client_status_msg_queue_cleanup(mq);
				t->client_status_msg_queue = NULL;
			}
			
			cl_log(LOG_ERR, "lost a lot of packets!");
			return (IsToUs ? KEEPIT : DROPIT);
		}else{
			request_msg_rexmit(thisnode, t->last_seq+1L, seq-1L);
		}

		/* Try and Record each of the missing sequence numbers */
		if (t->first_missing_seq == 0
		    || 	t->first_missing_seq > t -> last_seq + 1 ){
			
			t->first_missing_seq = t -> last_seq +1;				
		} 

		for(k = t->last_seq+1; k < seq; ++k) {
			if (t->nmissing < MAXMISSING-1) {
				t->seqmissing[t->nmissing] = k;
				++t->nmissing;
			}else{
				int		minmatch = -1;
				seqno_t		minseq = INT_MAX;
				/*
				 * Replace the lowest numbered missing seqno
				 * with this one
				 */
				for (j=0; j < MAXMISSING; ++j) {
					if (t->seqmissing[j] == NOSEQUENCE) {
						minmatch = j;
						break;
					}
					if (minmatch < 0
					|| t->seqmissing[j] < minseq) {
						minmatch = j;
						minseq = t->seqmissing[j];
					}
				}
				t->seqmissing[minmatch] = k;
			}
		}
		t->last_seq = seq;
		t->last_iface = iface;
		return (IsToUs ? KEEPIT : DROPIT);
	}
	/*
	 * This packet appears to be older than the last one we got.
	 */

	/*
	 * Is it a (recorded) missing packet?
	 */
	if ( (*is_missing_packet = is_lost_packet(thisnode, seq))) {
		return (IsToUs ? KEEPIT : DROPIT);
	}

	if (ishealedpartition || isrestart) {
		const char *	sts;
		TIME_T	newts = 0L;

		send_ack_if_necessary(msg);
		
		if ((sts = ha_msg_value(msg, F_TIME)) == NULL
		||	sscanf(sts, TIME_X, &newts) != 1 || newts == 0L) {
			/* Toss it.  No valid timestamp */
			cl_log(LOG_ERR, "should_drop_message: bad timestamp");
			return DROPIT;
		}

		thisnode->rmt_lastupdate = newts;
		t->nmissing = 0;
		t->last_seq = seq;
		t->last_rexmit_req = zero_longclock;
		t->last_iface = iface;
		t->first_missing_seq = 0;
		if (t->client_status_msg_queue){			
			GList* mq = t->client_status_msg_queue;
			client_status_msg_queue_cleanup(mq);
			t->client_status_msg_queue = NULL;
		}
		return (IsToUs ? KEEPIT : DROPIT);
	}
	/* This is a DUP packet (or a really old one we lost track of) */
	if (DEBUGPKT) {
		cl_log(LOG_DEBUG, "should_drop_message: Duplicate packet");
		cl_log_message(LOG_DEBUG, msg);
	}
	return DROPIT;

}


/*
 * Control (inbound) packet processing...
 * This is part of the control_process() processing.
 *
 * This is where the reliable multicast protocol is implemented -
 * through the use of process_rexmit(), and add2_xmit_hist().
 * process_rexmit(), and add2_xmit_hist() use msghist to track sent
 * packets so we can retransmit them if they get lost.
 *
 * NOTE: It's our job to dispose of the packet we're given...
 */
static int
process_outbound_packet(struct msg_xmit_hist*	hist
,		struct ha_msg *	msg)
{

	char *		smsg;
	const char *	type;
	const char *	cseq;
	seqno_t		seqno = -1;
	const  char *	to;
	int		IsToUs;
	size_t		len;

	if (DEBUGPKTCONT) {
		cl_log(LOG_DEBUG, "got msg in process_outbound_packet");
	}
	if ((type = ha_msg_value(msg, F_TYPE)) == NULL) {
		cl_log(LOG_ERR, "process_outbound_packet: no type in msg.");
		ha_msg_del(msg); msg = NULL;
		return HA_FAIL;
	}
	if ((cseq = ha_msg_value(msg, F_SEQ)) != NULL) {
		if (sscanf(cseq, "%lx", &seqno) != 1
		||	seqno <= 0) {
			cl_log(LOG_ERR, "process_outbound_packet: "
			"bad sequence number");
			smsg = NULL;
			ha_msg_del(msg);
			return HA_FAIL;
		}
	}

	to = ha_msg_value(msg, F_TO);
	IsToUs = (to != NULL) && (strcmp(to, curnode->nodename) == 0);

	/* Convert the incoming message to a string */
	smsg = msg2wirefmt(msg, &len);

	/* If it didn't convert, throw original message away */
	if (smsg == NULL) {
		ha_msg_del(msg);
		return HA_FAIL;
	}
	/* Remember Messages with sequence numbers */
	if (cseq != NULL) {
		add2_xmit_hist (hist, msg, seqno);
	}
	/*
	if (DEBUGPKT){
		cl_msg_stats_add(time_longclock(), len);		
	}
	*/

	/* Direct message to "loopback" processing */
	process_clustermsg(msg, NULL);

	send_to_all_media(smsg, len);
	cl_free(smsg);

	/*  Throw away "msg" here if it's not saved above */
	if (cseq == NULL) {
		ha_msg_del(msg);
	}
	/* That's All Folks... */
	return HA_OK;
}


/*
 * Is this the sequence number of a lost packet?
 * If so, clean up after it.
 */
static int
is_lost_packet(struct node_info * thisnode, seqno_t seq)
{
	struct seqtrack *	t = &thisnode->track;
	int			j;
	int			ret = 0;
	
	for (j=0; j < t->nmissing; ++j) {
		/* Is this one of our missing packets? */
		if (seq == t->seqmissing[j]) {
			
			remove_msg_rexmit(thisnode, seq);

			/* Yes.  Delete it from the list */
			t->seqmissing[j] = NOSEQUENCE;
			/* Did we delete the last one on the list */
			if (j == (t->nmissing-1)) {
				t->nmissing --;
			}
			
			/* Swallow up found packets */
			while (t->nmissing > 0
			&&	t->seqmissing[t->nmissing-1] == NOSEQUENCE) {
				t->nmissing --;
			}
			if (t->nmissing == 0) {
				cl_log(LOG_INFO, "No pkts missing from %s!"
				,	thisnode->nodename);
				t->first_missing_seq = 0;
			}
			ret = 1;
			goto out;
		}
	}	
	
 out:
	if (!enable_flow_control){
		return ret;
	}
	
	if (ret && seq == t->first_missing_seq){
		/*determine the new first missing seq*/
		seqno_t old_missing_seq = t->first_missing_seq;
		seqno_t lastseq_to_ack;
		seqno_t x;
		seqno_t trigger = thisnode->track.ack_trigger;
		seqno_t ack_seq;
		t->first_missing_seq = 0;
		for (j=0; j < t->nmissing; ++j) {
			if (t->seqmissing[j] != NOSEQUENCE){
				if (t->first_missing_seq == 0 ||
				    t->seqmissing[j] < t->first_missing_seq){					
					t->first_missing_seq = t->seqmissing[j];
				}				
			}
		}		
		
		if (t->first_missing_seq == 0){
			lastseq_to_ack = t->last_seq;			
		}else {
			lastseq_to_ack = t->first_missing_seq - 1 ;
		}
		

		x = lastseq_to_ack % ACK_MSG_DIV;
		if (x >= trigger ){
			ack_seq = lastseq_to_ack/ACK_MSG_DIV*ACK_MSG_DIV + trigger;
		}else{
			ack_seq = (lastseq_to_ack/ACK_MSG_DIV -1)*ACK_MSG_DIV + trigger;
		}
		
		if (ack_seq >= old_missing_seq){
			send_ack_if_needed(thisnode, ack_seq);
		}
		
	}

	return ret;
	
}


extern int			max_rexmit_delay;
#define REXMIT_MS		max_rexmit_delay
#define ACCEPT_REXMIT_REQ_MS	(REXMIT_MS-10)


static void
dump_missing_pkts_info(void)
{
	int j;
	
	for (j = 0; j < config->nodecount; ++j) {
		struct node_info *	hip = &config->nodes[j];
		struct seqtrack *	t = &hip->track;
		int			seqidx;
		
		if (t->nmissing == 0){
			continue;
		}else{
			cl_log(LOG_DEBUG, "At max %d pkts missing from %s",
			       t->nmissing, hip->nodename);
		}
		for (seqidx = 0; seqidx < t->nmissing; ++seqidx) {			
			if (t->seqmissing[seqidx] != NOSEQUENCE) {
				cl_log(LOG_DEBUG, "%d: missing pkt: %ld", seqidx, t->seqmissing[seqidx]);
			}
		}
	}	
}

static void
check_rexmit_reqs(void)
{
	longclock_t	minrexmit = 0L;
	int		gottimeyet = FALSE;
	int		j;

	for (j=0; j < config->nodecount; ++j) {
		struct node_info *	hip = &config->nodes[j];
		struct seqtrack *	t = &hip->track;
		int			seqidx;

		if (t->nmissing <= 0 ) {
			continue;
		}
		/*
		 * We rarely reach this code, so avoid an extra system call
		 */
		if (!gottimeyet) {
			longclock_t	rexmitms = msto_longclock(REXMIT_MS);
			longclock_t	now = time_longclock();

			gottimeyet = TRUE;
			if (cmp_longclock(now, rexmitms) < 0) {
				minrexmit = zero_longclock;
			}else{
				minrexmit = sub_longclock(now, rexmitms);
			}
		}

		if (cmp_longclock(t->last_rexmit_req, minrexmit) > 0) {
			/* Too soon to ask for retransmission */
			continue;
		}

		
		if (t->nmissing > MAX_MISSING_PKTS){
			cl_log(LOG_ERR, "too many missing pkts(%d) from node %s",
			       t->nmissing, hip->nodename);
		}
		
		/* Time to ask for some packets again ... */
		for (seqidx = 0; seqidx < t->nmissing; ++seqidx) {
			if (t->seqmissing[seqidx] != NOSEQUENCE) {
				/*
				 * The code for asking for these by groups
				 * is complicated.  This code is not.
				 */
			  if (ANYDEBUG){
			    cl_log(LOG_INFO, "calling request_msg_rexmit()"
				   "from %s", __FUNCTION__);
			  }
				request_msg_rexmit(hip, t->seqmissing[seqidx]
				,	t->seqmissing[seqidx]);
			}
		}
	}
}

/* Initialize the transmit history */
static void
init_xmit_hist (struct msg_xmit_hist * hist)
{
	int	j;
	
	hist->lastmsg = MAXMSGHIST-1;
	hist->hiseq = hist->lowseq = 0;
	hist->ackseq = 0;
	hist->lowest_acknode = NULL;
	for (j=0; j < MAXMSGHIST; ++j) {
		hist->msgq[j] = NULL;
		hist->seqnos[j] = 0;
		hist->lastrexmit[j] = zero_longclock;
	}
}

#ifdef DO_AUDITXMITHIST
void
audit_xmit_hist(void)
{
	int	slot;

	for (slot = 0; slot < MAXMSGHIST; ++slot) {
		struct ha_msg* msg = msghist.msgq[slot];
		gboolean doabort = FALSE;

		if (msg == NULL) {
			continue;
		}
		if (!cl_is_allocated(msg)) {
			cl_log(LOG_CRIT
			,	"Unallocated message in audit_xmit_hist");
			doabort=TRUE;
		}
		if (msg->nfields <= 0) {
			cl_log(LOG_CRIT
			,	"Non-positive nfields in audit_xmit_hist");
			doabort=TRUE;
		}
		if (msg->nalloc <= 0) {
			cl_log(LOG_CRIT
			,	"Non-positive nalloc in audit_xmit_hist");
			doabort=TRUE;
		}
		if (msg->stringlen <= 0) {
			cl_log(LOG_CRIT
			,	"Non-positive stringlen in audit_xmit_hist");
			doabort=TRUE;
		}
		if (msg->nfields > msg->nalloc) {
			cl_log(LOG_CRIT
			,	"Improper nfields in audit_xmit_hist");
			doabort=TRUE;
		}
		if (msg->nfields > 100) {
			cl_log(LOG_CRIT
			,	"TOO Large nfields in audit_xmit_hist");
			doabort=TRUE;
		}
		if (get_stringlen(msg) <= msg->nfields*4) {
			cl_log(LOG_CRIT
			,	"Too small stringlen in audit_xmit_hist");
			doabort=TRUE;
		}
		if (!cl_is_allocated(msg->names)) {
			cl_log(LOG_CRIT
			,	"Unallocated msg->names in audit_xmit_hist");
			doabort=TRUE;
		}
		if (!cl_is_allocated(msg->nlens)) {
			cl_log(LOG_CRIT
			,	"Unallocated msg->nlens in audit_xmit_hist");
			doabort=TRUE;
		}
		if (!cl_is_allocated(msg->values)) {
			cl_log(LOG_CRIT
			,	"Unallocated msg->values in audit_xmit_hist");
			doabort=TRUE;
		}
		if (!cl_is_allocated(msg->vlens)) {
			cl_log(LOG_CRIT
			,	"Unallocated msg->vallens in audit_xmit_hist");
			doabort=TRUE;
		}
		if (doabort) {
			cl_log(LOG_CRIT
			,	"Message slot is %d", slot);
			abort();
		}
	}
}
#endif


gboolean
heartbeat_on_congestion(void)
{
	
	struct msg_xmit_hist* hist = &msghist;
	
	return hist->hiseq - hist->ackseq > FLOWCONTROL_LIMIT;
	
}


/* Add a packet to a channel's transmit history */
static void
add2_xmit_hist (struct msg_xmit_hist * hist, struct ha_msg* msg
,	seqno_t seq)
{
	int	slot;
	struct ha_msg* slotmsg;

	if (!cl_is_allocated(msg)) {
		cl_log(LOG_CRIT, "Unallocated message in add2_xmit_hist");
		abort();
	}
	AUDITXMITHIST;
	/* Figure out which slot to put the message in */
	slot = hist->lastmsg+1;
	if (slot >= MAXMSGHIST) {
		slot = 0;
	}
	hist->hiseq = seq;
	slotmsg = hist->msgq[slot];
	/* Throw away old packet in this slot */
	if (slotmsg != NULL) {
		/* Lowseq is less than the lowest recorded seqno */
		hist->lowseq = hist->seqnos[slot];
		hist->msgq[slot] = NULL;
		if (!cl_is_allocated(slotmsg)) {
			cl_log(LOG_CRIT
			,	"Unallocated slotmsg in add2_xmit_hist");
		}else{
			ha_msg_del(slotmsg);
		}
	}
	hist->msgq[slot] = msg;
	hist->seqnos[slot] = seq;
	hist->lastrexmit[slot] = 0L;
	hist->lastmsg = slot;
	
	if (enable_flow_control
	&&	live_node_count > 1) {
		int priority = 0;

		if ((hist->hiseq - hist->lowseq) > ((MAXMSGHIST*9)/10)) {
			priority = LOG_ERR;
		} else if ((hist->hiseq - hist->lowseq) > ((MAXMSGHIST*3)/4)) {
			priority = LOG_WARNING;
		}
		if (priority > 0) {
			cl_log(priority
			,	"Message hist queue is filling up"
			" (%d messages in queue)"
			,       (int)(hist->hiseq - hist->lowseq));
			hist_display(hist);
		}
	}

	AUDITXMITHIST;
	
	if (enable_flow_control
	&&	hist->hiseq - hist->ackseq > FLOWCONTROL_LIMIT){
		if (live_node_count < 2) {
			update_ackseq(hist->hiseq - (FLOWCONTROL_LIMIT-1));
			all_clients_resume();
		}else{
#if 0
			cl_log(LOG_INFO, "Flow control engaged with %d live nodes"
			,	live_node_count);
#endif
			all_clients_pause();
			hist_display(hist);
		}
	}
}


#define	MAX_REXMIT_BATCH	50
static void
process_rexmit(struct msg_xmit_hist * hist, struct ha_msg* msg)
{
	const char *	cfseq;
	const char *	clseq;
	seqno_t		fseq = 0;
	seqno_t		lseq = 0;
	seqno_t		thisseq;
	int		firstslot = hist->lastmsg-1;
	int		rexmit_pkt_count = 0;
	const char*	fromnodename = ha_msg_value(msg, F_ORIG);
	struct node_info* fromnode = NULL;

	if (fromnodename == NULL){
		cl_log(LOG_ERR, "process_rexmit"
		": from node not found in the message");
		return;		
	}
	if (firstslot >= MAXMSGHIST) {
		cl_log(LOG_ERR, "process_rexmit"
		": firstslot out of range [%d]"
		,	firstslot);
		hist->lastmsg = firstslot = MAXMSGHIST-1;
	}
	
	fromnode = lookup_tables(fromnodename, NULL);
	if (fromnode == NULL){
		cl_log(LOG_ERR, "fromnode not found ");
		return ;
	}

	if ((cfseq = ha_msg_value(msg, F_FIRSTSEQ)) == NULL
	    ||	(clseq = ha_msg_value(msg, F_LASTSEQ)) == NULL
	    ||	(fseq=atoi(cfseq)) <= 0 || (lseq=atoi(clseq)) <= 0
	||	fseq > lseq) {
		cl_log(LOG_ERR, "Invalid rexmit seqnos");
		cl_log_message(LOG_ERR, msg);
	}

	
	if (ANYDEBUG){
		cl_log(LOG_DEBUG, "rexmit request from node %s for msg(%ld-%ld)",
		       fromnodename, fseq, lseq);
	}
	/*
	 * Retransmit missing packets in proper sequence.
	 */
	for (thisseq = fseq; thisseq <= lseq; ++thisseq) {
		int	msgslot;
		int	foundit = 0;

		if (thisseq <= fromnode->track.ackseq){
			/* this seq has been ACKed by fromnode
			   we can saftely ignore this request message*/
			continue;
		}
		if (thisseq <= hist->lowseq) {
			/* Lowseq is less than the lowest recorded seqno */
			nak_rexmit(hist, thisseq, fromnodename, "seqno too low");
			continue;
		}
		if (thisseq > hist->hiseq) {
			/*
			 * Hopefully we just restarted and things are
			 * momentarily a little out of sync...
			 * Since the rexmit request doesn't send out our
			 * generation number, we're just guessing
			 * ... nak_rexmit(thisseq, fromnode, "seqno too high"); ...
			 *
			 * Otherwise it's a bug ;-)
			 */
			cl_log(LOG_WARNING
			,	"Rexmit of seq %lu requested. %lu is max."
			,	thisseq, hist->hiseq);
			continue;
		}

		for (msgslot = firstslot
		;	!foundit && msgslot != (firstslot+1); --msgslot) {
			char *		smsg;
			longclock_t	now = time_longclock();
			longclock_t	last_rexmit;
			size_t		len;

			if (msgslot < 0) {
				/* Time to wrap around */
				if (firstslot == MAXMSGHIST-1) { 
				  /* We're back where we started */
					break;
				}
				msgslot = MAXMSGHIST-1;
			}
			if (hist->msgq[msgslot] == NULL) {
				continue;
			}
			if (hist->seqnos[msgslot] != thisseq) {
				continue;
			}

			/*
			 * We resend a packet unless it has been re-sent in
			 * the last REXMIT_MS milliseconds.
			 */
			last_rexmit = hist->lastrexmit[msgslot];

			if (cmp_longclock(last_rexmit, zero_longclock) != 0
			&&	longclockto_ms(sub_longclock(now,last_rexmit))
			<	(ACCEPT_REXMIT_REQ_MS)) {
				/* Continue to outer loop */
				goto NextReXmit;
			}
			/*
			 *	Don't send too many packets all at once...
			 *	or we could flood serial links...
			 */
			++rexmit_pkt_count;
			if (rexmit_pkt_count > MAX_REXMIT_BATCH) {
				return;
			}
			/* Found it!	Let's send it again! */
			firstslot = msgslot -1;
			foundit=1;
			if (ANYDEBUG) {
				cl_log(LOG_INFO, "Retransmitting pkt %lu"
				,	thisseq);
				cl_log(LOG_INFO, "msg size =%d, type=%s",
				       get_stringlen(hist->msgq[msgslot]),
				       ha_msg_value(hist->msgq[msgslot], F_TYPE));
			}
			smsg = msg2wirefmt(hist->msgq[msgslot], &len);

			if (DEBUGPKT) {
				cl_log_message(LOG_INFO, hist->msgq[msgslot]);
				cl_log(LOG_INFO
				,	"Rexmit STRING conversion: [%s]"
				,	smsg);
	 		}

			/* If it didn't convert, throw original msg away */
			if (smsg != NULL) {
				hist->lastrexmit[msgslot] = now;
				send_to_all_media(smsg
				  ,	len);
				cl_free(smsg);
			}

		}
		if (!foundit) {
			nak_rexmit(hist, thisseq, fromnodename, "seqno not found");
		}
NextReXmit:/* Loop again */;
	}
}


static void
printout_histstruct(struct msg_xmit_hist* hist)
{
	cl_log(LOG_INFO,"hist information:");
	cl_log(LOG_INFO, "hiseq =%lu, lowseq=%lu,ackseq=%lu,lastmsg=%d",
	       hist->hiseq, hist->lowseq, hist->ackseq, hist->lastmsg);
	
}
static void
nak_rexmit(struct msg_xmit_hist * hist, 
	   seqno_t seqno, 
	   const char* fromnodename,
	   const char * reason)
{
	struct ha_msg*	msg;
	char	sseqno[32];
	struct node_info* fromnode = NULL;

	fromnode = lookup_tables(fromnodename, NULL);
	if (fromnode == NULL){
		cl_log(LOG_ERR, "fromnode not found ");
		return ;
	}
	
	snprintf(sseqno, sizeof(sseqno), "%lx", seqno);
	cl_log(LOG_ERR, "Cannot rexmit pkt %lu for %s: %s", 
	       seqno, fromnodename, reason);
	
	cl_log(LOG_INFO, "fromnode =%s, fromnode's ackseq = %ld", 
	       fromnode->nodename, fromnode->track.ackseq);
	
	printout_histstruct(hist);
	
	if ((msg = ha_msg_new(6)) == NULL) {
		cl_log(LOG_ERR, "no memory for " T_NAKREXMIT);
		return;
	}
	
	if (ha_msg_add(msg, F_TYPE, T_NAKREXMIT) != HA_OK
	    ||	ha_msg_add(msg, F_FIRSTSEQ, sseqno) != HA_OK
	    ||  ha_msg_add(msg, F_TO, fromnodename) !=HA_OK
	    ||	ha_msg_add(msg, F_COMMENT, reason) != HA_OK) {
		cl_log(LOG_ERR, "cannot create " T_NAKREXMIT " msg.");
		ha_msg_del(msg); msg=NULL;
		return;
	}
	send_cluster_msg(msg);
}


int
ParseTestOpts()
{
	const char *	openpath = HA_HBCONF_DIR "/OnlyForTesting";
	FILE *	fp;
	static struct TestParms p;
	char	name[64];
	char	value[512];
	int	something_changed = 0;

	if ((fp = fopen(openpath, "r")) == NULL) {
		if (TestOpts) {
			cl_log(LOG_INFO, "Test Code Now disabled.");
			something_changed=1;
		}
		TestOpts = NULL;
		return something_changed;
	}
	TestOpts = &p;
	something_changed=1;

	memset(&p, 0, sizeof(p));
	p.send_loss_prob = 0;
	p.rcv_loss_prob = 0;

	cl_log(LOG_INFO, "WARNING: Enabling Test Code");

	while((fscanf(fp, "%[a-zA-Z_]=%s\n", name, value) == 2)) {
		if (strcmp(name, "rcvloss") == 0) {
			p.rcv_loss_prob = atof(value);
			p.enable_rcv_pkt_loss = 1;
			cl_log(LOG_INFO, "Receive loss probability = %.3f"
			,	p.rcv_loss_prob);
		}else if (strcmp(name, "xmitloss") == 0) {
			p.send_loss_prob = atof(value);
			p.enable_send_pkt_loss = 1;
			cl_log(LOG_INFO, "Xmit loss probability = %.3f"
			,	p.send_loss_prob);
		}else if (strcmp(name, "allownodes") == 0) {
			strncpy(p.allow_nodes, value, sizeof(p.allow_nodes)-1);
			cl_log(LOG_INFO, "Allow nodes = %s", p.allow_nodes);
		}else{
			cl_log(LOG_ERR
			,	"Cannot recognize test param [%s] in [%s]"
			,	name, openpath);
		}
	}
	cl_log(LOG_INFO, "WARNING: Above Options Now Enabled.");
	
	fclose(fp);
	
	return something_changed;
}

#ifndef HB_VERS_FILE
/*
 * This file needs to be persistent across reboots, but isn't
 * really a log
 */
#	define HB_VERS_FILE HA_VARLIBHBDIR "/hb_generation"
#endif

#define	GENLEN	16	/* Number of chars on disk for gen # and '\n' */


/*
 *	Increment our generation number
 *	It goes up each time we restart to prevent replay attacks.
 */

#ifndef O_SYNC
#	define O_SYNC 0
#endif

static int
IncrGeneration(seqno_t * generation)
{
	char		buf[GENLEN+1];
	int		fd;
	int		flags = 0;

	if ((fd = open(HB_VERS_FILE, O_RDONLY)) < 0
	||	read(fd, buf, sizeof(buf)) < 1) {
		GetTimeBasedGeneration(generation);
		cl_log(LOG_WARNING, "No Previous generation - starting at %lu"
		,		(unsigned long)(*generation)+1);
		snprintf(buf, sizeof(buf), "%*lu", GENLEN, *generation);
		flags = O_CREAT;
	}
	close(fd);

	buf[GENLEN] = EOS;
	if (sscanf(buf, "%lu", generation) <= 0) {
		GetTimeBasedGeneration(generation);
		cl_log(LOG_WARNING, "BROKEN previous generation - starting at %ld"
		,	(*generation)+1);
		flags = O_CREAT;
		*generation = 0;
	}
	
	++(*generation);
	snprintf(buf, sizeof(buf), "%*lu\n", GENLEN-1, *generation);

	if ((fd = open(HB_VERS_FILE, O_WRONLY|O_SYNC|flags, 0644)) < 0) {
		return HA_FAIL;
	}
	if (write(fd, buf, GENLEN) != GENLEN) {
		close(fd);
		return HA_FAIL;
	}

	/*
	 * Some UNIXes don't implement O_SYNC.
	 * So we do an fsync here for good measure.  It can't hurt ;-)
	 */

	if (fsync(fd) < 0) {
		cl_perror("fsync failure on " HB_VERS_FILE);
		return HA_FAIL;
	}
	if (close(fd) < 0) {
		cl_perror("close failure on " HB_VERS_FILE);
		return HA_FAIL;
	}
	/*
	 * We *really* don't want to lose this data.  We won't be able to
	 * join the cluster again without it.
	 */
	sync();
#if HAVE_UNRELIABLE_FSYNC
	sleep(10);
#endif
	return HA_OK;
}



static int
GetTimeBasedGeneration(seqno_t * generation)
{
	*generation = (seqno_t) time(NULL);
	return HA_OK;
}


static void
get_localnodeinfo(void)
{
	const char *		openpath = HA_HBCONF_DIR "/nodeinfo";
	static struct utsname	u;
	static char		localnode[256];
	FILE *			fp;

	if (uname(&u) < 0) {
		cl_perror("uname(2) call failed");
		return;
	}

	localnodename = u.nodename;

	if ((fp = fopen(openpath, "r")) != NULL
	&&	fgets(localnode, sizeof(localnode), fp) != NULL
	&&	localnode[0] != EOS) {
		char * nlpos;
		if ((nlpos = memchr(localnode, '\n', sizeof(localnode))) != NULL) {
			*nlpos = EOS;
			localnodename = localnode;
		}
	}
	if (fp) {
		fclose(fp);
	}
	g_strdown(localnodename);
}
static void
hb_add_deadtime(int increment)
{
	longclock_t new_ticks;
	new_ticks = msto_longclock(config->deadtime_ms + increment);
	if (curnode->dead_ticks < new_ticks) {
		curnode->dead_ticks = new_ticks;
		send_local_status();
	}
	deadtime_tmpadd_count++;
}
static gboolean
hb_pop_deadtime(gpointer p)
{
	deadtime_tmpadd_count--;
	if (deadtime_tmpadd_count <= 0) {
		curnode->dead_ticks = msto_longclock(config->deadtime_ms);
		send_local_status();
		deadtime_tmpadd_count = 0;
	}
	return FALSE;
}

