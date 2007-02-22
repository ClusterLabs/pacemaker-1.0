/*
 * ha_msg_internal: heartbeat internal messaging functions
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <lha_internal.h>
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


	if (netstring_format || must_use_netstring(ret)){
		goto out;
	}
	
	if ( add_msg_auth(ret) != HA_OK) {
		ha_msg_del(ret);
		ret = NULL;
	}
	if (DEBUGPKTCONT) {
		ha_log(LOG_DEBUG, "add_control_msg_fields: packet returned");
		cl_log_message(LOG_DEBUG, ret);
	}

 out:
	return ret;
}



int
add_msg_auth(struct ha_msg * m)
{
	char	msgbody[MAXLINE];
	char	authstring[MAXLINE];
	char	authtoken[MAXLINE];	
	char*	msgbuf;
	int	buf_malloced = 0;
	int	buflen;
	const char *	from;
	const char *	ts;
	const char *	type;
	int		ret =  HA_FAIL;
       

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
	
	
	buflen =  get_stringlen(m);
	if (buflen < MAXLINE){
		msgbuf = &msgbody[0];
	}else{
		msgbuf =  cl_malloc(get_stringlen(m));
		if (msgbuf == NULL){
			cl_log(LOG_ERR, "%s: malloc failed",
			       __FUNCTION__);
			goto out;
		}
		buf_malloced = 1;
	}
	
	
	check_auth_change(config);
	msgbuf[0] = EOS;
	
	if (msg2string_buf(m, msgbuf, buflen, 0, NOHEAD) != HA_OK){
		ha_log(LOG_ERR
		       ,	"add_msg_auth: compute string failed");
		cl_log_message(LOG_ERR,m); 
		goto out;
	}
	

	
	if (!config->authmethod->auth->auth(config->authmethod, msgbuf
	,	strnlen(msgbuf, buflen)
	,	authtoken, DIMOF(authtoken))) {
		ha_log(LOG_ERR 
		,	"Cannot compute message authentication [%s/%s/%s]"
		,	config->authmethod->authname
		,	config->authmethod->key
		,	msgbuf);
		goto out;
	}

	sprintf(authstring, "%d %s", config->authnum, authtoken);

	/* It will add it if it's not there yet, or modify it if it is */
	ret= ha_msg_mod(m, F_AUTH, authstring);


 out:
	if (msgbuf && buf_malloced){
		cl_free(msgbuf);
	}       
	
	return ret;

}

gboolean
isauthentic(const struct ha_msg * m)
{
	char	msgbody[MAXLINE];
	char	authstring[MAXLINE];
	char	authbuf[MAXLINE];
	char*	msgbuf;
	int	buflen;
	int	buf_malloced = 0;
	const char *		authtoken = NULL;
	int			j;
	int			authwhich = 0;
	struct HBauth_info*	which;
	gboolean		ret =FALSE;

	
	buflen = get_stringlen(m);
	if (buflen < MAXLINE){
		msgbuf = &msgbody[0];
	}else{
		msgbuf =  cl_malloc(get_stringlen(m));
		if (msgbuf == NULL){
			cl_log(LOG_ERR, "%s: malloc failed",
			       __FUNCTION__);
			goto  out;
		}
		buf_malloced = 1;
	}

	/* Reread authentication? */
	check_auth_change(config);

	if (msg2string_buf(m, msgbuf, buflen,0, NOHEAD) != HA_OK){
		ha_log(LOG_ERR
		       ,	"add_msg_auth: compute string failed");
		goto out;
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
			if (ANYDEBUG){
				cl_log_message(LOG_INFO, m);
			}
		}
		goto out;
	}
	which = config->auth_config + authwhich;

	if (authwhich < 0 || authwhich >= MAXAUTH || which->auth == NULL) {
		ha_log(LOG_WARNING
		,	"Invalid authentication type [%d] in message!"
		,	authwhich);
		goto out;
	}
		
	
	if (!which->auth->auth(which
        ,	msgbuf, strnlen(msgbuf, buflen)
	,	authbuf, DIMOF(authbuf))) {
		ha_log(LOG_ERR, "Failed to compute message authentication");
		goto out;
	}
	if (strcmp(authstring, authbuf) == 0) {
		if (DEBUGAUTH) {
			ha_log(LOG_DEBUG, "Packet authenticated");
		}
		ret = TRUE;
		goto out;
	}
	if (DEBUGAUTH) {
		ha_log(LOG_INFO, "Packet failed authentication check, "
		       "authstring =%s,authbuf=%s ", authstring, authbuf);
	}
	
 out:	
	if (buf_malloced && msgbuf){
		cl_free(msgbuf);
	}
	return ret;

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

