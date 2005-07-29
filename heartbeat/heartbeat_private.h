/* $Id: heartbeat_private.h,v 1.16 2005/07/29 07:03:47 sunjd Exp $ */
/*
 * heartbeat_private.h: definitions for the Linux-HA heartbeat program
 * that are defined in heartbeat.c and are used by other .c files
 * that are only compiled into the heartbeat binary
 *
 * I evisage that eventually these funtions will be broken out
 * of heartbeat.c and that this heartbeat_private.h will no longer
 * be neccessary.
 *
 * Copyright (C) 2002 Horms <horms@verge.net.au>
 *
 * This file created from heartbeat.c
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _HEARTBEAT_PRIVATE_H
#define _HEARTBEAT_PRIVATE_H

#include <heartbeat.h>
#include <ha_msg.h>
#include <glib.h>

#include <clplumbing/longclock.h>
#include <clplumbing/proctrack.h>
#include <hb_proc.h>

enum comm_state {
	COMM_STARTING,
	COMM_LINKSUP
};

extern const char *	cmdname;
extern int		nice_failback;
extern int		WeAreRestarting;
extern int		shutdown_in_progress;
extern longclock_t	local_takeover_time;
extern enum comm_state	heartbeat_comm_state;

/* Used by signal handlers */
void hb_init_watchdog(void);
void hb_tickle_watchdog(void);
void hb_close_watchdog(void);

/* Used to register with heartbeat for receiving messages directly */
typedef void (*HBmsgcallback) (const char * type, struct node_info* fromnode
,	TIME_T msgtime, seqno_t seqno, const char * iface, struct ha_msg * msg);
void hb_register_msg_callback(const char * msgtype, HBmsgcallback callback);
void hb_register_comm_up_callback(void(*callback)(void));


int  hb_send_resources_held(int stable, const char * comment);
void hb_setup_child(void);
void init_resource_module(void);

gboolean hb_send_local_status(gpointer p);
gboolean hb_dump_all_proc_stats(gpointer p);
void	heartbeat_monitor(struct ha_msg * msg, int status, const char * iface);

void hb_emergency_shutdown(void);
void hb_initiate_shutdown(int quickshutdown);

void hb_versioninfo(void);
void hb_dump_proc_stats(volatile struct process_info * proc);
void hb_trigger_restart(int quickrestart);

void hb_giveup_resources(void);
void hb_kill_tracked_process(ProcTrack* p, void * data);
gboolean hb_mcp_final_shutdown(gpointer p);

struct ha_msg * add_control_msg_fields(struct ha_msg* ret);
#endif /* _HEARTBEAT_PRIVATE_H */
