/*
 * Intracluster message object (struct ha_msg)
 *
 * Copyright (C) 1999, 2000 Alan Robertson <alanr@unix.sh>
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

#ifndef _HA_MSG_H
#	define _HA_MSG_H 1
static const char * _ha_msg_h_Id = "$Id: ha_msg.h,v 1.24 2003/11/20 03:13:55 alan Exp $";
#include <stdio.h>
#include <clplumbing/ipc.h>

#define	HA_FAIL		0
#define	HA_OK		1

struct ha_msg {
	int	nfields;
	int	nalloc;
	size_t	stringlen;	/* #bytes needed to convert this to a string
				 * including the '\0' character at the end. */
	char **	names;
	int  *	nlens;
	char **	values;
	int  *	vlens;
};
#define	IFACE		"!^!\n"  
#define	MSG_START	">>>\n"
#define	MSG_END		"<<<\n"
#define	EQUAL		"="

#define	MAXMSG	1400	/* Maximum string length for a message */

	/* Common field names for our messages */
#define	F_TYPE		"t"		/* Message type */
#define	F_ORIG		"src"		/* Real Originator */
#define	F_NODE		"node"		/* Node being described */
#define	F_TO		"dest"		/* Destination (optional) */
#define	F_STATUS	"st"		/* New status (type = status) */
#define	F_TIME		"ts"		/* Timestamp */
#define F_SEQ		"seq"		/* Sequence number */
#define	F_LOAD		"ld"		/* Load average */
#define	F_COMMENT	"info"		/* Comment */
#define	F_TTL		"ttl"		/* Time To Live */
#define F_AUTH		"auth"		/* Authentication string */
#define F_HBGENERATION	"hg"		/* Heartbeat generation number */
#define F_FIRSTSEQ	"firstseq"	/* Lowest seq # to retransmit */
#define F_LASTSEQ	"lastseq"	/* Highest seq # to retransmit */
#define F_RESOURCES	"rsc_hold"	/* What resources do we hold? */
#define F_FROMID	"from_id"	/* from Client id */
#define F_TOID		"to_id"		/* To client id */
#define F_PID		"pid"		/* PID of client */
#define F_UID		"uid"		/* uid of client */
#define F_GID		"gid"		/* gid of client */
#define F_ISSTABLE	"isstable"	/* true/false for RESOURCES */
#define F_APIREQ	"reqtype"	/* API request type for "hbapi" */
#define F_APIRESULT	"result"	/* API request result code */
#define F_IFNAME	"ifname"	/* Interface name */
#define F_PNAME		"pname"		/* Parameter name */
#define F_PVALUE	"pvalue"	/* Parameter name */
#define F_DEADTIME	"deadtime"	/* Dead time interval in ms. */
#define F_KEEPALIVE	"keepalive"	/* Keep alive time interval in ms. */
#define F_LOGFACILITY	"logfacility"	/* Suggested cluster syslog facility */
#define F_NODETYPE	"nodetype"	/* Type of node */
#define F_RTYPE		"rtype"		/* Resource type */
#define F_ORDERSEQ	"oseq"		/* Order Sequence number */

	/* Message types */
#define	T_STATUS	"status"	/* Status (heartbeat) */
#define	T_IFSTATUS	"ifstat"	/* Interface status */
#define	T_ASKRESOURCES	"ask_resources"	/* Let other node ask my resources */
#define T_ASKRELEASE	"ip-request"	/* Please give up these resources... */
#define T_ACKRELEASE	"ip-request-resp"/* Resources given up... */
#define	T_STONITH	"stonith"	/* Stonith return code */
#define T_SHUTDONE	"shutdone"	/* External Shutdown complete */

#define T_APIREQ	"hbapi-req"	/* Heartbeat API request */
#define T_APIRESP	"hbapi-resp"	/* Heartbeat API response */
#define T_APICLISTAT	"hbapi-clstat"	/* Client status notification" */

#define	NOSEQ_PREFIX	"NS_"		/* PREFIX: Give no sequence number    */
	/* Used for messages which can't be retransmitted		      */
	/* Either they're protocol messages or from dumb (ping) endpoints     */
#define	T_REXMIT	NOSEQ_PREFIX "rexmit"    	 /* Rexmit request    */
#define	T_NAKREXMIT	NOSEQ_PREFIX "nak_rexmit"	/* NAK Rexmit request */
#define	T_NS_STATUS	NOSEQ_PREFIX "st"		/* ping status        */

/* Messages associated with nice_failback */
#define T_STARTING      "starting"      /* Starting Heartbeat		*/
					/* (requesting resource report)	*/
#define T_RESOURCES	"resource"      /* Resources report		*/

/* Messages associated with stonith completion results */
#define T_STONITH_OK		"OK"  	  /* stonith completed successfully */
#define T_STONITH_BADHOST	"badhost" /* stonith failed */
#define T_STONITH_BAD		"bad"	  /* stonith failed */
#define T_STONITH_NOTCONFGD	"n_stnth" /* no stonith device configured */
#define T_STONITH_UNNEEDED	"unneeded" /* STONITH not required */


/* Allocate new (empty) message */
struct ha_msg *	ha_msg_new(int nfields);

/* Free message */
void		ha_msg_del(struct ha_msg *msg);

/* Copy message */
struct ha_msg*	ha_msg_copy(const struct ha_msg *msg);

/* Add null-terminated name and a value to the message */
int		ha_msg_add(struct ha_msg * msg
		,	const char* name, const char* value);

/* Modify null-terminated name and a value to the message */
int		ha_msg_mod(struct ha_msg * msg
		,	const char* name, const char* value);

/* Add name, value (with known lengths) to the message */
int		ha_msg_nadd(struct ha_msg * msg, const char * name, int namelen
		,	const char * value, int vallen);

/* Add name=value string to a message */
int		ha_msg_add_nv(struct ha_msg* msg, const char * nvline, const char * bufmax);

/* Return value associated with particular name */
const char *	ha_msg_value(const struct ha_msg * msg, const char * name);

/* Reads an IPC stream -- converts it into a message */
struct ha_msg *	msgfromIPC(IPC_Channel * f);

/* Reads a stream -- converts it into a message */
struct ha_msg *	msgfromstream(FILE * f);

/* Same as above plus copying the iface name to "iface" */
struct ha_msg * if_msgfromstream(FILE * f, char *iface);

/* Writes a message into a stream */
int		msg2stream(struct ha_msg* m, FILE * f);

/* Converts a message into a string and adds the iface name on start */
char *     msg2if_string(const struct ha_msg *m, const char * iface);

/* Converts a string gotten via UDP into a message */
struct ha_msg *	string2msg(const char * s, size_t length);

/* Converts a message into a string */
char *		msg2string(const struct ha_msg *m);
/* Converts an ha_msg into an IPC message */
IPC_Message* hamsg2ipcmsg(struct ha_msg* m, IPC_Channel* ch);
/* Converts an IPC message into an ha_msg */
struct ha_msg* ipcmsg2hamsg(IPC_Message*m);
/* Outputs a message to an IPC channel */
int msg2ipcchan(struct ha_msg*m, IPC_Channel*ch);

/* Reads from control fifo, and creates a new message from it */
/* This adds the default sequence#, load avg, etc. to the message */
struct ha_msg *	controlfifo2msg(FILE * f);
void		ha_log_message(const struct ha_msg* msg);
int		isauthentic(const struct ha_msg * msg);
#endif /* __HA_MSG_H */
