/*
 * heartbeat.h: core definitions for the Linux-HA heartbeat program
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
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
#ifndef _HEARTBEAT_H
#	define _HEARTBEAT_H 1

static const char * _heartbeat_h_Id = "$Id: heartbeat.h,v 1.38 2003/08/06 13:48:46 horms Exp $";

#ifdef SYSV
#	include <sys/termio.h>
#	define TERMIOS	termio
#	define	GETATTR(fd, s)	ioctl(fd, TCGETA, s)
#	define	SETATTR(fd, s)	ioctl(fd, TCSETA, s)
#	define	FLUSH(fd)	ioctl(fd, TCFLSH, 2)
#else
#	define TERMIOS	termios
#	include <termios.h>
#	define	GETATTR(fd, s)	tcgetattr(fd, s)
#	define	SETATTR(fd, s)	tcsetattr(fd, TCSAFLUSH, s)
#	define	FLUSH(fd)	tcflush(fd, TCIOFLUSH)
#endif

#include <limits.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <HBauth.h>
#include <ha_msg.h>
#include <HBcomm.h>
#include <stonith/stonith.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/longclock.h>
#include <clplumbing/ipc.h>
#define index FooIndex
#define time FooTime
#include <glib.h>
#undef index
#undef time

/*
 * <syslog.h> might not contain LOG_PRI...
 * So, we define it ourselves, or error out if we can't...
 */

#ifndef LOG_PRI
#  ifdef LOG_PRIMASK
 	/* David Lee <T.D.Lee@durham.ac.uk> reports this works on Solaris */
#	define	LOG_PRI(p)      ((p) & LOG_PRIMASK)
#  else
#	error	"Syslog.h does not define either LOG_PRI or LOG_PRIMASK."
#  endif 
#endif

#define	MAXLINE		2048
#define	MAXFIELDS	30		/* Max # of fields in a msg */
#define HOSTLENG	100		/* Maximum size of "uname -a" return */
#define STATUSLENG	32		/* Maximum size of status field */
#define	MAXIFACELEN	30		/* Maximum interface length */
#define	MAXSERIAL	4
#define	MAXMEDIA	12
#define	MAXNODE		100
#define	MAXPROCS	((MAXNODE*2)+2)

#define	FIFOMODE	0600
#define	RQSTDELAY	10

#ifndef HA_D
#	define	HA_D		"/etc/ha.d"
#endif
#ifndef VAR_RUN_D
#	define	VAR_RUN_D	"/var/run"
#endif
#ifndef VAR_LOG_D
#	define	VAR_LOG_D	"/var/log"
#endif
#ifndef HALIB
#	define HALIB		"/usr/lib/heartbeat"
#endif
#ifndef HA_MODULE_D
#	define HA_MODULE_D	HALIB "/modules"
#endif
#ifndef HA_PLUGIN_D
#	define HA_PLUGIN_D	HALIB "/plugins"
#endif
#ifndef TTY_LOCK_D
#	if defined(linux)
#		define	TTY_LOCK_D	"/var/lock"
#	else
#		define	TTY_LOCK_D	"/var/spool/lock"
#	endif
#endif

#ifndef RSC_TMPDIR
#	define	RSC_TMPDIR	VAR_LIB_D "/rsctmp"
#endif

/* #define HA_DEBUG */
#define	DEFAULTLOG	VAR_LOG_D "/ha-log"
#define	DEFAULTDEBUG	VAR_LOG_D "/ha-debug"
#define	DEVNULL 	"/dev/null"

#define	HA_OKEXIT	0
#define	HA_FAILEXIT	1
#define	WHITESPACE	" \t\n\r\f"
#define	DELIMS		", \t\n\r\f"
#define	COMMENTCHAR	'#'
#define	CRLF		"\r\n"
#define	STATUS		"STATUS"
#define	INITSTATUS	"init"		/* Status of a node we've never heard from */
#define	UPSTATUS	"up"		/* Listening (we might not be xmitting) */
#define	ACTIVESTATUS	"active"	/* fully functional, and all links are up */
#define	DEADSTATUS	"dead"		/* Status of non-working link or machine */
#define	PINGSTATUS	"ping"		/* Status of a working ping node */
#define	JOINSTATUS	"join"		/* Status when an api client joins */
#define	LEAVESTATUS	"leave"		/* Status when an api client leaves */
#define	LINKUP		"up"		/* The status assigned to a working link */
#define	LOADAVG		"/proc/loadavg"
#define	PIDFILE		VAR_RUN_D "/heartbeat.pid"
#define KEYFILE         HA_D "/authkeys"
#define HA_SERVICENAME	"ha-cluster" 	/* Our official reg'd service name */
#define	UDPPORT		694		/* Our official reg'd port number */

/* Environment variables we pass to our scripts... */
#define CURHOSTENV	"HA_CURHOST"
#define OLDSTATUS	"HA_OSTATUS"
#define DATEFMT		"HA_DATEFMT"	/* Format string for date(1) */
#define LOGFENV		"HA_LOGFILE"	/* well-formed log file :-) */
#define DEBUGFENV	"HA_DEBUGLOG"	/* Debug log file */
#define LOGFACILITY	"HA_LOGFACILITY"/* Facility to use for logger */
#define HADIRENV	"HA_DIR"	/* The base HA directory */
#define HAFUNCENV	"HA_FUNCS"	/* Location of ha shell functions */
#define HANICEFAILBACK	"HA_NICEFAILBACK" /* "yes" when nice_failback is on */
#define HADONTASK	"HA_DONTASK"	/* "yes" when no other nodes "active" ...*/
#define HADEBUGVAL	"HA_DEBUG"	/* current debug value (if nonzero) */


#define	DEFAULTBAUD	B19200	/* Default serial link speed */
#define	DEFAULTBAUDRATE	19200	/* Default serial link speed as int */
#define	DEFAULTBAUDSTR	"19200"	/* Default serial link speed as string */

/* multicast defaults */
#define DEFAULT_MCAST_IPADDR "225.0.0.1" /* Default multicast group */
#define DEFAULT_MCAST_TTL 1	/* Default multicast TTL */
#define DEFAULT_MCAST_LOOP 0	/* Default mulitcast loopback option */

#define HB_STATIC_PRIO	1	/* Used with soft realtime scheduling */

#ifndef PPP_D
#	define	PPP_D		VAR_RUN_D "/ppp.d"
#endif
#ifndef FIFONAME
#	define	FIFONAME	VAR_LIB_D "/fifo"
#endif

#define	RCSCRIPT		HA_D "/harc"
#define CONFIG_NAME		HA_D "/ha.cf"
#define RESOURCE_CFG		HA_D "/haresources"

/* dynamic module directories */
#define COMM_MODULE_DIR	HA_MODULE_D "/comm"
#define AUTH_MODULE_DIR HA_MODULE_D "/auth"

#define	STATIC		/* static */
#define	MALLOCT(t)	((t *)(ha_malloc(sizeof(t))))

/* You may need to change this for your compiler */
#ifdef HAVE_STRINGIZE
#	define	ASSERT(X)	{if(!(X)) ha_assert(#X, __LINE__, __FILE__);}
#else
#	define	ASSERT(X)	{if(!(X)) ha_assert("X", __LINE__, __FILE__);}
#endif



#define HA_DATEFMT	"%Y/%m/%d_%T\t"
#define HA_FUNCS	HA_D "/shellfuncs"

#define	RC_ARG0		"harc"



/* Which side of a pipe is which? */

#define	P_READFD	0
#define	P_WRITEFD	1

#define	FD_STDIN	0
#define	FD_STDOUT	1
#define	FD_STDERR	2

typedef unsigned long seqno_t;

#define	MAXMISSING	16
#define	NOSEQUENCE	0xffffffffUL
struct seqtrack {
	longclock_t	last_rexmit_req;
	int		nmissing;
	seqno_t		generation;	/* Heartbeat generation # */
	seqno_t		last_seq;
	seqno_t		seqmissing[MAXMISSING];
	const char *	last_iface;
};

struct link {
	longclock_t	lastupdate;
	const char *	name;
	int		isping;
	char		status[STATUSLENG]; /* up or down */
	TIME_T rmt_lastupdate; /* node's idea of last update time for this link */
};

#define	NORMALNODE_I	0
#define	PINGNODE_I	1
#define	NORMALNODE      "normal"
#define	PINGNODE        "ping"
#define	UNKNOWNNODE     "unknown"

struct node_info {
	int		nodetype;
	char		nodename[HOSTLENG];	/* Host name from config file */
	char		status[STATUSLENG];	/* Status from heartbeat */
	struct link	links[MAXMEDIA];
	int		nlinks;
	TIME_T		rmt_lastupdate;	/* node's idea of last update time */
	seqno_t		status_seqno;	/* Seqno of last status update */
	longclock_t	dead_ticks;	/* # ticks to declare dead */
	longclock_t	local_lastupdate;/* Date of last update in clock_t time*/
	int		anypacketsyet;	 /* True after reception of 1st pkt */
	struct seqtrack	track;
	int		has_resources;	/* TRUE if node may have resources */
};


#define MAXAUTH	16

struct sys_config {
	TIME_T		cfg_time;		/* Timestamp of config file */
	TIME_T		auth_time;		/* Timestamp of authorization file */
	TIME_T		rsc_time;		/* Timestamp of haresources file */
	int		format_vers;		/* Version of this info */
	int		nodecount;		/* Number of nodes in cluster */
	long		heartbeat_ms;		/* Milliseconds between heartbeats */
	long		deadtime_ms;		/* Ticks before declaring dead */
	long		deadping_ms;		/* Ticks before declaring ping nodes */
	long		initial_deadtime_ms;	/* Ticks before saying dead 1st time*/
	long		warntime_ms;		/* Ticks before issuing warning */
	int		hopfudge;		/* hops beyond nodecount allowed */
	int    		log_facility;		/* syslog facility, if any */
	char		facilityname[PATH_MAX];	/* syslog facility name (if any) */
	char   		logfile[PATH_MAX];	/* path to log file, if any */
        int    		use_logfile;            /* Flag to use the log file*/
	char		dbgfile[PATH_MAX];	/* path to debug file, if any */
        int    		use_dbgfile;            /* Flag to use the debug file*/
	int		rereadauth;		/* 1 if we need to reread auth file */
	seqno_t		generation;	/* Heartbeat generation # */
	int		authnum;
	Stonith*	stonith;	/* Stonith method: WE NEED A LIST TO SUPPORT MULTIPLE STONITH DEVICES PER NODE -EZA */
	struct HBauth_info* authmethod;	/* auth_config[authnum] */
	struct node_info  nodes[MAXNODE];
	struct HBauth_info  auth_config[MAXAUTH];
	GList*		client_list;
			/* List data: struct client_child */
			/* These all show up in client_children */
			/* when they're spawned (and have a pid) */
	GHashTable*	client_children;/* Indexed by pid */
			/* associated data: struct client_child */
			/* They appear here after being spawned */
};


struct hb_media {
	void *		pd;		/* Private Data */
	const char *	name;		/* Unique medium name */
	char*		type;		/* Medium type */
	char*		description;	/* Medium description */
	const struct hb_media_fns*vf;	/* Virtual Functions */
	IPC_Channel*	wchan[2];
		/* Read by the write child processes.  */
	IPC_Channel*	rchan[2];
		/* Written to by the read child processes.  */
};

int parse_authfile(void);

#define	MAXMSGHIST	100
struct msg_xmit_hist {
	struct ha_msg*	msgq[MAXMSGHIST];
	seqno_t		seqnos[MAXMSGHIST];
	longclock_t	lastrexmit[MAXMSGHIST];
	int		lastmsg;
	seqno_t		hiseq;
	seqno_t		lowseq; /* one less than min actually present */
};

/*
 *	client_child: information on clients that we spawn and keep track of
 *	They don't strictly have to use the client API, but most probably do.
 *	We start them when we start up, and shut them down when we shut down.
 *	Normally, if they they die, we restart them.
 */
struct client_child {
	pid_t	pid;		/* Process id of child process */
	int	respawn;	/* Respawn it if it dies? */
	uid_t	u_runas;	/* Which user to run as? */
	gid_t	g_runas;	/* Which group id to run as? */
	int	respawncount;	/* Last time we respawned this command */
	int	shortrcount;	/* How many times has it respawned too fast? */
	char*	command;	/* What command to run? */
	char*	path;		/* Path (argv[0])? */
};

int api_remove_client_pid(pid_t c_pid, const char * reason);


extern struct sys_config *	config;
extern int			verbose;
extern int			debug;
extern int			udpport;
extern int			RestartRequested;
extern char *			localnodename;

#define	ANYDEBUG	(debug)
#define	DEBUGDETAILS	(debug >= 2)
#define	DEBUGAUTH	(debug >=3)
#define	DEBUGMODULE	(debug >=3)
#define	DEBUGPKT	(debug >= 4)
#define	DEBUGPKTCONT	(debug >= 5)

#define ha_log		cl_log
#define ha_perror	cl_perror

/* Generally useful exportable HA heartbeat routines... */
extern void		ha_assert(const char *s, int line, const char * file);

extern int		send_cluster_msg(struct ha_msg*msg);
extern void		cleanexit(int exitcode);
extern void		check_auth_change(struct sys_config *);
extern void		(*localdie)(void);
extern int		should_ring_copy_msg(struct ha_msg* m);
extern int 		controlipc2msg(IPC_Channel * channel
, 			struct ha_msg **);
extern int		add_msg_auth(struct ha_msg * msg);
extern unsigned char * 	calc_cksum(const char * authmethod, const char * key, const char * value);
struct node_info *	lookup_node(const char *);
struct link * lookup_iface(struct node_info * hip, const char *iface);
struct link *  iface_lookup_node(const char *);
int	add_node(const char * value, int nodetype);
void	SetParameterValue(const char * name, const char * value);

void*		ha_malloc(size_t size);
void*		ha_calloc(size_t nmemb, size_t size);
void		ha_free(void *ptr);
int		ha_is_allocated(const void *ptr);
void		ha_malloc_report(void);

#ifndef HA_HAVE_SETENV
int setenv(const char *name, const char * value, int why);
#endif

#ifndef HA_HAVE_SCANDIR
#include <dirent.h>
int
scandir (const char *directory_name,
	struct dirent ***array_pointer,
	int (*select_function) (const struct dirent *),

#ifdef USE_SCANDIR_COMPARE_STRUCT_DIRENT
	/* This is what the Linux man page says */
	int (*compare_function) (const struct dirent**, const struct dirent**)
#else
	/* This is what the Linux header file says ... */
	int (*compare_function) (const void *, const void *)
#endif
	);
#endif /* HAVE_SCANDIR */
#endif /* _HEARTBEAT_H */
