/*
 * recmgr.c: Recovery manager client plug-in implementation
 * 
 * Copyright 2002 Intel Corporation 
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
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
 *
 * This plugin is intended to provide an interface to the
 * apphbd so that when an application misses a heartbeat an 
 * external executable can be notified.  This external component
 * can then fork a process to restart the application, or
 * preform other actions when an application misses a heartbeat
 * (or otherwise causes the apphbd to generate an event for it).
 *
 * The plugin permits the recovery component to be separate
 * from the apphbd.  Thus the forking of processes and recovery
 * is done outside the apphbd.  The recovery component will
 * need to be registered with the apphbd like any other 
 * application. However, it should be recovered differently.
 *
 * We will probably need a special case in apphb to handle
 * recovery of the recovery manager.  In this implementation
 * the plugin (i.e. apphbd) is a client of the recovery 
 * manager) thus, if the recovery manager dies, the 
 * plugin can no longer forward recovery requests to it
 * until *something* restarts it.
 *
 */

#include <lha_internal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_signal.h>

#define PIL_PLUGINTYPE          APPHB_NOTIFY
#define PIL_PLUGINTYPE_S        APPHB_NOTIFY_S
#define PIL_PLUGIN              recmgr
#define PIL_PLUGIN_S	        "recmgr"
#define PIL_PLUGINLICENSE       LICENSE_LGPL
#define PIL_PLUGINLICENSEURL    URL_LGPL

#define DBGMIN		1
#define DBGDETAIL	7
int	debug = 0;

/*
 * Make this plugin the manager for AppHBNotification
 * plugin types 
 */
/* #define ENABLE_PLUGIN_MANAGER_PRIVATE */

#include <pils/interface.h>
#include <apphb.h>
#include <apphb_notify.h>

#include <recoverymgr.h>
#include <clplumbing/recoverymgr_cs.h>
#include <clplumbing/GSource.h>
#include <clplumbing/ipc.h>

/* 
 * use the built-in definitions for PILPluginOps 
 */
PIL_PLUGIN_BOILERPLATE2("1.0", Debug)

/* 
 * Locations to store info aquired during registration 
 */
static const PILPluginImports* 	PluginImports;
static PILPlugin*              	OurPlugin;
static PILInterface*           	OurInterface;
static void*                   	OurImports;
static void*                   	interfprivate;

/* 
 * notification plugin operations
 */
static int our_cregister(pid_t pid, const char *appname, const char *appinst,
		const char * curdir, uid_t uid, gid_t gid, void *handle);

static int our_status(const char *appname, const char *appinst,
		const char * curdir, pid_t pid, uid_t uid, gid_t gid, 
		apphb_event_t event);

static struct AppHBNotifyOps_s  recmgrops = {
	our_cregister
, 	our_status	
};

/** plug-in specific structures */
typedef struct recoverymgr_client recoverymgr_client_t;

/*
 * prototypes for internal functions
 */


/** 
 * If there was any global data allocated, or file 
 * descriptors opened, etc. clean it up here.
 * 
 * @todo Need to destroy the wait for connection object 
 */
static PIL_rc recmgrcloseintf(PILInterface* pi, void* pd)
{
	recoverymgr_disconnect();

	return PIL_OK;
}

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports, void *);

/**
 *  
 * Our plugin initialization and registration function
 * It gets called when the plugin gets loaded.
 */
PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports, void *user_ptr)
{
	PIL_rc			ret;
	/*PILPluginInitFun   	fun = &PIL_PLUGIN_INIT; (void)fun; */

	PluginImports = imports;
	OurPlugin = us;

	/* Register as a plugin */
	imports->register_plugin(us, &OurPIExports);

	/* Register interfaces */
	ret = imports->register_interface
	(	us
	,	PIL_PLUGINTYPE_S
	,	PIL_PLUGIN_S
	, 	&recmgrops
	,	recmgrcloseintf
	, 	&OurInterface
	, 	(void**)&OurImports
	, 	interfprivate); /*TODO: do we need this?? */


	return ret;

}


/**
 *  This function is called for every process that
 *  registers with apphbd.  It is currently not 
 *  used in the plugin.
 */
static int our_cregister(pid_t pid, const char *appname, const char *appinst,
		const char * curdir, uid_t uid, gid_t gid, void *handle)
{

	return 0;
}

/**
 * This function is called on every event.
 * We must forward the message to the recovery manager
 * to let it determine how to handle it.
 *
 * @return zero on success, non-zero on error
 */
static int our_status(const char *appname, const char *appinst,
		const char * curdir, pid_t pid, uid_t uid, gid_t gid,
		apphb_event_t event)
{
	int rc;

	/* attempt to connect to the recovery manager
	 * We may have already connected, if so, this
	 * will return an error of EEXIST.
	 * This is done in case the recovery manager dies 
	 * and is restarted.  This way we can make sure
	 * we are connected.
	 */
	rc = recoverymgr_connect("apphbd","recmgr-plugin");
	if (rc == EEXIST) 
	{
		rc = 0;
		errno = 0;	
	}
	
	if (rc != 0)
	{
		cl_perror("failed to connect to recovery manager");
		return 1;	
	}

	errno = 0;

	/* we are connected, so send the message */
	rc = recoverymgr_send_event(appname, appinst,
			pid, uid, gid, event);

	if (rc != 0)
	{
	   cl_perror("error sending message to recovery manager daemon");
	}

	return 0;
}



