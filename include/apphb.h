#ifndef _APPHB_H
#define _APPHB_H

/*
 * Copyright (C) 2002 Alan Robertson <alanr@unix.sh>
 * This software licensed under the GNU LGPL.
 *
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
 * Application Heartbeat API
 *
 * Application heartbeating declares an expectation between a client
 * application and a heartbeat monitoring service.  The heartbeat
 * monitoring service is used to monitor the basic sanity of
 * participating applications.
 *
 * To register with the monitoring service, use apphb_register().
 *
 * Once an application has registered, it is expected that it
 * will make periodic calls to apphb_hb().  If it does not, that
 * fact will be logged by the heartbeat monitoring service.
 *
 * To tell the monitoring service how often to expect apphb_hb(),
 * calls, use apphb_setinterval().
 *
 * To tell the monitoring service not to expect further apphb_hb()
 * calls, use apphb_unregister().
 *
 ****************************************************************
 *
 * Each of these functions returns a negative value on error
 * and sets errno to an appropriate value.
 *
 * Success is indicated by a non-negative return value.
 */

/*
 * apphb_register: register a process for heartbeat monitoring.
 *
 * parameters:
 *   appname: name this process is registered as (for notification purposes)
 *
 * The heartbeat interval for the current process is initially defaulted
 * to 10 seconds (10000 ms).
 *
 * NOTE: apphb_register() calls are not inherited by child processes.
 *	child processes must register themselves.
 *
 * errno values:
 * EEXIST:	 current process already registered for monitoring.
 * EBADF:	 application heartbeat service not available
 * EINVAL:	 NULL 'appname' argument
 * ENOSPC:	 too many clients already registered
 * ENAMETOOLONG: appname or appinstance argument is too long.
 */
int apphb_register(const char * appname, const char * appinstance);

/*
 * apphb_unregister: unregister a process from heartbeat monitoring.
 *
 * After this call, no further heartbeat calls are expected or allowed
 * from the current process, unless it reregisters.
 *
 * errno values:
 * EBADF:	application heartbeat service not available
 * ESRCH:	current process not registered for monitoring.
 */
int apphb_unregister(void);

/*
 * apphb_setinterval: set heartbeat interval
 * parameters:
 *   hbms: the expected heartbeat interval in milliseconds.
 *		an hbms of zero temporarily diables heartbeat monitoring
 *
 * errno values:
 * EBADF:	application heartbeat service not available
 * ESRCH:	current process not registered for monitoring.
 * EINVAL:	illegal/invalid hbms value
 *
 */
int apphb_setinterval(unsigned long hbms);

/*
 * apphb_setwarn: set heartbeat warning time
 * parameters:
 *   hbms: the heartbeat warning time in milliseconds
 *		an hbms of zero temporarily diables heartbeat monitoring
 *
 * errno values:
 * EBADF:	application heartbeat service not available
 * ESRCH:	current process not registered for monitoring.
 * EINVAL:	illegal/invalid hbms value
 *
 *
 */
int apphb_setwarn(unsigned long hbms);

/*
 * apphb_setreboot: set auto-reboot on failure
 * 		When a process which has autoreboot enabled
 * 		exits prematurely doesn't heartbeat, the OS
 * 		is immediately rebooted.
 * parameters:
 *   truefalse: set to a non-zero value to enable auto-reboot,
 *   		zero to disable auto-reboot for this process.
 *
 * errno values:
 * EBADF:	application heartbeat service not available
 * ESRCH:	current process not registered for monitoring.
 * EPERM:	no permission to set this machine to auto-reboot
 * 		on failure.
 */
int apphb_setreboot(unsigned int truefalse);

/*
 * apphb_hb: application heartbeat call.
 * 
 * errno values:
 * EBADF:	application heartbeat service not available
 * ESRCH:	current process not registered for monitoring.
 *
 * If a registered application does not call apphb_hb() frequently
 * enough, then when the heartbeat falls out of spec, the
 * event is logged.  Each time it resumes heartbeating afterwards,
 * this resumption is also logged.
 *
 * It is expected that there is a process somewhere watching these events,
 * and taking recovery actions if an application goes away or
 * fails to heartbeat either for too long, or heartbeats intermittently
 * too often.  This application is outside the scope of this API, but
 * in spite of this, recovery is really the whole point of application
 * heartbeating ;-)
 */
int apphb_hb(void);
#endif
