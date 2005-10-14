
/*
 * Linux HA Management Daemon
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <security/pam_appl.h>
#include <glib.h>

#include <heartbeat.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_syslog.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/cl_pidfile.h>

#include <mgmt/tls.h>
#include <mgmt/mgmt.h>
#include "mgmtd.h"


/* common daemon and debug functions */
static gboolean debug_level_adjust(int nsig, gpointer user_data);
static gboolean sigterm_action(int nsig, gpointer unused);
static void usage(const char* cmd, int exit_status);
static int init_start(void);
static int init_stop(const char *pid_file);
static int init_status(const char *pid_file, const char *client_name);

/* the initial func for modules */
extern int init_general(void);
extern void final_general(void);
extern int init_crm(void);
extern void final_crm(void);
extern int init_heartbeat(void);
extern void final_heartbeat(void);
extern int init_lrm(void);
extern void final_lrm(void);

/* management daemon internal data structure */
typedef struct
{
	int		id;
	GIOChannel*	ch;
	void*		session;
}client_t;

/* management daemon internal functions */
static gboolean on_listen(GIOChannel *source
, 			  GIOCondition condition
,			  gpointer data);

static gboolean on_msg_arrived(GIOChannel *source
,			       GIOCondition condition
,			       gpointer data);


static int new_client(int sock, void* session);
static client_t* lookup_client(int id);
static int del_client(int id);

static int pam_auth(const char* user, const char* passwd);
static int pam_conv(int n, const struct pam_message **msg,
		    struct pam_response **resp, void *data);

static char* dispatch_msg(const char* msg, int client_id);

const char* mgmtd_name 	= "mgmtd";

static GMainLoop* mainloop 	= NULL;
extern int debug_level;
static GHashTable* clients	= NULL;
static GHashTable* msg_map	= NULL;		
static GHashTable* evt_map	= NULL;		

int
main(int argc, char ** argv)
{
	int req_restart = FALSE;
	int req_status  = FALSE;
	int req_stop    = FALSE;
	
	int argerr = 0;
	int flag;
	char * inherit_debuglevel;

	cl_malloc_forced_for_glib();
	while ((flag = getopt(argc, argv, OPTARGS)) != EOF) {
		switch(flag) {
			case 'h':		/* Help message */
				usage(mgmtd_name, LSB_EXIT_OK);
				break;
			case 'v':		/* Debug mode, more logs*/
				++debug_level;
				break;
			case 's':		/* Status */
				req_status = TRUE;
				break;
			case 'k':		/* Stop (kill) */
				req_stop = TRUE;
				break;
			case 'r':		/* Restart */
				req_restart = TRUE;
				break;
			default:
				++argerr;
				break;
		}
	}

	if (optind > argc) {
		mgmtd_log(LOG_ERR,"WHY WE ARE HERE?");
		++argerr;
	}

	if (argerr) {
		usage(mgmtd_name, LSB_EXIT_GENERIC);
	}

	inherit_debuglevel = getenv(HADEBUGVAL);
	if (inherit_debuglevel != NULL) {
		debug_level = atoi(inherit_debuglevel);
		if (debug_level > 2) {
			debug_level = 2;
		}
	}

	cl_log_set_entity(mgmtd_name);
	cl_log_enable_stderr(FALSE);
	cl_log_set_facility(LOG_DAEMON);

	/* Use logd if it's enabled by heartbeat */
	cl_inherit_use_logd(ENV_PREFIX""KEY_LOGDAEMON, 0);

	inherit_logconfig_from_environment();

	if (req_status){
		return init_status(PID_FILE, mgmtd_name);
	}

	if (req_stop){
		return init_stop(PID_FILE);
	}

	if (req_restart) {
		init_stop(PID_FILE);
	}

	return init_start();
}

int
init_status(const char *pid_file, const char *client_name)
{
	long	pid =	cl_read_pidfile(pid_file);
	
	if (pid > 0) {
		fprintf(stderr, "%s is running [pid: %ld]\n"
			,	client_name, pid);
		return LSB_STATUS_OK;
	}
	fprintf(stderr, "%s is stopped.\n", client_name);
	return LSB_STATUS_STOPPED;
}

int
init_stop(const char *pid_file)
{
	long	pid;
	int	rc = LSB_EXIT_OK;



	if (pid_file == NULL) {
		mgmtd_log(LOG_ERR, "No pid file specified to kill process");
		return LSB_EXIT_GENERIC;
	}
	pid =	cl_read_pidfile(pid_file);

	if (pid > 0) {
		if (CL_KILL((pid_t)pid, SIGTERM) < 0) {
			rc = (errno == EPERM
			      ?	LSB_EXIT_EPERM : LSB_EXIT_GENERIC);
			fprintf(stderr, "Cannot kill pid %ld\n", pid);
		}else{
			mgmtd_log(LOG_INFO,
			       "Signal sent to pid=%ld,"
			       " waiting for process to exit",
			       pid);

			while (CL_PID_EXISTS(pid)) {
				sleep(1);
			}
		}
	}
	return rc;
}

static const char usagemsg[] = "[-srkhv]\n\ts: status\n\tr: restart"
	"\n\tk: kill\n\t"
	"h: help\n\tv: debug\n";

void
usage(const char* cmd, int exit_status)
{
	FILE* stream;

	stream = exit_status ? stderr : stdout;

	fprintf(stream, "usage: %s %s", cmd, usagemsg);
	fflush(stream);

	exit(exit_status);
}

gboolean
sigterm_action(int nsig, gpointer user_data)
{
	shutdown_mgmtd();	
	return TRUE;
}

static void
register_pid(gboolean do_fork,
	     gboolean (*shutdown)(int nsig, gpointer userdata))
{
	int	j;

	umask(022);

	for (j = 0; j < 3; ++j) {
		close(j);
		(void)open("/dev/null", j == 0 ? O_RDONLY : O_RDONLY);
	}
	CL_IGNORE_SIG(SIGINT);
	CL_IGNORE_SIG(SIGHUP);
	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGTERM
	,	 	shutdown, NULL, NULL);
	cl_signal_set_interrupt(SIGTERM, 1);
	cl_signal_set_interrupt(SIGCHLD, 1);
	/* At least they are harmless, I think. ;-) */
	cl_signal_set_interrupt(SIGINT, 0);
	cl_signal_set_interrupt(SIGHUP, 0);
}

static gboolean 
debug_level_adjust(int nsig, gpointer user_data)
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
			mgmtd_log(LOG_WARNING, "debug_level_adjust: Received an "
				"unexpected signal(%d). Something wrong?.",nsig);
	}

	return TRUE;
}


/* main loop of the daemon*/
int
init_start ()
{
	long 			pid;
	int 			ssock;
	struct sockaddr_in 	saddr;
	GIOChannel* 		sch;
	/* register pid */
	if ((pid = cl_lock_pidfile(PID_FILE)) < 0) {
		mgmtd_log(LOG_ERR, "already running: [pid %d]."
		,	 cl_read_pidfile(PID_FILE));
		mgmtd_log(LOG_ERR, "Startup aborted (already running)."
				  "Shutting down.");
		exit(100);
	}
	register_pid(FALSE, sigterm_action);

	/* enable coredumps */
	mgmtd_log(LOG_DEBUG, "Enabling coredumps");
 	cl_cdtocoredir();
	cl_enable_coredumps(TRUE);	
	cl_set_all_coredump_signal_handlers();

	/* enable dynamic up/down debug level */
	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGUSR1, 
		 	debug_level_adjust, NULL, NULL);
	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGUSR2, 
		 	debug_level_adjust, NULL, NULL);
		
	/* create the internal data structures */
	clients = g_hash_table_new(g_int_hash, g_int_equal);
	msg_map = g_hash_table_new_full(g_str_hash, g_str_equal, cl_free, NULL);
	evt_map = g_hash_table_new_full(g_str_hash, g_str_equal, cl_free, NULL);

	/* create the mainloop */
	mainloop = g_main_new(FALSE);
		
	/* init modules */
	init_general();
	init_heartbeat();
	init_lrm();
	init_crm();
	/* init ham & gnutls lib */
	tls_init_server();
	mgmt_set_mem_funcs(cl_malloc, cl_realloc, cl_free);

	/* create server socket */
	ssock = socket(AF_INET, SOCK_STREAM, 0);
	if (ssock == -1) {
		mgmtd_log(LOG_ERR, "Can not create server socket."
				  "Shutting down.");
		exit(100);
	}
	/* bind server socket*/
	memset(&saddr, '\0', sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(PORT);
	if (bind(ssock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
		mgmtd_log(LOG_ERR, "Can not bind server socket."
				  "Shutting down.");
		exit(100);
	}
	if (listen(ssock, 10) == -1) {
		mgmtd_log(LOG_ERR, "Can not start listen."
				"Shutting down.");
		exit(100);
	}	
			
	/* create source for server socket and add to the mainloop */
	sch = g_io_channel_unix_new(ssock);
	g_io_add_watch(sch, G_IO_IN|G_IO_ERR|G_IO_HUP, on_listen, NULL);
	
	/* run the mainloop */
	mgmtd_log(LOG_DEBUG, "main: run the loop...");
	mgmtd_log(LOG_INFO, "Started.");
	g_main_run(mainloop);

	/* exit, clean the pid file */
	final_crm();
	final_lrm();
	final_heartbeat();
	final_general();
	if (cl_unlock_pidfile(PID_FILE) == 0) {
		mgmtd_log(LOG_DEBUG, "[%s] stopped", mgmtd_name);
	}

	return 0;
}

gboolean
on_listen(GIOChannel *source, GIOCondition condition, gpointer data)
{
	char* msg;
	void* session;
	int ssock, csock;
	size_t laddr;
	struct sockaddr_in addr;
	int num = 0;
	char** args = NULL;

	if (condition & G_IO_IN) {
		/* accept the connection */
		ssock = g_io_channel_unix_get_fd(source);
		laddr = sizeof(addr);
		csock = accept(ssock, (struct sockaddr*)&addr, &laddr);
		if (csock == -1) {
			mgmtd_log(LOG_ERR, "%s accept socket failed", __FUNCTION__);
			return TRUE;
		}	
		/* create gnutls session for the server socket */
		session = tls_attach_server(csock);
		msg = mgmt_session_recvmsg(session);
		mgmtd_log(LOG_DEBUG, "recv msg: %s", msg);
		args = mgmt_msg_args(msg, &num);
		if (msg == NULL || num != 3 || STRNCMP_CONST(args[0], MSG_LOGIN) != 0) {
			mgmt_del_args(args);
			mgmt_del_msg(msg);
			mgmt_session_sendmsg(session, MSG_FAIL);
			tls_detach(session);
			close(csock);
			mgmtd_log(LOG_ERR, "%s receive login msg failed", __FUNCTION__);
			return TRUE;
		}
		/* authorization check with pam */	
		if (pam_auth(args[1],args[2]) != 0) {
			mgmt_del_args(args);
			mgmt_del_msg(msg);
			mgmt_session_sendmsg(session, MSG_FAIL);
			tls_detach(session);
			close(csock);
			mgmtd_log(LOG_ERR, "%s pam auth failed", __FUNCTION__);
			return TRUE;
		}
		mgmt_del_args(args);
		mgmt_del_msg(msg);
		mgmtd_log(LOG_DEBUG, "send msg: %s", MSG_OK);
		mgmt_session_sendmsg(session, MSG_OK);
		new_client(csock, session);
		return TRUE;
		
	}
	
	return TRUE;
}

gboolean
on_msg_arrived(GIOChannel *source, GIOCondition condition, gpointer data)
{
	client_t* client;
	char* msg;
	char* ret;
	
	if (condition & G_IO_IN) {
		client = lookup_client((int)data);
		if (client == NULL) {
			return TRUE;
		}
		msg = mgmt_session_recvmsg(client->session);
		mgmtd_log(LOG_DEBUG, "recv msg: %s", msg);
		if (msg == NULL || STRNCMP_CONST(msg, MSG_LOGOUT) == 0) {
			mgmt_del_msg(msg);
			del_client(client->id);
			return FALSE;
		}
		ret = dispatch_msg(msg, client->id);
		if (ret != NULL) {
			mgmtd_log(LOG_DEBUG, "send msg: %s", ret);
			mgmt_session_sendmsg(client->session, ret);
			mgmt_del_msg(ret);
		}
		else {
			mgmtd_log(LOG_DEBUG, "send msg: %s", MSG_FAIL);
			mgmt_session_sendmsg(client->session, MSG_FAIL);
		}
		mgmt_del_msg(msg);
	}
	
	return TRUE;
}

int
new_client(int sock, void* session)
{
	static int id = 1;
	client_t* client = cl_malloc(sizeof(client_t));
	client->id = id;
	client->ch = g_io_channel_unix_new(sock);
	g_io_add_watch(client->ch, G_IO_IN|G_IO_ERR|G_IO_HUP
	, 		on_msg_arrived, (gpointer)client->id);
	client->session = session;
	g_hash_table_insert(clients, (gpointer)&client->id, client);
	id++;
	return 0;
}

client_t*
lookup_client(int id)
{
	client_t* client = (client_t*)g_hash_table_lookup(clients, &id);
	if (client == NULL) {
		mgmtd_log(LOG_ERR, "no client id: %d", id);
	}
	return client;
}
int
del_client(int id)
{
	client_t* client = lookup_client(id);
	if (client == NULL) {
		return -1;
	}
	tls_detach(client->session);
	g_io_channel_unref(client->ch);
	g_hash_table_remove(clients, (gpointer)&client->id);
	cl_free(client);
	return 0;
}

int 
pam_auth(const char* user, const char* passwd)
{
	pam_handle_t *pamh = NULL;
	int ret;
	struct pam_conv conv;

	conv.conv = pam_conv;
	conv.appdata_ptr = strdup(passwd);

	ret = pam_start (mgmtd_name, user, &conv, &pamh);

	if (ret == PAM_SUCCESS) {
		ret = pam_authenticate (pamh, 0);
	}
	pam_end (pamh, ret);
	return ret == PAM_SUCCESS?0:-1;
}

int
pam_conv(int n, const struct pam_message **msg,
	 struct pam_response **resp, void *data)
{
	struct pam_response *reply;
	int i;
	char* passwd = (char*)data;
	*resp = NULL;
	
	/* 
	Alloc memory for response. refer to the url, we must use malloc.
	http://www.kernel.org/pub/linux/libs/pam/Linux-PAM-html/pam_appl-4.html#ss4.1
	*/
	reply = malloc(n * sizeof(*reply));
	if (reply == NULL) {
		return PAM_CONV_ERR;
	}
	memset(reply, 0, n * sizeof(*reply));

	/* process the msg from pam modules */
	for (i = 0; i < n; ++i) {
		switch (msg[i]->msg_style) {
			case PAM_PROMPT_ECHO_OFF:
			case PAM_PROMPT_ECHO_ON:
				reply[i].resp = passwd;
				break;
			case PAM_ERROR_MSG:
			case PAM_TEXT_INFO:
				break;
			default:
				free(reply);
				return PAM_CONV_ERR;
		}
	}
	*resp = reply;
	
	return PAM_SUCCESS;
}
int
reg_msg(const char* type, msg_handler fun)
{
	if (g_hash_table_lookup(msg_map, type) != NULL) {
		return -1;
	}
	g_hash_table_insert(msg_map, cl_strdup(type),(gpointer)fun);
	return 0;
}

int
fire_evt(const char* event)
{
	GList* id_list;
	GList* node;
	int list_changed = 0;
	
	char** args = mgmt_msg_args(event, NULL);
	if (args == NULL) {
		return -1;
	}
	id_list = g_hash_table_lookup(evt_map, args[0]);
	if (id_list == NULL) {
		mgmt_del_args(args);
		return -1;
	}
	node = id_list;
	while (node != NULL) {
		client_t* client;
		
		int id = (int)node->data;
		client = lookup_client(id);
		if (client == NULL) {
			/* remove the client id */
			node = g_list_next(node);
			id_list = g_list_remove(id_list, (gpointer)id);
			list_changed = 1;
			continue;
		}

		mgmtd_log(LOG_DEBUG, "send evt: %s", event);
		mgmt_session_sendmsg(client->session, event);
		mgmtd_log(LOG_DEBUG, "send evt: %s done", event);
		
		node = g_list_next(node);
	}
	if (list_changed == 1) {
		g_hash_table_replace(evt_map, cl_strdup(args[0]), (gpointer)id_list);
	}	
	mgmt_del_args(args);
	return 0;
}

char*
dispatch_msg(const char* msg, int client_id)
{
	msg_handler handler;
	char* ret;
	int num;
	char** args = mgmt_msg_args(msg, &num);
	if (args == NULL) {
		return NULL;
	}
	handler = (msg_handler)g_hash_table_lookup(msg_map, args[0]);
	if ( handler == NULL) {
		mgmt_del_args(args);
		return NULL;
	}
	ret = (*handler)(args, num, client_id);
	mgmt_del_args(args);
	return ret;
}
int
reg_evt(const char* type, int client_id)
{
	GList* id_list = g_hash_table_lookup(evt_map, type);
	id_list = g_list_append(id_list, (gpointer)client_id);
	g_hash_table_replace(evt_map, cl_strdup(type), (gpointer)id_list);
	return 0;
}
void
shutdown_mgmtd(void)
{
	mgmtd_log(LOG_INFO,"mgmtd is shutting down");
	if (mainloop != NULL && g_main_is_running(mainloop)) {
		g_main_quit(mainloop);
	}else {
		exit(LSB_EXIT_OK);
	}
}
