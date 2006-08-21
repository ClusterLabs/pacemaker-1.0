/*
 * hbaping.c: Fiber Channel Host Bus Adapters (HBA) aliveness code for heartbeat
 *
 * Copyright (C) 2004 Alain St-Denis <alain.st-denis@ec.gc.ca> 
 * 
 * The checksum code in this file code was borrowed from the ping program.
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

/*
 * This plugin only checks if it can reach device 0 by sending a HBA_SendScsiInquiry. 
 * There is room for improvement here.
 *
 */

#include <portability.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>

#ifdef HAVE_NETINET_IN_SYSTM_H
#	include <netinet/in_systm.h>
#endif /* HAVE_NETINET_IN_SYSTM_H */

#ifdef HAVE_NETINET_IP_VAR_H
#	include <netinet/ip_var.h>
#endif /* HAVE_NETINET_IP_VAR_H */

#ifdef HAVE_NETINET_IP_H
#	include <netinet/ip.h>
#endif /* HAVE_NETINET_IP_H */

#ifdef HAVE_NETINET_IP_COMPAT_H
#	include <netinet/ip_compat.h>
#endif /* HAVE_NETINET_IP_COMPAT_H */

#ifdef HAVE_NETINET_IP_FW_H
#	include <netinet/ip_fw.h>
#endif /* HAVE_NETINET_IP_FW_H */

#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <hbaapi.h>
#include <heartbeat.h>
#include <HBcomm.h>

#define PIL_PLUGINTYPE          HB_COMM_TYPE
#define PIL_PLUGINTYPE_S        HB_COMM_TYPE_S
#define PIL_PLUGIN              hbaping
#define PIL_PLUGIN_S            "hbaping"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL
#include <pils/plugin.h>


struct hbaping_private {
        HBA_WWN		        addr;   	/* dest WWN */
        HBA_HANDLE		handle;		/* hba handle */
	char 			namebuf[1028];  /* interface name */
	int			ident;		/* heartbeat pid */
	int			iseq;		/* sequence number */
};


static struct hb_media*	hbaping_new (const char* interface);
static int		hbaping_open (struct hb_media* mp);
static int		hbaping_close (struct hb_media* mp);
static void *		hbaping_read (struct hb_media* mp, int* lenp);
static int		hbaping_write (struct hb_media* mp, void* p, int len);

static struct hbaping_private *
			new_hbaping_interface(const char * host);

static int		hbaping_mtype(char **buffer);
static int		hbaping_descr(char **buffer);
static int		hbaping_isping(void);


#define		ISPINGOBJECT(mp)	((mp) && ((mp)->vf == (void*)&hbapingOps))
#define		PINGASSERT(mp)	g_assert(ISPINGOBJECT(mp))

static struct hb_media_fns hbapingOps ={
	hbaping_new,	/* Create single object function */
	NULL,		/* whole-line parse function */
	hbaping_open,
	hbaping_close,
	hbaping_read,
	hbaping_write,
	hbaping_mtype,
	hbaping_descr,
	hbaping_isping,
};

PIL_PLUGIN_BOILERPLATE2("1.0", Debug);

static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static struct hb_media_imports*	OurImports;
static void*			interfprivate;

#define LOG	PluginImports->log
#define MALLOC	PluginImports->alloc
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
	,	&hbapingOps
	,	NULL		/*close */
	,	&OurInterface
	,	(void*)&OurImports
	,	interfprivate); 
}
static int
hbaping_mtype(char **buffer) { 
	
	*buffer = MALLOC((strlen(PIL_PLUGIN_S) * sizeof(char)) + 1);

	strcpy(*buffer, PIL_PLUGIN_S);

	return strlen(PIL_PLUGIN_S);
}

static int
hbaping_descr(char **buffer) { 

	const char *str = "hbaping membership";	

	*buffer = MALLOC((strlen(str) * sizeof(char)) + 1);

	strcpy(*buffer, str);

	return strlen(str);
}

/* Yes, a ping device */
 
static int 
hbaping_isping(void) {
	return 1;
}

static struct hbaping_private *
new_hbaping_interface(const char * host)
{
	struct hbaping_private*	ppi;
	HBA_UINT32 hba_cnt;

	if ((ppi = (struct hbaping_private*)MALLOC(sizeof(struct hbaping_private))) == NULL) {
		return NULL;
	}
	memset(ppi, 0, sizeof (*ppi));

	if (HBA_LoadLibrary() != HBA_STATUS_OK) {
		LOG(PIL_CRIT, "error loading hbaapi: %s", strerror(errno));
		return(NULL);
	}

	hba_cnt = HBA_GetNumberOfAdapters();
	if (hba_cnt == 0) {
		LOG(PIL_CRIT, "no HBA found");
		return(NULL);
	}

	/* adapter identified by its name (e.g. qlogic-qla2200-0)
	 * only one HBA and one port per HBA. Bad.
	 */

	strncpy(ppi->namebuf, host, sizeof(ppi->namebuf));
	

	return(ppi);
}

/*
 *	Create new ping heartbeat object 
 *	Name of host is passed as a parameter
 */
static struct hb_media *
hbaping_new(const char * host)
{
	struct hbaping_private*	ipi;
	struct hb_media *	ret;

	ipi = new_hbaping_interface(host);
	if (ipi == NULL) {
		return(NULL);
	}

	ret = (struct hb_media *) MALLOC(sizeof(struct hb_media));
	if (ret != NULL) {
		char * name;
		ret->pd = (void*)ipi;
		name = MALLOC(strlen(host)+1);
		strcpy(name, host);
		ret->name = name;
		add_node(host, PINGNODE_I);

	}else{
		FREE(ipi); ipi = NULL;
	}
	return(ret);
}

/*
 *	Close UDP/IP broadcast heartbeat interface
 */

static int
hbaping_close(struct hb_media* mp)
{
	struct hbaping_private * ei;
	int	rc = HA_OK;

	PINGASSERT(mp);
	ei = (struct hbaping_private *) mp->pd;

	HBA_CloseAdapter(ei->handle);

	return(rc);
}

/*
 * Receive a heartbeat ping reply packet.
 * Here we do a HBA_SendScsiInquiry and build a HA msg if it is
 * successful.
 */

char hbaping_pkt[MAXLINE];
static void *
hbaping_read(struct hb_media* mp, int *lenp)
{
	struct hbaping_private *	ei;
	char RspBuffer[96], SenseBuffer[96];
	int rc;
	struct ha_msg *nmsg;
	static char ts[32];
	void *pkt;

	sleep(1); /* since we are not polling... */

	PINGASSERT(mp);
	ei = (struct hbaping_private *) mp->pd;

	rc = HBA_SendScsiInquiry(ei->handle, ei->addr, 0, 0, 0,
			(void *)&RspBuffer, sizeof(RspBuffer),
			(void *)&SenseBuffer, sizeof(SenseBuffer));
	if (rc != HBA_STATUS_OK) {
		/* LOG(PIL_CRIT, "can't inquiry to %s, status = %d", ei->namebuf, rc); */
		return NULL;
	}

	if ((nmsg = ha_msg_new(5)) == NULL) {
		LOG(PIL_CRIT, "cannot create new message");
		return NULL;
	}

        sprintf(ts, "%lx", time(NULL));
	if (ha_msg_add(nmsg, F_TYPE, T_NS_STATUS) != HA_OK
			||      ha_msg_add(nmsg, F_STATUS, PINGSTATUS) != HA_OK
			||      ha_msg_add(nmsg, F_ORIG, ei->namebuf) != HA_OK
			||      ha_msg_add(nmsg, F_TIME, ts) != HA_OK) {
		ha_msg_del(nmsg); nmsg = NULL;
		LOG(PIL_CRIT, "cannot add fields to message");
		return NULL;
	}

	if (add_msg_auth(nmsg) != HA_OK) {
		LOG(PIL_CRIT, "cannot add auth field to message");
		ha_msg_del(nmsg); nmsg = NULL;
		return NULL;
	}
	
	pkt = msg2wirefmt(nmsg, lenp);
	if( pkt == NULL){
		LOG(PIL_WARN, "containg msg to wirefmt failed in hbaping_read()\n");
		return NULL;
	}
	
	ha_msg_del(nmsg);
	
	memcpy(hbaping_pkt, pkt, *lenp);
	cl_free(pkt);
	
	return hbaping_pkt;
	
}

/*
 * Send a heartbeat packet over broadcast UDP/IP interface
 *
 * This is a noop.
 *
 * We ignore packets we're given to write that aren't "status" packets.
 *
 */

static int
hbaping_write(struct hb_media* mp, void *p, int len)
{
/*  	struct hbaping_private *	ei; */
/*  	HBA_PORTATTRIBUTES hba_portattrs; */
/*  	int			rc; */
/*  	const char *		type; */
/*  	const char *		ts; */

	PINGASSERT(mp);
/*  	ei = (struct hbaping_private *) mp->pd; */
/*  	type = ha_msg_value(msg, F_TYPE); */

/*  	if (type == NULL || strcmp(type, T_STATUS) != 0  */
/*  	|| ((ts = ha_msg_value(msg, F_TIME)) == NULL)) { */
/*  		return HA_OK; */
/*  	} */

	/*rc = HBA_GetAdapterPortAttributes(ei->handle, 0, &hba_portattrs);*/
	/*  	rc = HBA_STATUS_OK; */
	/*  	if (rc != HBA_STATUS_OK) { */
	/*  		LOG(PIL_CRIT, "can't get %s hba attributes (status = %d)!", ei->namebuf, rc); */
	/*  		return HA_FAIL; */
	/*  	} */
	
	/*if (hba_portattrs.PortState != HBA_PORTSTATE_ONLINE) {
		LOG(PIL_CRIT, "hba %s is not online!", ei->namebuf);
		return HA_FAIL;
	}*/

	return HA_OK;
}

/*
 *	Open hbaping. Basically a noop...
 */
static int
hbaping_open(struct hb_media* mp)
{
	struct hbaping_private * ppi;
	int retval;
	union {
		HBA_FCPTARGETMAPPING	fcp_tmap;
		struct {
			HBA_UINT32	cnt;
			HBA_FCPSCSIENTRY	entry[32];
		} fcp_tmapi;
	} map;



	PINGASSERT(mp);
	ppi = (struct hbaping_private *) mp->pd;
	
	if ((ppi->handle = HBA_OpenAdapter(ppi->namebuf)) == 0) {
		LOG(PIL_CRIT, "can't open adapter %s", ppi->namebuf);
		return(HA_FAIL);
	}
	
	/* discover target mapping, use the first port only
	 * will be used to contact shared device controller
	 * in hbaping_write
	 */
	
	map.fcp_tmapi.cnt = 32;
	retval = HBA_GetFcpTargetMapping(ppi->handle, &map.fcp_tmap);
	if (retval != HBA_STATUS_OK) {
		LOG(PIL_CRIT, "failure of HBA_GetFcpTargetMapping: %d", retval);
		return(HA_FAIL);
	}
	if (map.fcp_tmap.NumberOfEntries == 0) {
		LOG(PIL_CRIT, "no target found for adapter %s", ppi->namebuf);
		return(HA_FAIL);
	}
	/*memcpy(&(ppi->addr), &(map.fcp_tmap.entry[0].FcpId.PortWWN), 
	  sizeof(HBA_WWN));*/
	ppi->addr = map.fcp_tmap.entry[0].FcpId.PortWWN;
	
	ppi->ident = getpid() & 0xFFFF;


	LOG(LOG_NOTICE, "hbaping heartbeat started.");
	return HA_OK;
}

