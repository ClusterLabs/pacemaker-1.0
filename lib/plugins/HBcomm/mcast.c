/*
 * mcast.c: implements hearbeat API for UDP multicast communication
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 * Copyright (C) 2000 Chris Wright <chris@wirex.com>
 *
 * Thanks to WireX for providing hardware to test on.
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
#include <net/if.h>
#include <sys/ioctl.h>

#ifdef HAVE_SYS_SOCKIO_H
#	include <sys/sockio.h>
#endif

#include <HBcomm.h>
 
#define PIL_PLUGINTYPE          HB_COMM_TYPE
#define PIL_PLUGINTYPE_S        HB_COMM_TYPE_S
#define PIL_PLUGIN              mcast
#define PIL_PLUGIN_S            "mcast"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL
#include <pils/plugin.h>
#include <heartbeat.h>

struct mcast_private {
	char *  interface;      /* Interface name */
	struct  in_addr mcast;  /* multicast address */
	struct  sockaddr_in   addr;   /* multicast addr */
	u_short port;
	int     rsocket;        /* Read-socket */
	int     wsocket;        /* Write-socket */
	u_char	ttl;		/* TTL value for outbound packets */
	u_char	loop;		/* boolean, loop back outbound packets */
};


static int		mcast_parse(const char* configline);
static struct hb_media * mcast_new(const char * intf, const char *mcast
			,	u_short port, u_char ttl, u_char loop);
static int		mcast_open(struct hb_media* mp);
static int		mcast_close(struct hb_media* mp);
static void*		mcast_read(struct hb_media* mp, int* lenp);
static int		mcast_write(struct hb_media* mp, void* p, int len);
static int		mcast_descr(char** buffer);
static int		mcast_mtype(char** buffer);
static int		mcast_isping(void);


static struct hb_media_fns mcastOps ={
	NULL,		/* Create single object function */
	mcast_parse,	/* whole-line parse function */
	mcast_open,
	mcast_close,
	mcast_read,
	mcast_write,
	mcast_mtype,
	mcast_descr,
	mcast_isping,
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
	,	&mcastOps
	,	NULL		/*close */
	,	&OurInterface
	,	(void*)&OurImports
	,	interfprivate); 
}


/* helper functions */
static int mcast_make_receive_sock(struct hb_media* hbm);
static int mcast_make_send_sock(struct hb_media * hbm);
static struct mcast_private *
new_mcast_private(const char *ifn, const char *mcast, u_short port,
		u_char ttl, u_char loop);
static int set_mcast_if(int sockfd, char *ifname);
static int set_mcast_loop(int sockfd, u_char loop);
static int set_mcast_ttl(int sockfd, u_char ttl);
static int join_mcast_group(int sockfd, struct in_addr *addr, char *ifname);
static int if_getaddr(const char *ifname, struct in_addr *addr);
static int is_valid_dev(const char *dev);
static int is_valid_mcast_addr(const char *addr);
static int get_port(const char *port, u_short *p);
static int get_ttl(const char *ttl, u_char *t);
static int get_loop(const char *loop, u_char *l);


#define		ISMCASTOBJECT(mp) ((mp) && ((mp)->vf == (void*)&mcastOps))
#define		MCASTASSERT(mp)	g_assert(ISMCASTOBJECT(mp))

static int
mcast_mtype(char** buffer)
{ 
	*buffer = STRDUP(PIL_PLUGIN_S);
	if (!*buffer) {
		return 0;
	}

	return STRLEN_CONST(PIL_PLUGIN_S);
}

static int
mcast_descr(char **buffer)
{ 
	const char cret[] = "UDP/IP multicast";
	*buffer = STRDUP(cret);
	if (!*buffer) {
		return 0;
	}

	return STRLEN_CONST(cret);
}

static int
mcast_isping(void)
{
	/* nope, this is not a ping device */
	return 0;
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
mcast_parse(const char *line)
{
	const char *		bp = line;
	char			dev[MAXLINE];
	char			mcast[MAXLINE];
	char			token[MAXLINE];
	u_short			port = 0;	/* Bogus */
	u_char			ttl = 10;	/* Bogus */
	u_char			loop = 10;	/* Bogus */
	int			toklen;
	struct hb_media *	mp;

	/* Skip over white space, then grab the device */
	bp += strspn(bp, WHITESPACE);
	toklen = strcspn(bp, WHITESPACE);
	strncpy(dev, bp, toklen);
	bp += toklen;
	dev[toklen] = EOS;

	if (*dev != EOS)  {
		if (!is_valid_dev(dev)) {
			PILCallLog(LOG, PIL_CRIT, "mcast device [%s] is invalid or not set up properly", dev);
			return HA_FAIL;
		}
		/* Skip over white space, then grab the multicast group */
		bp += strspn(bp, WHITESPACE);
		toklen = strcspn(bp, WHITESPACE);
		strncpy(mcast, bp, toklen);
		bp += toklen;
		mcast[toklen] = EOS;
	
		if (*mcast == EOS)  {
			PILCallLog(LOG, PIL_CRIT, "mcast [%s] missing mcast address",
				dev);
			return(HA_FAIL);
		}
		if (!is_valid_mcast_addr(mcast)) {
			PILCallLog(LOG, PIL_CRIT, "mcast [%s] bad addr [%s]", dev, mcast);
			return(HA_FAIL);
		}

		/* Skip over white space, then grab the port */
		bp += strspn(bp, WHITESPACE);
		toklen = strcspn(bp, WHITESPACE);
		strncpy(token, bp, toklen);
		bp += toklen;
		token[toklen] = EOS;

		if (*token == EOS)  {
			PILCallLog(LOG, PIL_CRIT, "mcast [%s] missing port"
			,	dev);
			return(HA_FAIL);
		}
		if (get_port(token, &port) < 0 || port <= 0) {
			PILCallLog(LOG, PIL_CRIT, " mcast [%s] bad port [%d]", dev, port);
			return HA_FAIL;
		}

		/* Skip over white space, then grab the ttl */
		bp += strspn(bp, WHITESPACE);
		toklen = strcspn(bp, WHITESPACE);
		strncpy(token, bp, toklen);
		bp += toklen;
		token[toklen] = EOS;

		if (*token == EOS)  {
			PILCallLog(LOG, PIL_CRIT, "mcast [%s] missing ttl", dev);
			return(HA_FAIL);
		}
		if (get_ttl(token, &ttl) < 0 || ttl > 4) {
			PILCallLog(LOG, PIL_CRIT, " mcast [%s] bad ttl [%d]", dev, ttl);
			return HA_FAIL;
		}

		/* Skip over white space, then grab the loop */
		bp += strspn(bp, WHITESPACE);
		toklen = strcspn(bp, WHITESPACE);
		strncpy(token, bp, toklen);
		bp += toklen;
		token[toklen] = EOS;

		if (*token == EOS)  {
			PILCallLog(LOG, PIL_CRIT, "mcast [%s] missing loop", dev);
			return(HA_FAIL);
		}
		if (get_loop(token, &loop) < 0 ||	loop > 1) {
			PILCallLog(LOG, PIL_CRIT, " mcast [%s] bad loop [%d]", dev, loop);
			return HA_FAIL;
		}

		if ((mp = mcast_new(dev, mcast, port, ttl, loop)) == NULL) {
			return(HA_FAIL);
		}
		OurImports->RegisterNewMedium(mp);
	}

	return(HA_OK);
}

/*
 * Create new UDP/IP multicast heartbeat object 
 * pass in name of interface, multicast address, port, multicast
 * ttl, and multicast loopback value as parameters.
 * This should get called from hb_dev_parse().
 */
static struct hb_media *
mcast_new(const char * intf, const char *mcast, u_short port,
		    u_char ttl, u_char loop)
{
	struct mcast_private*	mcp;
	struct hb_media *	ret;

	/* create new mcast_private struct...hmmm...who frees it? */
	mcp = new_mcast_private(intf, mcast, port, ttl, loop);
	if (mcp == NULL) {
		PILCallLog(LOG, PIL_WARN, "Error creating mcast_private(%s, %s, %d, %d, %d)",
			 intf, mcast, port, ttl, loop);
		return(NULL);
	}
	ret = (struct hb_media*) MALLOC(sizeof(struct hb_media));
	if (ret != NULL) {
		char * name;
		ret->pd = (void*)mcp;
		name = STRDUP(intf);
		if (name != NULL) {
			ret->name = name;
		}
		else {
			FREE(ret);
			ret = NULL;
		}

	}
	if(ret == NULL) {
		FREE(mcp->interface);
		FREE(mcp);
	}
	return(ret);
}

/*
 *	Open UDP/IP multicast heartbeat interface
 */
static int
mcast_open(struct hb_media* hbm)
{
	struct mcast_private * mcp;

	MCASTASSERT(hbm);
	mcp = (struct mcast_private *) hbm->pd;

	if ((mcp->wsocket = mcast_make_send_sock(hbm)) < 0) {
		return(HA_FAIL);
	}
	if ((mcp->rsocket = mcast_make_receive_sock(hbm)) < 0) {
		mcast_close(hbm);
		return(HA_FAIL);
	}

	PILCallLog(LOG, PIL_INFO, "UDP multicast heartbeat started for group %s "
		"port %d interface %s (ttl=%d loop=%d)" , inet_ntoa(mcp->mcast),
		mcp->port, mcp->interface, mcp->ttl, mcp->loop);

	return(HA_OK);
}

/*
 *	Close UDP/IP multicast heartbeat interface
 */
static int
mcast_close(struct hb_media* hbm)
{
	struct mcast_private * mcp;
	int	rc = HA_OK;

	MCASTASSERT(hbm);
	mcp = (struct mcast_private *) hbm->pd;

	if (mcp->rsocket >= 0) {
		if (close(mcp->rsocket) < 0) {
			rc = HA_FAIL;
		}
	}
	if (mcp->wsocket >= 0) {
		if (close(mcp->wsocket) < 0) {
			rc = HA_FAIL;
		}
	}
	return(rc);
}

/*
 * Receive a heartbeat multicast packet from UDP interface
 */

char			mcast_pkt[MAXMSG];
static void *
mcast_read(struct hb_media* hbm, int *lenp)
{
	struct mcast_private *	mcp;
	socklen_t		addr_len = sizeof(struct sockaddr);
   	struct sockaddr_in	their_addr; /* connector's addr information */
	int	numbytes;

	MCASTASSERT(hbm);
	mcp = (struct mcast_private *) hbm->pd;
	
	if ((numbytes=recvfrom(mcp->rsocket, mcast_pkt, MAXMSG-1, 0
	,	(struct sockaddr *)&their_addr, &addr_len)) < 0) {
		if (errno != EINTR) {
			PILCallLog(LOG, PIL_CRIT, "Error receiving from socket: %s"
			    ,	strerror(errno));
		}
		return NULL;
	}
	/* Avoid possible buffer overruns */
	mcast_pkt[numbytes] = EOS;
	
	if (Debug >= PKTTRACE) {
		PILCallLog(LOG, PIL_DEBUG, "got %d byte packet from %s"
		    ,	numbytes, inet_ntoa(their_addr.sin_addr));
	}
	if (Debug >= PKTCONTTRACE && numbytes > 0) {
		PILCallLog(LOG, PIL_DEBUG, "%s", mcast_pkt);
	}
	
	*lenp = numbytes + 1 ;

	return mcast_pkt;;
}

/*
 * Send a heartbeat packet over multicast UDP/IP interface
 */

static int
mcast_write(struct hb_media* hbm, void *pkt, int len)
{
	struct mcast_private *	mcp;
	int			rc;
	
	MCASTASSERT(hbm);
	mcp = (struct mcast_private *) hbm->pd;

	if ((rc=sendto(mcp->wsocket, pkt, len, MSG_DONTWAIT
	,	(struct sockaddr *)&mcp->addr
	,	sizeof(struct sockaddr))) != len) {
		PILCallLog(LOG, PIL_CRIT, "Unable to send mcast packet [%d]: %s"
		,	rc, strerror(errno));
		return(HA_FAIL);
	}
	
	if (Debug >= PKTTRACE) {
		PILCallLog(LOG, PIL_DEBUG, "sent %d bytes to %s"
		    ,	rc, inet_ntoa(mcp->addr.sin_addr));
   	}
	if (Debug >= PKTCONTTRACE) {
		PILCallLog(LOG, PIL_DEBUG, "%s", (const char *)pkt);
   	}
	return(HA_OK);
}

/*
 * Set up socket for sending multicast UDP heartbeats
 */

static int
mcast_make_send_sock(struct hb_media * hbm)
{
	int sockfd;
	struct mcast_private * mcp;
	MCASTASSERT(hbm);
	mcp = (struct mcast_private *) hbm->pd;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		PILCallLog(LOG, PIL_WARN, "Error getting socket: %s", strerror(errno));
		return(sockfd);
   	}

	if (set_mcast_if(sockfd, mcp->interface) < 0) {
		PILCallLog(LOG, PIL_WARN, "Error setting outbound mcast interface: %s", strerror(errno));
	}

	if (set_mcast_loop(sockfd, mcp->loop) < 0) {
		PILCallLog(LOG, PIL_WARN, "Error setting outbound mcast loopback value: %s", strerror(errno));
	}

	if (set_mcast_ttl(sockfd, mcp->ttl) < 0) {
		PILCallLog(LOG, PIL_WARN, "Error setting outbound mcast TTL: %s", strerror(errno));
	}

	if (fcntl(sockfd,F_SETFD, FD_CLOEXEC)) {
		PILCallLog(LOG, PIL_WARN, "Error setting the close-on-exec flag: %s", strerror(errno));
	}
	return(sockfd);
}

/*
 * Set up socket for listening to heartbeats (UDP multicasts)
 */

#define	MAXBINDTRIES	50
static int
mcast_make_receive_sock(struct hb_media * hbm)
{

	struct mcast_private * mcp;
	int	sockfd;
	int	bindtries;
	int	boundyet=0;
	int	one=1;
	int	rc;
	int	binderr=0;

	MCASTASSERT(hbm);
	mcp = (struct mcast_private *) hbm->pd;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		PILCallLog(LOG, PIL_CRIT, "Error getting socket");
		return -1;
	}
	/* set REUSEADDR option on socket so you can bind a multicast */
	/* reader to multiple interfaces */
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one)) < 0){
		PILCallLog(LOG, PIL_CRIT, "Error setsockopt(SO_REUSEADDR)");
	}        

	/* ripped off from udp.c, if we all use SO_REUSEADDR */
	/* this shouldn't be necessary  */
	/* Try binding a few times before giving up */
	/* Sometimes a process with it open is exiting right now */

	for(bindtries=0; !boundyet && bindtries < MAXBINDTRIES; ++bindtries) {
		rc=bind(sockfd, (struct sockaddr *)&mcp->addr, sizeof(mcp->addr));
		binderr=errno;
		if (rc==0) {
			boundyet=1;
		} else if (rc == -1) {
			if (binderr == EADDRINUSE) {
				PILCallLog(LOG, PIL_CRIT, "Can't bind (EADDRINUSE), "
					"retrying");
				sleep(1);
			} else	{ 
			/* don't keep trying if the error isn't caused by */
			/* the address being in use already...real error */
				break;
			}
		}
	}
	if (!boundyet) {
		if (binderr == EADDRINUSE) {
			/* This happens with multiple udp or ppp interfaces */
			PILCallLog(LOG, PIL_INFO
			,	"Someone already listening on port %d [%s]"
			,	mcp->port
			,	mcp->interface);
			PILCallLog(LOG, PIL_INFO, "multicast read process exiting");
			close(sockfd);
			cleanexit(0);
		} else {
			PILCallLog(LOG, PIL_WARN, "Unable to bind socket. Giving up: %s", strerror(errno));
			close(sockfd);
			return(-1);
		}
	}
	/* join the multicast group...this is what really makes this a */
	/* multicast reader */
	if (join_mcast_group(sockfd, &mcp->mcast, mcp->interface) == -1) {
		PILCallLog(LOG, PIL_CRIT, "Can't join multicast group %s on interface %s",
			inet_ntoa(mcp->mcast), mcp->interface);
		PILCallLog(LOG, PIL_INFO, "multicast read process exiting");
		close(sockfd);
		cleanexit(0);
	}
	if (ANYDEBUG) 
		PILCallLog(LOG, PIL_DEBUG, "Successfully joined multicast group %s on"
			"interface %s", inet_ntoa(mcp->mcast), mcp->interface);
		
	if (fcntl(sockfd,F_SETFD, FD_CLOEXEC)) {
		PILCallLog(LOG, PIL_WARN, "Error setting the close-on-exec flag: %s", strerror(errno));
	}
	return(sockfd);
}

static struct mcast_private *
new_mcast_private(const char *ifn, const char *mcast, u_short port,
		u_char ttl, u_char loop)
{
	struct mcast_private *mcp;

	mcp = MALLOCT(struct mcast_private);
	if (mcp == NULL)  {
		return NULL;
	}

	mcp->interface = (char *)STRDUP(ifn);
	if(mcp->interface == NULL) {
		FREE(mcp);
		return NULL;
	}

	/* Set up multicast address */

	if (inet_pton(AF_INET, mcast, (void *)&mcp->mcast) <= 0) {
		FREE(mcp->interface);
		FREE(mcp);
		return NULL;
	}

	memset(&mcp->addr, 0, sizeof(mcp->addr));	/* zero the struct */
	mcp->addr.sin_family = AF_INET;		/* host byte order */
	mcp->addr.sin_port = htons(port);	/* short, network byte order */
	mcp->addr.sin_addr = mcp->mcast;
	mcp->port = port;
	mcp->wsocket = -1;
	mcp->rsocket = -1;
	mcp->ttl=ttl;
	mcp->loop=loop;
	return(mcp);
}

/* set_mcast_loop takes a boolean flag, loop, which is useful on
 * a writing socket.  with loop enabled (the default on a multicast socket)
 * the outbound packet will get looped back and received by the sending
 * interface, if it is listening for the multicast group and port that the
 * packet was sent to.  Returns 0 on success -1 on failure.
 */
static int set_mcast_loop(int sockfd, u_char loop)
{
	return setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
}

/* set_mcast_ttl will set the time-to-live value for the writing socket.
 * the socket default is TTL=1.  The TTL is used to limit the scope of the
 * packet and can range from 0-255.  
 * TTL     Scope
 * ----------------------------------------------------------------------
 *    0    Restricted to the same host. Won't be output by any interface.
 *    1    Restricted to the same subnet. Won't be forwarded by a router.
 *  <32    Restricted to the same site, organization or department.
 *  <64    Restricted to the same region.
 * <128    Restricted to the same continent.
 * <255    Unrestricted in scope. Global.
 *
 * Returns 0 on success -1 on failure.
 */
static int
set_mcast_ttl(int sockfd, u_char ttl)
{
	return setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
}

/*
 * set_mcast_if takes the name of an interface (i.e. eth0) and then
 * sets that as the interface to use for outbound multicast traffic.
 * If ifname is NULL, then it the OS will assign the interface.
 * Returns 0 on success -1 on faliure.
 */
static int
set_mcast_if(int sockfd, char *ifname)
{
	int rc;
	struct in_addr addr;

	/* Zero out the struct... we only care about the address... */
	memset(&addr, 0, sizeof(addr));

	rc = if_getaddr(ifname, &addr);
	if (rc == -1)
		return -1;
	return setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF
	,	(void*)&addr, sizeof(addr));
}

/* join_mcast_group is used to join a multicast group.  the group is
 * specified by a class D multicast address 224.0.0.0/8 in the in_addr
 * structure passed in as a parameter.  The interface name can be used
 * to "bind" the multicast group to a specific interface (or any
 * interface if ifname is NULL);
 * returns 0 on success, -1 on failure.
 */
static int
join_mcast_group(int sockfd, struct in_addr *addr, char *ifname)
{
	struct ip_mreq	mreq_add;

	memset(&mreq_add, 0, sizeof(mreq_add));
	memcpy(&mreq_add.imr_multiaddr, addr, sizeof(struct in_addr));

	if (ifname) {
		if_getaddr(ifname, &mreq_add.imr_interface);
	}
	return setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq_add, sizeof(mreq_add));
}

/* if_getaddr gets the ip address from an interface
 * specified by name and places it in addr.
 * returns 0 on success and -1 on failure.
 */
static int
if_getaddr(const char *ifname, struct in_addr *addr)
{
	struct ifreq	if_info;
	int		j;
	int		maxtry = 120;
	gboolean	gotaddr = FALSE;
	int		err = 0;
	
	if (!addr) {
		return -1;
	}

	addr->s_addr = INADDR_ANY;

	memset(&if_info, 0, sizeof(if_info));
	if (ifname) {
		strncpy(if_info.ifr_name, ifname, IFNAMSIZ-1);
	}else{	/* ifname is NULL, so use any address */
		return 0;
	}

	if (Debug > 0) {
		PILCallLog(LOG, PIL_DEBUG, "looking up address for %s"
		,	if_info.ifr_name);
	}
	for (j=0; j < maxtry && !gotaddr; ++j) {
		int		fd;
		if ((fd=socket(AF_INET, SOCK_DGRAM, 0)) == -1)	{
			PILCallLog(LOG, PIL_CRIT, "Error getting socket");
			return -1;
		}
		if (ioctl(fd, SIOCGIFADDR, &if_info) >= 0) {
			gotaddr = TRUE;
		}else{
			err = errno;
			switch(err) {
				case EADDRNOTAVAIL:
					sleep(1);
					break;	
				default:
					close(fd);
					goto getout;
			}
		}
		close(fd);
	}
getout:
	if (!gotaddr) {
		PILCallLog(LOG, PIL_CRIT
		,	"Unable to retrieve local interface address"
		" for interface [%s] using ioctl(SIOCGIFADDR): %s"
		,	ifname, strerror(err));
		return -1;
	}

	/*
	 * This #define w/void cast is to quiet alignment errors on some
	 * platforms (notably Solaris)
	 */
#define SOCKADDR_IN(a)        ((struct sockaddr_in *)((void*)(a)))
 
	memcpy(addr, &(SOCKADDR_IN(&if_info.ifr_addr)->sin_addr)
	,	sizeof(struct in_addr));

	return 0;
}

/* returns true or false */
static int
is_valid_dev(const char *dev)
{
	int rc=0;
	if (dev) {
		struct in_addr addr;
		if (if_getaddr(dev, &addr) != -1)
			rc = 1;
	}
	return rc;
}

/* returns true or false */
#define MCAST_NET	0xf0000000
#define MCAST_BASE	0xe0000000
static int
is_valid_mcast_addr(const char *addr)
{
	unsigned long mc_addr;

	/* make sure address is in host byte order */
	mc_addr = ntohl(inet_addr(addr));

	if ((mc_addr & MCAST_NET) == MCAST_BASE)
		return 1;

	return 0;
}

/* return port number on success, 0 on failure */
static int
get_port(const char *port, u_short *p)
{
	/* not complete yet */
	*p=(u_short)atoi(port);
	return 0;
}

/* returns ttl on succes, -1 on failure */
static int
get_ttl(const char *ttl, u_char *t)
{
	/* not complete yet */
	*t=(u_char)atoi(ttl);
	return 0;
}

/* returns loop on success, -1 on failure */
static int
get_loop(const char *loop, u_char *l)
{
	/* not complete yet */
	*l=(u_char)atoi(loop);
	return 0;
}

