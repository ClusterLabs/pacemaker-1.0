static const char _module_c_Id [] = "$Id: module.c,v 1.53 2003/07/01 02:36:22 alan Exp $";
/*
 * module: Dynamic module support code
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 * Copyright (C) 2000 Marcelo Tosatti <marcelo@conectiva.com.br>
 * 
 * Thanks to Conectiva S.A. for sponsoring Marcelo Tosatti work
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

#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#define index FOOindex
#define time FOOtime
#include <glib.h>
#undef index
#undef time
#include <unistd.h>
#include <dirent.h>
#include <ltdl.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <hb_module.h>
#include <hb_signal.h>
#include <pils/generic.h>
#include <HBcomm.h>
#include <hb_config.h>
#include "lock.h"

#ifndef RTLD_NOW
#	define RTLD_NOW 0
#endif

extern struct hb_media_fns** hbmedia_types;
extern int num_hb_media_types;


PILPluginUniv*		PluginLoadingSystem = NULL;
GHashTable*		AuthFunctions = NULL;
GHashTable*		CommFunctions = NULL;
GHashTable*		StonithFuncs = NULL;
static GHashTable*	Parameters = NULL;

static void		RegisterNewMedium(struct hb_media* mp);
const char *	GetParameterValue(const char * name);
static void		RegisterCleanup(void(*)(void));
struct hb_media_imports	CommImports =
{	GetParameterValue	/* So plugins can get option values */
,	RegisterNewMedium
,	ttylock
,	ttyunlock
,	StringToBaud
,	RegisterCleanup
,	hb_signal_process_pending
};

extern struct hb_media* sysmedia[];
extern int              nummedia;

static PILGenericIfMgmtRqst RegistrationRqsts [] =
{	{"HBauth",	&AuthFunctions,	NULL,		NULL, NULL}
,	{"HBcomm",	&CommFunctions,	&CommImports,	NULL, NULL}
,	{"stonith",	&StonithFuncs,	NULL,		NULL, NULL}
,	{NULL,		NULL,		NULL,		NULL, NULL}
};


int 
module_init(void)
{ 
	static int initialised = 0;

#if 0
	int errors = 0;
#endif
	PIL_rc	rc;

	(void)_module_c_Id;
	(void)_heartbeat_h_Id;
	(void)_hb_signal_h_Id;
	(void)_hb_config_h_Id;
	(void)_ha_msg_h_Id;

	/* Perform the init only once */
	if (initialised) {
		return HA_FAIL;
	}
	/* Initialize libltdl's list of preloaded modules */
	LTDL_SET_PRELOADED_SYMBOLS();

#if 0
	/* Initialize ltdl */
	if ((errors = lt_dlinit())) {
		return HA_FAIL;
	}
#endif

	if ((PluginLoadingSystem = NewPILPluginUniv(HA_PLUGIN_D))
	==	NULL) {
    		return(HA_FAIL);
	}

	if (DEBUGDETAILS) {
 		PILSetDebugLevel(PluginLoadingSystem, NULL, NULL, debug);
	}


	if ((rc = PILLoadPlugin(PluginLoadingSystem, "InterfaceMgr", "generic"
	,	&RegistrationRqsts)) != PIL_OK) {
	
		ha_log(LOG_ERR
		,	"ERROR: cannot load generic interface manager plugin"
	       " [%s/%s]: %s"
	      	,	"InterfaceMgr", "generic"
		,	PIL_strerror(rc));
		return HA_FAIL;
	}
 	PILSetDebugLevel(PluginLoadingSystem, NULL, NULL, debug);

	/* init completed */
	++initialised;

	return HA_OK;
}

static void
RegisterNewMedium(struct hb_media* mp)
{
 
	sysmedia[nummedia] = mp;
	++nummedia;
}


/* 
 * SetParameterValue() records a class of options given in the configuration
 * file so they can be passed to the plugins their use. This avoids coupling
 * through global variables which is problematic for plugins on some platforms.
 */
void
SetParameterValue(const char * name, const char * value)
{
	char *	namedup;
	char *	valdup;
	void *	gname;
	void *	gval;

	if (Parameters == NULL) {
		Parameters = g_hash_table_new(g_str_hash, g_str_equal);
		if (Parameters == NULL) {
			ha_log(LOG_ERR
			,	"ERROR: cannot create parameter table");
			return;
		}
	}
	if (g_hash_table_lookup_extended(Parameters, name, &gname
	,	&gval)) {
		g_hash_table_remove(Parameters, name);
		g_free(gval);
		g_free(gname);
	}
	namedup = g_strdup(name);
	valdup = g_strdup(value);
	g_hash_table_insert(Parameters, namedup, valdup);
}
/* 
 * GetParameterValue() provides information from the configuration file
 * for the plugins to use. This avoids coupling through global variables.
 */
const char *
GetParameterValue(const char * name)
{
	if (!Parameters) {
		return NULL;
	}
	return g_hash_table_lookup(Parameters, name);
}
static void
RegisterCleanup(void(*fun)(void))
{
	localdie = fun;
}
