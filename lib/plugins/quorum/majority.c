/* majority.c: quorum module
 * policy ---  if it has more than half of total number of nodes, you have the quorum
 *		if you have exactly half othe total number of nodes, you don't have the quorum
 *		otherwise you have a tie
 *
 * Copyright (C) 2005 Guochun Shi <gshi@ncsa.uiuc.edu>
 *
 * SECURITY NOTE:  It would be very easy for someone to masquerade as the
 * device that you're pinging.  If they don't know the password, all they can
 * do is echo back the packets that you're sending out, or send out old ones.
 * This does mean that if you're using such an approach, that someone could
 * make you think you have quorum when you don't during a cluster partition.
 * The danger in that seems small, but you never know ;-)
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


#include <lha_internal.h>
#include <pils/plugin.h>
#include <clplumbing/cl_log.h>
#include <string.h>
#include <clplumbing/cl_quorum.h>


#define PIL_PLUGINTYPE          HB_QUORUM_TYPE
#define PIL_PLUGINTYPE_S        HB_QUORUM_TYPE_S
#define PIL_PLUGIN              majority
#define PIL_PLUGIN_S            "majority"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL

static struct hb_quorum_fns majorityOps;

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
     
static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static struct hb_media_imports*	OurImports;
static void*			interfprivate = NULL;

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
					   &majorityOps,
					   NULL,
					   &OurInterface,
					   (void*)&OurImports,
					   interfprivate); 
}

static int
majority_getquorum(const char* cluster
,		int member_count, int member_quorum_votes
,		int total_node_count, int total_quorum_votes)
{
	cl_log(LOG_DEBUG, "quorum plugin: majority");
 	cl_log(LOG_DEBUG, "cluster:%s, member_count=%d, member_quorum_votes=%d", 
 	       cluster, member_count, member_quorum_votes);  
 	cl_log(LOG_DEBUG, "total_node_count=%d, total_quorum_votes=%d", 
 	       total_node_count, total_quorum_votes);  
		
 	if(member_count >=  total_node_count/2 + 1){ 
 		return QUORUM_YES; 
 	} else if ( total_node_count % 2 == 0 && member_count == total_node_count/2){
		return QUORUM_TIE;
	}
	
 	return QUORUM_NO; 
}
static int
majority_init(callback_t notify, const char* cl_name, const char* qs_name)
{
	return 0;
}
static void
majority_stop(void)
{
}
static struct hb_quorum_fns majorityOps ={
	majority_getquorum,
	majority_init,
	majority_stop
};
