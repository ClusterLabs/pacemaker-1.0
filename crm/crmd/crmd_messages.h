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
#ifndef XML_CRM_MESSAGES__H
#define XML_CRM_MESSAGES__H


#include <crm/crm.h>
#include <crm/common/ipc.h>
#include <crm/common/xml.h>
#include <crmd_fsa.h>

extern void *fsa_typed_data_adv(
	fsa_data_t *fsa_data, enum fsa_data_type a_type, const char *caller);

#define fsa_typed_data(x) fsa_typed_data_adv(msg_data, x, __FUNCTION__)

extern void register_fsa_error_adv(
	enum crmd_fsa_cause cause, enum crmd_fsa_input input,
	fsa_data_t *cur_data, void *new_data, const char *raised_from);

#define register_fsa_error(cause, input, new_data) register_fsa_error_adv(cause, input, msg_data, new_data, __FUNCTION__)

extern void register_fsa_input_adv(
	enum crmd_fsa_cause cause, enum crmd_fsa_input input,
	void *data, long long with_actions,
	gboolean after, const char *raised_from);

extern void fsa_dump_queue(int log_level);

#define crmd_fsa_stall() register_fsa_input_adv(msg_data->fsa_cause, I_WAIT_FOR_EVENT, msg_data->data, action, FALSE, __FUNCTION__)

#define register_fsa_input(cause, input, data) register_fsa_input_adv(cause, input, data, A_NOTHING, FALSE, __FUNCTION__)

#define register_fsa_input_before(cause, input, data) register_fsa_input_adv(cause, input, data, A_NOTHING, FALSE, __FUNCTION__)

#define register_fsa_input_later(cause, input, data) register_fsa_input_adv(cause, input, data, A_NOTHING, TRUE, __FUNCTION__)

#define register_fsa_input_w_actions(cause, input, data, actions) register_fsa_input_adv(cause, input, data, actions, FALSE, __FUNCTION__)

void delete_fsa_input(fsa_data_t *fsa_data);

GListPtr put_message(fsa_data_t *new_message);
fsa_data_t *get_message(void);
gboolean is_message(void);
gboolean have_wait_message(void);

extern gboolean relay_message(HA_Message *relay_message, gboolean originated_locally);

extern void crmd_ha_msg_callback(const HA_Message * msg, void* private_data);

extern gboolean crmd_ipc_msg_callback(IPC_Channel *client, gpointer user_data);

extern void process_message(
	HA_Message *msg, gboolean originated_locally, const char *src_node_name);

extern gboolean crm_dc_process_message(crm_data_t *whole_message,
				       crm_data_t *action,
				       const char *host_from,
				       const char *sys_from,
				       const char *sys_to,
				       const char *op,
				       gboolean dc_mode);

extern gboolean send_msg_via_ha(ll_cluster_t *hb_fd, HA_Message *msg);
extern gboolean send_msg_via_ipc(HA_Message *msg, const char *sys);

extern gboolean add_pending_outgoing_reply(const char *originating_node_name,
					   const char *crm_msg_reference,
					   const char *sys_to,
					   const char *sys_from);

extern gboolean crmd_authorize_message(
	ha_msg_input_t *client_msg, crmd_client_t *curr_client);

extern gboolean send_request(HA_Message *msg, char **msg_reference);

extern enum crmd_fsa_input handle_message(ha_msg_input_t *stored_msg);

extern gboolean send_ha_reply(ll_cluster_t *hb_cluster,
			      crm_data_t *xml_request,
			      crm_data_t *xml_response_data);

extern void lrm_op_callback(lrm_op_t* op);

#endif
