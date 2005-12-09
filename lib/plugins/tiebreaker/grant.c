 /* classic.c: tiebreaker module 
 *		this module always grant the quorum
 *
 * Copyright (C) 2005 Guochun Shi <gshi@ncsa.uiuc.edu>
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
#include <pils/plugin.h>
#include <clplumbing/cl_log.h>
#include <string.h>
#include <clplumbing/cl_tiebreaker.h>


#define PIL_PLUGINTYPE          HB_TIEBREAKER_TYPE
#define PIL_PLUGINTYPE_S        HB_TIEBREAKER_TYPE_S
#define PIL_PLUGIN              grant
#define PIL_PLUGIN_S            "grant"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL

static struct hb_tiebreaker_fns grantOps;

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
     
static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static struct hb_media_imports*	OurImports;
static void*			interfprivate;

#define LOG	PluginImports->log
#define MALLOC	PluginImports->alloc
#define STRDUP  PluginImports->mstrdup
#define FREE	PluginImports->mfree

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports);

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports)
{
	/* Force the compiler to do a little type checking */
	(void)(PILPluginInitFun)PIL_PLUGIN_INIT;

	PluginImports = imports;
	OurPlugin = us;
	
	/* Register ourself as a plugin */
	imports->register_plugin(us, &OurPIExports);  

	/*  Register our interface implementation */
 	return imports->register_interface(us, PIL_PLUGINTYPE_S,
					   PIL_PLUGIN_S,
					   &grantOps,
					   NULL,
					   &OurInterface,
					   (void*)&OurImports,
					   interfprivate); 
}

static int
grant_break_tie(int member_count, int total_count)
{
	return TRUE;
}

static struct hb_tiebreaker_fns grantOps ={
	grant_break_tie
};
