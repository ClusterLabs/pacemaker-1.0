/* $Id: tengine.h,v 1.17 2005/03/11 14:25:07 andrew Exp $ */
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
#ifndef TENGINE__H
#define TENGINE__H

#include <clplumbing/ipc.h>
#include <fencing/stonithd_api.h>

extern IPC_Channel *crm_ch;
extern GListPtr graph;
extern GMainLoop*  mainloop;
extern gboolean in_transition;

typedef enum {
	action_type_pseudo,
	action_type_rsc,
	action_type_crm
} action_type_e;

typedef struct synapse_s {
		int id;
		gboolean triggers_complete;
		gboolean complete;
		gboolean confirmed;
		GListPtr actions; /* action_t* */
		GListPtr inputs;  /* action_t* */
} synapse_t;

typedef struct te_timer_s te_timer_t;

typedef struct action_s {
		int id;
		int timeout;
		te_timer_t *timer;

		action_type_e type;

		gboolean invoked;
		gboolean complete;
		gboolean can_fail;
		
		crm_data_t *xml;
		
} action_t;


enum timer_reason {
	timeout_action,
	timeout_timeout,
	timeout_fuzz
};

struct te_timer_s
{
	int source_id;
	int timeout;
	enum timer_reason reason;
	action_t *action;

};

/* tengine */
extern gboolean initialize_graph(void);
extern gboolean process_graph_event(crm_data_t *event);
/*	const char *event_node,   const char *event_rsc, const char *rsc_state,
 *	const char *event_action, const char *event_rc, const char *op_status); */
extern int match_graph_event(action_t *action, crm_data_t *event);
extern int match_down_event(const char *target, const char *filter, int rc);

extern gboolean initiate_transition(void);

/* utils */
extern void print_state(int log_level);
extern void send_success(const char *text);
/*extern void send_abort(const char *text, HA_Message *msg); */
extern void send_abort(const char *text, crm_data_t *msg);
extern gboolean stop_te_timer(te_timer_t *timer);
extern gboolean start_te_timer(te_timer_t *timer);
extern gboolean do_update_cib(crm_data_t *xml_action, int status);

/* unpack */
extern gboolean unpack_graph(crm_data_t *xml_graph);
extern gboolean extract_event(crm_data_t *msg);
extern gboolean process_te_message(
	HA_Message * msg, crm_data_t *xml_data, IPC_Channel *sender);

extern uint transition_timeout;
extern uint transition_fuzz_timeout;
extern uint default_transition_timeout;

extern te_timer_t *transition_timer;
extern te_timer_t *transition_fuzz_timer;
extern cib_t *te_cib_conn;

extern const char *actiontype2text(action_type_e type);

extern void tengine_stonith_callback(stonith_ops_t * op, void * private_data);
extern void tengine_stonith_connection_destroy(gpointer user_data);
extern gboolean tengine_stonith_dispatch(IPC_Channel *sender, void *user_data);
extern void check_for_completion(void);
void process_trigger(int action_id);

#endif


