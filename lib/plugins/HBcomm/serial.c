const static char _serial_c_Id [] = "$Id: serial.c,v 1.28 2004/01/20 16:23:11 alan Exp $";

/*
 * Linux-HA serial heartbeat code
 *
 * The basic facilities for round-robin (ring) heartbeats are
 * contained within.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <portability.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <heartbeat.h>
#include <HBcomm.h>
#include <clplumbing/longclock.h>
#include <clplumbing/timers.h>

#define PIL_PLUGINTYPE		HB_COMM_TYPE
#define PIL_PLUGINTYPE_S	HB_COMM_TYPE_S
#define PIL_PLUGIN		serial
#define PIL_PLUGIN_S		"serial"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL
#include <pils/plugin.h>


struct serial_private {
        char *			ttyname;
        int			ttyfd;		/* For direct TTY i/o */ 
	int			consecutive_errors;
        struct hb_media*	next;
};

static int		serial_baud = 0;
static const char *	baudstring;

/* Used to maintain a list of our serial ports in the ring */
static struct hb_media*		lastserialport;

static struct hb_media*	serial_new(const char * value);
static struct ha_msg*	serial_read(struct hb_media *mp);
static char *		ttygets(char * inbuf, int length
,				struct serial_private *tty);
static int		serial_write(struct hb_media*mp, struct ha_msg *msg);
static int		serial_open(struct hb_media* mp);
static int		ttysetup(int fd, const char * ourtty);
static int		opentty(char * serial_device);
static int		serial_close(struct hb_media* mp);
static int		serial_init(void);

static void		serial_localdie(void);

static int		serial_mtype(char **buffer);
static int		serial_descr(char **buffer);
static int		serial_isping(void);


/*
 * serialclosepi is called as part of unloading the serial HBcomm plugin.
 * If there was any global data allocated, or file descriptors opened, etc.
 * which is associated with the plugin, and not a single interface
 * in particular, here's our chance to clean it up.
 */

static void
serialclosepi(PILPlugin*pi)
{
	serial_localdie();
}


/*
 * serialcloseintf called as part of shutting down the serial HBcomm interface.
 * If there was any global data allocated, or file descriptors opened, etc.
 * which is associated with the serial implementation, here's our chance
 * to clean it up.
 */
static PIL_rc
serialcloseintf(PILInterface* pi, void* pd)
{
	return PIL_OK;
}

static struct hb_media_fns serialOps ={
	serial_new,	/* Create single object function */
	NULL,		/* whole-line parse function */
	serial_open,
	serial_close,
	serial_read,
	serial_write,
	serial_mtype,
	serial_descr,
	serial_isping,
};

PIL_PLUGIN_BOILERPLATE("1.0", Debug, serialclosepi);
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
	PIL_rc	rc;
	/* Force the compiler to do a little type checking */
	(void)(PILPluginInitFun)PIL_PLUGIN_INIT;

	PluginImports = imports;
	OurPlugin = us;

	/* Register ourself as a plugin */
	imports->register_plugin(us, &OurPIExports);  

	/*  Register our interface implementation */
 	rc = imports->register_interface(us, PIL_PLUGINTYPE_S
	,	PIL_PLUGIN_S
	,	&serialOps
	,	serialcloseintf		/*close */
	,	&OurInterface
	,	(void*)&OurImports
	,	interfprivate); 
	serial_init();
	return rc;
}

#define		IsTTYOBJECT(mp)	((mp) && ((mp)->vf == (void*)&serial_media_fns))
//#define		TTYASSERT(mp)	ASSERT(IsTTYOBJECT(mp))
#define		TTYASSERT(mp)
#define		RTS_WARNTIME	3600

static int
serial_mtype (char **buffer) { 
	
	*buffer = MALLOC((strlen("serial") * sizeof(char)) + 1);

	strcpy(*buffer, "serial");

	return strlen("serial");
}

static int
serial_descr (char **buffer) { 

	const char *str = "serial ring";	

	*buffer = MALLOC((strlen(str) * sizeof(char)) + 1);

	strcpy(*buffer, str);

	return strlen(str);
}

static int
serial_isping (void) {
	return 0;
}

/* Initialize global serial data structures */
static int
serial_init (void)
{
	(void)_serial_c_Id;
	(void)_heartbeat_h_Id;
	(void)_ha_msg_h_Id;	/* ditto */
	lastserialport = NULL;

	/* This eventually ought be done through the configuration API */
	if (serial_baud <= 0) {
		if ((baudstring  = OurImports->ParamValue("baud")) != NULL) {
			serial_baud = OurImports->StrToBaud(baudstring);
		}
	}
	if (serial_baud <= 0 || baudstring == NULL) {
		serial_baud = DEFAULTBAUD;
		baudstring  = DEFAULTBAUDSTR;
	}
	if (ANYDEBUG) {
		LOG(PIL_DEBUG, "serial_init: serial_baud = 0x%x"
		,	serial_baud);
	}
	return(HA_OK);
}

/* Process a serial port declaration */
static struct hb_media *
serial_new (const char * port)
{
	struct	stat	sbuf;
	struct hb_media * ret;


	/* Let's see if this looks like it might be a serial port... */
	if (*port != '/') {
		LOG(PIL_CRIT
		,	"Serial port not full pathname [%s] in config file"
		,	port);
		return(NULL);
	}

	if (stat(port, &sbuf) < 0) {
		LOG(PIL_CRIT, "Nonexistent serial port [%s] in config file"
		,	port);
		return(NULL);
	}
	if (!S_ISCHR(sbuf.st_mode)) {
		LOG(PIL_CRIT
		,	"Serial port [%s] not a char device in config file"
		,	port);
		return(NULL);
	}

	ret = (struct hb_media*)MALLOC(sizeof(struct hb_media));
	if (ret != NULL) {
		struct serial_private * sp;
		sp = (struct serial_private*)
			MALLOC(sizeof(struct serial_private));
		if (sp != NULL)  {
			/*
			 * This implies we have to process the "new"
			 * for this object in the parent process of us all...
			 * otherwise we can't do this linking stuff...
			 */
			sp->next = lastserialport;
			lastserialport=ret;
			sp->ttyname = (char *)MALLOC(strlen(port)+1);
			sp->consecutive_errors = 0;
			strcpy(sp->ttyname, port);
			ret->name = sp->ttyname;
			ret->pd = sp;
		}else{
			FREE(ret);
			ret = NULL;
			LOG(PIL_CRIT, "Out of memory (private serial data)");
		}
	}else{
		LOG(PIL_CRIT, "Out of memory (serial data)");
	}
	return(ret);
}

static int
serial_open (struct hb_media* mp)
{
	struct serial_private*	sp;

	TTYASSERT(mp);
	sp = (struct serial_private*)mp->pd;
	if (OurImports->devlock(sp->ttyname) < 0) {
		LOG(PIL_CRIT, "cannot lock line %s", sp->ttyname);
		return(HA_FAIL);
	}
	if ((sp->ttyfd = opentty(sp->ttyname)) < 0) {
		return(HA_FAIL);
	}
	LOG(PIL_INFO, "Starting serial heartbeat on tty %s (%s baud)"
	,	sp->ttyname, baudstring);
	return(HA_OK);
}

static int
serial_close (struct hb_media* mp)
{
	struct serial_private*	sp;
	int rc;

	TTYASSERT(mp);
	sp = (struct serial_private*)mp->pd;
	rc = close(sp->ttyfd) < 0 ? HA_FAIL : HA_OK;
	OurImports->devunlock(sp->ttyname);
	return rc;
}

/* Set up a serial line the way we want it be done */
static int
ttysetup(int fd, const char * ourtty)
{
	struct TERMIOS	ti;

	if (GETATTR(fd, &ti) < 0) {
		LOG(PIL_CRIT, "cannot get tty attributes: %s", strerror(errno));
		return(HA_FAIL);
	}

#ifndef IUCLC
#	define IUCLC	0	/* Ignore it if not supported */
#endif
#ifndef CBAUD
#	define CBAUD	0
#endif

	ti.c_iflag &= ~(IGNBRK|IUCLC|IXANY|IXOFF|IXON|ICRNL|PARMRK);
	/* Unsure if I want PARMRK or not...  It may not matter much */
	ti.c_iflag |=  (INPCK|ISTRIP|IGNCR|BRKINT);

	ti.c_oflag &= ~(OPOST);
	ti.c_cflag &= ~(CBAUD|CSIZE|PARENB);

#ifndef CRTSCTS
#	define CRTSCTS 0	/* AIX and others don't have this */
#endif

/*
 * Make a silly Linux/Gcc -Wtraditional warning go away
 * This is not my fault, you understand...                       ;-)
 * Suggestions on how to better work around it would be welcome.
 */
#if CRTSCTS == 020000000000
#	undef CRTSCTS
#	define CRTSCTS 020000000000U
#endif

	ti.c_cflag |=  (serial_baud|(unsigned)CS8|(unsigned)CREAD
	|		(unsigned)CLOCAL|(unsigned)CRTSCTS);

	ti.c_lflag &= ~(ICANON|ECHO|ISIG);
#ifdef HAVE_TERMIOS_C_LINE
	ti.c_line = 0;
#endif
	ti.c_cc[VMIN] = 1;
	ti.c_cc[VTIME] = 1;
	if (SETATTR(fd, &ti) < 0) {
		LOG(PIL_CRIT, "cannot set tty attributes: %s"
		,	strerror(errno));
		return(HA_FAIL);
	}
	if (ANYDEBUG) {
		LOG(PIL_DEBUG, "tty setup on %s complete.", ourtty);
		LOG(PIL_DEBUG, "Baud rate set to: 0x%x", serial_baud);
		LOG(PIL_DEBUG, "ti.c_iflag = 0x%x", ti.c_iflag);
		LOG(PIL_DEBUG, "ti.c_oflag = 0x%x", ti.c_oflag);
		LOG(PIL_DEBUG, "ti.c_cflag = 0x%x", ti.c_cflag);
		LOG(PIL_DEBUG, "ti.c_lflag = 0x%x", ti.c_lflag);
	}
	/* For good measure */
	FLUSH(fd);
	tcsetpgrp(fd, getsid(getpid()));
	return(HA_OK);
}

#ifndef O_NOCTTY
#	define O_NOCTTY	0	/* Ignore it if not supported */
#endif

/* Open a tty and set it's line parameters */
static int
opentty(char * serial_device)
{
	int	fd;

	if ((fd=open(serial_device, O_RDWR|O_NOCTTY)) < 0 ) {
		LOG(LOG_CRIT, "cannot open %s: %s", serial_device
		,	strerror(errno));
		return(fd);
	}
	if (!ttysetup(fd, serial_device)) {
		close(fd);
		return(-1);
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC)) {
		LOG(PIL_WARN,"Error setting the close-on-exec flag: %s"
		,	strerror(errno));
	}
	/* Cause the other guy to flush his I/O */
	tcsendbreak(fd, 0);
	return(fd);
}

static struct hb_media* ourmedia = NULL;

static void
serial_localdie(void)
{
	int	ourtty;
	if (!ourmedia || !ourmedia->pd) {
		return;
	}
	ourtty = ((struct serial_private*)(ourmedia->pd))->ttyfd;
	if (ourtty >= 0) {
		if (ANYDEBUG) {
			LOG(PIL_DEBUG, "serial_localdie: Flushing tty");
		}
		tcflush(ourtty, TCIOFLUSH);
	}
}

/* This function does all the writing to our tty ports */
static int
serial_write (struct hb_media*mp, struct ha_msg*m)
{
	char *			str;

	int			wrc;
	int			size;
	int			ourtty;
	static gboolean		warnyet=FALSE;
	static longclock_t	warninterval;
	static longclock_t	lastwarn;

	TTYASSERT(mp);

	if (!warnyet) {
		warninterval = msto_longclock(RTS_WARNTIME*1000L);
	}
	ourmedia = mp;	/* Only used for the "localdie" function */
	OurImports->RegisterCleanup(serial_localdie);
	ourtty = ((struct serial_private*)(mp->pd))->ttyfd;
	if ((str=msg2string(m)) == NULL) {
		LOG(PIL_CRIT, "Cannot convert message to tty string");
		return(HA_FAIL);
	}
	size = strlen(str);
	if (DEBUGPKT) {
		LOG(PIL_DEBUG, "Sending pkt to %s [%d bytes]"
		,	mp->name, size);
	}
	if (DEBUGPKTCONT) {
		LOG(PIL_DEBUG, str);
	}
	setmsalarm(500);
	wrc = write(ourtty, str, size);
	cancelmstimer();
	if (DEBUGPKTCONT) {
		LOG(PIL_DEBUG, "serial write returned %d", wrc);
	}

	if (wrc < 0 || wrc != size) {
		if (DEBUGPKTCONT && wrc < 0) {
			LOG(PIL_DEBUG, "serial write errno was %d", errno);
		}
		if (wrc > 0 || (wrc < 0 && errno == EINTR)) {
			longclock_t	now = time_longclock();
			tcflush(ourtty, TCIOFLUSH);

			if (!warnyet
			||	cmp_longclock(sub_longclock(now, lastwarn)
			,		warninterval) >= 0) {

				lastwarn = now;
				warnyet = TRUE;
				LOG(LOG_ERR
				,	"TTY write timeout on [%s]"
				" (no connection or bad cable"
				"? [see documentation])"
				,	mp->name);
			}
		}else{
			LOG(PIL_CRIT, "TTY write failure on [%s]: %s"
			,	mp->name, strerror(errno));
		}
	}
	ha_free(str);
	return(HA_OK);
}

/* This function does all the reading from our tty ports */
static struct ha_msg *
serial_read (struct hb_media*mp)
{
	char buf[MAXLINE];
	const char *		bufmax = buf + sizeof(buf);
	struct hb_media*	sp;
	struct serial_private*	thissp;
	struct ha_msg*		ret;
	char *			newmsg = NULL;
	int			newmsglen = 0;
	int			startlen;
	const char *		start = MSG_START;
	const char *		end = MSG_END;
	int			endlen;
	struct serial_private*	spp;

	TTYASSERT(mp);
	thissp = (struct serial_private*)mp->pd;

	if ((ret = ha_msg_new(0)) == NULL) {
		LOG(PIL_CRIT, "Cannot get new message");
		return(NULL);
	}
	startlen = strlen(start);
	if (start[startlen-1] == '\n') {
		--startlen;
	}
	endlen = strlen(end);
	if (end[endlen-1] == '\n') {
		--endlen;
	}
	/* Skip until we find a MSG_START (hopefully we skip nothing) */
	while (ttygets(buf, MAXLINE, thissp) != NULL
	&&	strncmp(buf, start, startlen) != 0) {
		/* Nothing */
	}

	while (ttygets(buf, MAXLINE, thissp) != NULL
	&&	strncmp(buf, MSG_END, endlen) != 0) {

		/* Add the "name=value" string on this line to the message */
		if (ha_msg_add_nv(ret, buf, bufmax) != HA_OK) {
			ha_msg_del(ret);
			return(NULL);
		}
	}

	if (buf[0] == EOS || ret->nfields < 1) {
		ha_msg_del(ret);
		return NULL;
	}else{
		thissp->consecutive_errors=0;
	}

	/* Should this message should continue around the ring? */

	if (!isauthentic(ret) || !should_ring_copy_msg(ret)) {
		/* Avoid infinite loops... Ignore this message */
		return(ret);
	}


	/* Add Name=value pairs until we reach MSG_END or EOF */
	/* Forward message to other port in ring (if any) */
	for (sp=lastserialport; sp; sp=spp->next) {
		TTYASSERT(sp);
		spp = (struct serial_private*)sp->pd;
		if (sp == mp) {
			/* That's us! */
			continue;
		}

		/* Modify message, decrementing TTL (and reauthenticate it) */
		if (newmsglen) {
			const char *		ttl_s;
			int			ttl;
			char			nttl[8];

			/* Decrement TTL in the message before forwarding */
			if ((ttl_s = ha_msg_value(ret, F_TTL)) == NULL) {
				return(ret);
			}
			ttl = atoi(ttl_s);
			sprintf(nttl, "%d", ttl-1);
			ha_msg_mod(ret, F_TTL, nttl);

			/* Re-authenticate message */
			add_msg_auth(ret);

			if ((newmsg = msg2string(ret)) == NULL) {
				LOG(PIL_CRIT, "Cannot convert serial msg to string");
				continue;
			}
			newmsglen = strlen(newmsg);
		}
		/*
		 * This will eventually have to be changed
		 * if/when we change from FIFOs to more general IPC
		 */
		if (newmsglen) {
			/*
			 * I suppose it just becomes an IPC abstraction
			 * and we issue a "msgput" or some such on it...
			 */
#if 0
			if (DEBUGPKT) {
				ha_log(LOG_DEBUG
				,	"serial_read: writing %s"
				" (len: %d) to %d"
				,	newmsg, newmsglen
				,	sp->wpipe[P_WRITEFD]);
			}
			/* FIXME!!!!! */
			/* I think the code as it stands never calls this
			 * anyway.  It *should* call this, but I don't
			 * see any way for newmsglen to become nonzero
			 */
			write(sp->wchan[P_WRITEFD], newmsg, newmsglen);
#else
			;
#endif
		}
	}

	if (newmsglen) {
		ha_free(newmsg);
	}
	return(ret);
}


/* Gets function for our tty */
static char *
ttygets(char * inbuf, int length, struct serial_private *tty)
{
	char *	cp;
	char *	end = inbuf + length;
	int	rc;
	int	fd = tty->ttyfd;

	for(cp=inbuf; cp < end; ++cp) {
		int saverr;
		errno = 0;
		/* One read per char -- yecch  (but it's easy) */
		rc = read(fd, cp, 1);
		saverr = errno;
		OurImports->CheckForEvents();
		errno = saverr;
		if (rc != 1) {
			if (rc == 0 || errno == EINTR) {
				LOG(PIL_CRIT, "EOF in ttygets [%s]: %s [%d]"
				,	tty->ttyname, strerror(errno), rc);
				++tty->consecutive_errors;
				tcsetpgrp(fd, getsid(getpid()));
				if ((tty->consecutive_errors % 10) == 0) {
					LOG(PIL_WARN
					,	"10 consecutive EOF"
					" errors from serial port %s"
					,	tty->ttyname);
					LOG(PIL_INFO
					,	"%s pgrp: %d", tty->ttyname
					,	tcgetpgrp(fd));
					sleep(10);
				}
				return(NULL);
			}
			errno = 0;
			continue;
		}else{
			tty->consecutive_errors = 0;
		}
			
		if (*cp == '\r' || *cp == '\n') {
			break;
		}
	}
	*cp = '\0';
	return(inbuf);
}
/*
 * $Log: serial.c,v $
 * Revision 1.28  2004/01/20 16:23:11  alan
 * Fixed an oversight which turns out to have been luckily-benign in practice.
 *
 * Revision 1.27  2003/05/21 21:56:50  alan
 * Minor tweak to the last tty fix from Carson Gaspar.
 *
 * Revision 1.26  2003/05/21 21:55:07  alan
 * Put in a bug fix from Carson Gaspar <carson@taltos.org> to open our
 * serial ttys with O_NOCTTY to avoid having them become our controlling
 * ttys.
 *
 * Revision 1.25  2003/04/15 23:09:52  alan
 * Continuing saga of the semi-major heartbeat process restructure.
 *
 * Revision 1.24  2003/02/07 08:37:18  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.23  2003/02/05 09:06:34  horms
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
 * Revision 1.22  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.21  2002/10/22 17:41:58  alan
 * Added some documentation about deadtime, etc.
 * Switched one of the sets of FIFOs to IPC channels.
 * Added msg_from_IPC to ha_msg.c make that easier.
 * Fixed a few compile errors that were introduced earlier.
 * Moved hb_api_core.h out of the global include directory,
 * and back into a local directory.  I also make sure it doesn't get
 * installed.  This *shouldn't* cause problems.
 * Added a ipc_waitin() function to the IPC code to allow you to wait for
 * input synchronously if you really want to.
 * Changes the STONITH test to default to enabled.
 *
 * Revision 1.20  2002/10/18 07:16:10  alan
 * Put in Horms big patch plus a patch for the apcmastersnmp code where
 * a macro named MIN returned the MAX instead.  The code actually wanted
 * the MAX, so when the #define for MIN was surrounded by a #ifndef, then
 * it no longer worked...  This fix courtesy of Martin Bene.
 * There was also a missing #include needed on older Linux systems.
 *
 * Revision 1.19  2002/06/16 06:11:26  alan
 * Put in a couple of changes to the PILS interfaces
 *  - exported license information (name, URL)
 *  - imported malloc/free
 *
 * Revision 1.18  2002/04/24 12:11:40  alan
 * updated the copyright date.
 *
 * Revision 1.17  2002/04/13 22:35:08  alan
 * Changed ha_msg_add_nv to take an end pointer to make it safer.
 * Added a length parameter to string2msg so it would be safer.
 * Changed the various networking plugins to use the new string2msg().
 *
 * Revision 1.16  2002/04/09 21:53:27  alan
 * A large number of minor cleanups related to exit, cleanup, and process
 * management.  It all looks reasonably good.
 * One or two slightly larger changes (but still not major changes) in
 * these same areas.
 * Basically, now we wait for everything to be done before we exit, etc.
 *
 * Revision 1.15  2002/04/09 12:45:36  alan
 * Put in changes to the bcast, mcast and serial code such that
 * interrupted system calls in reads are ignored.
 *
 * Revision 1.14  2002/04/04 09:21:59  lars
 * Make the serial code also report a bad cable as the likely cause of the error.
 * (Most common mistake on the lists so far)
 *
 * Revision 1.13  2002/02/10 22:50:39  alan
 * Added a bit to the serial code to limit the number of messages which
 * come out (and the workload on the machine) when the serial port goes
 * nuts.
 *
 * Revision 1.12  2001/10/25 14:34:17  alan
 * Changed the serial code to send a BREAK when one side first starts up their
 * conversation.
 * Configured the receiving code to flush I/O buffers when they receive a break
 * Configured the code to ignore SIGINTs (except for their buffer flush effect)
 * Configured the code to use SIGQUIT instead of SIGINT when communicating that
 * the shutdown resource giveup is complete.
 *
 * This is all to fix a bug which occurs because of leftover out-of-date messages
 * in the serial buffering system.
 *
 * Revision 1.11  2001/10/04 21:14:30  alan
 * Patch from Reza Arbab <arbab@austin.ibm.com> to make it compile correctly
 * on AIX.
 *
 * Revision 1.10  2001/10/03 05:22:19  alan
 * Added code to save configuration parameters so we can pass them to the various communication plugins...
 *
 * Revision 1.9  2001/10/02 22:31:33  alan
 * Fixed really dumb error in the serial code.  I called an init function *after*
 * returning from the function ;-)
 *
 * Revision 1.8  2001/10/02 21:57:19  alan
 * Added debugging to the tty code.
 *
 * Revision 1.7  2001/10/02 20:15:41  alan
 * Debug code, etc. from Matt Soffen...
 *
 * Revision 1.6  2001/10/02 05:12:19  alan
 * Various portability fixes (make warnings go away) for Solaris.
 *
 * Revision 1.5  2001/09/27 17:02:34  alan
 * Shortened alarm time in write in serial.c
 * Put in a handful of Solaris warning-elimination patches.
 *
 * Revision 1.4  2001/09/07 16:18:17  alan
 * Updated ping.c to conform to the new plugin loading system.
 * Changed log messages in bcast, mcast, ping and serial to use the
 * new logging function.
 *
 * Revision 1.3  2001/08/15 16:56:47  alan
 * Put in the code to allow serial port comm plugins to work...
 *
 * Revision 1.2  2001/08/15 16:17:12  alan
 * Fixed the code so that serial comm plugins build/load/work.
 *
 * Revision 1.1  2001/08/10 17:16:44  alan
 * New code for the new plugin loading system.
 *
 * Revision 1.27  2001/06/08 04:57:48  alan
 * Changed "config.h" to <portability.h>
 *
 * Revision 1.26  2001/05/31 15:51:08  alan
 * Put in more fixes to get module loading (closer to) working...
 *
 * Revision 1.25  2001/05/26 17:38:01  mmoerz
 * *.cvsignore: added automake generated files that were formerly located in
 * 	     config/
 * * Makefile.am: removed ac_aux_dir stuff (for libtool) and added libltdl
 * * configure.in: removed ac_aux_dir stuff (for libtool) and added libltdl as
 * 		a convenience library
 * * bootstrap: added libtools libltdl support
 * * heartbeat/Makefile.am: added some headerfile to noinst_HEADERS
 * * heartbeat/heartbeat.c: changed dlopen, dlclose to lt_dlopen, lt_dlclose
 * * heartbeat/crc.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/mcast.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/md5.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/ping.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/serial.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/sha1.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/udp.c: changed to libltdl, exports functions via EXPORT()
 * * heartbeat/hb_module.h: added EXPORT() Macro, changed to libtools function
 * 			pointer
 * * heartbeat/module.c: converted to libtool (dlopen/dlclose -> lt_dlopen/...)
 * 		      exchanged scandir with opendir, readdir. enhanced
 * 		      autoloading code so that only .la modules get loaded.
 *
 * Revision 1.24  2001/05/12 06:05:23  alan
 * Put in the latest portability fixes (aka autoconf fixes)
 *
 * Revision 1.23  2001/05/11 06:20:26  alan
 * Fixed CFLAGS so we load modules from the right diurectory.
 * Fixed minor static symbol problems.
 * Fixed a bug which kept early error messages from coming out.
 *
 * Revision 1.22  2001/05/10 22:36:37  alan
 * Deleted Makefiles from CVS and made all the warnings go away.
 *
 * Revision 1.21  2000/12/12 23:23:47  alan
 * Changed the type of times from time_t to TIME_T (unsigned long).
 * Added BuildPreReq: lynx
 * Made things a little more OpenBSD compatible.
 *
 * Revision 1.20  2000/12/04 22:16:33  alan
 * Simplfied a BSD compatibility fix.
 *
 * Revision 1.19  2000/12/04 20:33:17  alan
 * OpenBSD fixes from Frank DENIS aka Jedi/Sector One <j@c9x.org>
 *
 * Revision 1.18  2000/09/01 21:10:46  marcelo
 * Added dynamic module support
 *
 * Revision 1.17  2000/08/13 04:36:16  alan
 * Added code to make ping heartbeats work...
 * It looks like they do, too ;-)
 *
 * Revision 1.16  2000/08/04 03:45:56  alan
 * Moved locking code into lock.c, so it could be used by both heartbeat and
 * the client code.  Also restructured it slightly...
 *
 * Revision 1.15  2000/07/26 05:17:19  alan
 * Added GPL license statements to all the code.
 *
 * Revision 1.14  2000/05/17 13:39:55  alan
 * Added the close-on-exec flag to sockets and tty fds that we open.
 * Thanks to Christoph J�ger for noticing the problem.
 *
 * Revision 1.13  2000/04/27 13:24:34  alan
 * Added comments about lock file fix. Minor corresponding code changes.
 *
 * Revision 1.12  2000/04/11 22:12:22  horms
 * Now cleans locks on serial devices from dead processes succesfully
 *
 * Revision 1.11  2000/02/23 18:44:53  alan
 * Put in a bug fix from Cliff Liang <lqm@readworld.com> to fix the tty
 * locking code.  The parameters to sscanf were mixed up.
 *
 * Revision 1.10  1999/11/15 05:31:43  alan
 * More tweaks for CTS/RTS flow control.
 *
 * Revision 1.9  1999/11/14 08:23:44  alan
 * Fixed bug in serial code where turning on flow control caused
 * heartbeat to hang.  Also now detect hangs and shutdown automatically.
 *
 * Revision 1.8  1999/11/11 04:58:04  alan
 * Fixed a problem in the Makefile which caused resources to not be
 * taken over when we start up.
 * Added RTSCTS to the serial port.
 * Added lots of error checking to the resource takeover code.
 *
 * Revision 1.7  1999/11/07 20:57:21  alan
 * Put in Matt Soffen's latest FreeBSD patch...
 *
 * Revision 1.6  1999/10/25 15:35:03  alan
 * Added code to move a little ways along the path to having error recovery
 * in the heartbeat protocol.
 * Changed the code for serial.c and ppp-udp.c so that they reauthenticate
 * packets they change the ttl on (before forwarding them).
 *
 * Revision 1.5  1999/10/10 20:12:54  alanr
 * New malloc/free (untested)
 *
 * Revision 1.4  1999/10/05 06:17:30  alanr
 * Fixed various uninitialized variables
 *
 * Revision 1.3  1999/09/30 15:55:12  alanr
 *
 * Added Matt Soffen's fix to change devname to serial_device for some kind
 * of FreeBSD compatibility.
 *
 * Revision 1.2  1999/09/26 14:01:18  alanr
 * Added Mijta's code for authentication and Guenther Thomsen's code for serial locking and syslog reform
 *
 * Revision 1.1.1.1  1999/09/23 15:31:24  alanr
 * High-Availability Linux
 *
 * Revision 1.11  1999/09/18 02:56:36  alanr
 * Put in Matt Soffen's portability changes...
 *
 * Revision 1.10  1999/09/16 05:50:20  alanr
 * Getting ready for 0.4.3...
 *
 * Revision 1.9  1999/08/17 03:49:26  alanr
 * added log entry to bottom of file.
 *
 */
