/*
 * Linux HA quorum daemon
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
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

#include <portability.h>

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>

#include <glib.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <heartbeat.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_syslog.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/cl_pidfile.h>
#include <ha_msg.h>

#include <clplumbing/cl_plugin.h>
#include <clplumbing/cl_quorumd.h>

/* x.509 related */
#define SERVERKEY 	HB_HA_DIR"/server-key.pem"
#define SERVERCERT	HB_HA_DIR"/server-cert.pem"
#define CACERT		HB_HA_DIR"/ca-cert.pem"
#define CACRL		HB_HA_DIR"/ca-crl.pem"
#define DH_BITS 1024


static int verify_certificate (gnutls_session session, char* CN);
static gnutls_session initialize_tls_session (int sd, char* CN);
static void initialize_tls_global(void);

static gnutls_dh_params dh_params;
static gnutls_certificate_credentials x509_cred;

/* Message types */

#define ENV_PREFIX 	"HA_"
#define KEY_LOGDAEMON   "use_logd"
#define HADEBUGVAL	"HA_DEBUG"
#define OPTARGS		"skrhvt"
#define PID_FILE 	HA_VARRUNDIR"/quorumd.pid"
#define QUORUMD		"quorumd"

#define PORT		5561

static gboolean sig_handler(int nsig, gpointer user_data);

static void usage(const char* cmd, int exit_status);
static int init_start(void);
static int init_stop(const char *pid_file);
static int init_status(const char *pid_file, const char *client_name);
static void shutdown_quorumd(void);
static gboolean sigterm_action(int nsig, gpointer unused);

static gboolean on_listen(GIOChannel *ch
, 			  GIOCondition condition
,			  gpointer data);
static struct hb_quorumd_fns* get_protocol(const char* version);
static void _load_config_file(gpointer key, gpointer value, gpointer user_data);
static void _dump_data(gpointer key, gpointer value, gpointer user_data);


extern int debug_level;
static GMainLoop* mainloop = NULL;
static GHashTable* protocols = NULL;

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
				usage(QUORUMD, LSB_EXIT_OK);
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
		quorum_log(LOG_ERR,"WHY WE ARE HERE?");
		++argerr;
	}

	if (argerr) {
 		usage(QUORUMD, LSB_EXIT_GENERIC);
	}

	inherit_debuglevel = getenv(HADEBUGVAL);
	if (inherit_debuglevel != NULL) {
		debug_level = atoi(inherit_debuglevel);
		if (debug_level > 2) {
			debug_level = 2;
		}
	}

	cl_log_set_entity(QUORUMD);
	cl_log_enable_stderr(FALSE);
	cl_log_set_facility(LOG_DAEMON);

	/* Use logd if it's enabled by heartbeat */
	cl_inherit_use_logd(ENV_PREFIX""KEY_LOGDAEMON, 0);

	inherit_logconfig_from_environment();

	if (req_status){
		return init_status(PID_FILE, QUORUMD);
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
	long pid = cl_read_pidfile(pid_file);
	
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
		quorum_log(LOG_ERR, "No pid file specified to kill process");
		return LSB_EXIT_GENERIC;
	}
	pid =	cl_read_pidfile(pid_file);

	if (pid > 0) {
		if (CL_KILL((pid_t)pid, SIGTERM) < 0) {
			rc = (errno == EPERM
			      ?	LSB_EXIT_EPERM : LSB_EXIT_GENERIC);
			fprintf(stderr, "Cannot kill pid %ld\n", pid);
		}else{
			quorum_log(LOG_INFO,
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
				"\n\tk: kill\n\th: help\n\tv: debug\n";

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
	shutdown_quorumd();	
	return TRUE;
}
void
shutdown_quorumd(void)
{
	quorum_log(LOG_INFO,"quorumd is shutting down");
	if (mainloop != NULL && g_main_is_running(mainloop)) {
		g_main_quit(mainloop);
	}else {
		exit(LSB_EXIT_OK);
	}
}

static void
register_pid(gboolean do_fork,
	     gboolean (*shutdown)(int nsig, gpointer userdata))
{
	int j;
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

gboolean 
sig_handler(int nsig, gpointer user_data)
{
	switch (nsig) {
		case SIGUSR1:
			debug_level++;
			if (debug_level > 2) {
				debug_level = 0;
			}
			quorum_log(LOG_INFO, "set debug_level to %d", debug_level);
			break;

		case SIGUSR2:
			g_hash_table_foreach(protocols, _dump_data, GINT_TO_POINTER(LOG_INFO));
			break;
		
		case SIGHUP:
			g_hash_table_foreach(protocols, _load_config_file, NULL);
			break;
			
		default:
			quorum_log(LOG_WARNING, "sig_handler: Received an "
				"unexpected signal(%d). Something wrong?.",nsig);
	}

	return TRUE;
}


/* main loop of the daemon*/
int
init_start ()
{
	int 			ssock;
	struct sockaddr_in 	saddr;
	GIOChannel* 		sch;
	
	/* register pid */
	if (cl_lock_pidfile(PID_FILE) < 0) {
		quorum_log(LOG_ERR, "already running: [pid %d]."
		,	 cl_read_pidfile(PID_FILE));
		quorum_log(LOG_ERR, "Startup aborted (already running)."
				  "Shutting down.");
		exit(100);
	}
	register_pid(FALSE, sigterm_action);

	/* enable coredumps */
	quorum_log(LOG_DEBUG, "Enabling coredumps");
 	cl_cdtocoredir();
	cl_enable_coredumps(TRUE);	
	cl_set_all_coredump_signal_handlers();
	
	/* initialize gnutls */
	initialize_tls_global();
	
	/* enable dynamic up/down debug level */
	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGUSR1, 
				 sig_handler, NULL, NULL);
	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGUSR2, 
				 sig_handler, NULL, NULL);
	G_main_add_SignalHandler(G_PRIORITY_HIGH, SIGHUP, 
				 sig_handler, NULL, NULL);
		
	/* create the mainloop */
	mainloop = g_main_new(FALSE);

	/* create the protocal table */
	protocols = g_hash_table_new(g_str_hash, g_str_equal);
		
	/* create server socket */
	ssock = socket(AF_INET, SOCK_STREAM, 0);
	if (ssock == -1) {
		quorum_log(LOG_ERR, "Can not create server socket."
				  "Shutting down.");
		exit(100);
	}
	/* bind server socket*/
	memset(&saddr, '\0', sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(PORT);
	if (bind(ssock, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
		quorum_log(LOG_ERR, "Can not bind server socket."
				  "Shutting down.");
		exit(100);
	}
	if (listen(ssock, 10) == -1) {
		quorum_log(LOG_ERR, "Can not start listen."
				"Shutting down.");
		exit(100);
	}	

	/* create source for server socket and add to the mainloop */
	sch = g_io_channel_unix_new(ssock);
	g_io_add_watch(sch, G_IO_IN|G_IO_ERR|G_IO_HUP, on_listen, NULL);
	
	/* run the mainloop */
	quorum_log(LOG_DEBUG, "main: run the loop...");
	quorum_log(LOG_INFO, "Started.");

	g_main_run(mainloop);

	/* exit, clean the pid file */
	if (cl_unlock_pidfile(PID_FILE) == 0) {
		quorum_log(LOG_DEBUG, "[%s] stopped", QUORUMD);
	}

	return 0;
}

gboolean
on_listen(GIOChannel *ch, GIOCondition condition, gpointer data)
{
	int ssock, csock;
	unsigned laddr;
	struct sockaddr_in addr;
	char buf[MAXMSG];
	char CN[MAX_DN_LEN];
	ssize_t len;
	gnutls_session session;
	struct hb_quorumd_fns *fns;

	if (condition & G_IO_IN) {
		/* accept the connection */
		ssock = g_io_channel_unix_get_fd(ch);
		laddr = sizeof(addr);
		csock = accept(ssock, (struct sockaddr*)&addr, &laddr);
		if (csock == -1) {
			quorum_log(LOG_ERR, "%s accept socket failed", __FUNCTION__);
			return TRUE;
		}
		memset(CN, 0, MAX_DN_LEN);
		session = initialize_tls_session(csock, CN);
		if (session == NULL) {
			quorum_log(LOG_ERR, "%s tls handshake failed", __FUNCTION__);
			close(csock);
			return TRUE;
		}
		memset(buf,0,MAXMSG);
		len = gnutls_record_recv(session, buf, MAXMSG);		
		if (len <= 0) {
			quorum_log(LOG_ERR, "can't get version info");
			gnutls_bye (session, GNUTLS_SHUT_WR);
			gnutls_deinit (session);
			close(csock);
			return TRUE;
		}
		quorum_debug(LOG_DEBUG, "version:%s(%d)",buf,(int)len);
		fns = get_protocol(buf);
		if(fns != NULL) {
			fns->on_connect(csock,session,CN);
		}
		else {
			quorum_log(LOG_WARNING, "version %s is not supported", buf);
			gnutls_bye (session, GNUTLS_SHUT_WR);
			gnutls_deinit (session);
			close(csock);
		}
	}
	return TRUE;
}

struct hb_quorumd_fns* 
get_protocol(const char* version)
{
	struct hb_quorumd_fns*	protocol;
	protocol = g_hash_table_lookup(protocols, version);
	
	if (protocol == NULL) {
		protocol = cl_load_plugin("quorumd", version);
		if (protocol != NULL) {
			if (protocol->init() != -1) {
				g_hash_table_insert(protocols, cl_strdup(version), protocol);
			}
			else {
				protocol = NULL;
			}
		}
			
	}
	return protocol;
}
void 
_load_config_file(gpointer key, gpointer value, gpointer user_data)
{
	struct hb_quorumd_fns*	protocol = (struct hb_quorumd_fns*) value;
	protocol->load_config_file();
}
void 
_dump_data(gpointer key, gpointer value, gpointer user_data)
{
	struct hb_quorumd_fns*	protocol = (struct hb_quorumd_fns*) value;
	protocol->dump_data(GPOINTER_TO_INT(user_data));
}
int
verify_certificate (gnutls_session session, char* CN)
{
	unsigned int cert_list_size;
	const gnutls_datum *cert_list;
	int ret;
	char dn[MAX_DN_LEN];
	size_t dn_len = MAX_DN_LEN;
	gnutls_x509_crt cert;

	ret = gnutls_certificate_verify_peers(session);

	if (ret < 0)
	{
		quorum_debug(LOG_DEBUG,"gnutls_certificate_verify_peers2 returns error");
		return -1;
	}
	if (gnutls_certificate_type_get (session) != GNUTLS_CRT_X509) {
		quorum_debug(LOG_DEBUG,"The certificate is not a x.509 cert");
    		return -1;
	}
	if (gnutls_x509_crt_init (&cert) < 0)
	{
		quorum_debug(LOG_DEBUG,"error in gnutls_x509_crt_init");
		return -1;
	}

	cert_list = gnutls_certificate_get_peers (session, &cert_list_size);
	if (cert_list == NULL)
	{
		quorum_debug(LOG_DEBUG,"No certificate was found!");
		return -1;
	}

	if (gnutls_x509_crt_import (cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0)
	{
		quorum_debug(LOG_DEBUG,"error parsing certificate");
		return -1;
	}

	if (gnutls_x509_crt_get_expiration_time (cert) < time (0))
	{
		quorum_debug(LOG_DEBUG,"The certificate has expired");
		return -1;
	}

	if (gnutls_x509_crt_get_activation_time (cert) > time (0))
	{
		quorum_debug(LOG_DEBUG,"The certificate is not yet activated");
		return -1;
	}
	memset(dn, 0, MAX_DN_LEN);
	gnutls_x509_crt_get_dn(cert, dn, &dn_len);
	strncpy(CN, strstr(dn, "CN=")+3, MAX_DN_LEN);
	CN[MAX_DN_LEN-1]= '\0';
	quorum_debug(LOG_DEBUG,"The certificate cn:%s",CN);
	gnutls_x509_crt_deinit (cert);

	return 0;
}

gnutls_session
initialize_tls_session (int sd, char* CN)
{
	int ret;
	gnutls_session session;

	gnutls_init (&session, GNUTLS_SERVER);
	gnutls_set_default_priority (session);
	gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, x509_cred);
	gnutls_certificate_server_set_request (session, GNUTLS_CERT_REQUIRE);
	gnutls_dh_set_prime_bits (session, DH_BITS);
	gnutls_transport_set_ptr (session, (gnutls_transport_ptr) sd);
	ret = gnutls_handshake (session);
	if (ret < 0)
	{
		close (sd);
		gnutls_deinit (session);
		quorum_log(LOG_WARNING,"handshake failed");
		return NULL;
	}
	if (verify_certificate(session,CN) < 0) {
		return NULL;
	}
	return session;
}
void initialize_tls_global(void)
{
	gnutls_global_init ();

	gnutls_certificate_allocate_credentials (&x509_cred);
	gnutls_certificate_set_x509_trust_file (x509_cred, CACERT,
						GNUTLS_X509_FMT_PEM);
	gnutls_certificate_set_x509_crl_file (x509_cred, CACRL,
						GNUTLS_X509_FMT_PEM);
	gnutls_certificate_set_x509_key_file (x509_cred, SERVERCERT, SERVERKEY,
						GNUTLS_X509_FMT_PEM);
	gnutls_dh_params_init (&dh_params);
	gnutls_dh_params_generate2 (dh_params, DH_BITS);

	gnutls_certificate_set_dh_params (x509_cred, dh_params);
}
