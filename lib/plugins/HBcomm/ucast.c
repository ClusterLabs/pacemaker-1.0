/* $Id: ucast.c,v 1.27 2005/10/15 02:37:52 gshi Exp $ */
/*
 * Adapted from alanr's UDP broadcast heartbeat bcast.c by Stéphane Billiart
 *	<stephane@reefedge.com>
 *
 * (c) 2002  Stéphane Billiart <stephane@reefedge.com>
 * (c) 2002  Alan Robertson <alanr@unix.sh>
 *
 * Brian Tinsley <btinsley@emageon.com> 
 *  - allow use of hostname in ha.cf
 *  - set IP type of service of write socket
 *  - many MALLOC calls were not checked for failure
 *  - code janitor
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
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef HAVE_INET_ATON
	extern  int     inet_aton(const char *, struct in_addr *);
#endif
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#if defined(SO_BINDTODEVICE)
#include <net/if.h>
#endif

#include <heartbeat.h>
#include <HBcomm.h>


/*
 * Plugin information
 */
#define PIL_PLUGINTYPE          HB_COMM_TYPE
#define PIL_PLUGINTYPE_S        HB_COMM_TYPE_S
#define PIL_PLUGIN              ucast
#define PIL_PLUGIN_S            "ucast"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL
#include <pils/plugin.h>


/*
 * Macros/Defines
 */
#define ISUCASTOBJECT(mp) ((mp) && ((mp)->vf == (void*)&ucastOps))
#define UCASTASSERT(mp)	g_assert(ISUCASTOBJECT(mp))

#define LOG		PluginImports->log
#define MALLOC		PluginImports->alloc
#define STRDUP  	PluginImports->mstrdup
#define FREE		PluginImports->mfree

#define	MAXBINDTRIES	10


/*
 * Structure Declarations
 */

struct ip_private {
        char* interface;		/* Interface name */
	struct in_addr heartaddr;	/* Peer node address */
        struct sockaddr_in addr;	/* Local address */
        int port;			/* UDP port */
        int rsocket;			/* Read-socket */
        int wsocket;			/* Write-socket */
};


/*
 * Function Prototypes
 */

PIL_rc PIL_PLUGIN_INIT(PILPlugin *us, const PILPluginImports *imports);

static int ucast_parse(const char *line);
static struct hb_media* ucast_new(const char *intf, const char *addr);
static int ucast_open(struct hb_media *mp);
static int ucast_close(struct hb_media *mp);
static void* ucast_read(struct hb_media *mp, int* lenp);
static int ucast_write(struct hb_media *mp, void *msg, int len);

static int HB_make_receive_sock(struct hb_media *ei);
static int HB_make_send_sock(struct hb_media *mp);

static struct ip_private* new_ip_interface(const char *ifn,
				const char *hbaddr, int port);

static int ucast_descr(char **buffer);
static int ucast_mtype(char **buffer);
static int ucast_isping(void);


/*
 * External Data
 */

extern struct hb_media *sysmedia[];
extern int nummedia;

/*
 * Module Public Data
 */

const char hb_media_name[] = "UDP/IP unicast";	

static struct hb_media_fns ucastOps = {
	NULL,		
	ucast_parse,
	ucast_open,
	ucast_close,
	ucast_read,
	ucast_write,
	ucast_mtype,
	ucast_descr,
	ucast_isping
};

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static struct hb_media_imports*	OurImports;
static void*			interfprivate;
static int			localudpport;


/*
 * Implmentation
 */

PIL_rc PIL_PLUGIN_INIT(PILPlugin *us, const PILPluginImports *imports)
{
	/* Force the compiler to do a little type checking */
	(void)(PILPluginInitFun)PIL_PLUGIN_INIT;

	PluginImports = imports;
	OurPlugin = us;

	/* Register ourself as a plugin */
	imports->register_plugin(us, &OurPIExports);

	/*  Register our interface implementation */
 	return imports->register_interface(us, PIL_PLUGINTYPE_S,
		PIL_PLUGIN_S, &ucastOps, NULL,
		&OurInterface, (void*)&OurImports, interfprivate); 
}

static int ucast_parse(const char *line)
{ 
	const char *bp = line;
	int toklen;
	struct hb_media *mp;
	char dev[MAXLINE];
	char ucast[MAXLINE];

	/* Skip over white space, then grab the device */
	bp += strspn(bp, WHITESPACE);
	toklen = strcspn(bp, WHITESPACE);
	strncpy(dev, bp, toklen);
	bp += toklen;
	dev[toklen] = EOS;

	if (*dev != EOS)  {
#ifdef NOTYET
		if (!is_valid_dev(dev)) {
			PILCallLog(LOG, PIL_CRIT, "ucast: bad device [%s]", dev);
			return HA_FAIL;
		}
#endif
		bp += strspn(bp, WHITESPACE);
		toklen = strcspn(bp, WHITESPACE);
		strncpy(ucast, bp, toklen);
		bp += toklen;
		ucast[toklen] = EOS;
	
		if (*ucast == EOS)  {
			PILCallLog(LOG, PIL_CRIT,
			  "ucast: [%s] missing target IP address/hostname",
			  dev);
			return HA_FAIL;
		}
		if (!(mp = ucast_new(dev, ucast)))
			return HA_FAIL;

		sysmedia[nummedia++] = mp;
	}

	return HA_OK;
}

static int ucast_mtype(char **buffer)
{
	*buffer = STRDUP(PIL_PLUGIN_S);
	if (!*buffer) {
		PILCallLog(LOG, PIL_CRIT, "ucast: memory allocation error (line %d)",
				(__LINE__ - 2) );
		return 0;
	}

	return strlen(*buffer);
}

static int ucast_descr(char **buffer)
{ 
	*buffer = strdup(hb_media_name);
	if (!*buffer) {
		PILCallLog(LOG, PIL_CRIT, "ucast: memory allocation error (line %d)",
				(__LINE__ - 2) );
		return 0;
	}

	return strlen(*buffer);
}

static int ucast_isping(void)
{
	return 0;
}

static int ucast_init(void)
{
	struct servent *service;

	g_assert(OurImports != NULL);

	if (localudpport <= 0) {
		const char *chport;
		if ((chport  = OurImports->ParamValue("udpport")) != NULL) {
			if (sscanf(chport, "%d", &localudpport) <= 0
			    || localudpport <= 0) {
				PILCallLog(LOG, PIL_CRIT,
					"ucast: bad port number %s", chport);
				return HA_FAIL;
			}
		}
	}

	/* No port specified in the configuration... */

	if (localudpport <= 0) {
		/* If our service name is in /etc/services, then use it */
		if ((service=getservbyname(HA_SERVICENAME, "udp")) != NULL)
			localudpport = ntohs(service->s_port);
		else
			localudpport = UDPPORT;
	}
	return HA_OK;
}

/*
 *	Create new UDP/IP unicast heartbeat object 
 *	Name of interface and address are passed as parameters
 */
static struct hb_media*
ucast_new(const char *intf, const char *addr)
{
	struct ip_private *ipi;
	struct hb_media *ret;
	char *name;

	ucast_init();

	if (!(ipi = new_ip_interface(intf, addr, localudpport))) {
		PILCallLog(LOG, PIL_CRIT, "ucast: interface [%s] does not exist", intf);
		return NULL;
	}
	if (!(ret = (struct hb_media*)MALLOC(sizeof(struct hb_media)))) {
		PILCallLog(LOG, PIL_CRIT, "ucast: memory allocation error (line %d)",
			(__LINE__ - 2) );
		FREE(ipi->interface);
		FREE(ipi);
	}
	else {
		ret->pd = (void*)ipi;
		if (!(name = STRDUP(intf))) {
			PILCallLog(LOG, PIL_CRIT,
				"ucast: memory allocation error (line %d)",
				(__LINE__ - 3) );
			FREE(ipi->interface);
			FREE(ipi);
			FREE(ret);
			ret = NULL;
		}
		else {
			ret->name = name;
		}
	}

	return ret;
}

/*
 *	Open UDP/IP unicast heartbeat interface
 */
static int ucast_open(struct hb_media* mp)
{
	struct ip_private * ei;

	UCASTASSERT(mp);
	ei = (struct ip_private*)mp->pd;

	if ((ei->wsocket = HB_make_send_sock(mp)) < 0)
		return HA_FAIL;
	if ((ei->rsocket = HB_make_receive_sock(mp)) < 0) {
		ucast_close(mp);
		return HA_FAIL;
	}

	PILCallLog(LOG, PIL_INFO, "ucast: started on port %d interface %s to %s",
		localudpport, ei->interface, inet_ntoa(ei->addr.sin_addr));

	return HA_OK;
}

/*
 *	Close UDP/IP unicast heartbeat interface
 */
static int ucast_close(struct hb_media* mp)
{
	struct ip_private *ei;
	int rc = HA_OK;

	UCASTASSERT(mp);
	ei = (struct ip_private*)mp->pd;

	if (ei->rsocket >= 0) {
		if (close(ei->rsocket) < 0)
			rc = HA_FAIL;
	}
	if (ei->wsocket >= 0) {
		if (close(ei->wsocket) < 0)
			rc = HA_FAIL;
	}
	return rc;
}


/*
 * Receive a heartbeat unicast packet from UDP interface
 */

char ucast_pkt[MAXMSG];

static void *
ucast_read(struct hb_media* mp, int *lenp)
{
	struct ip_private *ei;
	socklen_t addr_len;
	struct sockaddr_in their_addr;
	int numbytes;
	
	UCASTASSERT(mp);
	ei = (struct ip_private*)mp->pd;

	addr_len = sizeof(struct sockaddr);
	if ((numbytes = recvfrom(ei->rsocket, ucast_pkt, MAXMSG-1, 0,
		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		if (errno != EINTR) {
			PILCallLog(LOG, PIL_CRIT, "ucast: error receiving from socket: %s",
				strerror(errno));
		}
		return NULL;
	}
	if (numbytes == 0) {
		PILCallLog(LOG, PIL_CRIT, "ucast: received zero bytes");
		return NULL;
	}
	
	ucast_pkt[numbytes] = EOS;
	
	if (DEBUGPKT) {
		PILCallLog(LOG, PIL_DEBUG, "ucast: received %d byte packet from %s",
			numbytes, inet_ntoa(their_addr.sin_addr));
	}
	if (DEBUGPKTCONT) {
		PILCallLog(LOG, PIL_DEBUG, "%s", ucast_pkt);
	}

	*lenp = numbytes +1;
	
	return ucast_pkt;
	
	
}

/*
 * Send a heartbeat packet over unicast UDP/IP interface
 */

static int
ucast_write(struct hb_media* mp, void *pkt, int len)
{
	struct ip_private *ei;
	int rc;
	
	UCASTASSERT(mp);
	ei = (struct ip_private*)mp->pd;
	
	if ((rc = sendto(ei->wsocket, pkt, len, 0
	,		(struct sockaddr *)&ei->addr
	,		 sizeof(struct sockaddr))) != len) {
		PILCallLog(LOG, PIL_CRIT, "Unable to send [%d] ucast packet: %s"
		,	rc, strerror(errno));
		return HA_FAIL;
	}
	
	if (DEBUGPKT) {
		PILCallLog(LOG, PIL_DEBUG, "ucast: sent %d bytes to %s", rc,
		    inet_ntoa(ei->addr.sin_addr));
   	}
	if (DEBUGPKTCONT) {
		PILCallLog(LOG, PIL_DEBUG, "%s", (const char*)pkt);
   	}
	return HA_OK;	
}

/*
 * Set up socket for sending unicast UDP heartbeats
 */

static int HB_make_send_sock(struct hb_media *mp)
{
	int sockfd;
	struct ip_private *ei;
	int tos;
#if defined(SO_BINDTODEVICE)
	struct ifreq i;
#endif

	UCASTASSERT(mp);
	ei = (struct ip_private*)mp->pd;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		PILCallLog(LOG, PIL_CRIT, "ucast: Error creating write socket: %s",
			strerror(errno));
   	}

	/*
 	 * 21 December 2002
 	 * Added by Brian TInsley <btinsley@emageon.com>
 	 */
	tos = IPTOS_LOWDELAY;
	if (setsockopt(sockfd, IPPROTO_IP, IP_TOS,
				&tos, sizeof(tos)) < 0) {
		PILCallLog(LOG, PIL_CRIT, "ucast: error setting socket option IP_TOS: %s",
					strerror(errno));
	}
	else {
		PILCallLog(LOG, PIL_INFO,
		  "ucast: write socket priority set to IPTOS_LOWDELAY on %s",
		  ei->interface);
	}

#if defined(SO_BINDTODEVICE)
	{
		/*
		 *  We want to send out this particular interface
		 *
		 * This is so we can have redundant NICs, and heartbeat on both
		 */
		strcpy(i.ifr_name,  ei->interface);

		if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,
				&i, sizeof(i)) == -1) {
			PILCallLog(LOG, PIL_CRIT,
			  "ucast: error setting option SO_BINDTODEVICE(w) on %s: %s",
			  i.ifr_name, strerror(errno));
			close(sockfd);
			return -1;
		}
		PILCallLog(LOG, PIL_INFO, "ucast: bound send socket to device: %s",
			i.ifr_name);
	}
#endif
	if (fcntl(sockfd,F_SETFD, FD_CLOEXEC) < 0) {
		PILCallLog(LOG, PIL_CRIT, "ucast: error setting close-on-exec flag: %s",
			strerror(errno));
	}

	return sockfd;
}

/*
 * Set up socket for listening to heartbeats (UDP unicast)
 */

static int HB_make_receive_sock(struct hb_media *mp) {

	struct ip_private *ei;
	struct sockaddr_in my_addr;
	int sockfd;
	int bindtries;
	int boundyet = 0;
	int j;

	UCASTASSERT(mp);
	ei = (struct ip_private*)mp->pd;

	memset(&(my_addr), 0, sizeof(my_addr));	/* zero my address struct */
	my_addr.sin_family = AF_INET;		/* host byte order */
	my_addr.sin_port = htons(ei->port);	/* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY;	/* auto-fill with my IP */

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		PILCallLog(LOG, PIL_CRIT, "ucast: error creating read socket: %s",
			strerror(errno));
		return -1;
	}
	/* 
 	 * Set SO_REUSEADDR on the server socket s. Variable j is used
 	 * as a scratch varable.
 	 *
 	 * 16th February 2000
 	 * Added by Horms <horms@vergenet.net>
 	 * with thanks to Clinton Work <work@scripty.com>
 	 */
	j = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
			(void *)&j, sizeof j) < 0) {
		/* Ignore it.  It will almost always be OK anyway. */
		PILCallLog(LOG, PIL_CRIT,
			"ucast: error setting socket option SO_REUSEADDR: %s",
			strerror(errno));
	}        
#if defined(SO_BINDTODEVICE)
	{
		/*
		 *  We want to receive packets only from this interface...
		 */
		struct ifreq i;
		strcpy(i.ifr_name,  ei->interface);

		if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE,
				&i, sizeof(i)) == -1) {
			PILCallLog(LOG, PIL_CRIT,
			  "ucast: error setting option SO_BINDTODEVICE(r) on %s: %s",
			  i.ifr_name, strerror(errno));
			close(sockfd);
			return -1;
		}
		PILCallLog(LOG, PIL_INFO, "ucast: bound receive socket to device: %s",
			i.ifr_name);
	}
#endif

	/* Try binding a few times before giving up */
	/* Sometimes a process with it open is exiting right now */

	for (bindtries=0; !boundyet && bindtries < MAXBINDTRIES; ++bindtries) {
		if (bind(sockfd, (struct sockaddr *)&my_addr,
				sizeof(struct sockaddr)) < 0) {
			PILCallLog(LOG, PIL_CRIT, "ucast: error binding socket. Retrying: %s",
				strerror(errno));
			sleep(1);
		}
		else{
			boundyet = 1;
		}
	}
	if (!boundyet) {
#if !defined(SO_BINDTODEVICE)
		if (errno == EADDRINUSE) {
			/* This happens with multiple udp or ppp interfaces */
			PILCallLog(LOG, PIL_INFO,
			  "ucast: someone already listening on port %d [%s]",
			  ei->port, ei->interface);
			PILCallLog(LOG, PIL_INFO, "ucast: UDP read process exiting");
			close(sockfd);
			cleanexit(0);
		}
#else
		PILCallLog(LOG, PIL_CRIT, "ucast: unable to bind socket. Giving up: %s",
			strerror(errno));
		close(sockfd);
		return -1;
#endif
	}
	if (fcntl(sockfd,F_SETFD, FD_CLOEXEC) < 0) {
		PILCallLog(LOG, PIL_CRIT, "ucast: error setting close-on-exec flag: %s",
			strerror(errno));
	}
	return sockfd;
}

static struct ip_private* new_ip_interface(const char *ifn,
				const char *hbaddr, int port)
{
	struct ip_private *ep;
	struct hostent *h;

	/*
 	 * 21 December 2002
 	 * Added by Brian TInsley <btinsley@emageon.com>
 	 */
	if (!(h = gethostbyname(hbaddr))) {
		PILCallLog(LOG, PIL_CRIT, "ucast: cannot resolve hostname");
		return NULL;
	}

	if (!(ep = (struct ip_private*) MALLOC(sizeof(struct ip_private)))) {
		PILCallLog(LOG, PIL_CRIT, "ucast: memory allocation error (line %d)",
			(__LINE__ - 2) );
		return NULL;
	}

	/*
	 * use address from gethostbyname
	*/
	memcpy(&ep->heartaddr, h->h_addr_list[0], sizeof(ep->heartaddr));

	if (!(ep->interface = STRDUP(ifn))) {
		PILCallLog(LOG, PIL_CRIT, "ucast: memory allocation error (line %d)",
			(__LINE__ - 2) );
		FREE(ep);
		return NULL;
	}
	
	bzero(&ep->addr, sizeof(ep->addr));	/* zero the struct */
	ep->addr.sin_family = AF_INET;		/* host byte order */
	ep->addr.sin_port = htons(port);	/* short, network byte order */
	ep->port = port;
	ep->wsocket = -1;
	ep->rsocket = -1;
	ep->addr.sin_addr = ep->heartaddr;

	return ep;
}
