/*
 * 
 * Copyright (c) 2002 Intel Corporation 
 * This code is based on the apphbd library code
 * which is Copyright (c) 2002 Alan Robertson
 * 
 * This software licensed under the GNU LGPL.
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
 ****************************************************************
 * Recover Manager API
 * 
 * Success is indicated by a non-negative return value.
 */

#ifndef _RECOVERYMGR_H
#define _RECOVERYMGR_H

#include <apphb_notify.h>

/**
 * Registers a recovery manager with apphbd
 *
 * parameters:
 * @param appname name this process that registered
 * @param appinstance 
 * @param callback The function to be called for every client event
 *
 * @return zero or an errno value as follows:
 * EEXIST:	 current process already connected to recovery manager
 * EBADF:	 service not available
 * EINVAL:	 NULL 'appname' argument
 * ENOSPC:	 too many connections
 * ENAMETOOLONG: appname or appinstance argument is too long.
 */
int recoverymgr_connect(const char * appname, 
			const char * appinstance);

/**
 * Unregister a recovery manager from the apphbd
 * After this call, the client will no longer receive
 * notifications regarding events from monitored processes.
 *
 * @return zero or an errno value as follows:
 * EBADF:	recovery manager service not available
 * ESRCH:	current process not connected to recovery manager
 */
int recoverymgr_disconnect(void);

/**
 * For each event you want to have passed to the
 * recovery manager, this function should be called.
 *
 * @param client This is the information about the
 *               process that caused the apphbd to
 *               generate an event. It also 
 *               contains the event information.
 *
 * @return zero or an errno value as follows:
 * @TODO add errno values
 */
int recoverymgr_send_event(const char *appname, const char *appinst,
		pid_t pid, uid_t uid, gid_t gid,
		apphb_event_t event);


#endif /* _RECOVERYMGR_H */

