/*
 * cms_client.h: cms daemon client header
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
 * Author: Zhu Yi (yi.zhu@intel.com)
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
#ifndef __CMS_CLIENT_H__
#define __CMS_CLIENT_H__

#include <saf/ais_message.h>
#include "cms_cluster.h"
#include "cms_client_types.h"



void cms_client_close_all(GHashTable * cms_client_table);
void cms_client_input_destroy(gpointer user_data);
int cms_client_add(GHashTable ** cms_client_table,
		   struct IPC_CHANNEL * newclient);

/*
 * Naming conventions: functions prefixed with client_process_
 * are operation functions executed by local cms daemon after
 * receives a request from the client.
 */
int client_process_qstatus(IPC_Channel * client, client_header_t * msg,
			   cms_data_t * cmsdata);
int client_process_mqopen(IPC_Channel * client, client_header_t * msg,
			  cms_data_t * cmsdata);
int client_process_mqclose(IPC_Channel * client, client_header_t * msg,
			   cms_data_t * cmsdata);
int client_process_mqunlink(IPC_Channel * client, client_header_t * msg,
			    cms_data_t * cmsdata);
int client_process_mqsend(IPC_Channel * client, client_header_t * msg,
			  cms_data_t * cmsdata);
int client_process_mqgroup_insert(IPC_Channel * client, client_header_t * msg,
				  cms_data_t * cmsdata);
int client_process_mqgroup_remove(IPC_Channel * client, client_header_t * msg,
				  cms_data_t * cmsdata);
int client_process_mqgroup_track(IPC_Channel * client,	client_header_t * msg);
int client_process_mqgroup_track_stop(IPC_Channel * client,
				      client_header_t * msg);
int client_process_message_request(IPC_Channel * client, client_header_t * msg);

int client_process_mqsend_reply(IPC_Channel * client, client_header_t * msg, cms_data_t * cmsdata);


/*
 * Naming conventions: functions prefixed with client_send_ are
 * functions that send reply message to the client by local cms.
 */
int client_send_msg(IPC_Channel * client, size_t len, gpointer data);
int client_send_msg_freeclient(IPC_Channel * client, size_t len, gpointer data);
int client_send_error_msg(IPC_Channel * client, const char * name,
			  size_t type, SaErrorT error);
int client_send_qstatus(IPC_Channel * client, mqueue_t * queue, int flag);
int client_send_client_qopen(IPC_Channel * client, mqueue_request_t * request,
			     guint handle, int flag);
int client_send_ack_msg(IPC_Channel * client, mqueue_request_t * request,
			guint handle, int flag);
int client_send_notready_msg(IPC_Channel * client, client_header_t * msg);

#endif /* __CMS_CLIENT_H__ */
