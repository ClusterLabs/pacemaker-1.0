/* $Id: stonithd.c,v 1.56 2005/06/20 16:20:33 sunjd Exp $ */

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

#include <config.h>
#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
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
#include <clplumbing/cl_malloc.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/realtime.h>
#include <apphb.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <hb_api.h>
#include <lrm/raexec.h>
#include <fencing/stonithd_msg.h>
#include <fencing/stonithd_api.h>
#include <clplumbing/cl_pidfile.h>
#include <clplumbing/Gmain_timeout.h>

/* For integration with heartbeat */
#define MAXCMP 80
#define MAGIC_EC 100
#define stonithd_log(priority, fmt...); \
	if ( debug_level == 0 && priority == LOG_DEBUG ) { \
		; \
	} else { \
		cl_log(priority, fmt); \
	}

/* Only export logs when debug_level >= 2 */
#define stonithd_log2(priority, fmt...); \
	if ( debug_level == 2 && priority == LOG_DEBUG ) { \
		cl_log(priority, fmt); \
	}

typedef struct {
	char * name;
	pid_t pid;		/* client pid */
	uid_t uid;		/* client UID */
	gid_t gid;		/* client GID */
	IPC_Channel * ch;
	char * removereason;
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
	void * data;	/* private data */
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
	"QUERY", "RESET", "POWERON", "POWERON"
};

static GList * client_list = NULL;
static GHashTable * executing_queue = NULL;
static GList * local_started_stonith_rsc = NULL;
static int negative_callid_counter = -2;

typedef int (*stonithd_api_msg_handler)(const struct ha_msg * msg,
					gpointer data);

struct api_msg_to_handler
{
	const char *			msg_type;
	stonithd_api_msg_handler	handler;
};

typedef int (*RA_subop_handler)(stonithRA_ops_t * ra_op, gpointer data);

struct RA_operation_to_handler
{
	const char *			op_type;
	RA_subop_handler		handler;
	RA_subop_handler		post_handler;
};

/* Miscellaneous functions such as daemon routines and others. */
static void become_daemon(gboolean);
static int show_daemon_status(const char * pidfile);
static int kill_running_daemon(const char * pidfile);
static void inherit_config_from_environment(void);
static void stonithd_quit(int signo);
static gboolean adjust_debug_level(int nsig, gpointer user_data);

/* Dealing with the child quit/abort event when executing STONTIH RA plugins. 
 */
static void stonithdProcessDied(ProcTrack* p, int status, int signo
				, int exitcode, int waslogged);
static void stonithdProcessRegistered(ProcTrack* p);
static const char * stonithdProcessName(ProcTrack* p);

/* For application heartbeat related */
static unsigned long
        APPHB_INTERVAL  = 2000, /* MS */
        APPHB_WARNTIME  = 6000, /* MS */
	APPHB_INTVL_DETLA = 30; /* MS */

#define MY_APPHB_HB \
	if (SIGNONED_TO_APPHBD == TRUE) { \
		if (apphb_hb() != 0) { \
			SIGNONED_TO_APPHBD = FALSE; \
		} \
	}

/* 
 * Functions related to application heartbeat ( apphbd )
 */
static gboolean emit_apphb(gpointer data);
static int init_using_apphb(void);

/* Communication between nodes related.
 * For stonithing one node in the cluster.
 */
static void handle_msg_twhocan(const struct ha_msg* msg, void* private_data);
static void handle_msg_ticanst(const struct ha_msg* msg, void* private_data);
static void handle_msg_tstit(const struct ha_msg* msg, void* private_data);
static void handle_msg_trstit(const struct ha_msg* msg, void* private_data);
static gboolean stonithd_hb_msg_dispatch(IPC_Channel * ch, gpointer user_data);
static void stonithd_hb_msg_dispatch_destroy(gpointer user_data);
static int init_hb_msg_handler(void);

/* Local IPC communication related.
 * For implementing the functions for client APIs 
 */
static gboolean stonithd_client_dispatch(IPC_Channel * ch, gpointer user_data);
static void stonithd_IPC_destroy_notify(gpointer data);
static gboolean accept_client_dispatch(IPC_Channel * ch, gpointer data);
static gboolean stonithd_process_client_msg(struct ha_msg * msg, 
					    gpointer data);
static int init_client_API_handler(void);
static void free_client(stonithd_client_t * client);
static stonithd_client_t * get_exist_client_by_chan(GList * client_list, 
						    IPC_Channel * ch);
static int delete_client_by_chan(GList ** client_list, IPC_Channel * ch);

/* Client API functions */
static int on_stonithd_signon(const struct ha_msg * msg, gpointer data);
static int on_stonithd_signoff(const struct ha_msg * msg, gpointer data);
static int on_stonithd_node_fence(const struct ha_msg * request, gpointer data);
static int on_stonithd_virtual_stonithRA_ops(const struct ha_msg * request, 
					  gpointer data);
static int on_stonithd_list_stonith_types(const struct ha_msg * request,
					  gpointer data);
static int stonithRA_operate(	stonithRA_ops_t * op, gpointer data );
static int stonithRA_start( stonithRA_ops_t * op, gpointer data );
static int stonithRA_stop( stonithRA_ops_t * op, gpointer data );
static int stonithRA_monitor( stonithRA_ops_t * op, gpointer data );
static int stonithRA_start_post( stonithRA_ops_t * op, gpointer data );
static int stonithop_result_to_local_client(stonith_ops_t * st_op, 
					    gpointer data);
static int send_stonithop_final_result( common_op_t * op );
static int stonithop_result_to_other_node( stonith_ops_t * st_op,
					   gpointer data);
static int send_stonithRAop_final_result(stonithRA_ops_t * ra_op, 
					 gpointer data);
static int post_handle_raop(stonithRA_ops_t * ra_op);
static void destory_key_of_op_htable(gpointer data);
static void free_stonithRA_ops_t(stonithRA_ops_t * ra_op);
static void free_stonith_ops_t(stonith_ops_t * st_op);
static void free_common_op_t(gpointer data);
static void free_stonith_rsc(stonith_rsc_t * srsc);
static stonith_rsc_t * get_started_stonith_resource(char * rsc_id);
static stonith_rsc_t * get_local_stonithobj_can_stonith(const char * node_name,
						const char * begin_rsc_id );
static int stonith_operate_locally(stonith_ops_t * st_op, stonith_rsc_t * srsc);
static gboolean stonithop_timeout(gpointer data);
static void my_hash_table_find( GHashTable * htable, gpointer * orig_key,
				gpointer * value, gpointer user_data);
static void has_this_callid(gpointer key, gpointer value,
				gpointer user_data);
static int require_others_to_stonith(stonith_ops_t * st_op);
static int initiate_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc, 
				    IPC_Channel * ch);
static int continue_local_stonithop(int old_key);
static int initiate_remote_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc,
				     IPC_Channel * ch);
static int changeto_remote_stonithop(int old_key);
static int require_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc, 
				   const char * asker_node);

static struct api_msg_to_handler api_msg_to_handlers[] = {
	{ ST_SIGNON,	on_stonithd_signon },
	{ ST_SIGNOFF,	on_stonithd_signoff },
	{ ST_STONITH,	on_stonithd_node_fence },
	{ ST_RAOP,	on_stonithd_virtual_stonithRA_ops },
	{ ST_LTYPES,	on_stonithd_list_stonith_types },
};

static struct RA_operation_to_handler raop_handler[] = {
	{ "start",	stonithRA_start, 	stonithRA_start_post },
	{ "stop",	stonithRA_stop,		NULL },
	{ "monitor",	stonithRA_monitor,	NULL },
	{ "status",	stonithRA_monitor,	NULL },
};

#define PID_FILE        HA_VARRUNDIR"/stonithd.pid"

/* define the message type between stonith daemons on different nodes */
#define T_WHOCANST  	"whocanst"	/* who can stonith a node */
#define T_ICANST	"icanst" 	/* I can stonith a node */	
#define T_STIT		"stit" 		/* please stonith it */	
#define T_RSTIT		"rstit" 	/* result of stonithing it */	
#define T_IALIVE 	"iamalive"	/* I'm alive */
#define T_QSTCAP	"qstcap"	/* query the stonith capacity -- 
					   all the nodes who can stonith */

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
#define STD_PIDFILE         "/var/run/stonithd.pid"

/* Do not need itselv's log file for real wotk, only for debugging
#define DAEMON_LOG      "/var/log/stonithd.log"
#define DAEMON_DEBUG    "/var/log/stonithd.debug"
*/

static ProcTrack_ops StonithdProcessTrackOps = {
	stonithdProcessDied,
	stonithdProcessRegistered,
	stonithdProcessName
};

static const char * 	local_nodename		= NULL;
static GMainLoop *	mainloop 		= NULL;
static const char * 	stonithd_name		= "stonithd";
static gboolean		STARTUP_ALONE 		= FALSE;
static gboolean 	SIGNONED_TO_HB		= FALSE;
static gboolean 	SIGNONED_TO_APPHBD	= FALSE;
static gboolean 	NEED_SIGNON_TO_APPHBD	= FALSE;
static ll_cluster_t *	hb			= NULL;
static gboolean 	TEST 			= FALSE;
static IPC_Auth* 	ipc_auth 		= NULL;
static int	 	debug_level		= 0;
static int 		stonithd_child_count	= 0;

int
main(int argc, char ** argv)
{
	int main_rc = LSB_EXIT_OK;
	int option_char;

	cl_malloc_forced_for_glib(); /* Force more memory checks */
        cl_log_set_entity(stonithd_name);
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(LOG_DAEMON);
        /* Use logd if it's enabled by heartbeat */
        cl_inherit_use_logd(ENV_PREFIX ""KEY_LOGDAEMON, 0);

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
				NEED_SIGNON_TO_APPHBD = TRUE;
				break;

			 /* Get the interval to emitting the application 
			    hearbteat to apphbd.
			  */
			case 'i':
				if (optarg) {
					APPHB_INTERVAL = atoi(optarg);		
					APPHB_WARNTIME = 3*APPHB_INTERVAL;
				}
				break;
			
			
			case 's': /* Show daemon status */
				return show_daemon_status(STD_PIDFILE);
				break; /* Never reach here, just for uniform */

			case 'k': /* kill the running daemon */
				return(kill_running_daemon(STD_PIDFILE));
				break; /* Never reach here, just for uniform */

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
				stonithd_log(LOG_ERR, "Error:getopt returned"
					" character code %c.", option_char);
				printf("%s\n", simple_help_screen);
				return (STARTUP_ALONE == TRUE) ? 
					LSB_EXIT_EINVAL : MAGIC_EC;
		}
	} while (1);

	inherit_config_from_environment();
	

	if (cl_read_pidfile(STD_PIDFILE) > 0 ) {
		stonithd_log(LOG_NOTICE, "%s %s", argv[0], M_RUNNING);
		return (STARTUP_ALONE == TRUE) ? LSB_EXIT_OK : MAGIC_EC;
	}

	/* Not use daemon() API, since it's not POSIX compliant */
	become_daemon(STARTUP_ALONE);

	hb = ll_cluster_new("heartbeat");
	if ( hb == NULL ) {
		stonithd_log(LOG_ERR, "ll_cluster_new failed.");
		stonithd_log(LOG_ERR, "%s %s", argv[0], M_ABORT);
		return (STARTUP_ALONE == TRUE) ? LSB_EXIT_GENERIC : MAGIC_EC;
	}

	if (hb->llc_ops->signon(hb, stonithd_name)!= HA_OK) {
		stonithd_log(LOG_ERR, "Cannot signon with heartbeat");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		goto delhb_quit;
	} else {
		stonithd_log(LOG_INFO, "Signing in with heartbeat.");
		SIGNONED_TO_HB = TRUE;
		local_nodename = hb->llc_ops->get_mynodeid(hb);
		if (local_nodename == NULL) {
			stonithd_log(LOG_ERR, "Cannot get local node id");
			stonithd_log(LOG_ERR, "REASON: %s", 
				     hb->llc_ops->errmsg(hb));
			main_rc = LSB_EXIT_GENERIC;
			goto signoff_quit;
		}
	}

	mainloop = g_main_new(FALSE);

	/*
	 * Initialize the handler of IPC messages from heartbeat, including
	 * the messages produced by myself as a client of heartbeat.
	 */
	if ( (main_rc = init_hb_msg_handler()) != 0) {
		stonithd_log(LOG_ERR, "An error in init_hb_msg_handler.");
		goto signoff_quit;
	}
	
	/*
	 * Initialize the handler of IPC messages from my clients.
	 */
	if ( (main_rc = init_client_API_handler()) != 0) {
		stonithd_log(LOG_ERR, "An error in init_hb_msg_handler.");
		goto delhb_quit;
	}

	if (NEED_SIGNON_TO_APPHBD == TRUE) {
		if ( (main_rc=init_using_apphb()) != 0 ) {
			stonithd_log(LOG_ERR, "An error in init_using_apphb");
			goto signoff_quit;
		} else {
			SIGNONED_TO_APPHBD = TRUE;
		}
	}

	/* Initialize some global variables */
	executing_queue = g_hash_table_new_full(g_int_hash, g_int_equal,
						 destory_key_of_op_htable,
						 free_common_op_t);
	/* To avoid the warning message when app_interval is very small. */
	MY_APPHB_HB
	/* drop_privs(0, 0); */  /* very important. cpu limit */
	stonithd_log2(LOG_DEBUG, "Enter g_mainloop\n");
	stonithd_log(LOG_NOTICE, "%s %s", argv[0], M_STARTUP );

	cl_set_all_coredump_signal_handlers(); 
	set_sigchld_proctrack(G_PRIORITY_HIGH);
	drop_privs(0, 0); /* become "nobody" */
	g_main_run(mainloop);
	return_to_orig_privs();

	MY_APPHB_HB
	if (SIGNONED_TO_APPHBD == TRUE) {
		apphb_unregister();
		SIGNONED_TO_APPHBD = FALSE;
	}

signoff_quit:
	if (hb != NULL && hb->llc_ops->signoff(hb, FALSE) != HA_OK) {
		stonithd_log(LOG_ERR, "Cannot sign off from heartbeat.");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		main_rc = LSB_EXIT_GENERIC;
	}

delhb_quit:
	if (hb != NULL) {
		if (hb->llc_ops->delete(hb) != HA_OK) {
			stonithd_log(LOG_ERR, "Cannot delete API object.");
			stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			main_rc = LSB_EXIT_GENERIC;
		}
	}

	g_list_free(client_list); /* important. Others! free each item! pls check */
	g_hash_table_destroy(executing_queue); /* ? */
	if ( NULL != ipc_auth ) {
		g_hash_table_destroy(ipc_auth->uid); /* ? */
		cl_free(ipc_auth); /* ? */
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
become_daemon(gboolean startup_alone)
{
	pid_t pid;
	int j;

	if (startup_alone == TRUE) {
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

static void
stonithdProcessDied(ProcTrack* p, int status, int signo
		    , int exitcode, int waslogged)
{
	const char * pname = p->ops->proctype(p);
	gboolean rc;
	common_op_t * op = NULL;
	int * orignal_key = NULL;

	stonithd_log2(LOG_DEBUG, "stonithdProcessDied: begin"); 
	stonithd_child_count--;
	stonithd_log2(LOG_DEBUG, "there still are %d child process running"
			, stonithd_child_count);
	stonithd_log(LOG_DEBUG, "child process %s[%d] exited, its exit code: %d"
		     " when signo=%d.", pname, p->pid, exitcode, signo);

	rc = g_hash_table_lookup_extended(executing_queue, &(p->pid) 
			, (gpointer *)&orignal_key, (gpointer *)&op);

	if (rc == FALSE) {
		stonithd_log(LOG_NOTICE, "received child quit signal, "
			"but no this child pid in excuting list.");
		goto end;
	}

	if (op == NULL) {
		stonithd_log(LOG_ERR, "received child quit signal: "
			"but op==NULL");
		goto end;
	}

	if ( op->scenario == STONITH_RA_OP ) {
		if (op->op_union.ra_op == NULL || op->result_receiver == NULL) {
			stonithd_log(LOG_ERR,   "stonithdProcessDied: "
						"op->op_union.ra_op == NULL or "
						"op->result_receiver == NULL");
			goto end;
		}

		op->op_union.ra_op->call_id = p->pid;
		op->op_union.ra_op->op_result = exitcode;
		send_stonithRAop_final_result(op->op_union.ra_op, 
				      op->result_receiver);
		post_handle_raop(op->op_union.ra_op);
		g_hash_table_remove(executing_queue, &(p->pid));
		goto end;
	}

	/* next is stonith operation related */
	if ( op->scenario == STONITH_INIT || op->scenario == STONITH_REQ ) {
		if (op->op_union.st_op == NULL || op->result_receiver == NULL ) {
			stonithd_log(LOG_ERR, "stonithdProcessDied: "
					      "op->op_union.st_op == NULL or "
					      "op->result_receiver == NULL");
			goto end;
		}
			
		if (exitcode == S_OK) {
			op->op_union.st_op->op_result = STONITH_SUCCEEDED;
			op->op_union.st_op->node_list = 
				g_string_append(op->op_union.st_op->node_list
						, local_nodename);
			send_stonithop_final_result(op); 
			g_hash_table_remove(executing_queue, &(p->pid));
			goto end;
		}
		/* Go ahead when exitcode != S_OK */
		stonithd_log(LOG_DEBUG, "Failed to STONITH node %s with one " 
			"local device, exitcode = %d. Will try to use the "
			"next local device."
			,	op->op_union.st_op->node_name, exitcode); 
		if (ST_OK == continue_local_stonithop(p->pid)) {
			goto end;
		} else {
			stonithd_log(LOG_DEBUG, "Failed to STONITH node %s "
				"locally.", op->op_union.st_op->node_name);
			if (op->scenario == STONITH_INIT) {
				stonithd_log(LOG_DEBUG, "Will ask other nodes "
					"to help STONITH node %s."
					,	op->op_union.st_op->node_name); 
			}
			if (changeto_remote_stonithop(p->pid) != ST_OK) {
				op->op_union.st_op->op_result = STONITH_GENERIC;
				send_stonithop_final_result(op);
				g_hash_table_remove(executing_queue, &(p->pid));
			}
		}		
	}
end:
	g_free(p->privatedata);
	p->privatedata = NULL;
}

static void
stonithdProcessRegistered(ProcTrack* p)
{
	stonithd_child_count++;
	stonithd_log(LOG_DEBUG, "Child process [%s] started [ pid: %d ]."
		     , p->ops->proctype(p), p->pid);
	stonithd_log2(LOG_DEBUG, "there are %d child process running"
			, stonithd_child_count);
}

static const char * 
stonithdProcessName(ProcTrack* p)
{
	gchar * process_name = p->privatedata;
	return  process_name;
}

static int
init_hb_msg_handler(void)
{
	IPC_Channel * hbapi_ipc = NULL;
	unsigned int msg_mask;
	
	stonithd_log2(LOG_DEBUG, "init_hb_msg_handler: begin");

	/* Set message callback */
	if (hb->llc_ops->set_msg_callback(hb, T_WHOCANST, 
				  handle_msg_twhocan, hb) != HA_OK) {
		stonithd_log(LOG_ERR, "Cannot set msg T_WHOCANST callback");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return LSB_EXIT_GENERIC;
	}

	if (hb->llc_ops->set_msg_callback(hb, T_ICANST, 
				  handle_msg_ticanst, hb) != HA_OK) {
		stonithd_log(LOG_ERR, "Cannot set msg T_ICANST callback");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return LSB_EXIT_GENERIC;
	}

	if (hb->llc_ops->set_msg_callback(hb, T_STIT, 
				  handle_msg_tstit, hb) != HA_OK) {
		stonithd_log(LOG_ERR, "Cannot set msg T_STIT callback");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return LSB_EXIT_GENERIC;
	}

	if (hb->llc_ops->set_msg_callback(hb, T_RSTIT, 
				  handle_msg_trstit, hb) != HA_OK) {
		stonithd_log(LOG_ERR, "Cannot set msg T_WHOCANST callback");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return LSB_EXIT_GENERIC;
	}

	msg_mask = LLC_FILTER_DEFAULT;
	cl_log(LOG_DEBUG, "Setting message filter mode");
	if (hb->llc_ops->setfmode(hb, msg_mask) != HA_OK) {
		stonithd_log(LOG_ERR, "Cannot set filter mode");
		stonithd_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return LSB_EXIT_GENERIC;
	}

	if (NULL == (hbapi_ipc = hb->llc_ops->ipcchan(hb))) {
		stonithd_log(LOG_ERR,"Cannot get hb's client IPC channel.");
		return LSB_EXIT_GENERIC;
	}
	/* Watch the hearbeat API's IPC channel for input */
	G_main_add_IPC_Channel(G_PRIORITY_HIGH, hbapi_ipc, FALSE, 
				stonithd_hb_msg_dispatch, (gpointer)hb, 
				stonithd_hb_msg_dispatch_destroy);

	return 0;
}


static gboolean
stonithd_hb_msg_dispatch(IPC_Channel * ipcchan, gpointer user_data)
{
	ll_cluster_t *hb = (ll_cluster_t*)user_data;
    
	while (hb->llc_ops->msgready(hb)) {
		/* FIXME:  This should be a higher-level API function (broken API) */
		if (hb->llc_ops->ipcchan(hb)->ch_status == IPC_DISCONNECT) {
			stonithd_quit(0);
		}
		/* invoke the callbacks with none-block mode */
		stonithd_log2(LOG_DEBUG, "there are hb msg received.");
		hb->llc_ops->rcvmsg(hb, 0);
	}
	/* deal with some abnormal errors/bugs? */
	return TRUE;
}

static void
stonithd_hb_msg_dispatch_destroy(gpointer user_data)
{
	return;
}

static void
handle_msg_twhocan(const struct ha_msg* msg, void* private_data)
{
	const char * target = NULL;
	const char * from = NULL;
	int call_id;

	stonithd_log(LOG_DEBUG, "I got T_WHOCANST msg.");	
	if ( (from = cl_get_string(msg, F_ORIG)) == NULL) {
		stonithd_log(LOG_ERR, "handle_msg_twhocan: no F_ORIG field.");
		return;
	}

	/* Don't handle the message sent by myself when not in TEST mode */
	if ( strncmp(from, local_nodename, MAXCMP) 
		== 0) {
		stonithd_log(LOG_DEBUG, "received a T_WHOCANST msg from myself.");
		if (TEST == FALSE) {
			return;
		} else {
			stonithd_log(LOG_DEBUG, "In TEST mode, so continue.");
		}
	}

	if ( (target = cl_get_string(msg, F_STONITHD_NODE)) == NULL) {
		stonithd_log(LOG_ERR, "handle_msg_twhocan: no F_STONITHD_NODE "
			     "field.");
		return;
	}

	if (HA_OK != ha_msg_value_int(msg, F_STONITHD_CALLID, &call_id)) {
		stonithd_log(LOG_ERR, "handle_msg_twhocan: no F_STONITHD_CALLID "
			     "field.");
		return;
	}

	if (get_local_stonithobj_can_stonith(target, NULL) != NULL ) {
		struct ha_msg * reply;

		if ((reply = ha_msg_new(1)) == NULL) {
			stonithd_log(LOG_ERR, "handle_msg_twhocan: "
					"out of memory");
			return;
		}

		if ( (ha_msg_add(reply, F_TYPE, T_ICANST) != HA_OK)
	    	    ||(ha_msg_add(reply, F_ORIG, local_nodename) != HA_OK)
	    	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, call_id)
			!= HA_OK)) {
			stonithd_log(LOG_ERR, "handle_msg_twhocan: "
					"cannot add field.");
			ZAPMSG(reply);
			return;
		}

		if (hb == NULL) {
			stonithd_log(LOG_ERR, "handle_msg_twhocan: hb==NULL");
			ZAPMSG(reply);
			return;
		}

		if (HA_OK!=hb->llc_ops->sendnodemsg(hb, reply, from)) {
			stonithd_log(LOG_ERR, "handle_msg_twhocan: "
					"sendnodermsg failed.");
			return;
		}
		stonithd_log(LOG_DEBUG, "handle_msg_twhocan: I can stonith "
			     "node %s.", target);
		stonithd_log(LOG_DEBUG,"handle_msg_twhocan: "
				"send reply successfully.");
		ZAPMSG(reply);
	} else {
		stonithd_log(LOG_DEBUG, "handle_msg_twhocan: I cannot stonith "
			     "node %s.", target);
	}
}

static void
handle_msg_ticanst(const struct ha_msg* msg, void* private_data)
{
	const char * from = NULL;
	int  call_id;
	int * orig_key = NULL;
	common_op_t * op = NULL;

	stonithd_log(LOG_DEBUG, "handle_msg_ticanst: got T_ICANST msg.");
	if ( (from = cl_get_string(msg, F_ORIG)) == NULL) {
		stonithd_log(LOG_ERR, "handle_msg_ticanst: no F_ORIG field.");
		return;
	}

	/* Don't handle the message sent by myself when not TEST mode */
	if ( (strncmp(from, local_nodename, MAXCMP) 
		== 0) && ( TEST == FALSE ) ){
		stonithd_log(LOG_DEBUG, "received a T_ICANST msg from myself.");
		return;
	}

	if ( HA_OK != ha_msg_value_int(msg, F_STONITHD_CALLID, &call_id) ) { 
		stonithd_log(LOG_ERR, "handle_msg_ticanst: no F_STONITHD_CALLID"
			     " field.");
		return;
	}

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
	}
	/* wait the timeout function to send back the result */
}

static void
handle_msg_tstit(const struct ha_msg* msg, void* private_data)
{
	const char * target = NULL;
	const char * from = NULL;
	int call_id;
	int timeout;
	int optype;
	stonith_rsc_t * srsc = NULL;
	stonith_ops_t * st_op = NULL;

	stonithd_log(LOG_DEBUG, "handle_msg_tstit: got T_STIT msg.");	
	if ( (from = cl_get_string(msg, F_ORIG)) == NULL) {
		stonithd_log(LOG_ERR, "handle_msg_tstit: no F_ORIG field.");
		return;
	}

	/* Don't handle the message sent by myself */
	if ( strncmp(from, local_nodename, MAXCMP) 
		== 0 && TEST == FALSE ) {
		stonithd_log(LOG_DEBUG, "received a T_STIT msg from myself, "
			     "will abandon it.");
		return;
	}

	if ( (target = cl_get_string(msg, F_STONITHD_NODE)) == NULL) {
		stonithd_log(LOG_ERR, "handle_msg_tstit: no F_STONITHD_NODE "
			     "field.");
		return;
	}

	/* Don't handle the message about stonithing myself */
	if ( strncmp(target, local_nodename, MAXCMP) 
		== 0) {
		stonithd_log(LOG_DEBUG, "received a T_STIT message to require "
				"to stonith myself.");
		if (TEST == FALSE) {
			return;
		} else {
			stonithd_log(LOG_DEBUG, "In TEST mode, continue even "
				     "to stonith myself.");
		}
	}

	if ( HA_OK != ha_msg_value_int(msg, F_STONITHD_OPTYPE, &optype)) {
		stonithd_log(LOG_ERR, "handle_msg_tstit: no F_STONITHD_OPTYPE "
			     "field.");
		return;
	}

	if ( HA_OK != ha_msg_value_int(msg, F_STONITHD_CALLID, &call_id)) { 
		stonithd_log(LOG_ERR, "handle_msg_tstit: no F_STONITHD_CALLID "
			     "field.");
		return;
	}

	if ( HA_OK != ha_msg_value_int(msg, F_STONITHD_TIMEOUT, &timeout)) {
		stonithd_log(LOG_ERR, "handle_msg_tstit: no F_STONITHD_TIMEOUT "
			     "field.");
		return;
	}

	if ((srsc = get_local_stonithobj_can_stonith(target, NULL)) != NULL ) {
		st_op = g_new(stonith_ops_t, 1);
		st_op->node_list = g_string_new("");
		st_op->node_name = g_strdup(target);
		st_op->optype  = optype;
		st_op->call_id = call_id;
		st_op->timeout = timeout;
		if (ST_OK != require_local_stonithop(st_op, srsc, from)) {
			free_stonith_ops_t(st_op);
			st_op = NULL;
		}
	}
}

static int
require_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc,
			const char * asker)
{
	int * child_id;

	child_id = g_new(int, 1);
	if ((*child_id = stonith_operate_locally(st_op, srsc)) <= 0) {
		stonithd_log(LOG_ERR, "require_local_stonithop: "
			"stonith_operate_locally failed.");
		ZAPGDOBJ(child_id);
		return ST_FAIL;
	} else {
		common_op_t * op;
		int * tmp_callid;

		op = g_new(common_op_t, 1);
		op->scenario = STONITH_REQ;
		op->result_receiver = g_strdup(asker); 
		op->data = srsc->rsc_id;
		op->op_union.st_op = st_op;
		g_hash_table_insert(executing_queue, child_id, op);
		tmp_callid = g_new(int, 1);
		*tmp_callid = *child_id;
		Gmain_timeout_add_full(G_PRIORITY_HIGH_IDLE, st_op->timeout,
		   		   stonithop_timeout, tmp_callid, NULL);
		stonithd_log(LOG_DEBUG, "require_local_stonithop: inserted "
			    "optype=%d, child_id=%d", st_op->optype, *child_id);
		return ST_OK;
	}
}

static void
handle_msg_trstit(const struct ha_msg* msg, void* private_data)
{
	const char * from = NULL;
	int call_id;
	int op_result;
	int * orig_key = NULL;
	common_op_t * op = NULL;

	stonithd_log(LOG_DEBUG, "handle_msg_trstit: got T_RSTIT msg.");	
	if ( (from = cl_get_string(msg, F_ORIG)) == NULL) {
		stonithd_log(LOG_ERR, "handle_msg_trstit: no F_ORIG field.");
		return;
	}
	stonithd_log(LOG_DEBUG, "This T_RSTIT message is from %s.", from);	

	/* Don't handle the message sent by myself when not in TEST mode */
	if ( TEST == FALSE &&
	     strncmp(from, local_nodename, MAXCMP) == 0) {
		stonithd_log(LOG_DEBUG, "received a T_RSTIT msg from myself.");
		return;
	}

	if ( HA_OK != ha_msg_value_int(msg, F_STONITHD_CALLID, &call_id)) {
		stonithd_log(LOG_ERR, "handle_msg_trstit: no F_STONITHD_CALLID "
			     "field.");
		return;
	}

	if ( HA_OK != ha_msg_value_int(msg, F_STONITHD_FRC, &op_result)) {
		stonithd_log(LOG_ERR, "handle_msg_trstit: no F_STONITHD_FRC "
			     "field.");
		return;
	}

	my_hash_table_find(executing_queue, (gpointer *)&orig_key, 
			   (gpointer *)&op, &call_id);
	if ( op != NULL && 
	    (op->scenario == STONITH_INIT || op->scenario == STONITH_REQ)) {
		op->op_union.st_op->op_result = op_result;
		op->op_union.st_op->node_list = 
			g_string_append(op->op_union.st_op->node_list, from);
		send_stonithop_final_result(op);
		stonithd_log(LOG_DEBUG, "handle_msg_trstit: clean the "
			     "executing queue.");
		g_hash_table_remove(executing_queue, orig_key);
	} else {
		stonithd_log(LOG_DEBUG, "handle_msg_trstit: the stonith "
			"operation (call_id==%d) has finished before "
			"receiving this message", call_id);
	}

	stonithd_log2(LOG_DEBUG, "handle_msg_trstit: end");	
}

static int 
init_client_API_handler(void)
{
	GHashTable*		chanattrs;
	IPC_WaitConnection*	apichan = NULL;
	char			path[] = IPC_PATH_ATTR;
	char			sock[] = STONITHD_SOCK;
	struct passwd*  	pw_entry;
	GWCSource*		api_source;
	GHashTable*     	uid_hashtable;
	int             	tmp = 1;

	stonithd_log2(LOG_DEBUG, "init_client_API_handler: begin");

	chanattrs = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(chanattrs, path, sock);
	apichan = ipc_wait_conn_constructor(IPC_DOMAIN_SOCKET, chanattrs);
	if (apichan == NULL) {
		stonithd_log(LOG_ERR, "Cannot open stonithd's api socket: %s",
				sock);
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
        /* Add root;s uid */
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
	api_source = G_main_add_IPC_WaitConnection(G_PRIORITY_HIGH, apichan,
				ipc_auth, FALSE, accept_client_dispatch, NULL, NULL);

	if (api_source == NULL) {
		cl_log(LOG_DEBUG, "Cannot create API listening source of "
			"server side from IPC");
		return	LSB_EXIT_GENERIC;
	}

	return 0;
}

static gboolean
accept_client_dispatch(IPC_Channel * ch, gpointer user)
{
	if (ch == NULL) {
		stonithd_log(LOG_ERR, "IPC accepting a connection failed.");
		return FALSE;
	}

	stonithd_log(LOG_DEBUG, "IPC accepted a connection.");
	G_main_add_IPC_Channel(G_PRIORITY_HIGH, ch, FALSE, 
			stonithd_client_dispatch, (gpointer)ch,
			stonithd_IPC_destroy_notify);

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
			stonithd_log(LOG_DEBUG, "IPC disconneted with a client.");
#if 0
			/* For verify the API use */
			if  ((msg = msgfromIPC_noauth(ch)) == NULL ) {
				/* Must be here */
				stonithd_log(LOG_ERR, "2. Failed when receiving IPC messages.");
				return FALSE;
			}
#endif 
			stonithd_log(LOG_DEBUG, "stonithd_client_dispatch: "
				"delete a client due to IPC_DISCONNECT.");
			delete_client_by_chan(&client_list, ch); /* ??? */
			return FALSE;
		}

		/* Authority issue ? */
		if  ((msg = msgfromIPC_noauth(ch)) == NULL ) {
			stonithd_log(LOG_ERR, "Failed when receiving IPC messages.");
			return FALSE;
		} else {
			stonithd_process_client_msg(msg, (gpointer)ch);
		}
	}
			
	return TRUE;
}

static void
stonithd_IPC_destroy_notify(gpointer data)
{
	IPC_Channel * ch = (IPC_Channel *) data;

	/* deal with client disconnection event */
	stonithd_log(LOG_DEBUG, "An IPC is destroyed.");

	if (ch == NULL) {
		stonithd_log(LOG_DEBUG, "IPC_destroy_notify: ch==NULL");
		return;
	}

	if ( ST_OK == delete_client_by_chan(&client_list, ch) ) {
		stonithd_log(LOG_DEBUG, "Delete a client from client_list "
			"in stonithd_IPC_destroy_notify.");
	} else {
		stonithd_log(LOG_DEBUG, "stonithd_IPC_destroy_notify: Failed "
			"to delete a client from client_list, maybe it has "
			"been deleted in signoff function.");
	}
}

static gboolean
stonithd_process_client_msg(struct ha_msg * msg, gpointer data)
{
	const char * msg_type = NULL;
	const char * api_type = NULL;
	IPC_Channel * ch = (IPC_Channel *) data;
	int i, rc;
	
	if (  ((msg_type = cl_get_string(msg, F_STONITHD_TYPE)) != NULL)
	    && (STRNCMP_CONST(msg_type, ST_APIREQ) == 0) ) {
		stonithd_log(LOG_DEBUG, "received an API request msg.");	
	} else {
		stonithd_log(LOG_ERR, "received a msg of none-API request.");
        	ZAPMSG(msg);
        	return TRUE;
	}	

	if ( (api_type = cl_get_string(msg, F_STONITHD_APIREQ)) == NULL) {
		stonithd_log(LOG_ERR, "got an incorrect api request msg.");
		ZAPMSG(msg);
		return TRUE;
	}

	stonithd_log(LOG_DEBUG, "begin to dealing with a api msg %s from "
			"a client.", api_type);
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
on_stonithd_signon(const struct ha_msg * request, gpointer data)
{
	stonithd_client_t * client = NULL;
	struct ha_msg * reply;
	const char * api_reply = ST_APIOK;
	const char * tmpstr = NULL;
	int  tmpint;
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
	client = g_new(stonithd_client_t, 1);
	client->pid = tmpint;
	client->ch = ch;
	client->removereason = NULL;

	if (HA_OK == ha_msg_value_int(request, F_STONITHD_CEUID, &tmpint)) {
		client->uid = tmpint;
	} else {
		stonithd_log(LOG_ERR, "signon msg contains no or incorrect "
				"UID field.");
		api_reply = ST_BADREQ;
		free_client(client);
		client = NULL;
		goto send_back_reply; 
	}

	if (HA_OK == ha_msg_value_int(request, F_STONITHD_CEGID, &tmpint)) {
		client->gid = tmpint;
	} else {
		stonithd_log(LOG_ERR, "signon msg contains no or incorrect "
					"GID field.");
		api_reply = ST_BADREQ;
		free_client(client);
		client = NULL;
		goto send_back_reply; 
	}

	if ((tmpstr = cl_get_string(request, F_STONITHD_CNAME)) != NULL) {
		client->name = g_strdup(tmpstr);
		stonithd_log(LOG_DEBUG, "client->name=%s.", client->name);
	} else {
		stonithd_log(LOG_ERR, "signon msg contains no name field.");
		api_reply = ST_BADREQ;
		free_client(client);
		client = NULL;
		goto send_back_reply; 
	}
	
	/* lack the authority check from uid&gid */
	/* add the client to client list */
 	if ( STRNCMP_CONST(api_reply, ST_APIOK) == 0 ) {
		client_list = g_list_append(client_list, client);
		stonithd_log(LOG_DEBUG,"client %s (pid=%d) succeeded to "
			"signon to stonithd.", client->name, client->pid);
	} else {
		stonithd_log(LOG_ERR, "signon failed.");
		free_client(client);
		client = NULL;
	}
	
send_back_reply:
	reply = ha_msg_new(1);
	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RSIGNON) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK ) ) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_signon: cannot add field.");
		return ST_FAIL;
	}
	
	if (msg2ipcchan(reply, ch) != HA_OK) { /* How to deal the error*/
		ZAPCHAN(ch);
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "can't send signon reply message to IPC");
		return ST_FAIL;
	}

	ZAPMSG(reply);
	return ST_OK;
}

static int
on_stonithd_signoff(const struct ha_msg * request, gpointer data)
{
	struct ha_msg * reply;
	const char * api_reply = ST_APIOK;
	stonithd_client_t * client = NULL;
	IPC_Channel * ch = (IPC_Channel *) data;
	int tmpint;

	stonithd_log(LOG_DEBUG, "on_stonithd_signoff: begin.");
	/* parameter check, maybe redundant */
	if ( ch == NULL || request == NULL ) {
		stonithd_log(LOG_ERR, "parameter error, signoff failed.");
		return ST_FAIL;
	}

	if ( HA_OK != ha_msg_value_int(request, F_STONITHD_CPID, &tmpint) ) {
		stonithd_log(LOG_ERR, "signoff msg contains no or incorrect "
				"PID field.");
		api_reply = ST_BADREQ;
		goto send_back_reply;
	}

	if ( (client = get_exist_client_by_chan(client_list, ch)) != NULL ) {
		if (client->pid == tmpint) {
			stonithd_log(LOG_DEBUG, "have the client %s (pid=%d) "
				     "sign off.", client->name, client->pid);
			delete_client_by_chan(&client_list, ch);
			client = NULL;
		} else {
			stonithd_log(LOG_NOTICE, "The client's channel isnot "
				     "correspond to the former pid(%d). It "
				     "seems a child is using its parent's "
				     "IPC channel.", client->pid);
		}
	} else {
		stonithd_log(LOG_NOTICE, "The client pid=%d seems already "
			     "signed off ", tmpint);
	}

 	if ( STRNCMP_CONST(api_reply, ST_APIOK) == 0 ) {
		stonithd_log(LOG_DEBUG,"client pid=%d has sign off stonithd "
				"succeedly.", tmpint);
	} else {
		stonithd_log(LOG_INFO,"client pid=%d failed to sign off "
				"stonithd ", tmpint);
	}

send_back_reply:
	reply = ha_msg_new(3);
	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RSIGNOFF) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK ) ) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_signoff: cannot add field.");
		return ST_FAIL;
	}
	
	if (msg2ipcchan(reply, ch) != HA_OK) { /* How to deal the error*/
		ZAPCHAN(ch); /* ? */
		ZAPMSG(reply);
		stonithd_log(LOG_WARNING, "can't send signoff reply message to IPC");
		return ST_FAIL;
        }

	ZAPMSG(reply);
	return ST_OK;
}

static int
on_stonithd_node_fence(const struct ha_msg * request, gpointer data)
{
	const char * api_reply = ST_APIOK;
	IPC_Channel * ch = (IPC_Channel *) data;
	stonith_ops_t * st_op = NULL;
	struct ha_msg * reply;
	int tmpint;
	const char * tmpstr = NULL; 
	int call_id = 0;
	stonithd_client_t * client = NULL;
	stonith_rsc_t * srsc = NULL;

	stonithd_log2(LOG_DEBUG, "stonithd_node_fence: begin.");
	/* parameter check, maybe redundant */
	if ( ch == NULL || request == NULL ) {
		stonithd_log(LOG_ERR, "stonithd_node_fence: parameter error.");
		return ST_FAIL;
	}

	/* Check if have signoned */
	if ((client = get_exist_client_by_chan(client_list, ch)) == NULL ) {
		stonithd_log(LOG_ERR, "stonithd_node_fence: not signoned yet.");
		return ST_FAIL;
	}

	st_op = g_new(stonith_ops_t, 1);
	st_op->node_list = g_string_new("");
	st_op->node_uuid = NULL;

	if ( HA_OK == ha_msg_value_int(request, F_STONITHD_OPTYPE, &tmpint)) {
		st_op->optype = tmpint;
	} else {
		stonithd_log(LOG_ERR, "The stonith requirement message contains"
			     " no operation type field.");
		api_reply = ST_BADREQ;
		g_free(st_op);
		goto sendback_reply;
	}

	if (HA_OK == ha_msg_value_int(request, F_STONITHD_TIMEOUT, &tmpint)) {
		st_op->timeout = tmpint;	
	} else {
		stonithd_log(LOG_ERR, "The stonith requirement message contains"
			     " no timeout field.");
		api_reply = ST_BADREQ;
		g_free(st_op);
		goto sendback_reply;
	}

	if ((tmpstr = cl_get_string(request, F_STONITHD_NODE)) != NULL ) {
		st_op->node_name = g_strdup(tmpstr);	
	} else {
		stonithd_log(LOG_ERR, "The stonith requirement message contains"
			     " no target node name field.");
		api_reply = ST_BADREQ;
		g_free(st_op);
		goto sendback_reply;
	}

	if ((tmpstr = cl_get_string(request, F_STONITHD_NODE_UUID)) != NULL ) {
		st_op->node_uuid = g_strdup(tmpstr);	
	} else {
		stonithd_log(LOG_WARNING, "The stonith requirement message"
			     " contains no target node UUID field.");
	}

	stonithd_log(LOG_DEBUG, "client %s [pid: %d] want a STONITH "
			"operation %s to node %s."
		,	client->name, client->pid
		,	stonith_op_strname[st_op->optype], st_op->node_name);
	
	/* If the node is me, should stonith myself. ;-) No, never come here
	 * from this API while the node name is myself.
	 * So just broadcast the requirement to the whole of cluster to do it.
	 */
	if ( st_op->optype != QUERY && TEST == FALSE &&
	    (srsc = get_local_stonithobj_can_stonith(st_op->node_name, NULL)) 
		!= NULL && 
	    (call_id=initiate_local_stonithop(st_op, srsc, client->ch)) > 0 ) {
			api_reply = ST_APIOK;
	} else { 
		/* including query operation */
		/* call_id < 0 is the correct value when require others to do */
		if ((call_id = initiate_remote_stonithop(st_op, srsc, 
			client->ch)) < 0 ) {
			api_reply = ST_APIOK;
		} else {
			free_stonith_ops_t(st_op);
			api_reply = ST_APIFAIL;
		}
	}	

sendback_reply:
	/* send back the sync result */
	reply = ha_msg_new(3);
	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RSTONITH) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK )
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, call_id))
		!= HA_OK ) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "stonithd_node_fence: cannot add field.");
		return ST_FAIL;
	}
	
	if (msg2ipcchan(reply, ch) != HA_OK) {
		ZAPCHAN(ch); /* ? */
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "stonithd_node_fence: cannot send reply "
				"message to IPC");
		return ST_FAIL;
        }

	if (ch->ops->waitout(ch) == IPC_OK) {
		stonithd_log(LOG_DEBUG, "stonithd_node_fence: end and sent "
			    "back a reply.");
	} else {
		stonithd_log(LOG_DEBUG, "stonithd_node_fence: end and "
			    "failed to sent back a reply.");
		return ST_FAIL;
	}

	stonithd_log2(LOG_DEBUG, "stonithd_node_fence: end");
	return ST_OK;
}

static int
initiate_local_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc, 
			 IPC_Channel * ch)
{
	int call_id;
	
	if (st_op == NULL || srsc == NULL) {
		stonithd_log(LOG_ERR, "initiate_local_stonithop: "
			"st_op == NULL or srsc == NULL.");
		return -1;
	}

	if ((call_id = stonith_operate_locally(st_op, srsc)) <= 0) {
		stonithd_log(LOG_ERR, "stonith_operate_locally failed.");
		return -1;
	} else {
		common_op_t * op;
		int * tmp_callid;

		op = g_new(common_op_t, 1);
		op->scenario = STONITH_INIT;
		op->data = srsc->rsc_id;
		op->result_receiver = ch;
		st_op->call_id = call_id;
		op->op_union.st_op = st_op;
		tmp_callid = g_new(int, 1);
		*tmp_callid = call_id;
		g_hash_table_insert(executing_queue, tmp_callid, op);
		tmp_callid = g_new(int, 1);
		*tmp_callid = call_id;
		Gmain_timeout_add_full(G_PRIORITY_HIGH_IDLE, st_op->timeout,
		   		   stonithop_timeout, tmp_callid, NULL);
		stonithd_log(LOG_DEBUG, "initiate_local_stonithop: inserted "
			    "optype=%d, child_id=%d", st_op->optype, call_id);
		return call_id;
	}
}

static int
continue_local_stonithop(int old_key)
{
	stonith_rsc_t * srsc = NULL;
	char * rsc_id = NULL;
	int * child_pid;
	common_op_t * op = NULL;
	int * orignal_key = NULL;

	stonithd_log2(LOG_DEBUG, "continue_local_stonithop: begin.");
	if (FALSE == g_hash_table_lookup_extended(executing_queue, &old_key, 
			(gpointer *)&orignal_key, (gpointer *)&op)) {
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

	rsc_id = op->data;
	if ( rsc_id == NULL ) {
		stonithd_log(LOG_ERR, "continue_local_stonithop: the old rsc_id"
				" == NULL, not correct!.");
		return ST_FAIL;
	}

	child_pid = g_new(int, 1);
	while ((srsc = get_local_stonithobj_can_stonith(op->op_union.st_op->node_name,
		rsc_id)) != NULL ) { 
		if ((*child_pid=stonith_operate_locally(op->op_union.st_op, srsc)) > 0) {
			g_hash_table_steal(executing_queue, orignal_key);
			stonithd_log(LOG_DEBUG, "continue_local_stonithop: "
				     "removed optype=%d, key_id=%d", 
				     op->op_union.st_op->optype, *orignal_key);
			g_free(orignal_key);
			orignal_key = NULL;
			/* donnot need to free the old one */
			op->data = srsc->rsc_id;
			g_hash_table_insert(executing_queue, child_pid, op);
			stonithd_log(LOG_DEBUG, "continue_local_stonithop: "
				     "inserted optype=%d, child_id=%d", 
				     op->op_union.st_op->optype, *child_pid);
			return ST_OK;
		} else {
			rsc_id = srsc->rsc_id;
		}
	}
	g_free(child_pid);

	return ST_FAIL;
}

static int
initiate_remote_stonithop(stonith_ops_t * st_op, stonith_rsc_t * srsc,
                          IPC_Channel * ch)
{
	if (st_op->optype == QUERY) {
		if ( NULL != get_local_stonithobj_can_stonith(
				st_op->node_name, NULL)) {
			st_op->node_list = g_string_append(
				st_op->node_list, local_nodename);
		}
	}

	st_op->call_id = negative_callid_counter;
	if (ST_OK!=require_others_to_stonith(st_op) && st_op->optype!=QUERY) {
		stonithd_log(LOG_ERR, "require_others_to_stonith failed.");
		st_op->call_id = 0;
		return 1;
	} else {
		common_op_t * op;
		int * tmp_callid;

		op = g_new(common_op_t, 1);
		op->scenario = STONITH_INIT;
		op->result_receiver = ch;
		op->op_union.st_op = st_op;
		tmp_callid = g_new(int, 1);
		*tmp_callid = st_op->call_id;
		g_hash_table_insert(executing_queue, tmp_callid, op);
		tmp_callid = g_new(int, 1);
		*tmp_callid = st_op->call_id;
		Gmain_timeout_add_full(G_PRIORITY_HIGH_IDLE, st_op->timeout,
				   stonithop_timeout, tmp_callid, NULL);

		stonithd_log(LOG_DEBUG, "initiate_remote_stonithop: inserted "
			  "optype=%d, key=%d", op->op_union.st_op->optype, *tmp_callid);
		stonithd_log(LOG_DEBUG, "initiate_remote_stonithop: "
			     "require_others_to_stonith succeed.");
		return negative_callid_counter--;
	}
}

static int
changeto_remote_stonithop(int old_key)
{
	common_op_t * op = NULL;
	int * orignal_key = NULL;

	stonithd_log2(LOG_DEBUG, "changeto_remote_stonithop: begin.");
	if (FALSE == g_hash_table_lookup_extended(executing_queue, &old_key, 
			(gpointer *)&orignal_key, (gpointer *)&op)) {
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
	} else {
		/* donnt need to free op->data */
		op->data = NULL;
		g_hash_table_steal(executing_queue, orignal_key);
		stonithd_log(LOG_DEBUG, "changeto_remote_stonithop: removed "
			  "optype=%d, key=%d", op->op_union.st_op->optype, *orignal_key);
		*orignal_key = op->op_union.st_op->call_id;
		g_hash_table_insert(executing_queue, orignal_key, op);
		stonithd_log(LOG_DEBUG, "changeto_remote_stonithop: inserted "
			  "optype=%d, key=%d", op->op_union.st_op->optype, *orignal_key);
	}

	return ST_OK;
}

static int
send_stonithop_final_result( common_op_t * op)
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
stonithop_result_to_local_client( stonith_ops_t * st_op, gpointer data)
{
	struct ha_msg * reply = NULL;
	IPC_Channel * ch = (IPC_Channel *)data;

	stonithd_log2(LOG_DEBUG, "stonithop_result_to_local_client: begin.");
	if ( st_op == NULL || data == NULL ) {
		stonithd_log(LOG_ERR, "stonithop_result_to_local_client: "
				      "parameter error.");
		return ST_FAIL;
	}

	if (st_op->op_result == STONITH_SUCCEEDED ) {
		stonithd_log(LOG_INFO, "%s %s: optype=%d. whodoit: %s"
			,	M_STONITH_SUCCEED
			,	st_op->node_name, st_op->optype
			,	((GString *)(st_op->node_list))->str);
	} else {
		stonithd_log(LOG_INFO
			,	"%s %s: optype=%d, op_result=%d" 
			,	M_STONITH_FAIL, st_op->node_name
			,	st_op->optype, st_op->op_result);
	}

	stonithd_log(LOG_DEBUG, "stonith finished: optype=%d, node_name=%s", 
		     st_op->optype, st_op->node_name);

	reply = ha_msg_new(0);

	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
  	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_STRET) != HA_OK ) 
	    ||(ha_msg_add_int(reply, F_STONITHD_OPTYPE, st_op->optype) != HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_NODE, st_op->node_name) != HA_OK)
	    ||(st_op->node_uuid == NULL
		|| ha_msg_add(reply, F_STONITHD_NODE_UUID, st_op->node_uuid) != HA_OK)
	    ||(st_op->node_list == NULL
		|| ha_msg_add(reply, F_STONITHD_APPEND,
			((GString *)(st_op->node_list))->str) != HA_OK )
	    ||(ha_msg_add_int(reply, F_STONITHD_TIMEOUT, st_op->timeout)!=HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, st_op->call_id) !=HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_FRC, st_op->op_result) 
		!= HA_OK )) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "stonithop_result_to_local_client: "
			     "cannot add fields.");
		return ST_FAIL;
	}

	if ( msg2ipcchan(reply, ch) != HA_OK) {
		ZAPCHAN(ch); /* ? */
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "stonithop_result_to_local_client: cannot"
			     " send final result message via IPC");
		return ST_FAIL;
	} else {
		stonithd_log(LOG_DEBUG, "stonithop_result_to_local_client: "
			     "succeed in sending back final result message.");
	}

	ZAPMSG(reply);
	stonithd_log2(LOG_DEBUG, "stonithop_result_to_local_client: end.");
	return ST_OK;
}

static int
stonithop_result_to_other_node( stonith_ops_t * st_op, gpointer data)
{
	char * node_name = (char *)data;
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

	if ((reply = ha_msg_new(3)) == NULL) {
		stonithd_log(LOG_ERR, "stonithop_result_to_other_node: "
			     "ha_msg_new: out of memory");
		return ST_FAIL;
	}

	if ( (ha_msg_add(reply, F_TYPE, T_RSTIT) != HA_OK)
    	    ||(ha_msg_add(reply, F_ORIG, local_nodename) != HA_OK)
    	    ||(ha_msg_add_int(reply, F_STONITHD_FRC, st_op->op_result) != HA_OK)
    	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, st_op->call_id) 
		!= HA_OK)) {
		stonithd_log(LOG_ERR, "stonithop_result_to_other_node: "
			     "ha_msg_add: cannot add field.");
		ZAPMSG(reply);
		return ST_FAIL;
	}

	if (hb == NULL) {
		stonithd_log(LOG_ERR, "stonithop_result_to_other_node: "
			     "hb == NULL");
		ZAPMSG(reply);
		return ST_FAIL;
	}

	if (HA_OK!=hb->llc_ops->sendnodemsg(hb, reply, node_name)) {
		stonithd_log(LOG_ERR, "stonithop_result_to_other_node: "
			     "sendnodermsg failed.");
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

	st_obj = srsc->stonith_obj;
	if (st_obj == NULL ) {
		stonithd_log(LOG_ERR, "stonith_operate: "
			"srsc->stonith_obj == NULL.");
		return -1;
	}

	/* stonith it by myself in child */
	return_to_orig_privs();
	if ((pid = fork()) < 0) {
		stonithd_log(LOG_ERR, "stonith_operate_locally: fork failed.");
		return_to_dropped_privs();
		return -1;
	} else if (pid > 0) { /* in the parent process */
		snprintf(buf_tmp, 39, "%s_%s_%d", st_obj->stype, srsc->rsc_id
			, (int)st_op->optype); 
		NewTrackedProc( pid, 1
				, (debug_level>1)? PT_LOGVERBOSE : PT_LOGNORMAL
				, g_strdup(buf_tmp), &StonithdProcessTrackOps);
		return_to_dropped_privs();
		return pid;
	}

	/* now in child process */
	/* this operation may be on block status */
	exit(stonith_req_reset(st_obj, st_op->optype, 
				      g_strdup(st_op->node_name)));
}

static int
require_others_to_stonith(stonith_ops_t * st_op)
{
	struct ha_msg * msg;
	if ((msg = ha_msg_new(1)) == NULL) {
		stonithd_log(LOG_ERR, "require_others_to_stonith: "
					"out of memory");
		return ST_FAIL;
	}

	stonithd_log2(LOG_DEBUG, "require_others_to_stonith: begin.");
	if ( (ha_msg_add(msg, F_TYPE, (st_op->optype == QUERY) ? 
					T_WHOCANST : T_STIT) != HA_OK)
	    ||(ha_msg_add(msg, F_ORIG, local_nodename) != HA_OK)
	    ||(ha_msg_add(msg, F_STONITHD_NODE, st_op->node_name) != HA_OK)
	    ||(st_op->node_uuid == NULL
	       || ha_msg_add(msg, F_STONITHD_NODE_UUID, st_op->node_uuid) != HA_OK)
	    ||(ha_msg_add_int(msg, F_STONITHD_OPTYPE, st_op->optype) != HA_OK)
	    ||(ha_msg_add_int(msg, F_STONITHD_TIMEOUT, st_op->timeout) != HA_OK)
	    ||(ha_msg_add_int(msg, F_STONITHD_CALLID, st_op->call_id) 
		!= HA_OK) ) {
		stonithd_log(LOG_ERR, "require_others_to_stonith: "
					"cannot add field.");
		ZAPMSG(msg);
		return ST_FAIL;
	}

	if (hb == NULL) {
		stonithd_log(LOG_ERR, "require_others_to_stonith: hb==NULL");
		ZAPMSG(msg);
		return ST_FAIL;
	}

	if (HA_OK != hb->llc_ops->sendclustermsg(hb, msg) ) {
		stonithd_log(LOG_ERR,"require_others_to_stonith: "
			     "sendclustermsg failed.");
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
		if (begin_search_list == NULL) {
			stonithd_log(LOG_DEBUG, "get_local_stonithobj_can_"
				"stonith: local_started_stonith_rsc is empty.");
			return NULL;
		}
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
				stonithd_log(LOG_DEBUG, "get_local_stonithobj_"
					"can_stonith: host=%s.", *this);
				if ( strncmp(node_name, *this, MAXCMP) == 0 ) {
					return tmp_srsc;
				}
			}
		} else {
			if (tmp_srsc == NULL) {
				stonithd_log(LOG_DEBUG, "get_local_stonithobj_"
					"can_stonith: tmp_srsc=NULL.");
				
			} else {
				stonithd_log(LOG_DEBUG, "get_local_stonithobj_"
				    "can_stonith: tmp_srsc->node_list = NULL.");
			}
		}
	}

	return NULL;
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
	/* since g_hash_table_find donnot exist in early version of glib-2.0 */
	my_hash_table_find(executing_queue, (gpointer *)&orig_key, 
			   (gpointer *)&op, call_id);
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
		stonithd_log(LOG_DEBUG, "stonithop_timeout: the stonith operation"
			     "(call_id==%d) has finished.", *call_id);
	}

	/* Kill the possible child process forked for this operation */
	if (orig_key!=NULL && *orig_key > 0 && CL_PID_EXISTS(*orig_key)) {
		return_to_orig_privs();
		CL_KILL(*orig_key, 9);	
		return_to_dropped_privs();
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
on_stonithd_virtual_stonithRA_ops(const struct ha_msg * request, gpointer data)
{
	const char * api_reply = ST_APIOK;
	IPC_Channel * ch = (IPC_Channel *) data;
	struct ha_msg * reply;
	const char * tmpstr = NULL;
	stonithRA_ops_t * ra_op;
	int * child_pid = NULL;
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
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: "
			     "not signoned yet.");
		return ST_FAIL;
	}

	/* handle the RA operations such as 'start' 'stop', 'monitor' and etc */
	ra_op = g_new(stonithRA_ops_t, 1);
	ra_op->private_data = NULL;

	if ((tmpstr = cl_get_string(request, F_STONITHD_RSCID)) != NULL) {
		ra_op->rsc_id = g_strdup(tmpstr);
		stonithd_log(LOG_DEBUG, "ra_op->rsc_id=%s.", ra_op->rsc_id);
	} else {
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: "
			     "the request message contains no rsc_id field.");
		api_reply = ST_BADREQ;
		goto send_back_reply;
	}

	if ((tmpstr = cl_get_string(request, F_STONITHD_RAOPTYPE)) != NULL) {
		ra_op->op_type = g_strdup(tmpstr);
		stonithd_log(LOG_DEBUG, "ra_op->op_type=%s.", ra_op->op_type);
	} else {
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: the "
			     "request message contains no op_type field.");
		api_reply = ST_BADREQ;
		goto send_back_reply;
	}

	if ((tmpstr = cl_get_string(request, F_STONITHD_RANAME)) != NULL) {
		ra_op->ra_name = g_strdup(tmpstr);
		stonithd_log(LOG_DEBUG, "ra_op->ra_name=%s.", ra_op->ra_name);
	} else {
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: the "
			     "request message contains no ra_name field.");
		api_reply = ST_BADREQ;
		goto send_back_reply;
	}

	ra_op->params = NULL;
	if ((ra_op->params = cl_get_hashtable(request, F_STONITHD_PARAMS))
	    != NULL) {
		stonithd_log2(LOG_DEBUG, "on_stonithd_virtual_stonithRA_ops:"
			     "ra_op->params address:=%p.", ra_op->params);
	} else {
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: the "
			     "request msg contains no parameter field.");
		api_reply = ST_BADREQ;
		goto send_back_reply;
	}

	stonithd_log(LOG_DEBUG, "client %s [pid: %d] want a resource "
			"operation %s on stonith RA %s [resource id: %s]"
		,	client->name, client->pid
		,	ra_op->op_type, ra_op->ra_name, ra_op->rsc_id);

	/* execute stonith plugin : begin */
	/* When in parent process then be back here */
	child_pid = g_new(int,1);
	if ((*child_pid = stonithRA_operate(ra_op, NULL)) <= 0) {
		api_reply = ST_APIFAIL;
	} else {
		common_op_t * op;
		int * key_tmp;

		op = g_new(common_op_t, 1);
		op->scenario = STONITH_RA_OP;
		op->result_receiver = client->ch;
		op->op_union.ra_op = ra_op;
		key_tmp = g_new(int, 1);
		*key_tmp = *child_pid;
		g_hash_table_insert(executing_queue, key_tmp, op);
		stonithd_log(LOG_DEBUG, "on_stonithd_virtual_stonithRA_ops: "
			     "insert child_pid=%d to table", *key_tmp);
	}	
	/* execute stonith plugin : end */

	/* send back the sync result at once */
send_back_reply:
	reply = ha_msg_new(3);
	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RRAOP) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_APIRET, api_reply) != HA_OK ) 
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, *child_pid)
		!= HA_OK ) ) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: "
			     "cannot add message field.");
		g_free(child_pid);
		return ST_FAIL;
	}
	g_free(child_pid);

	if (msg2ipcchan(reply, ch) != HA_OK) { /* How to deal the error*/
		ZAPCHAN(ch); /* ? */
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_virtual_stonithRA_ops: "
			     "cannot send reply message to IPC");
		return ST_FAIL;
        }

	if (ch->ops->waitout(ch) == IPC_OK) {
		stonithd_log(LOG_DEBUG, "on_stonithd_virtual_stonithRA_ops: "
			     "sent back a synchonrous result.");
		return ST_OK;
	} else {
		stonithd_log(LOG_ERR, "on_stonithd_virtaul_stonithRA_ops: "
			     "failed to send back a synchonrous result.");
		return ST_FAIL;
	}

	stonithd_log2(LOG_DEBUG, "on_stonithd_virtaul_stonithRA_ops: end");
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

	stonithd_log(LOG_DEBUG, "RA %s's op %s finished.", 
		     ra_op->ra_name, ra_op->op_type);
	reply = ha_msg_new(0);

	stonithd_log(LOG_DEBUG, "ra_op->op_type=%s, ra_op->ra_name=%s",
		     ra_op->op_type, ra_op->ra_name); /* can be deleted*/
	if ( (ha_msg_add(reply, F_STONITHD_TYPE, ST_APIRPL) != HA_OK ) 
  	    ||(ha_msg_add(reply, F_STONITHD_APIRPL, ST_RAOPRET) != HA_OK ) 
	    ||(ha_msg_add(reply, F_STONITHD_RAOPTYPE, ra_op->op_type) != HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_RANAME, ra_op->ra_name) != HA_OK)
	    ||(ha_msg_add(reply, F_STONITHD_RSCID, ra_op->rsc_id) != HA_OK)
	    ||(ha_msg_addhash(reply, F_STONITHD_PARAMS, ra_op->params) != HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_CALLID, ra_op->call_id)!= HA_OK)
	    ||(ha_msg_add_int(reply, F_STONITHD_FRC, ra_op->op_result)
		!= HA_OK )) {
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "send_stonithRAop_final_result: cannot "
			     "add message fields.");
		return ST_FAIL;
	}

	if ( msg2ipcchan(reply, ch) != HA_OK) {
		ZAPCHAN(ch); /* ? */
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "send_stonithRAop_final_result: cannot "
			     "send final result message via IPC");
		return ST_FAIL;
	} else {
		stonithd_log(LOG_DEBUG, "send_stonithRAop_final_result: "
			     "succeed in sending back final result message.");
	}

	ZAPMSG(reply);
	return ST_OK;
}

static int
post_handle_raop(stonithRA_ops_t * ra_op)
{
	int i;

	for (i = 0; i < DIMOF(raop_handler); i++) {
		if ( strncmp(ra_op->op_type, raop_handler[i].op_type, MAXCMP)
			== 0 ) {
			/* call the handler of the operation */
			if (raop_handler[i].post_handler != NULL) {
				return raop_handler[i].post_handler(ra_op, NULL);
			} else 
				break;
		}
	}

        if (i == DIMOF(raop_handler)) {
                stonithd_log(LOG_NOTICE, "received an unknown RA op,"
					"and now just ignore it.");
		return ST_FAIL;
        }

	return ST_OK;
}

static int
on_stonithd_list_stonith_types(const struct ha_msg * request, gpointer data)
{
	const char * api_reply = ST_APIOK;
	IPC_Channel * ch = (IPC_Channel *) data;
	struct ha_msg * reply;
	char **	typelist;

	/* Need to check whether signon? */
	reply = ha_msg_new(0);

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
		ZAPCHAN(ch); /* ? */
		ZAPMSG(reply);
		stonithd_log(LOG_ERR, "on_stonithd_list_stonith_types: cannot "
			     " send reply message to IPC");
		return ST_FAIL;
        }

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
			     "RA op, and now just ignore it.");
        }

	return(-1);	
}

static int
stonithRA_start( stonithRA_ops_t * op, gpointer data)
{
	int		rc; /* To return compatible return code */
	stonith_rsc_t * srsc;
	Stonith *	stonith_obj = NULL;
	pid_t 		pid;
	StonithNVpair*	snv;
	char 		buf_tmp[40];

	/* Check the parameter */
	if ( op == NULL || op->rsc_id <= 0 || op->op_type == NULL
	    || op->ra_name == NULL || op->params == NULL ) {
		stonithd_log(LOG_ERR, "stonithRA_start: parameter error.");
		return ST_FAIL;
	}

	srsc = get_started_stonith_resource(op->rsc_id);
	if (srsc != NULL) {
		/* seems started, just to confirm it */
		stonith_obj = srsc->stonith_obj;
		goto probe_status;
	}

	/* Don't find in local_started_stonith_rsc, not on start status */
	stonithd_log(LOG_DEBUG, "stonithRA_start: op->params' address=%p"
		     , op->params);
	if (debug_level > 0) {
		print_str_hashtable(op->params);
	}

	stonith_obj = stonith_new(op->ra_name);
	if (stonith_obj == NULL) {
		stonithd_log(LOG_ERR, "Invalid RA/device type: '%s'", 
		             op->ra_name);
		return ST_FAIL;
	}

	/* Set the stonith plugin's debug level */
	stonith_set_debug(stonith_obj, debug_level);
	snv = stonith_ghash_to_NVpair(op->params);
	if ( snv == NULL
	||	stonith_set_config(stonith_obj, snv) != S_OK ) {
		stonithd_log(LOG_ERR,
			"Invalid config info for %s device.", op->ra_name);

		stonith_delete(stonith_obj); 
		stonith_obj = NULL;
		if (snv != NULL) {
			free_NVpair(snv);
			snv = NULL;
		}
		return ST_FAIL; /*exit(rc);*/
	}
	if (snv != NULL) {
		free_NVpair(snv);
		snv = NULL;
	}

	op->private_data = stonith_obj;

probe_status:
	return_to_orig_privs();
        if ((pid = fork()) < 0) {
                stonithd_log(LOG_ERR, "stonithRA_start: fork failed.");
		return_to_dropped_privs();
                return -1;
        } else if (pid > 0) { /* in the parent process */
		snprintf(buf_tmp, 39, "%s_%s_%s", stonith_obj->stype
			, op->rsc_id , "start"); 
		NewTrackedProc( pid, 1
				, (debug_level>1)? PT_LOGVERBOSE : PT_LOGNORMAL
				, g_strdup(buf_tmp), &StonithdProcessTrackOps);
		return_to_dropped_privs();
                return pid;
        }

	/* Now in the child process */
	/* Need to distiguish the exit code more carefully */
	if ( (rc = stonith_get_status(stonith_obj)) == S_OK ) {
		exit(EXECRA_OK);
	} else {
		exit(EXECRA_UNKNOWN_ERROR);
	}
}

static int
stonithRA_start_post( stonithRA_ops_t * ra_op, gpointer data )
{
	stonith_rsc_t * srsc;

	if (ra_op->op_result != EXECRA_OK ) {
		return ST_FAIL;	
	}

	srsc = get_started_stonith_resource(ra_op->rsc_id);
	if (srsc != NULL) { /* Already started before this operation */
		return ST_OK;
	}

	srsc = g_new(stonith_rsc_t, 1);
	srsc->rsc_id = ra_op->rsc_id;
	ra_op->rsc_id = NULL;
	srsc->ra_name = ra_op->ra_name;
	ra_op->ra_name = NULL;
	srsc->params = ra_op->params;
	ra_op->params = NULL;
	srsc->stonith_obj = ra_op->private_data;
	ra_op->private_data = NULL;
	srsc->node_list = NULL;
	srsc->node_list = stonith_get_hostlist(srsc->stonith_obj);
	if ( debug_level >= 2 ) {
		char **	this;
		stonithd_log2(LOG_DEBUG, "Got HOSTLIST");
		for(this=srsc->node_list; *this; ++this) {
			stonithd_log2(LOG_DEBUG, "%s", *this);
		}
	}

	if (srsc->node_list == NULL) {
		stonithd_log(LOG_ERR, "Could not list nodes for stonith RA %s."
		,	srsc->ra_name);
		/* Need more error handlings? */
	}

	local_started_stonith_rsc = 
			g_list_append(local_started_stonith_rsc, srsc);

	return ST_OK;
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

	srsc = get_started_stonith_resource(ra_op->rsc_id);
	if (srsc != NULL) {
		stonithd_log2(LOG_DEBUG, "got the active stonith_rsc: " 
				"RA name = %s, rsc_id = %s."
				, srsc->ra_name, srsc->rsc_id);
		snprintf(buf_tmp, 39, "%s_%s_%s", srsc->stonith_obj->stype
			 , ra_op->rsc_id, "stop");
		stonith_delete(srsc->stonith_obj);
		srsc->stonith_obj = NULL;
		local_started_stonith_rsc = 
			g_list_remove(local_started_stonith_rsc, srsc);
		free_stonith_rsc(srsc);	
	} else {
		stonithd_log(LOG_NOTICE, "try to stop a resource %s who is not "
			     "in started resource queue.", ra_op->rsc_id); 
		snprintf(buf_tmp, 39, "%s_%s_%s", "unknown"
			 , ra_op->rsc_id, "stop");
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
		stonithd_log(LOG_DEBUG, "stonithRA_monitor: the resource is not "
				"started.");
		/* This resource is not started */
		child_exitcode = EXECRA_NOT_RUNNING;
	}

	return_to_orig_privs();
        if ((pid = fork()) < 0) {
                stonithd_log(LOG_ERR, "stonithRA_monitor: fork failed.");
		return_to_dropped_privs();
                return -1;
        } else if (pid > 0) { /* in the parent process */
		snprintf(buf_tmp, 39, "%s_%s_%s", (srsc != NULL) ? 
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
		stonithd_log2(LOG_DEBUG, "stonithRA_monitor: child exit, "
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

static void 
free_client(stonithd_client_t * client)
{
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
	
	client->ch = NULL;
	/* don not need to destroy it! */
	/* ZAPCHAN(client->ch); */
	g_free(client);
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
	
	stonithd_log2(LOG_DEBUG, "free_stonithRA_ops_t: ra_op->private_data.=%p"
		     , (Stonith *)(ra_op->private_data));
	if (ra_op->private_data != NULL ) {
		stonith_delete((Stonith *)(ra_op->private_data));
	}
	/* Has used g_hash_table_new_full to create params */
	g_hash_table_destroy(ra_op->params);
	ZAPGDOBJ(ra_op);
}

static void
free_stonith_ops_t(stonith_ops_t * st_op)
{
	if (st_op == NULL) {
		stonithd_log(LOG_DEBUG, "free_stonith_ops_t: st_op==NULL");
		return;
	}

	stonithd_log2(LOG_DEBUG, "free_stonith_ops_t: begin.");
	ZAPGDOBJ(st_op->node_name);
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
	g_hash_table_destroy(srsc->params);
	srsc->params = NULL;

	if (srsc->stonith_obj != NULL ) {
		stonithd_log2(LOG_DEBUG, "free_stonith_rsc: destroy stonith_obj.");
		stonith_delete(srsc->stonith_obj);
		srsc->stonith_obj = NULL;
	}

	stonith_free_hostlist(srsc->node_list);
	srsc->node_list = NULL;

	stonithd_log2(LOG_DEBUG, "free_stonith_rsc: finished.");
}

static stonithd_client_t *
get_exist_client_by_chan(GList * client_list, IPC_Channel * ch)
{
	stonithd_client_t * client;
	GList * tmplist = NULL;
	
	stonithd_log2(LOG_DEBUG, "get_exist_client_by_chan: begin.");
	if (client_list == NULL) {
		stonithd_log(LOG_DEBUG, "get_exist_client_by_chan: "
			     "client_list == NULL");
		return NULL;
	} 

	if (ch == NULL) {
		stonithd_log(LOG_DEBUG, "get_exist_client_by_chan:ch=NULL");
		return NULL;
	} 

	tmplist = g_list_first(client_list);
	for (tmplist = g_list_first(client_list); tmplist != NULL; 
	     tmplist = g_list_next(tmplist)) {
		stonithd_log2(LOG_DEBUG, "tmplist=%p", tmplist);
		client = (stonithd_client_t *)tmplist->data;
		if (client != NULL && client->ch == ch) {
			stonithd_log(LOG_DEBUG, "get_exist_client_by_chan: "
					"client %s.", client->name);
			return client;
		}
	}

	stonithd_log2(LOG_DEBUG, "get_exist_client_by_chan: end.");
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

static int
delete_client_by_chan(GList ** client_list, IPC_Channel * ch)
{
	stonithd_client_t * client;

	if ( (client = get_exist_client_by_chan(*client_list, ch)) != NULL ) {
		stonithd_log(LOG_DEBUG, "delete_client_by_chan: delete client "
			"%s (pid=%d)", client->name, client->pid);
		*client_list = g_list_remove(*client_list, client);
		free_client(client);
		client = NULL;
		stonithd_log2(LOG_DEBUG, "delete_client_by_chan: return OK.");
		stonithd_log(LOG_DEBUG, "delete_client_by_chan: new "
			     "client_list = %p", *client_list);
		return ST_OK;
	} else {
		stonithd_log(LOG_DEBUG, "delete_client_by_chan: no client "
			"using this channel, so no client deleted.");
		return ST_FAIL;
	}
}

static void
inherit_config_from_environment(void)
{
	char * inherit_env = NULL;

	/* Donnot need to free the return pointer from getenv */
	inherit_env = getenv(HADEBUGVAL);
	if (inherit_env != NULL && atoi(inherit_env) != 0 ) {
		debug_level = atoi(inherit_env);
		inherit_env = NULL;
	}

	inherit_env = getenv(LOGFENV);
	if (inherit_env != NULL) {
		cl_log_set_logfile(inherit_env);
		inherit_env = NULL;
	}

	inherit_env = getenv(DEBUGFENV);
	if (inherit_env != NULL) {
		cl_log_set_debugfile(inherit_env);
		inherit_env = NULL;
	}

	inherit_env = getenv(LOGFACILITY);
	if (inherit_env != NULL) {
		int facility = -1;
		facility = cl_syslogfac_str2int(inherit_env);
		if ( facility != -1 ) {
			cl_log_set_facility(facility);
		}
		inherit_env = NULL;
	}
}

/* make the apphb_interval, apphb_warntime adjustable important */
static int
init_using_apphb(void)
{
	char stonithd_instance[40];

	sprintf(stonithd_instance, "%s_%ldd", stonithd_name, (long)getpid());
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
destory_key_of_op_htable(gpointer data)
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

	/* Donnot need to free 'data' field */
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

/* 
 * $Log: stonithd.c,v $
 * Revision 1.56  2005/06/20 16:20:33  sunjd
 * fix the NULL pointer issues in BSC
 *
 * Revision 1.55  2005/06/20 05:52:53  sunjd
 * always return the STONITHer in node_list field
 *
 * Revision 1.54  2005/06/20 03:04:58  sunjd
 * Bug 644, including set debug level for stonith plugin;
 * add and adjust logs;
 * format tweak,
 *
 * Revision 1.53  2005/06/15 16:13:41  davidlee
 * common code for syslog facility name/value conversion
 *
 * Revision 1.52  2005/06/06 09:20:35  sunjd
 * minor log tweak
 *
 * Revision 1.51  2005/06/03 08:37:43  sunjd
 * add logs
 *
 * Revision 1.50  2005/06/03 08:05:00  sunjd
 * should not be an error log
 *
 * Revision 1.49  2005/06/02 07:51:05  sunjd
 * For the const char array, use STRNCMP_CONST instead
 *
 * Revision 1.48  2005/05/31 06:36:17  sunjd
 * make the apphb interval adjustable
 * simple help & CLI option tweak.
 *
 * Revision 1.47  2005/05/30 08:07:15  sunjd
 * remove the incorrect uses of STRNCMP_CONST.
 *
 * Revision 1.46  2005/05/30 06:14:11  sunjd
 * should be not an error
 *
 * Revision 1.45  2005/05/27 09:16:47  sunjd
 * should report its status be not running when a stonith RA is not started; a few debug messages tweak
 *
 * Revision 1.44  2005/05/27 02:57:11  sunjd
 * fix a silly error :-(; add several log messages
 *
 * Revision 1.43  2005/05/24 06:08:33  sunjd
 * Bug 563: stonithd needs to log exit codes properly; Support loglevel adjustment with SIGUSR1 and SIGUSR2
 *
 * Revision 1.42  2005/05/20 16:15:30  alan
 * Replaced calls to g_main_timeout* functions to ours -- because ours work.
 * Made stonithd lock itself into memory.
 * corrected the opening of /dev/null to allow writing to stdout and stderr
 * Fixed a few spelling errors.
 * Changed it to always enable the ability to take core dumps.
 * Made sure it changed into the core dump directory - and stayed there.
 *
 * Revision 1.41  2005/05/20 15:46:31  alan
 * Made it so SIGCHLD can interrupt stonithd - otherwise hangs can occur.
 *
 * Revision 1.40  2005/05/20 15:42:53  alan
 * Removed code to handle SIGQUIT - that's used for forcing a core dump.
 *
 * Revision 1.39  2005/05/13 23:58:13  gshi
 * use cl_xxx_pidfile functions
 *
 * Revision 1.38  2005/04/19 10:21:22  sunjd
 * changes the restart behaviour of system calls when interrupted by a signal
 *
 * Revision 1.37  2005/04/08 07:32:48  sunjd
 * Replace log function with macro to enhance the running efficiency.
 * Make internal loglevel more granular.
 * Inherit configurations from environment variables set by heartbeat.
 *
 * Revision 1.36  2005/04/07 07:46:18  sunjd
 * use STRLEN_CONST & STRNCMP_CONST instead
 *
 * Revision 1.35  2005/04/06 18:07:52  gshi
 * bug 433
 *
 * provide an option to destroy channel in signoff call
 *
 * Revision 1.34  2005/04/06 03:40:27  sunjd
 * some polish(actually mainly inluded in last checking)
 *
 * Revision 1.33  2005/04/06 03:33:20  sunjd
 * use the new function cl_inherit_use_logd
 *
 * Revision 1.32  2005/03/21 15:00:58  sunjd
 * Change loglevel
 *
 * Revision 1.31  2005/03/21 09:13:42  sunjd
 * Change the return code to reflect the daemon status
 *
 * Revision 1.30  2005/03/21 08:56:39  sunjd
 * Change a log message's loglevel
 *
 * Revision 1.29  2005/03/18 11:00:04  sunjd
 * fix a 'mispell' error ;-)
 *
 * Revision 1.28  2005/03/18 10:38:43  andrew
 * Make sure node_uuid doesnt contain random data
 * I reccomend a common function for packing and unpacking stonith_ops be
 *  created and to have it initialize all the fields to NULL.
 *
 * Revision 1.27  2005/03/16 08:33:57  sunjd
 * Fix a bug which may lead to a infinite loop.
 * Add logs; adjust several logs' loglevel.
 *
 * Revision 1.26  2005/03/15 19:52:24  gshi
 * changed cl_log_send_to_logging_daemon() to cl_log_set_uselogd()
 * added cl_log_get_uselogd()
 *
 * Revision 1.25  2005/03/11 08:46:09  sunjd
 * log message changes
 *
 * Revision 1.24  2005/03/11 06:46:17  sunjd
 * degrade some log messages' loglevel
 *
 * Revision 1.23  2005/03/11 05:46:50  sunjd
 * Degrade some logs' priority
 *
 * Revision 1.22  2005/03/11 03:23:43  sunjd
 * make TEST mode work well; add more logs
 *
 * Revision 1.21  2005/03/08 08:17:16  sunjd
 * add the STONITH result patterns for CTS
 *
 * Revision 1.20  2005/03/08 00:12:04  sunjd
 * Add the support to the 'status' operation
 *
 * Revision 1.19  2005/02/24 06:51:49  sunjd
 * fix a initializing bug; add some logs and comments
 *
 * Revision 1.18  2005/02/22 01:26:58  sunjd
 * add cl_malloc_forced_for_glib;
 * add authority verification to IPC channel
 * downgrade previlege to nobody as possible.
 * other minor polish
 *
 * Revision 1.17  2005/02/17 08:21:42  sunjd
 * fix the log level; correct the word misspelling
 *
 * Revision 1.16  2005/02/16 21:40:06  gshi
 *
 * stonithd should quit when IPC is disconnected instead of when IPC is not disconnected
 *
 * Revision 1.15  2005/02/15 17:40:44  alan
 * Hopefully got rid of an infinite loop in STONITHd.
 *
 * Revision 1.14  2005/02/12 17:13:34  alan
 * Changed a "cannot open pid file" message into a DEBUG message.
 *
 * Revision 1.13  2005/02/07 00:23:01  alan
 * BEAM fix: changed printf to print a non-NULL value ;-)
 *
 * Revision 1.12  2005/02/06 08:54:34  sunjd
 * remove return, or the loop is useless
 *
 * Revision 1.11  2005/02/02 09:42:03  sunjd
 * Take it into account that it may be started up by heartbeat.
 *
 * Revision 1.10  2005/01/03 18:12:10  alan
 * Stonith version 2.
 * Some compatibility with old versions is still missing.
 * Basically, you can't (yet) use files for config information.
 * But, we'll fix that :-).
 * Right now ssh, null and baytech all work.
 *
 * Revision 1.9  2004/12/20 03:26:41  sunjd
 * Patch from Andrew. donnot define a union with no name, that may cause a compiling issue
 *
 * Revision 1.8  2004/12/09 07:29:09  sunjd
 * minor polish
 *
 * Revision 1.7  2004/12/09 06:51:07  sunjd
 * 1) fix a bug related to QUERY operation
 * 2) add more debugging output and remove the redundant code to kill child processes.
 *
 * Revision 1.6  2004/12/08 09:31:44  sunjd
 * add code to output more debugging information
 *
 * Revision 1.5  2004/12/08 03:24:16  sunjd
 * 1) Use the new ha_msg_add_in/ha_msg_value_int to replace ha_msg_addbin/cl_get_binary, so to
 * avoid the possible endian issue.
 * 2) Make it more strict to kill the possible children.
 *
 * Revision 1.4  2004/12/06 21:24:07  msoffen
 * Fixed C++ style comments
 *
 */
