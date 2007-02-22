/*
 * hb_signal.h: signal handling routines to be used by Heartbeat
 *
 * Copyright (C) 2002 Horms <horms@verge.net.au>
 *
 * Derived from code in heartbeat.c in this tree
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
 */

#ifndef _HB_SIGNAL_H
#define _HB_SIGNAL_H

#include <clplumbing/cl_signal.h>

void hb_signal_signal_all(int sig);

void hb_signal_reaper_handler(int sig);

void hb_signal_reaper_action(int waitflags);

void hb_signal_term_handler(int sig);

void hb_signal_term_action(void);

void hb_signal_debug_usr1_handler(int sig);

void hb_signal_debug_usr1_action(void);

void hb_signal_debug_usr2_handler(int sig);

void hb_signal_debug_usr2_action(void);

void parent_hb_signal_debug_usr1_handler(int sig);

void parent_hb_signal_debug_usr1_action(void);

void parent_hb_signal_debug_usr2_handler(int sig);

void parent_hb_signal_debug_usr2_action(void);

void hb_signal_reread_config_handler(int sig);

void hb_signal_reread_config_action(void);

void hb_signal_false_alarm_handler(int sig);

void hb_signal_false_alarm_action(void);

void hb_signal_process_pending_set_mask_set(const sigset_t *set);

unsigned int hb_signal_pending(void);

void hb_signal_process_pending(void);

int hb_signal_set_common(sigset_t *set);

int hb_signal_set_write_child(sigset_t *set);

int hb_signal_set_read_child(sigset_t *set);

int hb_signal_set_fifo_child(sigset_t *set);

int hb_signal_set_master_control_process(sigset_t *set);

#endif /* _HB_SIGNAL_H */
