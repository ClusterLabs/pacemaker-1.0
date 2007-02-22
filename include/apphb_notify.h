/*
 * Author: Alan Robertson <alanr@unix.sh>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _APPHB_NOTIFY_H
#	define _APPHB_NOTIFY_H
/*
 * Definitions for apphb plugins.
 */

typedef struct AppHBNotifyOps_s AppHBNotifyOps;
typedef struct AppHBNotifyImports_s AppHBNotifyImports;

/*
 * Apphb event types
 */
enum apphb_event {
	APPHB_HUP	= 1,	/* Hangup w/o unregister */
	APPHB_NOHB	= 2,	/* Failed to heartbeat as requested */
	APPHB_HBAGAIN	= 3,	/* Heartbeating restarted */
	APPHB_HBWARN	= 4,	/* Heartbeat outside warning interval */
	APPHB_HBUNREG	= 5	/* Application unregistered */
};
typedef enum apphb_event apphb_event_t;

/*
 * Plugin exported functions.
 */
struct AppHBNotifyOps_s {
	int (*cregister)(pid_t pid, const char * appname, const char * appinst
	,	const char * curdir, uid_t uid, gid_t gid, void * handle);
	int (*status)(const char * appname, const char * appinst
	,	const char * curdir, pid_t pid, uid_t uid, gid_t gid 
	,	apphb_event_t event);
};

/*
 * Plugin imported functions.
 */
struct AppHBNotifyImports_s {
	/* Boolean return value */
	int (*auth)	(void * clienthandle
,	uid_t * uidlist, gid_t* gidlist, int nuid, int ngid);
};

#define APPHB_NOTIFY    AppHBNotification
#define APPHB_NOTIFY_S  "AppHBNotification"

#endif
