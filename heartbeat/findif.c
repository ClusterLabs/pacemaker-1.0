/* $Id: findif.c,v 1.38 2004/04/29 14:18:20 kevin Exp $ */
/*
 * findif.c:	Finds an interface which can route a given address
 *
 *	It's really simple to write in C, but hard to write in the shell...
 *
 *	This code is dependent on IPV4 addressing conventions...
 *		Sorry.
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
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
 *
 *
 *
 ***********************************************************
 *
 *	Our single argument is of the form:
 *		address[/netmask[/interface][/broadcast]]
 *
 *	So, the following forms are legal:
 *	         address
 *	         address/netmask
 *	         address/netmask/broadcast
 *	         address/netmask/interface
 *	         address/netmask/interface/broadcast
 *
 *     E.g.
 *		135.9.216.100
 *		135.9.216.100/24		Implies a 255.255.255.0 netmask
 *		135.9.216.100/24/255.255.255.0/135.9.216.255
 *		135.9.216.100/24/255.255.255.0/eth0
 *		135.9.216.100/24/255.255.255.0/eth0/135.9.216.255
 *
 *
 *	If the CIDR netmask is omitted, we choose the netmask associated with
 *	the route we selected.
 *
 *	If the broadcast address was omitted, we assume the highest address
 *	in the subnet.
 *
 *	If the interfaceis omitted, we choose the interface associated with
 *	the route we selected.
 *
 *
 *	See http://www.doom.net/docs/netmask.html for a table explaining
 *	CIDR address format and their relationship to life, the universe
 *	and everything.
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef __linux__
#undef __OPTIMIZE__
/*
 * This gets rid of some silly -Wtraditional warnings on Linux
 * because the netinet header has some slightly funky constants
 * in it.
 */
#endif /* __linux__ */

#include <netinet/in.h>
#include <arpa/inet.h>

#define DEBUG 0

int	OutputInCIDR=0;

void ConvertQuadToInt (char *dest, int destlen);

void ConvertBitsToMask (char *mask, int masklen);

/*
 * Different OSes offer different mechnisms to obtain this information.
 * Not all this can be determined at configure-time; need a run-time element.
 *
 * typedef ... SearchRoute ...:
 *	For routines that interface on these mechanisms.
 *	Return code:
 *		<0:	mechanism invalid, so try next mechanism
 *		0:	mechanism worked: good answer
 *		>0:	mechanism worked: bad answer
 *	On non-zero, errmsg may have been filled with an error message
 */
typedef int SearchRoute (char *address, struct in_addr *in
,        struct in_addr *addr_out, char *best_if, size_t best_iflen
,        unsigned long *best_netmask, char *errmsg
,	int errmsglen);

static SearchRoute SearchUsingProcRoute;
static SearchRoute SearchUsingRouteCmd;

SearchRoute *search_mechs[] = {
	&SearchUsingProcRoute,
	&SearchUsingRouteCmd,
	NULL
};

void GetAddress (char *inputaddress, char **address, char **netmaskbits
,	 char **bcast_arg, char **if_specified);

void ValidateNetmaskBits (char *netmaskbits, unsigned long *netmask);

int netmask_bits (unsigned long netmask);

const char *	cmdname = "findif";
void usage(void);

#define DELIM	'/'
#define	BAD_BROADCAST	(0L)
#define	MAXSTR	128

void
ConvertQuadToInt (char *dest, int destlen)
{
	unsigned ipquad[4] = { 0, 0, 0, 0 };
	unsigned long int intdest = 0;

	/*
	 * Convert a dotted quad into a value in the local byte order
 	 * 
 	 * 	ex.  192.168.123.1 would be converted to
 	 * 	1.123.168.192
 	 * 	1.7B.A8.C0
 	 * 	17BA8C0
 	 * 	24881344
	 *
	 * This replaces our argument with a new string -- in decimal...
 	 *
	 */

	while (strstr (dest, ".")) {
		*strstr(dest, ".") = ' ';
	}
	sscanf (dest, "%u%u%u%u", &ipquad[3], &ipquad[2], &ipquad[1]
	,	&ipquad[0]);
	
#if useMULT
	intdest = (ipquad[0] * 0x1000000) + (ipquad[1] * 0x10000) 
	+ 		(ipquad[2] * 0x100) + ipquad[3];
#else
	intdest = (	((ipquad[0]&0xff) <<24)
	|		((ipquad[1]&0xff) <<16)
	|		((ipquad[2]&0xff) <<8) 
	|	 	 (ipquad[3]&0xff));
#endif
	snprintf (dest, destlen, "%ld", intdest);
}

void
ConvertBitsToMask (char *mask, int masklen)
{
	int maskbits;
	unsigned long int maskflag = 0x1;
	unsigned long int longmask = 0;
	int i;
	int count;
	char *p;
	char *lastp=mask+masklen;
	char maskwithoutdots[12];

	/*
	 * Are there '.'s inside ?
	 * (like from the output of the "route get" command)
	 */
	if ((p = strtok(mask, ".")) != NULL) {
		i = strnlen(p, masklen);

		strncpy(maskwithoutdots, p, (i <= 3 ? i : 3));

		for (count=0; count < 3; count++) {
			p=strtok(NULL, ".");
			if (p != NULL) {
				i = strnlen(p, lastp-p);
				strncat(maskwithoutdots, p
				,	(i <= 3 && p != NULL ? i : 3));
			}
			else {
				count = 3;
			}
		}

		maskbits = atoi(maskwithoutdots);

	}else{
		maskbits = atoi(mask);
	}

	for (i = 0; i < 32; i++) {
		if (i <  maskbits) {
			longmask |= maskflag;
		}
		/* printf ("%d:%x:%x\n", i, longmask, maskflag); */
		maskflag = maskflag << 1;
	}

	/* printf ("longmask: %u (%X)\n", longmask, longmask); */
	snprintf (mask, masklen, "%ld", longmask);
}

static int
SearchUsingProcRoute (char *address, struct in_addr *in
, 	struct in_addr *addr_out, char *best_if, size_t best_iflen
,	unsigned long *best_netmask
,	char *errmsg, int errmsglen)
{
	long    	flags, refcnt, use, metric, gw, mask;
	unsigned long   dest;
	long		best_metric = LONG_MAX;
	
	char	buf[2048];
	char	interface[MAXSTR];
	FILE *routefd = NULL;

	if ((routefd = fopen(PROCROUTE, "r")) == NULL) {
		snprintf(errmsg, errmsglen
		,	"Cannot open %s for reading"
		,	PROCROUTE);
		return(-1);
	}

	/* Skip first (header) line */
	fgets(buf, sizeof(buf), routefd);
	while (fgets(buf, sizeof(buf), routefd) != NULL) {
		if (sscanf(buf, "%[^\t]\t%lx%lx%lx%lx%lx%lx%lx"
		,	interface, &dest, &gw, &flags, &refcnt, &use
		,	&metric, &mask)
		!= 8) {
			snprintf(errmsg, errmsglen, "Bad line in %s: %s"
			,	PROCROUTE, buf);
			return(1);
		}
		if ( (in->s_addr&mask) == (in_addr_t)(dest&mask)
		&&	metric < best_metric) {
			best_metric = metric;
			*best_netmask = mask;
			strncpy(best_if, interface, best_iflen);
		}
	}
	fclose(routefd);

	if (best_metric == LONG_MAX) {
		snprintf(errmsg, errmsglen, "No route to %s\n", address);
		return(1); 
	}

	return(0);
}

static int
SearchUsingRouteCmd (char *address, struct in_addr *in
,	struct in_addr *addr_out, char *best_if, size_t best_iflen
,	unsigned long *best_netmask
,	char *errmsg, int errmsglen)
{
	char	dest[20], mask[20];
	char	routecmd[MAXSTR];
	int	best_metric = INT_MAX;	
	char	buf[2048];
	char	interface[MAXSTR];
	char  *cp, *sp;
	int done = 0;
	FILE *routefd = NULL;

	
	/* Open route and get the information */
	snprintf (routecmd, errmsglen, "%s %s %s", ROUTE, ROUTEPARM, address);
	routefd = popen (routecmd, "r");
	if (routefd == NULL)
		return (-1);
	mask[0] = EOS;


	while ((done < 3) && fgets(buf, sizeof(buf), routefd)) {
		int buflen = strnlen(buf, sizeof(buf));
		cp = buf;

		sp = buf + buflen;
		while (sp!=buf && isspace((int)*(sp-1))) --sp;
		*sp = '\0';

		buf[buflen] = EOS;
		if (strstr (buf, "mask:")) {
			/*strsep(&cp, ":");cp++;*/
			cp = strtok(buf, ":");
			cp = strtok(NULL, ":");cp++;
			strncpy(mask, cp, sizeof(mask));
                  	done++;
		}

		if (strstr (buf, "interface:")) {
			/*strsep(&cp, ":");cp++;*/
			cp = strtok(buf, ":");
			cp = strtok(NULL, ":");cp++;
			strncpy(interface, cp, sizeof(interface));
                  	done++;
		}

		if (strstr (buf, "destination:")) {
			/*strsep(&cp, ":");cp++;*/
			cp = strtok(buf, ":");
			cp = strtok(NULL, ":");cp++;
			strncpy(dest, cp, sizeof(dest));
                  	done++;
		}
	}

	/*
	 * Check to see if mask isn't available.  It may not be
	 *	returned if multiple IP's are defined.
	 *	use 255.255.255.255 for mask then
	 */
	/* I'm pretty sure this is the wrong behavior...
	 * I think the right behavior is to declare an error and give up.
	 * The admin didn't define his routes correctly.  Fix them.
	 * It's useless to take over an IP address with no way to
	 * return packets to the originator.  Without the right subnet
	 * mask, you can't reply to any packets you receive.
	 */
	if (strnlen(mask, sizeof(mask)) == 0) {
		strncpy (mask, "255.255.255.255", sizeof(mask));
	}
	ConvertQuadToInt  (dest, sizeof(dest));
	ConvertBitsToMask (mask, sizeof(mask));
/*
	ConvertMaskToInt (mask);
*/

	if (inet_pton(AF_INET, address, addr_out) <= 0) {
		snprintf(errmsg, errmsglen
		,	"IP address [%s] not valid.", address);
		usage();
		return(1);
	}

	if ((in->s_addr & atoi(mask)) == (addr_out->s_addr & atoi(mask))) {
		best_metric = 0;
		*best_netmask = atoi(mask);
		strncpy(best_if, interface, best_iflen);
	}

	fclose(routefd);
	if (best_metric == INT_MAX) {
		snprintf(errmsg, errmsglen, "No route to %s\n", address);
		return(1);
	}

	return (0);
}

void
GetAddress (char *inputaddress, char **address, char **netmaskbits
,	 char **bcast_arg, char **if_specified)
{
	/*
	 *	See comment at the top of this file for
	 *	the format of the argument passed to this programme.
	 *	The format is broken out into the strings
	 *	passed to this function.
	 *
	 */
	*address = inputaddress;

	if ((*netmaskbits = strchr(*address, DELIM)) != NULL) {
		**netmaskbits = EOS;
		++(*netmaskbits);

		if ((*bcast_arg=strchr(*netmaskbits, DELIM)) != NULL) {
			**bcast_arg = EOS;
			++*bcast_arg;
			/* Did they specify the interface to use? */
			if (!isdigit((int)**bcast_arg)) {
				*if_specified = *bcast_arg;
				if ((*bcast_arg=strchr(*bcast_arg,DELIM))
				!=	NULL){
					**bcast_arg = EOS;
					++*bcast_arg;
				}else{
					*bcast_arg = NULL;
				}
				/* OK... Now we know the interface */
			}
		}
	}
}

void
ValidateNetmaskBits (char *netmaskbits, unsigned long *netmask)
{
	if (netmaskbits != NULL) {
		size_t	nmblen = strnlen(netmaskbits, 3);

		/* Maximum netmask is 32 */

		if (nmblen > 2 || nmblen == 0
		||	(strspn(netmaskbits, "0123456789") != nmblen)) {
			fprintf(stderr, "Invalid netmask specification"
			" [%s]", netmaskbits);
			usage();
		}else{
			unsigned long	bits = atoi(netmaskbits);

			if (bits < 1 || bits > 32) {
				fprintf(stderr
				,	"Invalid netmask specification [%s]"
				,	netmaskbits);
				usage();
			}

			bits = 32 - bits;
			*netmask = (1L<<(bits))-1L;
			*netmask = ((~(*netmask))&0xffffffffUL);
			*netmask = htonl(*netmask);
		}
	}
}

int
netmask_bits(unsigned long netmask) {
	int	j;

	netmask = netmask & 0xFFFFFFFFUL;

	for (j=0; j <= 32; ++j) {
		if ((netmask >> j)&0x1) {
			break;
		}
	}

	return 32 - j;
}

int
main(int argc, char ** argv) {

	char *	iparg = NULL;
	char *	address = NULL;
	char *	bcast_arg = NULL;
	char *	netmaskbits = NULL;
	struct in_addr	in;
	struct in_addr	addr_out;
	unsigned long	netmask;
	char	best_if[MAXSTR];
	char *	if_specified = NULL;
	unsigned long	best_netmask = INT_MAX;
	int		argerrs	= 0;

	cmdname=argv[0];


	memset(&addr_out, 0, sizeof(addr_out));
	memset(&in, 0, sizeof(in));

	switch (argc) {
	case 2:	/* No -C argument */
		if (argv[1][0] == '-') {
			argerrs=1;
		}
		iparg=argv[1];
		break;
	case 3: /* Hopefully a -C argument */
		if (strncmp(argv[1], "-C", sizeof("-C")) != 0) {
			argerrs=1;
		}
		OutputInCIDR=1;
		iparg=argv[2];
		break;
	default:
		argerrs=1;
		break;
	}
	if (argerrs) {
		usage();
		return(1);
	}

	GetAddress (iparg, &address, &netmaskbits, &bcast_arg
	,	 &if_specified);

	/* Is the IP address we're supposed to find valid? */
	 
	if (inet_pton(AF_INET, address, (void *)&in) <= 0) {
		fprintf(stderr, "IP address [%s] not valid.", address);
		usage();
		return(1);
	}

	/* Validate the netmaskbits field */
	ValidateNetmaskBits (netmaskbits, &netmask);

	if (if_specified != NULL) {
		strncpy(best_if, if_specified, sizeof(best_if));
		*(best_if + sizeof(best_if) - 1) = '\0';
	}else{
		SearchRoute **sr = search_mechs;
		char errmsg[MAXSTR] = "No valid mecahnisms";
		int rc = -1;

		strcpy(best_if, "UNKNOWN"); /* just in case */

		while (*sr) {
			errmsg[0] = '\0';
			rc = (*sr) (address, &in, &addr_out, best_if
			,	sizeof(best_if)
			,	&best_netmask, errmsg, sizeof(errmsg));
			if (rc >= 0) {		/* Mechanism worked */
				break;
			}
			sr++;
		}
		if (rc != 0) {	/* No route, or all mechanisms failed */
			if (*errmsg) {
				fprintf(stderr, "%s", errmsg);
				return(1);
			}
		}
	}

	if (netmaskbits) {
		best_netmask = netmask;
	}else if (best_netmask == 0L) {
		fprintf(stderr
		,	"ERROR: Cannot use default route w/o netmask [%s]\n"
		,	 address);
		return(1);
	}
		


	/* Did they tell us the broadcast address? */

	if (bcast_arg) {
		best_netmask = htonl(best_netmask);
		/* Yes, they gave us a broadcast address */
		if (!OutputInCIDR) {
			printf("%s\t netmask %d.%d.%d.%d\tbroadcast %s\n"
			,	best_if
                	,       (int)((best_netmask>>24) & 0xff)
                	,       (int)((best_netmask>>16) & 0xff)
                	,       (int)((best_netmask>>8) & 0xff)
                	,       (int)(best_netmask & 0xff)
			,	bcast_arg);
		}else{
			printf("%s\t netmask %d\tbroadcast %s\n"
			,	best_if
			,	netmask_bits(best_netmask)
			,	bcast_arg);
		}
	}else{
		/* No, we use a common broadcast address convention */
		unsigned long	def_bcast;

			/* Common broadcast address */
		def_bcast = (in.s_addr | (~best_netmask));
#if DEBUG
		fprintf(stderr, "best_netmask = %08lx, def_bcast = %08lx\n"
		,	best_netmask,  def_bcast);
#endif

		/* Make things a bit more machine-independent */
		best_netmask = htonl(best_netmask);
		def_bcast = htonl(def_bcast);
		if (!OutputInCIDR) {
			printf("%s\tnetmask %d.%d.%d.%d\tbroadcast %d.%d.%d.%d\n"
			,       best_if
			,       (int)((best_netmask>>24) & 0xff)
			,       (int)((best_netmask>>16) & 0xff)
			,       (int)((best_netmask>>8) & 0xff)
			,       (int)(best_netmask & 0xff)
			,       (int)((def_bcast>>24) & 0xff)
			,       (int)((def_bcast>>16) & 0xff)
			,       (int)((def_bcast>>8) & 0xff)
			,       (int)(def_bcast & 0xff));
		}else{
			printf("%s\tnetmask %d\tbroadcast %d.%d.%d.%d\n"
			,       best_if
			,	netmask_bits(best_netmask)
			,       (int)((def_bcast>>24) & 0xff)
			,       (int)((def_bcast>>16) & 0xff)
			,       (int)((def_bcast>>8) & 0xff)
			,       (int)(def_bcast & 0xff));
		}
	}
	return(0);
}

void
usage()
{
	fprintf(stderr, "\n"
		"%s version " VERSION " Copyright Alan Robertson\n"
		"\n"
		"Usage: %s address[/netmask[/interface][/broadcast]]\n"
		"\n"
		"Where:\n"
		"    address: IP address of the new virtual interface\n"
		"    netmask: CIDR netmask of the network that "
			"address belongs to\n"
		"    interface: interface to add the virtual interface to\n"
		"    broadcast: broadcast address of the network that "
			"address belongs to\n"
		"\n"
		"Options:\n"
		"    -C: Output netmask as the number of bits rather "
			"than as 4 octets.\n"
	,	cmdname, cmdname);
	exit(1);
}
		
/*
Iface	Destination	Gateway 	Flags	RefCnt	Use	Metric	Mask		MTU	Window	IRTT                                                       
eth0	33D60987	00000000	0005	0	0	0	FFFFFFFF	0	0	0                                                                               
eth0	00D60987	00000000	0001	0	0	0	00FFFFFF	0	0	0                                                                               
lo	0000007F	00000000	0001	0	0	0	000000FF	0	0	0                                                                                 
eth0	00000000	FED60987	0003	0	0	0	00000000	0	0	0                                                                               

netstat -rn outpug from RedHat Linux 6.0
Kernel IP routing table
Destination     Gateway         Genmask         Flags   MSS Window  irtt Iface
192.168.85.2    0.0.0.0         255.255.255.255 UH        0 0          0 eth1
10.0.0.2        0.0.0.0         255.255.255.255 UH        0 0          0 eth2
208.132.134.61  0.0.0.0         255.255.255.255 UH        0 0          0 eth0
208.132.134.32  0.0.0.0         255.255.255.224 U         0 0          0 eth0
192.168.85.0    0.0.0.0         255.255.255.0   U         0 0          0 eth1
10.0.0.0        0.0.0.0         255.255.255.0   U         0 0          0 eth2
127.0.0.0       0.0.0.0         255.0.0.0       U         0 0          0 lo
0.0.0.0         208.132.134.33  0.0.0.0         UG        0 0          0 eth0

|--------------------------------------------------------------------------------
netstat -rn output from FreeBSD 3.3
Routing tables

Internet:
Destination        Gateway            Flags     Refs     Use     Netif Expire
default            209.61.94.161      UGSc        3        8      pn0
192.168            link#1             UC          0        0      xl0
192.168.0.2        0:60:8:a4:91:fd    UHLW        0       38      lo0
192.168.0.255      ff:ff:ff:ff:ff:ff  UHLWb       1     7877      xl0
209.61.94.160/29   link#2             UC          0        0      pn0
209.61.94.161      0:a0:cc:26:c2:ea   UHLW        6    17265      pn0   1105
209.61.94.162      0:a0:cc:27:1c:fb   UHLW        1      568      pn0   1098
209.61.94.163      0:a0:cc:29:1f:86   UHLW        0     4749      pn0   1095
209.61.94.166      0:a0:cc:27:2d:e1   UHLW        0       12      lo0
209.61.94.167      ff:ff:ff:ff:ff:ff  UHLWb       0    10578      pn0

|--------------------------------------------------------------------------------
netstat -rn output from FreeBSD 4.2
Routing tables

Internet:
Destination        Gateway            Flags     Refs     Use     Netif Expire
default            64.65.195.1        UGSc        1       11      dc0
64.65.195/24       link#1             UC          0        0      dc0 =>
64.65.195.1        0:3:42:3b:0:dd     UHLW        2        0      dc0   1131
64.65.195.184      0:a0:cc:29:1f:86   UHLW        2    18098      dc0   1119
64.65.195.194      0:a0:cc:27:2d:e1   UHLW        3   335161      dc0    943
64.65.195.200      52:54:0:db:33:b3   UHLW        0       13      dc0    406
64.65.195.255      ff:ff:ff:ff:ff:ff  UHLWb       1      584      dc0
127.0.0.1          127.0.0.1          UH          0        0      lo0
192.168/16         link#2             UC          0        0      vx0 =>
192.168.0.1        0:20:af:e2:f0:36   UHLW        0        2      lo0
192.168.255.255    ff:ff:ff:ff:ff:ff  UHLWb       0        1      vx0

Internet6:
Destination                       Gateway                       Flags      Netif Expire
::1                               ::1                           UH          lo0
fe80::%dc0/64                     link#1                        UC          dc0
fe80::%vx0/64                     link#2                        UC          vx0
fe80::%lo0/64                     fe80::1%lo0                   Uc          lo0
ff01::/32                         ::1                           U           lo0
ff02::%dc0/32                     link#1                        UC          dc0
ff02::%vx0/32                     link#2                        UC          vx0
ff02::%lo0/32                     fe80::1%lo0                   UC          lo0
*/

/* 
 * $Log: findif.c,v $
 * Revision 1.38  2004/04/29 14:18:20  kevin
 * Be nice to the non C99 compliant comilers. (Declarations come first)
 *
 * Revision 1.37  2004/04/29 07:12:19  lars
 * add missing return
 *
 * Revision 1.36  2004/04/27 11:33:19  horms
 * Honour -C flag when broadcast is supplied
 *
 * Revision 1.35  2004/04/27 11:25:59  horms
 * Document -C option to findif
 *
 * Revision 1.34  2004/03/25 07:55:39  alan
 * Moved heartbeat libraries to the lib directory.
 *
 * Revision 1.33  2004/02/17 22:11:57  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.32  2004/01/20 10:16:08  horms
 * Should be strncpy not strncmp
 *
 * Revision 1.31  2004/01/14 17:51:07  alan
 * Put in some minor code cleanups suggested by Emily Ratliff.
 *
 * Revision 1.30  2003/09/26 05:55:54  alan
 * Fixed an undefined variable warning...
 *
 * Revision 1.29  2003/07/02 22:27:00  alan
 * Fixed a bug in the broadcast output format for CIDR -C option for findif.
 *
 * Revision 1.28  2003/06/18 03:43:42  horms
 * spelling
 *
 * Revision 1.27  2003/04/19 03:59:15  alan
 * Put in an enhancement to findif to allow Tuomo Soini to get the output he
 * wants in CIDR format.
 *
 * Revision 1.26  2003/04/15 01:58:54  horms
 * turn debuging off
 *
 * Revision 1.25  2003/04/14 10:22:44  horms
 * should now work correclty with /32
 *
 * Revision 1.24  2003/04/14 09:53:17  horms
 * minor reformating
 *
 * Revision 1.23  2003/02/27 06:46:35  horms
 * Slightly better documentation of the options to findif and thus IPaddr.
 * In particular, how to explicitly define the base interface for a VIP.
 *
 * Revision 1.22  2003/02/07 08:37:16  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.21  2003/02/05 09:06:33  horms
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
 * Revision 1.20  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.19  2002/10/31 19:10:48  msoffen
 * Corrected the find route with "ROUTE" command handles the default route properly.
 *
 * Revision 1.18  2002/07/08 04:14:12  alan
 * Updated comments in the front of various files.
 * Removed Matt's Solaris fix (which seems to be illegal on Linux).
 *
 * Revision 1.17  2002/04/29 07:18:44  alan
 * Patch from Thomas Hepper for Solaris.
 *
 * Revision 1.16  2001/10/13 21:03:12  alan
 * Put in a fix to the findif command which makes it test which way to get
 * routing information at run time rather than at compile time.
 * This permits configure to be run as a normal user on environments where
 * /proc/route isn't available.
 *
 * Revision 1.15  2001/10/03 05:45:56  alan
 * Added a couple of patches from Matt Soffen:
 * Make a debug statement conditional ;-)
 * Fix configure so it does things correctly on FreeBSD and Solaris findif.c
 * configuration parameters.
 *
 * Revision 1.14  2001/09/27 17:02:34  alan
 * Shortened alarm time in write in serial.c
 * Put in a handful of Solaris warning-elimination patches.
 *
 * Revision 1.13  2001/08/10 17:35:37  alan
 * Removed some files for comm plugins
 * Moved the rest of the software over to use the new plugin system for comm
 * plugins.
 *
 * Revision 1.12  2001/07/17 15:00:04  alan
 * Put in Matt's changes for findif, and committed my changes for the new module loader.
 * You now have to have glib.
 *
 * Revision 1.11  2001/06/23 04:30:26  alan
 * Changed the code to use inet_pton() when it's available, and
 * emulate it when it's not...  Patch was from Chris Wright.
 *
 * Revision 1.10  2001/06/07 21:29:44  alan
 * Put in various portability changes to compile on Solaris w/o warnings.
 * The symptoms came courtesy of David Lee.
 *
 * Revision 1.9  2001/05/10 22:36:37  alan
 * Deleted Makefiles from CVS and made all the warnings go away.
 *
 * Revision 1.8  2001/02/05 04:55:27  alan
 * Sparc fix from Uzi.
 *
 * Revision 1.7  2000/08/30 20:32:39  alan
 * Fixed a byte ordering problem in findif.c.  There's probably another one in the code yet.
 *
 * Revision 1.6  2000/08/13 20:37:49  alan
 * Fixed a bug related to byte-ordering in findif.c.  Thanks to
 *         Lars Kellogg-Stedman for the fix.  There are probably some
 * 	related to byte ordering in input that still need fixing...
 *
 * Revision 1.5  2000/07/26 05:17:19  alan
 * Added GPL license statements to all the code.
 *
 * Revision 1.4  2000/06/21 04:34:48  alan
 * Changed henge.com => linux-ha.org and alanr@henge.com => alanr@suse.com
 *
 * Revision 1.3  2000/01/26 15:16:48  alan
 * Added code from Michael Moerz <mike@cubit.at> to keep findif from
 * core dumping if /proc/route can't be read.
 *
 * Revision 1.2  1999/09/30 18:34:27  alanr
 * Matt Soffen's FreeBSD changes
 *
 * Revision 1.1.1.1  1999/09/23 15:31:24  alanr
 * High-Availability Linux
 *
 * Revision 1.5  1999/09/22 16:49:03  alanr
 * Put in the ability to explicitly specify the interface on the command line argument.
 * This was requested by Lars Marowsky-Bree.
 *
 * Revision 1.4  1999/09/16 15:03:24  alanr
 * fixed a glaring bug in CIDR style addresses...
 *
 * Revision 1.3  1999/09/12 06:23:00  alanr
 * Fixed calculation of the broadcast address.
 * Disallowed using default route to locate interface, unless a netmask is specified.
 *
 * Revision 1.2  1999/08/17 03:45:32  alanr
 * added RCS log to end of file...
 *
 */
