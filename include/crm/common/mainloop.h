/* 
 * Copyright (C) 2009 Andrew Beekhof <andrew@beekhof.net>
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
#ifndef CRM_COMMON_MAINLOOP__H
#define CRM_COMMON_MAINLOOP__H

#include <glib.h>

typedef struct trigger_s 
{
	GSource source;
	gboolean trigger;
	void *user_data;
	guint id;

} crm_trigger_t;

extern crm_trigger_t *mainloop_add_trigger(
    int priority, gboolean (*dispatch)(gpointer user_data), gpointer userdata);

extern void mainloop_set_trigger(crm_trigger_t* source);

extern gboolean mainloop_destroy_trigger(crm_trigger_t* source);

extern gboolean crm_signal(int sig, void (*dispatch)(int sig));

extern gboolean mainloop_add_signal(int sig, void (*dispatch)(int sig));

extern gboolean mainloop_destroy_signal(int sig);

#endif
