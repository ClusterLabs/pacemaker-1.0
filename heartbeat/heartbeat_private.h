/* $Id: heartbeat_private.h,v 1.8 2004/05/15 09:28:09 andrew Exp $ */
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
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _HEARTBEAT_PRIVATE_H
#define _HEARTBEAT_PRIVATE_H

#include <ha_msg.h>
#include <glib.h>

#include <clplumbing/proctrack.h>
#include <hb_proc.h>

extern const char *	cmdname;
extern int		nice_failback;
extern int		WeAreRestarting;
extern int		shutdown_in_progress;
extern longclock_t	local_takeover_time;

/* Used by signal handlers */
void hb_init_watchdog(void);
void hb_tickle_watchdog(void);
void hb_close_watchdog(void);

int  hb_send_resources_held(int stable, const char * comment);

gboolean hb_mcp_final_shutdown(gpointer p);
gboolean hb_send_local_status(gpointer p);
gboolean hb_dump_all_proc_stats(gpointer p);
void	heartbeat_monitor(struct ha_msg * msg, int status, const char * iface);

void hb_emergency_shutdown(void);
void hb_initiate_shutdown(int quickshutdown);

void hb_versioninfo(void);
void hb_dump_proc_stats(volatile struct process_info * proc);
void hb_trigger_restart(int quickrestart);

#ifndef WITH_CRM
void hb_giveup_resources(void);
#endif
void hb_kill_tracked_process(ProcTrack* p, void * data);

struct ha_msg * add_control_msg_fields(struct ha_msg* ret);
#endif /* _HEARTBEAT_PRIVATE_H */
