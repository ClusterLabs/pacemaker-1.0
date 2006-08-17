/*
 * ha_mgmt_client.c: functions called by JAVA 
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
#include <ha_msg.h>
#include <mgmt/mgmt.h>
#include "ha_mgmt_client.h"
#include "ha_tsa_common.h"


#define CMD_SUBSCRIBE_EVENT	"cmd_subscribe_event"
#define	IPC_PATH_ATTR	"path"		/* pathname attribute */
#define TSA_CMDPATH 	HA_VARRUNDIR"/heartbeat/tsa_eventd"
#define TSA_HA_CLI	HA_LIBHBDIR"/tsa_hacli"
/*FIXME: remove hardcode pathname */
#define HB_SCRIPT	"/etc/init.d/heartbeat"	

static IPC_Channel*	eventd_signon(void);
static int		eventd_signoff(IPC_Channel*);

static IPC_Channel*
eventd_signon ()
{
	GHashTable* eventd_chan_attrs;
	IPC_Channel * ipc_chan;
	char path[] = IPC_PATH_ATTR;
	char cmd_path[] = TSA_CMDPATH;

	/* create the command ipc channel to eventd*/
	eventd_chan_attrs = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(eventd_chan_attrs, path, cmd_path);
	ipc_chan = ipc_channel_constructor(IPC_ANYTYPE, eventd_chan_attrs);
	g_hash_table_destroy(eventd_chan_attrs);

	if ( ipc_chan == NULL ){
		eventd_signoff(ipc_chan);
		cl_log(LOG_WARNING,
			"eventd_signon: can not connect to eventd channel");
		return NULL;
	}

	if ( ipc_chan->ops->initiate_connection(ipc_chan) != IPC_OK) {
		eventd_signoff(ipc_chan);
		cl_log(LOG_WARNING,
			"eventd_signon: can not initiate connection");
		return NULL;
	}
	return ipc_chan;
}

static int
eventd_signoff (IPC_Channel * ipc_chan)
{
	if (ipc_chan) {
		if (IPC_ISWCONN(ipc_chan)) {
	 		ipc_chan->ops->destroy(ipc_chan);
	 	}
		ipc_chan = NULL;
	}
	return HA_OK;
}

/* wait_for_events: block until an event received,
	called by Java code */
char *	
wait_for_events(void)
{
	static char buf[1024];	/* to avoid memory leak */
	struct ha_msg *msg = NULL, *cmd = NULL;
	IPC_Channel * ipc_chan;
	
	while ( (ipc_chan = eventd_signon()) == NULL ) {
		sleep(3);
	}

	if ( ( cmd = ha_msg_new(1) ) == NULL ) {
		cl_log(LOG_ERR, "%s: alloc msg failed.", __FUNCTION__);
		goto exit;
	}
	
	ha_msg_add(cmd, "cmd", CMD_SUBSCRIBE_EVENT);
	if ( msg2ipcchan(cmd, ipc_chan) != HA_OK ) {
		cl_log(LOG_ERR, "%s: send msg to ipchan failed.",
			__FUNCTION__);
	}

	msg = msgfromIPC(ipc_chan, MSG_ALLOWINTR);
	ha_msg_del(cmd);

exit:
	eventd_signoff(ipc_chan);
	if ( msg ) {
		strncpy(buf, ha_msg_value(msg, "event"), 1024);
		ha_msg_del(msg);
		return buf;
	}
	return NULL;
}


void
clLog(int priority, const char* logs)
{
	init_logger("TSA");
	cl_log(priority, "%s", logs);
}


void	
start_heartbeat(const char* node)
{
	char * ssh_path = NULL;
	int rc, len;

	ssh_path = run_shell_cmnd("which ssh", &rc, &len);
	if ( ssh_path == NULL ) {
		return;
	}
	ssh_path[strlen(ssh_path) - 1] = '\0';
	if ( fork() == 0 ) {
		char cmd[1024];
		snprintf(cmd, 1024, "%s %s \"%s start\"", ssh_path, node, HB_SCRIPT);
		system(cmd);
	}
	cl_free(ssh_path);
}


void	
stop_heartbeat(const char* node)
{
	char * ssh_path = NULL;
	int rc, len;

	ssh_path = run_shell_cmnd("which ssh", &rc, &len);
	if ( ssh_path == NULL ) {
		return;
	}
	ssh_path[strlen(ssh_path) - 1] = '\0';
	if ( fork() == 0 ) {
		char cmd[1024];
		snprintf(cmd, 1024, "%s %s \"%s start\"", ssh_path, node, HB_SCRIPT);
		system(cmd);
	}
	cl_free(ssh_path);
}


/* native way: talk to library with JNI */
char*  
process_cmnd_native(const char* cmd)
{
	static char buf[1024];	/* to avoid memory leak */
	char *msg = NULL, *result = NULL, **cmd_args = NULL;
	int i, len;

	init_logger("TSA");
        
	init_mgmt_lib("tsa", ENABLE_LRM|ENABLE_CRM|ENABLE_HB|CACHE_CIB);
	cl_log(LOG_DEBUG, "%s: begin.", __FUNCTION__);
	/* cl_log(LOG_INFO, "%s: cmd: %s", __FUNCTION__, cmd); */
	cmd_args = split_string(cmd, &len, " ");
	if ( cmd_args == NULL ) {
		cl_log(LOG_INFO, "%s: cmd_args is NULL.", __FUNCTION__);
        	final_mgmt_lib(); 
		return NULL;
	}

	msg = mgmt_new_msg(cmd_args[0], NULL);
	for(i = 1; i < len; i++ ) {
		msg = mgmt_msg_append(msg, cmd_args[i]);
	}

	cl_log(LOG_DEBUG, "process_command: [%s}", msg);
	result = process_msg(msg);
	cl_log(LOG_DEBUG, "process_command: [%s] => [%s]", msg, result); 
	mgmt_del_msg(msg);
	free_array((void**)cmd_args, len);
	if ( result == NULL ) {
		cl_log(LOG_ERR, "%s: end. result is NULL.", __FUNCTION__);
        	final_mgmt_lib(); 
		return NULL;
	}
	strncpy(buf, result, 1024);
	mgmt_del_msg(result);
	cl_log(LOG_DEBUG, "%s: end.", __FUNCTION__);


        final_mgmt_lib(); 
	return buf;
}

/* external way: 
	invoke the external tool tsa_hacli to operate on linux-ha 
*/
char*	
process_cmnd_external(const char *cmd)
{
	static char result[4096];
	char cmd_buf[1024];
	char * buf = NULL;
	int rc, len;

	memset(result, 0, 4096);
	snprintf(cmd_buf, 1024, "%s %s", TSA_HA_CLI, cmd);
	buf = run_shell_cmnd(cmd_buf, &rc, &len); 
	if ( buf ) {
		strncpy(result, buf, 4096);	
		cl_free(buf);
	}
	return result;
}

/* eventd way:
	talk to event daemon with IPC
*/
char*
process_cmnd_eventd(const char *cmd)
{
	static char buf[1024];	/* to avoid memory leak */
	struct ha_msg *cmd_msg = NULL, *msg = NULL;
	IPC_Channel * ipc_chan = NULL;
	const char * result = NULL;

	memset(buf, 0, 1024);
	while ( ( ipc_chan = eventd_signon()) == NULL ) {
		sleep(3);
	}

	if ( ( cmd_msg = ha_msg_new(1) ) == NULL ) {
		cl_log(LOG_ERR, "%s: alloc msg failed.", __FUNCTION__);
		goto exit;
	}
	
	ha_msg_add(cmd_msg, "cmd", cmd);
	if ( msg2ipcchan(cmd_msg, ipc_chan) != HA_OK ) {
		cl_log(LOG_ERR, "%s: send msg to ipchan failed.",
			__FUNCTION__);
	}
	ha_msg_del(cmd_msg);
	msg = msgfromIPC(ipc_chan, MSG_ALLOWINTR);
	if ( msg && ( result = ha_msg_value(msg, "result"))) {
		strncpy(buf, result, 1024);
	}
exit:
	eventd_signoff(ipc_chan);
	return buf;
}
