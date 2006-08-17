/*
 * ha_mgmt_client.h: header file for ha_mgmt_client.c
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
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

#ifndef _HA_MGMT_CLIENT_H
#define _HA_MGMT_CLIENT_H

char*	wait_for_events(void);
void	clLog(int priority, const char* logs);

void	start_heartbeat(const char* node);
void	stop_heartbeat(const char* node);

char*	process_cmnd_native(const char* cmd);
char*	process_cmnd_external(const char *cmd);
char*	process_cmnd_eventd(const char* cmd);

void	init_logger(const char *entity);

#endif
