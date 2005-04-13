/*
 * TODO:
 * 1) Man page update
 */
/* $Id: heartbeat.c,v 1.390 2005/04/13 23:01:19 alan Exp $ */
/*
 * heartbeat: Linux-HA heartbeat code
 *
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

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include <uuid/uuid.h>

#include "setproctitle.h"

#define OPTARGS			"dkMrRsvlC:"
#define	ONEDAY			(24*60*60)	/* Seconds in a day */
#define REAPER_SIG		0x0001UL
#define TERM_SIG		0x0002UL
#define DEBUG_USR1_SIG		0x0004UL
#define DEBUG_USR2_SIG		0x0008UL
#define PARENT_DEBUG_USR1_SIG	0x0010UL
#define PARENT_DEBUG_USR2_SIG	0x0020UL
#define REREAD_CONFIG_SIG	0x0040UL
#define FALSE_ALARM_SIG		0x0080UL



#define	ALWAYSRESTART_ON_SPLITBRAIN	1


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

int				debug = 0;
gboolean			verbose = FALSE;
int				timebasedgenno = FALSE;
int				parse_only = FALSE;
static gboolean			killrunninghb = FALSE;
static gboolean			rpt_hb_status = FALSE;
int				RestartRequested = FALSE;
static long			hb_pid_in_file = 0L;
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
static int			CoreProcessCount = 0;
static int			managed_child_count= 0;
int				UseOurOwnPoll = FALSE;
static longclock_t		NextPoll = 0UL;
static int			ClockJustJumped = FALSE;
longclock_t			local_takeover_time = 0L;
static int 			deadtime_tmpadd_count = 0;
gboolean			enable_flow_control = TRUE;

static void print_a_child_client(gpointer childentry, gpointer unused);

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
static void	request_msg_rexmit(struct node_info *, seqno_t lowseq
,			seqno_t hiseq);
static void	check_rexmit_reqs(void);
static void	mark_node_dead(struct node_info* hip);
static void	change_link_status(struct node_info* hip, struct link *lnk
,			const char * new);
static void	comm_now_up(void);
static long	get_running_hb_pid(void);
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
static gboolean hb_reregister_with_apphbd(gpointer dummy);

static void	hb_add_deadtime(int increment);
static gboolean	hb_pop_deadtime(gpointer p);

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


/*
 * The biggies
 */
static void	read_child(struct hb_media* mp);
static void	write_child(struct hb_media* mp);
static void	fifo_child(IPC_Channel* chan);		/* Reads from FIFO */
		/* The REAL biggie ;-) */
static void	master_control_process(IPC_Channel* fifoproc);

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

	if ((ipcid = shmget(IPC_PRIVATE, sizeof(*procinfo), 0666)) < 0) {
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
#ifdef STRINGSCMD
	static int	everprinted=0;
	char		cmdline[MAXLINE];
	char		buf[MAXLINE];
	FILE	 	*f;

	cl_log(LOG_INFO, "%s: version %s", cmdname, VERSION);

	/*
	 * The reason why we only do this once is that we are doing it with
	 * our priority which could hang the machine, and forking could
	 * possibly cause us to miss a heartbeat if this is done
	 * under load.
	 * FIXME!  We really need fork anyway...
	 */
	if (!(ANYDEBUG && !everprinted)) {
		return;
	}

	/*
	 * Do 'strings' on ourselves, and look for version info...
	 */

	/* This command had better be well-behaved! */
	snprintf(cmdline, MAXLINE
		/* Break up the string so RCS won't react to it */
	,	"%s %s/%s | grep '^\\$Id" ": .*\\$' | sort -u"
	,	STRINGSCMD, HALIB, cmdname);

	if ((f = popen(cmdline, "r")) == NULL) {
		cl_perror("Cannot run: %s", cmdline);
		return;
	}

	while (fgets(buf, MAXLINE, f)) {
		++everprinted;
		if (buf[strlen(buf)-1] == '\n') {
			buf[strlen(buf)-1] = EOS;
		}
		cl_log(LOG_INFO, "%s", buf);
	}

	pclose(f);
#endif
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

	if ( (shost = ha_strdup(h)) == NULL) {
		return NULL;
	}
	g_strdown(shost);
	for (j=0; j < config->nodecount; ++j) {
		if (strcmp(shost, config->nodes[j].nodename) == 0)
			break;
	}
	ha_free(shost);
	if (j == config->nodecount) {
		return NULL;
	} else {
		return (config->nodes+j);
	}
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
		cl_log(LOG_ERR, "change_logfile_ownship:"
		       " entry for user %s not found", apiuser);
		return;
	}
	
	if (config->use_logfile){
		chown(config->logfile, entry->pw_uid, entry->pw_gid);
	}
	if (config->use_dbgfile){
		chown(config->dbgfile, entry->pw_uid, entry->pw_gid);
	}
	
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
	IPC_Channel*	fifochildipc[2];
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
	
	if(GetUUID(config->uuid) != HA_OK){
		cl_log(LOG_ERR, "getting uuid for the local node failed");
		return HA_FAIL;
	}
	
	add_uuidtable(config->uuid, (char*)curnode);
	uuid_copy(curnode->uuid, config->uuid);

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
	system("rm -fr " RSC_TMPDIR);

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
		if (smj->vf->open(smj) != HA_OK) {
			cl_log(LOG_ERR, "cannot open %s %s"
			,	smj->type
			,	smj->name);
			return HA_FAIL;
		}
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "%s channel %s now open..."
			,	smj->type, smj->name);
		}
	}

 	PILSetDebugLevel(PluginLoadingSystem, NULL, NULL, debug);
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

	if (ipc_channel_pair(fifochildipc) != IPC_OK) {
		cl_perror("cannot create FIFO ipc channel");
		return HA_FAIL;
	}

	/* Now the fun begins... */
/*
 *	Optimal starting order:
 *		fifo_child();
 *		write_child();
 *		read_child();
 *		master_control_process();
 *
 */

	/* Fork FIFO process... */
	ourproc = procinfo->nprocs;
	switch ((pid=fork())) {
		case -1:	cl_perror("Can't fork FIFO process!");
				return HA_FAIL;
				break;

		case 0:		/* Child */
				close(watchdogfd);
				curproc = &procinfo->info[ourproc];
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
	NewTrackedProc(pid, 0, PT_LOGVERBOSE, GINT_TO_POINTER(ourproc)
	,	&CoreProcessTrackOps);

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "FIFO process pid: %d", pid);
	}

	ourproc = procinfo->nprocs;

	for (j=0; j < nummedia; ++j) {
		struct hb_media* mp = sysmedia[j];

		ourproc = procinfo->nprocs;

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
	}




	ourproc = procinfo->nprocs;
	master_control_process(fifochildipc[P_READFD]);

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

	if (hb_signal_set_read_child(NULL) < 0) {
		cl_log(LOG_ERR, "read_child(): hb_signal_set_read_child(): "
		"Soldiering on...");
	}

	cl_make_realtime(-1, hb_realtime_prio, 16, 8);
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
			continue;
		}
		hb_signal_process_pending();
		
		imsg = wirefmt2ipcmsg(pkt, pktlen, ourchan);
		ha_free(pkt);
		if (imsg != NULL) {
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
	cl_make_realtime(-1, hb_realtime_prio, 16, 8);
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

	cl_make_realtime(-1, hb_realtime_prio, 16, 32);
	cl_cdtocoredir();
	cl_set_all_coredump_signal_handlers();
	drop_privs(0, 0);	/* Become nobody */
	curproc->pstat = RUNNING;

	if (ANYDEBUG) {
		/* Limit ourselves to 10% of the CPU */
		cl_cpu_limit_setpercent(10);
	}

	for (;;) {

		msg = msgfromstream(fifo);
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
				hb_signal_process_pending();
				chan->ops->waitout(chan);
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
			exit(2);
		}
		cl_cpu_limit_update();
		cl_realtime_malloc_check();
	}
}

static gboolean
Gmain_hb_signal_process_pending(void *data)
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
	struct ha_msg*	msg = msgfromIPC(source, 0);

	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG, "FIFO_child_msg_dispatch() {");
	}
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
	struct ha_msg*	msg = msgfromIPC(source, MSG_NEEDAUTH);
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
master_control_process(IPC_Channel* fifoproc)
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
	GCHSource*		FifoChildSource;
	GMainLoop*		mainloop;
	long			memstatsinterval;


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
		/* Limit ourselves to 50% of the CPU */
		cl_cpu_limit_setpercent(50);
		/* Update our CPU limit periodically */
		Gmain_timeout_add_full(G_PRIORITY_HIGH-1
		,	cl_cpu_limit_ms_interval()
		,	hb_update_cpu_limit, NULL, NULL);
	}
	cl_make_realtime(-1, hb_realtime_prio, 32, 150);

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
	Gmain_timeout_add(5000, hb_pop_deadtime, NULL);

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



	/* We only read from this source, we never write to it */
	FifoChildSource = G_main_add_IPC_Channel(PRI_FIFOMSG, fifoproc
	,	FALSE, FIFO_child_msg_dispatch, NULL, NULL);


	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"Starting local status message @ %ld ms intervals"
		,	config->heartbeat_ms);
	}

	/* Child I/O processes */
	for(j = 0; j < nummedia; j++) {
		/*
		 * We cannot share a socket between the the write and read
		 * children, though it might sound like it would work ;-)
		 */

		/* Connect up the write child IPC channel... */
		G_main_add_IPC_Channel(PRI_CLUSTERMSG
		,	sysmedia[j]->wchan[P_WRITEFD], FALSE
		,	NULL, sysmedia+j, NULL);

		
		/* Connect up the read child IPC channel... */
		G_main_add_IPC_Channel(PRI_CLUSTERMSG
		,	sysmedia[j]->rchan[P_WRITEFD], FALSE
		,	read_child_dispatch, sysmedia+j, NULL);

}	
	

	/*
	 * Things to do on a periodic basis...
	 */
	
	/* Send local status at the "right time" */
	Gmain_timeout_add_full(PRI_SENDSTATUS, config->heartbeat_ms
	,	hb_send_local_status, NULL, NULL);

	/* Dump out memory stats periodically... */
	memstatsinterval = (debug ? 10*60*1000 : ONEDAY*1000);
	Gmain_timeout_add_full(PRI_DUMPSTATS, memstatsinterval
	,	hb_dump_all_proc_stats, NULL, NULL);

	/* Audit clients for liveness periodically */
	Gmain_timeout_add_full(PRI_AUDITCLIENT, 9*1000
	,	api_audit_clients, NULL, NULL);

	/* Reset timeout times to "now" */
	for (j=0; j < config->nodecount; ++j) {
		struct node_info *	hip;
		hip= &config->nodes[j];
		hip->local_lastupdate = time_longclock();
	}

	/* Check for pending signals */
	Gmain_timeout_add_full(PRI_SENDSTATUS, config->heartbeat_ms
	,       Gmain_hb_signal_process_pending, NULL, NULL);

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
	int	refcnt = GPOINTER_TO_INT(m->msg_private);

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
		ha_free(m->msg_buf);
		memset(m, 0, sizeof(*m));
		ha_free(m);
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


	if ((hdr = (IPC_Message*)ha_malloc(sizeof(*hdr)))  == NULL) {
		return NULL;
	}
	
	memset(hdr, 0, sizeof(*hdr));

	if ((copy = (char*)ha_malloc(ch->msgpad + len))
	    == NULL) {
		ha_free(hdr);
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

	if (DEBUGDETAILS){
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
	
	if (DEBUGDETAILS) {
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

	if (DEBUGDETAILS){
		cl_log(LOG_DEBUG,"polled_input_dispatch() {");
	}
	NextPoll = add_longclock(now, msto_longclock(POLL_INTERVAL));


	LookForClockJumps();

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
	check_rexmit_reqs();

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
	if (DEBUGDETAILS){
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

	if (ANYDEBUG){
		cl_log(LOG_DEBUG
		,	"Comm_now_up(): updating status to " ACTIVESTATUS);
	}
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
	static int shutdown_phase = 0;

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
		Gmain_timeout_add(1000, hb_mcp_final_shutdown /* phase 2 */
		,	NULL);
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
	cl_log(LOG_INFO,"Heartbeat shutdown complete.");

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
HBDoMsg_T_ACKMSG(const char * type, struct node_info * fromnode,
	      TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg)
{
	const char*	ackseq_str = ha_msg_value(msg, F_ACKSEQ);
	seqno_t		ackseq;
	struct msg_xmit_hist* hist = &msghist;	
	const char*	to =  
		(const char*)ha_msg_value(msg, F_TO);
	struct node_info* tonode;
	
	
	if (!to || (tonode = lookup_tables(to, NULL)) == NULL
	    || tonode != curnode){
		return;
	}

	if (ackseq_str == NULL||
	    sscanf(ackseq_str, "%lx", &ackseq) != 1){
		goto out;
	}


	if (ackseq == fromnode->track.ackseq){
		/*dup message*/
		goto out;
	}

	if (ackseq < hist->ackseq){
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
	
	if ( ackseq < fromnode->track.ackseq){
		/* late or dup ack
		 * ignored
		 */
		goto out;
	}
	
	fromnode->track.ackseq = ackseq;
	
	if (hist->lowest_acknode != NULL &&
	    STRNCMP_CONST(hist->lowest_acknode->status, 
		    DEADSTATUS) == 0){
		/* the lowest acked node is dead
		 * we cannnot count on that node 
		 * to update our ackseq
		 */
		hist->lowest_acknode = NULL;
	}

	if (hist->lowest_acknode == NULL ||
	    hist->lowest_acknode == fromnode){
		/*find the new lowest and update hist->ackseq*/
		seqno_t	minseq;
		int	minidx;
		int	i;
		
		hist->lowest_acknode = NULL;
		minidx = -1;
		minseq = 0;
		for (i = 0; i < config->nodecount; i++){
			struct node_info* hip = &config->nodes[i];
			
			if (STRNCMP_CONST(hip->status,DEADSTATUS) == 0
			    || STRNCMP_CONST(hip->status, INITSTATUS) == 0
			    /*although the status is active, but no ACK message
			     *has arrived from that node yet
			     */
			    || hip->track.ackseq == 0 
			    || hip->nodetype == PINGNODE_I){
				continue;
			}
			
			if (minidx == -1 || 
			    hip->track.ackseq < minseq){
				minseq = hip->track.ackseq;
				minidx = i;
			}
		}
		
		if (minidx == -1){
			/*each node is in either DEASTATUS or INITSTATUS*/
			goto out;
		}
		if (minidx == config->nodecount){
			cl_log(LOG_ERR, "minidx out of bound"
			       "minidx=%d",minidx );
			goto out;
		}
		hist->ackseq = minseq;
		hist->lowest_acknode = &config->nodes[minidx];
		
		if (hist->hiseq - hist->ackseq < MAXMSGHIST/2){
			all_clients_resume();
		}
	}
 out:
#if 0
	cl_log(LOG_INFO, "hist->ackseq =%ld, node %s's ackseq=%ld",
	       hist->ackseq, fromnode->nodename,
	       fromnode->track.ackseq);
	
	if (hist->lowest_acknode){
		cl_log(LOG_INFO,"expecting from %s",hist->lowest_acknode->nodename);
	}
#endif 

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
	
	/* Dooes it contain F_PROTOCOL field?*/
	
	

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
		
		QueueRemoteRscReq(PerformQueuedNotifyWorld, msg);
		strncpy(fromnode->status, status
		, 	sizeof(fromnode->status));
		heartbeat_monitor(msg, KEEPIT, iface);
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
send_ack_if_needed(struct node_info* thisnode, seqno_t seq)
{
	struct ha_msg*	hmsg;
	char		seq_str[32];
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
send_ack_if_necessary(const struct ha_msg* m)
{
	const char*	fromnode = ha_msg_value(m, F_ORIG);
	uuid_t		fromuuid;
	const char*	seq_str = ha_msg_value(m, F_SEQ);
	seqno_t		seq;
	struct	node_info*	thisnode = NULL;

	if (!enable_flow_control){
		return;
	}
	
	if ( cl_get_uuid(m, F_ORIGUUID, fromuuid) != HA_OK){
		uuid_clear(fromuuid);
	}
	
	if (fromnode == NULL ||
	    seq_str == NULL ||
	    sscanf( seq_str, "%lx", &seq) != 1){		
		return;
	}
	
	thisnode = lookup_tables(fromnode, fromuuid);
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
	uuid_t			fromuuid;
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
		}
	}

	/* Extract message type, originator, timestamp, auth */
	type = ha_msg_value(msg, F_TYPE);
	from = ha_msg_value(msg, F_ORIG);
	
	if ( cl_get_uuid(msg, F_ORIGUUID, fromuuid) != HA_OK){
		uuid_clear(fromuuid);
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
		sscanf(cseq, "%lx", &seqno);
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
	
	thisnode = lookup_tables(from, fromuuid);
	
	if (thisnode == NULL) {
#if defined(MITJA)
		/* If a node isn't in the configfile, add it... */
		cl_log(LOG_WARNING
		,   "process_status_message: new node [%s] in message"
		,	from);
		add_node(from, NORMALNODE_I);
		thisnode = lookup_node(from);
		if (thisnode == NULL) {
			return;
		}
#else
		/* If a node isn't in the configfile - whine */
		cl_log(LOG_ERR
		,   "process_status_message: bad node [%s] in message"
		,	from);
		cl_log_message(LOG_ERR, msg);
		return;
#endif
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
			cl_log(LOG_INFO,"Heartbeat shutdown complete.");
			if (procinfo->restart_after_shutdown) {
				cl_log(LOG_INFO
				,	"Heartbeat restart triggered.");
				restart_heartbeat();
			}
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
	int	procindex = GPOINTER_TO_INT(p->privatedata);
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
			,	"Client %s killed by signal %d."
			,	managedchild->command
			,	signo);
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
	int	nsig = GPOINTER_TO_INT(data);
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
		(void)execl("/bin/sh", "sh", "-c", centry->command
		,	(const char *)NULL);

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

	cl_log(LOG_INFO, "ha_malloc stats: %lu/%lu  %lu/%lu [pid%d/%s]"
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

	getrlimit(RLIMIT_NOFILE, &oflimits);
	for (j=3; j < oflimits.rlim_cur; ++j) {
		close(j);
	}

	hb_close_watchdog();
	
	if (quickrestart) {
		/* THIS IS RESOURCE WORK!  FIXME */
		if (nice_failback) {
			cl_log(LOG_INFO, "Current resources: -R -C %s"
			,	decode_resources(procinfo->i_hold_resources));
			execl(HALIB "/heartbeat", "heartbeat", "-R"
			,	"-C"
			,	decode_resources(procinfo->i_hold_resources)
			,	(const char *)NULL);
		}else{
			execl(HALIB "/heartbeat", "heartbeat", "-R"
			,	(const char *)NULL);
		}
	}else{
		/* Make sure they notice we're dead */
		sleep((config->deadtime_ms+999)/1000+1);
		/* "Normal" restart (not quick) */
		unlink(PIDFILE);
		execl(HALIB "/heartbeat", "heartbeat", (const char *)NULL);
	}
	cl_log(LOG_ERR, "Could not exec " HALIB "/heartbeat");
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
 * The anypktsyet field in the node structure gets set to TRUE whenever we
 * either hear from a node, or we declare it dead, and issue a fake "dead"
 * status packet.
 */

static void
check_comm_isup(void)
{
	struct node_info *	hip;
	int	j;
	int	heardfromcount = 0;

	if (heartbeat_comm_state == COMM_LINKSUP) {
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
		comm_now_up();
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

	if (msg == NULL || (type = ha_msg_value(msg, F_TYPE)) == NULL) {
		cl_perror("Invalid message in send_cluster_msg");
		if (msg != NULL) {
			ha_msg_del(msg);
		}
		return HA_FAIL;
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
		ssize_t	writerc;

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

		if ((smsg = msg2wirefmt(msg, &len)) == NULL) {
			cl_log(LOG_ERR
			,	"send_cluster_msg: cannot convert"
			" message to wire format (pid %d)", (int)getpid());
			rc = HA_FAIL;
		}else if ((ffd = open(FIFONAME, O_WRONLY|O_NDELAY|O_APPEND)) < 0) {
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
			if (DEBUGDETAILS) {
				cl_log(LOG_INFO
				,	"FIFO message [type %s] written"
				, type);
			}
			ha_free(smsg);
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

		if (!shutdown_in_progress) {
			cause_shutdown_restart();
		}
		return;
	}

	strncpy(hip->status, DEADSTATUS, sizeof(hip->status));
	/* THIS IS RESOURCE WORK!  FIXME */
	hb_rsc_recover_dead_resources(hip);
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
	long		running_hb_pid = get_running_hb_pid();
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

	if ((tmp_cmdname = ha_strdup(argv[0])) == NULL) {
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
				++debug;
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
				++verbose;
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

	get_localnodeinfo();
	SetParameterValue(KEY_HBVERSION, VERSION);

	/* Default message handling... */
	hb_register_msg_callback(T_REXMIT,	HBDoMsg_T_REXMIT);
	hb_register_msg_callback(T_STATUS,	HBDoMsg_T_STATUS);
	hb_register_msg_callback(T_NS_STATUS,	HBDoMsg_T_STATUS);
	hb_register_msg_callback(T_QCSTATUS,	HBDoMsg_T_QCSTATUS);
	hb_register_msg_callback(T_ACKMSG,	HBDoMsg_T_ACKMSG);

	if (init_set_proc_title(argc, argv, envp) < 0) {
		cl_log(LOG_ERR, "Allocation of proc title failed.");
		cleanexit(generic_error);
	}
	set_proc_title("%s", cmdname);

	hbmedia_types = ha_malloc(sizeof(struct hbmedia_types **));

	if (hbmedia_types == NULL) {
		cl_log(LOG_ERR, "Allocation of hbmedia_types failed.");
		cleanexit(generic_error);
	}



	if (debug > 0) {
		static char cdebug[8];
		snprintf(cdebug, sizeof(debug), "%d", debug);
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
			cleanexit(hb_pid_in_file ? LSB_STATUS_VAR_PID
			:	LSB_STATUS_STOPPED);
		}else{
			struct utsname u;
			uname(&u);
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
		||	parse_ha_resources(RESOURCE_CFG) != HA_OK){
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
		&&	parse_ha_resources(RESOURCE_CFG)){
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
                &&      parse_ha_resources(RESOURCE_CFG))) {
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
		setenv(HALOGD, cl_log_get_uselogd()? "yes":"no", 1);
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
	cl_log(LOG_ERR, "Emergency Shutdown: "
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

static long
get_running_hb_pid()
{
	long	pid;
	FILE *	lockfd;
	if ((lockfd = fopen(PIDFILE, "r")) != NULL
	&&	fscanf(lockfd, "%ld", &pid) == 1 && pid > 0) {
		hb_pid_in_file = pid;
		if (CL_KILL((pid_t)pid, 0) >= 0 || errno != ESRCH) {
			fclose(lockfd);
			return pid;
		}
	}
	if (lockfd != NULL) {
		fclose(lockfd);
	}
	return -1L;
}


extern pid_t getsid(pid_t);


static void
make_daemon(void)
{
	long			pid;
	FILE *			lockfd;
	const char *		devnull = "/dev/null";

	/* See if heartbeat is already running... */

	if ((pid=get_running_hb_pid()) > 0 && pid != getpid()) {
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
			fprintf(stderr, "%s: could not start daemon\n"
			,	cmdname);
			perror("fork");
			exit(LSB_EXIT_GENERIC);
		}else if (pid > 0) {
			exit(LSB_EXIT_OK);
		}
	}
	pid = (long) getpid();
	lockfd = fopen(PIDFILE, "w");
	if (lockfd != NULL) {
		fprintf(lockfd, "%ld\n", pid);
		fclose(lockfd);
	}else{
		fprintf(stderr, "%s: could not create pidfile [%s]\n"
		,	cmdname, PIDFILE);
		exit(LSB_EXIT_EPERM);
	}

	cl_log_enable_stderr(FALSE);

	setenv(HADIRENV, HA_D, TRUE);
	setenv(DATEFMT, HA_DATEFMT, TRUE);
	setenv(HAFUNCENV, HA_FUNCS, TRUE);
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
#define	SEQGAP	100	/* A heuristic number */

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
	uuid_t			touuid;
	const char *		from= ha_msg_value(msg, F_ORIG);
	uuid_t			fromuuid;
	const char *		type = ha_msg_value(msg, F_TYPE);
	const char *		cgen = ha_msg_value(msg, F_HBGENERATION);
	seqno_t			seq;
	seqno_t			gen = 0;
	int			IsToUs;
	int			j;
	int			isrestart = 0;
	int			ishealedpartition = 0;
	int			is_status = 0;
	
	
	if ( cl_get_uuid(msg, F_ORIGUUID, fromuuid) != HA_OK){
		uuid_clear(fromuuid);
	}
	
	if (from && !uuid_is_null(fromuuid)){
		update_tables(from, fromuuid);
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
	if (cgen != NULL) {
		sscanf(cgen, "%lx", &gen);
	}
	
	if ( cl_get_uuid(msg, F_TOUUID, touuid) != HA_OK){
		uuid_clear(touuid);
	}
	
	
	if(!uuid_is_null(touuid)){
		IsToUs = (uuid_compare(touuid, config->uuid) == 0);
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

				cl_log(LOG_WARNING
				,	"Cluster node %s"
				" returning after partition."
				,	thisnode->nodename);
				cl_log(LOG_WARNING
				,	"Deadtime value may be too small.");
				cl_log(LOG_INFO
				,	"See documentation for information"
				" on tuning deadtime.");

				/* THIS IS RESOURCE WORK!  FIXME */
				/* IS THIS RIGHT??? FIXME ?? */
#ifndef ALWAYSRESTART_ON_SPLITBRAIN
				if (DoManageResources) {
#endif
					send_local_status();
					Gmain_timeout_add(2000
					,	CauseShutdownRestart, NULL);
#ifndef ALWAYSRESTART_ON_SPLITBRAIN
				}
#endif
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


	/* Direct message to "loopback" processing */
	process_clustermsg(msg, NULL);

	send_to_all_media(smsg, len);
	ha_free(smsg);

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
		
	}else if (t->first_missing_seq != 0){
		request_msg_rexmit(thisnode, t->first_missing_seq, t->first_missing_seq);
	}
	return ret;
	
}


static void
request_msg_rexmit(struct node_info *node, seqno_t lowseq
,	seqno_t hiseq)
{
	struct ha_msg*	hmsg;
	char		low[16];
	char		high[16];
	if ((hmsg = ha_msg_new(6)) == NULL) {
		cl_log(LOG_ERR, "no memory for " T_REXMIT);
		return;
	}

	snprintf(low, sizeof(low), "%lu", lowseq);
	snprintf(high, sizeof(high), "%lu", hiseq);


	if (	ha_msg_add(hmsg, F_TYPE, T_REXMIT) == HA_OK
	&&	ha_msg_add(hmsg, F_TO, node->nodename)==HA_OK
	&&	ha_msg_add(hmsg, F_FIRSTSEQ, low) == HA_OK
	&&	ha_msg_add(hmsg, F_LASTSEQ, high) == HA_OK) {
		/* Send a re-transmit request */
		if (send_cluster_msg(hmsg) != HA_OK) {
			cl_log(LOG_ERR, "cannot send " T_REXMIT
			" request to %s", node->nodename);
		}
		node->track.last_rexmit_req = time_longclock();
	}else{
		ha_msg_del(hmsg);
		cl_log(LOG_ERR, "Cannot create " T_REXMIT " message.");
	}
}

#define REXMIT_MS	1000

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

		if (cmp_longclock(t->last_rexmit_req, minrexmit) < 0) {
			/* Too soon to ask for retransmission */
			continue;
		}
		/* Time to ask for some packets again ... */
		for (seqidx = 0; seqidx < t->nmissing; ++seqidx) {
			if (t->seqmissing[seqidx] != NOSEQUENCE) {
				/*
				 * The code for asking for these by groups
				 * is complicated.  This code is not.
				 */
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
		if (!ha_is_allocated(msg)) {
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
		if (!ha_is_allocated(msg->names)) {
			cl_log(LOG_CRIT
			,	"Unallocated msg->names in audit_xmit_hist");
			doabort=TRUE;
		}
		if (!ha_is_allocated(msg->nlens)) {
			cl_log(LOG_CRIT
			,	"Unallocated msg->nlens in audit_xmit_hist");
			doabort=TRUE;
		}
		if (!ha_is_allocated(msg->values)) {
			cl_log(LOG_CRIT
			,	"Unallocated msg->values in audit_xmit_hist");
			doabort=TRUE;
		}
		if (!ha_is_allocated(msg->vlens)) {
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
	
	return hist->hiseq - hist->ackseq > MAXMSGHIST/2;
	
}


/* Add a packet to a channel's transmit history */
static void
add2_xmit_hist (struct msg_xmit_hist * hist, struct ha_msg* msg
,	seqno_t seq)
{
	int	slot;
	struct ha_msg* slotmsg;

	if (!ha_is_allocated(msg)) {
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
	if (hist->lowseq == 0) {
		hist->lowseq = seq;
	}
	slotmsg = hist->msgq[slot];
	/* Throw away old packet in this slot */
	if (slotmsg != NULL) {
		/* Lowseq is less than the lowest recorded seqno */
		hist->lowseq = hist->seqnos[slot];
		hist->msgq[slot] = NULL;
		if (!ha_is_allocated(slotmsg)) {
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
	AUDITXMITHIST;
	
	if (enable_flow_control
	    && hist->hiseq - hist->ackseq > MAXMSGHIST/2){
		all_clients_pause();
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
	
	if (fromnodename == NULL){
		cl_log(LOG_ERR, "process_rexmit: "
		       "from node not found in the message");
		return;		
	}
	
	if ((cfseq = ha_msg_value(msg, F_FIRSTSEQ)) == NULL
	    ||	(clseq = ha_msg_value(msg, F_LASTSEQ)) == NULL
	||	(fseq=atoi(cfseq)) <= 0 || (lseq=atoi(clseq)) <= 0
	||	fseq > lseq) {
		cl_log(LOG_ERR, "Invalid rexmit seqnos");
		cl_log_message(LOG_ERR, msg);
	}

	/*
	 * Retransmit missing packets in proper sequence.
	 */
	for (thisseq = fseq; thisseq <= lseq; ++thisseq) {
		int	msgslot;
		int	foundit = 0;
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
				msgslot = MAXMSGHIST;
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
			<	REXMIT_MS) {
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
	
	snprintf(sseqno, sizeof(sseqno), "%lx", seqno);
	cl_log(LOG_ERR, "Cannot rexmit pkt %lu for %s: %s", 
	       seqno, fromnodename, reason);

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
	const char *	openpath = HA_D "/OnlyForTesting";
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
	return something_changed;
}

#ifndef HB_VERS_FILE
/*
 * This file needs to be persistent across reboots, but isn't
 * really a log
 */
#	define HB_VERS_FILE VAR_LIB_D "/hb_generation"
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
		cl_log(LOG_WARNING, "No Previous generation - starting at 1");
		snprintf(buf, sizeof(buf), "%*d", GENLEN, 0);
		flags = O_CREAT;
	}
	close(fd);

	buf[GENLEN] = EOS;
	sscanf(buf, "%lu", generation);
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
	const char *		openpath = HA_D "/nodeinfo";
	static struct utsname	u;
	static char		localnode[256];
	FILE *			fp;
	uname(&u);
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

/*
 * $Log: heartbeat.c,v $
 * Revision 1.390  2005/04/13 23:01:19  alan
 * fixed stupid syntax error that somehow escaped me.
 *
 * Revision 1.389  2005/04/13 22:26:20  alan
 * Put in code to cause us to dump core - even if we're running seteuid to nobody.
 *
 * Revision 1.388  2005/04/13 18:04:46  gshi
 * bug 442:
 *
 * Enable logging daemon  by default
 * use static variables in cl_log and export interfaces to get/set variables
 *
 * Revision 1.387  2005/04/11 19:51:23  gshi
 * add logging channel to main loop
 *
 * Revision 1.386  2005/04/01 20:17:29  gshi
 * set channel peer pid correctly bwteeen heartbeat and write/read/fifo child
 *
 * Revision 1.385  2005/03/22 23:35:01  alan
 * Undid change done for bugzilla 298: infinite loop in heartbeat
 * since it's an infinite loop, upping the CPU % was not helpful.
 *
 * Revision 1.384  2005/03/22 21:19:11  gshi
 * if we have not got any ACK message from a node yet,
 * we exclude it in compuation.
 *
 * Revision 1.383  2005/03/21 18:40:57  gshi
 * we don't pause clients if flow control is not enabled
 *
 * Revision 1.382  2005/03/21 18:05:06  gshi
 * fixed a bug reported by Jason Whiteaker:
 * heartbeat shall not expect ACK from ping nodes
 *
 * Revision 1.381  2005/03/21 17:51:57  gshi
 * disable flow control in heartbeat when running with old versions in other nodes
 *
 * Revision 1.380  2005/03/18 23:22:16  gshi
 * add a parameter (int flag) to msgfromIPC()
 * flag can have the following bit set
 * if (flag & MSG_NEEDAUTH): authentication is required for the message
 * if (flag & MSG_ALLOWINTR): if there is interruption which causes recv() to return
 * 			   return NULL.
 *
 * most of time, it is called with flag = 0
 *
 * Revision 1.379  2005/03/17 05:37:23  gshi
 * fixed a bug:
 *
 * if there are multiple messages missing before a client status message,
 * heartbeat will enter an infinite loop
 *
 * Revision 1.378  2005/03/17 05:32:45  alan
 * Put in a change to make heartbeat restart whenever a split brain heals
 * not just if we manage resources.
 * This should make the split brain test succeed, and it should work around
 * a join problem in the CCM.
 *
 * And, yes, its a kludge.
 *
 * Revision 1.377  2005/03/16 17:11:15  lars
 * Janitorial work: Stray \n removal.
 *
 * Revision 1.376  2005/03/15 18:40:06  gshi
 * change should_send_blocking to should_send_block 'cause it's a better name
 *
 * Revision 1.375  2005/03/15 18:06:09  gshi
 * change the logfile/debugfile ownership to HA_CCMUSER(hacluster) in the initialize_heartbeat()
 *
 * Revision 1.374  2005/03/14 22:57:17  gshi
 * typo
 *
 * Revision 1.373  2005/03/08 23:54:01  alan
 * Modified the maximum CPU percentage to 75% for the main
 * process...
 *
 * This still worries me, but if the problem doesn't recur, then it
 * must have been an OK change to make ;-)
 *
 * Revision 1.372  2005/03/04 15:34:59  alan
 * Fixed various signed/unsigned errors...
 *
 * Revision 1.371  2005/03/04 04:44:11  zhenh
 * make heartbeat can simulate the broken of more than one nodes, for the split-brain cts test
 *
 * Revision 1.370  2005/03/03 19:32:48  gshi
 * A T_REXMIT is a message with NOSEQ_PREFIX.
 * It should be not be processed unless it is broadcast or
 * addressed to this node.
 *
 * Added some debug infomation
 *
 * Revision 1.369  2005/02/23 21:03:53  gshi
 * fixed a bug that when all nodes are in INITSTATUS,
 * hist->lowest_acknode is set to wrong pointer
 *
 * Revision 1.368  2005/02/21 09:48:38  alan
 * Moved the code to enable processing of T_SHUTDONE messages until after reading
 * the config file.
 *
 * Revision 1.367  2005/02/21 07:14:15  alan
 * Fixed a bug where a process which wasn't running would cause an infinite loop.
 *
 * Revision 1.366  2005/02/21 05:30:25  alan
 * Made heartbeat pass BasicSanityCheck again...
 * Now on to better tests :-)
 *
 * Revision 1.365  2005/02/21 02:24:09  alan
 * More diddles in new client shutdown code...
 *
 * Revision 1.364  2005/02/21 01:16:16  alan
 * Changed the heartbeat code for shutting down clients.
 * We no longer remove them from the list, instead we maintain a pointer
 * to the last client not yet shut down, and update that pointer
 * without changing anything.
 *
 * Revision 1.363  2005/02/20 08:00:06  alan
 * Changed a g_assert into a different test, a different printout
 * and a raw abort instead of the assert...
 *
 * Revision 1.362  2005/02/20 07:25:16  alan
 * Fixed a condition in a debug printout so it's right :-)
 *
 * Revision 1.361  2005/02/20 03:11:40  alan
 * Suppressed BEAM warning for weird bitfield/enum.
 *
 * Revision 1.360  2005/02/19 16:13:27  alan
 * Added a tiny bit more debug.
 *
 * Revision 1.359  2005/02/18 22:02:41  alan
 * Added more debug output for this weird client not being killed bug...
 *
 * Revision 1.358  2005/02/18 20:30:27  alan
 * Put in an assert and some more debugging for this annoying shutdown problem...
 *
 * Revision 1.357  2005/02/18 18:26:25  alan
 * More debugging for client child killing.
 * Weird...
 *
 * Revision 1.356  2005/02/18 16:45:59  alan
 * Cleaned up and made more detailed some of the death-of-process logging.
 * No real code changes intended...
 *
 * Revision 1.355  2005/02/17 19:21:34  gshi
 * BEAM FIX: add surrounding braces
 *
 * Revision 1.354  2005/02/17 17:24:26  alan
 * Changed some code to use STRNCMP_CONST to get rid of some BEAM complaints...
 *
 * Revision 1.353  2005/02/14 21:06:11  gshi
 * BEAM fix:
 *
 * replacing the binary usage in core code with uuid function
 *
 * Revision 1.352  2005/02/14 07:40:34  horms
 * On Sparc Linux O_NONBLOCK!=O_NDELAY and this is needed to avoid a loop.
 *
 * Revision 1.351  2005/02/12 17:15:16  alan
 * Forced code to use cl_malloc() for glib use.
 *
 * Revision 1.350  2005/02/08 08:10:27  gshi
 * change the way stringlen and netstringlen is computed.
 *
 * Now it is computed resursively in child messages in get_stringlen() and get_netstringlen()
 * so it allows changing child messages dynamically.
 *
 * Revision 1.349  2005/02/07 21:32:38  gshi
 * move the free from the calling function in wirefmt2ipcmsg() to the caller
 *
 * Revision 1.348  2005/02/07 18:14:49  gshi
 * fix for shutdown
 *
 * sometimes an "Emertency Shutdown" error message may appear in normal shutdown
 * This is due to a child (ccm) died in the shutdown process but the master process
 * has not enter shutdown state machine yet. This will result in calling hb_mcp_final_shutdown()
 * too many times.
 *
 * Revision 1.347  2005/01/28 00:19:52  gshi
 * fixed a bug: a node should only process ACK message with dest to it
 * not all of them.
 *
 * Revision 1.346  2005/01/27 19:36:02  alan
 * Fixed various minor compile problems.
 *
 * Revision 1.345  2005/01/27 17:23:43  alan
 * Fixed some debug message formats
 * Changed the code to not close everything when starting a STONITH child process
 * Moved our initial close calls to the very first part of main().
 *
 * Revision 1.344  2005/01/20 19:17:49  gshi
 * added flow control, if congestion happens, clients will be paused while heartbeat messages can still go through
 * congestion is denfined as (last_send_out_seq_number - last_ack_seq_number) is greater than half of message queue.
 *
 * Revision 1.343  2005/01/18 20:33:03  andrew
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
 * Revision 1.342  2004/12/09 23:12:39  gshi
 * change variable name in channel struct from is_send_blocking
 * to should_send_blocking since this variable determine
 * if the send function *should* block or not in case of full queue
 *
 * Revision 1.341  2004/12/06 21:02:44  gshi
 * in client_status() call
 *
 * only the querying client in the querying node should receive
 * the client status message. Clients in other nodes, or other clients in
 * local node should not be bothered.
 *
 * Revision 1.340  2004/12/04 01:21:11  gshi
 * Fixed a bug that a client may not get client status callback if it connects to heartbeat
 * immediately after heartbeat starts.
 * The fix is that heartbeat disallows a client to connect to it if heartbeat is not ready yet
 *
 * Revision 1.339  2004/12/04 00:47:45  gshi
 * fixed an infinite loop in heartbeat when IPC pipe is full: a channel
 * to communicate with a client in heartbeat should not be blocking
 *
 * Revision 1.338  2004/11/22 20:06:41  gshi
 * new IPC message should be memset-ed to 0
 * to avoid errors caused by adding a new field (void*) msg_buf
 *
 * Revision 1.337  2004/11/18 00:34:37  gshi
 * 1. use one system call send() instead of two for each message in IPC.
 * 2. fixed a bug: heartbeat could crash if IPC pipe beween heartbeat and a client
 * is full.
 *
 * Revision 1.336  2004/11/16 05:58:00  zhenh
 * 1.Make the ordering shutdown work. 2.Move HBDoMsg_T_SHUTDONE() to hb_resource.c
 *
 * Revision 1.335  2004/11/08 20:48:36  gshi
 * implemented logging daemon
 *
 * The logging daemon is to double-buffer log messages to protect us from blocking
 * writes to syslog / logfiles.
 *
 * Revision 1.334  2004/11/02 20:47:49  gshi
 * the patch ensures the following:
 * 1. in heartbeat, it will not deliver client join/leave messages
 * (with sequence number seq_0) until all messages with seq < seq_0
 * is received and delivered, thus receiving a join/leave message has
 * the guarantee that all message before that is received in client side.
 * This applies to all, include ordered message clients and non-ordered
 * message clients.
 *
 * 2. in client, it guarantees no ordered messages loss from peer. It will not
 * deliver a restarted client's ordered message until it received the previous
 * client's leave message. Combining the  the guarantee from 1) and the queue
 * system in client side, messages delivery and order are guaranteed in
 * client restart case.
 *
 * Revision 1.333  2004/10/25 06:15:43  zhenh
 * move the code of adjust deadtime to right place to avoid error logs
 *
 * Revision 1.332  2004/10/24 14:47:31  lge
 * -pedantic-errors fixes 4:
 *  * Warning: static declaration for `verbose' follows non-static
 *    warning: overflow in implicit constant conversion
 *    Warning: unsigned int format, int arg (arg #)
 *   only casted, not "fixed":
 *    warning: long unsigned int format, long int arg (arg #)
 *    would include changing all deadtime_ms and similar to unsigned long.
 *    needs to be discussed first.
 *    offending idiom is:
 *    long l;
 *    sscanf(buf,"%lx",&l);
 *
 * Revision 1.331  2004/10/22 14:23:09  alan
 * Added comments explaining the shutdown phases.
 *
 * Revision 1.330  2004/10/20 19:33:13  gshi
 * reverse the previous wrong commit
 * sorry
 *
 * Revision 1.328  2004/10/19 09:47:47  zhenh
 * make the deadtime tunable
 *
 * Revision 1.327  2004/10/16 04:12:56  alan
 * Added core dump directories, and a bunch of code to cd into the
 * right core dump directory, and activated that code in several
 * different applications.  Note that I didn't do them all -- in particular
 * the SAF/AIS applications haven't been touched yet.
 *
 * Revision 1.326  2004/10/08 21:31:48  alan
 * BEAM fix:  potentially tried to free a NULL pointer.
 *
 * Revision 1.325  2004/10/08 18:37:06  alan
 * Put in two things:
 * 	Got rid of old SUSEisms in the install process
 *
 * 	Added code to shut down respawn clients in reverse order.
 *
 * Revision 1.324  2004/10/06 16:38:15  alan
 * Put in a fix to exit if we're out of memory.
 *
 * Revision 1.323  2004/10/04 16:28:58  lge
 * fix for heartbeat & sleep 1; heartbeat -k
 *  Shutdown delayed until Communication is up.
 *
 * Revision 1.322  2004/10/01 13:10:33  lge
 * micro fixes
 *  initialize logfacility = -1 in hb_cluster_new()
 *  fix off by one error in media_idx range check in read_child_dispatch()
 *  add commented out "not running" branch to we_own_resource() in ResourceManager.in
 *
 * Revision 1.321  2004/09/18 23:13:37  alan
 * Brought forward changes from 1.2 CVS - and added portability macros
 * for STRLEN_CONST and STRNCMP_CONST
 *
 * Revision 1.291.2.15  2004/09/17 05:39:55  alan
 * Fixed a bug where we printed out the "trying to register" message once per
 * minute, when we ought to be printing it once per hour instead.
 * Also, changed a few error messages slightly.
 *
 * we can't set our interval correctly, and we'll reregister and try again.
 *
 * Revision 1.291.2.12  2004/09/15 14:52:05  alan
 * Minor tweaks in the apphbd (re-)registration code in heartbeat.
 * Not tested yet.  I'll do that in a few minutes ;-)
 *
 * Revision 1.291.2.11  2004/09/11 20:52:33  alan
 * Brought back changes from HEAD
 * 1) Added apphbd option
 * 2) Restructuring of packet handling
 * 3) Various BEAM fixes
 *
 * Revision 1.291.2.10  2004/09/11 06:36:32  msoffen
 * Moved variable  definitions to top of functions/blocks, changed comments to c style comments.
 *
 * Revision 1.291.2.9  2004/09/07 14:55:35  alan
 * Increased the size of a set of retransmissions that are sent all at once
 * to 50.  At high heartbeat rates a short glitch can lose more packets at once
 * than originally anticipated when the code was written.
 * Changed code to retransmit missing packets in the correct original order, not
 * in reverse order as it had done before.  One of those ideas that seemed
 * good at the time.. (?!).
 *
 * 11 September 2004: Merged some log info from stable and HEAD (AlanR)
 * Revision 1.320  2004/09/14 15:07:28  gshi
 * change glib API to glib2 API
 *
 * Revision 1.319  2004/09/10 22:47:40  alan
 * BEAM FIXES:  various minor fixes related to running out of memory.
 *
 * Revision 1.318  2004/09/10 01:12:23  alan
 * BEAM CHANGES: Fixed a couple of very minor bugs, and cleaned up some BEAM warnings.
 *
 * Revision 1.317  2004/09/07 16:07:53  alan
 * Added hb_setup_child() to centralize child setup overhead.  Pulled from STABLE branch.
 *
 * Revision 1.316  2004/09/07 14:51:12  alan
 * Increased the size of a set of retransmissions that are sent all at once
 * to 50.  At high heartbeat rates a short glitch can lose more packets at once
 * than originally anticipated when the code was written.
 * Changed code to retransmit missing packets in the original order, not
 * in reverse order as it had done before... (one of those ideas that
 * seemed like a good idea for some reason at the time.  Sigh...)
 *
 * Revision 1.315  2004/09/05 05:05:29  alan
 * Upped the CPU limit for write children -- running 10ms heartbeat interval
 *
 * Revision 1.314  2004/09/05 02:34:56  alan
 * HEAD change to up % of CPU heartbeat main process is allowed to use to 30%
 * I've been testing with 10ms heartbeat intervals...
 *
 * Revision 1.313  2004/09/03 21:03:01  gshi
 *  fixed some warnings in ia64 machines
 *
 * Revision 1.312  2004/08/29 04:33:41  msoffen
 * Fixed comments to properly compile
 *
 * Revision 1.311  2004/08/29 03:01:12  msoffen
 * Replaced all // COMMENTs with / * COMMENT * /
 *
 * Revision 1.310  2004/08/26 00:52:42  gshi
 * fixed a bug which will make ping node unrecognizable
 * in re-reading config when heartbeat receiving a SIGHUP signal
 *
 * the bug is introduced by uuid code
 *
 * Revision 1.309  2004/08/14 14:42:23  alan
 * Put in lots of comments about resource-work tieins in heartbeat.c, plus put in
 * a short term workaround for dealing with one particular case:
 * req_our_resources().
 *
 * Revision 1.308  2004/08/10 04:55:24  alan
 * Completed first pass of -M flag reorganization.
 * It passes BasicSanityCheck.
 * But, I haven't actually tried -M yet ;-)
 *
 * Revision 1.307  2004/08/09 05:05:39  alan
 * Added code to allow heartbeat packet processing by callback functions,
 * and moved a couple of (non-resource) functions over to use this new
 * callback processing.  Looks like it's working.  Still need to move
 * over the resource packet handling over.  This is groundwork for making
 * the '-M' flag work correctly.
 *
 * Revision 1.306  2004/07/26 12:39:46  andrew
 * Change the type of some int's to size_t to stop OSX complaining
 *
 * Revision 1.305  2004/07/07 19:07:14  gshi
 * implemented uuid as nodeid
 *
 * Revision 1.304  2004/05/24 09:18:49  sunjd
 * make heartbeat an apphbd client
 *
 * Revision 1.303  2004/05/17 15:12:08  lars
 * Reverting over-eager approach to disabling old resource manager code.
 *
 * Revision 1.302  2004/05/15 09:28:08  andrew
 * Disable ALL legacy resource management iff configured with --enable-crm
 * Possibly I have been a little over-zealous but likely the feature(s)
 *  would need to be re-written to use the new design anyway.
 *
 * Revision 1.301  2004/04/15 16:25:01  alan
 * Increased the allowable CPU percentage for write child processes to 20% - because pings can be expensive.
 *
 * Revision 1.300  2004/04/10 16:33:54  alan
 * Fixed a bug in setting watchdog timer timeouts.
 * I thought the units for these intervals were ticks, but they were seconds
 * instead.  OOPS!
 * But, now it shuts down within a second of when it should.
 *
 * Revision 1.299  2004/03/26 07:50:02  chuyee
 *
 * Add checking heartbeat client status APIs:
 *
 * 	client_status()
 * 	cstatus_callback()
 *
 * Revision 1.298  2004/03/25 12:27:03  lars
 * Be case-insensitive when looking up a node in our tables too.
 *
 * Revision 1.297  2004/03/25 10:17:28  lars
 * Part I: Lower-case hostnames whereever they are coming in. STONITH
 * module audit to follow.
 *
 * Revision 1.296  2004/03/25 07:55:40  alan
 * Moved heartbeat libraries to the lib directory.
 *
 * Revision 1.295  2004/03/17 18:04:05  msoffen
 * Changes to make the FIFO work on Solaris
 *
 * Revision 1.294  2004/03/05 17:25:19  alan
 * cleanup of netstring patch.
 * Hopefully it also cleaned up the size_t vs int problems in the code.
 *
 * Revision 1.293  2004/03/03 05:31:50  alan
 * Put in Gochun Shi's new netstrings on-the-wire data format code.
 * this allows sending binary data, among many other things!
 *
 * Revision 1.292  2004/02/17 22:11:57  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.291  2004/02/17 21:10:30  alan
 * I'm removing this patch:
 *   When a node shuts down gracefully, we now mark it dead instead of
 *   silently taking over its resources.  That way a very quick restart won't
 *   confuse us.
 * Because it causes heartbeat to declare one side as having a split brain
 * when subsequent packets come in from the shutting-down machine.
 * This causes a restart.
 *
 * Revision 1.290  2004/02/14 15:48:34  alan
 * When a node shuts down gracefully, we now mark it dead instead of
 * silently taking over its resources.  That way a very quick restart won't
 * confuse us.
 *
 * Revision 1.289  2004/02/08 09:23:02  alan
 * Put in a little more data from the "attempted replay attack" message.
 *
 * Revision 1.288  2004/02/06 07:18:15  horms
 * Fixed duplicated global definitions
 *
 * Revision 1.287  2004/01/30 15:09:35  lars
 * Fix prototype too.
 *
 * Revision 1.286  2004/01/30 15:08:43  lars
 * Remove a shadow variable.
 *
 * Revision 1.285  2004/01/22 01:52:31  alan
 * Made a test error message a little more explicit.
 *
 * Revision 1.284  2004/01/21 11:34:14  horms
 * - Replaced numerous malloc + strcpy/strncpy invocations with strdup
 *   * This usually makes the code a bit cleaner
 *   * Also is easier not to make code with potential buffer over-runs
 * - Added STRDUP to pils modules
 * - Removed some spurious MALLOC and FREE redefinitions
 *   _that could never be used_
 * - Make sure the return value of strdup is honoured in error conditions
 *
 * Revision 1.283  2004/01/21 00:54:30  horms
 * Added ha_strdup, so strdup allocations are audited
 *
 * Revision 1.282  2004/01/08 08:38:01  horms
 * Post API clean up of API Register fifo which it is a unix socket now - Alan, hb_api.py still needs to be updated
 *
 * Revision 1.281  2003/12/08 20:55:00  alan
 * Fixed a bug reported by John Leach <john@johnleach.co.uk> where heartbeat
 * sometimes fails to close the watchdog device before execing itself.
 *
 * Revision 1.280  2003/11/20 03:13:55  alan
 * Fixed a bug where we always waited forever for client messages once
 * we got the first one.
 *
 * Added real authentication code to the API infrastructure.
 *
 * Added lots of debugging messages.
 *
 * Changed the IPC code to authenticate based on int values, not on int *'s, since the
 * latter had no advantage and required malloc/freeing storage - which mostly wasn't
 * being done.
 *
 * Revision 1.279  2003/10/29 04:05:01  alan
 * Changed things so that the API uses IPC instead of FIFOs.
 * This isn't 100% done - python API code needs updating, and need to check authorization
 * for the ability to "sniff" other people's packets.
 *
 * Revision 1.278  2003/10/27 10:42:52  horms
 *
 * Ensure that init_config() and parse_ha_resources() are
 * only called once on restart. Else all sorts of strange
 * things can happen. Kurosawa Takahiro.
 *
 * Revision 1.277  2003/09/26 05:48:19  alan
 * Fixed a few undefined variable complaints.
 *
 * Revision 1.276  2003/09/26 04:34:13  alan
 * Fine tuning the auditing code so it doesn't bitch inappropriately.
 *
 * Revision 1.275  2003/09/19 19:21:14  alan
 * Fixed the bug where we ran resource scripts twice.
 * The fix consisted of causing the resource requests to be queued, so that they aren't run simultaneously.
 *
 * Revision 1.274  2003/08/06 13:48:46  horms
 * Allow respawn programmes to have arguments. Diarmuid O'Neill + Horms
 *
 * Revision 1.273  2003/07/22 09:51:35  alan
 * Patch to fix problem noted by "Ing. Jozef Sakalos" <jsakalos@ba.success.sk>
 * with comparisons between signed an unsigned ints.
 *
 * Revision 1.272  2003/07/14 04:10:31  alan
 * Changed the "clock just jumped" code to do nothing nowadays.
 * It's not really needed, but it's good information because it can mess up
 * other systems that aren't so tolerant of this kind of nonsense.
 *
 * Revision 1.271  2003/07/13 12:43:30  alan
 * Changed the recovery code to shut down *un*gracefully if resource
 * takeover doesn't finish when it should.  This should trigger
 * a STONITH.
 *
 * Revision 1.270  2003/07/03 23:27:19  alan
 * Moved #defines for parameter names to a public header file.
 * Added the ability to ask for the heartbeat version info through the API.
 *
 * Revision 1.269  2003/07/03 21:49:33  alan
 * Added code to allow us to use our own poll substitute for everything...
 *
 * Revision 1.268  2003/07/01 10:12:26  horms
 * Use defines for node types rather than arbitary strings
 *
 * Revision 1.267  2003/07/01 02:36:22  alan
 * Several somewhat-related things in this change set:
 * Added new API call to get general parameters.
 * Added new API code to test this new call.
 * Added new ability to name a node something other than the uname -n name.
 *
 * Revision 1.266  2003/06/28 04:47:51  alan
 * Fixed some terrible, horrible, no good very bad reload bugs -- especially
 * with nice_failback turned on.  Yuck!
 * Also fixed a STONITH bug.  The previous code wouldn't STONTIH a node
 * we hadn't heard from yet -- but we really need to.
 * Decreased debugging verbosity a bit...
 *
 * Revision 1.265  2003/06/24 06:43:53  alan
 * Removed superfluous include of <ha_config.h>
 *
 * Revision 1.264  2003/06/04 15:51:35  alan
 * removed a duplicate debug log message...
 *
 * Revision 1.263  2003/05/23 14:55:51  alan
 * Changed the "probable replay attack" message from a debug to an error message.
 *
 * Revision 1.262  2003/05/22 23:13:26  alan
 * Changed the code to fix a bug in resource auditing code.
 * We now indicate if an update to the resource set is incremental or full.
 *
 * Revision 1.261  2003/05/22 05:10:15  alan
 * Fixed some comments in heartbeat.c
 *
 * Revision 1.260  2003/05/19 20:37:00  alan
 * Turned the code back on for dropping privileges for child processes.
 *
 * Revision 1.259  2003/05/09 15:41:42  alan
 * Put in a patch to make OpenBSD compile.  Apparently, it doesn't know
 * that NULL is a pointer...
 *
 * Revision 1.258  2003/05/09 15:15:37  alan
 * Turned off the most expensive and onerous debugging code.
 *
 * Revision 1.257  2003/05/05 11:46:02  alan
 * Added code to limit our CPU usage when we're running with debugging on.
 * Changed the code to use separate IPC_Channels for read and write children.
 * 	This latter was to fix a reasonably nasty bug...
 *
 * Revision 1.256  2003/04/30 22:28:09  alan
 * Changed heartbeat.c to use cl_log instead of ha_log.
 * Fixed up various formatting things.
 * Changed heartbeat to use the per-node timeout timer.
 *
 * Revision 1.255  2003/04/23 01:32:27  horms
 * Fixed indentation
 *
 * Revision 1.254  2003/04/23 01:31:16  horms
 * Return early to avoid excessive indenting
 *
 * Revision 1.253  2003/04/18 07:48:28  alan
 * Fixed the string length of a message to be retransmitted.
 *
 * Revision 1.252  2003/04/18 07:39:25  alan
 * Fixed an 'oops' from previous change where I wrote messages to write processes without a terminating 0 byte.
 *
 * Revision 1.251  2003/04/18 06:09:46  alan
 * Fixed an off-by-one error in writing messages to the FIFO.
 * Also got rid of some now-unused functions, and fixed a minor glitch in BasicSanitCheck.
 *
 * Revision 1.250  2003/04/16 13:07:59  alan
 * Dropped the read_child_dispatch() NULL link message because it can occur
 * "normally" when we get garbled packets (particularly from serial ports).
 *
 * Revision 1.249  2003/04/15 23:06:53  alan
 * Lots of new code to support the semi-massive process restructuriing.
 *
 * Revision 1.247  2003/03/29 02:48:44  alan
 * More small changes on the road to restructuring heartbeat processees.
 *
 * Revision 1.246  2003/03/28 16:49:43  alan
 * Removed a memory leak introduced by the previous change.
 *
 * Revision 1.245  2003/03/28 16:09:03  lars
 * Restructured loop a bit to stop my head from spinning.
 *
 * Revision 1.244  2003/03/27 07:04:26  alan
 * 1st step in heartbeat process restructuring.
 * Create fifo_child() processes to read the FIFO written by the shell scripts.
 *
 * Revision 1.243  2003/03/24 08:17:04  horms
 * merged in changes from stable branch
 *
 * Revision 1.241.2.1  2003/03/12 18:24:50  lars
 * Syncing 1.0.x series with CVS head in preparation for 1.0.2 release.
 *
 * Revision 1.242  2003/03/07 01:13:05  alan
 * Put in code for a time-based generation number option.
 *
 * Revision 1.241  2003/02/07 08:37:16  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.240  2003/02/05 09:06:33  horms
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
 * Revision 1.239  2003/02/05 06:46:19  alan
 * Added the rtprio config option to the ha.cf file.
 *
 * Revision 1.238  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.237  2003/01/17 08:31:52  msoffen
 * Updated to match current procftpd (in an attempt to get
 * setproctitle working on Solaris).
 *
 * Revision 1.236  2003/01/16 00:49:46  msoffen
 * Created static variable instead of "run time" allocation for config variable
 * becuase on Solaris the variable wasn't being created with proper memory
 * alignment.
 *
 * Revision 1.235  2003/01/08 21:17:39  msoffen
 * Made changes to allow compiling with -Wtraditional to work.
 *
 * Revision 1.234  2002/11/29 19:18:34  alan
 * Put in pointers to the documentation for deadtime in the returning
 * from cluster partition message.
 *
 * Revision 1.233  2002/11/28 17:10:05  alan
 * We had a problem with local status updates getting all hosed sometimes
 * (depending on timing).  This greatly simplifies the management of
 * local status, and even takes a field out of the heartbeat packet.
 *
 * A fix like this was suggested by Horms.
 *
 * Revision 1.232  2002/11/26 08:26:31  horms
 * process latent ipc information as neccessary
 *
 * Revision 1.231  2002/11/22 07:04:39  horms
 * make lots of symbols static
 *
 * Revision 1.230  2002/11/09 16:44:04  alan
 * Added supplementary groups to the 'respawn'ed clients.
 *
 * Revision 1.229  2002/10/30 17:15:42  alan
 * Changed shutdown_restart to occur after a little delay when a cluster parition is discovered.
 * Added a little debugging code turned on at the highest levels of debug.
 * Added code to dump the current malloc arena when memory stats come out.
 * Changed it so memory stats come every 10 minutes if debugging is enabled when
 * heartbeat is started.
 *
 * Revision 1.228  2002/10/22 17:41:58  alan
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
 * Revision 1.227  2002/10/21 14:27:37  msoffen
 * Added packet Debug information and more error messages (when unable to open
 * a FIFO properly).
 *
 * Revision 1.226  2002/10/21 10:17:18  horms
 * hb api clients may now be built outside of the heartbeat tree
 *
 * Revision 1.225  2002/10/21 02:00:35  horms
 * Use CL_KILL() instead of kill() throughout the code.
 * This makes the code nice and homogenous and removes
 * the need for spurious inclusion of signal.h
 *
 * Revision 1.224  2002/10/19 16:04:33  alan
 * Moved most of the resource handling code in heartbeat into hb_resource.[ch]
 * Some inline code that's tied to packet reception is still in heartbeat.c,
 * but the vast majority has been moved to these new files.
 *
 * Revision 1.223  2002/10/18 22:46:30  alan
 * Rearranged heartbeat.c so that all the resource-related code is at the bottom fo the
 * file so we can migrate it to a separate file (and header file).
 *
 * Revision 1.222  2002/10/18 07:16:08  alan
 * Put in Horms big patch plus a patch for the apcmastersnmp code where
 * a macro named MIN returned the MAX instead.  The code actually wanted
 * the MAX, so when the #define for MIN was surrounded by a #ifndef, then
 * it no longer worked...  This fix courtesy of Martin Bene.
 * There was also a missing #include needed on older Linux systems.
 *
 * Revision 1.221  2002/10/15 13:41:30  alan
 * Switched heartbeat over to use the GSource library functions.
 * Added the standby capability to the heartbeat init script
 * Changed the proctrack library code to use cl_log() instead of g_log().
 * Removed a few unused header files.
 *
 * Revision 1.220  2002/10/08 13:43:59  alan
 * Put in some more pairs of grab/drop privileges around sending
 * signals.
 *
 * Fixed a bug where heartbeat simply wouldn't start due to a
 * race condition.  The race condition was that the control process
 * might start running before the master status process opened the
 * FIFO the CP was reading from, and the CP would get immediate EOF.
 * This was rare, but it happened.  Probably could happen a lot
 * more on an large-scale SMP.
 *
 * This condition originally caused an immediate "no local heartbeat"
 * condition and looked more like failure to shut down than failure to
 * start up at least in my testing.
 *
 * Revision 1.219  2002/10/08 03:40:37  alan
 * An attempt to fix Matt's problem which appears to be the result of dropping
 * privileges incorrectly.
 *
 * Revision 1.218  2002/10/07 19:43:39  alan
 * Put in a change which should allow us to work correctly on FreeBSD
 * with its weird "you can't change your scheduler unless you're root" behavior
 * (even if you're requesting normal privileges).
 *
 * Revision 1.217  2002/10/07 17:57:40  alan
 * Changed the privilege dropping code a bit.
 *
 * Revision 1.216  2002/10/07 04:39:15  alan
 * Put in code to make shutdown problems easier to debug, and to hopefully keep shutdowns from
 * hanging forever.
 *
 * Revision 1.215  2002/10/05 19:45:10  alan
 * Make apphbd run at high priority and locked into memory.
 * Moved make_realtime() into the clplumbing library
 * Added functions to clplumbing library to run as nobody.
 * Fixed the ping packet dump debug code
 * Memory statistics dumps have been marked more clearly as informational
 * Heartbeat network-facing (read, write) processes now run as nobody.
 * apphbd now also runs as nobody.
 * Fixed a bug in the standby timer code that would keep it from
 * timing out properly.
 * Minor updates for the OCF membership header file as per comments
 * 	from OCF group.
 *
 * Revision 1.214  2002/10/04 14:34:32  alan
 * Closed a security hole pointed out by Nathan Wallwork.
 *
 * Revision 1.213  2002/09/26 09:20:43  horms
 * Fixed file descriptor leak in heartbeat side of heartbeat API.
 * I'm not sure about ignoreing SIGPIPE, but it will do for now.
 *
 * Revision 1.212  2002/09/26 03:28:46  alan
 * Made the 2.53 patch work again.
 * Added code for preallocating a little memory before locking
 * ourselves into memory.
 *
 * Added a new poll routine to be used in place of the system
 * poll routine (for the cases we can do this).
 * Changed apphbd to use it, and added code to allow
 * heartbeat to use it (requires 2.5 kernel).
 *
 * Minor realtime fixes:
 * 	use write instead of fwrite
 * 	Don't open FIFO to client for each msg to them.
 *
 * Fixed a bug in apphbd where it complain and loop when
 * clients disconnected unexpectedly.
 *
 * Added apphbd change to allow clients to specify warn times.
 * Changed "client disconnected without telling us" from
 * an error to a warning
 *
 * Revision 1.211  2002/09/20 02:09:50  alan
 * Switched heartbeat to do everything with longclock_t instead of clock_t.
 * Switched heartbeat to be configured fundamentally from millisecond times.
 * Changed heartbeat to not use alarms for much of anything.
 * These are relatively major changes, but the seem to work fine.
 *
 * Revision 1.210  2002/09/17 18:53:37  alan
 * Put in a fix to keep mach_down from doing anything with ping node information.
 * Also put in a change to make lmb's last portability fix more portable ;-)
 *
 * Revision 1.209  2002/09/17 14:13:24  alan
 * A bit more LSB compliance code.
 *
 * Revision 1.208  2002/09/17 13:41:38  alan
 * Fixed a bug in PILS pointed out by lmb which kept it from working
 * 	when a user specified a STONITH directive in heartbeat.
 * 	This had to do with a static variable which I had to get rid of.
 * 	It was a bit painful.
 * Changed heartbeat main to exit with LSB-compliant exit codes.
 * Put in the fixes for debug signals interfering with other signals.
 * Put in code to make us not try and take over resources from ping
 * 	nodes when they go down (since they don't have any).
 * Put in a realtime fix for client API I/O (removed some test code).
 * Changed api_test to use the new cl_log facility.
 * Eliminated some unused code which was supposed to provide
 * 	application heartbeating.  It couldn't yet be used and was a bad idea.
 *
 * Enabled logging to stderr when heartbeat first starts.
 *
 * Revision 1.207  2002/09/13 04:16:12  alan
 * Put in fixes for warnings that Thomas Hepper ran into.
 *
 * Revision 1.206  2002/09/12 12:36:09  horms
 * * Added PINGSTATUS and used it instead of directly using "ping"
 *   as the status for an active ping node
 * * Used DEADSTATUS everywhere, instead of "dead"
 *
 * Revision 1.205  2002/09/11 13:07:36  alan
 * renamed healed_cluster_partition to be called cause_shutdown_restart,
 * and moved the log message elsewhere.
 * Put in a comment asking if we should call that when we hear no local heartbeat
 * instead of just shutting down.
 *
 * Revision 1.204  2002/09/10 21:50:06  alan
 * Added code, modified code to move to a common set of logging functions
 * - cl_log() and friends.
 *
 * Revision 1.203  2002/09/10 04:35:58  alan
 * Removed references to PAGE_SIZE to avoid conflicts in OpenBSD.
 *
 * Revision 1.202  2002/09/05 12:48:27  alan
 * Fixed a bug in exit codes for failure of the generic plugin loader noted by Nathan Wallwork.
 *
 * Revision 1.201  2002/08/27 17:17:46  alan
 * Put in code which hopefully fixes a restart problem which had been
 * seen on Solaris, and might also affect Linux.
 * It has to do with some fprintfs to stderr which it does when it first
 * starts up.
 *
 * Revision 1.200  2002/08/20 19:44:45  alan
 * Put in a Solaris patch from Thomas Hepper <th@ant.han.de>
 * He was having trouble with wait3 calls being interrupted, so processes
 * deaths got missed.  Maybe I had this problem before, and worked around
 * it by waiting in a loop on exit...
 *
 * Revision 1.199  2002/08/14 21:38:05  alan
 * Capabilities for plugins for apphbd, also for watchdog support.
 *
 * Put in a "fixed" version of base64.c with different logging code.
 *
 * Changed the API for the IPC code slightly.
 *
 * Revision 1.198  2002/08/10 02:21:11  alan
 * Moved the SIGIGNORE stuff to the common portability.h file
 * Added a OpenBSD fix - we used to open a FIFO as O_RDWR.  Now we open
 * it twice instead.
 *
 * Revision 1.197  2002/08/09 15:11:20  msoffen
 * Same change as apphb/apphbd.c (for the sigignore fix).
 *
 * Revision 1.196  2002/08/07 18:20:33  msoffen
 * Cleaned up many warning messages from FreeBSD and Solaris.
 * Cleaned up signal calls with SIG_IGN for Solaris to use sigignore function (to remove some warnings).
 *
 * Revision 1.195  2002/08/02 22:47:49  alan
 * Fixed a minor and obscure protocol bug.
 * When we got a rexmit packet just as we start up, it might be from
 * our previous incarnation, and we can't rexmit it, because we don't have that
 * packet.  Before we NAKed it, now we just ignore it, because they'll
 * see that we restarted very soon...
 *
 * Revision 1.194  2002/07/30 01:30:13  alan
 * The process tracking code had incorrect/invalid
 ig logging levels specified for process tracking.  Depending
 * on the values of certain system headers it could cause heartbeat
 * to exit.  It did so quite repeatedly for STONITH processes.
 *
 * Revision 1.193  2002/07/26 22:58:12  alan
 * Changed the code to write a 'v' to /dev/watchdog before
 * shutting down, so it will know we mean to shut down.
 *
 * This patch due to Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 * Revision 1.192  2002/07/16 11:47:53  lars
 * Type and alignment fixes for IA64, x86_64, sparc, s390 and PPC(64).
 *
 * Revision 1.191  2002/07/08 04:14:12  alan
 * Updated comments in the front of various files.
 * Removed Matt's Solaris fix (which seems to be illegal on Linux).
 *
 * Revision 1.190  2002/06/21 14:52:51  alan
 * Put in a fix which should cause configuration errors to go to stderr in
 * addition to whereever else they might be headed ;-)
 *
 * Revision 1.189  2002/06/06 06:10:03  alan
 * Fixed some problems running with newer versions of libtool and automake
 * Added first draft of code for apphbd daemon
 * Added beginnings of application heartbeating for heartbeat clients.
 *
 * Revision 1.188  2002/04/29 07:21:57  alan
 * Small code reformatting...
 *
 * Revision 1.187  2002/04/26 21:49:45  alan
 * Put in some debug code so that the plugins we load get
 * the right level of debug turned on in them when we start.
 *
 * Revision 1.186  2002/04/20 05:36:22  alan
 * Added a little debug for debugging Matt Soffen's authentication
 * problems.
 *
 * Revision 1.185  2002/04/19 21:32:20  alan
 * Changed setpgrp to setpgid(0,0)
 *
 * Revision 1.184  2002/04/15 16:48:13  alan
 * Fixed a bug in the handling of shutdowns where if both machines were shutting down
 * simultaneously, one or both would take over the others resources, even
 * though they were shutting down.  This showed up with the cluster partitioning tests.
 *
 * Fixed a bug in the -r restart code, where it didn't follow through
 * and finish the shutdown at all.  The necessary SIGQUIT wasn't sent, and
 * some of the processes weren't killed.
 *
 * Revision 1.183  2002/04/14 09:20:53  alan
 * Changed reaper_action() to have a waitflags argument, and now
 * we wait for children to die rather than fooling with signals
 * etc.
 *
 * Revision 1.182  2002/04/14 09:06:09  alan
 * Made yet another attempt to get all our SIGCHLDs.
 *
 * Revision 1.181  2002/04/14 00:39:30  alan
 * Put in a comment about "strings" needing to run in a separate
 * process...
 *
 * Revision 1.180  2002/04/13 22:45:37  alan
 * Changed a little of the code in heartbeat.c to use the new longclock_t
 * type and functions.  It ought to completely replace the use of
 * times() *everywhere*
 *
 * Reorganized some of the code for handling nice_failback to not all
 * be in the process_resources() function...
 *
 * Moved all the code which is triggered when our links first come up to
 * a single function, instead of scattered about in several different
 * places.
 *
 * Moved the code to take over local resources out of the process_clustermsg()
 * function into the poll loop code.  This eliminates calling the
 * process_resources() function for every packet.
 *
 * Moved the resource auditing code out into a separate function so
 * I could call it in more than one place.
 *
 * Moved all the resource handling code in the process_clustermsg() function
 * to be together, so it's more readable.
 *
 * Revision 1.179  2002/04/13 03:46:52  alan
 * more signal diddles...
 *
 * Revision 1.178  2002/04/13 02:07:14  alan
 * Made some changes to the way signals are handled so we don't lose
 * SIGCHLDs when shutting down.
 *
 * Revision 1.177  2002/04/12 19:36:14  alan
 * Previous changes broke nice_failback.
 * There was some code which was sent out the STARTING messages
 * which had been called because it was before some code which
 * bypassed protocol processing.  This code is now at the
 * end of the loop.
 *
 * Revision 1.176  2002/04/12 15:14:28  alan
 * Changed the processing of resource requests so we eliminate some
 * timing holes.
 *
 * First, we ignore ip-request-resp messages during shutdowns, so that we
 * don't acquire any new resources while we're shutting down :-)
 *
 * Secondly, we needed to queue ip-requests and don't answer them right
 * away if we're running any resource acquisition code ourselves.
 * This is because if we've just started a resource takeover ourselves,
 * and someone asks to have it back, we'll answer that they can have it
 * without releasing it ourselves because we don't realize that
 * we're acquiring it, because we don't have it quite yet.
 * By delaying until all resource acquisition/release processes
 * are complete, we can give an accurate answer to this request.
 *
 * Two things caused these bug to appear:
 * We now always answer any ip-request (if we're managing resources at all),
 * and we keep our links up for a little while longer than we used to
 * while we're shutting down.  So, the windows for these two behaviors have
 * been opened up a little wider - though I suspect they've both been
 * possible before.  Other changes made takeovers run faster, so the
 * combination was effective in making the bug apparent ;-)
 *
 * Solving the first was easy - we just filter out ip-request-resp
 * messages when shutting down.  The second one required that a queue be added
 * for handling incoming resource acquisition messages.  To implement this
 * queue we used Glib GHook-s, which are good things for recording a function
 * and a pointer to data in, and later running them.
 *
 * GHooks are a handy kludge to have around...
 *
 * Revision 1.175  2002/04/11 18:33:54  alan
 * Takeover/failover is much faster and a little safer than it was before...
 *
 * For the normal failback case
 * 	If the other machine is down, resources are taken immediately
 *
 * 	If the other machine is up, resources are requested and taken over
 * 		when they have been released.  If the other machine
 * 		never releases them, they are never taken over.
 * 		No background process is ever spawned to "eventually" take
 * 		them over.
 *
 * For the nice failback case
 * 	All resources are acquired *immediately* after the other machine is
 * 		declared dead.
 *
 * Changed the rules about initial deadtime:
 *
 * It now only insists the time be equal to deadtime.
 *
 * It gives a warning if its less than 10 seconds.
 *
 * If not specified, here is how it defaults...
 * 	If deadtime is less than or equal to 10 seconds, then it defaults it to be
 * 	twice the deadtime.
 *
 * 	If deadtime is greater than 10 seconds, then it defaults it to be
 * 	the same as deadtime.
 *
 * Revision 1.174  2002/04/10 21:05:33  alan
 * Put in some changes to control_process() to hopefully make it
 * exit completely reliably.
 * After 300 iterations, I saw a case where it hung in the read for control
 * packets, and didn't respond to signals, but all its children were dead.
 * I now close the FIFO, so that all reads will fail with EOF, and then
 * changed the read loop to drop out when it got EOF.
 * I added a  loop afterwards which consists of a pause and poll for signals
 * until all its children died.
 *
 * Revision 1.173  2002/04/10 07:41:14  alan
 * Enhanced the process tracking code, and used the enhancements ;-)
 * Made a number of minor fixes to make the tests come out better.
 * Put in a retry for writing to one of our FIFOs to allow for
 * an interrupted system call...
 * If a timeout came just as we started our system call, then
 * this could help.  Since it didn't go with a dead process, or
 * other symptoms this could be helpful.
 *
 * Revision 1.172  2002/04/09 21:53:26  alan
 * A large number of minor cleanups related to exit, cleanup, and process
 * management.  It all looks reasonably good.
 * One or two slightly larger changes (but still not major changes) in
 * these same areas.
 * Basically, now we wait for everything to be done before we exit, etc.
 *
 * Revision 1.171  2002/04/09 06:37:27  alan
 * Fixed the STONITH code so it works again ;-)
 *
 * Also tested (and fixed) the case of graceful shutdown nodes not getting
 * STONITHed.  We also don't STONITH nodes which had no resources at
 * the time they left the cluster, at least when nice_failback is set.
 *
 * Revision 1.170  2002/04/07 13:54:06  alan
 * This is a pretty big set of changes ( > 1200 lines in plain diff)
 *
 * The following major bugs have been fixed
 *  - STONITH operations are now a precondition for taking over
 *    resources from a dead machine
 *
 *  - Resource takeover events are now immediately terminated when shutting
 *    down - this keeps resources from being held after shutting down
 *
 *  - heartbeat could sometimes fail to start due to how it handled its
 *    own status through two different channels.  I restructured the handling
 *    of local status so that it's now handled almost exactly like handling
 *    the status of remote machines
 *
 * There is evidence that all these serious bugs have been around a long time,
 * even though they are rarely (if ever) seen.
 *
 * The following minor bugs have been fixed:
 *
 *  - the standby test now retries during transient conditions...
 *
 *  - the STONITH code for the test method "ssh" now uses "at" to schedule
 *    the stonith operation on the other node so it won't hang when using
 *    newer versions of ssh.
 *
 * The following new test was added:
 *  - SimulStart - starting all nodes ~ simultaneously
 *
 * The following significant restructuring of the code occurred:
 *
 *  - Completely rewrote the process management and death-of-child code to
 *    be uniform, and be based on a common semi-object-oriented approach
 *    The new process tracking code is very general, and I consider it to
 *    be part of the plumbing for the OCF.
 *
 *  - Completely rewrote the event handling code to be based on the Glib
 *    mainloop paradigm. The sets of "inputs" to the main loop are:
 *     - "polled" events like signals, and once-per-loop occurrances
 *     - messages from the cluster and users
 *     - API registration requests from potential clients
 *     - API calls from clients
 *
 *
 * The following minor changes were made:
 *
 *  - when nice_failback is taking over resources, since we always negotiate for
 *    taking them over, so we no longer have a timeout waiting for the other
 *    side to reply.  As a result, the timeout for waiting for the other
 *    side is now much longer than it was.
 *
 *  - transient errors for standby operations now print WARN instead of EROR
 *
 *  - The STONITH and standby tests now don't print funky output to the
 *    logs.
 *
 *  - added a new file TESTRESULTS.out for logging "official" test results.
 *
 * Groundwork was laid for the following future changes:
 *  - merging the control and master status processes
 *
 *  - making a few other things not wait for process completion in line
 *
 *  - creating a comprehensive asynchronous action structure
 *
 *  - getting rid of the "interface" kludge currently used for tracking
 *    activity on individual interfaces
 *
 * The following things still need to be tested:
 *
 *  - STONITH testing (including failures)
 *
 *  - clock jumps
 *
 *  - protocol retransmissions
 *
 *  - cross-version compatability of status updates (I added a new field)
 *
 * Revision 1.169  2002/04/04 17:55:27  alan
 * Put in a whole bunch of new code to manage processes much more generally, and powerfully.
 * It fixes two important bugs:  STONITH wasn't waited on before we took over resources.
 * And, we didn't stop our takeover processes before we started to shut down.
 *
 * Revision 1.168  2002/04/02 19:40:36  alan
 * Failover was completely broken because of a typo in the configure.in file
 * Changed the run level priorities so that heartbeat starts after
 * drbd by default.
 * Changed it so that heartbeat by default runs in init level 5 too...
 *
 * Fixed a problem which happened when both nodes started about simultaneously.
 * The result was that hb_standby wouldn't work afterwards.
 *
 * Raised the debug level of some reasonably verbose messages so that you can
 * turn on debug 1 and not be flooded with log messages.
 *
 * Changed the code so that in the case of nice_failback there is no waiting for
 * the other side to give up resources, because we negotiate this in advance.
 * It gets this information through and environment variable.
 *
 * Revision 1.167  2002/03/27 01:59:58  alan
 * Hopefully, fixed a bug where requests to retransmit packets
 * (and other unsequenced protocol packets) get dropped because they don't
 * have sequence numbers.
 *
 * Revision 1.166  2002/03/15 14:26:36  alan
 * Added code to help debug the current missing to/from/ts/,etc. problem...
 *
 * Revision 1.165  2002/02/21 21:43:33  alan
 * Put in a few fixes to make the client API work more reliably.
 * Put in a few changes to the process exit handling code which
 * also cause heartbeat to (attempt to) restart when it finds one of it's
 * own processes dies.  Restarting was already broken :-(
 *
 * Revision 1.164  2002/02/12 18:13:39  alan
 * Did 3 things:
 * 	Changed the API test program to use syslog for some messages.
 * 	Changed the API code to be a little less verbose
 * 	Removed the ns_st file from the rc.d directory (since it does
 * 		nothing and is no longer needed)
 *
 * Revision 1.163  2002/02/12 15:22:29  alan
 * Put in code to filter out rc script execution on every possible message,
 * so that only those scripts that actually exist will we attempt to execute.
 *
 * Revision 1.162  2002/02/11 22:31:34  alan
 * Added a new option ('l') to make heartbeat run at low priority.
 * Added support for a new capability - to start and stop client
 * 	processes together with heartbeat itself.
 *
 * Revision 1.161  2002/02/10 23:09:25  alan
 * Added a little initial code to support starting client
 * programs when we start, and shutting them down when we stop.
 *
 * Revision 1.160  2002/02/09 21:21:42  alan
 * Minor message and indentation changes.
 *
 * Revision 1.159  2002/01/16 22:59:17  alan
 * Fixed a dumb error in restructuring the code.
 * I passed the retransmit history structure by value instead of by address,
 * so there was a HUGE memory leak.
 *
 * Revision 1.158  2001/10/26 11:08:31  alan
 * Changed the code so that SIGINT never interrupts us.
 * Changed the code so that SIGALRM doesn't interrupt certain child
 * processes (particularly in shutdown).  Ditto for shutdown wrt SIGTERM and
 * SIGCHILD.
 *
 * Revision 1.157  2001/10/25 14:34:17  alan
 * Changed the serial code to send a BREAK when one side first starts up their
 * conversation.
 * Configured the receiving code to flush I/O buffers when they receive a break
 * Configured the code to ignore SIGINTs (except for their buffer flush effect)
 * Configured the code to use SIGQUIT instead of SIGINT when communicating that
 * the shutdown resource giveup is complete.
 *
 * This is all to fix a bug which occurs because of leftover out-of-date messages
 * in the serial buffering system.
 *
 * Revision 1.156  2001/10/25 05:06:30  alan
 * A few changes to tighten up the definition of "stability" so we
 * don't complain about things falsely, nor do we prohibit attempting
 * takeovers unnecessarily either.
 *
 * Revision 1.155  2001/10/24 20:46:28  alan
 * A large number of patches.  They are in these categories:
 * 	Fixes from Matt Soffen
 * 	Fixes to test environment things - including changing some ERRORs to
 * 		WARNings and vice versa.
 * 	etc.
 *
 * Revision 1.154  2001/10/24 00:24:44  alan
 * Moving in the direction of being able to get rid of one of our
 * control processes.
 * Today's work: splitting control_process() into control_process() and
 * process_control_packet().
 * The idea is that once the control_process and the master_status_process
 * are merged, that this function can be called from the select already
 * present in master_status_process().
 *
 * Revision 1.153  2001/10/23 05:40:41  alan
 * Put in code to make the management of the audit periods work a little
 * more neatly.
 *
 * Revision 1.152  2001/10/23 04:19:24  alan
 * Put in code so that a "stop" really stops heartbeat (again).
 *
 * Revision 1.151  2001/10/22 05:22:53  alan
 * Fixed the split-brain (cluster partition) problem.
 * Also, fixed lots of space/tab nits in various places in heartbeat.
 *
 * Revision 1.150  2001/10/22 04:02:29  alan
 * Put in a patch to check the arguments to cl_log calls...
 *
 * Revision 1.149  2001/10/13 22:27:15  alan
 * Removed a superfluous signal_all(SIGTERM)
 *
 * Revision 1.148  2001/10/13 09:23:19  alan
 * Fixed a bug in the new standby code.
 * It now waits until resources are fully given up before taking them over.
 * It now also manages the nice_failback state consistency audits correctly.
 * Still need to make it work for the not nice_failback case...
 *
 * Revision 1.147  2001/10/13 00:23:05  alan
 * Put in comments about a serious problem with respect to resource takeover...
 *
 * Revision 1.146  2001/10/12 23:05:21  alan
 * Put in a message about standby only being implemented when nice_failback
 * is on.
 *
 * Revision 1.145  2001/10/12 22:38:06  alan
 * Added Luis' patch for providing the standby capability
 *
 * Revision 1.144  2001/10/12 17:18:37  alan
 * Changed the code to allow for signals happening while signals are being processed.
 * Changed the code to allow us to have finer heartbeat timing resolution.
 *
 * Revision 1.143  2001/10/10 13:18:35  alan
 * Fixed a typo on ClockJustJumped.  Oops!
 *
 * Revision 1.142  2001/10/09 19:22:52  alan
 * Made some minor changes to how we handle clock jumps and timeout other
 * nodes.  I'm not sure why it's necessary, or if it is for that matter.
 * But it shouldn't *hurt* anything either.  This problem reported by Matt Soffen.
 *
 * Revision 1.141  2001/10/04 02:45:06  alan
 * Added comments about the lousy realtime behavior of the old method
 * of sending messages.  Changed the indentation of one line.
 *
 * Revision 1.140  2001/10/03 18:09:51  alan
 * Changed the process titles a little so that the medium type is displayed.
 *
 * Revision 1.139  2001/10/02 20:15:40  alan
 * Debug code, etc. from Matt Soffen...
 *
 * Revision 1.138  2001/10/02 05:12:19  alan
 * Various portability fixes (make warnings go away) for Solaris.
 *
 * Revision 1.137  2001/10/02 04:22:45  alan
 * Fixed a minor bug regarding reporting how late a heartbeat is when there is no previous
 * heartbeat to compare it to.  In that circumstance, it shouldn't report at all.
 * Now, that's what it does too ;-)
 *
 * Revision 1.136  2001/10/01 22:00:54  alan
 * Improved Andreas Piesk's patch for no-stonith after shutdown.
 * Probably fixed a bug about not detecting status changes after a restart.
 * Fixed a few coding standards kind of things.
 *
 * Revision 1.135  2001/10/01 20:24:36  alan
 * Changed the code to not open the FIFO for every message we send to the cluster.
 * This should improve our worst-case (and average) latency.
 *
 * Revision 1.134  2001/09/29 19:08:24  alan
 * Wonderful security and error correction patch from Emily Ratliff
 * 	<ratliff@austin.ibm.com>
 * Fixes code to have strncpy() calls instead of strcpy calls.
 * Also fixes the number of arguments to several functions which were wrong.
 * Many thanks to Emily.
 *
 * Revision 1.133  2001/09/18 14:19:45  horms
 *
 * Signal handlers set flags and actions are executed accordingly
 * as part of event loops. This avoids problems with executing some
 * system calls within signal handlers. In paricular calling exec() from
 * within a signal may result in process with unexpected signal masks.
 *
 * Unset the signal mask for SIGTERM upon intialisation. This is harmless
 * and a good safety measure in case the calling process has masked
 * this signal.
 *
 * Revision 1.132  2001/09/07 05:48:30  horms
 * Changed recently added _handler funciotns to _sig to match previously defined signal handlers. Horms
 *
 * Revision 1.131  2001/09/07 05:46:39  horms
 * Changed recently added _handler funciotns to _sig to match previously defined signal handlers. Horms
 *
 * Revision 1.130  2001/09/07 01:09:06  alan
 * Put in code to make the glib error messages get redirected to whereever
 * the other ha_log messages go...
 *
 * Revision 1.129  2001/09/07 00:07:14  alan
 * Fixed the code for dealing with the test packet dropping facility.
 * It has been broken since I changed the startup order.
 *
 * Revision 1.128  2001/09/06 16:14:35  horms
 * Added code to set proctitle for heartbeat processes. Working on why heartbeat doesn't restart itself properly. I'd send the latter as a patch to the list but it is rather intertwined in the former
 *
 * Revision 1.127  2001/08/10 17:35:38  alan
 * Removed some files for comm plugins
 * Moved the rest of the software over to use the new plugin system for comm
 * plugins.
 *
 * Revision 1.126  2001/08/02 06:09:19  alan
 * Put in fix inspired by pubz@free.fr (aka erwan@mandrakesoft.com ?)
 * to fix recovery after a cluster partition.
 * He discovered that the problem was that the master status process was
 * setting a flag which the control process needed to check.  So, the result
 * was that it never saw the flag set because of the different address spaces.
 * So, I put the flag into the shared memory segment.
 *
 * Revision 1.125  2001/08/02 01:45:16  alan
 * copyright change and message change.
 *
 * Revision 1.124  2001/07/17 15:00:04  alan
 * Put in Matt's changes for findif, and committed my changes for the new module loader.
 * You now have to have glib.
 *
 * Revision 1.123  2001/07/04 17:00:56  alan
 * Put in changes to make the the sequence number updating report failure
 * if close or fsync fails.
 *
 * Revision 1.122  2001/07/03 14:09:07  alan
 * More debug for Matt Soffen...
 *
 * Revision 1.121  2001/07/02 22:29:35  alan
 * Put in a little more basic debug for heartbeat.
 *
 * Revision 1.120  2001/07/02 19:12:57  alan
 * Added debugging code around startup of child processes.
 *
 * Revision 1.119  2001/06/28 12:16:44  alan
 * Committed the *rest* of Juri Haberland's script patch that I thought I
 * had already applied :-(.
 *
 * Revision 1.118  2001/06/27 23:33:46  alan
 * Put in the changes to use times(&proforma_tms) instead of times(NULL)
 *
 * Revision 1.117  2001/06/23 07:01:48  alan
 * Changed CLOCKS_PER_SEC back into CLK_TCK.
 * Quite a few places, and add portability stuff for it to portability.h
 *
 * Revision 1.116  2001/06/16 12:19:08  alan
 * Updated various pieces of code to use CLOCKS_PER_SEC instead of CLK_TCK
 * and moved the portability #ifdefs to only two places...
 *
 * Revision 1.115  2001/06/08 04:57:47  alan
 * Changed "config.h" to <portability.h>
 *
 * Revision 1.114  2001/06/07 21:29:44  alan
 * Put in various portability changes to compile on Solaris w/o warnings.
 * The symptoms came courtesy of David Lee.
 *
 * Revision 1.113  2001/05/31 16:51:18  alan
 * Made not being able to create the PID file a fatal error...
 *
 * Revision 1.112  2001/05/31 13:50:56  alan
 * Moving towards getting modules working.  More debug also...
 *
 * Revision 1.111  2001/05/27 04:58:32  alan
 * Made some warnings go away.
 *
 * Revision 1.110  2001/05/26 17:38:01  mmoerz
 * *.cvsignore: added automake generated files that were formerly located in
 * 	     config/
 * * Makefile.am: removed ac_aux_dir stuff (for libtool) and added libltdl
 * * configure.in: removed ac_aux_dir stuff (for libtool) and added libltdl as
 * 		a convenience library
 * * bootstrap: added libtools libltdl support
 * * heartbeat/Makefile.am: added some headerfile to noinst_HEADERS
 * * heartbeat/heartbeat.c: changed dlopen, dlclose to lt_dlopen, lt_dlclose
 * * heartbeat/crc.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/mcast.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/md5.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/ping.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/serial.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/sha1.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/udp.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/hb_module.h: added EXPORT() Macro, changed to libtools function
 * 			pointer
 * * heartbeat/module.c: converted to libtool (dlopen/dlclose -> lt_dlopen/...)
 * 		      exchanged scandir with opendir, readdir. enhanced
 * 		      autoloading code so that only .la modules get loaded.
 *
 * Revision 1.109  2001/05/22 13:25:02  alan
 * Put in David Lee's portability fix for attaching shared memory segs
 * without getting alignment warnings...
 *
 * Revision 1.108  2001/05/21 15:29:50  alan
 * Moved David Lee's LOG_PRI (syslog) patch from heartbeat.c to heartbeat.h
 *
 * Revision 1.107  2001/05/21 15:11:50  alan
 * Added David Lee's change to work without the LOG_PRI macro in config.h
 *
 * Revision 1.106  2001/05/20 04:37:35  alan
 * Fixed a bug in the hb_versioninfo() function where a variable
 * was supposed to be static, but wasn't...
 *
 * Revision 1.105  2001/05/15 19:52:50  alan
 * More portability fixes from David Lee
 *
 * Revision 1.104  2001/05/11 14:55:06  alan
 * Followed David Lee's suggestion about splitting out all the heartbeat process
 * management stuff into a separate header file...
 * Also changed to using PATH_MAX for maximum pathname length.
 *
 * Revision 1.103  2001/05/11 06:20:26  alan
 * Fixed CFLAGS so we load modules from the right diurectory.
 * Fixed minor static symbol problems.
 * Fixed a bug which kept early error messages from coming out.
 *
 * Revision 1.102  2001/05/10 22:36:37  alan
 * Deleted Makefiles from CVS and made all the warnings go away.
 *
 * Revision 1.101  2001/05/09 23:21:21  mmoerz
 * autoconf & automake & libtool changes
 *
 * * following directories have been added:
 *
 *   - config	will contain autoconf/automake scripts
 *   - linux-ha	contains config.h which is generated by autoconf
 * 		will perhaps some day contain headers which are used throughout
 * 		linux-ha
 *   - replace	contains as the name implies replacement stuff for targets
 * 		where specific sources are missing.
 *
 * * following directories have been added to make a split up between c-code
 *   and shell scripts and to easy their installation with automake&autoconf
 *
 *   - heartbeat/init.d		containment of init.d script for heartbeat
 *   - heartbeat/logrotate.d	containment of logrotate script for heartbeat
 *
 *   - ldirectord/init.d		similar to heartbeat
 *   - ldirectord/logrotate.d	similar to heartbeat
 *
 * * general changes touching the complete repository:
 *
 *   - all Makefiles have been replaced by Makefile.ams.
 *
 *   - all .cvsingnore files have been enhanced to cope with the dirs/files
 *     that are added by automake/autoconf
 *     Perhaps it would be a nice idea to include those files, but the sum
 *     of their size if beyond 100KB and they are likely to vary from
 *     automake/autoconf version.
 *     Let's keep in mind that we will have to include them in distribution
 *     .tgz anyway.
 *
 *   - in dir replace setenv.c was placed to available on platform where
 *     putenv() has to be used since setenv is depricated (better rewrite
 *     code -> to be done later)
 *
 * * following changes have been made to the files of linux-ha:
 *
 *   - all .cvsignore files have been changed to ignore files generated by
 *     autoconf/automake and all files produced during the build-process
 *
 *   - heartbeat/heartbeat.c:	added #include <config.h>
 *
 *   - heartbeat/config.c:		added #include <config.h>
 *
 * * following files have been added:
 *    - Makefile.am: see above
 *    - configure.in: man autoconf/automake file
 *    - acconfig.h: here are additional defines that are needed for
 * 		 linux-ha/config.h
 *    - bootstrap: the shell script that 'compiles' the autoconf/automake script
 * 		into a useable form
 *    - config/.cvsignore: no comment
 *    - doc/Makefile.am: no comment
 *    - heartbeat/Makefile.am: no comment
 *    - heartbeat/lib/Makefile.am: no comment
 *    - heartbeat/init.d/.cvsignore: no comment
 *    - heartbeat/init.d/heartbeat: copy of hearbeat/hearbeat.sh
 *    - heartbeat/init.d/Makefile.am: no comment
 *    - heartbeat/logrotate.d/.cvsignore: no comment
 *    - heartbeat/logrotate.d/Makefile.am: no comment
 *    - heartbeat/logrotate.d/heartbeat: copy of hearbeat/heartbeat.logrotate
 *    - heartbeat/rc.d/Makefile.am: no comment
 *    - heartbeat/resource.d/Makefile.am: no comment
 *    - ldirectord/Makefile.am: no comment
 *    - ldirectord/init.d/Makefile.am: no comment
 *    - ldirectord/init.d/.cvsignore: no comment
 *    - ldirectord/init.d/ldiretord: copy of ldirectord/ldirectord.sh
 *    - ldirectord/logrotate.d/Makefile.am: no comment
 *    - ldirectord/logrotate.d/.cvsignore: no comment
 *    - ldirectord//ldiretord: copy of ldirectord/ldirectord.logrotate
 *    - linux-ha/.cvsignore: no comment
 *    - replace/.cvsignore: no comment
 *    - replace/setenv.c: replacement function for targets where setenv is missing
 *    - replace/Makefile.am: no comment
 *    - stonith/Makefile.am: no comment
 *
 * Revision 1.100  2001/04/19 13:41:54  alan
 * Removed the two annoying "error" messages that occur when heartbeat
 * is shut down.  They are: "controlfifo2msg: cannot create message"
 * and "control_process: NULL message"
 *
 * Revision 1.99  2001/03/16 03:01:12  alan
 * Put in a fix to Norbert Steinl's problem with the logger facility
 * and priority being wrong.
 *
 * Revision 1.98  2001/03/11 06:23:09  alan
 * Fixed the bug of quitting whenever stats needed to be printed.
 * This bug was reported by Robert_Macaulay@Dell.com.
 * The underlying problem was that the stonith code didn.t exit after
 * the child process completed, but returned, and then everything got
 * a bit sick after that ;-)
 *
 * Revision 1.97  2001/03/11 03:16:12  alan
 * Fixed the problem with mcast not incrementing nummedia.
 * Installed mcast module in the makefile.
 * Made the code for printing things a little more cautious about data it is
 * passed as a parameter.
 *
 * Revision 1.96  2001/03/06 21:11:05  alan
 * Added initdead (initial message) dead time to heartbeat.
 *
 * Revision 1.95  2001/02/01 11:52:04  alan
 * Change things to that things occur in the right order.
 * We need to not start timing message reception until we're completely started.
 * We need to Stonith the other guy before we take over their resources.
 *
 * Revision 1.94  2000/12/12 23:23:46  alan
 * Changed the type of times from time_t to TIME_T (unsigned long).
 * Added BuildPreReq: lynx
 * Made things a little more OpenBSD compatible.
 *
 * Revision 1.93  2000/12/04 22:11:22  alan
 * FreeBSD compatibility changes.
 *
 * Revision 1.92  2000/11/12 21:12:48  alan
 * Set the close-on-exec bit for the watchdog file descriptor.
 *
 * Revision 1.91  2000/11/12 04:29:22  alan
 * Fixed: syslog/file simultaneous logging.
 * 	Added a group for API clients.
 * 	Serious problem with replay attack protection.
 * 	Shutdown now waits for resources to be completely given up
 * 		before stopping heartbeat.
 * 	Made the stonith code run in a separate process.
 *
 * Revision 1.90  2000/09/10 03:48:52  alan
 * Fixed a couple of bugs.
 * - packets that were already authenticated didn't get reauthenticated correctly.
 * - packets that were irretrievably lost didn't get handled correctly.
 *
 * Revision 1.89  2000/09/02 23:26:24  alan
 * Fixed bugs surrounding detecting cluster partitions, and around
 * restarts.  Also added the unfortunately missing ifstat and ns_stat files...
 *
 * Revision 1.88  2000/09/01 22:35:50  alan
 * Minor change to make restarts after cluster partitions work more reliably.
 *
 * Revision 1.87  2000/09/01 21:15:23  marcelo
 * Fixed auth file reread wrt dynamic modules
 *
 * Revision 1.86  2000/09/01 21:10:46  marcelo
 * Added dynamic module support
 *
 * Revision 1.85  2000/09/01 06:27:49  alan
 * Added code to force a status update when we restart.
 *
 * Revision 1.84  2000/09/01 06:07:43  alan
 * Fixed the "missing library" problem, AND probably fixed the perennial
 * problem with partitioned cluster.
 *
 * Revision 1.83  2000/09/01 04:18:59  alan
 * Added missing products to Specfile.
 * Perhaps fixed the partitioned cluster problem.
 *
 * Revision 1.82  2000/08/13 04:36:16  alan
 * Added code to make ping heartbeats work...
 * It looks like they do, too ;-)
 *
 * Revision 1.81  2000/08/11 00:30:07  alan
 * This is some new code that does two things:
 * 	It has pretty good replay attack protection
 * 	It has sort-of-basic recovery from a split partition.
 *
 * Revision 1.80  2000/08/01 05:48:25  alan
 * Fixed several serious bugs and a few minor ones for the heartbeat API.
 *
 * Revision 1.79  2000/07/31 03:39:40  alan
 * This is a working version of heartbeat with the API code.
 * I think it even has a reasonable security policy in it.
 *
 * Revision 1.78  2000/07/31 00:05:17  alan
 * Put the high-priority stuff back into heartbeat...
 *
 * Revision 1.77  2000/07/31 00:04:32  alan
 * First working version of security-revised heartbeat API code.
 * Not all the security checks are in, but we're making progress...
 *
 * Revision 1.76  2000/07/26 05:17:19  alan
 * Added GPL license statements to all the code.
 *
 * Revision 1.75  2000/07/21 16:59:38  alan
 * More minor changes to the Stonith API.
 * I switched from enums to #defines so that people can use #ifdefs if in
 * the future they want to do so.  In fact, I changed the ONOFF code
 * in the Baytech module to do just that.  It's convenient that way :-)
 * I *still* don't define the ON/OFF operation code in the API though :-)
 *
 * Revision 1.74  2000/07/21 13:25:51  alan
 * Made heartbeat consistent with current Stonith API.
 *
 * Revision 1.73  2000/07/21 04:22:34  alan
 * Revamped the Stonith API to make it more readily extensible.
 * This nice improvement was suggested by Bhavesh Davda of Avaya.
 * Thanks Bhavesh!
 *
 * Revision 1.72  2000/07/20 16:51:54  alan
 * More API fixes.
 * The new API code now deals with interfaces changes, too...
 *
 * Revision 1.71  2000/07/17 19:27:52  alan
 * Fixed a bug in stonith code (it didn't always kill telnet command)
 *
 * Revision 1.70  2000/07/16 22:14:37  alan
 * Added stonith capabilities to heartbeat.
 * Still need to make the stonith code into a library...
 *
 * Revision 1.69  2000/07/16 20:42:53  alan
 * Added the late heartbeat warning code.
 *
 * Revision 1.68  2000/07/11 03:49:42  alan
 * Further evolution of the heartbeat API code.
 * It works quite a bit at this point - at least on the server side.
 * Now, on to the client side...
 *
 * Revision 1.67  2000/07/11 00:25:52  alan
 * Added a little more API code.  It looks like the rudiments are now working.
 *
 * Revision 1.66  2000/07/10 23:08:41  alan
 * Added code to actually put the API code in place.
 * Wonder if it works?
 *
 * Revision 1.65  2000/06/17 12:09:10  alan
 * Fixed the problem when one side or the other has no local resources.
 * Before it whined incessantly about being no one holding local resources.
 * Now, it thinks it owns local resources even if there aren't any.
 * (sort of like being the king of nothing).
 *
 * Revision 1.64  2000/06/15 14:24:31  alan
 * Changed the version #.  Minor comment changes.
 *
 * Revision 1.63  2000/06/15 06:03:50  alan
 * Missing '[' in debug message.  pretty low priority.
 *
 * Revision 1.62  2000/06/15 05:51:41  alan
 * Added a little more version info when debugging is turned on.
 *
 * Revision 1.61  2000/06/14 22:08:29  lclaudio
 * *** empty log message ***
 *
 * Revision 1.60  2000/06/14 15:43:14  alan
 * Put in a little shutdown code to make child processes that we've started go away.
 *
 * Revision 1.59  2000/06/14 06:17:35  alan
 * Changed comments quite a bit, and the code a little...
 *
 * Revision 1.58  2000/06/13 20:34:10  alan
 * Hopefully put the finishing touches on the restart/nice_failback code.
 *
 * Revision 1.57  2000/06/13 20:19:24  alan
 * Added code to make restarting (-R) work with nice_failback. But, not enough, yet...
 *
 * Revision 1.56  2000/06/13 17:59:53  alan
 * Fixed the nice_failback code to change the way it handles states.
 *
 * Revision 1.55  2000/06/13 04:20:41  alan
 * Fixed a bug for handling logfile.  It never worked, except by the default case.
 * Fixed a bug related to noting when various nodes were out of transition.
 *
 * Revision 1.54  2000/06/12 23:01:14  alan
 * Added comments about new behavior for -r flag with nice_failover.
 *
 * Revision 1.53  2000/06/12 22:03:11  alan
 * Put in a fix to the link status code, to undo something I'd broken, and also to simplify it.
 * I changed heartbeat.sh so that it uses the -r flag to restart heartbeat instead
 * of stopping and starting it.
 *
 * Revision 1.52  2000/06/12 06:47:35  alan
 * Changed a little formatting to make things read nicer on an 80-column screen.
 *
 * Revision 1.51  2000/06/12 06:11:09  alan
 * Changed resource takeover order to left-to-right
 * Added new version of nice_failback.  Hopefully it works wonderfully!
 * Regularized some error messages
 * Print the version of heartbeat when starting
 * Hosts now have three statuses {down, up, active}
 * SuSE compatability due to Friedrich Lobenstock and alanr
 * Other minor tweaks, too numerous to mention.
 *
 * Revision 1.50  2000/05/27 07:43:06  alan
 * Added code to set signal(SIGCHLD, SIG_DFL) in 3 places.  Fix due to lclaudio and Fabio Olive Leite
 *
 * Revision 1.49  2000/05/17 13:01:49  alan
 * Changed argv[0] and cmdname to be shorter.
 * Changed ha parsing function to close ha.cf.
 * Changed comments in ppp-udp so that it notes the current problems.
 *
 * Revision 1.48  2000/05/11 22:47:50  alan
 * Minor changes, plus code to put in hooks for the new API.
 *
 * Revision 1.47  2000/05/09 03:00:59  alan
 * Hopefully finished the removal of the nice_failback code.
 *
 * Revision 1.46  2000/05/09 00:38:44  alan
 * Removed most of the nice_failback code.
 *
 * Revision 1.45  2000/05/03 01:48:28  alan
 * Added code to make non-heartbeat child processes not run as realtime procs.
 * Also fixed the message about creating FIFO to not be an error, just info.
 *
 * Revision 1.44  2000/04/28 21:41:37  alan
 * Added the features to lock things in memory, and set our priority up.
 *
 * Revision 1.43  2000/04/27 12:50:20  alan
 * Changed the port number to 694.  Added the pristene target to the ldirectord
 * Makefile.  Minor tweaks to heartbeat.sh, so that it gives some kind of
 * message if there is no configuration file yet.
 *
 * Revision 1.42  2000/04/12 23:03:49  marcelo
 * Added per-link status instead per-host status. Now we will able
 * to develop link<->service dependacy scheme.
 *
 * Revision 1.41  2000/04/08 21:33:35  horms
 * readding logfile cleanup
 *
 * Revision 1.40  2000/04/05 13:40:28  lclaudio
 *   + Added the nice_failback feature. If the cluster is running when
 *	 the primary starts it acts as a secondary.
 *
 * Revision 1.39  2000/04/03 08:26:29  horms
 *
 *
 * Tidied up the output from heartbeat.sh (/etc/rc.d/init.d/heartbeat)
 * on Redhat 6.2
 *
 * Logging to syslog if a facility is specified in ha.cf is instead of
 * rather than as well as file logging as per instructions in ha.cf
 *
 * Fixed a small bug in shellfunctions that caused logs to syslog
 * to be garbled.
 *
 * Revision 1.38  1999/12/25 19:00:48  alan
 * I now send local status unconditionally every time the clock jumps backwards.
 *
 * Revision 1.37  1999/12/25 08:44:17  alan
 * Updated to new version stamp
 * Added Lars Marowsky-Bree's suggestion to make the code almost completely
 * immune from difficulties inherent in jumping the clock around.
 *
 * Revision 1.36  1999/11/27 16:00:02  alan
 * Fixed a minor bug about where a continue should go...
 *
 * Revision 1.35  1999/11/26 07:19:17  alan
 * Changed heartbeat.c so that it doesn't say "seqno not found" for a
 * packet which has been retransmitted recently.
 * The code continued to the next iteration of the inner loop.  It needed
 * to continue to the next iteration of the outer loop.  lOOPS!
 *
 * Revision 1.34  1999/11/25 20:13:15  alan
 * Minor retransmit updates.  Need to add another source file to CVS, too...
 * These updates were to allow us to simulate lots of packet losses.
 *
 * Revision 1.33  1999/11/23 08:50:01  alan
 * Put in the complete basis for the "reliable" packet transport for heartbeat.
 * This include throttling the packet retransmission on both sides, both
 * from the requestor not asking too often, and from the resender, who won't
 * retransmit a packet any more often than once a second.
 * I think this looks pretty good at this point (famous last words :-)).
 *
 * Revision 1.32  1999/11/22 20:39:49  alan
 * Removed references to the now-obsolete monitoring code...
 *
 * Revision 1.31  1999/11/22 20:28:23  alan
 * First pass of putting real packet retransmission.
 * Still need to request missing packets from time to time
 * in case retransmit requests get lost.
 *
 * Revision 1.30  1999/11/14 08:23:44  alan
 * Fixed bug in serial code where turning on flow control caused
 * heartbeat to hang.  Also now detect hangs and shutdown automatically.
 *
 * Revision 1.29  1999/11/11 04:58:04  alan
 * Fixed a problem in the Makefile which caused resources to not be
 * taken over when we start up.
 * Added RTSCTS to the serial port.
 * Added lots of error checking to the resource takeover code.
 *
 * Revision 1.28  1999/11/09 07:34:54  alan
 * *Correctly* fixed the problem Thomas Hepper reported.
 *
 * Revision 1.27  1999/11/09 06:13:02  alan
 * Put in Thomas Hepper's bug fix for the alarm occurring when waiting for
 * resources to be listed during initial startup.
 * Also, minor changes to make config work without a linker warning...
 *
 * Revision 1.26  1999/11/08 02:07:59  alan
 * Minor changes for reasons I can no longer recall :-(
 *
 * Revision 1.25  1999/11/06 03:41:15  alan
 * Fixed some bugs regarding logging
 * Also added some printout for initially taking over resources
 *
 * Revision 1.24  1999/10/25 15:35:03  alan
 * Added code to move a little ways along the path to having error recovery
 * in the heartbeat protocol.
 * Changed the code for serial.c and ppp-udp.c so that they reauthenticate
 * packets they change the ttl on (before forwarding them).
 *
 * Revision 1.23  1999/10/19 13:55:36  alan
 * Changed comments about being red hat compatible
 * Also, changed heartbeat.c to be both SuSE and Red Hat compatible in it's -s
 * output
 *
 * Revision 1.22  1999/10/19 01:55:54  alan
 * Put in code to make the -k option loop until the killed heartbeat stops running.
 *
 * Revision 1.21  1999/10/11 14:29:15  alanr
 * Minor malloc tweaks
 *
 * Revision 1.20  1999/10/11 05:18:07  alanr
 * Minor tweaks in mem stats, etc
 *
 * Revision 1.19  1999/10/11 04:50:31  alanr
 * Alan Cox's suggested signal changes
 *
 * Revision 1.18  1999/10/10 22:22:47  alanr
 * New malloc scheme + send initial status immediately
 *
 * Revision 1.17  1999/10/10 20:12:08  alanr
 * New malloc/free (untested)
 *
 * Revision 1.16  1999/10/05 18:47:52  alanr
 * restart code (-r flag) now works as I think it should
 *
 * Revision 1.15  1999/10/05 16:11:49  alanr
 * First attempt at restarting everything with -R/-r flags
 *
 * Revision 1.14  1999/10/05 06:17:06  alanr
 * Fixed various uninitialized variables
 *
 * Revision 1.13  1999/10/05 05:17:34  alanr
 * Added -s (status) option to heartbeat, and used it in heartbeat.sh...
 *
 * Revision 1.12  1999/10/05 04:35:10  alanr
 * Changed it to use the new heartbeat -k option to shut donw heartbeat.
 *
 * Revision 1.11  1999/10/05 04:09:45  alanr
 * Fixed a problem reported by Thomas Hepper where heartbeat won't start if a regular
 * file by the same name as the FIFO exists.  Now I just remove it...
 *
 * Revision 1.10  1999/10/05 04:03:42  alanr
 * added code to implement the -r (restart already running heartbeat process) option.
 * It seems to work and everything!
 *
 * Revision 1.9  1999/10/04 03:12:20  alanr
 * Shutdown code now runs from heartbeat.
 * Logging should be in pretty good shape now, too.
 *
 * Revision 1.8  1999/10/03 03:13:47  alanr
 * Moved resource acquisition to 'heartbeat', also no longer attempt to make the FIFO, it's now done in heartbeat.  It should now be possible to start it up more readily...
 *
 * Revision 1.7  1999/10/02 18:12:08  alanr
 * Create fifo in heartbeat.c and change ha_perror() to  a var args thing...
 *
 * Revision 1.6  1999/09/30 05:40:37  alanr
 * Thomas Hepper's fixes
 *
 * Revision 1.5  1999/09/29 03:22:09  alanr
 * Added the ability to reread auth config file on SIGHUP
 *
 * Revision 1.4  1999/09/27 04:14:42  alanr
 * We now allow multiple strings, and the code for logging seems to also be working...  Thanks Guyscd ..
 *
 * Revision 1.3  1999/09/26 22:00:02  alanr
 * Allow multiple auth strings in auth file... (I hope?)
 *
 * Revision 1.2  1999/09/26 14:01:05  alanr
 * Added Mijta's code for authentication and Guenther Thomsen's code for serial locking and syslog reform
 *
 * Revision 1.1.1.1  1999/09/23 15:31:24  alanr
 * High-Availability Linux
 *
 * Revision 1.34  1999/09/16 05:50:20  alanr
 * Getting ready for 0.4.3...
 *
 * Revision 1.33  1999/09/15 17:47:13  alanr
 * removed the floating point load average calculation.  We didn't use it for anything anyway...
 *
 * Revision 1.32  1999/09/14 22:35:00  alanr
 * Added shared memory for tracking memory usage...
 *
 * Revision 1.31  1999/08/28 21:08:07  alanr
 * added code to handle SIGUSR1 and SIGUSR2 to diddle debug levels and
 * added code to not start heartbeat up if it's already running...
 *
 * Revision 1.30  1999/08/25 06:34:26  alanr
 * Added code to log outgoing messages in a FIFO...
 *
 * Revision 1.29  1999/08/18 04:27:31  alanr
 * #ifdefed out setting signal handler for SIGCHLD to SIG_IGN
 *
 * Revision 1.28  1999/08/17 03:48:11  alanr
 * added log entry...
 *
 */
