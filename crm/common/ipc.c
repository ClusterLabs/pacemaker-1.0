/* $Id: ipc.c,v 1.22 2005/02/25 10:22:42 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
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
#include <sys/param.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>

#include <crm/crm.h>

#include <clplumbing/ipc.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/uids.h>
#include <clplumbing/realtime.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_poll.h>

#ifdef USE_LIBXML
#  include <libxml/xmlmemory.h>
#  include <libxml/parser.h>
#  include <libxml/xpath.h>
#endif

#include <crm/common/ipc.h>
#include <crm/msg_xml.h>
#include <ha_msg.h>


#include <crm/dmalloc_wrapper.h>

/* frees msg */
gboolean 
send_ipc_message(IPC_Channel *ipc_client, HA_Message *msg)
{
	gboolean all_is_good = TRUE;

	if (msg == NULL) {
		crm_err("cant send NULL message");
		all_is_good = FALSE;

	} else if (ipc_client == NULL) {
		crm_err("cant send message without an IPC Channel");
		all_is_good = FALSE;

	} else if(ipc_client->ch_status != IPC_CONNECT) {
		crm_err("IPC Channel is not connected");
		all_is_good = FALSE;

#if 0
	} else if(ipc_client->should_send_blocking == FALSE) {
		crm_verbose("Setting IPC Channel to blocking..."
			    " least some messages get lost");
		ipc_client->should_send_blocking = TRUE;
#endif
	}

	if(all_is_good && msg2ipcchan(msg, ipc_client) != HA_OK) {
		crm_err("Could not send IPC, message");
		all_is_good = FALSE;

		if(ipc_client->ch_status != IPC_CONNECT) {
			crm_err("IPC Channel is no longer connected");
		} 
		
	}	

	crm_log_message_adv(all_is_good?LOG_DEV:LOG_ERR,"outbound.ipc.log",msg);

	CRM_DEV_ASSERT(ipc_client->send_queue->current_qlen < ipc_client->send_queue->max_qlen);
	
	crm_msg_del(msg);
	
	return all_is_good;
}

void
default_ipc_connection_destroy(gpointer user_data)
{
	return;
}

int
init_server_ipc_comms(
	char *channel_name,
	gboolean (*channel_client_connect)(IPC_Channel *newclient,gpointer user_data),
	void (*channel_connection_destroy)(gpointer user_data))
{
	/* the clients wait channel is the other source of events.
	 * This source delivers the clients connection events.
	 * listen to this source at a relatively lower priority.
	 */
    
	char    commpath[SOCKET_LEN];
	IPC_WaitConnection *wait_ch;
	
	sprintf(commpath, WORKING_DIR "/%s", channel_name);

	wait_ch = wait_channel_init(commpath);

	if (wait_ch == NULL) {
		return 1;
	}
	
	G_main_add_IPC_WaitConnection(
		G_PRIORITY_LOW, wait_ch, NULL, FALSE,
		channel_client_connect, channel_name,
		channel_connection_destroy);

	crm_devel("Listening on: %s", commpath);

	return 0;
}

GCHSource*
init_client_ipc_comms(const char *channel_name,
		      gboolean (*dispatch)(
			      IPC_Channel* source_data, gpointer user_data),
		      void *client_data, IPC_Channel **ch)
{
	IPC_Channel *a_ch = NULL;
	GCHSource *the_source = NULL;
	void *callback_data = client_data;

	a_ch = init_client_ipc_comms_nodispatch(channel_name);
	if(ch != NULL) {
		*ch = a_ch;
		if(callback_data == NULL) {
			callback_data = a_ch;
		}
	}

	if(a_ch == NULL) {
		crm_err("Setup of client connection failed,"
			" not adding channel to mainloop");
		
		return NULL;
	}

	if(dispatch == NULL) {
		crm_warn("No dispatch method specified..."
			 "maybe you meant init_client_ipc_comms_nodispatch()?");
	} else {
		crm_devel("Adding dispatch method to channel");

		the_source = G_main_add_IPC_Channel(
			G_PRIORITY_LOW, a_ch, FALSE, dispatch, callback_data, 
			default_ipc_connection_destroy);
	}
	
	return the_source;
}

IPC_Channel *
init_client_ipc_comms_nodispatch(const char *channel_name)
{
	IPC_Channel *ch;
	GHashTable  *attrs;
	static char  path[] = IPC_PATH_ATTR;

	char *commpath = NULL;
	int local_socket_len = 2; /* 2 = '/' + '\0' */

	
	local_socket_len += strlen(channel_name);
	local_socket_len += strlen(WORKING_DIR);

	crm_malloc(commpath, sizeof(char)*local_socket_len);
	if(commpath != NULL) {
		sprintf(commpath, WORKING_DIR "/%s", channel_name);
		commpath[local_socket_len - 1] = '\0';
		crm_devel("Attempting to talk on: %s", commpath);
	}
	
	attrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(attrs, path, commpath);

	ch = ipc_channel_constructor(IPC_ANYTYPE, attrs);
	g_hash_table_destroy(attrs);

	if (ch == NULL) {
		crm_err("Could not access channel on: %s", commpath);
		return NULL;
		
	} else if (ch->ops->initiate_connection(ch) != IPC_OK) {
		crm_debug("Could not init comms on: %s", commpath);
		return NULL;
	}

	ch->ops->set_recv_qlen(ch, 100);
	ch->ops->set_send_qlen(ch, 100);
/* 	ch->should_send_blocking = TRUE; */

	crm_devel("Processing of %s complete", commpath);

	return ch;
}

IPC_WaitConnection *
wait_channel_init(char daemonsocket[])
{
	IPC_WaitConnection *wait_ch;
	mode_t mask;
	char path[] = IPC_PATH_ATTR;
	GHashTable * attrs;

	
	attrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(attrs, path, daemonsocket);
    
	mask = umask(0);
	wait_ch = ipc_wait_conn_constructor(IPC_ANYTYPE, attrs);
	if (wait_ch == NULL) {
		cl_perror("Can't create wait channel of type %s",
			  IPC_ANYTYPE);
		exit(1);
	}
	mask = umask(mask);
    
	g_hash_table_destroy(attrs);
    
	return wait_ch;
}



gboolean
subsystem_msg_dispatch(IPC_Channel *sender, void *user_data)
{
	int lpc = 0;
	IPC_Message *msg = NULL;
	ha_msg_input_t *new_input = NULL;
	gboolean all_is_well = TRUE;
	const char *sys_to;
	const char *task;

	while(sender->ops->is_message_pending(sender)) {
		gboolean process = FALSE;
		if (sender->ch_status == IPC_DISCONNECT) {
			/* The message which was pending for us is that
			 * the IPC status is now IPC_DISCONNECT */
			break;
		}
		if (sender->ops->recv(sender, &msg) != IPC_OK) {
			perror("Receive failure:");
			return !all_is_well;
		}
		if (msg == NULL) {
			crm_err("No message this time");
			continue;
		}

		lpc++;
		new_input = new_ipc_msg_input(msg);
		msg->msg_done(msg);
		crm_log_message(LOG_MSG, new_input->msg);

		sys_to = cl_get_string(new_input->msg, F_CRM_SYS_TO);
		task   = cl_get_string(new_input->msg, F_CRM_TASK);

		if(safe_str_eq(task, CRM_OP_HELLO)) {
			process = TRUE;

		} else if(sys_to == NULL) {
			crm_err("Value of %s was NULL!!", F_CRM_SYS_TO);
			
		} else if(task == NULL) {
			crm_err("Value of %s was NULL!!", F_CRM_TASK);
			
		} else {
			process = TRUE;
		}

		if(process){
			gboolean (*process_function)
				(HA_Message *msg, crm_data_t *data, IPC_Channel *sender) = NULL;
			process_function = user_data;
#ifdef MSG_LOG
			crm_log_message_adv(
				LOG_MSG, NULL, new_input->msg);
#endif
			if(FALSE == process_function(
				   new_input->msg, new_input->xml, sender)) {
				crm_warn("Received a message destined for %s"
					 " by mistake", sys_to);
			}
		} else {
#ifdef MSG_LOG
			crm_log_message_adv(
				LOG_ERR, NULL, new_input->msg);
#endif
		}
		
		delete_ha_msg_input(new_input);
		msg = NULL;
	}

	/* clean up after a break */
	if(msg != NULL)
		msg->msg_done(msg);

	crm_verbose("Processed %d messages", lpc);
	if (sender->ch_status == IPC_DISCONNECT) {
		crm_err("The server has left us: Shutting down...NOW");

		exit(1); /* shutdown properly later */
		
		return !all_is_well;
	}
	return all_is_well;
}

