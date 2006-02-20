/* $Id: tengine.h,v 1.35 2006/02/20 16:21:51 andrew Exp $ */
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

#include <crm/transition.h>
#include <clplumbing/ipc.h>
#include <fencing/stonithd_api.h>

extern IPC_Channel *crm_ch;
extern GMainLoop*  mainloop;

/* tengine */
extern crm_action_t *match_down_event(
	int rc, const char *target, const char *filter);
extern void send_stonith_update(stonith_ops_t * op);

extern gboolean cib_action_update(crm_action_t *action, int status);

/* utils */
extern gboolean stop_te_timer(crm_action_timer_t *timer);
extern gboolean start_te_timer(crm_action_timer_t *timer);
extern const char *get_rsc_state(const char *task, op_status_t status);

/* unpack */
extern gboolean extract_event(crm_data_t *msg);
extern gboolean process_te_message(
	HA_Message * msg, crm_data_t *xml_data, IPC_Channel *sender);

extern crm_graph_t *transition_graph;
extern GTRIGSource *transition_trigger;

extern char *te_uuid;
extern cib_t *te_cib_conn;

extern void notify_crmd(crm_graph_t *graph);

#include <te_callbacks.h>

extern void trigger_graph_processing(const char *fn, int line);
extern void abort_transition_graph(
	int abort_priority, enum transition_action abort_action,
	const char *abort_text, crm_data_t *reason, const char *fn, int line);

#define trigger_graph()	trigger_graph_processing(__FUNCTION__, __LINE__)
#define abort_transition(pri, action, text, reason)			\
	abort_transition_graph(pri, action, text, reason,__FUNCTION__,__LINE__);

#endif

