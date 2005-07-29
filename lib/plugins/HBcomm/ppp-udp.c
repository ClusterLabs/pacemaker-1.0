/* $Id: ppp-udp.c,v 1.17 2005/07/29 07:18:55 sunjd Exp $ */
/*
 *	ppp-udp.c:	Implements UDP over PPP for bidirectional ring
 *			heartbeats.
 *
 * NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
 *
 *	This code is deprecated, never worked wonderfully, and is now
 *	removed from the build process.  Don't expect it to work.  It has been
 *	retained for historical reasons, and as a graphic explanation of why
 *	one should use the straight serial code and not write a ppp module
 *	again ;-)
 *
 *	STRONG CAUTION:
 *	It also has known sprintf-related security bugs which we won't fix
 *	because we're not maintaining this file.  It probably also no longer
 *	compiles against everything else in the system.
 *
 * NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
 *
 * Copyright (C) 1999, 2000, 2001 Alan Robertson <alanr@unix.sh>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *
 *	ppp-udp.c:	Implements UDP over PPP for bidirectional ring
 *			heartbeats.
 *
 *	This is basically the same kind of stuff that occurs in serial.c,
 *	except we start up PPP on the links to ensure that data doesn't get
 *	mangled or lost.
 *
 *	It is also similar to the stuff in udp.c, because the links are
 *	UDP/IP.  The difference is that we have to start (and restart) PPP
 *	over the serial links, and we don't use broadcast (because there's
 *	only one address on the "net").  As before, thanks to Tom Vogt for
 *	the prototype code in udp.c on which this is based.
 *
 *	But unlike both of those two methods, this one is a real pain in the
 *	neck, because of the interactions with starting, stopping and hanging
 *	pppd processes.  The current version appears to work reasonably well
 *	with respect to various kinds of problems, but appears to still have
 *	problems.  As a result, this method is not recommended.
 *
 *	Addressing:
 *	We require that all the PPP links be given private (non-routable)
 *	IP addresses.  These addresses are described by RFC 1918
 *	(which obsoletes RFC 1597).
 *
 *	In modern parlance these are described as:
 *		10.0.0.0/8
 *		172.16.0.0/12
 *		192.168.0.0/16
 *
 *	It's stupid (and probably a mistake) to use routable addresses, since
 *	routing to one of these addresses is definitely an error, and there are
 *	plenty of these to go around...
 *
 *	Writer:
 *	The writer calls open_write() if it sees it has no socket.  If it gets
 *	a write error, it closes its socket, and calls open_write() again.
 *
 *	Reader:
 *	The reader calls open_read() if it sees it has no socket. When the
 *	reader gets an error, it closes its socket, and calls open_read again.
 *
 *	open_write:
 *		Looks for unwanted ppp, kills it, removes ppp-start file.
 *		Forks off a pppd. It succeeds when the ppp-start file
 *		appears.  It normally fails on the first call
 *		The writer will try to open on each packet it is given
 *		to write, until it succeeds.
 *	open_read:
 *		returns failure if the ppp-start file doesn't exist.
 *		If it does, then it will open the read socket.
 *		It normally fails on it's first call.
 *		The caller is expected to loop (with sleep) until open succeeds.
 *		Note that it relies on open_write to start pppd.
 */

#define	PPPD		"/usr/sbin/pppd"
#define SHELL		"/bin/sh"	/* Must support sh -c */
#define	PPP_OPTS	"noauth lcp-echo-failure 3 lcp-echo-interval 2 nodefaultroute nodetach"

#define	ERRTHRESH	3	/* Retry known write failures this many times */
#define	REFUSEDRESTART	300	/* Probably about 5 minutes worth of time */
				/* This tells us how many writes should fail */
				/* with connection refused before we restart */
				/* PPPd on our end */
#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#if defined(SO_BINDTODEVICE)
#	include <net/if.h>
#endif
#include <heartbeat.h>
#include <clplumbing/cl_signal.h>

struct ip_private {
	struct hb_media *next;		/* Next UDP/PPP interface */
	char *		ipaddr;		/* The (local) IP address we use */
        char *		interface;      /* Interface name */
        char *		far_addr;	/* Far end address as a string */
	int		ppp_started;	/* Have we started pppd yet? */
	int		ppp_pid;	/* PPP process id */
        struct hostent  farhost;	/* FarEnd address */
        struct sockaddr_in addr;	/* Far End addr */
        int		port;		/* What port is this on? */
        int		rsocket;        /* Read-socket */
        int		wsocket;        /* Write-socket */
};

struct ip_private*	pppref = NULL;
#define	PPPCOUNT	30	/* Restart after 30 secs w/o messages */
static int		ppp_countdown = PPPCOUNT;


static int	hb_dev_init(void);
static void	ppp_localdie(void);
static struct hb_media*
		hb_dev_new(const char* tty, const char* ipaddr);
static int	hb_dev_parse(const char * line);
static int	hb_dev_open(struct hb_media* mp);
static int	hb_dev_close(struct hb_media* mp);
static struct ha_msg*
		hb_dev_read(struct hb_media* mp);
static int	hb_dev_write(struct hb_media* mp, struct ha_msg *msg);
static int	ppp_udp_make_receive_sock(struct hb_media* ei);
static int	ppp_udp_make_send_sock(struct hb_media * mp);
static int	ppp_udp_open_write(struct hb_media * mp);
static int	ppp_udp_open_read(struct hb_media * mp);
static char *	ppp_udp_ppp_start_path(struct hb_media * mp);
static int 	ppp_udp_ppp_proc_info(struct hb_media * mp);
static int 	ppp_udp_start_ppp(struct hb_media *mp);
static int	set_up_ip(struct hb_media* mp);
static int	is_valid_local_addr(const char * cp);
static int	is_valid_serial(const char * port);
static struct hb_media*
		last_udp_ppp_interface;
static void save_ppp_info(struct hb_media * mp);
static void check_ppp_info(int sig);
static int hb_dev_mtype (char **buffer);
static int hb_dev_descr (char **buffer);
static int hb_dev_isping (void);

extern int	udpport;	/* Shared with udp.c */

#define		ISUDPOBJECT(mp)	((mp) && ((mp)->vf == (void*)&ppp_udp_media_fns))
/*#define		PPPUDPASSERT(mp)	ASSERT(ISUDPOBJECT(mp)) */
#define PPPUDPASSERT(mp) 
static int hb_dev_mtype (char **buffer)
{
	*buffer = ha_strdup("ppp-udp");
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

static int hb_dev_descr (char **buffer)
{
	*buffer = ha_strdup("Serial ring running PPP/UDP");
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

static int hb_dev_isping (void) { 
	return 0;
}

static int
hb_dev_init(void)
{
	udpport = UDPPORT;
	last_udp_ppp_interface = NULL;
	return(HA_OK);
}

/*
 *	Create new PPP-UDP/IP heartbeat object 
 *	Name of interface is passed as a parameter
 */
static struct hb_media *
hb_dev_new(const char* tty, const char* ipaddr)
{
	struct ip_private*	ipi;
	struct hb_media *	ret;

	ipi = MALLOCT(struct ip_private);
	if (ipi == NULL) {
		ha_log(LOG_ERR, "Out of memory");
		return(NULL);
	}

	memset(ipi, 0, sizeof(*ipi));


	ipi->ipaddr = ha_strdup(ipaddr);
	if (!ipi->ipaddr) {
		ha_log(LOG_ERR, "Out of memory");
		ha_free(ipi);
		return NULL;
	}

        ipi->port = udpport;

	ret = MALLOCT(struct hb_media);
	if (ret != NULL) {
		char *	name;
		ret->pd = (void*)ipi;
		name = ha_strdup(tty);
		if (name == NULL)  {
			ha_free(ipi);
			ha_free(ipi->addr);
			ha_free(ret);
			ret=NULL;
			return(ret);
		}
		ret->name = name;
		ipi->next = last_udp_ppp_interface;
		ipi->next = ret;
		ipi->rsocket = ipi->wsocket = -1;
	}else{
		ha_log(LOG_ERR, "Out of memory");
		ha_free(ipi);
		ha_free(ipi->addr);
	}
	return(ret);
}

/*
 *	Parse the ppp-udp line we've been given as an argument.
 *	We aren't given the initial "ppp-udp" token.
 *	What we do have here though should look like this:
 *		/dev/ttySxxx IP-address /dev/xxx2 IP-address2 ...
 *	in pairs.
 *
 *	We call is_valid_local_addr() to validate the IP addresses we're given.
 *	It insists that they must be RFC-defined local addresses.
 */

static int
hb_dev_parse(const char * line)
{
	const char *	bp = line;

	while (*bp != EOS) {
		char	tty[MAXLINE];
		char	ip[MAXLINE];
		char	msg[MAXLINE];
		int	toklen;
		extern struct hb_media* sysmedia[];
		extern int		nummedia;
		struct hb_media *	mp;

		/* Skip over white space, then grab the tty name */
		bp += strspn(bp, WHITESPACE);
		toklen = strcspn(bp, WHITESPACE);
		strncpy(tty, bp, toklen);
		bp += toklen;
		tty[toklen] = EOS;

		if (*tty == EOS)  {
			break;
		}

		/* Skip over white space, then grab the IP address */
		bp += strspn(bp, WHITESPACE);
		toklen = strcspn(bp, WHITESPACE);
		strncpy(ip, bp, toklen);
		bp += toklen;
		ip[toklen] = EOS;

		if (*ip == EOS)  {
			sprintf(msg, "ppp-udp tty [%s]: missing IP address"
			,	tty);
			ha_log(LOG_ERR, msg);
			return(HA_FAIL);
		}
		if (!is_valid_serial(tty)) {
			return(HA_FAIL);
		}
		if (!is_valid_local_addr(ip)) {
			return(HA_FAIL);
		}
		if ((mp = hb_dev_new(tty, ip)) == NULL)  {
			return(HA_FAIL);
		}
		sysmedia[nummedia] = mp;
		++nummedia;
	}
	return(HA_OK);
}

#define DEVSLASH	"/dev/"
static char *
ppp_udp_ppp_start_path(struct hb_media * mp)
{
	static char	tmp [MAXLINE];
	static char	result [MAXLINE];
	const char *	rcp;
	char *		cp;

	if (strncmp(mp->name, DEVSLASH, sizeof(DEVSLASH)-1) == 0) {
		rcp = mp->name + sizeof(DEVSLASH)-1;
	}else{
		rcp = mp->name;
	}
	strcpy(tmp, rcp);

	for (cp = tmp; *cp != EOS; ++cp) {
		if (*cp == '/') {
			*cp = '.';
		}
	}

	sprintf(result, "%s/%s", PPP_D, tmp);
	return(result);
}

static int 
ppp_udp_ppp_proc_info(struct hb_media * mp)
{
	const char *	ppp_start_file = ppp_udp_ppp_start_path(mp);
	FILE *		fp;
	struct ip_private * ei;
	char		line[MAXLINE];
	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG, "Attempting to open %s", ppp_start_file);
   	}
	/*
	 *	This is the information we put in the start file...
	 *	$IPREMOTE
	 *	$IFNAME
	 *	$PPPD_PID
	 *	$IPLOCAL
	 */
	if ((fp=fopen(ppp_start_file, "r")) != NULL)  {
		if (ANYDEBUG) {
			ha_log(LOG_DEBUG, "%s opened.", ppp_start_file);
		}
		if (fgets(line, MAXLINE-1, fp) != NULL) {
			if (ei->far_addr != NULL)  {
				ha_free(ei->far_addr);
				ei->far_addr = NULL;
			}
			line[strlen(line)-1] = EOS;
			ei->far_addr = ha_strdup(line);
			if (!ei->far_addr) {
				return NULL;
			}
			if (ANYDEBUG) {
				ha_log(LOG_DEBUG, "addr=%s", line);
			}
		}
		if (fgets(line, MAXLINE-1, fp) != NULL) {
			if (ei->interface != NULL)  {
				ha_free(ei->interface);
				ei->interface = NULL;
			}
			line[strlen(line)-1] = EOS;
			ei->interface = ha_strdup(line);
			if (!ei->interface) {
				if (ei->far_addr) {
					ha_free(ei->far_addr);
				}
				return NULL;
			}
			if (ANYDEBUG) {
				ha_log(LOG_DEBUG, "if=%s", ei->interface);
			}
		}
		if (fgets(line, MAXLINE-1, fp) != NULL) {
			pid_t	pid = atoi(line);
			if (pid > 1) {
				ei->ppp_pid = pid;
			}
			if (ANYDEBUG) {
				ha_log(LOG_DEBUG, "pid=%d", ei->ppp_pid);
			}
		}
		if (ANYDEBUG) {
			ha_log(LOG_DEBUG, "closing %s.", ppp_start_file);
		}
		fclose(fp);
		return(HA_OK);
	}else{
		return(HA_FAIL);
	}
}

/*
 *	Open PPP-UDP/IP heartbeat interface
 */
static int
hb_dev_open(struct hb_media* mp)
{
	struct ip_private * ei;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	ei->wsocket = -1;
	ei->rsocket = -1;
	ha_log(LOG_NOTICE, "PPP/UDP heartbeat started on port %d tty %s"
	,	udpport, mp->name);
	return(HA_OK);
}
/*
 *	open_write:
 *	Try and set up socket for running PPPd process if any.
 *	If we haven't started one yet, we
 *		look for unwanted ppp, kills it, removes ppp-start file.
 *		fork off a pppd
 *	If the PPP link isn't up yet, we return failure.
 *		Otherwise we open it up with ppp_udp_make_send_sock()
 *	It's up to the application to retry.
 */
static int
ppp_udp_open_write(struct hb_media * mp)
{
	struct ip_private * ei;
	extern int	WeAreRestarting;
	static int	FirstOpenAttempt=1;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	if (ANYDEBUG)  {
		ha_log(LOG_DEBUG, "ppp_udp_open_write called");
	}
	if (FirstOpenAttempt) {
		if (WeAreRestarting) {
			if (ppp_udp_ppp_proc_info(mp) == HA_OK) {
				ha_log(LOG_INFO
				,	"Using existing PPPd pid %d"
				,	ei->ppp_pid);
				ei->ppp_started = 1;
			}else{
				ha_log(LOG_INFO
				,	"restart: no existing PPPd process");
			}
		}
		FirstOpenAttempt=0;
	}
	if (ei->wsocket >= 0) {
		close(ei->wsocket);
		ei->wsocket = -1;
		ei->ppp_started = 0;	/* Force restart */
		if (ei->ppp_pid > 1) {
			CL_KILL(ei->ppp_pid, SIGTERM);
		}
	}
	if (ei->ppp_started && ei->ppp_pid > 1
	&&	(!CL_PID_EXISTS(ei->ppp_pid))) {
		/* It must have died while starting up */
		ei->ppp_started = 0;
	}
	if (!ei->ppp_started) {
		/* There may be a ppp running, but we don't want it ... */
		if (ppp_udp_ppp_proc_info(mp) == HA_OK) {
			if (ei->ppp_pid > 1) {
				if (CL_KILL(ei->ppp_pid, 0) >= 0) {
					ha_log(LOG_NOTICE
					,	"PID %d: killing PPPd pid %d"
					,	getpid(), ei->ppp_pid);
				}
				CL_KILL(ei->ppp_pid, SIGTERM);
			}else{
				ha_log(LOG_ERR, "Cannot kill unknown "
						"PPPd process!");
			}
			unlink(ppp_udp_ppp_start_path(mp));
		}
		if (ppp_udp_start_ppp(mp) != HA_OK) {
			return(HA_FAIL);
		}
		ei->ppp_started = 1;
	}
	/* Is the PPP link up yet? */
	if (ppp_udp_ppp_proc_info(mp) != HA_OK) {
		/* Oh well!  Try again later... */
		return(HA_FAIL);
	}
	if ((ei->wsocket = ppp_udp_make_send_sock(mp)) < 0) {
		return(HA_FAIL);
	}
	return(HA_OK);
}

/*
 *	Start PPP on the serial port.
 *
 *	Note: This code is likely to have to be tweaked a little to make it
 *	work with non-Linux OSes (and maybe with new Linux versions, too...)
 *
 *	Note that this script has to *somehow* cause the following information
 *	to appear in the start file:
 *		$IPREMOTE
 *		$IFNAME
 *		$PPPD_PID
 *	We do that right now by putting (for Red Hat) a modification into
 *	/etc/ppp/ip-up.local.  On other distributions, it may have to go into
 *	/etc/ppp-ip-up instead.
 */

extern pid_t	processes[];
extern int	num_procs;

static int
ppp_udp_start_ppp(struct hb_media * mp)
{
	char		PPPcmd[MAXLINE];
	struct ip_private * ei;
	pid_t		pid;
	extern int	baudrate;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	if (ANYDEBUG)  {
		ha_log(LOG_DEBUG, "Starting PPPd");
	}
	sprintf(PPPcmd, "exec %s %s %d %s %s: >%s/start.msgs 2>&1"
	,	PPPD, mp->name, baudrate, PPP_OPTS, ei->ipaddr
	,	PPP_D);
	switch ((pid=fork())) {
		case -1:	ha_perror("Can't fork pppd process!");
				return(HA_FAIL);
				break;

		case 0:		/* Child */
				execl(SHELL, SHELL, "-c", PPPcmd, NULL);
				ha_perror("Cannot exec shell for pppd!");
				cleanexit(1);
				break;

		default:	/* Parent */
				ei->ppp_pid = pid;
				ha_log(LOG_NOTICE
				       , "PPPd process %d started", pid);
	}
	
	/* Take PPP down with us! */
	pppref = ei;
	localdie = ppp_localdie;

	return(HA_OK);
}

static int
ppp_udp_open_read(struct hb_media * mp)
{
	struct ip_private * ei;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	if (ANYDEBUG)  {
		ha_log(LOG_DEBUG, "ppp_udp_open_read called");
	}
	if (ei->rsocket >= 0) {
		close(ei->rsocket);
		ei->rsocket = -1;
	}
	/* Is the PPP link up yet? */
	if (ppp_udp_ppp_proc_info(mp) != HA_OK) {
		/* Oh well!  Try again later... */
		return(HA_FAIL);
	}
	save_ppp_info(mp);
	if ((ei->rsocket = ppp_udp_make_receive_sock(mp)) < 0) {
		hb_dev_close(mp);
		return(HA_FAIL);
	}
	return(HA_OK);
}

/*
 *	Close PPP-UDP/IP heartbeat interface
 */
static int
hb_dev_close(struct hb_media* mp)
{
	struct ip_private * ei;
	int	rc = HA_OK;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	if (ANYDEBUG)  {
		ha_log(LOG_DEBUG, "ppp_udp_close called");
	}
	if (ei->rsocket >= 0) {
		if (close(ei->rsocket) < 0) {
			rc = HA_FAIL;
		}
		ei->rsocket = -1;
	}
	if (ei->wsocket >= 0) {
		if (close(ei->wsocket) < 0) {
			rc = HA_FAIL;
		}
		ei->wsocket = -1;
	}
	/* We don't like our PPP process for some reason */
	if (ei->ppp_pid > 1) {
		if (CL_KILL(ei->ppp_pid, 0) >= 0) {
			ha_log(LOG_NOTICE, "PID %d: killing PPPd pid %d."
			,	getpid(), ei->ppp_pid);
		}
		/* Even the reader can cause this, if things look bad */
		CL_KILL(ei->ppp_pid, SIGTERM);
		unlink(ppp_udp_ppp_start_path(mp));
		ei->ppp_pid = 0;
	}
	ei->ppp_started = 0;	/* This will make us restart it */
	return(rc);
}
/*
 * Receive a heartbeat packet from PPP-UDP interface
 */

static struct ha_msg *
hb_dev_read(struct hb_media* mp)
{
	struct ip_private *	ei;
	char			buf[MAXLINE];
	int			addr_len = sizeof(struct sockaddr);
   	struct sockaddr_in	their_addr; /* connector's addr information */
	int			numbytes;
	static int		errcount = 0;
	struct ha_msg*		ret;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	if (ANYDEBUG)  {
		ha_log(LOG_DEBUG, "ppp_udp_read called");
	}
	/* Wait for PPP to start... */
	while (ei->rsocket < 0) {
		if (ppp_udp_open_read(mp) != HA_OK) {
			sleep(1);
		}
	}

	if ((numbytes=recvfrom(ei->rsocket, buf, MAXLINE-1, 0
	,	(struct sockaddr *)&their_addr, &addr_len)) < 0) {
		if (errno != EINTR) {
			if (errno == EBADF) {
				errcount = ERRTHRESH+1;
			}else{
				ha_perror("Error receiving from socket");
				++errcount;
			}
			if (errcount > ERRTHRESH) {
				/* Kill PPPd.  The writer will restart it */
				hb_dev_close(mp);
			}
		}
		return(NULL);
	}else{
		ppp_countdown = PPPCOUNT;
		errcount = 0;
		buf[numbytes] = EOS;
	}
	if ((ret = string2msg(buf)) == NULL)  {
		return(HA_FAIL);
	}
	/*
	 * Code for copying the packet to other PPP/UDP interfaces.
	 * That's what makes this a bidirectional serial ring
	 */

	if (DEBUGPKT) {
		ha_log(LOG_DEBUG, "got %d byte packet from %s"
		,	numbytes, inet_ntoa(their_addr.sin_addr));
	}
	if (DEBUGPKTCONT) {
		ha_log(LOG_DEBUG, "%s", buf);
	}

	/* Should this message should continue around the ring? */
	if (isauthentic(ret) && should_ring_copy_msg(ret)) {
		int		ttl_mod=0;
		struct hb_media* mpp;

		/* Forward message to other port in ring (if any) */

		for (mpp = last_udp_ppp_interface; mpp; )  {
			struct ip_private * eip;
			PPPUDPASSERT(mpp);
			eip = mpp->pd;
			if (mpp == mp) {
				/* That's us! */
				continue;
			}
			if (!ttl_mod) {
				char		nttl[8];
				const char *	ttl_s;
				int		ttl;
				if ((ttl_s = ha_msg_value(ret, F_TTL)) == NULL){
					return(ret);
				}
				ttl = atoi(ttl_s);
				sprintf(nttl, "%d", ttl-1);
        			ha_msg_mod(ret, F_TTL, nttl);
				/* Re-authenticate message */
				add_msg_auth(ret);
				ttl_mod=1;
			}

			/* Write to the next port in the ring */
			mp->vf->write(mpp, ret);
			mpp=eip->next;
		}
	}
	return(ret);
}

/*
 * Send a heartbeat packet over PPP-UDP/IP interface
 */

static int
hb_dev_write(struct hb_media* mp, struct ha_msg* hmsg)
{
	struct ip_private *	ei;
	int			rc;
	char			msg[MAXLINE];
	int			errcount;
	static int		connrefusedcount = 0;
	char *			pkt;
	int			size;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	if ((pkt = msg2string(hmsg))  == NULL) {
		return(HA_FAIL);
	}
	size = strlen(pkt)+1;

	if (ANYDEBUG)  {
		ha_log(LOG_DEBUG, "ppp_udp_write called");
	}
	/* Is PPP connection already open? */
	if (ei->wsocket < 0) {
		ppp_udp_open_write(mp);
	}

	/*
	 *	Look to see if our PPP process is still alive.
	 *	It's not very expensive, and it's a very reliable check that's
	 *	robust with respect to all things that might cause PPP to
	 *	die at weird times.
	 *	Then, we don't need for the system to tell us that we have
	 *	no route to host to let us know.  I suppose we could use
	 *	death of child, but we only have one, so this works fine.
	 */
	if (ei->ppp_pid > 0 && !CL_PID_EXIS(ei->ppp_pid)) {
		/* Our PPP process has died.  Start a new one */
		ha_log(LOG_DEBUG, "PPPd process %d is gone.", ei->ppp_pid);
		hb_dev_close(mp);
		ppp_udp_open_write(mp);
	}

	/* Can't write to socket yet... */
	if (ei->wsocket < 0) {
		/* Pretend we wrote the packet without error */
		ha_free(pkt);
		return(HA_OK);
	}

	errcount = 0;

	/* We will try a few times to deliver the packet we've been given */
	do {
		errno = 0;
		if ((rc=sendto(ei->wsocket, (char*)pkt, size, 0
		,	(struct sockaddr *)&ei->addr
		,	sizeof(struct sockaddr))) != size) {
			++errcount;
			sprintf(msg, "Error sending pkt to %s"
			,	inet_ntoa(ei->addr.sin_addr));
			ha_perror(msg);
		}else{
			if (ANYDEBUG)  {
				ha_log(LOG_DEBUG, "ppp_udp_write succeeds");
			}
			errcount = 0;
		}
		/* Due to pppd weirdness, every other pkt gets ECONNREFUSED
		 * even though the packets aren't delivered.  So, we don't
		 * retry on ECONNREFUSED
		 */
	} while (rc != size && errcount < ERRTHRESH && errno != ECONNREFUSED);

	if (rc != size) {
		if (errno == ECONNREFUSED)  {
			/* Account for pppd weirdness */
			if (connrefusedcount < 0) {
				connrefusedcount = 0;
			}
			connrefusedcount+=2;
		}
		if (errno == ECONNREFUSED && connrefusedcount < REFUSEDRESTART){
			/* Pretend we wrote the packet without error */
			/* Hopefully far end heartbeats will start up soon... */
			/* If not, we'll keep getting connection refused and */
			/* eventually restart PPPd (in case it's us) */
			ha_free(pkt);
			return(HA_OK);
		}
		ha_log(LOG_WARNING
		,	"Too many errors sending to %s... closing..."
		,	inet_ntoa(ei->addr.sin_addr));
		/* This will cause PPPd to restart */
		hb_dev_close(mp);
		ha_free(pkt);
		return(HA_FAIL);
	}else{
		/* Account for pppd weirdness */
		connrefusedcount -=1;
	}

	if (DEBUGPKT) {
		ha_log(LOG_DEBUG, "sent %d bytes to %s"
		,	rc, inet_ntoa(ei->addr.sin_addr));
	}
	if (DEBUGPKTCONT) {
		ha_log(LOG_DEBUG, "%s", pkt);
   	}
	ha_free(pkt);
	return(HA_OK);
}

/*
 * Set up socket for sending PPP-UDP heartbeats
 */

static int
ppp_udp_make_send_sock(struct hb_media * mp)
{
	int sockfd, one = 1;
	struct ip_private * ei;
	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	set_up_ip(mp);
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		ha_perror("Error getting socket");
		return(sockfd);
   	}
/*
 * SO_DONTROUTE says don't use a gateway to send the packet, send it directly
 * Setting IP_TTL to 1 also ensures that if its being routed by proxy arp
 * or other 'transparent' routers it won't wander further.
 * SO_BINDTODEVICE restricts our packets to going out this interface alone.
 * The combination seems good for our purposes.
 * Tom Vogt suggested DONTROUTE, and Alan Cox suggested setting TTL,
 * and SO_BINDTODEVICE.
 * Thanks guys!			I *love* belt and suspenders solutions!
 *
 */
#if defined(SO_DONTROUTE)
	/* usually, we don't want to be subject to routing. */
	if (setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE,&one, sizeof(one))< 0) {
		ha_perror("Error setting socket option SO_DONTROUTE");
		/* This is not fatal */
	}
#endif
#if defined(IPPROTP_IP) && defined(IP_TTL)
        /* Set the packet time to live to 1, so it will only take one hop */
        if (setsockopt(sockfd, IPPROTP_IP, IP_TTL ,&one, sizeof(one)) < 0) {
                ha_perror("Error setting socket option IP_TTL");
		/* This is not fatal */
        }
#endif
#if defined(SO_BINDTODEVICE)
	{
		/*
		 *  We want to send out this particular interface
		 *  Disadvantage: we won't get errors when ppp goes away.
		 *  Is this a bug?
		 *
		 *  We compensate for it in write by checking for pppd.
		 *  before every packet write.
		 *  Sometimes pppd gets wedged without going away, so we won't notice.
		 *  When this happens, the reader will notice the silence and whack pppd
		 *  after a while.
		 */
		struct ifreq i;
		strcpy(i.ifr_name,  ei->interface);

		if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE
		,	&i, sizeof(i)) == -1) {
			ha_perror("Error setting option SO_BINDTODEVICE(w)");
			/* This is not fatal */
		}
	}
#endif
	if (fcntl(sockfd,F_SETFD, FD_CLOEXEC)) {
		ha_perror("Error setting the close-on-exec flag");
	}
	return(sockfd);
}

/*
 * Set up socket for listening to heartbeats (PPP-UDP)
 */

int
ppp_udp_make_receive_sock(struct hb_media * mp) {

	struct ip_private * ei;
	struct sockaddr_in my_addr;    /* my address information */
	int	sockfd;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;
	set_up_ip(mp);
	memset(&(my_addr), 0, sizeof(my_addr));	/* zero my address struct */
	my_addr.sin_family = AF_INET;		/* host byte order */
	my_addr.sin_port = htons(ei->port);	/* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY;	/* auto-fill with my IP */

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		ha_perror("Error getting socket");
		return(-1);
	}
#if defined(SO_BINDTODEVICE)
/*
 *	If I bind to an interface, then I can't receive any more packets
 *	if the interface goes away and comes back.  Even if it comes back
 *	with the same interface name, we still can't receive any packets.
 *	This makes a certain amount of sense, but it sure causes a problem!
 *
 *	What we do about this is notice if the device file in
 *	/etc/ha.d/ppp.d is missing or has a new timestamp.
 *	When this happens, we close the socket and kill our pppd process.
 *	This causes an automatic restart
 */
	{
		/*
		 *  We want to packets only from this PPP interface...
		 */
		struct ifreq i;
		strcpy(i.ifr_name,  ei->interface);

		if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE
		,	&i, sizeof(i)) == -1) {
			ha_perror("Error setting option SO_BINDTODEVICE(r)");
			ha_perror(i.ifr_name);
			close(sockfd);
			return(-1);
		}
		if (ANYDEBUG) {
			ha_log(LOG_DEBUG
			,	"SO_BINDTODEVICE(r) set for device %s"
			,	i.ifr_name);
		}
	}
#endif
	if (bind(sockfd, (struct sockaddr *)&my_addr
	,	sizeof(struct sockaddr)) < 0) {
#if !defined(SO_BINDTODEVICE)
		if (errno == EADDRINUSE) {
			/* This happens with multiple udp or ppp interfaces */
			ha_log(LOG_NOTICE
			,	"Someone already listening on port %d [%s]"
			,	ei->port
			,	mp->name);
			ha_log(LOG_NOTICE, "PPP/UDP read process exiting.");
			close(sockfd);
			cleanexit(0);
		}
#endif
		ha_perror("Error binding ppp_receive socket");
		ha_log(LOG_NOTICE, "Read process exiting.");
		close(sockfd);
		cleanexit(1);
	}
	ha_log(LOG_NOTICE, "PPP/UDP socket open on interface (%s:%s)"
	,	ei->interface, mp->name);
	if (fcntl(sockfd,F_SETFD, FD_CLOEXEC)) {
		ha_perror("Error setting the close-on-exec flag");
	}
	return(sockfd);
}
#define	ALARMCNT	2
#define	NULLTS		((TIME_T) 0L)
static TIME_T			ppp_ts = NULLTS;
static char			ppp_path[MAXLINE];
static struct hb_media *	ppp_hbmedia;

static void
save_ppp_info(struct hb_media * mp)
{
	struct stat buf;
	char *	path = ppp_udp_ppp_start_path(mp);
	int	j;

	ppp_ts = NULLTS;
	ppp_hbmedia = mp;
	if (path == NULL) {
		return;
	}
	strcpy(ppp_path, path);
	for (j=0; j < 3; ++j) {
		TIME_T	now;
		now = (TIME_T) time(NULL);
		if (stat(ppp_path, &buf) >= 0) {
			ppp_ts = (TIME_T) buf.st_mtime;
			if ((now - ppp_ts) > 1) {
				break;
			}
			sleep(1);
		}else{
			break;
		}
	}
	ppp_countdown = PPPCOUNT;
	CL_SIGNAL(SIGALRM, check_ppp_info);
	CL_SIGINTERRUPT(SIGALRM, 1);
	alarm(ALARMCNT);
}

static void
check_ppp_info(int sig)
{
	struct stat buf;
	struct ip_private * ei;

	alarm(0);
	(void)sig;
	PPPUDPASSERT(ppp_hbmedia);
	ei = (struct ip_private *) ppp_hbmedia->pd;

	if (ei->ppp_pid > 0) {
		ppp_countdown -= ALARMCNT;
		if (ppp_countdown <= 0) {
			/* Yes, this really happens.
			 * It seems that one side sees a long-running PPPd
			 * and the other is waiting to establish a connection.
			 * The long-running side sees continual connection
			 * refused, and the other side waits to start.
			 * Detecting it here and on the write side both is
			 * a sort of belt and suspenders approach which assumes
			 * (correctly) that I don't understand everything.
			 */
			ha_log(LOG_ERR
			,	"PPPd pid %d may be wedged", ei->ppp_pid);
			hb_dev_close(ppp_hbmedia);
			ppp_countdown = PPPCOUNT;
		}
	}

	buf.st_mtime=0;
	if (ppp_ts != NULLTS
	&&	(stat(ppp_path, &buf) < 0 || ppp_ts != buf.st_mtime)) {
		/* OOPS! Need to re-open the socket */
		ha_log(LOG_NOTICE
		       , "PPP/UDP reader closing socket [%s]", ppp_path);
		ppp_ts = NULLTS;
		hb_dev_close(ppp_hbmedia);
	}else{
		alarm(ALARMCNT);
	}
}


static int
set_up_ip(struct hb_media * mp)
{
	struct ip_private * ei;
	struct hostent *he;

	PPPUDPASSERT(mp);
	ei = (struct ip_private *) mp->pd;

	/*
	 * Get the "hostent" structure for the host address
	 */

	if ((he=gethostbyname(ei->far_addr)) == NULL) {
		ha_perror("Error getting IP for broadcast address");
		return(HA_FAIL);
	}

	/*
	 * We now have the information we need.  Populate our
	 * structure with the information we've gotten.
	 */

	ei->farhost = *he;

	memset(&ei->addr, 0, sizeof(ei->addr));	/* zero the struct */
	ei->addr.sin_family = AF_INET;		/* host byte order */
	ei->addr.sin_port = htons(ei->port);	/* short, network byte order */
	ei->addr.sin_addr = *((struct in_addr *)ei->farhost.h_addr);
	return(HA_OK);
}
/*
 *	Make sure we were given a valid "local" address.
 *
 *	In modern parlance the set of local addresses are described as:
 *		10.0.0.0/8
 *		172.16.0.0/12
 *		192.168.0.0/16
 *
 *	This code is dependent on IPV4 addressing conventions...
 *		Sorry.
 *
 */
#define		MASK10	0x000000FFU
#define		VAL10	0x0000000AU

#define		MASK172	0x00000FFFU
#define		VAL172	0x000001ACU

#define		MASK192	0x0000FFFFU
#define		VAL192	0x0000A8C0U

static int
is_valid_local_addr(const char * addr)
{
	struct in_addr	in;
	char		msg[MAXLINE];
	if (inet_aton(addr, &in) == 0) {
		sprintf(msg, "IP address [%s] not valid in config file"
		,	addr);
		ha_log(LOG_ERR, msg);
		return(HA_FAIL);
	}
	if ((in.s_addr&MASK10) == VAL10) {
		return(HA_OK);
	}
	if ((in.s_addr&MASK172) == VAL172) {
		return(HA_OK);
	}
	if ((in.s_addr&MASK192) == VAL192) {
		return(HA_OK);
	}
	/* Valid address, but not a local address.  Throw it out. */
	sprintf(msg
	,	"IP address [%s] not a %s in config file (0x%x)"
	,	addr
	,	"local (RFC 1918) address"
	,	in.s_addr);
	ha_log(LOG_ERR, msg);
	return(HA_FAIL);
}

static int
is_valid_serial(const char * port)
{
	char		msg[MAXLINE];
	struct stat	sbuf;

	/* Let's see if this looks like it might be a serial port... */
	if (*port != '/') {
		sprintf(msg, "Serial port not full pathname [%s] in config file"
		,	port);
		ha_log(LOG_ERR, msg);
		return(HA_FAIL);
	}

	if (stat(port, &sbuf) < 0) {
		sprintf(msg, "Nonexistent serial port [%s] in config file"
		,	port);
		ha_perror(msg);
		return(HA_FAIL);
	}
	if (!S_ISCHR(sbuf.st_mode)) {
		sprintf(msg, "Serial port [%s] not a char device in config file"
		,	port);
		ha_log(LOG_ERR, msg);
		return(HA_FAIL);
	}
	return(HA_OK);
}

static void
ppp_localdie(void)
{
	if (pppref && pppref->ppp_pid > 0) {
		CL_KILL(pppref->ppp_pid, SIGTERM);
	}
}
/*
 * $Log: ppp-udp.c,v $
 * Revision 1.17  2005/07/29 07:18:55  sunjd
 * bug668: license update
 *
 * Revision 1.16  2004/10/09 01:49:42  lge
 * 199Kb patch transforming all // I could find into /* */
 * have a lot of fun :-/
 *
 * Revision 1.15  2004/02/17 22:11:59  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.14  2004/01/21 11:34:15  horms
 * - Replaced numerous malloc + strcpy/strncpy invocations with strdup
 *   * This usually makes the code a bit cleaner
 *   * Also is easier not to make code with potential buffer over-runs
 * - Added STRDUP to pils modules
 * - Removed some spurious MALLOC and FREE redefinitions
 *   _that could never be used_
 * - Make sure the return value of strdup is honoured in error conditions
 *
 * Revision 1.13  2003/02/07 08:37:18  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.12  2003/02/05 09:06:34  horms
 * Lars put a lot of work into making sure that portability.h
 * is included first, everywhere. However this broke a few
 * things when building against heartbeat headers that
 * have been installed (usually somewhere under /usr/include or
 * /usr/local/include).
 *
 * This patch should resolve this problem without undoing all of
 * Lars's hard work.
 *
 * As an asside: I think that portability.h is a virus that has
 * infected all of heartbeat's code and now must also infect all
 * code that builds against heartbeat. I wish that it didn't need
 * to be included all over the place. Especially in headers to
 * be installed on the system. However, I respect Lars's opinion
 * that this is the best way to resolve some weird build problems
 * in the current tree.
 *
 * Revision 1.11  2003/02/03 11:30:41  lars
 * Changed all files including system headers to include portability.h first, so
 * that global defines affecting the exported interfaces from the system headers
 * are indeed global and apply consistently to all headers.
 *
 * Revision 1.10  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.9  2002/11/20 11:01:33  lars
 * Fixed a CVS merge artifact. (Bad horms! ;-)
 *
 * Revision 1.8  2002/10/23 08:42:22  horms
 * Added CL_PID_EXISTS conveince macro
 *
 * Revision 1.7  2002/10/21 10:17:19  horms
 * hb api clients may now be built outside of the heartbeat tree
 *
 * Revision 1.6  2002/10/21 02:00:35  horms
 * Use CL_KILL() instead of kill() throughout the code.
 * This makes the code nice and homogenous and removes
 * the need for spurious inclusion of signal.h
 *
 * Revision 1.5  2002/10/18 07:16:10  alan
 * Put in Horms big patch plus a patch for the apcmastersnmp code where
 * a macro named MIN returned the MAX instead.  The code actually wanted
 * the MAX, so when the #define for MIN was surrounded by a #ifndef, then
 * it no longer worked...  This fix courtesy of Martin Bene.
 * There was also a missing #include needed on older Linux systems.
 *
 * Revision 1.4  2002/10/02 13:40:26  alan
 * Fixed some potential holes pointed out by Nathan Wallwork.
 *
 * Revision 1.3  2002/08/23 12:52:29  alan
 * I just put a caution about known security-related-bugs in this obsolete code.
 *
 * Revision 1.2  2001/10/02 16:10:05  alan
 * Replaced the obsolescent bzero function with memset.
 *
 * Revision 1.1  2001/08/10 17:16:44  alan
 * New code for the new plugin loading system.
 *
 * Revision 1.15  2001/05/11 06:20:26  alan
 * Fixed CFLAGS so we load modules from the right diurectory.
 * Fixed minor static symbol problems.
 * Fixed a bug which kept early error messages from coming out.
 *
 * Revision 1.14  2001/03/08 14:37:03  alan
 * Removed the ppp code from the build process...
 *
 * Revision 1.13  2000/12/12 23:23:47  alan
 * Changed the type of times from time_t to TIME_T (unsigned long).
 * Added BuildPreReq: lynx
 * Made things a little more OpenBSD compatible.
 *
 * Revision 1.12  2000/09/01 21:10:46  marcelo
 * Added dynamic module support
 *
 * Revision 1.11  2000/08/13 04:36:16  alan
 * Added code to make ping heartbeats work...
 * It looks like they do, too ;-)
 *
 * Revision 1.10  2000/07/26 05:17:19  alan
 * Added GPL license statements to all the code.
 *
 * Revision 1.9  2000/06/21 04:34:48  alan
 * Changed henge.com => linux-ha.org and alanr@henge.com => alanr@suse.com
 *
 * Revision 1.8  2000/05/17 13:39:54  alan
 * Added the close-on-exec flag to sockets and tty fds that we open.
 * Thanks to Christoph Jäger for noticing the problem.
 *
 * Revision 1.7  2000/05/17 13:01:49  alan
 * Changed argv[0] and cmdname to be shorter.
 * Changed ha parsing function to close ha.cf.
 * Changed comments in ppp-udp so that it notes the current problems.
 *
 * Revision 1.6  1999/10/25 15:35:03  alan
 * Added code to move a little ways along the path to having error recovery
 * in the heartbeat protocol.
 * Changed the code for serial.c and ppp-udp.c so that they reauthenticate
 * packets they change the ttl on (before forwarding them).
 *
 * Revision 1.5  1999/10/10 20:12:47  alanr
 * New malloc/free (untested)
 *
 * Revision 1.4  1999/10/05 16:11:53  alanr
 * First attempt at restarting everything with -R/-r flags
 *
 * Revision 1.3  1999/10/01 14:34:51  alanr
 * patch from Matt Soffen for FreeBSD.
 *
 * Revision 1.2  1999/09/26 14:01:14  alanr
 * Added Mijta's code for authentication and Guenther Thomsen's code for serial locking and syslog reform
 *
 * Revision 1.1.1.1  1999/09/23 15:31:24  alanr
 * High-Availability Linux
 *
 * Revision 1.10  1999/09/18 02:56:36  alanr
 * Put in Matt Soffen's portability changes...
 *
 * Revision 1.9  1999/09/16 05:50:20  alanr
 * Getting ready for 0.4.3...
 *
 * Revision 1.8  1999/08/17 03:48:31  alanr
 * added log entry to bottom of file.
 *
 */
