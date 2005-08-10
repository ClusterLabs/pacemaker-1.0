/* $Id: ping.c,v 1.43 2005/08/10 04:08:16 horms Exp $ */
/*
 * ping.c: ICMP-echo-based heartbeat code for heartbeat.
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
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

#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#ifdef HAVE_NETINET_IN_SYSTM_H
#	include <netinet/in_systm.h>
#endif /* HAVE_NETINET_IN_SYSTM_H */

#ifdef HAVE_NETINET_IP_VAR_H
#	include <netinet/ip_var.h>
#endif /* HAVE_NETINET_IP_VAR_H */

#ifdef HAVE_NETINET_IP_FW_H
#	include <netinet/ip_fw.h>
#endif /* HAVE_NETINET_IP_FW_H */

#ifdef HAVE_NETINET_IP_H
#	include <netinet/ip.h>
#endif /* HAVE_NETINET_IP_H */

#include <netinet/ip_icmp.h>

#ifdef HAVE_NETINET_IP_COMPAT_H
#	include <netinet/ip_compat.h>
#endif /* HAVE_NETINET_IP_COMPAT_H */

#include <net/if.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <heartbeat.h>
#include <HBcomm.h>

#ifdef linux
#	define	ICMP_HDR_SZ	sizeof(struct icmphdr)	/* 8 */
#else
#	define	ICMP_HDR_SZ	8
#endif

#define PIL_PLUGINTYPE          HB_COMM_TYPE
#define PIL_PLUGINTYPE_S        HB_COMM_TYPE_S
#define PIL_PLUGIN              ping
#define PIL_PLUGIN_S            "ping"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL
#include <pils/plugin.h>


struct ping_private {
        struct sockaddr_in      addr;   	/* ping addr */
        int    			sock;		/* ping socket */
	int			ident;		/* heartbeat pid */
	int			iseq;		/* sequence number */
};


static struct hb_media*	ping_new (const char* interface);
static int		ping_open (struct hb_media* mp);
static int		ping_close (struct hb_media* mp);
static void*		ping_read (struct hb_media* mp, int* lenp);
static int		ping_write (struct hb_media* mp, void* p, int len);

static struct ping_private *
			new_ping_interface(const char * host);
static int		in_cksum (u_short * buf, size_t nbytes);

static int		ping_mtype(char **buffer);
static int		ping_descr(char **buffer);
static int		ping_isping(void);


#define		ISPINGOBJECT(mp)	((mp) && ((mp)->vf == (void*)&pingOps))
#define		PINGASSERT(mp)	g_assert(ISPINGOBJECT(mp))

static struct hb_media_fns pingOps ={
	ping_new,	/* Create single object function */
	NULL,		/* whole-line parse function */
	ping_open,
	ping_close,
	ping_read,
	ping_write,
	ping_mtype,
	ping_descr,
	ping_isping,
};

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
	,	&pingOps
	,	NULL		/*close */
	,	&OurInterface
	,	(void*)&OurImports
	,	interfprivate); 
}
static int
ping_mtype(char **buffer) { 
	*buffer = STRDUP(PIL_PLUGIN_S);
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

static int
ping_descr(char **buffer) { 
	*buffer = STRDUP("ping membership");
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

/* Yes, a ping device */

static int
ping_isping(void) {
	return 1;
}


static struct ping_private *
new_ping_interface(const char * host)
{
	struct ping_private*	ppi;
	struct sockaddr_in *to;

	if ((ppi = (struct ping_private*)MALLOC(sizeof(struct ping_private)))
	== NULL) {
		return NULL;
	}
	memset(ppi, 0, sizeof (*ppi));
  	to = &ppi->addr;

#ifdef HAVE_SOCKADDR_IN_SIN_LEN
	ppi->addr.sin_len = sizeof(struct sockaddr_in);
#endif
	ppi->addr.sin_family = AF_INET;

	if (inet_pton(AF_INET, host, (void *)&ppi->addr.sin_addr) <= 0) {
		struct hostent *hep;
		hep = gethostbyname(host);
		if (hep == NULL) {
			PILCallLog(LOG, PIL_CRIT, "unknown host: %s: %s"
			,	host, strerror(errno));
			FREE(ppi); ppi = NULL;
			return NULL;
		}
		ppi->addr.sin_family = hep->h_addrtype;
		memcpy(&ppi->addr.sin_addr, hep->h_addr, hep->h_length);
	}
	ppi->ident = getpid() & 0xFFFF;
	return(ppi);
}

/*
 *	Create new ping heartbeat object 
 *	Name of host is passed as a parameter
 */
static struct hb_media *
ping_new(const char * host)
{
	struct ping_private*	ipi;
	struct hb_media *	ret;
	char * 			name;

	ipi = new_ping_interface(host);
	if (ipi == NULL) {
		return(NULL);
	}

	ret = (struct hb_media *) MALLOC(sizeof(struct hb_media));
	if (ret == NULL) {
		FREE(ipi); ipi = NULL;
		return(NULL);
	}

	ret->pd = (void*)ipi;
	name = STRDUP(host);
	if(name == NULL) {
		FREE(ipi); ipi = NULL;
		FREE(ret); ret = NULL;
		return(NULL);
	}
	ret->name = name;
	add_node(host, PINGNODE_I);

	return(ret);
}

/*
 *	Close ICMP ping heartbeat interface
 */

static int
ping_close(struct hb_media* mp)
{
	struct ping_private * ei;
	int	rc = HA_OK;

	PINGASSERT(mp);
	ei = (struct ping_private *) mp->pd;

	if (ei->sock >= 0) {
		if (close(ei->sock) < 0) {
			rc = HA_FAIL;
		}
	}
	return(rc);
}



/*
 * Receive a heartbeat ping reply packet.
 * NOTE: This code only needs to run once for ALL ping nodes.
 * FIXME!!
 */

static void *
ping_read(struct hb_media* mp, int *lenp)
{
	struct ping_private *	ei;
	union {
		char		cbuf[MAXLINE+ICMP_HDR_SZ];
		struct ip	ip;
	}buf;
	const char *		bufmax = ((char *)&buf)+sizeof(buf);
	char *			msgstart;
	socklen_t		addr_len = sizeof(struct sockaddr);
   	struct sockaddr_in	their_addr; /* connector's addr information */
	struct ip *		ip;
	struct icmp		icp;
	int			numbytes;
	int			hlen;
	struct ha_msg *		msg;
	const char 		*comment;
	char			*pkt;
	int			pktlen;
	
	PINGASSERT(mp);
	ei = (struct ping_private *) mp->pd;

ReRead:	/* We recv lots of packets that aren't ours */
	
	if ((numbytes=recvfrom(ei->sock, (void *) &buf.cbuf
	,	sizeof(buf.cbuf)-1, 0,	(struct sockaddr *)&their_addr
	,	&addr_len)) < 0) {
		if (errno != EINTR) {
			PILCallLog(LOG, PIL_CRIT, "Error receiving from socket: %s"
			,	strerror(errno));
		}
		return NULL;
	}
	/* Avoid potential buffer overruns */
	buf.cbuf[numbytes] = EOS;

	/* Check the IP header */
	ip = &buf.ip;
	hlen = ip->ip_hl * 4;

	if (numbytes < hlen + ICMP_MINLEN) {
		PILCallLog(LOG, PIL_WARN, "ping packet too short (%d bytes) from %s"
		,	numbytes
		,	inet_ntoa(*(struct in_addr *)
		&		their_addr.sin_addr.s_addr));
		return NULL;
	}
	
	/* Now the ICMP part */	/* (there may be a better way...) */
	memcpy(&icp, (buf.cbuf + hlen), sizeof(icp));
	
	if (icp.icmp_type != ICMP_ECHOREPLY || icp.icmp_id != ei->ident) {
		goto ReRead;	/* Not one of ours */
	}

	if (DEBUGPKT) {
		PILCallLog(LOG, PIL_DEBUG, "got %d byte packet from %s"
		,	numbytes, inet_ntoa(their_addr.sin_addr));
	}
	msgstart = (buf.cbuf + hlen + ICMP_HDR_SZ);

	if (DEBUGPKTCONT && numbytes > 0) {
		PILCallLog(LOG, PIL_DEBUG, "%s", msgstart);
	}
	
	pktlen = numbytes - hlen - ICMP_HDR_SZ;

	if ((pkt = ha_malloc(pktlen + 1)) == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	
	pkt[pktlen] = 0;
	
	memcpy(pkt, buf.cbuf + hlen + ICMP_HDR_SZ, pktlen);	
	*lenp = pktlen + 1;
 
	
	msg = wirefmt2msg(msgstart, bufmax - msgstart, MSG_NEEDAUTH);
	if (msg == NULL) {
                ha_free(pkt);
		errno = EINVAL;
		return(NULL);
	}
	comment = ha_msg_value(msg, F_COMMENT);
	if (comment == NULL || strcmp(comment, PIL_PLUGIN_S)) {
                ha_free(pkt);
		ha_msg_del(msg);
		errno = EINVAL;
		return(NULL);
	}
	
	ha_msg_del(msg);
	return(pkt);
}

/*
 * Send a heartbeat packet over ICMP ping channel
 *
 * The peculiar thing here is that we don't send the packet we're given at all
 *
 * Instead, we send out the packet we want to hear back from them, just
 * as though we were they ;-)  That's what comes of having such a dumb
 * device as a "member" of our cluster...
 *
 * We ignore packets we're given to write that aren't "status" packets.
 *
 */

static int
ping_write(struct hb_media* mp, void *p, int len)
{
	struct ping_private *	ei;
	int			rc;
	char*			pkt;
	union{
		char*			buf;
		struct icmp		ipkt;
	}*icmp_pkt;
	size_t			size;
	struct icmp *		icp;
	size_t			pktsize;
	const char *		type;
	const char *		ts;
	struct ha_msg *		nmsg;
	struct ha_msg *		msg;
	
	
	msg = wirefmt2msg(p, len, MSG_NEEDAUTH);
	if( !msg){
		PILCallLog(LOG, PIL_CRIT, "ping_write(): cannot convert wirefmt to msg");
		return(HA_FAIL);
	}
	
	PINGASSERT(mp);
	ei = (struct ping_private *) mp->pd;
	type = ha_msg_value(msg, F_TYPE);
	
	if (type == NULL || strcmp(type, T_STATUS) != 0 
	|| ((ts = ha_msg_value(msg, F_TIME)) == NULL)) {
		ha_msg_del(msg);
		return HA_OK;
	}

	/*
	 * We populate the following fields in the packet we create:
	 *
	 * F_TYPE:	T_NS_STATUS
	 * F_STATUS:	ping
	 * F_COMMENT:	ping
	 * F_ORIG:	destination name
	 * F_TIME:	local timestamp (from "msg")
	 * F_AUTH:	added by add_msg_auth()
	 */
	if ((nmsg = ha_msg_new(5)) == NULL) {
		PILCallLog(LOG, PIL_CRIT, "cannot create new message");
		ha_msg_del(msg);
		return(HA_FAIL);
	}

	if (ha_msg_add(nmsg, F_TYPE, T_NS_STATUS) != HA_OK
	||	ha_msg_add(nmsg, F_STATUS, PINGSTATUS) != HA_OK
	||	ha_msg_add(nmsg, F_COMMENT, PIL_PLUGIN_S) != HA_OK
	||	ha_msg_add(nmsg, F_ORIG, mp->name) != HA_OK
	||	ha_msg_add(nmsg, F_TIME, ts) != HA_OK) {
		ha_msg_del(nmsg); nmsg = NULL;
		PILCallLog(LOG, PIL_CRIT, "cannot add fields to message");
		ha_msg_del(msg);
		return HA_FAIL;
	}

	if (add_msg_auth(nmsg) != HA_OK) {
		PILCallLog(LOG, PIL_CRIT, "cannot add auth field to message");
		ha_msg_del(nmsg); nmsg = NULL;
		ha_msg_del(msg);
		return HA_FAIL;
	}
	
	if ((pkt = msg2wirefmt(nmsg, &size)) == NULL)  {
		PILCallLog(LOG, PIL_CRIT, "cannot convert message to string");
		ha_msg_del(msg);
		return HA_FAIL;
	}
	ha_msg_del(nmsg); nmsg = NULL;


	pktsize = size + ICMP_HDR_SZ;

	if ((icmp_pkt = MALLOC(pktsize)) == NULL) {
		PILCallLog(LOG, PIL_CRIT, "out of memory");
		ha_free(pkt);
		ha_msg_del(msg);
		return HA_FAIL;
	}

	icp = &(icmp_pkt->ipkt);
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = htons(ei->iseq);
	icp->icmp_id = ei->ident;	/* Only used by us */
	++ei->iseq;

	memcpy(icp->icmp_data, pkt, size);
	ha_free(pkt); pkt = NULL;

	/* Compute the ICMP checksum */
	icp->icmp_cksum = in_cksum((u_short *)icp, pktsize);

	if ((rc=sendto(ei->sock, (void *) icmp_pkt, pktsize, 0
	,	(struct sockaddr *)&ei->addr
	,	sizeof(struct sockaddr))) != (ssize_t)pktsize) {
		PILCallLog(LOG, PIL_CRIT, "Error sending packet: %s", strerror(errno));
		FREE(icmp_pkt);
		ha_msg_del(msg);
		return(HA_FAIL);
	}

	if (DEBUGPKT) {
		PILCallLog(LOG, PIL_DEBUG, "sent %d bytes to %s"
		,	rc, inet_ntoa(ei->addr.sin_addr));
   	}
	if (DEBUGPKTCONT) {
		PILCallLog(LOG, PIL_DEBUG, "ping pkt: %s"
		,	icp->icmp_data);
   	}
	FREE(icmp_pkt);
	ha_msg_del(msg);
	return HA_OK;


  
}

/*
 *	Open ping socket.
 */
static int
ping_open(struct hb_media* mp)
{
	struct ping_private * ei;
	int sockfd;
	struct protoent *proto;

	PINGASSERT(mp);
	ei = (struct ping_private *) mp->pd;


	if ((proto = getprotobyname("icmp")) == NULL) {
		PILCallLog(LOG, PIL_CRIT, "protocol ICMP is unknown: %s", strerror(errno));
		return HA_FAIL;
	}
	if ((sockfd = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
		PILCallLog(LOG, PIL_CRIT, "Can't open RAW socket.: %s", strerror(errno));
		return HA_FAIL;
    	}

	if (fcntl(sockfd, F_SETFD, FD_CLOEXEC)) {
		PILCallLog(LOG, PIL_CRIT, "Error setting the close-on-exec flag: %s"
		,	strerror(errno));
	}
	ei->sock = sockfd;

	PILCallLog(LOG, PIL_INFO, "ping heartbeat started.");
	return HA_OK;
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 *	This function taken from Mike Muuss' ping program.
 */
static int
in_cksum (u_short *addr, size_t len)
{
	size_t		nleft = len;
	u_short *	w = addr;
	int		sum = 0;
	u_short		answer = 0;

	/*
	 * The IP checksum algorithm is simple: using a 32 bit accumulator (sum)
	 * add sequential 16 bit words to it, and at the end, folding back all
	 * the carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	/* Mop up an odd byte, if necessary */
	if (nleft == 1) {
		sum += *(u_char*)w;
	}

	/* Add back carry bits from top 16 bits to low 16 bits */

	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */

	return answer;
}
