static const char * _ha_msg_c_Id = "$Id: ha_msg.c,v 1.46 2003/10/29 04:05:00 alan Exp $";
/*
 * Heartbeat messaging object.
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *
 * This software licensed under the GNU LGPL.
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
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/utsname.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <hb_proc.h>
#include <unistd.h>
#include <clplumbing/ipc.h>

#define		MINFIELDS	20
#define		CRNL		"\r\n"

#undef DOAUDITS
#ifdef DOAUDITS

void ha_msg_audit(const struct ha_msg* msg);

#	define	AUDITMSG(msg)	ha_msg_audit(msg)
#else
#	define	AUDITMSG(msg)	/* Nothing */
#endif


/* Create a new (empty) message */
struct ha_msg *
ha_msg_new(nfields)
{
	struct ha_msg *	ret;

	(void)_heartbeat_h_Id;
	(void)_ha_msg_c_Id;
	(void)_ha_msg_h_Id;
	ret = MALLOCT(struct ha_msg);
	if (ret) {
		ret->nfields = 0;
		ret->nalloc    = MINFIELDS;
		ret->names     = (char **)ha_calloc(sizeof(char *), MINFIELDS);
		ret->nlens     = (int *)ha_calloc(sizeof(int), MINFIELDS);
		ret->values    = (char **)ha_calloc(sizeof(char *), MINFIELDS);
		ret->vlens     = (int *)ha_calloc(sizeof(int), MINFIELDS);
		ret->stringlen = sizeof(MSG_START)+sizeof(MSG_END)-1;

		if (ret->names == NULL || ret->values == NULL
		||	ret->nlens == NULL || ret->vlens == NULL) {
			ha_log(LOG_ERR, "%s"
			,	"ha_msg_new: out of memory for ha_msg");
			ha_msg_del(ret);
			ret = NULL;
		}else if (curproc) {
			curproc->allocmsgs++;
			curproc->totalmsgs++;
			curproc->lastmsg = time(NULL);
		}
	}
	return(ret);
}

/* Delete (destroy) a message */
void
ha_msg_del(struct ha_msg *msg)
{
	if (msg) {
		int	j;
		AUDITMSG(msg);
		if (curproc) {
			curproc->allocmsgs--;
		}
		if (msg->names) {
			for (j=0; j < msg->nfields; ++j) {
				if (msg->names[j]) {
					ha_free(msg->names[j]);
					msg->names[j] = NULL;
				}
			}
			ha_free(msg->names);
			msg->names = NULL;
		}
		if (msg->values) {
			for (j=0; j < msg->nfields; ++j) {
				if (msg->values[j]) {
					ha_free(msg->values[j]);
					msg->values[j] = NULL;
				}
			}
			ha_free(msg->values);
			msg->values = NULL;
		}
		if (msg->nlens) {
			ha_free(msg->nlens);
			msg->nlens = NULL;
		}
		if (msg->vlens) {
			ha_free(msg->vlens);
			msg->vlens = NULL;
		}
		msg->nfields = -1;
		msg->nalloc = -1;
		msg->stringlen = -1;
		ha_free(msg);
	}
}
struct ha_msg*
ha_msg_copy(const struct ha_msg *msg)
{
	struct ha_msg*		ret;
	int			j;

	AUDITMSG(msg);

	ret = MALLOCT(struct ha_msg);
	ret->nfields	= msg->nfields;
	ret->nalloc	= msg->nalloc;
	ret->stringlen	= msg->stringlen;

	ret->names  = (char **)	ha_calloc(sizeof(char *), msg->nalloc);
	ret->nlens  = (int *)	ha_calloc(sizeof(int), msg->nalloc);
	ret->values = (char **)	ha_calloc(sizeof(char *), msg->nalloc);
	ret->vlens  = (int *)	ha_calloc(sizeof(int), msg->nalloc);

	if (ret->names == NULL || ret->values == NULL
	||	ret->nlens == NULL || ret->vlens == NULL) {
		ha_log(LOG_ERR
		,	"ha_msg_new: out of memory for ha_msg_copy");
		goto freeandleave;
	}
	memcpy(ret->nlens, msg->nlens, sizeof(msg->nlens[0])*msg->nfields);
	memcpy(ret->vlens, msg->vlens, sizeof(msg->nlens[0])*msg->nfields);

	for (j=0; j < msg->nfields; ++j) {

		if ((ret->names[j] = ha_malloc(msg->nlens[j]+1)) == NULL) {
			goto freeandleave;
		}
		memcpy(ret->names[j], msg->names[j], msg->nlens[j]+1);

		if ((ret->values[j] = ha_malloc(msg->vlens[j]+1)) == NULL) {
			goto freeandleave;
		}
		memcpy(ret->values[j], msg->values[j], msg->vlens[j]+1);
	}
	return ret;

freeandleave:
	ha_msg_del(ret);
	ret=NULL;
	return ret;
}

#ifdef DOAUDITS
void
ha_msg_audit(const struct ha_msg* msg)
{
	int	doabort = FALSE;
	int	j;

	if (!msg) {
		return;
	}
	if (!ha_is_allocated(msg)) {
		cl_log(LOG_CRIT, "Message @ 0x%x is not allocated"
		,	(unsigned) msg);
		abort();
	}
	if (msg->nfields < 0) {
		cl_log(LOG_CRIT, "Message @ 0x%x has negative fields (%d)"
		,	(unsigned) msg, msg->nfields);
		doabort = TRUE;
	}
	if (msg->nalloc < 0) {
		cl_log(LOG_CRIT, "Message @ 0x%x has negative nalloc (%d)"
		,	(unsigned) msg, msg->nalloc);
		doabort = TRUE;
	}
	if (msg->stringlen < 0) {
		cl_log(LOG_CRIT
		,	"Message @ 0x%x has negative stringlen (%d)"
		,	(unsigned) msg, msg->stringlen);
		doabort = TRUE;
	}
	if (msg->stringlen < 4 * msg->nfields) {
		cl_log(LOG_CRIT
		,	"Message @ 0x%x has too small stringlen (%d)"
		,	(unsigned) msg, msg->stringlen);
		doabort = TRUE;
	}
	if (!ha_is_allocated(msg->names)) {
		cl_log(LOG_CRIT
		,	"Message names @ 0x%x is not allocated"
		,	(unsigned) msg->names);
		doabort = TRUE;
	}
	if (!ha_is_allocated(msg->values)) {
		cl_log(LOG_CRIT
		,	"Message values @ 0x%x is not allocated"
		,	(unsigned) msg->values);
		doabort = TRUE;
	}
	if (!ha_is_allocated(msg->nlens)) {
		cl_log(LOG_CRIT
		,	"Message nlens @ 0x%x is not allocated"
		,	(unsigned) msg->nlens);
		doabort = TRUE;
	}
	if (!ha_is_allocated(msg->vlens)) {
		cl_log(LOG_CRIT
		,	"Message vlens @ 0x%x is not allocated"
		,	(unsigned) msg->vlens);
		doabort = TRUE;
	}
	if (doabort) {
		abort();
	}
	for (j=0; j < msg->nfields; ++j) {
		if (!ha_is_allocated(msg->names[j])) {
			cl_log(LOG_CRIT, "Message name[%d] @ 0x%x"
			" is not allocated."
			,	j, (unsigned) msg->names[j]);
		}
		if (!ha_is_allocated(msg->values[j])) {
			cl_log(LOG_CRIT, "Message value [%d] @ 0x%x"
			" is not allocated."
			,	j, (unsigned) msg->values[j]);
		}
	}
}
#endif

/* Add a null-terminated name and value to a message */
int
ha_msg_add(struct ha_msg * msg, const char * name, const char * value)
{
	return(ha_msg_nadd(msg, name, strlen(name), value, strlen(value)));
}

/* Add a name/value pair to a message (with sizes for name and value) */
int
ha_msg_nadd(struct ha_msg * msg, const char * name, int namelen
		,	const char * value, int vallen)
{
	int	next;
	char *	cpname;
	char *	cpvalue;
	int	startlen = sizeof(MSG_START)-1;
	int	newlen = msg->stringlen + (namelen+vallen+2);
				/* 2 == "=" + "\n" */

	if (!msg || (msg->nfields >= msg->nalloc)
	||	msg->names == NULL || msg->values == NULL) {
		ha_log(LOG_ERR, "ha_msg_nadd: cannot add field to ha_msg");
		return(HA_FAIL);
	}
	if (name == NULL || value == NULL
	||	namelen <= 0 || vallen < 0 || newlen >= MAXMSG) {
		ha_log(LOG_ERR, "ha_msg_nadd: "
				"cannot add name/value to ha_msg");
		return(HA_FAIL);
	}

	if (namelen >= startlen && strncmp(name, MSG_START, startlen) == 0) {
		ha_log(LOG_ERR, "ha_msg_nadd: illegal field");
		return(HA_FAIL);
	}
		
	if (memchr(value, '\n', vallen) != NULL) {
		ha_log(LOG_ERR, "ha_msg_nadd: newline in value. name [%s] "
				"value [%s]", name, value);
		return(HA_FAIL);
	}

	if ((cpname = ha_malloc(namelen+1)) == NULL) {
		ha_log(LOG_ERR, "ha_msg_nadd: no memory for string (name)");
		return(HA_FAIL);
	}
	if ((cpvalue = ha_malloc(vallen+1)) == NULL) {
		ha_free(cpname);
		ha_log(LOG_ERR, "ha_msg_nadd: no memory for string (value)");
		return(HA_FAIL);
	}
	/* Copy name, value, appending EOS to the end of the strings */
	strncpy(cpname, name, namelen);		cpname[namelen] = EOS;
	strncpy(cpvalue, value, vallen);	cpvalue[vallen] = EOS;

	next = msg->nfields;
	msg->values[next] = cpvalue;
	msg->vlens[next] = vallen;
	msg->names[next] = cpname;
	msg->nlens[next] = namelen;
	msg->stringlen = newlen;
	msg->nfields++;
	AUDITMSG(msg);
	return(HA_OK);
}

/* Add a "name=value" line to the name, value pairs in a message */
int
ha_msg_add_nv(struct ha_msg* msg, const char * nvline, const char * bufmax)
{
	int		namelen;
	const char *	valp;
	int		vallen;

	if (!nvline) {
		ha_log(LOG_ERR, "ha_msg_add_nv: NULL nvline");
		return(HA_FAIL);
	}
	/* How many characters before the '='? */
	if ((namelen = strcspn(nvline, EQUAL)) <= 0
	||	nvline[namelen] != '=') {
		ha_log(LOG_WARNING, "ha_msg_add_nv: line doesn't contain '='");
		ha_log(LOG_INFO, "%s", nvline);
		return(HA_FAIL);
	}
	valp = nvline + namelen +1; /* Point just *past* the '=' */
	if (valp >= bufmax)		return HA_FAIL;
	vallen = strcspn(valp, CRNL);
	if ((valp + vallen) >= bufmax)	return HA_FAIL;

	/* Call ha_msg_nadd to actually add the name/value pair */
	return(ha_msg_nadd(msg, nvline, namelen, valp, vallen));
	
}

/* Return the value associated with a particular name */
const char *
ha_msg_value(const struct ha_msg * msg, const char * name)
{
	int	j;
	if (!msg || !msg->names || !msg->values) {
		ha_log(LOG_ERR, "ha_msg_value: NULL msg");
		return(NULL);
	}

	AUDITMSG(msg);
	for (j=0; j < msg->nfields; ++j) {
		if (strcmp(name, msg->names[j]) == 0) {
			return(msg->values[j]);
		}
	}
	return(NULL);
}


/* Modify the value associated with a particular name */
int
ha_msg_mod(struct ha_msg * msg, const char * name, const char * value)
{
	int	j;

	AUDITMSG(msg);
	if (msg == NULL || name == NULL || value == NULL) {
		ha_log(LOG_ERR, "ha_msg_mod: NULL input.");
		return HA_FAIL;
	}
	for (j=0; j < msg->nfields; ++j) {
		if (strcmp(name, msg->names[j]) == 0) {
			char *	newv = ha_malloc(strlen(value)+1);
			int	newlen;
			int	sizediff = 0;
			if (newv == NULL) {
				ha_log(LOG_ERR, "ha_msg_mod: out of memory");
				return(HA_FAIL);
			}
			ha_free(msg->values[j]);
			msg->values[j] = newv;
			newlen = strlen(value);
			sizediff = newlen - msg->vlens[j];
			msg->stringlen += sizediff;
			msg->vlens[j] = newlen;
			strcpy(newv, value);
			AUDITMSG(msg);
			return(HA_OK);
		}
	}
	return(ha_msg_add(msg, name, value));
}

/* Return the next message found in the stream */
struct ha_msg *
msgfromstream(FILE * f)
{
	char		buf[MAXLINE];
	const char *	bufmax = buf + sizeof(buf);
	char *		getsret;
	struct ha_msg*	ret;

	clearerr(f);
	/* Skip until we find a MSG_START (hopefully we skip nothing) */
	while(1) {
		getsret = fgets(buf, MAXLINE, f);
		if(!getsret) {
			break;
		}
		if(strcmp(buf, MSG_START) == 0) {
			break;
		}
	}

	if (getsret == NULL || (ret = ha_msg_new(0)) == NULL) {
		/* Getting an error with EINTR is pretty normal */
		/* (so is EOF) */
		if (   (!ferror(f) || (errno != EINTR && errno != EAGAIN))
		&&	!feof(f)) {
			ha_log(LOG_ERR, "msgfromstream: cannot get message");
		}
		return(NULL);
	}

	/* Add Name=value pairs until we reach MSG_END or EOF */
	while(1) {
		getsret = fgets(buf, MAXLINE, f);
		if(!getsret) {
			break;
		}

		if(strlen(buf) > MAXLINE - 2) {
			ha_log(LOG_DEBUG
			,	"msgfromstream: field too long [%s]"
			,	buf);
		}

		if(!strcmp(buf, MSG_END)) {
			break;
		}


		/* Add the "name=value" string on this line to the message */
		if (ha_msg_add_nv(ret, buf, bufmax) != HA_OK) {
			ha_log(LOG_ERR, "NV failure (msgfromsteam): [%s]"
			,	buf);
			ha_msg_del(ret); ret=NULL;
			return(NULL);
		}
	}
	return(ret);
}
/* Return the next message found in the IPC channel */
struct ha_msg *
msgfromIPC(IPC_Channel * ch)
{
	int		rc;
	IPC_Message*	ipcmsg;
	struct ha_msg*	hmsg;

	rc = ch->ops->waitin(ch);

	switch(rc) {
		default:
		case IPC_FAIL:
			ha_perror("msgfromIPC: waitin failure");
			return NULL;

		case IPC_BROKEN:
			sleep(1);
			return NULL;

		case IPC_INTR:
			return NULL;

		case IPC_OK:
			break;
	}


	ipcmsg = NULL;
	rc = ch->ops->recv(ch, &ipcmsg);
#if 0
	if (DEBUGPKTCONT) {
		ha_log(LOG_DEBUG, "msgfromIPC: recv returns %d ipcmsg = 0x%lx"
		,	rc, (unsigned long)ipcmsg);
	}
#endif
	if (rc != IPC_OK) {
		return NULL;
	}

	hmsg = string2msg((char *)ipcmsg->msg_body, ipcmsg->msg_len);
	if(ipcmsg->msg_done) {
		ipcmsg->msg_done(ipcmsg);
	}

	AUDITMSG(hmsg);
	return hmsg;
}


/* Writes a message into a stream - used for serial lines */
int	
msg2stream(struct ha_msg* m, FILE * f)
{
	char *	s  = msg2string(m);
	if (s != NULL) {
		int	rc = HA_OK;
		if (fputs(s, f) == EOF) {
			rc = HA_FAIL;
			ha_perror("msg2stream: fputs failure");
		}
		if (fflush(f) == EOF) {
			ha_perror("msg2stream: fflush failure");
			rc = HA_FAIL;
		}
		ha_free(s);
		return(rc);
	}else{
		return(HA_FAIL);
	}
}
static void ipcmsg_done(IPC_Message* m);

static void
ipcmsg_done(IPC_Message* m)
{
	if (!m) {
		return;
	}
	if (m->msg_body) {
		ha_free(m->msg_body);
	}
	ha_free(m);
	m = NULL;
}


IPC_Message*
hamsg2ipcmsg(struct ha_msg* m, IPC_Channel* ch)
{
	char *		s  = msg2string(m);
	IPC_Message*	ret = NULL;
	if (s == NULL) {
		return ret;
	}
	ret = MALLOCT(IPC_Message);
	if (!ret) {
		ha_free(s);
		return ret;
	}
	ret->msg_done = ipcmsg_done;
	ret->msg_private = NULL;
	ret->msg_ch = ch;
	ret->msg_body = s;
	ret->msg_len = m->stringlen;

	return ret;
}

struct ha_msg*
ipcmsg2hamsg(IPC_Message*m)
{
	struct ha_msg*	ret = NULL;
	
	ret = string2msg(m->msg_body, m->msg_len);

	return ret;
}

int
msg2ipcchan(struct ha_msg*m, IPC_Channel*ch)
{
	IPC_Message*	imsg;

	if (m == NULL || ch == NULL) {
		ha_log(LOG_ERR, "Invalid msg2ipcchan argument");
		errno = EINVAL;
		return HA_FAIL;
	}
	
	if ((imsg = hamsg2ipcmsg(m, ch)) == NULL) {
		ha_log(LOG_ERR, "hamsg2ipcmsg() failure");
		return HA_FAIL;
	}

	if (ch->ops->send(ch, imsg) != IPC_OK) {
		if (ch->ch_status == IPC_CONNECT) {
			ha_log(LOG_ERR
			,	"msg2ipcchan: ch->ops->send() failure");
		}
		imsg->msg_done(imsg);
		return HA_FAIL;
	}
	return HA_OK;
}

/* Converts a string (perhaps received via UDP) into a message */
struct ha_msg *
string2msg(const char * s, size_t length)
{
	struct ha_msg*	ret;
	int		startlen;
	int		endlen;
	const char *	sp = s;
	const char *	smax = s + length;

	if ((ret = ha_msg_new(0)) == NULL) {
		return(NULL);
	}

	startlen = sizeof(MSG_START)-1;
	if (strncmp(sp, MSG_START, startlen) != 0) {
		/* This can happen if the sender gets killed */
		/* at just the wrong time... */
		ha_log(LOG_WARNING, "string2msg: no MSG_START");
		return(NULL);
	}else{
		sp += startlen;
	}

	endlen = sizeof(MSG_END)-1;

	/* Add Name=value pairs until we reach MSG_END or end of string */

	while (*sp != EOS && strncmp(sp, MSG_END, endlen) != 0) {

		if (sp >= smax)		return(NULL);
		/* Skip over initial CR/NL things */
		sp += strspn(sp, CRNL);
		if (sp >= smax)		return(NULL);

		/* End of message marker? */
		if (strncmp(sp, MSG_END, endlen) == 0) {
			break;
		}
		/* Add the "name=value" string on this line to the message */
		if (ha_msg_add_nv(ret, sp, smax) != HA_OK) {
			ha_log(LOG_ERR, "NV failure (string2msg):");
			ha_log(LOG_ERR, "Input string: [%s]", s);
			ha_msg_del(ret);
			return(NULL);
		}
		if (sp >= smax)		return(NULL);
		sp += strcspn(sp, CRNL);
	}
	return(ret);
}


/* Converts a message into a string (for sending out UDP interface) */
char *
msg2string(const struct ha_msg *m)
{
	int	j;
	char *	buf;
	char *	bp;

	AUDITMSG(m);
	if (m->nfields <= 0) {
		ha_log(LOG_ERR, "msg2string: Message with zero fields");
		return(NULL);
	}

	buf = ha_malloc(m->stringlen);

	if (buf == NULL) {
		ha_log(LOG_ERR, "msg2string: no memory for string");
	}else{
		bp = buf;
		strcpy(buf, MSG_START);
		for (j=0; j < m->nfields; ++j) {
			strcat(bp, m->names[j]);
			bp += m->nlens[j];
			strcat(bp, "=");
			bp++;
			strcat(bp, m->values[j]);
			bp += m->vlens[j];
			strcat(bp, "\n");
			bp++;
		}
		strcat(bp, MSG_END);
	}
	return(buf);
}

void
ha_log_message (const struct ha_msg *m)
{
	int	j;

	AUDITMSG(m);
	ha_log(LOG_INFO, "MSG: Dumping message with %d fields", m->nfields);

	for (j=0; j < m->nfields; ++j) {
		ha_log(LOG_INFO, "MSG[%d]: [%s=%s]",j
		,	m->names[j] ? m->names[j] : "NULL"
		,	m->values[j] ? m->values[j] : "NULL");
	}
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
 * $Log: ha_msg.c,v $
 * Revision 1.46  2003/10/29 04:05:00  alan
 * Changed things so that the API uses IPC instead of FIFOs.
 * This isn't 100% done - python API code needs updating, and need to check authorization
 * for the ability to "sniff" other people's packets.
 *
 * Revision 1.45  2003/07/14 04:30:49  alan
 * This patch from Kurosawa-san (by way of Horms):
 *    Heartbeat uses poll() in order to check messages in API FIFO and
 *    stdio functions to read messages.  stdio functions (fgets() in
 *    msgfromstream() in this case) uses a internal buffer.  When an application
 *    sends 2 messages at a time to API FIFO,  heartbeat's fgets() in
 *    msgfromstream() may read 2 messages to the internal buffer at a time.
 *    But heartbeat processes only one message and leaves the latter
 *    message, because there is no poll() event for the file descriptor.
 *
 * Revision 1.44  2003/06/24 06:36:51  alan
 * Fixed an unsafe sprintf which occurred only when high levels of debug
 * were turned on.
 *
 * Revision 1.43  2003/05/09 15:15:37  alan
 * Turned off the most expensive and onerous debugging code.
 *
 * Revision 1.42  2003/04/18 06:33:54  alan
 * Changed the audit code for messages to tolerate NULL message pointers.
 *
 * Revision 1.41  2003/04/18 06:09:46  alan
 * Fixed an off-by-one error in writing messages to the FIFO.
 * Also got rid of some now-unused functions, and fixed a minor glitch in BasicSanitCheck.
 *
 * Revision 1.40  2003/04/15 23:05:01  alan
 * Added new message copying function, and code
 * to check the integrity of messages.  Too slow now, will turn it down later.
 *
 * Revision 1.39  2003/03/27 07:04:26  alan
 * 1st step in heartbeat process restructuring.
 * Create fifo_child() processes to read the FIFO written by the shell scripts.
 *
 * Revision 1.38  2003/02/07 08:37:16  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.37  2003/02/05 09:06:33  horms
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
 * Revision 1.36  2002/11/22 07:04:39  horms
 * make lots of symbols static
 *
 * Revision 1.35  2002/10/30 17:17:40  alan
 * Added some debugging, and changed one message from an ERROR to a WARNING.
 *
 * Revision 1.34  2002/10/22 17:41:58  alan
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
 * Revision 1.33  2002/10/21 10:17:18  horms
 * hb api clients may now be built outside of the heartbeat tree
 *
 * Revision 1.32  2002/10/18 07:16:08  alan
 * Put in Horms big patch plus a patch for the apcmastersnmp code where
 * a macro named MIN returned the MAX instead.  The code actually wanted
 * the MAX, so when the #define for MIN was surrounded by a #ifndef, then
 * it no longer worked...  This fix courtesy of Martin Bene.
 * There was also a missing #include needed on older Linux systems.
 *
 * Revision 1.31  2002/10/08 14:33:18  msoffen
 * Changed ha_log_message to be NULL safe.
 *
 * Revision 1.30  2002/10/02 13:36:42  alan
 * Put in a fix from Nathan Wallwork for a potential security vulnerability.
 *
 * Revision 1.29  2002/09/26 06:09:38  horms
 * log a debug message if it looks like an feild in a heartbeat message has been truncated
 *
 * Revision 1.28  2002/09/20 02:09:50  alan
 * Switched heartbeat to do everything with longclock_t instead of clock_t.
 * Switched heartbeat to be configured fundamentally from millisecond times.
 * Changed heartbeat to not use alarms for much of anything.
 * These are relatively major changes, but the seem to work fine.
 *
 * Revision 1.27  2002/09/17 20:48:06  alan
 * Put in a check for NULL in ha_msg_mod().
 *
 * Revision 1.26  2002/08/10 02:13:32  alan
 * Better error logging when ha_msg functions are given bad name/value pairs.
 *
 * Revision 1.25  2002/07/08 04:14:12  alan
 * Updated comments in the front of various files.
 * Removed Matt's Solaris fix (which seems to be illegal on Linux).
 *
 * Revision 1.24  2002/04/13 22:35:08  alan
 * Changed ha_msg_add_nv to take an end pointer to make it safer.
 * Added a length parameter to string2msg so it would be safer.
 * Changed the various networking plugins to use the new string2msg().
 *
 * Revision 1.23  2002/04/11 05:57:44  alan
 * Made some of the debugging output clearer.
 *
 * Revision 1.22  2002/02/21 21:43:33  alan
 * Put in a few fixes to make the client API work more reliably.
 * Put in a few changes to the process exit handling code which
 * also cause heartbeat to (attempt to) restart when it finds one of it's
 * own processes dies.  Restarting was already broken :-(
 *
 * Revision 1.21  2002/02/14 14:09:29  alan
 * Put in a change requested by Ram Pai to allow message values to be
 * empty strings.
 *
 * Revision 1.20  2001/10/24 20:46:28  alan
 * A large number of patches.  They are in these categories:
 * 	Fixes from Matt Soffen
 * 	Fixes to test environment things - including changing some ERRORs to
 * 		WARNings and vice versa.
 * 	etc.
 *
 * Revision 1.19  2001/08/21 15:37:13  alan
 * Put in code to make sure the calls in msg2stream get checked for errors...
 *
 * Revision 1.18  2001/06/19 13:56:28  alan
 * FreeBSD portability patch from Matt Soffen.
 * Mainly added #include "portability.h" to lots of files.
 * Also added a library to Makefile.am
 *
 * Revision 1.17  2001/06/12 17:05:47  alan
 * Fixed bug reported by Emily Ratliff <ratliff@austin.ibm.com>
 * In ha_msg_mod() the code fails to update the stringlen value for
 * fields modified by the input parameters.
 * This could potentially cause a crash.
 * Thanks to Emily for reporting this bug!
 *
 * Revision 1.16  2001/05/11 14:55:06  alan
 * Followed David Lee's suggestion about splitting out all the heartbeat process
 * management stuff into a separate header file...
 * Also changed to using PATH_MAX for maximum pathname length.
 *
 * Revision 1.15  2000/07/26 05:17:19  alan
 * Added GPL license statements to all the code.
 *
 * Revision 1.14  2000/07/19 23:03:53  alan
 * Working version of most of the API code.  It still has the security bug...
 *
 * Revision 1.13  2000/07/11 14:42:42  alan
 * More progress on API code.
 *
 * Revision 1.12  2000/07/11 00:25:52  alan
 * Added a little more API code.  It looks like the rudiments are now working.
 *
 * Revision 1.11  2000/05/11 22:47:50  alan
 * Minor changes, plus code to put in hooks for the new API.
 *
 * Revision 1.10  2000/04/12 23:03:49  marcelo
 * Added per-link status instead per-host status. Now we will able
 * to develop link<->service dependacy scheme.
 *
 * Revision 1.9  1999/11/22 20:28:23  alan
 * First pass of putting real packet retransmission.
 * Still need to request missing packets from time to time
 * in case retransmit requests get lost.
 *
 * Revision 1.8  1999/10/25 15:35:03  alan
 * Added code to move a little ways along the path to having error recovery
 * in the heartbeat protocol.
 * Changed the code for serial.c and ppp-udp.c so that they reauthenticate
 * packets they change the ttl on (before forwarding them).
 *
 * Revision 1.7  1999/10/10 20:11:56  alanr
 * New malloc/free (untested)
 *
 * Revision 1.6  1999/10/05 06:00:55  alanr
 * Added RPM Cflags to Makefiles
 *
 * Revision 1.5  1999/10/03 03:13:43  alanr
 * Moved resource acquisition to 'heartbeat', also no longer attempt to make the FIFO, it's now done in heartbeat.  It should now be possible to start it up more readily...
 *
 * Revision 1.4  1999/09/29 03:22:05  alanr
 * Added the ability to reread auth config file on SIGHUP
 *
 * Revision 1.3  1999/09/26 21:59:58  alanr
 * Allow multiple auth strings in auth file... (I hope?)
 *
 * Revision 1.2  1999/09/26 14:01:01  alanr
 * Added Mijta's code for authentication and Guenther Thomsen's code for serial locking and syslog reform
 *
 * Revision 1.9  1999/09/16 05:50:20  alanr
 * Getting ready for 0.4.3...
 *
 * Revision 1.8  1999/08/25 06:34:26  alanr
 * Added code to log outgoing messages in a FIFO...
 *
 * Revision 1.7  1999/08/18 04:28:48  alanr
 * added function to dump a message to the log...
 *
 * Revision 1.6  1999/08/17 03:46:48  alanr
 * added log entry...
 *
 */
