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
#include <portability.h>
#include <heartbeat.h>

#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>

#include <crmd_fsa.h>
#include <crmd_messages.h>

#include <crm/dmalloc_wrapper.h>

/*	A_LOG, A_WARN, A_ERROR	*/
enum crmd_fsa_input
do_log(long long action,
       enum crmd_fsa_cause cause,
       enum crmd_fsa_state cur_state,
       enum crmd_fsa_input current_input,
       fsa_data_t *msg_data)
{
	unsigned log_type = LOG_DEBUG_3;	

	if(action & A_LOG) {
		log_type = LOG_DEBUG_2;
	} else if(action & A_WARN) {
		log_type = LOG_WARNING;
	} else if(action & A_ERROR) {
		log_type = LOG_ERR;
	}
	
	crm_log_maybe(log_type,
		   "[[FSA]] Input %s from %s() received in state (%s)",
		   fsa_input2string(msg_data->fsa_input),
		   msg_data->origin,
		   fsa_state2string(cur_state));
	
	if(msg_data->data_type == fsa_dt_ha_msg) {
		ha_msg_input_t *input = fsa_typed_data(msg_data->data_type);
		if(log_type > LOG_DEBUG) {
			crm_log_message(log_type, input->msg);
		}
		
	} else if(msg_data->data_type == fsa_dt_xml) {
		crm_data_t *input = fsa_typed_data(msg_data->data_type);
		if(crm_log_level >= log_type) {
			print_xml_formatted(
				log_type,  __FUNCTION__, input, NULL);
		}

	} else if(msg_data->data_type == fsa_dt_lrm) {
		lrm_op_t *input = fsa_typed_data(msg_data->data_type);
		crm_log_maybe(log_type, 
			   "Resource %s: Call ID %d returned %d (%d)."
			   "  New status if rc=0: %s",
			   input->rsc_id, input->call_id, input->rc,
			   input->op_status, (char*)input->user_data);
		
	} else if(msg_data->data_type == fsa_dt_ccm) {
		struct crmd_ccm_data_s *input = fsa_typed_data(
			msg_data->data_type);
		int event = input->event;
		crm_log_maybe(log_type,
			   "Received \"%s\" event from the CCM.",
			   ccm_event_name(event));
	}
	
	
	return I_NULL;
}

