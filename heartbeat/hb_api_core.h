/*
 * hb_api_core_h: Internal definitions and functions for the heartbeat API
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *
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

/*
 * NOTE:  This header NOT intended to be included by anything other than
 * heartbeat.  It is NOT a global header file, and should NOT be installed anywhere
 * outside the heartbeat tree.
 */

#ifndef _HB_API_CORE_H
#	define _HB_API_CORE_H 1
#include <sys/types.h>
#include <glib.h>
#include <clplumbing/longclock.h>
#include <clplumbing/GSource.h>
#include <ha_msg.h>

/*
 *   Per-client API data structure.
 */

typedef struct client_process {
	char	client_id[32];  /* Client identification */
	pid_t	pid;		/* PID of client process */
	uid_t	uid;		/* UID of client  process */
	int	iscasual;	/* 1 if this is a "casual" client */
	int	isindispatch;	/* TRUE if we're in dispatch now */
	const char*removereason;/* non-NULL if client is being removed */
	FILE*	input_fifo;	/* Input FIFO file pointer */
	int	output_fifofd;	/* Output FIFO file descriptor */
	GList*	msgQ;		/* Queue of msgs for client */
	int	msgcount;	/* length of client message queue */
	int    	signal;		/* What signal to indicate new msgs */
	int   	desired_types;	/* A bit mask of desired message types*/
	struct client_process*  next;
	GFDSource*		g_source_id;	/* return from G_main_add_fd() */
	int			fd;	/* FD that goes with g_source_id */
	
	longclock_t	next_app_hb;	/* Next application heartbeat time */
	longclock_t	app_hb_ticks;	/* ticks between app heartbeats */
}client_proc_t;

/*
 * Types of messages. 
 * DROPIT and/or DUPLICATE are only used when a debugging callback
 * is registered.
 */ 


/*
 *	This next set of defines is for the types of packets that come through
 *	heartbeat.
 *
 *	Any given packet behaves like an enumeration (should only have one bit
 *	on), but the options from client software treat them more like a set
 *	(bit field), with more than one at a time being on.  Normally the
 *	client only requests KEEPIT packets, but for debugging may want to
 *	ask to see the others too.
 */
#define	KEEPIT		0x01	/* A set of bits */
#define	NOCHANGE	0x02
#define	DROPIT		0x04
#define DUPLICATE	0x08
#define APICALL		0x10
#define PROTOCOL	0x20
#define	DEBUGTREATMENTS	(DROPIT|DUPLICATE|APICALL|NOCHANGE|PROTOCOL)
#define	ALLTREATMENTS	(DEBUGTREATMENTS|KEEPIT)
#define	DEFAULTREATMENT	(KEEPIT)

#define	API_SIGNON		"signon"
#define	API_SIGNOFF		"signoff"
#define	API_SETFILTER		"setfilter"
#	define	F_FILTERMASK	"fmask"
#define	API_SETSIGNAL		"setsignal"
#	define	F_SIGNAL	"signal"
#define	API_NODELIST		"nodelist"
#	define	F_NODENAME	"node"
#define	API_NODELIST_END	"nodelist-end"
#define	API_NODESTATUS		"nodestatus"
#define	API_NODETYPE		"nodetype"

#define	API_IFLIST		"iflist"
#	define	F_IFNAME	"ifname"
#define	API_IFLIST_END		"iflist-end"
#define	API_IFSTATUS		"ifstatus"
#define	API_GETPARM		"getparm"
#define	API_GETRESOURCES	"getrsc"


#define	API_OK			"OK"
#define	API_FAILURE		"fail"
#define	API_BADREQ		"badreq"
#define	API_MORE		"ok/more"

#define	API_FIFO_DIR	VAR_LIB_D "/api"
#define	API_FIFO_LEN	(sizeof(API_FIFO_DIR)+32)

#define	NAMEDCLIENTDIR	API_FIFO_DIR
#define	CASUALCLIENTDIR	VAR_LIB_D "/casual"

#define	REQ_SUFFIX	".req"
#define	RSP_SUFFIX	".rsp"

#ifndef API_REGFIFO
#	define	API_REGFIFO	VAR_LIB_D "/register"
#endif

void api_heartbeat_monitor(struct ha_msg *msg, int msgtype, const char *iface);
void api_process_registration(struct ha_msg *msg);
void process_api_msgs(fd_set* inputs, fd_set* exceptions);
int  compute_msp_fdset(fd_set* set, int fd1, int fd2);
gboolean api_audit_clients(gpointer p);

/* Return code for API query handlers */

#define I_API_RET	0 /* acknowledge client of successful API query */
#define I_API_IGN	1 /* do nothing */
#define I_API_BADREQ	2 /* send error msg to client with "failreason" as error reason */

/* Handler of API query */
typedef int (*api_query_handler_t) (const struct ha_msg* msg
			, struct ha_msg *resp, client_proc_t* client
			, const char **failreason);

struct api_query_handler {
		const char *queryname;
		api_query_handler_t handler;
};

#endif /* _HB_API_CORE_H */
