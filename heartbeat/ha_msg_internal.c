/* $Id: ha_msg_internal.c,v 1.54 2005/07/13 14:55:41 lars Exp $ */
/*
 * ha_msg_internal: heartbeat internal messaging functions
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
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
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/utsname.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <heartbeat_private.h>
#include <clplumbing/netstring.h>

#define		MINFIELDS	30
#define		CRNL		"\r\n"




#define	SEQ	"seq"
#define	LOAD1	"load1"

extern int		netstring_format;

#define IS_SEQ 1	/* the name is seq*/
#define IS_UUID 2  /* the value is uuid*/


/* The value functions are expected to return pointers to static data */
struct default_vals {
	const char *	name;
	const char * 	(*value)(void);
	int		flags;
};

static	const char * ha_msg_seq(void);
static	const char * ha_msg_timestamp(void);
static	const char * ha_msg_loadavg(void);
static	const char * ha_msg_from(void);
static  const char * ha_msg_fromuuid(void);
static	const char * ha_msg_ttl(void);
static	const char * ha_msg_hbgen(void);

/* Each of these functions returns static data requiring copying */
struct default_vals defaults [] = {
	{F_ORIG,	ha_msg_from,	0},
	{F_ORIGUUID,	ha_msg_fromuuid, 2},
	{F_SEQ,		ha_msg_seq,	1},
	{F_HBGENERATION,ha_msg_hbgen,	0},
	{F_TIME,	ha_msg_timestamp,0},
	{F_LOAD,	ha_msg_loadavg, 1},
	{F_TTL,		ha_msg_ttl, 0},
};

struct ha_msg *
add_control_msg_fields(struct ha_msg* ret)
{
	const char *	type;
	int		j;
	int		noseqno;
	const char *	to;
	cl_uuid_t	touuid;
	

	/* if F_TO field is present
	   this message is for one specific node
	   attach the uuid for that node*/
	
	if ((to = ha_msg_value(ret, F_TO)) != NULL ) {
		if (nodename2uuid(to, &touuid) == HA_OK){
			cl_msg_moduuid(ret, F_TOUUID, &touuid);
		} else{
			/* working with previous non-uuid version */
			/*
			ha_log(LOG_WARNING, " destnation %s uuid not found", to);
			*/
			/* do nothing */

		}		
	} else if (cl_get_uuid(ret, F_TOUUID, &touuid) == HA_OK){
		if ((to = uuid2nodename(&touuid)) != NULL){
			if (ha_msg_mod(ret, F_TO, to) != HA_OK){
				ha_log(LOG_WARNING, " adding field to message failed");
			}
		}else {
			ha_log(LOG_WARNING, " nodename not found for uuid");
		}
	}
	
	

	if ((type = ha_msg_value(ret, F_TYPE)) == NULL) {
		ha_log(LOG_ERR, "No type (add_control_msg_fields): ");
		cl_log_message(LOG_ERR, ret);
		ha_msg_del(ret);
		return(NULL);
	}
	
	if (DEBUGPKTCONT) {
		ha_log(LOG_DEBUG, "add_control_msg_fields: input packet");
		cl_log_message(LOG_DEBUG, ret);
	}

	noseqno = (strncmp(type, NOSEQ_PREFIX, sizeof(NOSEQ_PREFIX)-1) == 0);

	/* Add our default name=value pairs */
	for (j=0; j < DIMOF(defaults); ++j) {

		/*
		 * Should we skip putting a sequence number on this packet?
		 *
		 * We don't want requests for retransmission to be subject
		 * to being retransmitted according to the protocol.  They
		 * need to be outside the normal retransmission protocol.
		 * To accomplish that, we avoid giving them sequence numbers.
		 */
		if (noseqno && (defaults[j].flags & IS_SEQ)) {
			continue;
		}

		/* Don't put in duplicate values already gotten */
		if (noseqno && ha_msg_value(ret, defaults[j].name) != NULL) {
			/* This keeps us from adding another "from" field */
			continue;
		}
		
		if( defaults[j].flags & IS_UUID){
			if (cl_msg_moduuid(ret, defaults[j].name,
					   (const cl_uuid_t*)defaults[j].value()) != HA_OK ){
				ha_msg_del(ret);
				return(NULL);
			}
			
		}else {		
			if (ha_msg_mod(ret, defaults[j].name, 
				       defaults[j].value())
			    !=	HA_OK)  {
				ha_msg_del(ret);
				return(NULL);
			}
		}

	} 
	if (!netstring_format && !add_msg_auth(ret)) {
		ha_msg_del(ret);
		ret = NULL;
	}
	if (DEBUGPKTCONT) {
		ha_log(LOG_DEBUG, "add_control_msg_fields: packet returned");
		cl_log_message(LOG_DEBUG, ret);
	}

	return ret;
}



int
add_msg_auth(struct ha_msg * m)
{
	char	msgbody[MAXMSG];
	char	authstring[MAXLINE];
	char	authtoken[MAXLINE];
	
	{
		const char *	from;
		const char *	ts;
		const char *	type;

		/* Extract message type, originator, timestamp, auth */
		type = ha_msg_value(m, F_TYPE);
		from = ha_msg_value(m, F_ORIG);
		ts = ha_msg_value(m, F_TIME);

		if (from == NULL || ts == NULL || type == NULL) {
			ha_log(LOG_ERR
			,	"add_msg_auth: %s:  from %s"
			,	"missing from/ts/type"
			,	(from? from : "<?>"));
			cl_log_message(LOG_ERR, m);
		}
	}

	check_auth_change(config);
	msgbody[0] = EOS;
	
	if (msg2string_buf(m, msgbody, MAXMSG, 0, NOHEAD) != HA_OK){
		ha_log(LOG_ERR
		       ,	"add_msg_auth: compute string failed");
		return(HA_FAIL);
	}
	

	
	if (!config->authmethod->auth->auth(config->authmethod, msgbody
	,	strnlen(msgbody, MAXMSG)
	,	authtoken, DIMOF(authtoken))) {
		ha_log(LOG_ERR 
		,	"Cannot compute message authentication [%s/%s/%s]"
		,	config->authmethod->authname
		,	config->authmethod->key
		,	msgbody);
		return(HA_FAIL);
	}

	sprintf(authstring, "%d %s", config->authnum, authtoken);

	/* It will add it if it's not there yet, or modify it if it is */

	return(ha_msg_mod(m, F_AUTH, authstring));
}

gboolean
isauthentic(const struct ha_msg * m)
{
	char	msgbody[MAXMSG];
	char	authstring[MAXLINE];
	char	authbuf[MAXLINE];
	const char *	authtoken = NULL;
	int	j;
	int	authwhich = 0;
	struct HBauth_info*	which;
	
	
	if (get_stringlen(m) >= (ssize_t)sizeof(msgbody)) {
		return(0);
	}
	
	/* Reread authentication? */
	check_auth_change(config);


	if (msg2string_buf(m, msgbody, MAXMSG,0, NOHEAD) != HA_OK){
		ha_log(LOG_ERR
		       ,	"add_msg_auth: compute string failed");
		return(HA_FAIL);
	}
	
	for (j=0; j < m->nfields; ++j) {
		if (strcmp(m->names[j], F_AUTH) == 0) {
			authtoken = m->values[j];
			continue;
		}		
	}	
	
	if (authtoken == NULL
	||	sscanf(authtoken, "%d %s", &authwhich, authstring) != 2) {
		if (!cl_msg_quiet_fmterr) {
			ha_log(LOG_WARNING
			,	"Bad/invalid auth token, authtoken=%p"
			,	authtoken);
		}
		return(0);
	}
	which = config->auth_config + authwhich;

	if (authwhich < 0 || authwhich >= MAXAUTH || which->auth == NULL) {
		ha_log(LOG_WARNING
		,	"Invalid authentication type [%d] in message!"
		,	authwhich);
		return(0);
	}
		
	
	if (!which->auth->auth(which
        ,	msgbody, strnlen(msgbody, MAXMSG)
	,	authbuf, DIMOF(authbuf))) {
		ha_log(LOG_ERR, "Failed to compute message authentication");
		return(0);
	}
	if (strcmp(authstring, authbuf) == 0) {
		if (DEBUGAUTH) {
			ha_log(LOG_DEBUG, "Packet authenticated");
		}
		return(1);
	}
	if (DEBUGAUTH) {
		ha_log(LOG_INFO, "Packet failed authentication check, "
		       "authstring =%s,authbuf=%s ", authstring, authbuf);
	}
	return(0);
}


/* Add field to say who this packet is from */
STATIC	const char *
ha_msg_from(void)
{
	return localnodename;
}

/*Add field to say the node uuid this packet is from */
STATIC const char*
ha_msg_fromuuid()
{
	return (char*)&config->uuid;
}

/* Add sequence number field */
STATIC	const char *
ha_msg_seq(void)
{
	static char seq[32];
	static seqno_t seqno = 1;
	sprintf(seq, "%lx", seqno);
	++seqno;
	return(seq);
}

/* Add local timestamp field */
STATIC	const char *
ha_msg_timestamp(void)
{
	static char ts[32];
	sprintf(ts, TIME_X, (TIME_T)time(NULL));
	return(ts);
}

/* Add load average field */
STATIC	const char *
ha_msg_loadavg(void)
{
	static char	loadavg[64];
	static int 		fd = -1;
	char *		nlp;

	/*
	 * NOTE:  We never close 'fd'
	 * We keep it open to avoid touching the real filesystem once we
	 * are running, and avoid realtime problems.  I don't know that
	 * this was a significant problem, but if updates were being made
	 * to the / or /proc directories, then we could get blocked,
	 * and this was a very simple fix.
	 *
	 * We should probably get this information once every few seconds
	 * and use that, but this is OK for now...
	 */

	if (fd < 0 && (fd=open(LOADAVG, O_RDONLY)) < 0 ) {
		strcpy(loadavg, "n/a");
	}else{
		lseek(fd, 0, SEEK_SET);
		if (read(fd, loadavg, sizeof(loadavg)) <= 0) {
			strcpy(loadavg, "n/a");
		}
		loadavg[sizeof(loadavg)-1] = EOS;
	}

	if ((nlp = strchr(loadavg, '\n')) != NULL) {
		*nlp = EOS;
	}
	return(loadavg);
}

STATIC	const char *
ha_msg_ttl(void)
{
	static char	ttl[8];
	snprintf(ttl, sizeof(ttl), "%d", config->hopfudge + config->nodecount);
	return(ttl);
}

STATIC	const char *
ha_msg_hbgen(void)
{
	static char	hbgen[32];
	snprintf(hbgen, sizeof(hbgen), "%lx", config->generation);
	return(hbgen);
}


#ifdef TESTMAIN_MSGS
int
main(int argc, char ** argv)
{
	struct ha_msg*	m;
	while (!feof(stdin)) {
		if ((m=controlfifo2msg(stdin)) != NULL) {
			fprintf(stderr, "Got message!\n");	
			if (msg2stream(m, stdout) == HA_OK) {
				fprintf(stderr, "Message output OK!\n");
			}else{
				fprintf(stderr, "Could not output Message!\n");
			}
		}else{
			fprintf(stderr, "Could not get message!\n");
		}
	}
	return(0);
}
#endif
/*
 * $Log: ha_msg_internal.c,v $
 * Revision 1.54  2005/07/13 14:55:41  lars
 * Compile warnings: Ignored return values from sscanf/fgets/system etc,
 * minor signedness issues.
 *
 * Revision 1.53  2005/04/27 05:31:42  gshi
 *  use struct cl_uuid_t to replace uuid_t
 * use cl_uuid_xxx to replace uuid_xxx funcitons
 *
 * Revision 1.52  2005/03/04 15:34:59  alan
 * Fixed various signed/unsigned errors...
 *
 * Revision 1.51  2005/02/14 21:06:10  gshi
 * BEAM fix:
 *
 * replacing the binary usage in core code with uuid function
 *
 * Revision 1.50  2005/02/11 05:06:01  alan
 * Undid some accidental (but sloppy) name-mangling by alsoran...
 *
 * Revision 1.49  2005/02/08 08:10:27  gshi
 * change the way stringlen and netstringlen is computed.
 *
 * Now it is computed resursively in child messages in get_stringlen() and get_netstringlen()
 * so it allows changing child messages dynamically.
 *
 * Revision 1.48  2005/01/18 20:33:03  andrew
 * Appologies for the top-level commit, one change necessitated another which
 *   exposed some bugs... etc etc
 *
 * Remove redundant usage of XML in the CRM
 * - switch to "struct ha_msg" aka. HA_Message for everything except data
 * Make sure the expected type of all FSA input data is verified before processing
 * Fix a number of bugs including
 * - looking in the wrong place for the API result data in the CIB API
 *   (hideous that this actually worked).
 * - not overwriting error codes when sending the result to the client in the CIB API
 *   (this lead to some error cases being treated as successes later in the code)
 * Add PID to log messages sent to files (not to syslog)
 * Add a log level to calls for cl_log_message()
 * - convert existing calls, sorry if I got the level wrong
 * Add some checks in cl_msg.c code to prevent NULL pointer exceptions
 * - usually when NULL is passed to strlen() or similar
 *
 * Revision 1.47  2004/08/31 17:35:13  alan
 * Put in a "be quiet on format errors" change I had missed before...
 *
 * Revision 1.46  2004/08/29 04:33:41  msoffen
 * Fixed comments to properly compile
 *
 * Revision 1.45  2004/08/29 03:01:12  msoffen
 * Replaced all // COMMENTs with / * COMMENT * /
 *
 * Revision 1.44  2004/07/26 12:39:46  andrew
 * Change the type of some int's to size_t to stop OSX complaining
 *
 * Revision 1.43  2004/07/07 19:07:14  gshi
 * implemented uuid as nodeid
 *
 * Revision 1.42  2004/03/25 07:55:39  alan
 * Moved heartbeat libraries to the lib directory.
 *
 * Revision 1.41  2004/03/03 05:31:50  alan
 * Put in Gochun Shi's new netstrings on-the-wire data format code.
 * this allows sending binary data, among many other things!
 *
 * Revision 1.40  2004/02/17 22:11:57  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.39  2003/12/19 17:28:01  alan
 * Incorporated a patch from Guochun Shi <gshi@ncsa.uiuc.edu> to allow
 * the authentication facility to be able to authenticate binary messages.
 *
 * Revision 1.38  2003/11/10 08:55:20  lars
 * Bugfixes by Deng, Pan:
 *
 * - While receiving a ha_msg, the default number of fields is MINFIELDS,
 *   which is 20. After the reception, if more than 20 fields needed to be
 *   added, it will fail.  I changed the MINFIELDS to 30. It is not a
 *   graceful fix, but it can work for checkpoint service. I think the max
 *   fields should not be fixed.
 *
 * - The message create routine ha_msg_new() in ha_msg.c. It takes a
 *   parameter nfields, but the function does not use it at all. If nfields
 *   > MINFIELDS, the allocated fields should be nfields.
 *
 * Revision 1.37  2003/07/03 21:50:40  alan
 * Changed the function which adds the "from" address to not use uname, but
 * the already-computed localnodename variable.
 *
 * Revision 1.36  2003/05/30 15:22:11  kevin
 * Fix building on OpenBSD
 *
 * Revision 1.35  2003/04/18 06:09:46  alan
 * Fixed an off-by-one error in writing messages to the FIFO.
 * Also got rid of some now-unused functions, and fixed a minor glitch in BasicSanitCheck.
 *
 * Revision 1.34  2003/04/15 23:06:53  alan
 * Lots of new code to support the semi-massive process restructuriing.
 *
 * Revision 1.33  2003/03/29 02:48:44  alan
 * More small changes on the road to restructuring heartbeat processees.
 *
 * Revision 1.32  2003/02/07 08:37:16  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.31  2003/02/05 09:06:33  horms
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
 * Revision 1.30  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.29  2002/10/22 13:18:58  alan
 * Changed a few calls to ha_error(... to ha_log(LOG_ERR,...
 *
 * Revision 1.28  2002/10/21 14:31:17  msoffen
 * Additional debug to find actual cause of empty packets.
 *
 * Revision 1.27  2002/10/21 10:17:18  horms
 * hb api clients may now be built outside of the heartbeat tree
 *
 * Revision 1.26  2002/09/24 17:24:46  msoffen
 * Changed to log the bogus packet and accept blank packets / carriage returns
 * and just return.
 *
 * Revision 1.25  2002/09/17 18:53:37  alan
 * Put in a fix to keep mach_down from doing anything with ping node information.
 * Also put in a change to make lmb's last portability fix more portable ;-)
 *
 * Revision 1.24  2002/09/17 17:08:00  lars
 * strlen() returns size_t and requires %zd instead of %d in *printf() according
 * to ISO C. (Portability fix to compile on size_t != int archs)
 *
 * Revision 1.23  2002/09/13 14:47:46  alan
 * Put in a workaround for an annoying message we get in FreeBSD...
 *
 * Revision 1.22  2002/09/13 05:24:13  alan
 * Put in some debugging code for Matt Soffen in FreeBSD.
 *
 * Revision 1.21  2002/08/10 02:10:24  alan
 * Changed it so that it's impossible to get an extra newline in the output
 * from /proc/loadavg regardless of what the kernel gives us...
 *
 * Revision 1.20  2002/08/02 22:44:00  alan
 * Enhanced an error message when we get a name/value (NV) failure.
 *
 * Revision 1.19  2002/07/08 04:14:12  alan
 * Updated comments in the front of various files.
 * Removed Matt's Solaris fix (which seems to be illegal on Linux).
 *
 * Revision 1.18  2002/04/13 22:35:08  alan
 * Changed ha_msg_add_nv to take an end pointer to make it safer.
 * Added a length parameter to string2msg so it would be safer.
 * Changed the various networking plugins to use the new string2msg().
 *
 * Revision 1.17  2002/04/07 13:54:06  alan
 * This is a pretty big set of changes ( > 1200 lines in plain diff)
 *
 * The following major bugs have been fixed
 *  - STONITH operations are now a precondition for taking over
 *    resources from a dead machine
 *
 *  - Resource takeover events are now immediately terminated when shutting
 *    down - this keeps resources from being held after shutting down
 *
 *  - heartbeat could sometimes fail to start due to how it handled its
 *    own status through two different channels.  I restructured the handling
 *    of local status so that it's now handled almost exactly like handling
 *    the status of remote machines
 *
 * There is evidence that all these serious bugs have been around a long time,
 * even though they are rarely (if ever) seen.
 *
 * The following minor bugs have been fixed:
 *
 *  - the standby test now retries during transient conditions...
 *
 *  - the STONITH code for the test method "ssh" now uses "at" to schedule
 *    the stonith operation on the other node so it won't hang when using
 *    newer versions of ssh.
 *
 * The following new test was added:
 *  - SimulStart - starting all nodes ~ simultaneously
 *
 * The following significant restructuring of the code occurred:
 *
 *  - Completely rewrote the process management and death-of-child code to
 *    be uniform, and be based on a common semi-object-oriented approach
 *    The new process tracking code is very general, and I consider it to
 *    be part of the plumbing for the OCF.
 *
 *  - Completely rewrote the event handling code to be based on the Glib
 *    mainloop paradigm. The sets of "inputs" to the main loop are:
 *     - "polled" events like signals, and once-per-loop occurrances
 *     - messages from the cluster and users
 *     - API registration requests from potential clients
 *     - API calls from clients
 *
 *
 * The following minor changes were made:
 *
 *  - when nice_failback is taking over resources, since we always negotiate for
 *    taking them over, so we no longer have a timeout waiting for the other
 *    side to reply.  As a result, the timeout for waiting for the other
 *    side is now much longer than it was.
 *
 *  - transient errors for standby operations now print WARN instead of EROR
 *
 *  - The STONITH and standby tests now don't print funky output to the
 *    logs.
 *
 *  - added a new file TESTRESULTS.out for logging "official" test results.
 *
 * Groundwork was laid for the following future changes:
 *  - merging the control and master status processes
 *
 *  - making a few other things not wait for process completion in line
 *
 *  - creating a comprehensive asynchronous action structure
 *
 *  - getting rid of the "interface" kludge currently used for tracking
 *    activity on individual interfaces
 *
 * The following things still need to be tested:
 *
 *  - STONITH testing (including failures)
 *
 *  - clock jumps
 *
 *  - protocol retransmissions
 *
 *  - cross-version compatability of status updates (I added a new field)
 *
 * Revision 1.16  2002/03/27 02:10:22  alan
 * Finished (hopefully) the last bug fix.  Now it won't complain
 * if it authenticates a packet without a sequence number.  This was kinda
 * dumb anyway.  I know packets go out w/o seq numbers...
 *
 * Revision 1.15  2002/03/15 14:26:36  alan
 * Added code to help debug the current missing to/from/ts/,etc. problem...
 *
 * Revision 1.14  2001/10/25 14:17:28  alan
 * Changed a few of the errors into warnings.
 *
 * Revision 1.13  2001/10/24 20:46:28  alan
 * A large number of patches.  They are in these categories:
 * 	Fixes from Matt Soffen
 * 	Fixes to test environment things - including changing some ERRORs to
 * 		WARNings and vice versa.
 * 	etc.
 *
 * Revision 1.12  2001/09/29 19:08:24  alan
 * Wonderful security and error correction patch from Emily Ratliff
 * 	<ratliff@austin.ibm.com>
 * Fixes code to have strncpy() calls instead of strcpy calls.
 * Also fixes the number of arguments to several functions which were wrong.
 * Many thanks to Emily.
 *
 * Revision 1.11  2001/07/18 03:12:52  alan
 * Put in a couple of minor security fixes from Emily Ratliff.
 * The ttl value put in the messages is now checked for overflow, and the
 * hopfudge value it is based on is now bounded to 255...
 *
 * Revision 1.10  2001/07/17 15:00:04  alan
 * Put in Matt's changes for findif, and committed my changes for the new module loader.
 * You now have to have glib.
 *
 * Revision 1.9  2001/06/19 13:56:28  alan
 * FreeBSD portability patch from Matt Soffen.
 * Mainly added #include "portability.h" to lots of files.
 * Also added a library to Makefile.am
 *
 * Revision 1.8  2001/06/06 23:10:10  alan
 * Comment clarification as a result of Emily's code audit.
 *
 * Revision 1.7  2001/06/06 23:07:44  alan
 * Put in some code clarifications suggested by Emily Ratliff.  Thanks Emily!
 *
 * Revision 1.6  2001/04/19 13:41:54  alan
 * Removed the two annoying "error" messages that occur when heartbeat
 * is shut down.  They are: "controlfifo2msg: cannot create message"
 * and "control_process: NULL message"
 *
 * Revision 1.5  2000/09/10 03:48:52  alan
 * Fixed a couple of bugs.
 * - packets that were already authenticated didn't get reauthenticated correctly.
 * - packets that were irretrievably lost didn't get handled correctly.
 *
 * Revision 1.4  2000/08/11 00:30:07  alan
 * This is some new code that does two things:
 * 	It has pretty good replay attack protection
 * 	It has sort-of-basic recovery from a split partition.
 *
 * Revision 1.3  2000/07/26 05:17:19  alan
 * Added GPL license statements to all the code.
 *
 * Revision 1.2  2000/07/19 23:03:53  alan
 * Working version of most of the API code.  It still has the security bug...
 *
 * Revision 1.1  2000/07/11 00:25:52  alan
 * Added a little more API code.  It looks like the rudiments are now working.
 *
 *
 */
