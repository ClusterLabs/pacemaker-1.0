
/* File: stonithd.c
 * Description: STONITH daemon for node fencing
 *
 * Author: Sun Jiang Dong <sunjd@cn.ibm.com>
 * Copyright (c) 2004 International Business Machines
 *
 * This program is free software; you can redistribute it and/or
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
/* Todo: 
 * 1. Change to a obvious DFA?
 * 2. uuid support
 * 3. How to make a stonith object as a master object? Only depend to the 
 *    stonith plugins?
 */

/***************************************************
 * How does stonithd reset a node
 *
 * Every stonithd instance has zero or more stonith resources in
 * the started (enabled) state. These resources represent stonith
 * devices configured in such a way as to be able to manage one
 * or more nodes.
 *
 * 1. One of the stonithd instances receives a request to manage
 * (stonith) a node.
 *
 * 2. stonithd invokes each of the local stonith resources in
 * turn to try to stonith the node. stonith resources don't have
 * defined priority/preference.
 *
 * 3. If none of the local stonith resources succeeded, then
 * stonithd broadcasts a message to other stonithd instances (on
 * other nodes) with a request to stonith the node.
 *
 * 4. All other stonithd instances repeat step 2. However, they
 * don't proceed with step 3. They report back to the originating
 * stonithd about the outcome.
 *
 ***************************************************/



#include <crm_internal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */
#include <errno.h>
#include <pwd.h>
#include <glib.h>
#include <pils/plugin.h>
#include <stonith/stonith.h>
#include <pils/generic.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/cl_syslog.h>
#include <clplumbing/uids.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/proctrack.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_uuid.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/realtime.h>
#if SUPPORT_HEARTBEAT
#    include <apphb.h>
#    include <hb_api.h>
#endif
#include <heartbeat.h>
#include <ha_msg.h>
#include <lrm/raexec.h>
#include <fencing/stonithd_msg.h>
#include <fencing/stonithd_api.h>
#include <clplumbing/cl_pidfile.h>
#include <clplumbing/Gmain_timeout.h>
#include <crm/crm.h>
#include <crm/common/cluster.h>

#include <assert.h>
#define ST_ASSERT(cond) assert(cond)

#define MAX_NODE_STORAGE 8192 /* space for all nodenames incl delimiters */
#define REBOOT_BLOCK_TIMEOUT 10*1000

/* For integration with heartbeat */
#define MAXCMP 80
#define MAGIC_EC 100
#define stonithd_log(priority, fmt...); \
	if ( ( priority != LOG_DEBUG ) || (debug_level > 0) ) { \
		cl_log(priority, fmt); \
	}

/* Only export logs when debug_level >= 2 */
#define stonithd_log2(priority, fmt...); \
	if ( ( priority != LOG_DEBUG ) || (debug_level > 1) ) { \
		cl_log(priority, fmt); \
	}

static int pil_loglevel_to_cl_loglevel[] = {
	/* Indices: <none>=0, PIL_FATAL=1, PIL_CRIT=2, PIL_WARN=3,
	   PIL_INFO=4, PIL_DEBUG=5 
	*/
	LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_WARNING, LOG_INFO, LOG_DEBUG
	};

typedef struct {
	char * name;
	pid_t pid;		/* client pid */
	uid_t uid;		/* client UID */
	gid_t gid;		/* client GID */
	IPC_Channel * ch;
	IPC_Channel * cbch;
	char * removereason;
	cl_uuid_t cookie;
} stonithd_client_t;

typedef enum {
	STONITH_RA_OP,	/* stonith resource operation */
	STONITH_INIT,	/* stonith operation initiated by myself */
	STONITH_REQ,	/* stonith operation required by others */
} operate_scenario_t;

typedef struct {
	operate_scenario_t  scenario;
	void *	result_receiver;	/* final result receiver -- IPC_Channel
					 * or a node name/uuid */
	union {
		stonith_ops_t	* st_op;
		stonithRA_ops_t * ra_op;
	} op_union;
	int timer_id;
	char * rsc_id;	/* private data which is not used in RA operations */
} common_op_t;

typedef struct stonith_rsc
{
	char *    rsc_id;
	char *    ra_name;
	GHashTable *    params;
	Stonith *	stonith_obj;
	char **		node_list;
} stonith_rsc_t;

/* Must correspond to stonith_type_t */
static const char * stonith_op_strname[] =
{
	"QUERY", "RESET", "POWERON", "POWEROFF"
};
/* Must correspond to stonith_ret_t */
static const char * stonith_op_result_strname[] =
{
	"SUCCEEDED", "CANNOT", "TIMEOUT", "GENERIC",
};

static GHashTable * chan_gsource_pairs = NULL;	/* msg channel => GCHSource */
static GHashTable * cbch_gsource_pairs = NULL;	/* callback channel => GCHSource */
static GList * client_list = NULL;
static GHashTable * executing_queue = NULL;
static GList * local_started_stonith_rsc = NULL;
static GList * mem_hostlist = NULL;
/* The next line is only for CTS test with APITEST */
static GHashTable * reboot_blocked_table = NULL;
static int negative_callid_counter = -2;

typedef int (*stonithd_api_msg_handler)(struct ha_msg * msg,
					gpointer data);

struct hostlist_shmseg {
	int shmid;
	pid_t pid;
};

struct api_msg_to_handler
{
	const char *			msg_type;
	stonithd_api_msg_handler	handler;
};

typedef void (*stonithd_clu_msg_handler)(struct ha_msg * msg,
					gpointer data);
struct clu_msg_to_handler {
	const char *			msg_type;
	stonithd_clu_msg_handler	handler;
};

typedef int (*RA_subop_handler)(stonithRA_ops_t * ra_op, gpointer data);

struct RA_operation_to_handler
{
	const char *			op_type;
	RA_subop_handler		handler;
};

/* Miscellaneous functions such as daemon routines and others. */
static void become_daemon(void);
static int show_daemon_status(const char * pidfile);
static int kill_running_daemon(const char * pidfile);
static void stonithd_quit(int signo);
static gboolean adjust_debug_level(int nsig, gpointer user_data);

/* Dealing with the child quit/abort event when executing STONTIH RA plugins. 
 */
static gboolean valid_op(common_op_t *op);
static void handleRA_finished_op(common_op_t *op, pid_t pid, int exitcode);
static void handle_finished_op(common_op_t *op, pid_t pid, int exitcode);
static void stonithdProcessDied(ProcTrack* p, int status, int signo
				, int exitcode, int waslogged);
static void stonithdProcessRegistered(ProcTrack* p);
static const char * stonithdProcessName(ProcTrack* p);

#if SUPPORT_HEARTBEAT
/* For application heartbeat related */
static unsigned long
        APPHB_INTERVAL  = 2000, /* MS */
        APPHB_WARNTIME  = 6000, /* MS */
	APPHB_INTVL_DETLA = 30; /* MS */

#define MY_APPHB_HB() do { \
	if (SIGNONED_TO_APPHBD == TRUE) { \
		if (apphb_hb() != 0) { \
			SIGNONED_TO_APPHBD = FALSE; \
		} \
	} \
} while(0)

/* 
 * Functions related to application heartbeat ( apphbd )
 */
static gboolean emit_apphb(gpointer data);
static int init_using_apphb(void);
#endif

/* Communication between nodes related.
 * For stonithing one node in the cluster.
 */
static void handle_msg_twhocan(struct ha_msg* msg, void* private_data);
static void handle_msg_ticanst(struct ha_msg* msg, void* private_data);
static void handle_msg_tstit(struct ha_msg* msg, void* private_data);
static void handle_msg_trstit(struct ha_msg* msg, void* private_data);
static void handle_msg_resetted(struct ha_msg* msg, void* private_data);
#if SUPPORT_HEARTBEAT
static int init_hb_msg_handler(void);
#endif
#if SUPPORT_AIS
static gboolean stonithd_ais_dispatch(AIS_Message *wrapper, char *data, int sender);
static void stonithd_ais_destroy(gpointer user_data);
static struct ha_msg* ais_msg2ha_msg(char *input);
static int attr2fld(char *input, struct ha_msg *msg);
#endif
static void stonithd_hb_callback(struct ha_msg* msg, void* private_data);
static void stonithd_hb_connection_destroy(void* private_data);
static gboolean stonithd_sendmsg(const char *node_name,
						struct ha_msg *msg, const char *st_op_type);
static gboolean reboot_block_timeout(gpointer data);
static void timerid_destroy_notify(gpointer user_data);
static void free_timer(gpointer data);

/* Local IPC communication related.
 * For implementing the functions for client APIs 
 */
static gboolean stonithd_client_dispatch(IPC_Channel * ch, gpointer user_data);
static void stonithd_IPC_destroy_notify(gpointer data);
static gboolean accept_client_dispatch(IPC_Channel * ch, gpointer data);
static gboolean accept_client_connect_callback(IPC_Channel *ch, gpointer user);
static gboolean stonithd_process_client_msg(struct ha_msg * msg, 
					    gpointer data);
static int init_client_API_handler(void);
static void free_client(gpointer data, gpointer user_data);
static stonithd_client_t * get_exist_client_by_chan(GList * client_list, 
						    IPC_Channel * ch);
static int delete_client_by_chan(GList ** client_list, IPC_Channel * ch);

static stonithd_client_t* get_client_by_cookie(GList *client_list, cl_uuid_t *cookie);

/* Client API functions */
static int on_stonithd_signon(struct ha_msg * msg, gpointer data);
static int on_stonithd_signoff(struct ha_msg * msg, gpointer data);
static int on_stonithd_node_fence(struct ha_msg * request, gpointer data);
static int on_stonithd_virtual_stonithRA_ops(struct ha_msg * request, 
					  gpointer data);
static int on_stonithd_list_stonith_types(struct ha_msg * request,
					  gpointer data);
static int on_stonithd_cookie(struct ha_msg * request, gpointer data);

static int stonithRA_operate( stonithRA_ops_t * op, gpointer data );
static int stonithRA_start( stonithRA_ops_t * op, gpointer data );
static int stonithRA_stop( stonithRA_ops_t * op, gpointer data );
static int stonithRA_monitor( stonithRA_ops_t * op, gpointer data );
static int stonithop_result_to_local_client(const stonith_ops_t * st_op
					    , gpointer data);
static int send_stonithop_final_result( const common_op_t * op );
static int stonithop_result_to_other_node( stonith_ops_t * st_op,
					   gconstpointer data);
static int send_stonithRAop_final_result(stonithRA_ops_t * ra_op, 
					 gpointer data);
static void destroy_key_of_op_htable(gpointer data);

static stonith_ops_t * dup_stonith_ops_t(stonith_ops_t * st_op);
static stonithRA_ops_t * new_stonithRA_ops_t(struct ha_msg * request);
static void free_stonithRA_ops_t(stonithRA_ops_t * ra_op);
static stonith_ops_t * new_stonith_ops_t(struct ha_msg * request);
static void free_stonith_ops_t(stonith_ops_t * st_op);
static void free_common_op_t(gpointer data);
static void free_stonith_rsc(stonith_rsc_t * srsc);
static stonith_rsc_t * get_started_stonith_resource(char * rsc_id);
static stonith_rsc_t * get_local_stonithobj_can_stonith(const char * node_name,
						const char * begin_rsc_id );
static int stonith_operate_locally(stonith_ops_t * st_op, stonith_rsc_t * srsc);
static void timeout_destroy_notify(gpointer user_data);
static gboolean stonithop_timeout(gpointer data);
static void my_hash_table_find( GHashTable * htable, gpointer * orig_key,
				gpointer * value, gpointer user_data);
static void has_this_callid(gpointer key, gpointer value,
				gpointer user_data);
static void insert_into_executing_queue(common_op_t *op, int call_id);
static int require_others_to_stonith(const stonith_ops_t * st_op);
static int initiate_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc, 
				    IPC_Channel * ch);
static int continue_local_stonithop(int old_key);
static int initiate_remote_stonithop(stonith_ops_t * st_op, IPC_Channel * ch);
static int changeto_remote_stonithop(int old_key);
static int require_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc, 
				   const char * asker_node);
static int broadcast_reset_success(const char * target);
static void trans_log(int priority, const char * fmt, ...)G_GNUC_PRINTF(2,3);
static struct hostlist_shmseg *lookup_shm_hostlist(pid_t pid);
static void add_shm_hostlist(int shmid, pid_t pid);
static void remove_shm_hostlist(pid_t pid);
static gboolean hostlist2shmem(char *rsc_id,
					int shmid,char **hostlist,int maxlist, int is_lastgasp);
static char ** shmem2hostlist(pid_t pid);
static char ** copyshmem(char *s);
static void record_new_srsc(stonithRA_ops_t *ra_op);

static struct api_msg_to_handler api_msg_to_handlers[] = {
	{ ST_SIGNON,	on_stonithd_signon },
	{ ST_SIGNOFF,	on_stonithd_signoff },
	{ ST_STONITH,	on_stonithd_node_fence },
	{ ST_RAOP,	on_stonithd_virtual_stonithRA_ops },
	{ ST_LTYPES,	on_stonithd_list_stonith_types },
};

static struct RA_operation_to_handler raop_handler[] = {
	{ "start",	stonithRA_start },
	{ "stop",	stonithRA_stop },
	{ "monitor",	stonithRA_monitor },
	{ "status",	stonithRA_monitor },
};

#define PID_FILE        HA_VARRUNDIR "/stonithd.pid"

/* define the message type between stonith daemons on different nodes */
#define T_WHOCANST  	"whocanst"	/* who can stonith a node */
#define T_ICANST	"icanst" 	/* I can stonith a node */	
#define T_STIT		"stit" 		/* please stonith it */	
#define T_RSTIT		"rstit" 	/* result of stonithing it */	
#define T_RESETTED	"resetted"	/* a node is resetted successfully */
#define T_IALIVE 	"iamalive"	/* I'm alive */
#define T_QSTCAP	"qstcap"	/* query the stonith capacity -- 
					   all the nodes who can stonith */

static struct clu_msg_to_handler clu_msg_to_handlers[] = {
	{ T_WHOCANST, handle_msg_twhocan },
	{ T_ICANST, handle_msg_ticanst },
	{ T_STIT, handle_msg_tstit },
	{ T_RSTIT, handle_msg_trstit },
	{ T_RESETTED, handle_msg_resetted },
};

/* 
 * Notice log messages for other programs, such as scripts to judge the 
 * status of this stonith daemon.
 */
static const char * M_STARTUP = "start up successfully.",
		  * M_RUNNING = "is already running.",
		  * M_QUIT    = "normally quit.",
		  * M_ABORT   = "abnormally abort.",
		  * M_STONITH_SUCCEED = "Succeeded to STONITH the node",
		  * M_STONITH_FAIL    = "Failed to STONITH the node";

static const char * simple_help_screen =
"Usage: stonithd [-ahikrsv]\n"
"	-a	Start up alone outside of heartbeat.\n" 
"		By default suppose it be started up and monitored by heartbeat.\n"
"	-h	This help information\n"
"	-i	The interval (millisecond) of emitting application hearbeat.\n"
"	-k	Kill the daemon.\n"
"	-r	Register to apphbd. Now not register to apphbd by default.\n"
"	-s	Show the status of the daemons.\n"
"	-v	Run the stonithd in debug mode. Under debug mode more\n"
"		debug information is written to log file.\n";
/*	-t	Test mode only.\n" 	*/

static const char * optstr = "ahi:krsvt";
/* Will replace it with dynamical a config variable */
#define STD_PIDFILE         HA_VARRUNDIR "/stonithd.pid"

/* Do not need itselv's log file for real wotk, only for debugging
#define DAEMON_LOG      HA_VARLOGDIR "/stonithd.log"
#define DAEMON_DEBUG    HA_VARLOGDIR "/stonithd.debug"
*/

static ProcTrack_ops StonithdProcessTrackOps = {
	stonithdProcessDied,
	stonithdProcessRegistered,
	stonithdProcessName
};

static char * 	local_nodename		= NULL;
static GMainLoop *	mainloop 		= NULL;
static const char * 	stonithd_name		= "stonithd";
extern const char * 	crm_system_name;
static gboolean		STARTUP_ALONE 		= FALSE;
#if SUPPORT_HEARTBEAT
static gboolean 	SIGNONED_TO_APPHBD	= FALSE;
static gboolean 	NEED_SIGNON_TO_APPHBD	= FALSE;
static ll_cluster_t *	hb			= NULL;
#endif
static gboolean 	TEST 			= FALSE;
static IPC_Auth* 	ipc_auth 		= NULL;
extern int	 	debug_level	       ;
static int 		stonithd_child_count	= 0;

/* Returns the GCHSource object associated with the given callback channel, 
 * or NULL if no such association is found. NOTE: _ch_ is evaluated twice.
 */
#define CALLBACK_CHANNEL_GCHSOURCE(ch) ( \
		((cbch_gsource_pairs != NULL) && ((ch) != NULL)) ? \
		g_hash_table_lookup(cbch_gsource_pairs, (ch)) : NULL )

/* Returns a truth value if the given channel is a callback channel AND 
 * is being polled in g_main_loop. NOTE: _ch_ is evaluated twice.
 */
#define IS_POLLED_CALLBACK_CHANNEL(ch) (CALLBACK_CHANNEL_GCHSOURCE(ch) != NULL)

#define LOG_FAILED_TO_GET_FIELD(field)					\
			stonithd_log(LOG_ERR				\
			,	"%s:%d: cannot get field %s from message." \
			,__FUNCTION__,__LINE__,field)

/* Right now there's only one stonith type which is allowed as a
 * last gasp measure
 */
#define lastgasp_stonith(s) \
	(!strcmp(s,"suicide") || !strcmp(s,"null"))

#define st_get_int_value(msg,fld,i) do { \
	if (HA_OK != ha_msg_value_int(msg,fld,i)) { \
		LOG_FAILED_TO_GET_FIELD(fld); \
		rc = ST_FAIL; \
	} \
} while(0)
#define st_save_string(msg,fld,v) do { \
	const char *tmp; \
	tmp = cl_get_string(msg,fld); \
	if (!tmp) { \
		LOG_FAILED_TO_GET_FIELD(fld); \
		rc = ST_FAIL; \
	} else { \
		v = g_strdup(tmp); \
	} \
} while(0)
#define st_get_string(msg,fld,v) do { \
	v = cl_get_string(msg,fld); \
	if (!v) { \
		LOG_FAILED_TO_GET_FIELD(fld); \
		rc = ST_FAIL; \
	} \
} while(0)
#define st_get_hashtable(msg,fld,v) do { \
	v = cl_get_hashtable(msg,fld); \
	if (!v) { \
		LOG_FAILED_TO_GET_FIELD(fld); \
		rc = ST_FAIL; \
	} \
} while(0)

#define return_on_msg_from_us(type) do { \
	if ( !strncmp(from, local_nodename, MAXCMP) && !TEST ) { \
		stonithd_log(LOG_DEBUG, "received a " #type \
			"msg from myself, ignoring"); \
		return; \
	} \
} while(0)

int
main(int argc, char ** argv)
{
	int main_rc = LSB_EXIT_OK;
	int option_char;

	crm_system_name = stonithd_name;
        cl_log_set_entity(stonithd_name);
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(HA_LOG_FACILITY);
        /* Use logd if it's enabled by heartbeat */
        cl_inherit_logging_environment(0);

	do {
		option_char = getopt(argc, argv, optstr);

		if (option_char == -1) {
			break;
		}

		switch (option_char) {
			case 'a': /* Start up alone */
				STARTUP_ALONE = TRUE;
				break;

			case 'r': /* Register to apphbd */
#if SUPPORT_HEARTBEAT
				NEED_SIGNON_TO_APPHBD = TRUE;
#else
				printf("Sorry, Heartbeat support"
				" not included, can't use apphb.");
				return (STARTUP_ALONE == TRUE) ? 
					LSB_EXIT_EINVAL : MAGIC_EC;
#endif
				break;

			 /* Get the interval to emitting the application 
			    hearbteat to apphbd.
			  */
			case 'i':
#if SUPPORT_HEARTBEAT
				if (optarg) {
					APPHB_INTERVAL = atoi(optarg);		
					APPHB_WARNTIME = 3*APPHB_INTERVAL;
				}
#else
				printf("Sorry, Heartbeat support"
				" not included, can't use apphb.");
				return (STARTUP_ALONE == TRUE) ? 
					LSB_EXIT_EINVAL : MAGIC_EC;
#endif
				break;
			
			
			case 's': /* Show daemon status */
				return show_daemon_status(STD_PIDFILE);

			case 'k': /* kill the running daemon */
				return(kill_running_daemon(STD_PIDFILE));

			case 'v': /* Run with debug mode */
				debug_level++;
				/* adjust the PILs' debug level */ 
				PILpisysSetDebugLevel(7);
				break;

			case 'h':
				printf("%s\n",simple_help_screen);
				return (STARTUP_ALONE == TRUE) ? 
					LSB_EXIT_OK : MAGIC_EC;

			case 't':
				/* For test only, when the node handle the 
				 * message sent from itself.
				 */
				TEST = TRUE;	
				break;

			default:
				stonithd_log(LOG_ERR, "getopt returned"
					" character code %c", option_char);
				printf("%s\n", simple_help_screen);
				return (STARTUP_ALONE == TRUE) ? 
					LSB_EXIT_EINVAL : MAGIC_EC;
		}
	} while (1);

	cl_inherit_logging_environment(0);

	if (cl_read_pidfile(STD_PIDFILE) > 0 ) {
		stonithd_log(LOG_NOTICE, "%s %s", argv[0], M_RUNNING);
		return (STARTUP_ALONE == TRUE) ? LSB_EXIT_OK : MAGIC_EC;
	}

	/* Not use daemon() API, since it's not POSIX compliant */
	become_daemon();

	if( !STARTUP_ALONE ) {
		void *dispatch = NULL;
		void *destroy = NULL;
	    
	if(is_openais_cluster()) {
#if SUPPORT_AIS
		dispatch = stonithd_ais_dispatch;
		destroy = stonithd_ais_destroy;
#endif
	} else if(is_heartbeat_cluster()) {
#if SUPPORT_HEARTBEAT
		dispatch = stonithd_hb_callback;
		destroy = stonithd_hb_connection_destroy;
#endif
	}

	if (crm_cluster_connect(&local_nodename, NULL, dispatch, destroy,
#if SUPPORT_HEARTBEAT
				   &hb
#else
				   NULL
#endif
		) == FALSE) {
			stonithd_log(LOG_ERR, "failed to connect to cluster");
			stonithd_log(LOG_ERR, "%s %s", argv[0], M_ABORT);
			return (STARTUP_ALONE == TRUE) ? LSB_EXIT_GENERIC : MAGIC_EC;
		}
	}

	mainloop = g_main_new(FALSE);

	if( is_heartbeat_cluster()) {
#if SUPPORT_HEARTBEAT
	/*
	 * Initialize the handler of IPC messages from heartbeat, including
	 * the messages produced by myself as a client of heartbeat.
	 */
	if( !STARTUP_ALONE ) {
	if ( (main_rc = init_hb_msg_handler()) != 0) {
		goto signoff_quit;
	}
	}
#endif
	}

	/*
	 * Initialize the handler of IPC messages from my clients.
	 */
	if ( (main_rc = init_client_API_handler()) != 0) {
		goto delhb_quit;
	}

	if( is_heartbeat_cluster()) {
#if SUPPORT_HEARTBEAT
	if (NEED_SIGNON_TO_APPHBD == TRUE) {
		if ( (main_rc=init_using_apphb()) != 0 ) {
			stonithd_log(LOG_ERR, "An error in init_using_apphb");
			goto signoff_quit;
		} else {
			SIGNONED_TO_APPHBD = TRUE;
		}
	}
#endif
	}

	/* For tracking and remove the g_sources of messaging IPC channels */
	chan_gsource_pairs = g_hash_table_new(g_direct_hash, g_direct_equal);

	/* For tracking and remove the g_sources of callback IPC channels */
	cbch_gsource_pairs = g_hash_table_new(g_direct_hash, g_direct_equal);

	/* Initialize some global variables */
	executing_queue = g_hash_table_new_full(g_int_hash, g_int_equal,
						 destroy_key_of_op_htable,
						 free_common_op_t);

	/* The following line is only for CTS test with APITEST */
	reboot_blocked_table = g_hash_table_new_full(g_str_hash, g_str_equal
						, g_free, free_timer);

	if( is_heartbeat_cluster()) {
#if SUPPORT_HEARTBEAT
	/* To avoid the warning message when app_interval is very small. */
	MY_APPHB_HB();
#endif
	}

	/* drop_privs(0, 0); */  /* very important. cpu limit */
	stonithd_log2(LOG_DEBUG, "Enter g_mainloop\n");
	stonithd_log(LOG_NOTICE, "%s %s", argv[0], M_STARTUP );

	cl_set_all_coredump_signal_handlers(); 
	/* set larger maxdispatchtime */
	set_sigchld_proctrack(G_PRIORITY_HIGH,10*DEFAULT_MAXDISPATCHTIME);
	drop_privs(0, 0); /* become "nobody" */
	g_main_run(mainloop);
	return_to_orig_privs();

	if( is_heartbeat_cluster()) {
#if SUPPORT_HEARTBEAT
	MY_APPHB_HB();
	if (SIGNONED_TO_APPHBD == TRUE) {
		apphb_unregister();
		SIGNONED_TO_APPHBD = FALSE;
	}
#endif
	}

#if SUPPORT_HEARTBEAT
signoff_quit:
	if( is_heartbeat_cluster()) {
		if (hb != NULL && hb->llc_ops->signoff(hb, FALSE) != HA_OK) {
			stonithd_log(LOG_ERR, "Cannot sign off from heartbeat.");
			stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			main_rc = LSB_EXIT_GENERIC;
		}
	}
#endif

delhb_quit:
#if SUPPORT_HEARTBEAT
	if( is_heartbeat_cluster()) {
		if (hb != NULL) {
			if (hb->llc_ops->delete(hb) != HA_OK) {
				stonithd_log(LOG_ERR, "Cannot delete API object.");
				stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
				main_rc = LSB_EXIT_GENERIC;
			}
		}
	}
#endif

	if (client_list != NULL) {
		g_list_foreach(client_list, free_client, NULL);
		g_list_free(client_list);
	}

	g_hash_table_destroy(executing_queue); 
	if ( NULL != ipc_auth ) {
		g_hash_table_destroy(ipc_auth->uid);
		free(ipc_auth); 
	}
	
	if (cl_unlock_pidfile(PID_FILE) != 0) {
                stonithd_log(LOG_ERR, "it failed to remove pidfile %s.", STD_PIDFILE);
		main_rc = LSB_EXIT_GENERIC;
        }

	if (0 == main_rc) {
		stonithd_log(LOG_NOTICE, "%s %s", argv[0], M_QUIT );
	} else {
		stonithd_log(LOG_NOTICE, "%s %s", argv[0], M_ABORT );
	}

	return (STARTUP_ALONE == TRUE) ? main_rc : MAGIC_EC;
}

static gboolean
check_memory(gpointer unused)
{
	cl_realtime_malloc_check();
	return TRUE;
}
/* 
 * Notes: 
 * 1) Not use daemon() API for its portibility, any comment?
*/
static void
become_daemon()
{
	pid_t pid;
	int j;

	if (STARTUP_ALONE == TRUE) {
		pid = fork();

		if (pid < 0) { /* in parent process and fork failed. */
			stonithd_log(LOG_ERR, 
				     "become_daemon: forking a child failed.");
			stonithd_log(LOG_ERR, 
				     "exit due to not becoming a daemon.");
			exit(LSB_EXIT_GENERIC);
		} else if (pid > 0) {  /* in parent process and fork is ok */
			exit(LSB_EXIT_OK);
		}
	}

	umask(022);
	setsid();
	cl_cdtocoredir();
	cl_enable_coredumps(TRUE);

	for (j=0; j < 3; ++j) {
		close(j);
		(void)open("/dev/null", j == 0 ? O_RDONLY : O_WRONLY);
	}

	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGUSR1, 
		 	adjust_debug_level, NULL, NULL);
	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGUSR2, 
		 	adjust_debug_level, NULL, NULL);

	cl_signal_set_interrupt(SIGUSR1, FALSE);
	cl_signal_set_interrupt(SIGUSR2, FALSE);

	CL_IGNORE_SIG(SIGINT);
	cl_signal_set_interrupt(SIGINT, FALSE);
	CL_IGNORE_SIG(SIGHUP);
	cl_signal_set_interrupt(SIGHUP, FALSE);
	CL_SIGNAL(SIGTERM, stonithd_quit);
	cl_signal_set_interrupt(SIGTERM, TRUE);
	cl_signal_set_interrupt(SIGCHLD, TRUE);

	/* Temporarily donnot abort even failed to create the pidfile according
	 * to Andrew's suggestion. In the future will disable pidfile functions
	 * when started up by heartbeat.
	 */
	if (cl_lock_pidfile(STD_PIDFILE) < 0) {
		stonithd_log(LOG_ERR, "%s did not %s, although failed to lock the"
			     "pid file.", stonithd_name, M_ABORT);
	}

	cl_make_realtime(SCHED_OTHER, 0, 32, 128);
	Gmain_timeout_add(60*1000, check_memory, NULL);
}

static void
stonithd_quit(int signo)
{
	if (mainloop != NULL && g_main_is_running(mainloop)) {
		DisableProcLogging();
		g_main_quit(mainloop);
	} else {
		/*apphb_unregister();*/
		DisableProcLogging();
		exit((STARTUP_ALONE == TRUE) ? LSB_EXIT_OK : MAGIC_EC);
	}
}

static gboolean
valid_op(common_op_t *op)
{
	gboolean rc = TRUE;

	if (op == NULL) {
		stonithd_log(LOG_ERR, "received child quit signal: "
			"but op==NULL");
		rc = FALSE;
	}
	switch( op->scenario ) {
	case STONITH_RA_OP:
		if (op->op_union.ra_op == NULL) {
			stonithd_log(LOG_ERR, "op->op_union.ra_op == NULL");
			rc = FALSE;
		}
		if (op->result_receiver == NULL) {
			stonithd_log(LOG_ERR, "op->result_receiver == NULL");
			rc = FALSE;
		}
		break;
	case STONITH_INIT:
	case STONITH_REQ:
		if (op->op_union.st_op == NULL) {
			stonithd_log(LOG_ERR, "op->op_union.st_op == NULL");
			rc = FALSE;
		}
		if (op->result_receiver == NULL ) {
			stonithd_log(LOG_ERR, "op->result_receiver == NULL");
			rc = FALSE;
		}
		break;
	default:
		stonithd_log(LOG_ERR, "unsupported operation scenario");
		rc = FALSE;
	}
	return rc;
}

static void
handleRA_finished_op(common_op_t *op, pid_t pid, int exitcode)
{
	op->op_union.ra_op->call_id = pid;
	op->op_union.ra_op->op_result = exitcode;
	send_stonithRAop_final_result(op->op_union.ra_op, 
			      op->result_receiver);
	if( !strcmp(op->op_union.ra_op->op_type, "start") ) {
		record_new_srsc(op->op_union.ra_op);
	}
	g_hash_table_remove(executing_queue, &pid);
}

static void
handle_finished_op(common_op_t *op, pid_t pid, int exitcode)
{
	if (exitcode == S_OK) {
		op->op_union.st_op->op_result = STONITH_SUCCEEDED;
		op->op_union.st_op->node_list =
			g_string_append(op->op_union.st_op->node_list,local_nodename);
		send_stonithop_final_result(op);
		g_hash_table_remove(executing_queue, &pid);
		return;
	}
	/* Go ahead when exitcode != S_OK */
	stonithd_log(LOG_INFO, "failed to STONITH node %s with " 
		"local device %s (exitcode %d), gonna try the "
		"next local device"
		,	op->op_union.st_op->node_name, op->rsc_id, exitcode); 
	if (ST_OK == continue_local_stonithop(pid)) {
		return;
	}
	stonithd_log(LOG_DEBUG, "failed to STONITH node %s "
		"locally", op->op_union.st_op->node_name);
	/* The next statement is just for debugging */
	if (op->scenario == STONITH_INIT) {
		stonithd_log(LOG_DEBUG, "Will ask other nodes "
			"to help STONITH node %s."
			,	op->op_union.st_op->node_name); 
	}
	if (changeto_remote_stonithop(pid) != ST_OK) {
		op->op_union.st_op->op_result = STONITH_GENERIC;
		send_stonithop_final_result(op);
		g_hash_table_remove(executing_queue, &pid);
	}
}

static void
stonithdProcessDied(ProcTrack* p, int status, int signo
		    , int exitcode, int waslogged)
{
	const char * pname = p->ops->proctype(p);
	gboolean rc;
	common_op_t * op = NULL;
	int * original_key = NULL;

	stonithd_log2(LOG_DEBUG, "stonithdProcessDied: begin"); 
	stonithd_child_count--;
	stonithd_log2(LOG_DEBUG, "there still are %d child process running"
			, stonithd_child_count);
	stonithd_log(LOG_DEBUG, "Child process %s [%d] exited, its exit code:"
		     " %d when signo=%d.", pname,
		     proctrack_pid(p), exitcode, signo);

	rc = g_hash_table_lookup_extended(executing_queue, &(p->pid) 
			, (gpointer *)&original_key, (gpointer *)&op);
	if (rc == FALSE) {
		stonithd_log(LOG_WARNING, "child exits, but not tracked.");
	}
	else if( signo ) {
		if( proctrack_timedout(p) ) {
			stonithd_log(LOG_WARNING,
				"A STONITH operation timed out."); 
		}
	}
	else if( valid_op(op) ) {
		if( op->scenario == STONITH_RA_OP ) {
			handleRA_finished_op(op, proctrack_pid(p), exitcode);
		} else {
			handle_finished_op(op, proctrack_pid(p), exitcode);
		}
	}
	g_free(p->privatedata);
	reset_proctrack_data(p);
}

static void
stonithdProcessRegistered(ProcTrack* p)
{
	stonithd_child_count++;
	stonithd_log2(LOG_DEBUG, "Child process [%s] started [ pid: %d ]."
		     , p->ops->proctype(p), proctrack_pid(p));
	stonithd_log2(LOG_DEBUG, "there are %d child process running"
			, stonithd_child_count);
}

static const char * 
stonithdProcessName(ProcTrack* p)
{
	gchar * process_name = proctrack_data(p);
	stonithd_log2(LOG_DEBUG, "process name: %s", process_name);
	return  process_name;
}

#if SUPPORT_HEARTBEAT

#define set_msg_handler(type, handler) do { \
	if (hb->llc_ops->set_msg_callback(hb, type, \
				  handler, hb) != HA_OK) { \
		stonithd_log(LOG_ERR, "Cannot set msg " #type " callback"); \
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb)); \
		return LSB_EXIT_GENERIC; \
	} \
	} while(0)

static int
init_hb_msg_handler(void)
{
	unsigned int msg_mask;
	
	if (hb == NULL) {
		stonithd_log(LOG_ERR, "%s:%d: not connected to heartbeat"
			, __FUNCTION__, __LINE__);
		return LSB_EXIT_GENERIC;	
	}

	set_msg_handler(T_WHOCANST, handle_msg_twhocan);
	set_msg_handler(T_ICANST, handle_msg_ticanst);
	set_msg_handler(T_STIT, handle_msg_tstit);
	set_msg_handler(T_RSTIT, handle_msg_trstit);
	set_msg_handler(T_RESETTED, handle_msg_resetted);

	msg_mask = LLC_FILTER_DEFAULT;
	stonithd_log(LOG_DEBUG, "Setting message filter mode");
	if (hb->llc_ops->setfmode(hb, msg_mask) != HA_OK) {
		stonithd_log(LOG_ERR, "Cannot set filter mode");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return LSB_EXIT_GENERIC;
	}

	return 0;
}
#endif

static void
stonithd_hb_callback(struct ha_msg* msg, void* private_data)
{
	int i;
	const char *st_op_type = cl_get_string(msg, F_STONITHD_OP);

	if (!st_op_type) {
		stonithd_log(LOG_ERR, "%s:%d: empty %s field, can't proceed"
			, __FUNCTION__, __LINE__, F_STONITHD_OP);
		return;
	}
	for (i=0; i<DIMOF(clu_msg_to_handlers); i++) {
		if (!strncmp(st_op_type, clu_msg_to_handlers[i].msg_type, MAXCMP)) {
			/*call the handler of the message*/
			clu_msg_to_handlers[i].handler(msg, private_data);
			break;
		}
	}
	if (i == DIMOF(clu_msg_to_handlers)) {
		stonithd_log(LOG_ERR, "%s:%d: received an unknown "
			"stonith operation: %s"
			, __FUNCTION__, __LINE__, st_op_type);
	}
}
static void
stonithd_hb_connection_destroy(void* private_data)
{
	return;
}

#if SUPPORT_AIS	
static gboolean
stonithd_ais_dispatch(AIS_Message *wrapper, char *data, int sender) 
{
	struct ha_msg* msg;

	stonithd_log2(LOG_DEBUG,"%s:%d: Message received: '%d:%.80s'", 
		__FUNCTION__, __LINE__, wrapper->id, data);

	msg = ais_msg2ha_msg(data);
	if(msg == NULL) {
		goto bail;
	}
	stonithd_hb_callback(msg, NULL);

	ZAPMSG(msg);
	return TRUE;

	bail:
	stonithd_log(LOG_ERR, "%s:%d: bad msg: |%s|"
		, __FUNCTION__, __LINE__, data);
	return TRUE;
}

#define AIS_TAG "ais_msg"
#define skipwhite(p) while(*p && isspace(*p)) p++
#define skipnonwhite(p) while(*p && !isspace(*p)) p++
#define savestrn(d,p,len) do { \
	if( !(d = malloc(len+1)) ) { \
		stonithd_log(LOG_ERR, "out of memory"); \
	} else { \
		strncpy(d,p,len); \
		*(d+(len)) = '\0'; \
	} \
} while(0)

/* store next XML attribute as a field in msg */
static int
attr2fld(char *input, struct ha_msg *msg)
{
	char *p = input, *q;
	char *attr=NULL, *val=NULL;

	skipwhite(p);
	if (!(q = strstr(p,"=\"")))
		goto err;
	savestrn(attr,p,q-p);

	p = q+strlen("=\""); /* start of the value string */
	for( q=p; *q && *q != '"'; q++ ) {
		if( *q == '\\' )
			q++;
	}
	if( !*q ) /* premature end-of-string? */
		goto err;
	savestrn(val,p,q-p);
	if( !attr || !val )
		goto err;
	q++; /* move beyond the quote */

	if ((ha_msg_add(msg, attr, val) != HA_OK)) {
		stonithd_log(LOG_ERR, "%s:%d: cannot add field."
			, __FUNCTION__, __LINE__);
		goto err;
	}
	free(attr);
	free(val);
	return q-input;

err:
	free(attr);
	free(val);
	return 0;
}

static struct ha_msg*
ais_msg2ha_msg(char *input)
{
	char *p = input;
	int l;
	struct ha_msg *msg = NULL;

	if (*p != '<') {
		stonithd_log(LOG_ERR, "%s:%d: unexpected start of message: |%s|"
			, __FUNCTION__, __LINE__, p);
		goto err;
	}
	skipnonwhite(p);
	if ((msg = ha_msg_new(1)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d: out of memory"
			, __FUNCTION__, __LINE__);
		goto err;
	}
	while( *p && *p != '/' ) {
		if( !(l = attr2fld(p,msg)) ) {
			stonithd_log(LOG_ERR, "%s:%d: bad format: |%s|"
				, __FUNCTION__, __LINE__, p);
			goto err;
		}
		p += l;
	}
	if (strncmp(p,"/>",2)) {
		stonithd_log(LOG_ERR, "%s:%d: bad format: |%s|"
			, __FUNCTION__, __LINE__, p);
		goto err;
	}
	return msg;
	
err:
	if( msg ) {
		ZAPMSG(msg);
	}
	return NULL;
}

static void
stonithd_ais_destroy(gpointer user_data)
{
	stonithd_log(LOG_ERR, "AIS connection terminated");
	ais_fd_sync = -1;
	exit(1);
}
#endif

static gboolean
stonithd_sendmsg(const char *node_name, struct ha_msg *msg, const char *st_op_type)
{
	if(is_openais_cluster()) {
		if ((ha_msg_add(msg, F_TYPE, crm_system_name) != HA_OK)
		||(ha_msg_add(msg, F_STONITHD_OP, st_op_type) != HA_OK)) {
			stonithd_log(LOG_ERR, "%s:%d: cannot add field."
				, __FUNCTION__, __LINE__);
			return FALSE;
		}
	} else if(is_heartbeat_cluster()) {
		if ((ha_msg_add(msg, F_TYPE, st_op_type) != HA_OK)) {
			stonithd_log(LOG_ERR, "%s:%d: cannot add field."
				, __FUNCTION__, __LINE__);
			return FALSE;
		}
	} else {
		stonithd_log(LOG_ERR, "%s:%d: not connected to the cluster"
			, __FUNCTION__, __LINE__);
		return FALSE;
	}
	if ((ha_msg_add(msg, F_ORIG, local_nodename) != HA_OK)) {
		stonithd_log(LOG_ERR, "%s:%d: cannot add field."
			, __FUNCTION__, __LINE__);
		return FALSE;
	}
	return send_cluster_message(node_name, crm_msg_stonithd, msg, FALSE);
}

static void
handle_msg_twhocan(struct ha_msg* msg, void* private_data)
{
	struct ha_msg * reply;
	const char * target = NULL;
	const char * from = NULL;
	int call_id;
	int rc = ST_OK;

	stonithd_log(LOG_DEBUG, "I got T_WHOCANST msg.");	
	st_get_string(msg, F_ORIG, from);
	st_get_string(msg, F_STONITHD_NODE, target);
	st_get_int_value(msg, F_STONITHD_CALLID, &call_id);
	if( rc != ST_OK ) { /* didn't get all fields */
		return;
	}
	return_on_msg_from_us(T_WHOCANST);
	if( !get_local_stonithobj_can_stonith(target, NULL) ) {
		stonithd_log(LOG_DEBUG, "handle_msg_twhocan: I cannot stonith "
			     "node %s.", target);
		return;
	}
	if ((reply = ha_msg_new(1)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d: out of memory"
			, __FUNCTION__, __LINE__);
		return;
	}
	if ((ha_msg_add_int(reply, F_STONITHD_CALLID, call_id) != HA_OK)) {
		stonithd_log(LOG_ERR, "%s:%d: cannot add field."
			, __FUNCTION__, __LINE__);
		ZAPMSG(reply);
		return;
	}
	if (!stonithd_sendmsg(from, reply, T_ICANST)) {
		ZAPMSG(reply);
		return;
	}
	stonithd_log(LOG_DEBUG, "handle_msg_twhocan: replied that"
		" we can stonith node %s", target);
	ZAPMSG(reply);
}

static void
handle_msg_ticanst(struct ha_msg* msg, void* private_data)
{
	const char * from = NULL;
	int  call_id;
	int * orig_key = NULL;
	common_op_t * op = NULL;
	int rc = ST_OK;

	stonithd_log(LOG_DEBUG, "handle_msg_ticanst: got T_ICANST msg.");
	st_get_string(msg, F_ORIG, from);
	st_get_int_value(msg, F_STONITHD_CALLID, &call_id);
	if( rc != ST_OK ) { /* didn't get all fields */
		return;
	}
	return_on_msg_from_us(T_ICANST);
	my_hash_table_find(executing_queue, (gpointer *)&orig_key, 
			   (gpointer *)&op, &call_id);
	if ( op != NULL &&  /* QUERY only */
	    (op->scenario == STONITH_INIT || op->scenario == STONITH_REQ)) {
		/* add the separator blank space */
		op->op_union.st_op->node_list = 
			g_string_append(op->op_union.st_op->node_list, " ");
		op->op_union.st_op->node_list = 
			g_string_append(op->op_union.st_op->node_list, from);
		stonithd_log(LOG_DEBUG, "handle_msg_ticanst: QUERY operation "
			"(call_id=%d): added a node's name who can stonith "
			"another node.", call_id);
		stonithd_log(LOG_INFO, "Node %s can fence node %s."
			, from, op->op_union.st_op->node_name);
	}
	/* wait the timeout function to send back the result */
}

static void
handle_msg_tstit(struct ha_msg* msg, void* private_data)
{
	const char * target = NULL;
	const char * from = NULL;
	stonith_rsc_t * srsc = NULL;
	stonith_ops_t * st_op = NULL;
	int rc = ST_OK;

	stonithd_log(LOG_DEBUG, "handle_msg_tstit: got T_STIT msg.");	
	st_get_string(msg, F_ORIG, from);
	st_get_string(msg, F_STONITHD_NODE, target);
	if( rc != ST_OK ) { /* didn't get all fields */
		return;
	}
	return_on_msg_from_us(T_STIT);

	srsc = get_local_stonithobj_can_stonith(target, NULL);
	if( !srsc ) {
		return;
	}
	if ( !(st_op = new_stonith_ops_t(msg)) ) {
		stonithd_log(LOG_ERR, "%s:%d: %s"
			, __FUNCTION__, __LINE__
			, "failed to create a stonith_op.");
		return;
	}
	if ( !st_op->call_id ) {
		stonithd_log(LOG_ERR, "%s:%d: %s"
			, __FUNCTION__, __LINE__
			, "No F_STONITHD_CALLID field.");
		free_stonith_ops_t(st_op);
		return;
	}

	if (ST_OK == require_local_stonithop(st_op, srsc, from)) {
		stonithd_log(LOG_INFO, "Node %s try to help node %s to "
			"fence node %s.", local_nodename, from, target);
	}

	free_stonith_ops_t(st_op);
	st_op = NULL;
}

/* side effect: records a timer_id in the op */
static void
insert_into_executing_queue(common_op_t *op, int call_id)
{
	int *tmp_callid;

	tmp_callid = g_new(int, 1);
	*tmp_callid = call_id;
	g_hash_table_insert(executing_queue, tmp_callid, op);
	tmp_callid = g_new(int, 1);
	*tmp_callid = call_id;
	op->timer_id = Gmain_timeout_add_full(G_PRIORITY_HIGH_IDLE
				, op->op_union.st_op->timeout, stonithop_timeout
				, tmp_callid, timeout_destroy_notify);
	stonithd_log(LOG_DEBUG, "inserted optype=%s, key=%d",
			stonith_op_strname[op->op_union.st_op->optype], call_id);
}

static int
require_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc,
			const char * asker)
{
	int child_id;
	common_op_t * op;

	/* in case we are shooting ourselves, assume that we'll
	 * succeed and send success result to the initiator
	 */
	if( st_op->optype != QUERY && !strcmp(st_op->node_name,local_nodename) ) {
		st_op->op_result = STONITH_SUCCEEDED;
		st_op->node_list = g_string_append(st_op->node_list,local_nodename);
		stonithop_result_to_other_node(st_op, asker);
	}

	if ((child_id = stonith_operate_locally(st_op, srsc)) <= 0) {
		stonithd_log(LOG_ERR, "require_local_stonithop: "
			"stonith_operate_locally failed.");
		return ST_FAIL;
	}

	op = g_new0(common_op_t, 1);
	op->scenario = STONITH_REQ;
	op->result_receiver = g_strdup(asker);
	op->rsc_id = g_strdup(srsc->rsc_id);
	op->op_union.st_op = dup_stonith_ops_t(st_op);

	insert_into_executing_queue(op,child_id);
	return ST_OK;
}

static void
handle_msg_trstit(struct ha_msg* msg, void* private_data)
{
	const char * from = NULL;
	int call_id;
	int op_result;
	int * orig_key = NULL;
	common_op_t * op = NULL;
	int rc = ST_OK;

	stonithd_log(LOG_DEBUG, "handle_msg_trstit: got T_RSTIT msg");	
	st_get_string(msg, F_ORIG, from);
	st_get_int_value(msg, F_STONITHD_CALLID, &call_id);
	st_get_int_value(msg, F_STONITHD_FRC, &op_result);
	if( rc != ST_OK ) { /* didn't get all fields */
		return;
	}
	return_on_msg_from_us(T_RSTIT);

	stonithd_log(LOG_DEBUG, "this T_RSTIT message is from %s", from);	
	my_hash_table_find(executing_queue, (gpointer *)&orig_key, 
			   (gpointer *)&op, &call_id);
	if ( !op || 
	    (op->scenario != STONITH_INIT && op->scenario != STONITH_REQ)) {
		stonithd_log(LOG_DEBUG, "handle_msg_trstit: the stonith "
			"operation (call_id=%d) has finished before "
			"receiving this message", call_id);
		return;
	}
	op->op_union.st_op->op_result = (stonith_ret_t)op_result;
	op->op_union.st_op->node_list = 
		g_string_append(op->op_union.st_op->node_list, from);
	send_stonithop_final_result(op);
	stonithd_log(LOG_INFO, "Node %s fenced node %s: result=%s."
		, from, op->op_union.st_op->node_name
		, stonith_op_result_strname[op_result]);
	g_hash_table_remove(executing_queue, orig_key);
}

static int
broadcast_reset_success(const char * target)
{
	struct ha_msg * msg;

	if ((msg = ha_msg_new(3)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d: out of memory"
				, __FUNCTION__, __LINE__ );
		return ST_FAIL;
	}
	stonithd_log(LOG_DEBUG, "%s: Broadcast the reset success message to "
		"the whole cluster.", __FUNCTION__);
	if ((ha_msg_add(msg, F_STONITHD_NODE, target) != HA_OK)) {
		stonithd_log(LOG_ERR, "%s:%d:cannot add field."
				, __FUNCTION__, __LINE__);
		ZAPMSG(msg);
		return ST_FAIL;
	}
	if (!stonithd_sendmsg(NULL, msg, T_RESETTED)) {
		ZAPMSG(msg);
		return ST_FAIL;
	}
	ZAPMSG(msg);
	stonithd_log2(LOG_DEBUG,"%s: end.", __FUNCTION__);
	return ST_OK;	
}

static void
handle_msg_resetted(struct ha_msg* msg, void* private_data)
{
	const char * from = NULL;
	const char * target = NULL;
	int timer_id = -1;
	int rc = ST_OK;

	stonithd_log(LOG_DEBUG, "%s: begin", __FUNCTION__)

	st_get_string(msg, F_ORIG, from);
	st_get_string(msg, F_STONITHD_NODE, target);
	if( rc != ST_OK ) { /* didn't get all fields */
		return;
	}

	stonithd_log(LOG_DEBUG, "Got a notification of successfully resetting"
		" node %s from node %s with APITET.", target, from);	
	/* The timeout value equals 90 seconds now */
	timer_id = Gmain_timeout_add_full(G_PRIORITY_HIGH_IDLE
			, REBOOT_BLOCK_TIMEOUT
			, reboot_block_timeout, g_strdup(target)
			, timerid_destroy_notify);

	g_hash_table_replace(reboot_blocked_table, g_strdup(target)
				, g_memdup(&timer_id, sizeof(timer_id)));

	stonithd_log2(LOG_DEBUG, "handle_msg_trstit: end");	
}

static void
timerid_destroy_notify(gpointer data)
{
	gchar * target = (gchar *)data;

	if (target != NULL) {
		g_free(target);	
	}
}

static void
free_timer(gpointer data)
{
	int * timer_id = (int *) data;

	if (NULL == timer_id) {
		stonithd_log(LOG_WARNING, "%s:%d: No invalid timer ID."
			     , __FUNCTION__, __LINE__);
		return;
	}

	Gmain_timeout_remove(*timer_id);
	g_free(timer_id);
}

static gboolean 
reboot_block_timeout(gpointer data)
{
	gchar * target = (gchar *)data;

	if (NULL != 
		g_hash_table_lookup(reboot_blocked_table, target) ) {
		g_hash_table_remove(reboot_blocked_table, target);	
		stonithd_log(LOG_INFO, "unblock the reboot to node %s", target);
	} else {
		stonithd_log(LOG_WARNING, "node %s already unblocked" , target);
	}

	return FALSE;
}

static int 
init_client_API_handler(void)
{
	GHashTable*		chanattrs;
	IPC_WaitConnection*	apichan = NULL;
	IPC_WaitConnection*	cbchan = NULL;
	char			path[] = IPC_PATH_ATTR;
	char			sock[] = STONITHD_SOCK;
	char 			cbsock[] = STONITHD_CALLBACK_SOCK;
	struct passwd*  	pw_entry;
	GWCSource		* api_source, * callback_source;
	GHashTable*     	uid_hashtable;
	int             	tmp = 1;

	stonithd_log2(LOG_DEBUG, "init_client_API_handler: begin");

	chanattrs = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(chanattrs, path, sock);
	apichan = ipc_wait_conn_constructor(IPC_DOMAIN_SOCKET, chanattrs);
	if (apichan == NULL) {
		stonithd_log(LOG_ERR, "Cannot open stonithd's api socket: %s",
				sock);
		g_hash_table_destroy(chanattrs);
		return LSB_EXIT_EPERM;
	}
	/* Need to destroy the item of the hash table chanattrs? yes, will be.
	 * Look likely there are many same  'memory leak's in linux-ha code.
	 */
	g_hash_table_destroy(chanattrs);

	/* Make up ipc auth struct. Now only allow the clients with uid=root or
	 * uid=hacluster to connect.
	 */
        uid_hashtable = g_hash_table_new(g_direct_hash, g_direct_equal);
        /* Add root's uid */
        g_hash_table_insert(uid_hashtable, GUINT_TO_POINTER(0), &tmp);

        pw_entry = getpwnam(HA_CCMUSER);
        if (pw_entry == NULL) {
                stonithd_log(LOG_ERR, "Cannot get the uid of HACCMUSER");
        } else {
                g_hash_table_insert(uid_hashtable, GUINT_TO_POINTER
				    (pw_entry->pw_uid), &tmp);
        }

        if ( NULL == (ipc_auth = MALLOCT(struct IPC_AUTH)) ) {
                stonithd_log(LOG_ERR, "init_client_API_handler: MALLOCT failed.");
		g_hash_table_destroy(uid_hashtable);
        } else {
                ipc_auth->uid = uid_hashtable;
                ipc_auth->gid = NULL;
        }

	/* When to destroy the api_source */
	stonithd_log(LOG_DEBUG, "apichan=%p", apichan);
	api_source = G_main_add_IPC_WaitConnection(G_PRIORITY_HIGH, apichan,
			ipc_auth, FALSE, accept_client_dispatch, NULL, NULL);

	if (api_source == NULL) {
		stonithd_log(LOG_DEBUG, "Cannot create API listening source of "
			"server side from IPC");
		return	LSB_EXIT_GENERIC;
	}

	stonithd_log2(LOG_DEBUG, "init_client_callback_handler: begin");

	chanattrs = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(chanattrs, path, cbsock);
	cbchan = ipc_wait_conn_constructor(IPC_DOMAIN_SOCKET, chanattrs);
	if (cbchan == NULL) {
		stonithd_log(LOG_ERR, "Cannot open stonithd's callback socket:"
				" %s", sock);
		g_hash_table_destroy(chanattrs);
		return LSB_EXIT_EPERM;
	}
	g_hash_table_destroy(chanattrs);

	stonithd_log(LOG_DEBUG, "callback_chan=%p", cbchan);
	callback_source = G_main_add_IPC_WaitConnection(G_PRIORITY_HIGH, cbchan
		     , NULL, FALSE, accept_client_connect_callback, NULL, NULL);

	if (callback_source == NULL) {
		stonithd_log(LOG_DEBUG, "Cannot create callback listening "
			     " source of server side from IPC");
		return	LSB_EXIT_GENERIC;
	}

	return 0;
}

static gboolean
accept_client_dispatch(IPC_Channel * ch, gpointer user)
{
	GCHSource * gsrc = NULL;	

	if (ch == NULL) {
		stonithd_log(LOG_ERR, "IPC accepting a connection failed.");
		return FALSE;
	}

	stonithd_log2(LOG_DEBUG, "IPC accepted a connection.");
	gsrc = G_main_add_IPC_Channel(G_PRIORITY_HIGH, ch, FALSE, 
			stonithd_client_dispatch, (gpointer)ch,
			stonithd_IPC_destroy_notify);
	g_hash_table_insert(chan_gsource_pairs, ch, gsrc);

	return TRUE;
}

static gboolean
accept_client_connect_callback(IPC_Channel * ch, gpointer user)
{
	struct ha_msg * reply = NULL;
	const char * api_reply = ST_APIOK;
	GCHSource *gsrc;

	stonithd_log2(LOG_DEBUG, "IPC accepted a callback connection.");

	if (ch == NULL) {
		stonithd_log(LOG_ERR, "%s:%d: ch==NULL."
			     , __FUNCTION__, __LINE__);
		return TRUE;
	}

	if (ch->ch_status == IPC_DISCONNECT) {
		stonithd_log(LOG_WARNING
			, "callback IPC disconnected with a client: ch=%p", ch);
		return TRUE;
	}

	/* Tell the client that we want its cookie to authenticate
	 * itself, and then poll this channel for the cookie. 
	 */
	api_reply = ST_COOKIE;

	/* Poll this channel for incoming requests from the client.
	 * The only request expected on the callback channel is
	 * ST_SIGNON with cookie provided.  
	 */
	gsrc = G_main_add_IPC_Channel(G_PRIORITY_HIGH, ch, FALSE, 
			stonithd_client_dispatch, (gpointer)ch,
			stonithd_IPC_destroy_notify);

	/* Insert the polled callback channel into the mapping table 
	 * so that we can track it. Be sure to remove the mapping 
	 * when the channel is destroyed.  
	 */
	g_hash_table_insert(cbch_gsource_pairs, ch, gsrc);

	if ((reply = ha_msg_new(3)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d:ha_msg_new:out of memory."
				,__FUNCTION__, __LINE__);
		return TRUE;
	}

	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RSIGNON) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK ) ) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "%s:%d: cannot add field."
			     , __FUNCTION__, __LINE__);
		return TRUE;
	}
	
	if (msg2ipcchan(reply, ch) != HA_OK) {
		stonithd_log(LOG_ERR
			    , "Failed to reply sign message to callback IPC");
	}

	ZAPMSG(reply);
	return TRUE;
}

static gboolean
stonithd_client_dispatch(IPC_Channel * ch, gpointer user_data)
{
	struct ha_msg *	msg = NULL;

	stonithd_log2(LOG_DEBUG, "stonithd_client_dispatch: begin");

	if (ch == NULL) {
		stonithd_log(LOG_ERR, "stonithd_client_dispatch: ch==NULL.");
		return TRUE;
	}

	while ( ch->ops->is_message_pending(ch))  {
		if (ch->ch_status == IPC_DISCONNECT) {
			stonithd_log2(LOG_DEBUG
				, "IPC disconnected with a client: ch=%p", ch);
			stonithd_log2(LOG_DEBUG, "stonithd_client_dispatch: "
				"delete a client due to IPC_DISCONNECT.");
			delete_client_by_chan(&client_list, ch);
			return FALSE;
		}

		/* Authority issue ? */
		if  ((msg = msgfromIPC_noauth(ch)) == NULL ) {
			stonithd_log(LOG_ERR
				    , "Failed when receiving IPC messages.");
			return FALSE;
		}
		stonithd_process_client_msg(msg, (gpointer)ch);
	}
			
	stonithd_log2(LOG_DEBUG, "stonithd_client_dispatch: end");
	return TRUE;
}

/**
 * This handler is invoked by g_main_loop when a polled channel (pointed 
 * to by _data_) is closed. It removes the client connected on the channel,
 * and then removes the channel's gsource object from the main loop. The 
 * channel object itself is not destroyed here.
 */
static void
stonithd_IPC_destroy_notify(gpointer data)
{
	IPC_Channel * ch = (IPC_Channel *) data;
	GCHSource * tmp_gsrc = NULL;

	/* deal with client disconnection event */
	stonithd_log2(LOG_DEBUG, "An IPC is destroyed.");

	if (ch == NULL) {
		stonithd_log2(LOG_DEBUG, "IPC_destroy_notify: ch==NULL");
		return;
	}

	if ( ST_OK == delete_client_by_chan(&client_list, ch) ) {
		stonithd_log2(LOG_DEBUG, "Delete a client from client_list "
			"in stonithd_IPC_destroy_notify.");
	} else {
		stonithd_log2(LOG_DEBUG, "stonithd_IPC_destroy_notify: Failed "
			"to delete a client from client_list, maybe it has "
			"been deleted in signoff function.");
	}

	if ((tmp_gsrc = g_hash_table_lookup(chan_gsource_pairs, ch))) {
		G_main_del_IPC_Channel(tmp_gsrc);
		g_hash_table_remove(chan_gsource_pairs, ch);
	} else if ((tmp_gsrc = g_hash_table_lookup(cbch_gsource_pairs, ch))) {
		G_main_del_IPC_Channel(tmp_gsrc);
		g_hash_table_remove(cbch_gsource_pairs, ch);
	} else {
		stonithd_log(LOG_NOTICE
			, "%s::%d: Don't find channel's chan_gsource_pairs."
			, __FUNCTION__, __LINE__);
	}
	
	/* Don't destroy ch, which should be done in clplumbing lib */	
}

static gboolean
stonithd_process_client_msg(struct ha_msg * msg, gpointer data)
{
	const char * msg_type = NULL;
	const char * api_type = NULL;
	IPC_Channel * ch = (IPC_Channel *) data;
	int i, rc = ST_OK;
	
	st_get_string(msg, F_STONITHD_TYPE, msg_type);
	st_get_string(msg, F_STONITHD_APIREQ, api_type);
	if ( rc != ST_OK ) {
		ZAPMSG(msg);
		return TRUE;
	}

	/* If this is an incoming message from a callback channel, it MUST
	 * be a SIGNON request with cookie provided. Handle it here so that
	 * the message handlers can safely assume that they are processing
	 * requests from the messaging channel.
	 */
	if (IS_POLLED_CALLBACK_CHANNEL(ch)) {
		if (strcmp(api_type, ST_SIGNON) != 0) {
			stonithd_log(LOG_ERR, "received a non-signon request via "
					"the callback channel");
			ZAPMSG(msg);
			return TRUE;
		}
		stonithd_log2(LOG_DEBUG, "stonithd_process_client_msg: received "
				"signon request from callback channel.");
		on_stonithd_cookie(msg, ch);
		ZAPMSG(msg);
		return TRUE;
	}

	stonithd_log2(LOG_DEBUG, "begin to dealing with a api msg %s from "
			"a client PID:%d.", api_type, ch->farside_pid);
	for (i=0; i<DIMOF(api_msg_to_handlers); i++) {
		if ( strncmp(api_type, api_msg_to_handlers[i].msg_type, MAXCMP)
			 == 0 ) {
			/*call the handler of the message*/
			rc = api_msg_to_handlers[i].handler(msg, ch);
			if (rc != ST_OK) {
				stonithd_log(LOG_WARNING, "There is something "
					"wrong when handling msg %s.", api_type);
			}
			break;
		}
	}
	
        if (i == DIMOF(api_msg_to_handlers)) {
                stonithd_log(LOG_ERR, "received an unknown api msg,"
				"and just abandon it.");
        }

        ZAPMSG(msg);
        stonithd_log2(LOG_DEBUG, "stonithd_process_client_msg: end.");

        return TRUE;
}

static int
on_stonithd_signon(struct ha_msg * request, gpointer data)
{
	struct ha_msg * reply;
	const char * api_reply = ST_APIOK;
	stonithd_client_t * client = NULL;
	int  tmpint;
	int rc = ST_OK;

	IPC_Channel * ch = (IPC_Channel *) data;

	stonithd_log2(LOG_DEBUG, "on_stonithd_signon: begin.");
	/* parameter check, maybe redundant */
	if ( ch == NULL || request == NULL ) {
		stonithd_log(LOG_ERR, "parameter error, signon failed.");
		return ST_FAIL;
	}

	if (HA_OK != ha_msg_value_int(request, F_STONITHD_CPID, &tmpint)) {
		stonithd_log(LOG_ERR, "signon msg contains no or incorrect "
				"PID field.");
		api_reply = ST_BADREQ;
		goto send_back_reply; 
	}

	/* Deal with the redundant signon by error */
	/* Is the IPC channel value not repeatable for our conditon? Likely */
	if ( (client = get_exist_client_by_chan(client_list, ch)) != NULL ) {
		if (client->pid == tmpint) {
			stonithd_log(LOG_NOTICE, "The client pid=%d re-signon "
				    "unnecessarily.", client->pid);
		} else {
			stonithd_log(LOG_NOTICE, "The client's channel isnot "
				     "correspond to the former pid(%d). It "
				     "seems a child is using its parent's "
				     "IPC channel.", client->pid);
		}
		goto send_back_reply;
	}

	/* initialize client data here */
	client = g_new0(stonithd_client_t, 1);
	client->pid = tmpint;
	client->ch = ch;
	client->cbch = NULL;
	client->removereason = NULL;

	st_get_int_value(request, F_STONITHD_CEUID, (int *)&client->uid);
	st_get_int_value(request, F_STONITHD_CEGID, (int *)&client->gid);
	st_save_string(request, F_STONITHD_CNAME, client->name);
	if( rc != ST_OK ) {
		api_reply = ST_BADREQ;
		free_client(client, NULL);
		client = NULL;
		goto send_back_reply; 
	}

	/* Generate a session cookie for the client so that it can authenticate
	 * itself when establishing the callback channel. Older stonithd clients
	 * safely ignores this field.
	 */
	cl_uuid_generate(&client->cookie);

	/* lack the authority check from uid&gid */
	/* add the client to client list */
 	if ( STRNCMP_CONST(api_reply, ST_APIOK) == 0 ) {
		client_list = g_list_append(client_list, client);
		stonithd_log(LOG_DEBUG,"client %s (pid=%d) succeeded to "
			"signon to stonithd.", client->name, client->pid);
	} else {
		stonithd_log(LOG_ERR, "signon failed.");
		free_client(client, NULL);
		client = NULL;
	}
	
send_back_reply:
	if ((reply = ha_msg_new(4)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d:ha_msg_new:out of memory."
				,__FUNCTION__, __LINE__);
		return ST_FAIL;
	}
	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RSIGNON) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK ) 
 	    ||(client && ha_msg_adduuid(reply, F_STONITHD_COOKIE, &client->cookie) != HA_OK)) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_signon: cannot add field.");
		return ST_FAIL;
	}
	
	if (msg2ipcchan(reply, ch) != HA_OK) { /* How to deal the error*/
		ZAPMSG(reply);
		stonithd_log(LOG_WARNING, "can't send signon reply message to IPC");
		return ST_FAIL;
	}

	ZAPMSG(reply);
	return ST_OK;
}

/**
 * Construct a message with the given fields and send it through the given 
 * ipc channel _ch_ (do not wait for the message to be fully transmitted).
 *
 * The fields of the message are passed as name, value pairs, terminated 
 * by a NULL name. A field with a NULL value is NOT added to the message.
 *
 * Returns ST_OK if successful, or ST_FAIL on any error (the error message
 * is logged through a call to stonithd_log(LOG_ERR)).
 */
static int
stonithd_reply(IPC_Channel *ch, ...)
{
	va_list 	ap;
	struct ha_msg *	reply;
	const char *	name;
	const char *	value;

	/* create an empty ha_msg */
	if (!(reply = ha_msg_new(1))) {
		stonithd_log(LOG_ERR, "cannot allocate ha_msg");
		return ST_FAIL;
	}

	/* add fields */
	va_start(ap, ch);
	while ((name = va_arg(ap, const char *))) {
		if ((value = va_arg(ap, const char *))) {
			if (ha_msg_add(reply, name, value) != HA_OK) {
				break;
			}
		}
	}
	va_end(ap);

	if (name != NULL) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "cannot add field %s to reply message", name);
		return ST_FAIL;
	}

	/* send the message */
	if (msg2ipcchan(reply, ch) != HA_OK) { 
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "cannot send reply to ipc channel");
		return ST_FAIL;
	}

	/* success */
	ZAPMSG(reply);
	return ST_OK;
}

/* 
 * Process a "cookie" request received from a callback channel.
 * 
 * This request is only sent via the callback channel to authenticate the client
 * connected to this channel. A string token (aka cookie) is supplied in the 
 * request. The server looks up the list of registered clients to find the one
 * with the given cookie, and associates the callback channel with the client.
 *
 * If authentication is successful, an RSIGNON/ok reply is sent to the client.
 * Otherwise, an RSIGNON/fail or RSIGNON/badreq reply is sent, and an error 
 * message is returned in the ERROR field.
 *
 * The request can fail due to any of the following reasons:
 * - the supplied cookie token is unknown
 * - there is already a callback channel associated with the client
 */
static int
on_stonithd_cookie(struct ha_msg * request, gpointer data)
{
	IPC_Channel * 		ch = (IPC_Channel *)data;
 	cl_uuid_t		cookie;
	stonithd_client_t * 	client = NULL;
	const char *		errmsg = NULL;
	const char * 		ret = ST_APIOK;

	assert(ch != NULL);
	assert(request != NULL);
	assert(IS_POLLED_CALLBACK_CHANNEL(ch));

	stonithd_log2(LOG_DEBUG, "on_stonithd_cookie: begin.");

	/* Extract the supplied cookie from the request. */
	if ( cl_get_uuid(request, F_STONITHD_COOKIE, &cookie) != HA_OK ) {
		errmsg = "missing F_STONITHD_COOKIE field";
		ret = ST_BADREQ;
		goto send_reply;
	}
	
	/* Is this cookie valid? */
	if (!(client = get_client_by_cookie(client_list, &cookie))) {
		errmsg = "invalid cookie";
		ret = ST_APIFAIL;
		goto send_reply;
	}

	/* Is this client already associated with a callback channel? */
	if (client->cbch != NULL) {
		errmsg = "callback channel already registered";
		ret = ST_APIFAIL;
		goto send_reply;
	}

	/* Associate the callback channel with the identified client. */
	client->cbch = ch;
	ret = ST_APIOK;

send_reply:
	if (errmsg) {
		stonithd_log(LOG_ERR, "on_stonithd_cookie: %s", errmsg);
	}
	return stonithd_reply(ch, 
			F_STONITHD_TYPE,   ST_APIRPL,
			F_STONITHD_APIRPL, ST_RSIGNON,
			F_STONITHD_APIRET, ret,
			F_STONITHD_ERROR,  errmsg,
			NULL);
}

static int
on_stonithd_signoff(struct ha_msg * request, gpointer data)
{
	stonithd_client_t * client = NULL;
	IPC_Channel * ch = (IPC_Channel *) data;
	int tmpint;

	stonithd_log2(LOG_DEBUG, "on_stonithd_signoff: begin.");

	/* parameter check, maybe redundant */
	if ( ch == NULL || request == NULL ) {
		stonithd_log(LOG_ERR, "parameter error, signoff failed.");
		return ST_FAIL;
	}

	if ( HA_OK != ha_msg_value_int(request, F_STONITHD_CPID, &tmpint) ) {
		stonithd_log(LOG_ERR, "signoff msg contains no or incorrect "
				"PID field");
		return ST_FAIL;
	}

	if ( (client = get_exist_client_by_chan(client_list, ch)) != NULL ) {
		if (client->pid == tmpint) {
			stonithd_log(LOG_DEBUG, "client %s (pid=%d) "
				     "signed off", client->name, client->pid);
			delete_client_by_chan(&client_list, ch);
			client = NULL;
		} else {
			stonithd_log(LOG_NOTICE, "the client's channel doesn't "
				     "correspond to the former pid(%d), "
				     "maybe a child is using its parent's "
				     "IPC channel", client->pid);
		}
	} else {
		stonithd_log(LOG_NOTICE, "client with pid %d probably "
			     "signed off ", tmpint);
	}

	return ST_OK;
}

static int
on_stonithd_node_fence(struct ha_msg * request, gpointer data)
{
	const char * api_reply = ST_APIOK;
	IPC_Channel * ch = (IPC_Channel *) data;
	stonith_ops_t * st_op = NULL;
	struct ha_msg * reply;
	int call_id = 0;
	stonithd_client_t * client = NULL;
	stonith_rsc_t * srsc = NULL;
	gboolean neednot_reboot_node = FALSE;
	int ret = ST_OK;

	stonithd_log2(LOG_DEBUG, "stonithd_node_fence: begin.");
	/* parameter check, maybe redundant */
	if ( ch == NULL || request == NULL ) {
		stonithd_log(LOG_ERR, "stonithd_node_fence: parameter error.");
		return ST_FAIL;
	}

	/* Check if have signed on */
	client = get_exist_client_by_chan(client_list, ch);
	if ( !client ) {
		stonithd_log(LOG_ERR, "stonithd_node_fence: client not signed on");
		return ST_FAIL;
	}

	st_op = new_stonith_ops_t(request);
	if ( !st_op ) {
		stonithd_log(LOG_ERR, "%s:%d: failed to create a stonith_op_t"
			, __FUNCTION__, __LINE__);
		api_reply = ST_BADREQ;
		goto sendback_reply;
	}

	stonithd_log(LOG_INFO, "client %s [pid: %d] requests a STONITH "
			"operation %s on node %s"
		,	client->name, client->pid
		,	stonith_op_strname[st_op->optype], st_op->node_name);

	if ( (st_op->optype == RESET) && !TEST && /* TEST means BSC */
	   g_hash_table_lookup(reboot_blocked_table, st_op->node_name) ) {
		stonithd_log(LOG_INFO, "reset of node %s from client %s (pid %d) "
			"effectively ignored: node has been reset recently and might be rebooting"
			, st_op->node_name, client->name, client->pid);
		neednot_reboot_node = TRUE;
		api_reply = ST_APIOK;
		goto sendback_reply;
	}

	/* If the node is me, should stonith myself. ;-) No, never come here
	 * from this API while the node name is myself.
	 * So just broadcast the requirement to the whole of cluster to do it.
	 */
	if ( st_op->optype != QUERY && !TEST &&
	    (srsc = get_local_stonithobj_can_stonith(st_op->node_name, NULL))
		!= NULL && 
	    (call_id=initiate_local_stonithop(st_op, srsc, client->cbch)) > 0 ) {
			api_reply = ST_APIOK;
	} else { 
		/* including query operation */
		/* call_id < 0 is the correct value when require others to do */
		if ((call_id = initiate_remote_stonithop(st_op,client->cbch)) < 0 ) {
			api_reply = ST_APIOK;
		} else {
			api_reply = ST_APIFAIL;
		}
	}

sendback_reply:
	/* send back the sync result */
	if ((reply = ha_msg_new(4)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d:ha_msg_new:out of memory."
				,__FUNCTION__, __LINE__);
		ret = ST_FAIL;
		goto del_st_op_and_return;
	}

	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RSTONITH) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK )
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, call_id))
		!= HA_OK ) {
		stonithd_log(LOG_ERR, "stonithd_node_fence: cannot add field.");
		ret = ST_FAIL;
		goto del_st_op_and_return;
	}
	
	if (msg2ipcchan(reply, ch) != HA_OK) {
		stonithd_log(LOG_ERR, "stonithd_node_fence: cannot send reply "
				"message to IPC");
		ret = ST_FAIL;
		goto del_st_op_and_return;
        }

	if (ch->ops->waitout(ch) == IPC_OK) {
		stonithd_log(LOG_DEBUG, "stonithd_node_fence: sent "
			    "back a synchronous reply.");
	} else {
		stonithd_log(LOG_DEBUG, "stonithd_node_fence: "
			    "failed to sent back a synchronous reply.");
		ret = ST_FAIL;
		goto del_st_op_and_return;
	}

	if ( neednot_reboot_node == TRUE ) { 
		/* here must be api_reply==ST_APIOK */
		st_op->op_result = STONITH_SUCCEEDED;
		st_op->node_list = g_string_append(st_op->node_list
						, local_nodename);
		stonithop_result_to_local_client(st_op, client->cbch);
	}

del_st_op_and_return:
	ZAPMSG(reply);
	free_stonith_ops_t(st_op);
	st_op = NULL;
	stonithd_log2(LOG_DEBUG, "stonithd_node_fence: end");
	return ret;
}

static int
initiate_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc, 
			 IPC_Channel * ch)
{
	int call_id;
	common_op_t * op;

	if ((call_id = stonith_operate_locally(st_op, srsc)) <= 0) {
		stonithd_log(LOG_ERR, "stonith_operate_locally failed.");
		return -1;
	}

	op = g_new0(common_op_t, 1);
	op->scenario = STONITH_INIT;
	op->rsc_id = g_strdup(srsc->rsc_id);
	op->result_receiver = ch;
	st_op->call_id = call_id;
	op->op_union.st_op = dup_stonith_ops_t(st_op);

	insert_into_executing_queue(op,call_id);
	return call_id;
}

static int
continue_local_stonithop(int old_key)
{
	stonith_rsc_t * srsc = NULL;
	char * rsc_id = NULL;
	int child_pid = -1;
	common_op_t * op = NULL;
	int * original_key = NULL;

	stonithd_log2(LOG_DEBUG, "continue_local_stonithop: begin.");
	if (FALSE == g_hash_table_lookup_extended(executing_queue, &old_key, 
			(gpointer *)&original_key, (gpointer *)&op)) {
		stonithd_log(LOG_ERR, "continue_local_stonithop: No old_key's "
			     "item exist in executing_queue. Strange!"); 
		return ST_FAIL;
	}
	if (op->scenario != STONITH_INIT && op->scenario != STONITH_REQ &&
	    op->op_union.st_op == NULL) {
		stonithd_log(LOG_ERR, "continue_local_stonithop: the old_key's "
			     "item isnot a stonith item. Strange!");
		return ST_FAIL;
	}

	if ( op->rsc_id == NULL ) {
		stonithd_log(LOG_ERR, "continue_local_stonithop: the old rsc_id"
				" == NULL, not correct!.");
		return ST_FAIL;
	}

	/* op->rsc_id is the begin place to lookup a valid STONITH resource */
	rsc_id = op->rsc_id;
	while ((srsc = get_local_stonithobj_can_stonith(op->op_union.st_op->node_name,
		rsc_id)) != NULL ) { 
		if ((child_pid=stonith_operate_locally(op->op_union.st_op, srsc)) > 0) {
			g_hash_table_steal(executing_queue, original_key);
			stonithd_log(LOG_DEBUG, "continue_local_stonithop: "
				     "removed optype=%s, key_id=%d", 
				     stonith_op_strname[op->op_union.st_op->optype],
					 *original_key);
			/* donnot need to free the old one.
			 * original_key, op is a pair of key-value.
			 */
			*original_key = child_pid;

			if (op->rsc_id != NULL) {
				g_free(op->rsc_id);
			}
			op->rsc_id = g_strdup(srsc->rsc_id);
			g_hash_table_insert(executing_queue, original_key, op);
			stonithd_log(LOG_DEBUG, "continue_local_stonithop: "
				     "inserted optype=%s, child_id=%d", 
				     stonith_op_strname[op->op_union.st_op->optype],
					 child_pid);
			return ST_OK;
		} else {
			rsc_id = srsc->rsc_id;
		}
	}

	return ST_FAIL;
}

static int
initiate_remote_stonithop(stonith_ops_t * st_op, IPC_Channel * ch)
{
	common_op_t * op;

	if (st_op == NULL) {
		stonithd_log(LOG_ERR, "initiate_remote_stonithop: "
			"st_op == NULL or srsc == NULL.");
		return -1;
	}

	if (st_op->optype == QUERY) {
		if (get_local_stonithobj_can_stonith(st_op->node_name, NULL)) {
			st_op->node_list = g_string_append(
				st_op->node_list, local_nodename);
		}
	}

	st_op->call_id = negative_callid_counter;
	if (ST_OK!=require_others_to_stonith(st_op) && st_op->optype!=QUERY) {
		stonithd_log(LOG_ERR, "require_others_to_stonith failed.");
		st_op->call_id = 0;
		return 1;
	}

	op = g_new0(common_op_t, 1);
	op->scenario = STONITH_INIT;
	op->result_receiver = ch;
	op->op_union.st_op = dup_stonith_ops_t(st_op);

	insert_into_executing_queue(op,st_op->call_id);
	stonithd_log2(LOG_INFO, "Broadcasting the message succeeded: require "
		"others to stonith node %s.", st_op->node_name);

	return negative_callid_counter--;
}

static int
changeto_remote_stonithop(int old_key)
{
	common_op_t * op = NULL;
	int * original_key = NULL;

	stonithd_log2(LOG_DEBUG, "changeto_remote_stonithop: begin.");
	if (FALSE == g_hash_table_lookup_extended(executing_queue, &old_key, 
			(gpointer *)&original_key, (gpointer *)&op)) {
		stonithd_log(LOG_ERR, "changeto_remote_stonithop: no old_key's "
			     "item exist in executing_queue. Strange!"); 
		return ST_FAIL;
	}

	if (op->scenario != STONITH_INIT) {
		stonithd_log(LOG_DEBUG, "changeto_remote_stonithop: I am not "
			     "stonith initiator, donnot pass it to others.");
		return ST_FAIL;
	}

	if ( op->op_union.st_op == NULL) {
		stonithd_log(LOG_ERR, "changeto_remote_stonithop: "
				"op->op_union.st_op == NULL");
		return ST_FAIL;
	}

	if ( ST_OK != require_others_to_stonith(op->op_union.st_op) ) {
		stonithd_log(LOG_ERR, "require_others_to_stonith failed.");
		return ST_FAIL;
	}
	/* donnt need to free op->rsc_id now. */
	g_hash_table_steal(executing_queue, original_key);
	stonithd_log(LOG_DEBUG, "changeto_remote_stonithop: removed "
		  "optype=%s, key=%d",
		  stonith_op_strname[op->op_union.st_op->optype],
		  *original_key);
	*original_key = op->op_union.st_op->call_id;
	g_hash_table_insert(executing_queue, original_key, op);
	stonithd_log(LOG_DEBUG, "changeto_remote_stonithop: inserted "
		  "optype=%s, key=%d",
		  stonith_op_strname[op->op_union.st_op->optype],
		  *original_key);
	return ST_OK;
}

static int
send_stonithop_final_result( const common_op_t * op)
{
	if (op == NULL) {
		stonithd_log(LOG_ERR, "send_stonithop_final_result: "
			     "op == NULL");
		return ST_FAIL;
	}
	stonithd_log2(LOG_DEBUG, "send_stonithop_final_result: begin.");

	if (op->scenario == STONITH_INIT) {
		return stonithop_result_to_local_client(
				op->op_union.st_op, op->result_receiver);
	}
		
	if (op->scenario == STONITH_REQ) {
		return stonithop_result_to_other_node(
				op->op_union.st_op, op->result_receiver);
	}

	stonithd_log(LOG_DEBUG, "scenario value may be wrong.");
	return ST_FAIL;
}

static int
stonithop_result_to_local_client( const stonith_ops_t * st_op, gpointer data)
{
	struct ha_msg * reply = NULL;
	IPC_Channel * ch;
	stonithd_client_t * client = NULL;
	

	stonithd_log2(LOG_DEBUG, "stonithop_result_to_local_client: begin.");
	if ( st_op == NULL || data == NULL ) {
		stonithd_log(LOG_ERR, "stonithop_result_to_local_client: "
				      "parameter error.");
		return ST_FAIL;
	}

	ch = (IPC_Channel *)data;
	if ( (client = get_exist_client_by_chan(client_list, ch)) == NULL ) {
		/* Here the ch are already destroyed */
		stonithd_log(LOG_NOTICE, "It seems the client signed off, who "
			"raised the operation. So won't send out the result.");
		return ST_OK;
	}

	if (st_op->op_result == STONITH_SUCCEEDED ) {
		stonithd_log(LOG_INFO, "%s %s: optype=%s. whodoit: %s"
			,	M_STONITH_SUCCEED
			,	st_op->node_name
			,	stonith_op_strname[st_op->optype]
			,	((GString *)(st_op->node_list))->str);
		
		if ( st_op->optype == RESET ) { /* RESET */
			if (0 == STRNCMP_CONST(client->name, "apitest")) {
				broadcast_reset_success(st_op->node_name);
			}
		}
	} else {
		stonithd_log(LOG_ERR
			,	"%s %s: optype=%s, op_result=%s" 
			,	M_STONITH_FAIL, st_op->node_name
			,	stonith_op_strname[st_op->optype]
			,	stonith_op_result_strname[st_op->op_result]);
	}

	stonithd_log2(LOG_DEBUG
		, "stonith finished: optype=%s, node_name=%s, op_result=%s"
		, stonith_op_strname[st_op->optype]
		, st_op->node_name
		, stonith_op_result_strname[st_op->op_result]);

	if ( !st_op->node_uuid || !st_op->node_list ) {
		stonithd_log(LOG_ERR, "stonithop_result_to_local_client: "
			     "null node_uuid or node_list");
		return ST_FAIL;
	}

	if ((reply = ha_msg_new(10)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d:ha_msg_new:out of memory."
				,__FUNCTION__, __LINE__);
		return ST_FAIL;
	}

	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
  	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_STRET) != HA_OK ) 
	    ||(ha_msg_add_int(reply, F_STONITHD_OPTYPE, st_op->optype) != HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_NODE, st_op->node_name) != HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_NLIST,
			((GString *)(st_op->node_list))->str) != HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_TIMEOUT, st_op->timeout)!=HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, st_op->call_id) !=HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_NODE_UUID, st_op->node_uuid) != HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_FRC, st_op->op_result) != HA_OK)
	    ||(st_op->private_data &&
		ha_msg_add(reply, F_STONITHD_PDATA, st_op->private_data) != HA_OK)
		) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "stonithop_result_to_local_client: "
			     "cannot add fields.");
		return ST_FAIL;
	}

	if ( msg2ipcchan(reply, ch) != HA_OK) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "stonithop_result_to_local_client: cannot"
			     " send final result message via IPC");
		return ST_FAIL;
	} else {
		stonithd_log(LOG_DEBUG, "stonithop_result_to_local_client: "
			     "succeed in sending back final result message.");
	}

	ZAPMSG(reply);
	return ST_OK;
}

static int
stonithop_result_to_other_node( stonith_ops_t * st_op, gconstpointer data)
{
	const char * node_name = (const char *)data;
	struct ha_msg * reply;

	if (data == NULL) {
		stonithd_log(LOG_ERR, "stonithop_result_to_other_node: "
			     "data == NULL");
		return ST_FAIL;
	}

	if (st_op->op_result != STONITH_SUCCEEDED) {
		/* Actually donnot need to send */
		return ST_OK;
	}

	if ((reply = ha_msg_new(4)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d: out of memory"
				, __FUNCTION__, __LINE__ );
		return ST_FAIL;
	}

	if ((ha_msg_add_int(reply, F_STONITHD_FRC, st_op->op_result) != HA_OK)
    	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, st_op->call_id) 
		!= HA_OK)) {
		stonithd_log(LOG_ERR, "stonithop_result_to_other_node: "
			     "ha_msg_add: cannot add field.");
		ZAPMSG(reply);
		return ST_FAIL;
	}

	if (!stonithd_sendmsg(node_name, reply, T_RSTIT)) {
		ZAPMSG(reply);
		return ST_FAIL;
	}

	ZAPMSG(reply);
	stonithd_log(LOG_DEBUG,"stonithop_result_to_other_node: "
		     "send result message successfully.");
	return ST_OK;	
}

static int
stonith_operate_locally( stonith_ops_t * st_op, stonith_rsc_t * srsc)
{
	char buf_tmp[40];
	Stonith * st_obj = NULL;
	pid_t pid;

	if (st_op == NULL || srsc == NULL ) {
		stonithd_log(LOG_ERR, "stonith_operate_locally: "
				"parameter error.");
		return -1;
	}

	if (st_op->optype == QUERY) {
		stonithd_log2(LOG_DEBUG, "query operation.");
		return -1;
	}

	if (srsc->stonith_obj == NULL) {
		stonithd_log(LOG_ERR, "stonith_operate_locally: "
			"srsc->stonith_obj == NULL.");
		return -1;
	}

	st_obj = srsc->stonith_obj;

	/* stonith it by myself in child */
	return_to_orig_privs();
	if ((pid = fork()) < 0) {
		stonithd_log(LOG_ERR, "stonith_operate_locally: fork failed.");
		return_to_dropped_privs();
		return -1;
	} else if (pid > 0) { /* in the parent process */
		memset(buf_tmp,	0, sizeof(buf_tmp));
		snprintf(buf_tmp, sizeof(buf_tmp)-1, "%s_%s_%d", st_obj->stype
			, srsc->rsc_id , (int)st_op->optype); 
		NewTrackedProc( pid, 1
				, (debug_level>1)? PT_LOGVERBOSE : PT_LOGNORMAL
				, g_strdup(buf_tmp), &StonithdProcessTrackOps);
		stonithd_log(LOG_INFO, "%s::%d: sending fencing op %s for %s "
			"to %s (%s) (pid=%d)", __FUNCTION__
			, __LINE__, stonith_op_strname[st_op->optype], st_op->node_name
			, srsc->rsc_id, srsc->ra_name, pid);
		return_to_dropped_privs();
		return pid;
	}

	/* now in child process */
	/* this operation may be on block status */
	exit(stonith_req_reset(st_obj, st_op->optype, 
				      g_strdup(st_op->node_name)));
}

static int
require_others_to_stonith(const stonith_ops_t * st_op)
{
	struct ha_msg * msg;

	if ((msg = ha_msg_new(6)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d: out of memory"
				, __FUNCTION__, __LINE__ );
		return ST_FAIL;
	}

	stonithd_log(LOG_INFO, "we can't manage %s, broadcast request "
		"to other nodes", st_op->node_name);

	stonithd_log2(LOG_DEBUG, "require_others_to_stonith: begin.");
	if ((ha_msg_add(msg, F_STONITHD_NODE, st_op->node_name) != HA_OK)
	    ||(ha_msg_add_int(msg, F_STONITHD_OPTYPE, st_op->optype) != HA_OK)
	    ||(ha_msg_add_int(msg, F_STONITHD_TIMEOUT, st_op->timeout) != HA_OK)
	    ||(ha_msg_add_int(msg, F_STONITHD_CALLID, st_op->call_id) != HA_OK)) {
		stonithd_log(LOG_ERR, "require_others_to_stonith: "
					"cannot add field.");
		ZAPMSG(msg);
		return ST_FAIL;
	}

	if (!stonithd_sendmsg(NULL, msg,
		(st_op->optype == QUERY) ? T_WHOCANST : T_STIT)) {
		ZAPMSG(msg);
		return ST_FAIL;
	}

	ZAPMSG(msg);
	stonithd_log2(LOG_DEBUG,"require_others_to_stonith: end.");
	return ST_OK;
}

static stonith_rsc_t *
get_local_stonithobj_can_stonith( const char * node_name,
				  const char * begin_rsc_id )
{
	GList * tmplist = NULL;
	GList * begin_search_list = NULL;
	stonith_rsc_t * tmp_srsc = NULL;

	stonithd_log2(LOG_DEBUG, "get_local_stonithobj_can_stonith: begin.");
	if (local_started_stonith_rsc == NULL) {
		stonithd_log(LOG_DEBUG, "get_local_stonithobj_can_stonith: "
				"local_started_stonith_rsc == NULL");
		return NULL;
	}

	if ( node_name == NULL ) {
		stonithd_log(LOG_ERR, "get_local_stonithobj_can_stonith: "
				"node_name == NULL");
		return NULL;
	}

	if (begin_rsc_id == NULL) {
		begin_search_list = g_list_first(local_started_stonith_rsc);
	} else {
		for ( tmplist = g_list_first(local_started_stonith_rsc); 
		      tmplist != NULL; 
		      tmplist = g_list_next(tmplist)) {
		      	tmp_srsc = (stonith_rsc_t *)tmplist->data;
			if ( tmp_srsc != NULL && 
			     strncmp(tmp_srsc->rsc_id, begin_rsc_id, MAXCMP)
				 == 0) {
				begin_search_list = g_list_next(tmplist);
				break;
			}
		}
		if (begin_search_list == NULL) {
			stonithd_log(LOG_DEBUG, "get_local_stonithobj_can_"
				"stonith: begin_rsc_id donnot exist.");
			return NULL;
		}
	}

	for ( tmplist = begin_search_list;
	      tmplist != NULL; 
	      tmplist = g_list_next(tmplist)) {
		tmp_srsc = (stonith_rsc_t *)tmplist->data;
		if ( tmp_srsc != NULL && tmp_srsc->node_list != NULL ) {
			char **	this;
			for(this=tmp_srsc->node_list; *this; ++this) {
				stonithd_log2(LOG_DEBUG, "get_local_stonithobj_"
					"can_stonith: host=%s.", *this);
				if ( strncmp(node_name, *this, MAXCMP) == 0 ) {
					stonithd_log2(LOG_DEBUG, "stonith type found:"
						" %s", tmp_srsc->stonith_obj->stype);
					return tmp_srsc;
				}
			}
		} else {
			if (tmp_srsc == NULL) {
				stonithd_log2(LOG_DEBUG, "get_local_stonithobj_"
					"can_stonith: tmp_srsc=NULL.");
				
			} else {
				stonithd_log2(LOG_DEBUG, "get_local_stonithobj_"
				    "can_stonith: tmp_srsc->node_list = NULL.");
			}
		}
	}

	return NULL;
}

static void
timeout_destroy_notify(gpointer user_data)
{
	int * call_id = (int *) user_data;
	if (call_id != NULL) {
		g_free(call_id);	
	}
}

static gboolean
stonithop_timeout(gpointer data)
{
	int * call_id = (int *) data;
	int * orig_key = NULL;
	common_op_t * op = NULL;
	
	if (data == NULL) {
		stonithd_log(LOG_ERR, "stonithop_timeout: user_data == NULL.");
		return FALSE;
	}

	stonithd_log2(LOG_DEBUG, "stonithop_timeout: begin.");

	/* since g_hash_table_find donnot exist in early version of glib-2.0,
	 * So use the equality my_hash_table_find I wrote.
 	 */
	my_hash_table_find(executing_queue, (gpointer *)&orig_key, 
			   (gpointer *)&op, call_id);

	/* Kill the possible child process forked for this operation */
	if (orig_key!=NULL && *orig_key > 0 && CL_PID_EXISTS(*orig_key)) {
		return_to_orig_privs();
		CL_KILL(*orig_key, SIGKILL);	
		return_to_dropped_privs();
	}

	if ( op != NULL && 
	    (op->scenario == STONITH_INIT || op->scenario == STONITH_REQ)) {
		if (op->op_union.st_op->optype != QUERY) {
			op->op_union.st_op->op_result = STONITH_TIMEOUT;
		} else {
			op->op_union.st_op->op_result = STONITH_SUCCEEDED;
		}
	
		send_stonithop_final_result(op);
		/* Pay attention to not free over */
		g_hash_table_remove(executing_queue, orig_key);
	} else {
		stonithd_log(LOG_DEBUG, "stonithop_timeout: the stonith "
			"operation (call_id==%d) has finished before timeout."
			, *call_id);
	}

	return FALSE;
}

typedef struct {
	gpointer * key;
	gpointer * value;
	gpointer user_data;
} lookup_data_t;

static void
my_hash_table_find(GHashTable * htable, gpointer * orig_key,
		   gpointer * value, gpointer user_data)
{
	lookup_data_t tmp_data;

	tmp_data.key	   = orig_key;
	tmp_data.value     = value;
	tmp_data.user_data = user_data; 

	g_hash_table_foreach(htable, has_this_callid, (gpointer)&tmp_data);
}

static void
has_this_callid(gpointer key, gpointer value, gpointer user_data)
{
	int callid;
	common_op_t * op; 
	lookup_data_t * tmp_data = user_data;

	if (user_data == NULL) {
		stonithd_log(LOG_ERR, "has_this_callid: user_data == NULL.");
		return;
	} else {
		callid = *(int *)tmp_data->user_data;
	}

	if ( value == NULL ) {
		stonithd_log(LOG_ERR, "has_this_callid: value == NULL.");
		return;
	} else {
		op = (common_op_t *)value;
	}
	
	if (op->scenario != STONITH_INIT && op->scenario != STONITH_REQ ) {
		stonithd_log(LOG_ERR, "has_this_callid: scenario value error.");
		return;
	}

	if (op->op_union.st_op != NULL && op->op_union.st_op->call_id == callid ) {
		*(tmp_data->key) = key;
		*(tmp_data->value) = value;
	}
}

static int
on_stonithd_virtual_stonithRA_ops(struct ha_msg * request, gpointer data)
{
	const char * api_reply = ST_APIOK;
	IPC_Channel * ch = (IPC_Channel *) data;
	struct ha_msg * reply;
	stonithRA_ops_t * ra_op;
	int child_pid = -1;
	stonithd_client_t * client = NULL;

	stonithd_log2(LOG_DEBUG, "on_stonithd_stonithRA_ops: begin.");
	/* parameter check, maybe redundant */
	if ( ch == NULL || request == NULL ) {
		stonithd_log(LOG_ERR, "on_virtual_stonithRA_ops: "
			     "parameter error.");
		return ST_FAIL;
	}

	/* Check if have signoned */
	if ((client = get_exist_client_by_chan(client_list, ch)) == NULL ) {
		stonithd_log(LOG_ERR, "client [pid: %d] "
			     "not signed on", ch->farside_pid);
		if ( NULL!= (ra_op = new_stonithRA_ops_t(request)) ) {
			stonithd_log(LOG_DEBUG, "client [pid: %d] requests a "
				"resource operation %s on %s (%s)"
				, ch->farside_pid, ra_op->op_type
				, ra_op->rsc_id, ra_op->ra_name);
			free_stonithRA_ops_t(ra_op);
			ra_op = NULL;
		}
		return ST_FAIL;
	}

	/* handle the RA operations such as 'start' 'stop', 'monitor' and etc */
	if ( NULL== (ra_op = new_stonithRA_ops_t(request)) ) {
		api_reply = ST_BADREQ;
		goto send_back_reply;
	}

	stonithd_log(LOG_DEBUG, "client %s [pid: %d] requests a resource "
		"operation %s on %s (%s)"
		,	client->name, client->pid
		,	ra_op->op_type, ra_op->rsc_id, ra_op->ra_name);

	/* execute stonith plugin : begin */
	/* When in parent process then be back here */
	if ((child_pid = stonithRA_operate(ra_op, NULL)) <= 0) {
		api_reply = ST_APIFAIL;
		free_stonithRA_ops_t(ra_op);
		ra_op = NULL;
	} else {
		common_op_t * op;
		int * key_tmp;

		op = g_new0(common_op_t, 1);
		op->scenario = STONITH_RA_OP;
		op->result_receiver = client->cbch;
		op->op_union.ra_op = ra_op;
		op->timer_id = -1;

		key_tmp = g_new(int, 1);
		*key_tmp = child_pid;
		g_hash_table_insert(executing_queue, key_tmp, op);
		stonithd_log2(LOG_DEBUG, "on_stonithd_virtual_stonithRA_ops: "
			     "insert child_pid=%d to table", *key_tmp);
	}	
	/* execute stonith plugin : end */

send_back_reply:
	/* send back the sync result at once */
	if ((reply = ha_msg_new(4)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d:ha_msg_new:out of memory."
				,__FUNCTION__, __LINE__);
		return ST_FAIL;
	}

	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RRAOP) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK ) 
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, child_pid)
		!= HA_OK ) ) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: "
			     "cannot add message field.");
		return ST_FAIL;
	}

	if (msg2ipcchan(reply, ch) != HA_OK) { /* How to deal the error*/
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: "
			     "cannot send reply message to IPC");
		return ST_FAIL;
        }

	if (ch->ops->waitout(ch) != IPC_OK) {
		stonithd_log(LOG_ERR, "on_stonithd_virtaul_stonithRA_ops: "
			     "failed to send back a synchonrous result.");
		ZAPMSG(reply);
		return ST_FAIL;
	}

	stonithd_log2(LOG_DEBUG, "on_stonithd_virtual_stonithRA_ops: "
		     "sent back a synchonrous result.");
	ZAPMSG(reply);
	return ST_OK;
}

static int
send_stonithRAop_final_result( stonithRA_ops_t * ra_op, gpointer data)
{
	struct ha_msg * reply = NULL;
	IPC_Channel * ch = (IPC_Channel *)data;

	stonithd_log2(LOG_DEBUG, "send_stonithRAop_final_result: begin.");
	if ( ra_op == NULL || data == NULL ) {
		stonithd_log(LOG_ERR, "send_stonithRAop_final_result: "
				      "parameter error.");
		return ST_FAIL;
	}

	stonithd_log(LOG_DEBUG
		     , "%s's (%s) op %s finished. op_result=%d"
		     , ra_op->rsc_id
		     , ra_op->ra_name
			 , ra_op->op_type
			 , ra_op->op_result);

	if ( NULL == get_exist_client_by_chan(client_list, ch) ) {
		/* Here the ch are already destroyed */
		stonithd_log(LOG_NOTICE, "It seems the client signed off, who "
		      "raised the RA operation. So won't send out the result.");
		return ST_OK;
	}

	if ((reply = ha_msg_new(8)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d:ha_msg_new:out of memory"
				,__FUNCTION__, __LINE__);
		return ST_FAIL;
	}

	stonithd_log2(LOG_DEBUG, "ra_op->op_type=%s, ra_op->rsc_id=%s, ra_op->ra_name=%s",
		     ra_op->op_type, ra_op->rsc_id, ra_op->ra_name);
	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
  	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RAOPRET) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_RAOPTYPE, ra_op->op_type) != HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_RANAME, ra_op->ra_name) != HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_RSCID, ra_op->rsc_id) != HA_OK)
	    ||(ha_msg_addhash(reply, F_STONITHD_PARAMS, ra_op->params) != HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, ra_op->call_id)!= HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_FRC, ra_op->op_result)
		!= HA_OK )) {
		stonithd_log(LOG_ERR, "send_stonithRAop_final_result: cannot "
			     "add message fields.");
		ZAPMSG(reply);
		return ST_FAIL;
	}

	if ( msg2ipcchan(reply, ch) != HA_OK) {
		stonithd_log(LOG_ERR, "send_stonithRAop_final_result: cannot "
			     "send final result message via IPC");
		ZAPMSG(reply);
		return ST_FAIL;
	} else {
		stonithd_log2(LOG_DEBUG, "send_stonithRAop_final_result: "
			     "succeed in sending back final result message.");
	}

	ZAPMSG(reply);
	return ST_OK;
}

static int
on_stonithd_list_stonith_types(struct ha_msg * request, gpointer data)
{
	const char * api_reply = ST_APIOK;
	IPC_Channel * ch = (IPC_Channel *) data;
	struct ha_msg * reply;
	char **	typelist;

	/* Need to check whether signon? */
	if ((reply = ha_msg_new(4)) == NULL) {
		stonithd_log(LOG_ERR, "%s:%d:ha_msg_new:out of memory."
				,__FUNCTION__, __LINE__);
		return ST_FAIL;
	}

	typelist = stonith_types();
	if (typelist == NULL) {
		stonithd_log(LOG_ERR, "Could not list Stonith types.");
		api_reply = ST_APIFAIL;
	} else {
		char **	this;
		for(this = typelist; *this; ++this) {
			stonithd_log(LOG_DEBUG,"stonith type: %s\n", *this);
			if ( HA_OK != cl_msg_list_add_string(reply,
				F_STONITHD_STTYPES, *this) ) {
				ZAPMSG(reply);
				stonithd_log(LOG_ERR, "list_stonith_types: "
						"cannot add field.");
				return ST_FAIL;
			}
		}
	}

	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RLTYPES) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK ) ) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_list_stonith_types: cannot "
			     "add message fields.");
		return ST_FAIL;
	}
	
	if (msg2ipcchan(reply, ch) != HA_OK) { /* How to deal the error*/
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_list_stonith_types: cannot "
			     " send reply message to IPC");
		return ST_FAIL;
        }

	ZAPMSG(reply);
	stonithd_log(LOG_DEBUG, "on_stonithd_list_stonith_types: end and sent "
				"back a reply.");
	return ST_OK;
}

static int
stonithRA_operate( stonithRA_ops_t * op, gpointer data )
{
	int i;

	/* 
	 * As for op->op_type, now only accept three operations: start, stop
	 * and monitor.
	 */
	for (i = 0; i < DIMOF(raop_handler); i++) {
		if ( strncmp(op->op_type, raop_handler[i].op_type, MAXCMP)
			 == 0 ) {
			/* call the handler of the operation */
			 return raop_handler[i].handler(op, data);
		}
	}

        if (i == DIMOF(raop_handler)) {
                stonithd_log(LOG_WARNING, "stonithRA_operate: received an unknown "
			     "RA op %s, ignoring", op->op_type);
        }

	return(-1);	
}

static int
stonithRA_start( stonithRA_ops_t * op, gpointer data)
{
	stonith_rsc_t * srsc;
	pid_t 		pid;
	StonithNVpair*	snv;
	Stonith *	stonith_obj = NULL;
	char 		buf_tmp[40];
	int		shmid=0, shmsize=0;
	char **		hostlist;

	/* Check the parameter */
	if ( op == NULL || op->rsc_id == NULL || op->op_type == NULL
	    || op->ra_name == NULL || op->params == NULL ) {
		stonithd_log(LOG_ERR, "stonithRA_start: parameter error");
		return ST_FAIL;
	}

	srsc = get_started_stonith_resource(op->rsc_id);
	if (srsc != NULL) {
		stonithd_log(LOG_DEBUG, "%s: %s is "
			"already started, we just probe the status"
			, __FUNCTION__, srsc->rsc_id);
		/* seems started, just to confirm it */
		stonith_obj = srsc->stonith_obj;
		goto probe_status;
	}

	shmsize = MAX_NODE_STORAGE;
	shmid = shmget(IPC_PRIVATE, shmsize, (SHM_R | SHM_W));
	if( shmid < 0 ) {
		stonithd_log(LOG_ERR,"%s:%d: shmget failed: %s",
			__FUNCTION__, __LINE__, strerror(errno));
		return ST_FAIL;
	}
	stonithd_log2(LOG_DEBUG, "%s: got a shmem seg of size %d"
		     , __FUNCTION__, shmsize);

	/* Don't find in local_started_stonith_rsc, not on start status */
	stonithd_log2(LOG_DEBUG, "stonithRA_start: op->params' address=%p"
		     , op->params);
	if (debug_level > 0) {
		print_str_hashtable(op->params);
	}

	return_to_orig_privs();
	stonith_obj = stonith_new(op->ra_name);
	if (stonith_obj == NULL) {
		return_to_dropped_privs();
		stonithd_log(LOG_ERR, "invalid RA/device type: '%s'", 
		             op->ra_name);
		return ST_FAIL;
	}

	/* Set the stonith plugin's debug level */
	stonith_set_debug(stonith_obj, debug_level);
	stonith_set_log(stonith_obj, (PILLogFun)trans_log);

	snv = stonith_ghash_to_NVpair(op->params);
	if ( snv == NULL
	||	stonith_set_config(stonith_obj, snv) != S_OK ) {
		stonithd_log(LOG_ERR,
			"invalid config info for %s (device %s)", op->rsc_id, op->ra_name);
		stonith_delete(stonith_obj); 
		return_to_dropped_privs();
		stonith_obj = NULL;
		if (snv != NULL) {
			free_NVpair(snv);
			snv = NULL;
		}
		return ST_FAIL; /*exit(rc);*/
	}

	return_to_dropped_privs();
	if (snv != NULL) {
		free_NVpair(snv);
		snv = NULL;
	}

	op->stonith_obj = stonith_obj;

probe_status:
	return_to_orig_privs();
        if ((pid = fork()) < 0) {
                stonithd_log(LOG_ERR, "stonithRA_start: fork failed.");
		return_to_dropped_privs();
                return -1;
        } else if (pid > 0) { /* in the parent process */
		if( shmid ) {
			add_shm_hostlist(shmid,pid);
		}
		memset(buf_tmp, 0, sizeof(buf_tmp));
		snprintf(buf_tmp, sizeof(buf_tmp)-1, "%s_%s_%s", stonith_obj->stype
			, op->rsc_id , "start"); 
		NewTrackedProc( pid, 1
				, (debug_level>1)? PT_LOGVERBOSE : PT_LOGNORMAL
				, g_strdup(buf_tmp), &StonithdProcessTrackOps);
		return_to_dropped_privs();
                return pid;
        }

	/* Now in the child process */
	/* Need to distiguish the exit code more carefully */
	if ( S_OK != stonith_get_status(stonith_obj) ) {
		exit(EXECRA_UNKNOWN_ERROR);
	}
	if( !shmid ) { /* Already started before this operation */
		exit(EXECRA_OK);
	}
	hostlist = stonith_get_hostlist(stonith_obj);
	if( !hostlist ) {
		stonithd_log(LOG_ERR, "cannot list nodes for %s"
		,	op->rsc_id);
		exit(EXECRA_INVALID_PARAM);
	}
	if( !hostlist2shmem(op->rsc_id,shmid,hostlist,shmsize,
			lastgasp_stonith(stonith_obj->stype)) ) {
		exit(EXECRA_INVALID_PARAM);
	}
	exit(EXECRA_OK);
}

static struct hostlist_shmseg *
lookup_shm_hostlist(pid_t pid)
{
	GList *l;
	struct hostlist_shmseg *p;

	for( l = g_list_first(mem_hostlist); l; l = g_list_next(l) ) {
		p = (struct hostlist_shmseg *)(l->data);
		if( p->pid == pid )
			return p;
	}
	return NULL;
}

static void
add_shm_hostlist(int shmid, pid_t pid)
{
	struct hostlist_shmseg *p;

	if( !(p = calloc(sizeof(struct hostlist_shmseg),1)) ) {
		stonithd_log(LOG_ERR, "out of memory");
		return;
	}
	p->shmid=shmid;
	p->pid=pid;
	mem_hostlist = g_list_append(mem_hostlist,p);
}

static void
remove_shm_hostlist(pid_t pid)
{
	struct hostlist_shmseg *p;

	if( !(p = lookup_shm_hostlist(pid)) ) {
		return;
	}
	if( shmctl(p->shmid, IPC_RMID, NULL) < 0 ) {
		stonithd_log(LOG_ERR,"%s:%d: shmctl failed: %s",
			__FUNCTION__, __LINE__, strerror(errno));
		return;
	}
	mem_hostlist = g_list_remove(mem_hostlist, p);
	free(p);
}

/* store the hostlist to a shared memory segment */
static gboolean
hostlist2shmem(char *rsc_id, int shmid,char **hostlist,int maxlist,int is_lastgasp)
{
	char *s, *q, **h;
	int rc = TRUE;

	if( !hostlist ) {
		return FALSE;
	}
	if( (s = shmat(shmid,0,0)) == (void *)-1 ) {
		stonithd_log(LOG_ERR,"%s:%d: shmat failed: %s",
			__FUNCTION__, __LINE__, strerror(errno));
		return FALSE;
	}
	q = s;
	for( h = hostlist; *h; h++ ) {
		if( !TEST && !is_lastgasp && !strcmp(*h, local_nodename) ) {
			stonithd_log(LOG_DEBUG,"remove us (%s) from the host list for %s"
				, *h, rsc_id);
			continue; /* we can't reset ourselves */
		}
		if( q-s+strlen(*h)+1 > maxlist-1 ) {
			stonithd_log(LOG_ERR,"%s:%d: size of node "
				"list for %s exceeds storage: skipping %s",
				__FUNCTION__, __LINE__, rsc_id, *h);
			rc = FALSE;
			break;
		}
		stonithd_log(LOG_DEBUG,"%s claims it can manage node %s"
			, rsc_id, *h);
		strcpy(q,*h);
		q += strlen(q)+1;
	}
	*q = '\0'; /* additional '\0' to end the list */
	stonith_free_hostlist(hostlist);
	if( shmdt(s) == -1 ) {
		stonithd_log(LOG_ERR,"%s:%d: shmdt failed: %s",
			__FUNCTION__, __LINE__, strerror(errno));
		return FALSE;
	}
	if( q == s ) { /* oops, no hosts configured */
		stonithd_log(LOG_WARNING,"host list for %s "
			"is empty, please fix your constraints", rsc_id);
		rc = FALSE;
	}
	return rc;
}

/* build the hostlist from the shmem segment
 * drop the shmem segment afterwards
 */
static char **
shmem2hostlist(pid_t pid)
{
	struct hostlist_shmseg *p;
	char *s, **hostlist;

	if( !(p = lookup_shm_hostlist(pid)) ) {
		return NULL; /* no hostlist found */
	}
	return_to_orig_privs();
	if( (s = shmat(p->shmid,0,SHM_RDONLY)) == (void *)-1 ) {
		stonithd_log(LOG_ERR,"%s:%d: shmat failed: %s",
			__FUNCTION__, __LINE__, strerror(errno));
		return NULL;
	}
	hostlist = copyshmem(s);
	if( shmdt(s) == -1 ) {
		stonithd_log(LOG_ERR,"%s:%d: shmdt failed: %s",
			__FUNCTION__, __LINE__, strerror(errno));
		stonith_free_hostlist(hostlist);
		return NULL;
	}
	remove_shm_hostlist(pid);
	return_to_dropped_privs();
	return hostlist;
}

static char **
copyshmem(char *s)
{
	int n;
	char *q, **h, **hostlist;

	for( q = s, n = 0; *q; q += strlen(q)+1, n++ )
		;
	if( n == 0 ) {
		return NULL;
	}
	if( !(hostlist = (char **)calloc(sizeof(char *),(n+1))) ) {
		stonithd_log(LOG_ERR, "out of memory");
		return NULL;
	}
	for( q = s, h = hostlist; *q; q += strlen(q)+1, h++ ) {
		if( !(*h = (char *)malloc(strlen(q)+1)) ) {
			stonithd_log(LOG_ERR, "out of memory");
			stonith_free_hostlist(hostlist);
			return NULL;
		}
		strcpy(*h,q);
	}
	return hostlist;
}

static void
record_new_srsc(stonithRA_ops_t *ra_op)
{
	stonith_rsc_t * srsc;
	char **node_list;

	if( get_started_stonith_resource(ra_op->rsc_id) ) {
		return; /* start of an already started stonith object */
	}
	node_list = shmem2hostlist(ra_op->call_id);
	if( !node_list ) {
		stonithd_log(LOG_WARNING,"start %s failed, because its hostlist "
			"is empty", ra_op->rsc_id);
		return;
	}
	/* ra_op will be free at once, so it's safe to set some of its
	 * fields as NULL.
	 */
	srsc = g_new0(stonith_rsc_t, 1);
	srsc->rsc_id = g_strdup(ra_op->rsc_id);
	srsc->ra_name = g_strdup(ra_op->ra_name);
	srsc->params = ra_op->params;
	ra_op->params = NULL;
	srsc->stonith_obj = ra_op->stonith_obj;
	ra_op->stonith_obj = NULL;
	srsc->node_list = node_list;

	if ( debug_level >= 2 ) {
		char **	this;
		stonithd_log2(LOG_DEBUG, "Got HOSTLIST");
		for(this=srsc->node_list; *this; ++this) {
			stonithd_log2(LOG_DEBUG, "%s", *this);
		}
	}

	local_started_stonith_rsc = 
			g_list_append(local_started_stonith_rsc, srsc);
}

static int
stonithRA_stop( stonithRA_ops_t * ra_op, gpointer data )
{
	stonith_rsc_t * srsc = NULL;
	pid_t pid;
	char  buf_tmp[40];

	if (ra_op == NULL || ra_op->rsc_id == NULL ) {
		stonithd_log(LOG_ERR, "stonithRA_stop: parameter error.");
		return -1;
	}

	memset(buf_tmp, 0, sizeof(buf_tmp));
	srsc = get_started_stonith_resource(ra_op->rsc_id);
	if (srsc != NULL) {
		stonithd_log2(LOG_DEBUG, "got the active stonith_rsc: " 
				"RA name = %s, rsc_id = %s."
				, srsc->ra_name, srsc->rsc_id);
		snprintf(buf_tmp, sizeof(buf_tmp)-1, "%s_%s_%s"
			, srsc->stonith_obj->stype
			, ra_op->rsc_id, ra_op->op_type);
		return_to_orig_privs();
		stonith_delete(srsc->stonith_obj);
		return_to_dropped_privs();
		srsc->stonith_obj = NULL;
		local_started_stonith_rsc = 
			g_list_remove(local_started_stonith_rsc, srsc);
		free_stonith_rsc(srsc);	
	} else {
		stonithd_log(LOG_NOTICE, "try to stop a resource %s who is "
			     "not in started resource queue.", ra_op->rsc_id);
		snprintf(buf_tmp, sizeof(buf_tmp)-1, "%s_%s_%s"
			, ra_op->ra_name, ra_op->rsc_id, ra_op->op_type);
	}

	return_to_orig_privs();
        if ((pid = fork()) < 0) {
		return_to_dropped_privs();
                stonithd_log(LOG_ERR, "stonithRA_stop: fork failed.");
                return -1;
        } else if (pid > 0) { /* in the parent process */
		NewTrackedProc( pid, 1
				, (debug_level>1)? PT_LOGVERBOSE : PT_LOGNORMAL
				, g_strdup(buf_tmp), &StonithdProcessTrackOps);
		return_to_dropped_privs();
                return pid;
        }
	
	/* in child process */
	exit(0);
}

/* Don't take the weight of the check into account yet */
static int
stonithRA_monitor( stonithRA_ops_t * ra_op, gpointer data )
{
	stonith_rsc_t * srsc = NULL;
	pid_t pid;
	char  buf_tmp[40];
	int child_exitcode = EXECRA_OK;

	stonithd_log2(LOG_DEBUG, "stonithRA_monitor: begin.");
	if (ra_op == NULL || ra_op->rsc_id == NULL ) {
		stonithd_log(LOG_ERR, "stonithRA_monitor: parameter error.");
		return -1;
	}

	srsc = get_started_stonith_resource(ra_op->rsc_id);
	if ( srsc == NULL ) {
		stonithd_log(LOG_DEBUG, "stonithRA_monitor: %s is not "
				"started.",ra_op->rsc_id);
		/* This resource is not started */
		child_exitcode = EXECRA_NOT_RUNNING;
	}

	return_to_orig_privs();
        if ((pid = fork()) < 0) {
                stonithd_log(LOG_ERR, "stonithRA_monitor: fork failed.");
		return_to_dropped_privs();
                return -1;
        } else if (pid > 0) { /* in the parent process */
		memset(buf_tmp, 0, sizeof(buf_tmp));
		snprintf(buf_tmp, sizeof(buf_tmp)-1, "%s_%s_%s", (srsc!=NULL) ? 
			 srsc->stonith_obj->stype : "unknown"
			 , ra_op->rsc_id, "monitor");
		NewTrackedProc( pid, 1
				, (debug_level>1)? PT_LOGVERBOSE : PT_LOGNORMAL
				, g_strdup(buf_tmp), &StonithdProcessTrackOps);
		return_to_dropped_privs();
		stonithd_log2(LOG_DEBUG, "stonithRA_monitor: end.");
                return pid;
        }

	/* Go here the child */
	/* When the resource is not started... */
	if (child_exitcode == EXECRA_NOT_RUNNING) {
		stonithd_log2(LOG_DEBUG, "stonithRA_monitor: child exit, "
				"exitcode: EXECRA_NOT_RUNNING.");
		exit(EXECRA_NOT_RUNNING);
	}

	/* Need to distiguish the exit code more carefully? Should 
	   EXECRA_STATUS_UNKNOWN be EXECRA_EXEC_UNKNOWN_ERROR? likely if 
	   according to the status code S_* value in the file stonith.c.
	 */
	if ( stonith_get_status(srsc->stonith_obj) == S_OK ) {
		stonithd_log2(LOG_DEBUG, "stonithRA_monitor: child exit, "
				"exitcode: EXECRA_OK");
		exit(EXECRA_OK);
	} else {
		stonithd_log(LOG_DEBUG, "stonithRA_monitor: child exit, "
				"exitcode: EXECRA_STATUS_UNKNOWN");
		exit(EXECRA_STATUS_UNKNOWN);
	}
}

static stonith_rsc_t *
get_started_stonith_resource(char * rsc_id )
{
	GList * tmplist = NULL;
	stonith_rsc_t * srsc = NULL;

	for ( tmplist = g_list_first(local_started_stonith_rsc); 
	      tmplist != NULL; 
	      tmplist = g_list_next(tmplist)) {
		srsc = (stonith_rsc_t *)tmplist->data;
		if (srsc != NULL && 
		    strncmp(srsc->rsc_id, rsc_id, MAXCMP) == 0) {
			return srsc;
		}
	}

	return NULL;
}

/**
 * (Deep-) frees the given client object (pointed to by _data_). In addition,
 * if the client's callback channel is not polled in g_main_loop, destroy the
 * channel (since it will not be auto-destroyed by clplumbing).
 */
static void 
free_client(gpointer data, gpointer user_data)
{
	stonithd_client_t * client = data;

	if ( client == NULL ) {
		return;
	}
	
	if ( client->name != NULL ) {
		g_free(client->name);
		client->name = NULL;
	}
	
	if ( client->removereason != NULL ) {
		g_free(client->removereason);
		client->removereason = NULL;
	}

	/* do not need to destroy them! */
	client->ch = NULL;
	client->cbch = NULL;

	g_free(client);
}

static stonithRA_ops_t * 
new_stonithRA_ops_t(struct ha_msg * request)
{
	stonithRA_ops_t * ra_op = NULL;
	int rc = ST_OK;
 
	ra_op = g_new0(stonithRA_ops_t, 1);
	st_save_string(request, F_STONITHD_RSCID, ra_op->rsc_id);
	st_save_string(request, F_STONITHD_RAOPTYPE, ra_op->op_type);
	st_save_string(request, F_STONITHD_RANAME, ra_op->ra_name);
	st_get_hashtable(request, F_STONITHD_PARAMS, ra_op->params);
	if( rc != ST_OK ) {
		free_stonithRA_ops_t(ra_op);
		return NULL;
	}
	return ra_op;
}

static void
free_stonithRA_ops_t(stonithRA_ops_t * ra_op)
{
	if (ra_op == NULL) {
		stonithd_log(LOG_DEBUG, "free_stonithRA_ops_t: ra_op==NULL");
		return;
	}

	ZAPGDOBJ(ra_op->rsc_id);
       	ZAPGDOBJ(ra_op->ra_name);
	ZAPGDOBJ(ra_op->op_type);
	
	stonithd_log2(LOG_DEBUG, "free_stonithRA_ops_t: ra_op->stonith_obj.=%p"
		     , (Stonith *)(ra_op->stonith_obj));
	if (ra_op->stonith_obj != NULL ) {
		return_to_orig_privs();
		stonith_delete((Stonith *)(ra_op->stonith_obj));
		return_to_dropped_privs();
	}
	/* Has used g_hash_table_new_full to create params */
	g_hash_table_destroy(ra_op->params);
	ZAPGDOBJ(ra_op);
}

static stonith_ops_t * 
dup_stonith_ops_t(stonith_ops_t * st_op)
{
	stonith_ops_t * ret;

	if ( NULL == st_op ) {
		return NULL;
	}

	ret = g_new0(stonith_ops_t, 1);
	ret->optype = st_op->optype;
	ret->node_name = g_strdup(st_op->node_name);
	ret->node_uuid = g_strdup(st_op->node_uuid);
	ret->timeout = st_op->timeout;
	ret->call_id = st_op->call_id;
	ret->op_result = st_op->op_result;
	/* In stonith daemon ( this file ), node_list is only a GString */
	ret->node_list = g_string_new( ((GString *)(st_op->node_list))->str );
	ret->private_data = g_strdup(st_op->private_data);

	return ret;
}

static stonith_ops_t * 
new_stonith_ops_t(struct ha_msg * msg)
{
	stonith_ops_t * st_op = NULL;
	const char * node_uuid = NULL;
	const char * pdata = NULL;
	int rc = ST_OK;
 
	st_op = g_new0(stonith_ops_t, 1);
	st_save_string(msg, F_STONITHD_NODE, st_op->node_name);
	st_get_int_value(msg, F_STONITHD_OPTYPE, (int *)&st_op->optype);
	st_get_int_value(msg, F_STONITHD_TIMEOUT, &st_op->timeout);
	if( rc != ST_OK ) {
		free_stonith_ops_t(st_op);
		return NULL;
	}

	/* The field sometimes is needed or not. Decided by its caller*/
	if ( HA_OK != ha_msg_value_int(msg, F_STONITHD_CALLID, &st_op->call_id)) { 
		stonithd_log2(LOG_DEBUG, "No F_STONITHD_CALLID field.");
	}

	if ((node_uuid = cl_get_string(msg, F_STONITHD_NODE_UUID))
		 == NULL ) {
		stonithd_log2(LOG_DEBUG, "The stonith requirement message"
			     " contains no target node UUID field.");
	}

	if ((pdata = cl_get_string(msg, F_STONITHD_PDATA)) == NULL ) {
		stonithd_log2(LOG_DEBUG, "The stonith requirement message"
			     " contains no F_STONITHD_PDATA field.");
	}

	st_op->node_list = g_string_new("");
	st_op->node_uuid = g_strdup(node_uuid);
	st_op->private_data = g_strdup(pdata);

	return st_op;
}

static void
free_stonith_ops_t(stonith_ops_t * st_op)
{
	if (st_op == NULL) {
		stonithd_log2(LOG_DEBUG, "free_stonith_ops_t: st_op==NULL");
		return;
	}

	stonithd_log2(LOG_DEBUG, "free_stonith_ops_t: begin.");
	ZAPGDOBJ(st_op->node_name);
	ZAPGDOBJ(st_op->node_uuid);

	ZAPGDOBJ(st_op->private_data);

	if (st_op->node_list != NULL) {
		g_string_free(st_op->node_list, TRUE);
		st_op->node_list = NULL;
	}
	ZAPGDOBJ(st_op);
	stonithd_log2(LOG_DEBUG, "free_stonith_ops_t: end.");
}

static void
free_stonith_rsc(stonith_rsc_t * srsc)
{
	if (srsc == NULL) {
		stonithd_log(LOG_DEBUG, "free_stonith_rsc: srsc==NULL");
		return;
	}
	
	stonithd_log2(LOG_DEBUG, "free_stonith_rsc: begin.");
	
	ZAPGDOBJ(srsc->rsc_id);
	ZAPGDOBJ(srsc->ra_name);
	stonithd_log2(LOG_DEBUG, "free_stonith_rsc: destroy params.");
	/* Has used g_hash_table_new_full to create params */
	g_hash_table_destroy(srsc->params);
	srsc->params = NULL;

	if (srsc->stonith_obj != NULL ) {
		stonithd_log2(LOG_DEBUG, "free_stonith_rsc: destroy stonith_obj.");
		return_to_orig_privs();
		stonith_delete(srsc->stonith_obj);
		return_to_dropped_privs();
		srsc->stonith_obj = NULL;
	}

	stonith_free_hostlist(srsc->node_list);
	srsc->node_list = NULL;

	ZAPGDOBJ(srsc);
	stonithd_log2(LOG_DEBUG, "free_stonith_rsc: finished.");
}

/**
 * Find in _client_list_ the client connected on the given ipc channel _ch_.
 * Either a messaging channel or a callback channel will do. If no client is
 * found to be connected on that channel, NULL is returned.
 */
static stonithd_client_t *
get_exist_client_by_chan(GList * client_list, IPC_Channel * ch)
{
	stonithd_client_t * client;
	GList * tmplist = NULL;
	
	stonithd_log2(LOG_DEBUG, "get_exist_client_by_chan: begin.");
	if ( !client_list || !ch ) {
		return NULL;
	} 

	for (tmplist = g_list_first(client_list); tmplist != NULL; 
	     tmplist = g_list_next(tmplist)) {
		stonithd_log2(LOG_DEBUG, "tmplist=%p", tmplist);
		client = (stonithd_client_t *)tmplist->data;
		if (client != NULL 
		    && (client->ch == ch || client->cbch == ch )) {
			stonithd_log2(LOG_DEBUG, "get_exist_client_by_chan: "
					"client %s.", client->name);
			return client;
		}
	}

	stonithd_log2(LOG_DEBUG, "get_exist_client_by_chan: end.");
	return NULL;
}

typedef GList * LIST_ITER;
#define LIST_FOREACH(obj, iter, list) \
	for (iter = g_list_first(list); \
	     iter && ((obj = iter->data) || 1); \
	     iter = g_list_next(iter))

/**
 * Find the client object with the given cookie and returns a pointer to it.
 * If the cookie is an empty string, or no client can be found, returns NULL. 
 */
static stonithd_client_t *
get_client_by_cookie(GList *client_list, cl_uuid_t *cookie)
{
	LIST_ITER		iter;
	stonithd_client_t * 	client;
	
	ST_ASSERT(client_list != NULL);
	ST_ASSERT(cookie != NULL);

	LIST_FOREACH(client, iter, client_list) {
		if (cl_uuid_compare(&client->cookie, cookie) == 0)
			return client;
	}
	return NULL;
}

#if 0
/* Remain it for future use */
static stonithd_client_t *
get_exist_client_by_pid(GList * client_list, pid_t pid)
{
	stonithd_client_t * client;
	GList * tmplist = NULL;

	if (client_list == NULL ) {
		stonithd_log(LOG_INFO, "get_exist_client_by_pid: "
				"client_list=NULL");
		return NULL;
	} 

	for ( tmplist = g_list_first(client_list); tmplist != NULL;
	      tmplist = g_list_next(tmplist)) {
		client = (stonithd_client_t *)tmplist->data;
		if (client != NULL && client->pid == pid) {
			stonithd_log(LOG_DEBUG, "client %s.", client->name);
			return client;
		}
	}

	return NULL;
}
#endif

/**
 * Delete the registered client connected on the given ipc channel. The 
 * channel can be either a messaging channel (client->ch) or a callback 
 * channel (client->cbch). 
 */
static int
delete_client_by_chan(GList ** client_list, IPC_Channel * ch)
{
	stonithd_client_t * client;

	client = get_exist_client_by_chan(*client_list, ch);
	if ( !client ) {
		stonithd_log2(LOG_DEBUG, "delete_client_by_chan: no client "
			"using this channel");
		return ST_FAIL;
	}
	stonithd_log2(LOG_DEBUG, "delete_client_by_chan: delete client "
		"%s (pid=%d)", client->name, client->pid);
	*client_list = g_list_remove(*client_list, client);
	free_client(client, NULL);
	
	client = NULL;
	stonithd_log2(LOG_DEBUG, "delete_client_by_chan: new "
		     "client_list = %p", *client_list);
	return ST_OK;
}

#if SUPPORT_HEARTBEAT
/* make the apphb_interval, apphb_warntime adjustable important */
static int
init_using_apphb(void)
{
	char stonithd_instance[40];

	snprintf(stonithd_instance, sizeof(stonithd_instance), "%s_%ldd"
	,	stonithd_name, (long)getpid());
        if (apphb_register(stonithd_name, stonithd_instance) != 0) {
                stonithd_log(LOG_ERR, "Failed when trying to register to apphbd.");
                stonithd_log(LOG_ERR, "Maybe apphd isnot running. Quit.");
                return LSB_EXIT_GENERIC;
        }
        stonithd_log(LOG_INFO, "Registered to apphbd.");

        apphb_setinterval(APPHB_INTERVAL);
        apphb_setwarn(APPHB_WARNTIME);

        Gmain_timeout_add(APPHB_INTERVAL - APPHB_INTVL_DETLA, emit_apphb, NULL);

	return 0;
}

static gboolean
emit_apphb(gpointer data)
{
	if (SIGNONED_TO_APPHBD == FALSE) {
		return FALSE;
	}

	if (apphb_hb() != 0) {
		SIGNONED_TO_APPHBD = FALSE;
		return FALSE;
	};

	return TRUE;
}
#endif

static int
show_daemon_status(const char * pidfile)
{
	if (cl_read_pidfile(STD_PIDFILE) > 0) {
		stonithd_log(LOG_INFO, "%s %s", stonithd_name, M_RUNNING);
		return 0;
	} else {
		stonithd_log(LOG_INFO, "%s seem stopped.", stonithd_name);
		return -1;
	}
}

static int
kill_running_daemon(const char * pidfile)
{
	pid_t pid;

	if ( (pid = cl_read_pidfile(STD_PIDFILE)) < 0 ) {
		stonithd_log(LOG_NOTICE, "cannot get daemon PID to kill it.");
		return LSB_EXIT_GENERIC;	
	}

	if (CL_KILL(pid, SIGTERM) == 0) {
		stonithd_log(LOG_INFO, "Signal sent to a runnig stonithd "
			"daemon, its pid is %ld,", (long)pid);
		return LSB_EXIT_OK;
	} else {
		stonithd_log(LOG_ERR, "Cannot kill the running stonithd "
			     "daemon, its pid is %ld.", (long)pid);
		return (errno == EPERM ? LSB_EXIT_EPERM : LSB_EXIT_GENERIC);
	} 
}

static void
destroy_key_of_op_htable(gpointer data)
{
	g_free((int*)data);
}

static void
free_common_op_t(gpointer data)
{
	common_op_t * op = (common_op_t *)data;

	if ( op == NULL ) {
		stonithd_log(LOG_ERR, "free_common_op_t: data==NULL");
		return;
	}

	stonithd_log2(LOG_DEBUG, "free_common_op_t: begin.");
	
	if ( op->scenario == STONITH_RA_OP ) {
		free_stonithRA_ops_t(op->op_union.ra_op);
		op->op_union.ra_op = NULL;
	} else {
		free_stonith_ops_t(op->op_union.st_op);
		op->op_union.st_op = NULL;
	}

	if ( op->scenario == STONITH_REQ ) {
		ZAPGDOBJ(op->result_receiver);
	}	

	if (op->rsc_id != NULL) {
		g_free(op->rsc_id);
		op->rsc_id = NULL;
	}

	if (op->timer_id != -1) {
		Gmain_timeout_remove(op->timer_id);
		op->timer_id = -1;
	}

	g_free(op);
	stonithd_log2(LOG_DEBUG, "free_common_op_t: end.");
}

static gboolean 
adjust_debug_level(int nsig, gpointer user_data)
{
	switch (nsig) {
		case SIGUSR1:
			debug_level++;
			if (debug_level > 2) {
				debug_level = 2;
			}
			break;

		case SIGUSR2:
			debug_level--;
			if (debug_level < 0) {
				debug_level = 0;
			}
			break;
		
		default:
			stonithd_log(LOG_WARNING, "adjust_debug_level: "
				"Something wrong?.");
	}

	return TRUE;
}

static void
trans_log(int priority, const char * fmt, ...)
{
	va_list         ap;
	char            buf[MAXLINE];

        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf)-1, fmt, ap);
        va_end(ap);
        cl_log(pil_loglevel_to_cl_loglevel[ priority % sizeof
		(pil_loglevel_to_cl_loglevel) ], "%s", buf);
}

/* vim:sw=8:ts=8
*/
