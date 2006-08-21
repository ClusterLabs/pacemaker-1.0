/*
 * tsa_eventd.c: event daemon
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_pidfile.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/lsb_exitcodes.h>
#include <ha_msg.h>
#include <mgmt/mgmt.h>
#include "ha_tsa_common.h"

#define	IPC_PATH_ATTR	"path"		/* pathname attribute */
#define TSA_CMDPATH 	HA_VARRUNDIR"/heartbeat/tsa_eventd"
#define PID_FILE 	HA_VARRUNDIR"/tsa_eventd.pid"
#define MAX_CLIENT	16
#define CMD_SUBSCRIBE_EVENT	"cmd_subscribe_event"

typedef struct {
	GCHSource * gsrc;
	IPC_Channel *ch;
} eventd_client_t;

eventd_client_t	*event_client = NULL;
GMainLoop* 	mainloop = NULL;

void		tsa_shutdown(int signo);
void *		tsa_main_loop(void * param);
int		eventd_stop(const char *pid_file);
int		cib_event_handler(const char *event);
int		disconnected_handler(const char *event);
void 		init_logger(const char * entity);
gboolean	sigterm_action(int nsig, gpointer user_data);
void		register_pid(gboolean do_fork, 
			gboolean (*shutdown)(int nsig, gpointer userdata));
int		eventd_start(int debug);
int		eventd_status(void);
int		eventd_stop(const char *pid_file);
gboolean	on_channel_connect (IPC_Channel* ch, gpointer user_data);
void		on_channel_remove (gpointer user_data);
gboolean	on_receive_cmnd(IPC_Channel *ch, gpointer user_data);
char*		process_mgmt_msg(const char *arg);


char*  
process_mgmt_msg(const char *arg)
{
	char *msg = NULL, *result = NULL;
	int i, len;
	char *buf = NULL;
	char ** cmd = NULL;

	cl_log(LOG_DEBUG, "%s: arg = %s", __FUNCTION__, arg);
	if ( (cmd = split_string(arg , &len, " ")) == NULL || len == 0) {
		return NULL;
	}

	msg = mgmt_new_msg(cmd[0], NULL);
	for(i = 1; i < len; i++ ) {
		msg = mgmt_msg_append(msg, cmd[i]);
	}

	result = process_msg(msg);
	mgmt_del_msg(msg);

	if ( result == NULL ) {
		free_array((void**)cmd, len);
        	final_mgmt_lib(); 
		return NULL;
	}
	buf = cl_strdup(result);
	mgmt_del_msg(result);
	free_array((void**)cmd, len);

	return buf;
}

int
cib_event_handler(const char *event)
{
	struct ha_msg* msg = NULL;

	cl_log(LOG_INFO, "%s: event received: %s", __FUNCTION__, event);
	if ( event_client && event_client->ch ) {	
		if ((msg = ha_msg_new(1)) == NULL ) {
			return 0;
		}		
		ha_msg_add(msg, "event", event);
		if ( msg2ipcchan(msg, event_client->ch) != HA_OK ) {
			cl_log(LOG_ERR, "%s: send msg to client failed.", __FUNCTION__);
		}

		ha_msg_del(msg);
	} else {
		cl_log(LOG_WARNING, "%s: no event client.", __FUNCTION__);
	}	
	return 1;
}


int
disconnected_handler(const char *event)
{
	/* TODO: send event */
	return 1;
}

gboolean
sigterm_action(int nsig, gpointer user_data)
{
	tsa_shutdown(0);	
	return TRUE;
}

void
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

	cl_signal_set_interrupt(SIGINT, 0);
	cl_signal_set_interrupt(SIGHUP, 0);
}


gboolean
on_channel_connect (IPC_Channel* ch, gpointer user_data)
{
	eventd_client_t* client = NULL;

	cl_log(LOG_INFO, "%s: client with farside_pid %u signon",
			__FUNCTION__, ch->farside_pid);

	if ( (client = cl_malloc(sizeof(eventd_client_t))) == NULL ) {
		cl_log(LOG_ERR, "%s: create client failed.", __FUNCTION__);
		return TRUE; 
	}

	client->ch = ch;
	client->gsrc = G_main_add_IPC_Channel(G_PRIORITY_DEFAULT,
			ch, FALSE, on_receive_cmnd, client,
			on_channel_remove);

	return TRUE;
}

void
on_channel_remove(gpointer user_data)
{
	eventd_client_t *client = (eventd_client_t*) user_data;

	cl_log(LOG_INFO, "%s: client with farside pid %u removed.", 
		__FUNCTION__, client->ch->farside_pid);

	/* if it is the event client, set event client to NULL */
	if ( client == event_client ) {
		event_client = NULL;
	}

	if ( client->gsrc != NULL ) {
		G_main_del_IPC_Channel(client->gsrc);
	}

	cl_free(client);
}


gboolean	
on_receive_cmnd(IPC_Channel *ch, gpointer user_data)
{
	eventd_client_t *client = NULL;
	struct ha_msg *msg = NULL;
	const char * cmd = NULL;

	client = (eventd_client_t*) user_data;
	if (ch->ch_status == IPC_DISCONNECT ) {
		return FALSE;
	}

	if ( ! ch->ops->is_message_pending(ch)) {
		return FALSE;
	}
	
	if ((msg = msgfromIPC_noauth(ch)) == NULL ) {
		return TRUE;
	}

	if ( (cmd = ha_msg_value(msg, "cmd")) == NULL ) {
		return TRUE;
	}
	
	if ( strcmp(cmd, CMD_SUBSCRIBE_EVENT) == 0 ) {
		cl_log(LOG_INFO, "%s: client with farside_pid %u is the event client.",
			__FUNCTION__, ch->farside_pid);
		event_client = client;
	} else {
		char* buf = NULL;
		struct ha_msg * result = ha_msg_new(1);	

		if ( result == NULL ) {
			cl_log(LOG_ERR, "%s: alloc result failed.", __FUNCTION__);
			ha_msg_del(msg);
			return TRUE;
		}
		
		buf = process_mgmt_msg(cmd);	
		if ( buf ) {
			cl_log(LOG_DEBUG, "%s: got result: %s.", __FUNCTION__, buf);
			ha_msg_add(result, "result", buf);
		} else {
			ha_msg_add(result, "result", "");
		}

		if ( msg2ipcchan(result, ch) != HA_OK ) {
			cl_log(LOG_ERR, "%s: send msg to client failed.", __FUNCTION__);
		}
		if ( buf ) {
			cl_free(buf);
		}
		ha_msg_del(result);
	}
		
	ha_msg_del(msg);
	return TRUE;
}

int
eventd_status(void)
{
	long	pid = cl_read_pidfile(PID_FILE);

	if (pid > 0) {
		fprintf(stderr, "eventd is running [pid: %ld]\n", pid);
		return LSB_STATUS_OK;
	}
	fprintf(stderr, "eventd is stopped.\n");
	return LSB_STATUS_STOPPED;
}

void
tsa_shutdown(int signo) 
{
        if ( mainloop && g_main_is_running(mainloop)) {
                g_main_quit(mainloop);
        }
}

int
eventd_start(int debug)
{
	char path[] = IPC_PATH_ATTR;
	char cmd_path[] = TSA_CMDPATH;
	GHashTable * conn_attrs;
	IPC_WaitConnection * conn_cmd = NULL;

	/* init */
	if(cl_lock_pidfile(PID_FILE) < 0 ){
		cl_log(LOG_ERR, "already running.");
		exit(100);
	}
       
	register_pid( FALSE, sigterm_action);
	cl_cdtocoredir();
	cl_enable_coredumps(TRUE);	
	cl_set_all_coredump_signal_handlers();


	/* mgmt lib */
	init_mgmt_lib("tsad", ENABLE_LRM|ENABLE_CRM|ENABLE_HB|CACHE_CIB);
	reg_event(EVT_CIB_CHANGED, cib_event_handler);
	reg_event(EVT_DISCONNECTED, disconnected_handler);
	
	/* IPC channel */
	conn_attrs = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(conn_attrs, path, cmd_path);
	conn_cmd = ipc_wait_conn_constructor(IPC_ANYTYPE, conn_attrs);
	g_hash_table_destroy(conn_attrs);

	G_main_add_IPC_WaitConnection(G_PRIORITY_HIGH, conn_cmd, 
			NULL, FALSE, on_channel_connect, conn_cmd, NULL);
        
	/* main loop */
        mainloop = g_main_new(FALSE);
        if ( mainloop == NULL ) {
                cl_log(LOG_ERR, "%s: couldn't creat mainloop", __FUNCTION__);
        }
        g_main_run(mainloop);

	/* loop completes, clean up */
	final_mgmt_lib();
	return 0;
}

int
eventd_stop(const char *pid_file)
{
	long	pid;
	int	rc = LSB_EXIT_OK;
	pid = cl_read_pidfile(pid_file);
	if (pid > 0) {
		if (CL_KILL((pid_t)pid, SIGTERM) < 0) {
			rc = (errno == EPERM ?
				LSB_EXIT_EPERM : LSB_EXIT_GENERIC);
			fprintf(stderr, "Cannot kill pid %ld\n", pid);
		} else {
			while (CL_PID_EXISTS(pid)) {
				sleep(1);
			}
		}
	}
	return rc;
}

int
main(int argc, char ** argv)
{
	int flag = 0;
	int debug_level = 0;
	int rq_stop = 0;
	int rq_status = 0;
	int argerr = 0;
	
	init_logger("TSA-eventd");
	while ((flag = getopt(argc, argv, "dsk")) != -1) {
		switch(flag) {
		case 'd':
			++debug_level;
			break;
		case 'k':
			rq_stop = TRUE;
			break;
		case 's':
			rq_status = TRUE;
			break;
		default:
			++argerr;
			break;
		}
	}

	if(rq_stop) {
		return eventd_stop(PID_FILE);
	} else if(rq_status){
		return eventd_status();
	} else {	
		return eventd_start(debug_level);
	}
}

