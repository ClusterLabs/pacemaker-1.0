/* $Id: hb_api.h,v 1.42 2005/12/21 02:34:32 gshi Exp $ */
/*
 * Client-side Low-level clustering API for heartbeat.
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

/*
 * Currently the client-side heartbeat API needs to write in the /var/lock
 * directory for non-casual (named) clients.  This has implications for the
 * euid, egid that we run as.
 *
 * Expect to set make your binaries setgid to uucp, or allow the uid
 * they run as to join the group uucp (or whatever your local system
 * has it set up as).
 *
 * Additionally, you must belong to the group hbapi.  Fortunately, UNIX
 * group permissions are quite flexible, and you can do both.
 */

/*
 * Known deficiencies of this API:
 *
 * Each of the various set..callback functions should probably return
 * the current callback and private data parameter, so the caller can
 * restore them later.
 *
 */

#ifndef __HB_API_H
#	define __HB_API_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <ha_msg.h>
#include <clplumbing/ipc.h>

#define	LLC_PROTOCOL_VERSION	2

#include <clplumbing/cl_uuid.h>

typedef void (*llc_msg_callback_t) (struct ha_msg* msg
,	void* private_data);

typedef void (*llc_nstatus_callback_t) (const char *node, const char * status
,	void* private_data);

typedef void (*llc_ifstatus_callback_t) (const char *node
,	const char * interface, const char * status
,	void* private_data);

typedef void (*llc_cstatus_callback_t) (const char *node
,	const char * client, const char * status
,	void* private_date);

typedef struct ll_cluster {
	void *		ll_cluster_private;
	struct llc_ops*	llc_ops;
}ll_cluster_t;

struct llc_ops {
	int		(*signon) (ll_cluster_t*, const char * clientid);
	int		(*signoff) (ll_cluster_t*, gboolean destroy_channel);
	int		(*delete) (ll_cluster_t*);
	
/*
 *************************************************************************
 * Status Update Callbacks
 *************************************************************************
 */

/*
 *	set_msg_callback:	Define callback for the given message type 
 *
 *	msgtype:	Type of message being handled. 
 *			Messages intercepted by nstatus_callback or
 *			ifstatus_callback functions won't be handled here.
 *
 *	callback:	callback function.
 *
 *	p:		private data - later passed to callback.
 */
	int		(*set_msg_callback) (ll_cluster_t*, const char * msgtype
	,			llc_msg_callback_t callback, void * p);

/*
 *	set_nstatus_callback:	Define callback for node status messages
 *				This is a message of type "status"
 *
 *	cbf:		callback function.
 *
 *	p:		private data - later passed to callback.
 */

	int		(*set_nstatus_callback) (ll_cluster_t*
	,		llc_nstatus_callback_t cbf, 	void * p);
/*
 *	set_ifstatus_callback:	Define callback for interface status messages
 *				This is a message of type "ifstat"
 *			These messages are received whenever an interface goes
 *			dead or becomes active again.
 *
 *	cbf:		callback function.
 *
 *	p:		private data - later passed to callback.
 */
	int             (*set_ifstatus_callback) (ll_cluster_t*
	,		llc_ifstatus_callback_t cbf, void * p);
/*
 *	set_cstatus_callback:	Define callback from client status messages
 *				This is a message of type "hbapi-clstat"
 *			These messages are received whenever an client on
 *			other nodes goes dead or becomes active again.
 *
 *	cbf		callback function.
 *
 *	p:		private data - later passed to callback.
 */
	int		(*set_cstatus_callback) (ll_cluster_t*
	,		llc_cstatus_callback_t cbf, void * p);


/*************************************************************************
 * Getting Current Information
 *************************************************************************/

/*
 *	init_nodewalk:	Initialize walk through list of list of known nodes
 */
	int		(*init_nodewalk)(ll_cluster_t*);
/*
 *	nextnode:	Return next node in the list of known nodes
 */
	const char *	(*nextnode)(ll_cluster_t*);
/*
 *	end_nodewalk:	End walk through the list of known nodes
 */
	int		(*end_nodewalk)(ll_cluster_t*);
/*
 *	node_status:	Return most recent heartbeat status of the given node
 */
	const char *	(*node_status)(ll_cluster_t*, const char * nodename);
/*
 *	node_type:	Return type of the given node
 */
	const char *	(*node_type)(ll_cluster_t*, const char * nodename);
/*
 *	num_nodes:	Return the number of nodes(excluding ping nodes)
 */
	int		(*num_nodes)(ll_cluster_t*);

/*
 *	init_ifwalk:	Initialize walk through list of list of known interfaces
 */
	int		(*init_ifwalk)(ll_cluster_t*, const char * node);
/*
 *	nextif:	Return next node in the list of known interfaces on node
 */
	const char *	(*nextif)(ll_cluster_t*);
/*
 *	end_ifwalk:	End walk through the list of known interfaces
 */
	int		(*end_ifwalk)(ll_cluster_t*);
/*
 *	if_status:	Return current status of the given interface
 */
	const char*	(*if_status)(ll_cluster_t*, const char * nodename
,			const char *iface);
/*
 *	client_status:	Return current status of the given client
 */
	const char*	(*client_status)(ll_cluster_t*, const char *host,
			const char *clientid, int timeout);

/*
 *	get_uuid_by_name:
 *			return the uuid for the node which has the given name
 */
	
	int		(*get_uuid_by_name)(ll_cluster_t*,
					    const char*,
					    cl_uuid_t*);
/*
 *	get_name_by_uuid:
 *			return the name for the node which has the given uuid
 */
	
	int		(*get_name_by_uuid)(ll_cluster_t*,
					    cl_uuid_t*,
					    char*,
					    size_t);
	
	
/*************************************************************************
 * Intracluster messaging
 *************************************************************************/

/*
 *	sendclustermsg:	Send the given message to all cluster members
 */
	int		(*sendclustermsg)(ll_cluster_t*
,			struct ha_msg* msg);
/*
 *	sendnodemsg:	Send the given message to the given node in cluster.
 */
	int		(*sendnodemsg)(ll_cluster_t*
,			struct ha_msg* msg
,			const char * nodename);

/*
 *	sendnodemsg_byuuid: 
 *			Send the given message to the given node in cluster.
 */

	int		(*sendnodemsg_byuuid)(ll_cluster_t*,
					      struct ha_msg* msg,
					      cl_uuid_t*);

/*
 *	send_ordered_clustermsg: Send ordered message to all cluster members.
 */
	int		(*send_ordered_clustermsg)(ll_cluster_t*
,			struct ha_msg* msg);
/*
 *	send_ordered_nodemsg:	Send ordered message to node.
 */
	int		(*send_ordered_nodemsg)(ll_cluster_t*
,			struct ha_msg* msg
,			const char* nodename);
 
 
/*
 *	inputfd:	Return fd which can be given to select(2) or poll(2)
 *			for determining when messages are ready to be read.
 *			Only to be used in select() or poll(), please...
 *			Note that due to IPC input buffering, always check
 *			msgready() before going into select() or poll()
 *			or you might hang there forever.
 */
	int		(*inputfd)(ll_cluster_t*);
 
/*
 *	ipcchan:	Return IPC channel which can be given to
 *			G_main_add_IPC_Channel() for mainloop use.
 *			Please do not use send(), recv() directly.
 *			Feel free to use waitin(), waitout(),
 *			is_message_pending(), is_sending_blocked(),
 *			set_recv_qlen(), set_send_qlen(), resume_io(),
 *			verify_auth().
 */
	IPC_Channel*	(*ipcchan)(ll_cluster_t*);
/*
 *	msgready:	Returns TRUE (1) when a message is ready to be read.
 */
	int		(*msgready)(ll_cluster_t*);
/*
 *	setmsgsignal:	Associates the given signal with the "message waiting"
 *			condition.
 */
	int		(*setmsgsignal)(ll_cluster_t*, int signo);
/*
 *	rcvmsg:	Cause the next message to be read - activating callbacks for
 *		processing the message.  If no callback processes the message
 *		it will be ignored.  The message is automatically disposed of.
 *		It returns 1 if a message was received.
 */
	int		(*rcvmsg)(ll_cluster_t*, int blocking);

/*
 *	Return next message not intercepted by a callback.
 *	NOTE: you must dispose of this message by calling ha_msg_del().
 */
	struct ha_msg* (*readmsg)(ll_cluster_t*, int blocking);

/*
 *************************************************************************
 * Debugging
 *************************************************************************
 *
 *	setfmode: Set filter mode.  Analagous to promiscous mode in TCP.
 *		Gotta be root to turn on debugging!
 *
 *	LLC_FILTER_DEFAULT (default)
 *		In this mode, all messages destined for this pid
 *		are received, along with all that don't go to specific pids.
 *
 *	LLC_FILTER_PMODE See all messages, but filter heart beats
 *
 *				that don't tell us anything new.
 *	LLC_FILTER_ALLHB See all heartbeats, including those that
 *				 don't change status.
 *	LLC_FILTER_RAW	See all packets, from all interfaces, even
 *			dups.  Pkts with auth errors are still ignored.
 *
 *	Set filter mode.  Analagous to promiscous mode in TCP.
 *
 */
#	define	LLC_FILTER_DEFAULT	0
#	define	LLC_FILTER_PMODE	1
#	define	LLC_FILTER_ALLHB	2
#	define	LLC_FILTER_RAW		3

	int (*setfmode)(ll_cluster_t*, unsigned mode);
/*
 *	Return the value of a heartbeat configuration parameter
 *	as a malloc-ed string().  You need to free() the result when
 *	you're done with it.
 */
	char * (*get_parameter)(ll_cluster_t *, const char * paramname);

/*
 *	Return heartbeat's deadtime
 */
	long (*get_deadtime)(ll_cluster_t *);


/*
 *	Return heartbeat's keepalive time
 */
	long (*get_keepalive)(ll_cluster_t *);

/*
 *	Return my node id
 */
	const char * (*get_mynodeid)(ll_cluster_t *);


/*
 *	Return a suggested logging facility for cluster things
 *
 *	< 0 means we're not logging to syslog.
 */
	int (*get_logfacility)(ll_cluster_t *);

/*
 *	Return the current resource ownership status.
 *
 *	NOTE:  this call will fail if heartbeat isn't
 *	managing resources.  It can return "all", "local" or "foreign", "none"
 *	or "transition".  This call will eventually go away when we rewrite
 *	the resource management code.  "transition" means that things are
 *	currently changing.
 */
	const char * (*get_resources)(ll_cluster_t *);

	/*
	 * chan_is_connected()
	 *
	 * Return true if the channel is connected
	 */
	
	gboolean (*chan_is_connected)(ll_cluster_t *);

	
	/* Set the send queue length in heartbeat side
	   for the channel. This function can be used
	   to set a large send queue if the client will
	   receive slowly */
	
	int	(*set_sendq_len)(ll_cluster_t* lcl, int length);
	     
	/* set the send blocking mode
	 * TRUE indicate blocking, i.e if the send queue is full
	 * the function will block there until there are slots available
	 * or the IPC is disconnected
	 * FALSE indicates the function will return immediately even 
	 * if there is no slot available
	 */	
	int (*set_send_block_mode)(ll_cluster_t*, gboolean);
	
	     
	const char * (*errmsg)(ll_cluster_t*);
};

/* Parameters we can ask for via get_parameter */
#define	KEY_HBVERSION	"hbversion"	/* Not a configuration parameter */
#define	KEY_HOST	"node"
#define KEY_HOPS	"hopfudge"
#define KEY_KEEPALIVE	"keepalive"
#define KEY_DEADTIME	"deadtime"
#define KEY_DEADPING	"deadping"
#define KEY_WARNTIME	"warntime"
#define KEY_INITDEAD	"initdead"
#define KEY_WATCHDOG	"watchdog"
#define	KEY_BAUDRATE	"baud"
#define	KEY_UDPPORT	"udpport"
#define	KEY_FACILITY	"logfacility"
#define	KEY_LOGFILE	"logfile"
#define	KEY_DBGFILE	"debugfile"
#define KEY_FAILBACK	"nice_failback"
#define KEY_AUTOFAIL	"auto_failback"
#define KEY_STONITH	"stonith"
#define KEY_STONITHHOST "stonith_host"
#define KEY_CLIENT_CHILD "respawn"
#define KEY_COMPRESSION "compression"
#define KEY_COMPRESSION_THRESHOLD "compression_threshold"
#define KEY_TRADITIONAL_COMPRESSION "traditional_compression"
#define KEY_RT_PRIO	"rtprio"
#define KEY_GEN_METH	"hbgenmethod"
#define KEY_REALTIME	"realtime"
#define KEY_DEBUGLEVEL	"debug"
#define KEY_NORMALPOLL	"normalpoll"
#define KEY_APIPERM	"apiauth"
#define KEY_MSGFMT      "msgfmt"
#define KEY_LOGDAEMON   "use_logd"
#define KEY_CONNINTVAL	"conn_logd_time"
#define KEY_BADPACK	"log_badpack"
#define KEY_REGAPPHBD	"use_apphbd"
#define KEY_COREDUMP	"coredumps"
#define KEY_COREROOTDIR	"coreroot"
#define KEY_REL2	"crm"
#define KEY_AUTOJOIN	"autojoin"
#define KEY_UUIDFROM	"uuidfrom"
#define KEY_ENV		"env"
#define KEY_MAX_REXMIT_DELAY "max_rexmit_delay"

ll_cluster_t*	ll_cluster_new(const char * llctype);
#endif /* __HB_API_H */
