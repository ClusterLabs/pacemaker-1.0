/* $Id: apphb_notify.h,v 1.4 2004/10/24 13:00:12 lge Exp $ */
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
	,	uid_t uid, gid_t gid, void * handle);
	int (*status)(const char * appname, const char * appinst, pid_t pid
	,	uid_t uid, gid_t gid, apphb_event_t event);
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
#define APPHB_NOTIFY_S  MKSTRING(AppHBNotification)

#endif
