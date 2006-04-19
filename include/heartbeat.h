/* $Id: heartbeat.h,v 1.83 2006/04/19 21:08:54 alan Exp $ */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
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
#include <clplumbing/proctrack.h>
#include <clplumbing/cl_malloc.h>
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

#define	MAXFIELDS	30		/* Max # of fields in a msg */
#define HOSTLENG	100		/* Maximum size of "uname -a" return */
#define STATUSLENG	32		/* Maximum size of status field */
#define	MAXIFACELEN	30		/* Maximum interface length */
#define	MAXSERIAL	4
#define	MAXMEDIA	64
#define	MAXNODE		100
#define	MAXPROCS	((2*MAXMEDIA)+2)

#define	FIFOMODE	0600
#define	RQSTDELAY	10
#define	ACK_MSG_DIV	10
#ifndef HA_D
#	define	HA_D		HA_RC_DIR
#endif
#ifndef VAR_RUN_D
#	define	VAR_RUN_D	HA_VARRUNDIR
#endif
#ifndef VAR_LOG_D
#	define	VAR_LOG_D	HA_VARLOGDIR
#endif
#ifndef HALIB
#	define HALIB		HA_LIBDIR
#endif
#ifndef HA_MODULE_D
#	define HA_MODULE_D	HALIB "/modules"
#endif
#ifndef HA_PLUGIN_D
#	define HA_PLUGIN_D	HALIB "/plugins"
#endif
#ifndef TTY_LOCK_D
#	define	TTY_LOCK_D	HA_LOCKDIR
#endif

#ifndef RSC_TMPDIR
#	define	RSC_TMPDIR	HA_VARRUNDIR "/heartbeat/rsctmp"
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
#define	ONLINESTATUS	"online"	/* Status of an online client */
#define	OFFLINESTATUS	"offline"	/* Status of an offline client */
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
#define HALOGD		"HA_LOGD"	/* whether we use logging daemon or not */

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
#ifndef	HOSTUUIDCACHEFILE
#	define	HOSTUUIDCACHEFILE	VAR_LIB_D "/hostcache"
#endif
#ifndef DELHOSTCACHEFILE
#	define  DELHOSTCACHEFILE	VAR_LIB_D "/delhostcache"
#endif
#define		HOSTUUIDCACHEFILETMP	HOSTUUIDCACHEFILE ".tmp"
#define		DELHOSTCACHEFILETMP	DELHOSTCACHEFILE ".tmp"

#define	RCSCRIPT		HA_D "/harc"
#define CONFIG_NAME		HA_D "/ha.cf"
#define RESOURCE_CFG		HA_D "/haresources"

/* dynamic module directories */
#define COMM_MODULE_DIR	HA_MODULE_D "/comm"
#define AUTH_MODULE_DIR HA_MODULE_D "/auth"

#define	STATIC		/* static */

/* You may need to change this for your compiler */
#ifdef HAVE_STRINGIZE
#	define	ASSERT(X)	{if(!(X)) ha_assert(#X, __LINE__, __FILE__);}
#else
#	define	ASSERT(X)	{if(!(X)) ha_assert("X", __LINE__, __FILE__);}
#endif



#define HA_DATEFMT	"%Y/%m/%d_%T\t"
#define HA_FUNCS	HA_D "/shellfuncs"

#define	RC_ARG0		"harc"
#define ENV_PREFIX	"HA_"


/* Which side of a pipe is which? */

#define	P_READFD	0
#define	P_WRITEFD	1

#define	FD_STDIN	0
#define	FD_STDOUT	1
#define	FD_STDERR	2

#define PROTOCOL_VERSION	1

typedef unsigned long seqno_t;

#define	MAXMSGHIST	200
#define	MAXMISSING	MAXMSGHIST

#define	NOSEQUENCE	0xffffffffUL
struct seqtrack {
	longclock_t	last_rexmit_req;
	int		nmissing;
	seqno_t		generation;	/* Heartbeat generation # */
	seqno_t		last_seq;
	seqno_t		first_missing_seq; /* the smallest missing seq number*/
	GList*		client_status_msg_queue; /*client status message queue*/
	seqno_t		seqmissing[MAXMISSING];
	const char *	last_iface;
	seqno_t		ack_trigger; /*whenever a message received 
				      *with seq % ACK_MSG_DIV == ack_trigger
				      *we send back an ACK
				    */
	seqno_t		ackseq; /* ACKed seq*/
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
#define	NORMALNODE	"normal"
#define	PINGNODE	"ping"
#define	UNKNOWNNODE	"unknown"

struct node_info {
	int		nodetype;
	char		nodename[HOSTLENG];	/* Host name from config file */
	cl_uuid_t	uuid;
	char		status[STATUSLENG];	/* Status from heartbeat */
	gboolean	status_suppressed;	/* Status reports suppressed
						   for now */
	struct ha_msg*	saved_status_msg;	/* Last status (ignored) */
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

typedef enum {
	HB_JOIN_NONE	= 0,	/* Don't allow runtime joins of unknown nodes */
	HB_JOIN_OTHER	= 1,	/* Allow runtime joins of other nodes */
	HB_JOIN_ANY	= 2,	/* Don't even require _us_ to be in ha.cf */
}hbjointype_t;

#define MAXAUTH	16

#ifndef PATH_MAX
# define PATH_MAX MAXPATHLEN
#endif

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
	int		log_facility;		/* syslog facility, if any */
	char		facilityname[PATH_MAX];	/* syslog facility name (if any) */
	char   		logfile[PATH_MAX];	/* path to log file, if any */
        int    		use_logfile;            /* Flag to use the log file*/
	char		dbgfile[PATH_MAX];	/* path to debug file, if any */
	int    		use_dbgfile;            /* Flag to use the debug file*/
	int		memreserve;		/* number of kbytes to preallocate in heartbeat */
	int		rereadauth;		/* 1 if we need to reread auth file */
	seqno_t		generation;		/* Heartbeat generation # */
	cl_uuid_t	uuid;			/* uuid for this node*/
	int		uuidfromname;		/* do we get uuid from nodename?*/
	hbjointype_t	rtjoinconfig;		/* Runtime join behavior */
	int		authnum;
	Stonith*	stonith;	/* Stonith method - r1-style cluster only */
	struct HBauth_info* authmethod;	/* auth_config[authnum] */
	struct node_info  nodes[MAXNODE];
	struct HBauth_info  auth_config[MAXAUTH];
	GList*		client_list;
			/* List data: struct client_child */
	GList*		last_client;/* Last in client_list */
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

struct msg_xmit_hist {
	struct ha_msg*	msgq[MAXMSGHIST];
	seqno_t		seqnos[MAXMSGHIST];
	longclock_t	lastrexmit[MAXMSGHIST];
	int		lastmsg;
	seqno_t		hiseq;
	seqno_t		lowseq; /* one less than min actually present */
	seqno_t		ackseq;
	struct node_info* lowest_acknode;
};

/*
 *	client_child: information on clients that we spawn and keep track of
 *	They don't strictly have to use the client API, but most probably do.
 *	We start them when we start up, and shut them down when we shut down.
 *	Normally, if they they die, we restart them.
 */
struct client_child {
	pid_t		pid;		/* Process id of child process */
	ProcTrack*	proctrack;	/* Process tracking structure */
	int		respawn;	/* Respawn it if it dies? */
	uid_t		u_runas;	/* Which user to run as? */
	gid_t		g_runas;	/* Which group id to run as? */
	int		respawncount;	/* Last time we respawned */
	int		shortrcount;	/* Count of fast respawns */
	char*		command;	/* What command to run? */
	char*		path;		/* Path (argv[0])? */
};

int api_remove_client_pid(pid_t c_pid, const char * reason);


extern struct sys_config *	config;
extern int			debug_level;
extern int			udpport;
extern int			RestartRequested;
extern char *			localnodename;


#define ha_log		cl_log
#define ha_perror	cl_perror

/* Generally useful exportable HA heartbeat routines... */
extern void		ha_assert(const char *s, int line, const char * file);
gboolean		heartbeat_on_congestion(void);
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
int	delete_node(const char * value);
void	SetParameterValue(const char * name, const char * value);

gint		uuid_equal(gconstpointer v, gconstpointer v2);
guint		uuid_hash(gconstpointer key);
int		write_cache_file(struct sys_config * cfg);
int		read_cache_file(struct sys_config * cfg);
int		write_delnode_file(struct sys_config * cfg);
void		add_nametable(const char* nodename, struct node_info* value);
void		add_uuidtable(cl_uuid_t*, struct node_info* value);
const char *	uuid2nodename(cl_uuid_t* uuid);
int		nodename2uuid(const char* nodename, cl_uuid_t*);
int		inittable(void);
gboolean	update_tables(const char* nodename, cl_uuid_t* uuid);
struct node_info* lookup_tables(const char* nodename, cl_uuid_t* uuid);
void		cleanuptable(void);
int		tables_remove(const char* nodename, cl_uuid_t* uuid);
int		GetUUID(struct sys_config*, const char*, cl_uuid_t* uuid);
void		remove_from_dellist( const char* nodename);
void		append_to_dellist(struct node_info* hip);
void		request_msg_rexmit(struct node_info *node, seqno_t lowseq, seqno_t hiseq);
int		remove_msg_rexmit(struct node_info *node, seqno_t seq);
int		init_rexmit_hash_table(void);
int		destroy_rexmit_hash_table(void);

#endif /* _HEARTBEAT_H */
