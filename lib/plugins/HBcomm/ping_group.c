/* $Id: ping_group.c,v 1.5 2004/02/17 22:11:59 lars Exp $ */
/*
 * ping_group.c: ICMP-echo-based heartbeat code for heartbeat.
 *
 * This allows a group of nodes to be pinged. The group is
 * considered to be available if any of the nodes are available.
 *
 * Copyright (C) 2003 Horms <horms@verge.net.au>
 *
 * Based heavily on ping.c
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

#ifdef HAVE_NETINET_IP_H
#	include <netinet/ip.h>
#endif /* HAVE_NETINET_IP_H */

#include <netinet/ip_icmp.h>

#ifdef HAVE_NETINET_IP_H
#	include <netinet/ip.h>
#endif /* HAVE_NETINET_IP_H */

#ifdef HAVE_NETINET_IP_VAR_H
#	include <netinet/ip_var.h>
#endif /* HAVE_NETINET_IP_VAR_H */

#ifdef HAVE_NETINET_IP_COMPAT_H
#	include <netinet/ip_compat.h>
#endif /* HAVE_NETINET_IP_COMPAT_H */

#ifdef HAVE_NETINET_IP_FW_H
#	include <netinet/ip_fw.h>
#endif /* HAVE_NETINET_IP_FW_H */

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
#define PIL_PLUGIN              ping_group
#define PIL_PLUGIN_S            "ping_group"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL
#include <pils/plugin.h>


#define NSLOT			16              /* How old ping sequence
						   numbers can be to still
						   count */
typedef struct ping_group_node ping_group_node_t;

struct ping_group_node {
        struct sockaddr_in      addr;   	/* ping addr */
	ping_group_node_t	*next;
};

typedef struct {
	int			ident;		/* heartbeat pid */
        int    			sock;		/* ping socket */
	ping_group_node_t	*node;
	size_t			nnode;
	int			slot[NSLOT];
	int			iseq;		/* sequence number */
} ping_group_private_t;


static int   		ping_group_parse(const char *line);
static int		ping_group_open (struct hb_media* mp);
static int		ping_group_close (struct hb_media* mp);
static struct ha_msg*	ping_group_read (struct hb_media* mp);
static int		ping_group_write (struct hb_media* mp
,				struct ha_msg* msg);

static struct hb_media * ping_group_new(const char *name);
static int		in_cksum (u_short * buf, int nbytes);

static int		ping_group_mtype(char **buffer);
static int		ping_group_descr(char **buffer);
static int		ping_group_isping(void);


#define		ISPINGGROUPOBJECT(mp)	                                     \
			((mp) && ((mp)->vf == (void*)&ping_group_ops))
#define		PINGGROUPASSERT(mp)	g_assert(ISPINGGROUPOBJECT(mp))

/*
 * pingclose is called as part of unloading the ping HBcomm plugin.
 * If there was any global data allocated, or file descriptors opened, etc.
 * which is associated with the plugin, and not a single interface
 * in particular, here's our chance to clean it up.
 */

static void
ping_group_close_pi(PILPlugin*pi)
{
}


/*
 * ping_group_close_intf called as part of shutting down the ping HBcomm interface.
 * If there was any global data allocated, or file descriptors opened, etc.
 * which is associated with the ping implementation, here's our chance
 * to clean it up.
 */
static PIL_rc
ping_group_close_intf(PILInterface* pi, void* pd)
{
	return PIL_OK;
}

static struct hb_media_fns ping_group_ops ={
	NULL,		  /* Create single object function */
	ping_group_parse, /* whole-line parse function */
	ping_group_open,
	ping_group_close,
	ping_group_read,
	ping_group_write,
	ping_group_mtype,
	ping_group_descr,
	ping_group_isping,
};

PIL_PLUGIN_BOILERPLATE("1.0", Debug, ping_group_close_pi);

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
	,	&ping_group_ops
	,	ping_group_close_intf		/*close */
	,	&OurInterface
	,	(void*)&OurImports
	,	interfprivate); 
}
static int
ping_group_mtype(char **buffer) { 
	
	*buffer = STRDUP(PIL_PLUGIN_S);
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

static int
ping_group_descr(char **buffer) { 
	*buffer = STRDUP("ping group membership");
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

/* Yes, a ping device */

static int
ping_group_isping(void) {
	return 1;
}


static ping_group_node_t *
new_ping_group_node(const char *host)
{
	ping_group_node_t* node;

	node = (ping_group_node_t*)MALLOC(sizeof(ping_group_node_t));
	if(!node) {
		return(NULL);
	}
	memset(node, 0, sizeof(ping_group_node_t));

#ifdef HAVE_SOCKADDR_IN_SIN_LEN
	node->addr.sin_len = sizeof(struct sockaddr_in);
#endif
	node->addr.sin_family = AF_INET;

	if (inet_pton(AF_INET, host, (void *)&node->addr.sin_addr) <= 0) {
		struct hostent *hp;
		hp = gethostbyname(host);
		if (hp == NULL) {
			LOG(PIL_CRIT, "unknown host: %s: %s"
			,	host, strerror(errno));
			FREE(node);
			return NULL;
		}
		node->addr.sin_family = hp->h_addrtype;
		memcpy(&node->addr.sin_addr, hp->h_addr, hp->h_length);
	}

	return(node);
}

static int
ping_group_add_node(struct hb_media* media, const char *host)
{
	ping_group_private_t *priv;
	ping_group_node_t *node;

	PINGGROUPASSERT(media);
	priv = (ping_group_private_t *)media->pd;

	node = new_ping_group_node(host);
	if(!node) {
		return(HA_FAIL);
	}

	node->next = priv->node;
	priv->node = node;
	priv->nnode++;

	return(HA_OK);
}

/*
 *	Create new ping heartbeat object 
 *	Name of host is passed as a parameter
 */
static struct hb_media *
ping_group_new(const char *name)
{
	ping_group_private_t*	priv;
	struct hb_media *	media;
	char *			tmp;

	priv = (ping_group_private_t*)MALLOC(sizeof(ping_group_private_t));
	if(!priv) {
		return(NULL);
	}
	memset(priv, 0, sizeof(ping_group_private_t));

	priv->ident = getpid() & 0xFFFF;

	media = (struct hb_media *) MALLOC(sizeof(struct hb_media));
	if(!media) {
		FREE(priv);
		return(NULL);
	}

	media->pd = (void*)priv;
	tmp = STRDUP(name);
	if(!tmp) {
		FREE(priv);
		FREE(media);
		return(NULL);
	}

	media->name = tmp;
	add_node(tmp, PINGNODE_I);

	/* Fake it so that PINGGROUPASSERT() will work
	 * before the media is registered */
	media->vf = (void*)&ping_group_ops;

	return(media);
}

static void
ping_group_destroy_data(struct hb_media* media)
{
	ping_group_private_t*	priv;
	ping_group_node_t *	node;

	PINGGROUPASSERT(media);
        priv = (ping_group_private_t *)media->pd;

	while(priv->node) {
		node = priv->node;
		priv->node = node->next;
		FREE(node);
	}
}

static void
ping_group_destroy(struct hb_media* media)
{
	ping_group_private_t*	priv;

	PINGGROUPASSERT(media);
        priv = (ping_group_private_t *)media->pd;

	ping_group_destroy_data(media);

	FREE(priv);
	media->pd = NULL;

	/* XXX: How can we free this? Should media->name really be const?
	 * And on the same topic, how are media unregistered / freed ? */
	/*
	tmp = (char *)media->name;
	FREE(tmp);
	media->name = NULL;
	*/
}

/*
 *	Close UDP/IP broadcast heartbeat interface
 */

static int
ping_group_close(struct hb_media* mp)
{
	ping_group_private_t * ei;
	int	rc = HA_OK;

	PINGGROUPASSERT(mp);
	ei = (ping_group_private_t *) mp->pd;

	if (ei->sock >= 0) {
		if (close(ei->sock) < 0) {
			rc = HA_FAIL;
		}
	}

	ping_group_destroy_data(mp);
	return(rc);
}

/*
 * Receive a heartbeat ping reply packet.
 */

static struct ha_msg *
ping_group_read(struct hb_media* mp)
{
	ping_group_private_t *	ei;
	union {
		char		cbuf[MAXLINE+ICMP_HDR_SZ];
		struct ip	ip;
	}buf;
	const char *		bufmax = ((char *)&buf)+sizeof(buf);
	char *			msgstart;
	int			addr_len = sizeof(struct sockaddr);
   	struct sockaddr_in	their_addr; /* connector's addr information */
	struct ip *		ip;
	struct icmp		icp;
	int			numbytes;
	int			hlen;
	int 			seq;
	size_t			slotn;
	ping_group_node_t	*node;
	struct ha_msg		*msg = NULL;
	const char 		*comment;

	PINGGROUPASSERT(mp);
	ei = (ping_group_private_t *) mp->pd;

	if ((numbytes=recvfrom(ei->sock, (void *) &buf.cbuf
	,	sizeof(buf.cbuf)-1, 0,	(struct sockaddr *)&their_addr
	,	&addr_len)) < 0) {
		if (errno != EINTR) {
			LOG(PIL_CRIT, "Error receiving from socket: %s"
			,	strerror(errno));
		}
		return(NULL);
	}
	/* Avoid potential buffer overruns */
	buf.cbuf[numbytes] = EOS;

	/* Check the IP header */
	ip = &buf.ip;
	hlen = ip->ip_hl * 4;

	if (numbytes < hlen + ICMP_MINLEN) {
		LOG(PIL_WARN, "ping packet too short (%d bytes) from %s"
		,	numbytes
		,	inet_ntoa(*(struct in_addr *)
		&		their_addr.sin_addr.s_addr));
		return(NULL);
	}

	/* Now the ICMP part */	/* (there may be a better way...) */
	memcpy(&icp, (buf.cbuf + hlen), sizeof(icp));

	if (icp.icmp_type != ICMP_ECHOREPLY || icp.icmp_id != ei->ident) {
		return(NULL);
	}
	seq = ntohs(icp.icmp_seq);

	if (DEBUGPKT) {
		LOG(PIL_DEBUG, "got %d byte packet from %s"
		,	numbytes, inet_ntoa(their_addr.sin_addr));
	}
	msgstart = (buf.cbuf + hlen + ICMP_HDR_SZ);

	if (DEBUGPKTCONT && numbytes > 0) {
		LOG(PIL_DEBUG, "%s", msgstart);
	}

	for(node = ei->node; node; node = node->next) {
		if(!memcmp(&(their_addr.sin_addr), &(node->addr.sin_addr),
					sizeof(struct in_addr))) {
			break;
		}
	}
	if(!node) {
		return(NULL);
	}

	msg = string2msg(msgstart, bufmax - msgstart);
	if(msg == NULL) {
		return(NULL);
	}
	comment = ha_msg_value(msg, F_COMMENT);
	if(comment == NULL || strcmp(comment, PIL_PLUGIN_S)) {
		ha_msg_del(msg);
		return(NULL);
	}

	slotn = seq % NSLOT;
	if(seq > ei->iseq) {
		/* New Sequins ! */
		ei->iseq = seq;
		ei->slot[slotn] = 0;
	}
	else if(seq < ei->iseq - NSLOT) {
		/* Sequence is too old */
		ha_msg_del(msg);
		return(NULL);
	}

	if(!ei->slot[slotn]++) {
		/* First response responded */
		return(msg);
	}
	
	ha_msg_del(msg);
	return(NULL);
}

/*
 * Send a heartbeat packet over broadcast UDP/IP interface
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
ping_group_write(struct hb_media* mp, struct ha_msg * msg)
{
	ping_group_private_t *	ei;
	int			rc;
	char*			pkt;
	union{
		char*			buf;
		struct icmp		ipkt;
	}*icmp_pkt;
	int			size;
	struct icmp *		icp;
	int			pktsize;
	const char *		type;
	const char *		ts;
	struct ha_msg *		nmsg;
	ping_group_node_t *	node;
	struct timespec		pause = {0, 1};

	PINGGROUPASSERT(mp);
	ei = (ping_group_private_t *) mp->pd;
	type = ha_msg_value(msg, F_TYPE);

	if (type == NULL || strcmp(type, T_STATUS) != 0 
	|| ((ts = ha_msg_value(msg, F_TIME)) == NULL)) {
		return HA_OK;
	}

	/*
	 * We populate the following fields in the packet we create:
	 *
	 * F_TYPE:	T_NS_STATUS
	 * F_STATUS:	ping
	 * F_COMMENT:	ping_group
	 * F_ORIG:	destination name
	 * F_TIME:	local timestamp (from "msg")
	 * F_AUTH:	added by add_msg_auth()
	 */
	if ((nmsg = ha_msg_new(5)) == NULL) {
		LOG(PIL_CRIT, "cannot create new message");
		return(HA_FAIL);
	}

	if (ha_msg_add(nmsg, F_TYPE, T_NS_STATUS) != HA_OK
	||	ha_msg_add(nmsg, F_STATUS, PINGSTATUS) != HA_OK
	||	ha_msg_add(nmsg, F_COMMENT, PIL_PLUGIN_S) != HA_OK
	||	ha_msg_add(nmsg, F_ORIG, mp->name) != HA_OK
	||	ha_msg_add(nmsg, F_TIME, ts) != HA_OK) {
		ha_msg_del(nmsg); nmsg = NULL;
		LOG(PIL_CRIT, "cannot add fields to message");
		return HA_FAIL;
	}

	if (add_msg_auth(nmsg) != HA_OK) {
		LOG(PIL_CRIT, "cannot add auth field to message");
		ha_msg_del(nmsg); nmsg = NULL;
		return HA_FAIL;
	}

	if ((pkt = msg2string(nmsg)) == NULL)  {
		LOG(PIL_CRIT, "cannot convert message to string");
		return HA_FAIL;
	}
	ha_msg_del(nmsg); nmsg = NULL;

	size = strlen(pkt)+1;

	pktsize = size + ICMP_HDR_SZ;

	if ((icmp_pkt = MALLOC(size + ICMP_HDR_SZ)) == NULL) {
		LOG(PIL_CRIT, "out of memory");
		ha_free(pkt);
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

	for(node = ei->node; node; node = node->next) {
		if ((rc=sendto(ei->sock, (void *) icmp_pkt, pktsize, 0
		,	(struct sockaddr *)&node->addr
		,	sizeof(struct sockaddr))) != pktsize) {
			LOG(PIL_CRIT, "Error sending packet: %s"
			,	strerror(errno));
			FREE(icmp_pkt);
			return(HA_FAIL);
		}

		if (DEBUGPKT) {
			LOG(PIL_DEBUG, "sent %d bytes to %s"
			,	rc, inet_ntoa(node->addr.sin_addr));
   		}

		nanosleep(&pause, NULL);
	}

	if (DEBUGPKTCONT) {
		LOG(PIL_DEBUG, pkt);
   	}

	FREE(icmp_pkt);
	return HA_OK;
}

/*
 *	Open ping socket.
 */

static int
ping_group_open(struct hb_media* mp)
{
	ping_group_private_t * ei;
	int sockfd;
	struct protoent *proto;

	PINGGROUPASSERT(mp);
	ei = (ping_group_private_t *) mp->pd;


	if ((proto = getprotobyname("icmp")) == NULL) {
		LOG(PIL_CRIT, "protocol ICMP is unknown: %s", strerror(errno));
		return HA_FAIL;
	}
	if ((sockfd = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
		LOG(PIL_CRIT, "Can't open RAW socket.: %s", strerror(errno));
		return HA_FAIL;
    	}

	if (fcntl(sockfd, F_SETFD, FD_CLOEXEC)) {
		LOG(PIL_CRIT, "Error setting the close-on-exec flag: %s"
		,	strerror(errno));
	}
	ei->sock = sockfd;

	LOG(LOG_NOTICE, "ping group heartbeat started.");
	return HA_OK;
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 *	This function taken from Mike Muuss' ping program.
 */
static int
in_cksum (u_short *addr, int len)
{
	int		nleft = len;
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


/* mcast_parse will parse the line in the config file that is 
 * associated with the media's type (hb_dev_mtype).  It should 
 * receive the rest of the line after the mtype.  And it needs
 * to call hb_dev_new, add the media to the list of available media.
 *
 * So in this case, the config file line should look like
 * mcast [device] [mcast group] [port] [mcast ttl] [mcast loop]
 * for example:
 * mcast eth0 225.0.0.1 694 1 0
 */

static int
ping_group_parse(const char *line)
{
	char		tmp[MAXLINE];
	size_t		len;
	size_t		nhost = 0;
	struct hb_media *media;

	/* Skip over white space, then grab the name */
	line += strspn(line, WHITESPACE);
	len = strcspn(line, WHITESPACE);
	strncpy(tmp, line, len);
	line += len;
	*(tmp+len) = EOS;

	if(*tmp == EOS) {
		return(HA_FAIL);
	}

	media = ping_group_new(tmp);
	if (!media) {
		return(HA_FAIL);
	}

	while(1) {
		/* Skip over white space, then grab the host */
		line += strspn(line, WHITESPACE);
		len = strcspn(line, WHITESPACE);
		strncpy(tmp, line, len);
		line += len;
		*(tmp+len) = EOS;

		if(*tmp == EOS) {
			break;
		}

		if(ping_group_add_node(media, tmp) < 0) {
			ping_group_destroy(media);
			return(HA_FAIL);
		}
		nhost++;
	}

	if(nhost == 0) {
		ping_group_destroy(media);
		return(HA_FAIL);
	}
		
	OurImports->RegisterNewMedium(media);

	return(HA_OK);
}

