/* $Id: send_arp.c,v 1.10 2004/04/27 09:49:32 horms Exp $ */
/* 
 * send_arp
 * 
 * This program sends out one ARP packet with source/target IP and Ethernet
 * hardware addresses suuplied by the user.  It uses the libnet libary from
 * Packet Factory (http://www.packetfactory.net/libnet/ ). It has been tested
 * on Linux, FreeBSD, and on Solaris.
 * 
 * This inspired by the sample application supplied by Packet Factory.

 * Matt Soffen

 * Copyright (C) 2001 Matt Soffen <matt@soffen.com>
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <limits.h>
#include <libnet.h>
#include <syslog.h>
#include <libgen.h>
#include <clplumbing/timers.h>
#include <clplumbing/cl_signal.h>

#ifdef HAVE_LIBNET_1_0_API
#	define	LTYPE	struct libnet_link_int
#endif
#ifdef HAVE_LIBNET_1_1_API
#	define	LTYPE	libnet_t
#endif

#define PIDDIR       HA_VARLIBDIR "/" PACKAGE "/rsctmp/send_arp"
#define PIDFILE_BASE PIDDIR "/send_arp-"

static int send_arp(LTYPE* l, u_long ip, u_char *device, u_char mac[6]
,	u_char *broadcast, u_char *netmask, u_short arptype);

static char print_usage[]={
	"send_arp: sends out custom ARP packet. packetfactory.net\n"
	"\tusage: send_arp [-i repeatinterval-ms] [-r repeatcount]"
	" [-p pidfile] device src_ip_addr src_hw_addr broadcast_ip_addr netmask\n"
	"\tIf src_hw_addr is \"auto\" then the address of device will be used"
};

static void convert_macaddr (u_char *macaddr, u_char enet_src[6]);
static int get_hw_addr(char *device, u_char mac[6]);
int write_pid_file(const char *pidfilename);
int create_pid_directory(const char *piddirectory);

#define AUTO_MAC_ADDR "auto"


#ifndef LIBNET_ERRBUF_SIZE
#	define LIBNET_ERRBUF_SIZE 256
#endif

static void
byebye(int nsig)
{
	(void)nsig;
	/* Avoid an "error exit" log message if we're killed */
	exit(0);
}


int
main(int argc, char *argv[])
{
	int	c = -1;
	char	errbuf[LIBNET_ERRBUF_SIZE];
	char*	device;
	char*	ipaddr;
	char*	macaddr;
	char*	broadcast;
	char*	netmask;
	u_long	ip;
	u_char  src_mac[6];
	LTYPE*	l;
	int	repeatcount = 1;
	int	j;
	long	msinterval = 1000;
	int	flag;
	char    pidfilenamebuf[64];
	char    *pidfilename = NULL;

	CL_SIGINTERRUPT(SIGTERM, 1);
	CL_SIGNAL(SIGTERM, byebye);

	openlog("send_arp", LOG_CONS | LOG_PID, LOG_USER);

	while ((flag = getopt(argc, argv, "i:r:p:")) != EOF) {
		switch(flag) {

		case 'i':	msinterval= atol(optarg);
				break;

		case 'r':	repeatcount= atoi(optarg);
				break;

		case 'p':	pidfilename= optarg;
				break;

		default:	fprintf(stderr, "usage: %s\n\n", print_usage);
				return 1;
				break;
		}
	}
	if (argc-optind != 5) {
		fprintf(stderr, "usage: %s\n\n", print_usage);
		return 1;
	}

	/*
	 *	argv[optind+1] DEVICE		dc0,eth0:0,hme0:0,
	 *	argv[optind+2] IP		192.168.195.186
	 *	argv[optind+3] MAC ADDR		00a0cc34a878
	 *	argv[optind+4] BROADCAST	192.168.195.186
	 *	argv[optind+5] NETMASK		ffffffffffff
	 */

	device    = argv[optind];
	ipaddr    = argv[optind+1];
	macaddr   = argv[optind+2];
	broadcast = argv[optind+3];
	netmask   = argv[optind+4];

	if (!pidfilename) {
		if (snprintf(pidfilenamebuf, sizeof(pidfilenamebuf), "%s%s", 
					PIDFILE_BASE, ipaddr) >= 
				(int)sizeof(pidfilenamebuf)) {
			syslog(LOG_INFO, "Pid file truncated");
			return EXIT_FAILURE;
		}
		pidfilename = pidfilenamebuf;
	}

	if(write_pid_file(pidfilename) < 0) {
		return EXIT_FAILURE;
	}

#if defined(HAVE_LIBNET_1_0_API)
	if ((ip = libnet_name_resolve(ipaddr, 1)) == -1UL) {
		syslog(LOG_ERR, "Cannot resolve IP address [%s]", ipaddr);
		unlink(pidfilename);
		return EXIT_FAILURE;
	}

	l = libnet_open_link_interface(device, errbuf);
	if (!l) {
		syslog(LOG_ERR, "libnet_open_link_interface on %s: %s"
		,	device, errbuf);
		unlink(pidfilename);
		return EXIT_FAILURE;
	}
#elif defined(HAVE_LIBNET_1_1_API)
	if ((l=libnet_init(LIBNET_LINK, device, errbuf)) == NULL) {
		syslog(LOG_ERR, "libnet_init failure on %s", device);
		unlink(pidfilename);
		return EXIT_FAILURE;
	}
	if ((signed)(ip = libnet_name2addr4(l, ipaddr, 1)) == -1) {
		syslog(LOG_ERR, "Cannot resolve IP address [%s]", ipaddr);
		unlink(pidfilename);
		return EXIT_FAILURE;
	}
#else
#	error "Must have LIBNET API version defined."
#endif

	if (!strcasecmp(macaddr, AUTO_MAC_ADDR)) {
		if (get_hw_addr(device, src_mac) < 0) {
			 syslog(LOG_ERR, "Cannot find mac address for %s", 
					 device);
			 unlink(pidfilename);
			 return EXIT_FAILURE;
		}
	}
	else {
		convert_macaddr(macaddr, src_mac);
	}

/*
 * We need to send both a broadcast ARP request as well as the ARP response we
 * were already sending.  All the interesting research work for this fix was
 * done by Masaki Hasegawa <masaki-h@pp.iij4u.or.jp> and his colleagues.
 */
	for (j=0; j < repeatcount; ++j) {
		c = send_arp(l, ip, device, src_mac, broadcast, 
				netmask, ARPOP_REQUEST);
		mssleep(msinterval / 2);
		c = send_arp(l, ip, device, src_mac, broadcast, 
				netmask, ARPOP_REPLY);
		if (j != repeatcount-1) {
			mssleep(msinterval / 2);
		}
	}

	unlink(pidfilename);
	return c < 0  ? EXIT_FAILURE : EXIT_SUCCESS;
}


void
convert_macaddr (u_char *macaddr, u_char enet_src[6])
{
	int i, pos;
	u_char bits[3];

	pos = 0;
	for (i = 0; i < 6; i++) {
		/* Inserted to allow old-style MAC addresses */
		if (*macaddr == ':') {
			pos++;
		}
		bits[0] = macaddr[pos++];
		bits[1] = macaddr[pos++];
		bits[2] = '\0';

		enet_src[i] = strtol(bits, (char **)NULL, 16);
	}

}

#ifdef HAVE_LIBNET_1_0_API
int
get_hw_addr(char *device, u_char mac[6])
{
	struct ether_addr	*mac_address;
	struct libnet_link_int  *network;
	char                    err_buf[LIBNET_ERRBUF_SIZE];

	/* Get around bad prototype for libnet_error() */
	char errmess1 [] = "libnet_open_link_interface: %s\n";
	char errmess2 [] = "libnet_get_hwaddr: %s\n";


	network = libnet_open_link_interface(device, err_buf);
	if (!network) {
		libnet_error(LIBNET_ERR_FATAL, errmess1, err_buf);
		return -1;
	}

	mac_address = libnet_get_hwaddr(network, device, err_buf);
	if (!mac_address) {
		libnet_error(LIBNET_ERR_FATAL, errmess2, err_buf);
		return -1;
	}

	memcpy(mac, mac_address->ether_addr_octet, 6);

	return 0;
}
#endif

#ifdef HAVE_LIBNET_1_1_API
int
get_hw_addr(char *device, u_char mac[6])
{
	struct libnet_ether_addr	*mac_address;
	libnet_t		*ln;
	char			err_buf[LIBNET_ERRBUF_SIZE];

	ln = libnet_init(LIBNET_LINK, device, err_buf);
	if (!ln) {
		fprintf(stderr, "libnet_open_link_interface: %s\n", err_buf);
		return -1;
	}

	mac_address = libnet_get_hwaddr(ln);
	if (!mac_address) {
		fprintf(stderr,  "libnet_get_hwaddr: %s\n", err_buf);
		return -1;
	}

	memcpy(mac, mac_address->ether_addr_octet, 6);

	return 0;
}
#endif

#ifdef HAVE_LIBNET_1_0_API
int
send_arp(struct libnet_link_int *l, u_long ip, u_char *device, u_char *macaddr, u_char *broadcast, u_char *netmask, u_short arptype)
{
	int n;
	u_char *buf;
	u_char enet_dst[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


	if (libnet_init_packet(LIBNET_ARP_H + LIBNET_ETH_H, &buf) == -1) {
	syslog(LOG_ERR, "libnet_init_packet memory:");
		return -1;
	}

	/* Convert ASCII Mac Address to 6 Hex Digits. */

	/* Ethernet header */
	libnet_build_ethernet(enet_dst, macaddr, ETHERTYPE_ARP, NULL, 0, buf);

	/*
	 *  ARP header
	 */
	libnet_build_arp(ARPHRD_ETHER,	/* Hardware address type */
		ETHERTYPE_IP,			/* Protocol address type */
		6,				/* Hardware address length */
		4,				/* Protocol address length */
		arptype,			/* ARP operation */
		macaddr,			/* Source hardware addr */
		(u_char *)&ip,			/* Target hardware addr */
		enet_dst,			/* Destination hw addr */
		(u_char *)&ip,			/* Target protocol address */
		NULL,				/* Payload */
		0,				/* Payload length */
		buf + LIBNET_ETH_H);

	n = libnet_write_link_layer(l, device, buf, LIBNET_ARP_H + LIBNET_ETH_H);

	libnet_destroy_packet(&buf);
	return (n);
}
#endif /* HAVE_LIBNET_1_0_API */




#ifdef HAVE_LIBNET_1_1_API
int
send_arp(libnet_t* lntag, u_long ip, u_char *device, u_char macaddr[6], u_char *broadcast, u_char *netmask, u_short arptype)
{
	int n;
	u_char enet_dst[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	/*
	 *  ARP header
	 */
	libnet_build_arp(ARPHRD_ETHER,	/* hardware address type */
		ETHERTYPE_IP,	/* protocol address type */
		6,		/* Hardware address length */
		4,		/* protocol address length */
		arptype,	/* ARP operation type */
		macaddr,	/* sender Hardware address */
		(u_char *)&ip,	/* sender protocol address */
		enet_dst,	/* target hardware address */
		(u_char *)&ip,	/* target protocol address */
		NULL,		/* Payload */
		0,		/* Length of payload */
		lntag,		/* libnet context pointer */
		0		/* packet id */
	);

	/* Ethernet header */
	libnet_build_ethernet(enet_dst, macaddr, ETHERTYPE_ARP, NULL, 0
	,	lntag, 0);

	n = libnet_write(lntag);
	libnet_clear_packet(lntag);

	return (n);
}
#endif /* HAVE_LIBNET_1_1_API */


int
create_pid_directory(const char *pidfilename)  
{
	int	status;
	struct stat stat_buf;
	char    *pidfilename_cpy;
	char    *dir;

	pidfilename_cpy = strdup(pidfilename);
	if (!pidfilename_cpy) {
		syslog(LOG_INFO, "Memory allocation failure: %s\n",
				strerror(errno));
		return -1;
	}
	
	dir = dirname(pidfilename_cpy);

	status = stat(dir, &stat_buf); 

	if (status < 0 && errno != ENOENT && errno != ENOTDIR) {
		syslog(LOG_INFO, "Could not stat pid-file directory "
				"[%s]: %s", dir, strerror(errno));
		free(pidfilename_cpy);
		return -1;
	}
	
	if (!status) {
		if (S_ISDIR(stat_buf.st_mode)) {
			return 0;
		}
		syslog(LOG_INFO, "Pid-File directory exists but is "
				"not a directory [%s]", dir);
		free(pidfilename_cpy);
		return -1;
        }

	if (mkdir(dir, S_IRUSR|S_IWUSR|S_IXUSR | S_IRGRP|S_IXGRP) < 0) {
		syslog(LOG_INFO, "Could not create pid-file directory "
				"[%s]: %s", dir, strerror(errno));
		free(pidfilename_cpy);
		return -1;
	}

	free(pidfilename_cpy);
	return 0;
}


int
write_pid_file(const char *pidfilename)  
{

	int     	pidfilefd;
	char    	pidbuf[11];
	unsigned long   pid;
	size_t 		bytes;

	if (*pidfilename != '/') {
		syslog(LOG_INFO, "Invalid pid-file name, must begin with a "
				"'/' [%s]\n", pidfilename);
		return -1;
	}

	if (create_pid_directory(pidfilename) < 0) {
		return -1;
	}

	while (1) {
		pidfilefd = open(pidfilename, O_CREAT|O_EXCL|O_RDWR, 
				S_IRUSR|S_IWUSR);
		if (pidfilefd < 0) {
			if (errno != EEXIST) { /* Old PID file */
				syslog(LOG_INFO, "Could not open pid-file "
						"[%s]: %s", pidfilename, 
						strerror(errno));
				return -1;
			}
		}
		else {
			break;
		}

		pidfilefd = open(pidfilename, O_RDONLY, S_IRUSR|S_IWUSR);
		if (pidfilefd < 0) {
			syslog(LOG_INFO, "Could not open pid-file " 
					"[%s]: %s", pidfilename, 
					strerror(errno));
			return -1;
		}

		while (1) {
			bytes = read(pidfilefd, pidbuf, sizeof(pidbuf)-1);
			if (bytes < 0) {
				if (errno == EINTR) {
					continue;
				}
				syslog(LOG_INFO, "Could not read pid-file " 
						"[%s]: %s", pidfilename, 
						strerror(errno));
				return -1;
			}
			pidbuf[bytes] = '\0';
			break;
		}

		if(unlink(pidfilename) < 0) {
			syslog(LOG_INFO, "Could not delete pid-file "
	 				"[%s]: %s", pidfilename, 
					strerror(errno));
			return -1;
		}

		if (!bytes) {
			syslog(LOG_INFO, "Invalid pid in pid-file "
	 				"[%s]: %s", pidfilename, 
					strerror(errno));
			return -1;
		}

		close(pidfilefd);

		pid = strtoul(pidbuf, NULL, 10);
		if (pid == ULONG_MAX && errno == ERANGE) {
			syslog(LOG_INFO, "Invalid pid in pid-file "
	 				"[%s]: %s", pidfilename, 
					strerror(errno));
			return -1;
		}

		if (kill(pid, SIGKILL) < 0 && errno != ESRCH) {
			syslog(LOG_INFO, "Error killing old proccess [%lu] "
	 				"from pid-file [%s]: %s", pid,
					pidfilename, strerror(errno));
			return -1;
		}

		syslog(LOG_INFO, "Killed old send_arp process [%lu]\n",
				pid);
	}

	if (snprintf(pidbuf, sizeof(pidbuf), "%u"
	,	getpid()) >= (int)sizeof(pidbuf)) {
		syslog(LOG_INFO, "Pid too long for buffer [%u]", getpid());
		return -1;
	}

	while (1) {
		bytes = write(pidfilefd, pidbuf, strlen(pidbuf));
		if (bytes != strlen(pidbuf)) {
			if (bytes < 0 && errno == EINTR) {
				continue;
			}
			syslog(LOG_INFO, "Could not write pid-file "
					"[%s]: %s", pidfilename,
					strerror(errno));
			return -1;
		}
		break;
	}

	close(pidfilefd);

	return 0;
}


/*
 * $Log: send_arp.c,v $
 * Revision 1.10  2004/04/27 09:49:32  horms
 * Fix for pid code for FreeBSD (and others)
 *
 * Revision 1.9  2004/03/19 15:07:43  alan
 * Put in a fix provided by Michael Dipper <md@LF.net>
 * A '/' is missing from the pidfile pathname for send_arp.
 *
 * Revision 1.8  2004/02/17 22:11:57  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.7  2004/01/22 01:53:35  alan
 * Changed send_arp to exit(0) when killed by SIGTERM.
 *
 * Revision 1.6  2003/12/11 22:58:04  alan
 * Put in a patch from Werner Schulthei� for allowing old-style MAC addrs
 * in send_arp.
 *
 * Fixed some signed/unsigned warning issues in the code.
 *
 * Revision 1.5  2003/10/31 16:15:20  msoffen
 * Added limit.h to define MAXINT value.
 *
 * Revision 1.4  2003/10/29 11:40:50  horms
 * Send arp  creates a driectory to store its pid file in on-the-fly. IPaddr does likewise for state files.
 *
 * Revision 1.3  2003/10/23 07:36:29  horms
 * added pid file to sendarp
 *
 * Revision 1.2  2003/10/15 17:03:23  horms
 * Merged  get_hw_addr into and send_arp
 *
 * Revision 1.1  2003/10/15 09:52:12  horms
 * Move utilities that link against libnet (sendarp and get_hw_addr)
 * to their own directory so we no longer have to come up with
 * creative ways to selectively mangle CFLAGS.
 *
 * Revision 1.24  2003/09/26 06:02:13  alan
 * Fixed an undefined variable warning.
 *
 * Revision 1.23  2003/07/12 17:02:59  alan
 * Hopefully last fix for the changes made to allow user to specify arp intervals, etc.
 *
 * Revision 1.22  2003/07/12 16:19:54  alan
 * Fixed a bug in the new send_arp options and their invocation...
 *
 * Revision 1.21  2003/04/15 18:56:33  msoffen
 * Removed printf("\n") that served no purpose anymore (used to print .\n).
 *
 * Revision 1.20  2003/04/15 11:07:05  horms
 * errors go to stderr, not stdout
 *
 * Revision 1.19  2003/03/21 17:38:31  alan
 * Put in a patch by Thiago Rondon <thiago@nl.linux.org> to fix a minor
 * compile error in send_arp.c, which only affects the 1.1 libnet API code.
 *
 * Revision 1.18  2003/02/17 18:51:03  alan
 * Minor typo correction for #error line in send_arp.c
 *
 * Revision 1.17  2003/02/17 16:30:46  msoffen
 * Made it error out if libnet isn't defined at all (no 1.0 or 1.1 version).
 *
 * Revision 1.16  2003/02/17 15:31:50  alan
 * Fixed a nasty bug where we don't pass the interface to the libnet libraries
 * correctly.  Thanks to Steve Snodgrass for finding it.
 *
 * Revision 1.15  2003/02/07 08:37:17  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.14  2003/02/05 09:06:33  horms
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
 * Revision 1.13  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.12  2002/09/12 14:06:18  msoffen
 * Removed a write to stderr of ".", really served no purpose and always ran.
 * It was a carryover from the old send_arp.c.
 *
 * Revision 1.11  2002/09/05 06:12:42  alan
 * Put in code to recover a bug fix from Japan, plus
 * make the code hopefully work with both the old and new libnet APIs.
 *
 * Revision 1.10  2002/08/16 14:18:41  msoffen
 * Changes to get IP takeover working properly on OpenBSD
 * Changed how *BSD deletes an alias.
 * Create get_hw_addr (uses libnet package).
 *
 * Revision 1.9  2002/07/08 04:14:12  alan
 * Updated comments in the front of various files.
 * Removed Matt's Solaris fix (which seems to be illegal on Linux).
 *
 * Revision 1.8  2002/06/06 04:43:40  alan
 * Got rid of a warning (error) about an unused RCS version string.
 *
 * Revision 1.7  2002/05/28 18:25:48  msoffen
 * Changes to replace send_arp with a libnet based version.  This works accross
 * all operating systems we currently "support" (Linux, FreeBSD, Solaris).
 *
 * Revision 1.6  2001/10/24 00:21:58  alan
 * Fix to send both a broadcast ARP request as well as the ARP response we
 * were already sending.  All the interesting research work for this fix was
 * done by Masaki Hasegawa <masaki-h@pp.iij4u.or.jp> and his colleagues.
 *
 * Revision 1.5  2001/06/07 21:29:44  alan
 * Put in various portability changes to compile on Solaris w/o warnings.
 * The symptoms came courtesy of David Lee.
 *
 * Revision 1.4  2000/12/04 20:33:17  alan
 * OpenBSD fixes from Frank DENIS aka Jedi/Sector One <j@c9x.org>
 *
 * Revision 1.3  1999/10/05 06:17:29  alanr
 * Fixed various uninitialized variables
 *
 * Revision 1.2  1999/09/30 18:34:27  alanr
 * Matt Soffen's FreeBSD changes
 *
 * Revision 1.1.1.1  1999/09/23 15:31:24  alanr
 * High-Availability Linux
 *
 * Revision 1.4  1999/09/08 03:46:27  alanr
 * Changed things so they work when rearranged to match the FHS :-)
 *
 * Revision 1.3  1999/08/17 03:49:09  alanr
 * *** empty log message ***
 *
 */
