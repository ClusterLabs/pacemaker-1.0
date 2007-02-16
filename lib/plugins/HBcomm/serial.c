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
static void*		serial_read(struct hb_media *mp, int* lenp);
static char *		ttygets(char * inbuf, int length
,				struct serial_private *tty);
static int		serial_write(struct hb_media*mp, void *msg , int len);
static int		serial_open(struct hb_media* mp);
static int		ttysetup(int fd, const char * ourtty);
static int		opentty(char * serial_device);
static int		serial_close(struct hb_media* mp);
static int		serial_init(void);

static void		serial_localdie(void);

static int		serial_mtype(char **buffer);
static int		serial_descr(char **buffer);
static int		serial_isping(void);


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

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static struct hb_media_imports*	OurImports;
static void*			interfprivate;
static int			fragment_write_delay = 0;
#define FRAGSIZE 512

#define LOG	PluginImports->log
#define MALLOC	PluginImports->alloc
#define STRDUP  PluginImports->mstrdup
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
	,	NULL		/*close */
	,	&OurInterface
	,	(void*)&OurImports
	,	interfprivate); 
	serial_init();
	return rc;
}

#define		IsTTYOBJECT(mp)	((mp) && ((mp)->vf == (void*)&serial_media_fns))
/*
#define		TTYASSERT(mp)	ASSERT(IsTTYOBJECT(mp))
*/
#define		TTYASSERT(mp)
#define		RTS_WARNTIME	3600

static int
serial_mtype (char **buffer) { 
	*buffer = STRDUP("serial");
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

static int
serial_descr (char **buffer) { 
	*buffer = STRDUP("serial ring");
	if (!*buffer) {
		return 0;
	}

	return strlen(*buffer);
}

static int
serial_isping (void) {
	return 0;
}

static int
compute_fragment_write_delay(void)
{
	int rate_bps = atoi(baudstring);
	if (rate_bps < 300 ){
		cl_log(LOG_ERR, "%s: invalid baud rate(%s)",
		       __FUNCTION__, baudstring);
		return HA_FAIL;
	}
	
	fragment_write_delay = (1.0*FRAGSIZE)/(rate_bps/8)*1000000;
	return HA_OK;
}

/* Initialize global serial data structures */
static int
serial_init (void)
{
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
		PILCallLog(LOG, PIL_DEBUG, "serial_init: serial_baud = 0x%x"
		,	serial_baud);
	}
	
	if(compute_fragment_write_delay() != HA_OK){
		return HA_FAIL;
	}

	return HA_OK;
}

/* Process a serial port declaration */
static struct hb_media *
serial_new (const char * port)
{
	struct	stat	sbuf;
	struct hb_media * ret;


	/* Let's see if this looks like it might be a serial port... */
	if (*port != '/') {
		PILCallLog(LOG, PIL_CRIT
		,	"Serial port not full pathname [%s] in config file"
		,	port);
		return(NULL);
	}

	if (stat(port, &sbuf) < 0) {
		PILCallLog(LOG, PIL_CRIT, "Nonexistent serial port [%s] in config file"
		,	port);
		return(NULL);
	}
	if (!S_ISCHR(sbuf.st_mode)) {
		PILCallLog(LOG, PIL_CRIT
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
			sp->ttyname = STRDUP(port);
			if (sp->ttyname != NULL) {
				sp->consecutive_errors = 0;
				ret->name = sp->ttyname;
				ret->pd = sp;
			}else{
				FREE(sp);
				sp = NULL;
			}
		}
		if (sp == NULL) {
			FREE(ret);
			ret = NULL;
			PILCallLog(LOG, PIL_CRIT, "Out of memory (private serial data)");
		}
	}else{
		PILCallLog(LOG, PIL_CRIT, "Out of memory (serial data)");
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
		PILCallLog(LOG, PIL_CRIT, "cannot lock line %s", sp->ttyname);
		return(HA_FAIL);
	}
	if ((sp->ttyfd = opentty(sp->ttyname)) < 0) {
		return(HA_FAIL);
	}
	PILCallLog(LOG, PIL_INFO, "Starting serial heartbeat on tty %s (%s baud)"
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
		PILCallLog(LOG, PIL_CRIT, "cannot get tty attributes: %s", strerror(errno));
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
		PILCallLog(LOG, PIL_CRIT, "cannot set tty attributes: %s"
		,	strerror(errno));
		return(HA_FAIL);
	}
	if (ANYDEBUG) {
		PILCallLog(LOG, PIL_DEBUG, "tty setup on %s complete.", ourtty);
		PILCallLog(LOG, PIL_DEBUG, "Baud rate set to: 0x%x"
		,	(unsigned)serial_baud);
		PILCallLog(LOG, PIL_DEBUG, "ti.c_iflag = 0x%x"
		,	(unsigned)ti.c_iflag);
		PILCallLog(LOG, PIL_DEBUG, "ti.c_oflag = 0x%x"
		,	(unsigned)ti.c_oflag);
		PILCallLog(LOG, PIL_DEBUG,"ti.c_cflag = 0x%x"
		,	(unsigned)ti.c_cflag);
		PILCallLog(LOG, PIL_DEBUG, "ti.c_lflag = 0x%x"
		,	(unsigned)ti.c_lflag);
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
		PILCallLog(LOG, PIL_CRIT, "cannot open %s: %s", serial_device
		,	strerror(errno));
		return(fd);
	}
	if (!ttysetup(fd, serial_device)) {
		close(fd);
		return(-1);
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC)) {
		PILCallLog(LOG, PIL_WARN,"Error setting the close-on-exec flag: %s"
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
			PILCallLog(LOG, PIL_DEBUG, "serial_localdie: Flushing tty");
		}
		tcflush(ourtty, TCIOFLUSH);
	}
}


static char		serial_pkt[MAXMSG];

/* This function does all the reading from our tty ports */
static void *
serial_read(struct hb_media* mp, int *lenp)
{
	char			buf[MAXMSG];
	struct serial_private*	thissp;
	int			startlen;
	const char *		start = MSG_START;
	const char *		end = MSG_END;
	int			endlen;
	char			*p;
	int			len = 0;
	int			tmplen;


	
	
	TTYASSERT(mp);
	thissp = (struct serial_private*)mp->pd;

	startlen = strlen(start);
	if (start[startlen-1] == '\n') {
		--startlen;
	}
	endlen = strlen(end);
	if (end[endlen-1] == '\n') {
		--endlen;
	}
	
	memset(serial_pkt, 0, MAXMSG);
	serial_pkt[0] = 0;
	p = serial_pkt;
	
	/* Skip until we find a MSG_START (hopefully we skip nothing) */
	while (ttygets(buf, MAXMSG, thissp) != NULL
	       &&	strncmp(buf, start, startlen) != 0) {
		
		
		/*nothing*/
	}
	
	len = strnlen(buf, MAXMSG) + 1;
	if(len >=  MAXMSG){
		PILCallLog(LOG, PIL_CRIT,  "serial_read:MSG_START exceeds MAXMSG");
		return(NULL);
	}

	tmplen = strnlen(buf, MAXMSG);
	
	strcat(p, buf);
	p += tmplen;
	strcat(p, "\n");
	p++;

	while (ttygets(buf, MAXMSG, thissp) != NULL
	       &&	strncmp(buf, MSG_END, endlen) != 0) {
		
		
		len += strnlen(buf, MAXMSG) + 1;
		if(len >= MAXMSG){
			PILCallLog(LOG, PIL_CRIT, "serial_read:serial_pkt exceeds MAXMSG");
			return(NULL);
		}
		
		
		tmplen = strnlen(buf, MAXMSG);
		memcpy(p, buf, tmplen);		
		p += tmplen;
		strcat(p, "\n");
		p++;
	}

	
	if(strncmp(buf, MSG_END, endlen) == 0){
		
		len += strnlen(buf, MAXMSG) + 2;
		if(len >= MAXMSG){
			PILCallLog(LOG, PIL_CRIT, "serial_read:serial_pkt exceeds MAXMSG after adding MSG_END");
			return(NULL);
		}

		tmplen = strnlen(buf, MAXMSG);
		
		memcpy(p, buf, tmplen);
		p += tmplen;
		strcat(p, "\n");
		p++;
		p[0] = 0;
	}
	
	if (buf[0] == EOS ) {
		return NULL;
	}else{
		thissp->consecutive_errors=0;
	}
	
	*lenp = len;
	
	return(serial_pkt);	
}

/* This function does all the writing to our tty ports */
static int
serial_write(struct hb_media* mp, void *p, int len)
{

	int			string_startlen = sizeof(MSG_START)-1;
	int			netstring_startlen = sizeof(MSG_START_NETSTRING) - 1;

	char			*str;
	int			str_new = 0;
	int			wrc;
	int			size;
	int			ourtty;
	static gboolean		warnyet=FALSE;
	static longclock_t	warninterval;
	static longclock_t	lastwarn;
	int			i;
	int			loop;
	char*			datastr;
	
	if (strncmp(p, MSG_START, string_startlen) == 0) {
		str = p;
		size = strlen(str);
		if(size > len){
			return(HA_FAIL);
		}
	} else if(strncmp(p, MSG_START_NETSTRING, netstring_startlen) == 0) {
		
		struct ha_msg * msg;
		
		msg = wirefmt2msg(p, len, MSG_NEEDAUTH);
		if(!msg){
			ha_log(PIL_WARN, "serial_write(): wirefmt2msg() failed");
			return(HA_FAIL);
		}
		
		add_msg_auth(msg);
		str = msg2string(msg);		
		str_new = 1;
		size = strlen(str);
		ha_msg_del(msg);
		
	} else{
		return(HA_FAIL);
	}


	TTYASSERT(mp);
	

	if (!warnyet) {
		warninterval = msto_longclock(RTS_WARNTIME*1000L);
	}
	ourmedia = mp;	/* Only used for the "localdie" function */
	OurImports->RegisterCleanup(serial_localdie);
	ourtty = ((struct serial_private*)(mp->pd))->ttyfd;

	if (DEBUGPKT) {
		PILCallLog(LOG, PIL_DEBUG, "Sending pkt to %s [%d bytes]"
		,	mp->name, size);
	}
	if (DEBUGPKTCONT) {
		PILCallLog(LOG, PIL_DEBUG, "%s", str);
	}

	loop = size / FRAGSIZE + ((size%FRAGSIZE == 0)?0:1);       
	datastr =str;
	for (i = 0; i < loop; i++){
		int datalen ;
		
		datalen = FRAGSIZE;
		if ( (i == loop -1 )
		     && (size% FRAGSIZE != 0)){
			datalen =  size %FRAGSIZE;
		}
		
		setmsalarm(500);
		wrc = write(ourtty, datastr, datalen);
		cancelmstimer();
		if (i != (loop -1)) {
			usleep(fragment_write_delay);
		}
		if (DEBUGPKTCONT) {
			PILCallLog(LOG, PIL_DEBUG, "serial write returned %d", wrc);
		}
		
		if (wrc < 0 || wrc != datalen) {
			if (DEBUGPKTCONT && wrc < 0) {
				PILCallLog(LOG, PIL_DEBUG, "serial write errno was %d", errno);
			}
			if (wrc > 0 || (wrc < 0 && errno == EINTR)) {
				longclock_t	now = time_longclock();
				tcflush(ourtty, TCIOFLUSH);
				
				if (!warnyet
				    ||	cmp_longclock(sub_longclock(now, lastwarn)
						      ,		warninterval) >= 0) {
					
					lastwarn = now;
					warnyet = TRUE;
					PILCallLog(LOG, PIL_WARN
					,	"TTY write timeout on [%s]"
					" (no connection or bad cable"
					"? [see documentation])"
					,	mp->name);
					PILCallLog(LOG, PIL_INFO
					,	"See %s for details"
					,	HAURL("FAQ#TTYtimeout"));
				}
			}else{
				PILCallLog(LOG, PIL_CRIT, "TTY write failure on [%s]: %s"
					   ,	mp->name, strerror(errno));
			}
		}
		
		datastr +=datalen;
	}
	
	if(str_new){
		cl_free(str);
		str = NULL;
	}
	return(HA_OK);
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
				PILCallLog(LOG, PIL_CRIT, "EOF in ttygets [%s]: %s [%d]"
				,	tty->ttyname, strerror(errno), rc);
				++tty->consecutive_errors;
				tcsetpgrp(fd, getsid(getpid()));
				if ((tty->consecutive_errors % 10) == 0) {
					PILCallLog(LOG, PIL_WARN
					,	"10 consecutive EOF"
					" errors from serial port %s"
					,	tty->ttyname);
					PILCallLog(LOG, PIL_INFO
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
			
		if (*cp == '\n') {
			break;
		}
	}
	*cp = '\0';
	return(inbuf);
}

