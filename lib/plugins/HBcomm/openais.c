/*
 * openais.c: openais communication code for heartbeat.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <heartbeat.h>
#include <HBcomm.h>
#include <evs.h>

#define PIL_PLUGINTYPE          HB_COMM_TYPE
#define PIL_PLUGINTYPE_S        HB_COMM_TYPE_S
#define PIL_PLUGIN              openais
#define PIL_PLUGIN_S            "openais"
#define PIL_PLUGINLICENSE 	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL 	URL_LGPL
#include <pils/plugin.h>

struct ais_private {
        char *  interface;      /* Interface name */
	evs_handle_t handle;
	int	fd;
};


static struct hb_media_fns openaisOps;

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
 	return imports->register_interface(us, PIL_PLUGINTYPE_S
	,	PIL_PLUGIN_S
	,	&openaisOps
	,	NULL		/*close */
	,	&OurInterface
	,	(void*)&OurImports
	,	interfprivate); 
}

#define		ISOPENAISOBJECT(mp) ((mp) && ((mp)->vf == (void*)&openaisOps))
#define		OPENAISASSERT(mp)	g_assert(ISOPENAISOBJECT(mp))

static int 
openais_mtype(char** buffer) { 
	*buffer = STRDUP(PIL_PLUGIN_S);
	if (!*buffer) {
		return 0;
	}

	return STRLEN_CONST(PIL_PLUGIN_S);
}

static int
openais_descr(char **buffer) { 
	const char constret[] = "openais communication module";
	*buffer = STRDUP(constret);
	if (!*buffer) {
		return 0;
	}

	return STRLEN_CONST(constret);
}

static int
openais_isping(void) {
    return 0;
}

static gboolean openais_msg_ready = FALSE;
static char openais_pkt[MAXLINE];
static int openais_pktlen =0;
static void
evs_deliver_fn(struct in_addr source_addr, void* msg, 
	       int msg_len){

	if (openais_msg_ready){
		PILCallLog(LOG, PIL_CRIT, "message overwrite");
		return;
	}
	memcpy(openais_pkt, msg, msg_len);
	openais_pktlen = msg_len;
	openais_pkt[msg_len] = 0;
	openais_msg_ready = TRUE;
	return;
}

static void
evs_confchg_fn(struct in_addr *member_list, int member_list_entries,
	       struct in_addr *left_list, int left_list_entries,
	       struct in_addr *joined_list, int joined_list_entries){
	PILCallLog(LOG, PIL_INFO, "evs_confchg_fn is called");
	return;
}

static evs_callbacks_t callbacks = {
	evs_deliver_fn,
	evs_confchg_fn
};

static  struct evs_group groups[]={
	{"openais_comm"}
};

static int
openais_init(evs_handle_t* handle){
	
	if (evs_initialize(handle, &callbacks) != EVS_OK){
		PILCallLog(LOG, PIL_INFO, "evs_initialize failed");
		return HA_FAIL;
	}
	
	return(HA_OK);
}


static struct hb_media *
openais_new(const char * intf)
{
	struct ais_private*	ais;
	struct hb_media *	ret;


	ais = MALLOC(sizeof(struct ais_private));
	if (ais == NULL){
		PILCallLog(LOG, PIL_CRIT, "%s: malloc failed for ais_if",
			   __FUNCTION__);
		return NULL;
	}
	ais->interface = (char*) STRDUP(intf);
	if (ais->interface == NULL){
		PILCallLog(LOG, PIL_CRIT, "%s: STRDUP failed",
			   __FUNCTION__);
		return NULL;
		
	}

	if (openais_init(&ais->handle) != HA_OK){
		PILCallLog(LOG, PIL_CRIT, "%s: initialization failed",
			   __FUNCTION__);
	}
	
	ret = (struct hb_media*) MALLOC(sizeof(struct hb_media));
	if (ret != NULL) {
		char * name;
		memset(ret, 0, sizeof(*ret));
		ret->pd = (void*)ais;
		name = STRDUP(intf);
		if (name != NULL) {
			ret->name = name;
		} else {
			FREE(ret);
			ret = NULL;
		}
	}
	if (ret != NULL) {
		if (DEBUGPKT) {
			PILCallLog(LOG, PIL_DEBUG, 
					"openais_new: returning ret (%s)", 
					ret->name);
		}
	}else{
		FREE(ais->interface);
		FREE(ais);
		if (DEBUGPKT) {
			PILCallLog(LOG, PIL_DEBUG, "openais_new: ret was NULL");
		}
	}
	return(ret);
}

static int
openais_open(struct hb_media* mp)
{
	struct ais_private * ais;

	PILCallLog(LOG, PIL_INFO, "%s is called", __FUNCTION__);

	OPENAISASSERT(mp);
	ais = (struct ais_private *) mp->pd;
	if (evs_join(ais->handle, groups, 1) != EVS_OK){
		PILCallLog(LOG, PIL_CRIT, 
			   "%s: evs_join failed",
			   __FUNCTION__);
		return HA_FAIL;
	}
	
	if (evs_fd_get(ais->handle, &ais->fd) != EVS_OK){
		PILCallLog(LOG, PIL_CRIT, 
			   "%s: evs_fd_get failed",
			   __FUNCTION__);
		return HA_FAIL;
	}
	return(HA_OK);
}

static int
openais_close(struct hb_media* mp)
{
	struct ais_private * ais;

	PILCallLog(LOG, PIL_INFO, "%s is called", __FUNCTION__);
	OPENAISASSERT(mp);
	ais = (struct ais_private *) mp->pd;
	
	if (evs_finalize(ais->handle) != EVS_OK){
		PILCallLog(LOG,PIL_CRIT, 
			   "%s: evs_finalize failed",
			   __FUNCTION__);
		
		return HA_FAIL;
	}
	
	return HA_OK;
}


/*
 * Receive a heartbeat broadcast packet from OPENAIS interface
 */

static void *
openais_read(struct hb_media* mp, int * lenp)
{
	struct ais_private *  ais;
	struct pollfd pfd;
	
	ais= (struct ais_private *) mp->pd;

	while (!openais_msg_ready){
		pfd.fd = ais->fd;
		pfd.events = POLLIN|POLLPRI;
		pfd.revents = 0;

		OPENAISASSERT(mp);
		if (poll(&pfd, 1, -1) < 0){
			if (errno == EINTR){
				break;
			}else{
				PILCallLog(LOG, PIL_CRIT, 
					   "%s: poll failed, errno =%d", 
					   __FUNCTION__, errno);
				return NULL;
			}
		}
		
		if (pfd.revents & (POLLERR|POLLNVAL|POLLHUP)){
			PILCallLog(LOG, PIL_CRIT,
				   "%s: poll returns bad revents(%d)",
				   __FUNCTION__, pfd.revents);
			return NULL;
		}else if (!pfd.revents & POLLIN){
			PILCallLog(LOG, PIL_CRIT,
				   "%s: poll returns but no input data",
				   __FUNCTION__);
			return NULL;
		};
		
		if (evs_dispatch(ais->handle, EVS_DISPATCH_ONE) != EVS_OK){
			
			PILCallLog(LOG, PIL_CRIT, 
				   "%s: evs_dispatch() failed",
				   __FUNCTION__);		
			return NULL;
		}
	}

	if (openais_msg_ready){
		openais_msg_ready = FALSE;			
		*lenp = openais_pktlen +1;       
		return openais_pkt;
	}else{
		return NULL;
	}
}


/*
 * Send a heartbeat packet over openais interface
 */

static int
openais_write(struct hb_media* mp, void *pkt, int len)
{
	struct ais_private *   ais;
	struct iovec iov={
		.iov_base = pkt,
		.iov_len = len,
	};
	
	OPENAISASSERT(mp);
	ais = (struct ais_private *) mp->pd;

	if (evs_mcast_joined(ais->handle, EVS_TYPE_AGREED, &iov, 1)
	    != EVS_OK){
		PILCallLog(LOG, PIL_CRIT,  "%s: evs_mcast_joined failed",
			   __FUNCTION__);
		return HA_FAIL;
	}
	
	return(HA_OK);
}


static struct hb_media_fns openaisOps ={
	openais_new,
	NULL,		
	openais_open,
	openais_close,
	openais_read,
	openais_write,
	openais_mtype,
	openais_descr,
	openais_isping,
};
