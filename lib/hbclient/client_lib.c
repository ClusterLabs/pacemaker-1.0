/* $Id: client_lib.c,v 1.26 2005/03/30 19:46:50 gshi Exp $ */
/* 
 * client_lib: heartbeat API client side code
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
 */

/*
 *	Here's our approach:
 *
 *	Have each application connect to heartbeat via our IPC layer.
 *		This IPC layer currently uses sockets and provides
 *		a suitable authorization API.
 *
 *	We can validate permissions for "sniffing" using the builtin
 *		IPC authorization API.
 *
 *	This code thankfully no longer uses FIFOs.
 *
 */

#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <heartbeat.h>
#include <hb_api_core.h>
#include <hb_api.h>
#include <clplumbing/ttylock.h>
#include <glib.h>

struct sys_config *		config  = NULL;

int			netstring_format = TRUE;

struct stringlist {
	char *			value;
	struct stringlist *	next;
};


/*
 *	Queue of messages to be read later...
 */
struct MsgQueue {
	struct ha_msg *		value;
	struct MsgQueue *	next;
	struct MsgQueue *	prev;
};

typedef struct gen_callback {
	char *			msgtype;
	llc_msg_callback_t 	cf;
	void *			pd;
	struct gen_callback*	next;
}gen_callback_t;

#define	MXFIFOPATH	128
#define	HBPREFIX	"LCK..hbapi:"

/* Order sequence */
typedef struct order_seq {
	char			to_node[HOSTLENG];
	seqno_t			seqno;
	struct order_seq *	next;
}order_seq_t;

/* Order Queue */
struct orderQ {
	struct ha_msg *		orderQ[MAXMSGHIST];
	int			curr_index;
	seqno_t			curr_oseqno;
	seqno_t			curr_gen;
	seqno_t			curr_client_gen;
	seqno_t			first_msg_seq;
	seqno_t			first_msg_gen;
	seqno_t			first_msg_client_gen;
	struct orderQ		*backupQ;
};

typedef struct order_queue {
	char			from_node[HOSTLENG];
	struct orderQ		node;
	struct orderQ		cluster;
	struct order_queue*	next;
	struct ha_msg*		leave_msg;
        int			client_leaving;
}order_queue_t;

/*
 *	Our heartbeat private data
 */
typedef struct llc_private {
	const char *		PrivateId;	/* A "magic cookie */
	llc_nstatus_callback_t	node_callback;	/* Node status callback fcn */
	void*			node_private;	/* node status callback data*/
	llc_ifstatus_callback_t	if_callback;	/* IF status callback fcn */
	void*			if_private;	/* IF status callback data */
	llc_cstatus_callback_t	cstatus_callback;/*Client status callback fcn */
	void*			client_private;	/* client status callback data*/
	struct gen_callback*	genlist;	/* List of general callbacks*/
	IPC_Channel*		chan;		/* IPC communication channel*/
	struct stringlist *	nodelist;	/* List of nodes from query */
	struct stringlist *	iflist;		/* List of IFs from query */
	int			SignedOn;	/* 1 if we're signed on */
	int			iscasual;	/* 1 if casual client */
	long			deadtime_ms;	/* heartbeat's deadtime */
	long			keepalive_ms;	/* HB's keepalive time*/
	int			logfacility;	/* HB's logging facility */
	struct stringlist*	nextnode;	/* Next node for walknode */
	struct stringlist*	nextif;		/* Next interface for walkif*/
	/* Messages to be read after current call completes */
	struct MsgQueue *	firstQdmsg;
	struct MsgQueue *	lastQdmsg;
	/* The next two items are for ordered message delivery */
	order_seq_t		order_seq_head;	/* head of order_seq list */
	order_queue_t*		order_queue_head;/* head of order queue */
}llc_private_t;

static const char * OurID = "Heartbeat private data";	/* "Magic cookie" */

#define ISOURS(l) (l && l->ll_cluster_private &&			\
		(((llc_private_t*)(l->ll_cluster_private))->PrivateId) == OurID)
#define SEQGAP		16
#define DEBUGORDER	0

static void		ClearLog(void);
			/* Common code for request messages */
static struct ha_msg*	hb_api_boilerplate(const char * apitype);
static int		hb_api_signon(struct ll_cluster*, const char * clientid);
static int		hb_api_signoff(struct ll_cluster*);
static int		hb_api_setfilter(struct ll_cluster*, unsigned);
static void		destroy_stringlist(struct stringlist *);
static struct stringlist*
			new_stringlist(const char *);
static int		get_nodelist(llc_private_t*);
static void		zap_nodelist(llc_private_t*);
static int		get_iflist(llc_private_t*, const char *host);
static void		zap_iflist(llc_private_t*);
static void		zap_order_seq(llc_private_t* pi);
static void		zap_order_queue(llc_private_t* pi);
static int		enqueue_msg(llc_private_t*,struct ha_msg*);
static struct ha_msg*	dequeue_msg(llc_private_t*);
static gen_callback_t*	search_gen_callback(const char * type, llc_private_t*);
static int		add_gen_callback(const char * msgtype
,	llc_private_t*, llc_msg_callback_t, void*);
static int		del_gen_callback(llc_private_t*, const char * msgtype);

static struct ha_msg*	read_api_msg(llc_private_t*);
static struct ha_msg*	read_cstatus_respond_msg(llc_private_t*pi, int timeout);
static struct ha_msg*	read_hb_msg(ll_cluster_t*, int blocking);

static int		hb_api_setsignal(ll_cluster_t*, int nsig);
static int set_msg_callback
			(ll_cluster_t*, const char * msgtype
,			llc_msg_callback_t callback, void * p);
static int
set_nstatus_callback (ll_cluster_t*
,		llc_nstatus_callback_t cbf, 	void * p);
static int
set_cstatus_callback (ll_cluster_t*
,		llc_cstatus_callback_t cbf, void * p);
static int
set_ifstatus_callback (ll_cluster_t* ci
,		llc_ifstatus_callback_t cbf, void * p);
static int init_nodewalk (ll_cluster_t*);
static const char * nextnode (ll_cluster_t* ci);
static int init_ifwalk (ll_cluster_t* ci, const char * host);
static const char *	get_nodestatus(ll_cluster_t*, const char *host);
static const char *
get_clientstatus(ll_cluster_t*, const char *host, const char *clientid
,	int timeout);
static const char *	get_nodetype(ll_cluster_t*, const char *host);
static const char *	get_ifstatus(ll_cluster_t*, const char *host
,	const char * intf);
static char *		get_parameter(ll_cluster_t*, const char* pname);
static const char *	get_resources(ll_cluster_t*);
static int		get_inputfd(ll_cluster_t*);
static IPC_Channel*	get_ipcchan(ll_cluster_t*);
static int		msgready(ll_cluster_t*);
static int		setfmode(ll_cluster_t*, unsigned mode);
static int		sendclustermsg(ll_cluster_t*, struct ha_msg* msg);
static int		sendnodemsg(ll_cluster_t*, struct ha_msg* msg
,			const char * nodename);

STATIC void		add_order_seq(llc_private_t*, struct ha_msg* msg);
static int		send_ordered_clustermsg(ll_cluster_t* lcl, struct ha_msg* msg);
static int		send_ordered_nodemsg(ll_cluster_t* lcl, struct ha_msg* msg
,			const char * nodename);
static const char *	APIError(ll_cluster_t*);
static int		CallbackCall(llc_private_t* p, struct ha_msg * msg);
static struct ha_msg *	read_msg_w_callbacks(ll_cluster_t* llc, int blocking);
static int		rcvmsg(ll_cluster_t* llc, int blocking);

volatile struct process_info *	curproc = NULL;
static char		OurPid[16];
static const char *	OurClientID = NULL;
static char 		OurNode[SYS_NMLN];
static ll_cluster_t*	hb_cluster_new(void);

static void ha_api_perror(const char * fmt, ...) G_GNUC_PRINTF(1,2);
static void ha_api_log(int priority, const char * fmt, ...) G_GNUC_PRINTF(2,3);


#define	ZAPMSG(m)	{ha_msg_del(m); (m) = NULL;}

/*
 * All the boilerplate common to creating heartbeat API request
 * messages.
 */
static struct ha_msg*
hb_api_boilerplate(const char * apitype)
{
	struct ha_msg*	msg;

	if ((msg = ha_msg_new(4)) == NULL) {
		ha_api_log(LOG_ERR, "boilerplate: out of memory");
		return msg;
	}

	/* Message type: API request */
	if (ha_msg_add(msg, F_TYPE, T_APIREQ) != HA_OK) {
		ha_api_log(LOG_ERR, "boilerplate: cannot add F_TYPE field");
		ZAPMSG(msg);
		return msg;
	}
	/* Add field for API request type */
	if (ha_msg_add(msg, F_APIREQ, apitype) != HA_OK) {
		ha_api_log(LOG_ERR, "boilerplate: cannot add F_APIREQ field");
		ZAPMSG(msg);
		return msg;
	}
	/* Add field for destination */
	if (ha_msg_add(msg, F_TO, OurNode) != HA_OK) {
		ha_api_log(LOG_ERR, "boilerplate: cannot add F_TO field");
		ZAPMSG(msg);
		return msg;
	}
	/* Add our PID to the message */
	if (ha_msg_add(msg, F_PID, OurPid) != HA_OK) {
		ha_api_log(LOG_ERR, "boilerplate: cannot add F_PID field");
		ZAPMSG(msg);
		return msg;
	}
	
	/* Add our client ID to the message */
	if (ha_msg_add(msg, F_FROMID, OurClientID) != HA_OK) {
		ha_api_log(LOG_ERR, "boilerplate: cannot add F_FROMID field");
		ZAPMSG(msg);
		return msg;
	}
	return(msg);
}

/*
 * Sign ourselves on as a heartbeat client process.
 */
static int
hb_api_signon(struct ll_cluster* cinfo, const char * clientid)
{
	struct ha_msg*	request;
	struct ha_msg*	reply;
	struct utsname	un;
	int		rc;
	const char *	result;
	int		iscasual;
	llc_private_t* pi;
	const char	*tmpstr;
        char		regpath[] = API_REGSOCK;
	char		path[] = IPC_PATH_ATTR;
	GHashTable*	wchanattrs;
	char		cuid[20];
	char		cgid[20];

	if (!ISOURS(cinfo)) {
		ha_api_log(LOG_ERR, "hb_api_signon: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)cinfo->ll_cluster_private;

	/* Re-sign ourselves back on */
	if (pi->SignedOn) {
		hb_api_signoff(cinfo);
	}

	snprintf(OurPid, sizeof(OurPid), "%d", getpid());

	/* Create our client id */
	if (clientid != NULL) {
		OurClientID = clientid;
		iscasual = 0;
	}else{
		OurClientID = OurPid;
		iscasual = 1;
	}

	pi->iscasual = iscasual;


	if (uname(&un) < 0) {
		ha_api_perror("uname failure");
		return HA_FAIL;
	}
        memset(OurNode, 0, sizeof(OurNode));
	strncpy(OurNode, un.nodename, sizeof(OurNode)-1);
	g_strdown(OurNode);

	/* Initialize order_seq_head */
	pi->order_seq_head.seqno = 1;
	pi->order_seq_head.to_node[0] = '\0';
	pi->order_seq_head.next = NULL;

	/* Initialize order_queue_head */
	pi->order_queue_head = NULL;

	/* Crank out the boilerplate */
	if ((request = hb_api_boilerplate(API_SIGNON)) == NULL) {
		return HA_FAIL;
	}
	snprintf(cuid, sizeof(cuid)-1, "%ld",  (long)geteuid());
	/* Add our UID to the message */
	if (ha_msg_add(request, F_UID, cuid) != HA_OK) {
		ha_api_log(LOG_ERR, "hb_api_signon: cannot add F_UID field");
		ZAPMSG(request);
		return HA_FAIL;
	}
	snprintf(cgid, sizeof(cgid)-1, "%ld",  (long)getegid());
	/* Add our GID to the message */
	if (ha_msg_add(request, F_GID, cgid) != HA_OK) {
		ha_api_log(LOG_ERR, "hb_api_signon: cannot add F_GID field");
		ZAPMSG(request);
		return HA_FAIL;
	}
	wchanattrs = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(wchanattrs, path, regpath);

	/* Connect to the heartbeat API server */

	if ((pi->chan = ipc_channel_constructor(IPC_ANYTYPE, wchanattrs))
	==	NULL) {
		ha_api_log(LOG_ERR, "hb_api_signon: Can't connect"
		" to heartbeat");
		ZAPMSG(request);
		return HA_FAIL;
	}

	pi->chan->should_send_block = TRUE;

        if (pi->chan->ops->initiate_connection(pi->chan) != IPC_OK) {
		ha_api_log(LOG_ERR, "hb_api_signon: Can't initiate"
		" connection  to heartbeat");
		ZAPMSG(request);
                return HA_FAIL;
        }


	/* Send the registration request message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		pi->chan->ops->destroy(pi->chan);
		pi->chan = NULL;
		ha_api_perror("can't send message to IPC");
		ZAPMSG(request);
		return HA_FAIL;
	}
	

	ZAPMSG(request);
	pi->chan->ops->waitout(pi->chan);

	/* Read the reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return HA_FAIL;
	}

	/* Get the return code */
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0) {
		rc = HA_OK;
		pi->SignedOn = 1;

		if ((tmpstr = ha_msg_value(reply, F_DEADTIME)) == NULL
		||	sscanf(tmpstr, "%lx", (unsigned long*)&(pi->deadtime_ms)) != 1) {
			ha_api_log(LOG_ERR
			,	"hb_api_signon: Can't get deadtime ");
			ZAPMSG(reply);
			return HA_FAIL;
		}
		if ((tmpstr = ha_msg_value(reply, F_KEEPALIVE)) == NULL
		||	sscanf(tmpstr, "%lx", (unsigned long*)&(pi->keepalive_ms)) != 1) {
			ha_api_log(LOG_ERR
			,	"hb_api_signon: Can't get keepalive time ");
			ZAPMSG(reply);
			return HA_FAIL;
		}
		if ((tmpstr = ha_msg_value(reply, F_NODENAME)) == NULL
		||	strlen(tmpstr) >= sizeof(OurNode)) {
			ha_api_log(LOG_ERR
			,	"hb_api_signon: Can't get local node name");
			ZAPMSG(reply);
			return HA_FAIL;
		}else{
			strncpy(OurNode, tmpstr, sizeof(OurNode)-1);
			OurNode[sizeof(OurNode)-1] = EOS;
		}
		/* Sometimes they don't use syslog logging... */
		tmpstr = ha_msg_value(reply, F_LOGFACILITY);
		if (tmpstr == NULL
		||	sscanf(tmpstr, "%d", &(pi->logfacility)) != 1) {
			pi->logfacility = -1;
		}
	}else{
		rc = HA_FAIL;
	}
	ZAPMSG(reply);

	return rc;
}

/*
 * Sign off (disconnect) as a heartbeat client process.
 */
static int
hb_api_signoff(struct ll_cluster* cinfo)
{
	struct ha_msg*	request;
	llc_private_t* pi;

	if (!ISOURS(cinfo)) {
		ha_api_log(LOG_ERR, "hb_api_signoff: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)cinfo->ll_cluster_private;

	if (!pi->SignedOn) {
		/* Already signed off... No value in returning an error... */
		return HA_OK;
	}

	if ((request = hb_api_boilerplate(API_SIGNOFF)) == NULL) {
		ha_api_log(LOG_ERR, "hb_api_signoff: can't create msg");
		return HA_FAIL;
	}
	
	/* Send the message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("can't send message to IPC");
		return HA_FAIL;
	}
	pi->chan->ops->waitout(pi->chan);
	ZAPMSG(request);
	OurClientID = NULL;
	pi->chan->ops->destroy(pi->chan);
	pi->SignedOn = 0;
	zap_order_seq(pi);
	zap_order_queue(pi);

	return HA_OK;
}
/*
 * delete:  destroy the heartbeat API object
 */
static int
hb_api_delete(struct ll_cluster* ci)
{
	llc_private_t* pi;
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "hb_api_delete: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;

	/* Sign off */
	hb_api_signoff(ci);

	/* Free up interface and node lists */
	zap_iflist(pi);
	zap_nodelist(pi);

	/* What about our message queue? */

	/* Free up the private information */
	memset(pi, 0, sizeof(*pi));
	ha_free(pi);

	/* Free up the generic (llc) information */
	memset(ci, 0, sizeof(*ci));
	ha_free(ci);
	return HA_OK;
}

/*
 * Set message filter mode.
 */
static int
hb_api_setfilter(struct ll_cluster* ci, unsigned fmask)
{
	struct ha_msg*	request;
	struct ha_msg*	reply;
	int		rc;
	const char *	result;
	char		filtermask[32];
	llc_private_t* pi;

	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "hb_api_setfilter: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if ((request = hb_api_boilerplate(API_SETFILTER)) == NULL) {
		ha_api_log(LOG_ERR, "hb_api_setfilter: can't create msg");
		return HA_FAIL;
	}

	/* Format the filtermask information in hex */
	snprintf(filtermask, sizeof(filtermask), "%x", fmask);
	if (ha_msg_add(request, F_FILTERMASK, filtermask) != HA_OK) {
		ha_api_log(LOG_ERR, "hb_api_setfilter: cannot add field/2");
		ZAPMSG(request);
		return HA_FAIL;
	}
	
	/* Send the message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("can't send message to IPC");
		return HA_FAIL;
	}
	ZAPMSG(request);


	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return HA_FAIL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0) {
		rc = HA_OK;
	}else{
		rc = HA_FAIL;
	}
	ZAPMSG(reply);

	return rc;
}

/*
 * Set signal for message notification.
 * This is not believed to be a security hole :-)
 */
static int
hb_api_setsignal(ll_cluster_t* lcl, int nsig)
{
	struct ha_msg*	request;
	struct ha_msg*	reply;
	int		rc;
	const char *	result;
	char		csignal[32];
	llc_private_t* pi;

	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "hb_api_setsignal: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if ((request = hb_api_boilerplate(API_SETSIGNAL)) == NULL) {
		ha_api_log(LOG_ERR, "hb_api_setsignal: can't create msg");
		return HA_FAIL;
	}

	snprintf(csignal, sizeof(csignal), "%d", nsig);
	if (ha_msg_add(request, F_SIGNAL, csignal) != HA_OK) {
		ha_api_log(LOG_ERR, "hb_api_setsignal: cannot add field/2");
		ZAPMSG(request);
		return HA_FAIL;
	}
	
	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ha_api_perror("can't send message to IPC Channel");
		ZAPMSG(request);
		return HA_FAIL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return HA_FAIL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0) {
		rc = HA_OK;
	}else{
		rc = HA_FAIL;
	}
	ZAPMSG(reply);

	return rc;
}

/*
 * Retrieve the list of nodes in the cluster.
 */
static int
get_nodelist(llc_private_t* pi)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result = NULL;
	struct stringlist*	sl;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if ((request = hb_api_boilerplate(API_NODELIST)) == NULL) {
		ha_api_log(LOG_ERR, "get_nodelist: can't create msg");
		return HA_FAIL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("can't send message to IPC Channel");
		return HA_FAIL;
	}
	ZAPMSG(request);

	/* Loop as long as we get an API_MORE result */
	/* The final node will (hopefully) have an API_OK result type */

	while ((reply=read_api_msg(pi)) != NULL
	&& 	(result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	(strcmp(result, API_MORE) == 0 || strcmp(result, API_OK) == 0)
	&&	(sl = new_stringlist(ha_msg_value(reply, F_NODENAME))) != NULL){

		sl->next = pi->nodelist;
		pi->nodelist = sl;
		if (strcmp(result, API_OK) == 0) {
			pi->nextnode = pi->nodelist;
			ZAPMSG(reply);
			return(HA_OK);
		}

		ZAPMSG(reply);
	}
	if (reply != NULL) {
		zap_nodelist(pi);
		ZAPMSG(reply);
	}
	if (reply == NULL) {
		ha_api_log(LOG_ERR, "General read_api_msg() failure");
	}else if (result == NULL) {
		ha_api_log(LOG_ERR, "API reply missing " F_APIRESULT " field.");
	}else if (strcmp(result, API_MORE) != 0 && strcmp(result, API_OK) != 0) {
		ha_api_log(LOG_ERR, "Unexpected API result value: [%s]", result);
	}else if (ha_msg_value(reply, F_NODENAME) == NULL) {
		ha_api_log(LOG_ERR, "No nodename in API reply");
	}else{
		ha_api_log(LOG_ERR, "new_stringlist() failure.");
	}

	return HA_FAIL;
}
/*
 * Retrieve the list of interfaces for the given host.
 */

static int
get_iflist(llc_private_t* pi, const char *host)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	struct stringlist*	sl;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if ((request = hb_api_boilerplate(API_IFLIST)) == NULL) {
		ha_api_log(LOG_ERR, "get_iflist: can't create msg");
		return HA_FAIL;
	}
	if (ha_msg_add(request, F_NODENAME, host) != HA_OK) {
		ha_api_log(LOG_ERR, "get_iflist: cannot add field");
		ZAPMSG(request);
		return HA_FAIL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return HA_FAIL;
	}
	ZAPMSG(request);

	/* Loop as long as we get an API_MORE result */
	/* The final interface will (hopefully) have an API_OK result type */

	while ((reply=read_api_msg(pi)) != NULL
	&& 	(result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	(strcmp(result, API_MORE) == 0 || strcmp(result, API_OK) == 0)
	&&	(sl = new_stringlist(ha_msg_value(reply, F_IFNAME))) != NULL){

		sl->next = pi->iflist;
		pi->iflist = sl;
		if (strcmp(result, API_OK) == 0) {
			pi->nextif = pi->iflist;
			ZAPMSG(reply);
			return(HA_OK);
		}
		ZAPMSG(reply);
	}
	if (reply != NULL) {
		zap_iflist(pi);
		ZAPMSG(reply);
	}

	return HA_FAIL;
}

/*
 * Return the status of the given node.
 */

static const char *
get_nodestatus(ll_cluster_t* lcl, const char *host)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	const char *		status;
	static char		statbuf[128];
	const char *		ret;
	llc_private_t*		pi;

	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_nodestatus: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return NULL;
	}

	if ((request = hb_api_boilerplate(API_NODESTATUS)) == NULL) {
		return NULL;
	}
	if (ha_msg_add(request, F_NODENAME, host) != HA_OK) {
		ha_api_log(LOG_ERR, "get_nodestatus: cannot add field");
		ZAPMSG(request);
		return NULL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return NULL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0
	&&	(status = ha_msg_value(reply, F_STATUS)) != NULL) {
                memset(statbuf, 0, sizeof(statbuf));
		strncpy(statbuf, status, sizeof(statbuf) - 1);
		ret = statbuf;
	}else{
		ret = NULL;
	}
	ZAPMSG(reply);

	return ret;
}
/*
* Return the status of the given client.
*/

static const char *
get_clientstatus(ll_cluster_t* lcl, const char *host
,		const char *clientid, int timeout)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	llc_private_t*		pi;
	static char		statbuf[128];
	const char *		clientname;
	const char *		ret;
	
	ClearLog();
	if (!ISOURS(lcl)){
		ha_api_log(LOG_ERR,"get_clientstatus: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn){
		ha_api_log(LOG_ERR,"not signed on");
		return NULL;
	}
	clientname = (clientid == NULL) ? OurClientID : clientid;

	/* If host is NULL, user choose the callback method to
	 * get the result. This also implies timeout is useless */
	if (host == NULL) {
		struct ha_msg * m = NULL;

		if ((m = ha_msg_new(0)) == NULL
		||	ha_msg_add(m, F_TYPE, T_QCSTATUS) != HA_OK
		||	ha_msg_add(m, F_CLIENTNAME, clientname) != HA_OK
		||	ha_msg_add(m, F_FROMID, OurClientID) != HA_OK) {
			
			if (m){
				ha_msg_del(m);
			}
			
			ha_log(LOG_ERR, "%s: cannot add field", __FUNCTION__);
			return NULL;
		}
		if (sendclustermsg(lcl, m) != HA_OK) {
			ha_log(LOG_ERR, "%s: sendclustermsg fail",__FUNCTION__);
		}
		ha_msg_del(m);
		return NULL;
	}

	if (*host == EOS) {
		ha_api_log(LOG_ERR, "client status : bad nodename");
		return NULL;
	}
	if ((request = hb_api_boilerplate(API_CLIENTSTATUS)) == NULL) {
		ha_api_log(LOG_ERR, "hb_api_boilerplate failed");
		return NULL;
	}
	if (ha_msg_add(request,	F_NODENAME, host)!= HA_OK
	||	ha_msg_add(request, F_CLIENTNAME, clientname)!= HA_OK) {
		ha_api_log(LOG_ERR, "get_clientstatus: cannot add message field");
		ZAPMSG(request);
		return NULL;
	}
	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply = read_cstatus_respond_msg(pi, timeout)) == NULL) {
		return NULL;
	}

	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0
	&&	(result = ha_msg_value(reply, F_CLIENTSTATUS)) != NULL) {

                memset(statbuf, 0, sizeof(statbuf));
		strncpy(statbuf, result, sizeof(statbuf) - 1);
		ret = statbuf;
	} else {
		ha_api_perror("received wrong type of msg");
		ret = NULL;
	}

	ZAPMSG(reply);
	return ret;
}

/*
 * Return the type of the given node.
 */

static const char *
get_nodetype(ll_cluster_t* lcl, const char *host)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	const char *		status;
	static char		statbuf[128];
	const char *		ret;
	llc_private_t*		pi;

	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_nodetype: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return NULL;
	}

	if ((request = hb_api_boilerplate(API_NODETYPE)) == NULL) {
		return NULL;
	}
	if (ha_msg_add(request, F_NODENAME, host) != HA_OK) {
		ha_api_log(LOG_ERR, "get_nodetype: cannot add field");
		ZAPMSG(request);
		return NULL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return NULL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0
	&&	(status = ha_msg_value(reply, F_NODETYPE)) != NULL) {
                memset(statbuf, 0, sizeof(statbuf));
		strncpy(statbuf, status, sizeof(statbuf) - 1);
		ret = statbuf;
	}else{
		ret = NULL;
	}
	ZAPMSG(reply);

	return ret;
}
static char *	
get_parameter(ll_cluster_t* lcl, const char* pname)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	const char *		pvalue;
	char *			ret;
	llc_private_t*		pi;

	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_parameter: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return NULL;
	}

	if ((request = hb_api_boilerplate(API_GETPARM)) == NULL) {
		return NULL;
	}
	if (ha_msg_add(request, F_PNAME, pname) != HA_OK) {
		ha_api_log(LOG_ERR, "get_parameter: cannot add field");
		ZAPMSG(request);
		return NULL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return NULL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0
	&&	(pvalue = ha_msg_value(reply, F_PVALUE)) != NULL) {
		ret = ha_strdup(pvalue);
	}else{
		ret = NULL;
	}
	ZAPMSG(reply);

	return ret;
}

static const char *	
get_resources(ll_cluster_t* lcl)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	const char *		rvalue;
	char *			ret;
	llc_private_t*		pi;

	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_resources: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return NULL;
	}

	if ((request = hb_api_boilerplate(API_GETRESOURCES)) == NULL) {
		return NULL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return NULL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0
	&&	(rvalue = ha_msg_value(reply, F_RESOURCES)) != NULL) {
		static char		retvalue[64];
		strncpy(retvalue, rvalue, sizeof(retvalue)-1);
		retvalue[DIMOF(retvalue)-1] = EOS;
		ret = retvalue;
	}else{
		ret = NULL;
	}
	ZAPMSG(reply);

	return ret;
}

/*
 * Return heartbeat's keepalive time
 */
static long
get_keepalive(ll_cluster_t* lcl)
{
	llc_private_t* pi;

	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_keepalive: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	return (pi->keepalive_ms);

}

/*
 * Return heartbeat's dead time
 */
static long
get_deadtime(ll_cluster_t* lcl)
{
	llc_private_t* pi;

	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_deadtime: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;

	return (pi->deadtime_ms);
}

/*
 * Return suggested logging facility
 */
static int
get_logfacility(ll_cluster_t* lcl)
{
	llc_private_t* pi;
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_logfacility: bad cinfo");
		return -1;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;
	return (pi->logfacility);
}

/*
 * Return my nodeid.
 */
static const char *
get_mynodeid(ll_cluster_t* lcl)
{
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_mynodeid: bad cinfo");
		return NULL;
	}
	return (OurNode);
}



/*
 * Return the status of the given interface for the given machine.
 */
static const char *
get_ifstatus(ll_cluster_t* lcl, const char *host, const char * ifname)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	const char *		status;
	static char		statbuf[128];
	const char *		ret;
	llc_private_t* pi;

	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "get_ifstatus: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return NULL;
	}

	if ((request = hb_api_boilerplate(API_IFSTATUS)) == NULL) {
		return NULL;
	}
	if (ha_msg_add(request, F_NODENAME, host) != HA_OK) {
		ha_api_log(LOG_ERR, "get_ifstatus: cannot add field");
		ZAPMSG(request);
		return NULL;
	}
	if (ha_msg_add(request, F_IFNAME, ifname) != HA_OK) {
		ha_api_log(LOG_ERR, "get_ifstatus: cannot add field");
		ZAPMSG(request);
		return NULL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return NULL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0
	&&	(status = ha_msg_value(reply,F_STATUS)) != NULL) {
                memset(statbuf, 0, sizeof(statbuf));
		strncpy(statbuf, status, sizeof(statbuf) - 1);
		ret = statbuf;
	}else{
		ret = NULL;
	}
	ZAPMSG(reply);

	return ret;
}
/*
 * Zap our list of nodes
 */
static void
zap_nodelist(llc_private_t* pi)
{
	destroy_stringlist(pi->nodelist);
	pi->nodelist=NULL;
	pi->nextnode = NULL;
}
/*
 * Zap our list of interfaces.
 */
static void
zap_iflist(llc_private_t* pi)
{
	destroy_stringlist(pi->iflist);
	pi->iflist=NULL;
	pi->nextif = NULL;
}

static void
zap_order_seq(llc_private_t* pi)
{
	order_seq_t * order_seq = pi->order_seq_head.next;
	order_seq_t * next;

	while (order_seq != NULL){
		next = order_seq->next;
		ha_free(order_seq);
		order_seq = next;	 
	}
	pi->order_seq_head.next = NULL;
}

static void
zap_order_queue(llc_private_t* pi)
{
	order_queue_t *	oq = pi->order_queue_head;
	order_queue_t *	next;
	int		i;

	while (oq != NULL) {
		next = oq->next;
		for (i = 0; i < MAXMSGHIST; i++){
			if (oq->node.orderQ[i]){
				ZAPMSG(oq->node.orderQ[i]);
				oq->node.orderQ[i] = NULL;
			}
			if (oq->cluster.orderQ[i]){
				ZAPMSG(oq->cluster.orderQ[i]);
				oq->cluster.orderQ[i] = NULL;
			}
		}
		ha_free(oq);
		oq = next;     
	}
	pi->order_queue_head = NULL;
}

/*
 * Create a new stringlist.
 */
static struct stringlist*
new_stringlist(const char *s)
{
	struct stringlist*	ret;
	char *			cp;

	if (s == NULL) {
		return(NULL);
	}

	if ((cp = ha_strdup(s)) == NULL) {
		return(NULL);
	}
	if ((ret = MALLOCT(struct stringlist)) == NULL) {
		ha_free(cp);
		return(NULL);
	}
	ret->next = NULL;
	ret->value = cp;
	return(ret);
}

/*
 * Destroy (free) a stringlist.
 */
static void
destroy_stringlist(struct stringlist * s)
{
	struct stringlist *	this;
	struct stringlist *	next;

	for (this=s; this; this=next) {
		next = this->next;
		ha_free(this->value);
		memset(this, 0, sizeof(*this));
		ha_free(this);
	}
}

/*
 * Enqueue a message to be read later.
 */
static int
enqueue_msg(llc_private_t* pi, struct ha_msg* msg)
{
	struct MsgQueue*	newQelem;
	if (msg == NULL) {
		return(HA_FAIL);
	}
	if ((newQelem = MALLOCT(struct MsgQueue)) == NULL) {
		return(HA_FAIL);
	}
	newQelem->value = msg;
	newQelem->prev = pi->lastQdmsg;
	newQelem->next = NULL;
	if (pi->lastQdmsg != NULL) {
		pi->lastQdmsg->next = newQelem;
	}
	pi->lastQdmsg = newQelem;
	if (pi->firstQdmsg == NULL) {
		pi->firstQdmsg = newQelem;
	}
	return HA_OK;
}

/*
 * Dequeue a message.
 */
static struct ha_msg *
dequeue_msg(llc_private_t* pi)
{
	struct MsgQueue*	qret;
	struct ha_msg*		ret = NULL;
	

	qret = pi->firstQdmsg;

	if (qret != NULL) {
		ret = qret->value;
		pi->firstQdmsg=qret->next;
		if (pi->firstQdmsg) {
			pi->firstQdmsg->prev = NULL;
		}
		memset(qret, 0, sizeof(*qret));
		
		/*
		 * The only two pointers to this element are the first pointer,
		 * and the prev pointer of the next element in the queue.
		 * (or possibly lastQdmsg... See below)
		 */
		ha_free(qret);
	}
	if (pi->firstQdmsg == NULL) {
		 /* Zap lastQdmsg if it pointed at this Q element */
		pi->lastQdmsg=NULL;
	}
	return(ret);
}

/*
 * Search the general callback list for the given message type
 */
static gen_callback_t*
search_gen_callback(const char * type, llc_private_t* lcp)
{
	struct gen_callback*	gcb;

	for (gcb=lcp->genlist; gcb != NULL; gcb=gcb->next) {
		if (strcmp(type, gcb->msgtype) == 0) {
			return(gcb);
		}
	}
	return(NULL);
}
 
/*
 * Add a general callback to the list of general callbacks.
 */
static int
add_gen_callback(const char * msgtype, llc_private_t* lcp
,	llc_msg_callback_t funp, void* pd)
{
	struct gen_callback*	gcb;
	char *			type;

	if ((gcb = search_gen_callback(msgtype, lcp)) == NULL) {
		gcb = MALLOCT(struct gen_callback);
		if (gcb == NULL) {
			return(HA_FAIL);
		}
		type = ha_strdup(msgtype);
		if (type == NULL) {
			ha_free(gcb);
			return(HA_FAIL);
		}
		gcb->msgtype = type;
		gcb->next = lcp->genlist;
		lcp->genlist = gcb;
	}else if (funp == NULL) {
		return(del_gen_callback(lcp, msgtype));
	}
	gcb->cf = funp;
	gcb->pd = pd;
	return(HA_OK);
}

/*
 * Delete a general callback from the list of general callbacks.
 */
static int	
del_gen_callback(llc_private_t* lcp, const char * msgtype)
{
	struct gen_callback*	gcb;
	struct gen_callback*	prev = NULL;

	for (gcb=lcp->genlist; gcb != NULL; gcb=gcb->next) {
		if (strcmp(msgtype, gcb->msgtype) == 0) {
			if (prev) {
				prev->next = gcb->next;
			}else{
				lcp->genlist = gcb->next;
			}
			ha_free(gcb->msgtype);
			gcb->msgtype = NULL;
			free(gcb);
			return(HA_OK);
		}
		prev = gcb;
	}
	return(HA_FAIL);
}
 
/*
 * Read an API message.  All other messages are enqueued to be read later.
 */
static struct ha_msg *
read_api_msg(llc_private_t* pi)
{

	for (;;) {
		struct ha_msg*	msg;
		const char *	type;
		
		pi->chan->ops->waitin(pi->chan);
		if (pi->chan->ch_status  == IPC_DISCONNECT){
			break;
		}
		if ((msg=msgfromIPC(pi->chan, 0)) == NULL) {
			ha_api_perror("read_api_msg: "
				      "Cannot read reply from IPC channel");
			continue;
		}
		if ((type=ha_msg_value(msg, F_TYPE)) != NULL
		&&	strcmp(type, T_APIRESP) == 0) {
			return(msg);
		}
		/* Got an unexpected non-api message */
		/* Queue it up for reading later */
		enqueue_msg(pi, msg);
	}
	/*NOTREACHED*/
	return(NULL);
}

/*
 * Read a client status respond message either from local node or from
 * a remote node. All other messages are enqueued to be read later.
 */
static struct ha_msg *
read_cstatus_respond_msg(llc_private_t* pi, int timeout)
{
	struct ha_msg*	msg;
	const char *	type;
	struct pollfd	pfd;

	pfd.fd = pi->chan->ops->get_recv_select_fd(pi->chan);
	pfd.events = POLLIN;

	while ((pi->chan->ops->is_message_pending(pi->chan)) 
	||	(poll(&pfd, 1, timeout) > 0 && pfd.revents == POLLIN)) {

		while (pi->chan->ops->is_message_pending(pi->chan)) {
			if ((msg=msgfromIPC(pi->chan, 0)) == NULL) {
				ha_api_perror("read_api_msg: "
				"Cannot read reply from IPC channel");
				continue;
			}
			if (((type=ha_msg_value(msg, F_TYPE)) != NULL
			&&	strcmp(type, T_RCSTATUS) == 0)
			||	((type=ha_msg_value(msg, F_SUBTYPE)) != NULL
			&&	strcmp(type, T_RCSTATUS) == 0)) {
				return(msg);
			}
			/* Got an unexpected non-api message */
			/* Queue it up for reading later */
			enqueue_msg(pi, msg);
		}
	}

	/* Timeout or caught a signal */
	return NULL;
}

/* This is the place to handle out of order messages from a restarted 
 * client. If we receive messages from a restarted client yet no leave
 * message has been received for the previous client, we need to 
 * save the restarted client's messages in backup queue. When the leave
 * message is received, we then call moveup_backupQ()  so that the backup
 * queue is promoted to our current queue, not backup any more.
 */

static void
moveup_backupQ(struct orderQ* q)
{
	int	i;

	if (q == NULL){
		return;
	}
	
	if (q->backupQ){
		struct orderQ* backup_q = q->backupQ;
		
		memcpy(q, backup_q, sizeof(struct orderQ));
		
		if (backup_q->backupQ != NULL){
			cl_log(LOG_ERR, "moveup_backupQ:"
			       "backupQ in backupQ is not NULL");  
		}
		ha_free(backup_q);
		q->backupQ = NULL;
	}else {
		/*the queue must be empty*/
		for (i = 0; i < MAXMSGHIST; i++) {
			if (q->orderQ[i]){
				cl_log(LOG_ERR, "moveup_backupQ:"
				       "queue is not empty"
				       " possible memory leak");
				cl_log_message(LOG_ERR, q->orderQ[i]);

			}
		}
		
		q->curr_oseqno = 0;
		
	}

	return ;
}

/*
 * Pop up orderQ.
 */
static struct ha_msg *
pop_orderQ(struct orderQ * q)
{
	struct ha_msg *	msg;

	if (q->orderQ[q->curr_index]){
		msg = q->orderQ[q->curr_index];
		q->orderQ[q->curr_index] = NULL;
		q->curr_index = (q->curr_index + 1) % MAXMSGHIST;
		q->curr_oseqno++;
		return msg;
	}
	
	return NULL;
}


static int
msg_oseq_compare(seqno_t oseq1, seqno_t gen1,  
		 seqno_t oseq2,  seqno_t gen2)
{
	int ret;

	if ( gen1 > gen2){
		ret = 1;
	} else if (gen1 < gen2){
		ret = -1;
	} else {
		
		if (oseq1 > oseq2){
			ret = 1;
		} else if (oseq1 < oseq2){
			ret = -1;
		} else{
			ret =  0;
		}		
	}
	
	return ret;	

}


static void
reset_orderQ(struct orderQ* q)
{
	int i;

	for (i =0 ;i < MAXMSGHIST; i++){
		if (q->orderQ[i]){
			ha_msg_del(q->orderQ[i]);
			q->orderQ[i] = 0;
		}
	}
	
	if (q->backupQ != NULL){
		reset_orderQ(q->backupQ);
		ha_free(q->backupQ);
		q->backupQ = NULL;
	}
	
	memset(q, 0, sizeof(struct orderQ));

	return;
}
/*
 *	Process ordered message
 */

static void
display_orderQ(struct orderQ* q){
	if(!q){
		return;
	}

	cl_log(LOG_INFO, "curr_index=%x,  curr_oseqno=%lx, "
	       "curr_gen=%lx, curr_client_gen=%lx",
	       q->curr_index, q->curr_oseqno,
	       q->curr_gen, q->curr_client_gen);
	
	cl_log(LOG_INFO, "first_msg_seq =%lx, first_msg_gen = %lx,"
	       "first_msg_client_gen =%lx",
	       q->first_msg_seq, q->first_msg_gen, 
	       q->first_msg_client_gen);
	if (q->backupQ == NULL){
		cl_log(LOG_INFO, "q->backupQ is NULL");
	}else{
		display_orderQ(q->backupQ);
	}
	
}

static struct ha_msg *
process_ordered_msg(struct orderQ* q, struct ha_msg* msg,
		    seqno_t gen, seqno_t cligen,
		    seqno_t seq, seqno_t oseq,
		    int popmsg)
{
	int		i;	
	
	/*    	display_orderQ(q);  */
	
	/*if this is the first packet, pop it*/
	if ( q->first_msg_seq ==  0){
		q->first_msg_seq = seq;
		q->first_msg_client_gen = cligen;
		q->first_msg_gen = gen;
		q->curr_gen = gen;
		q->curr_client_gen = cligen;
		q->curr_oseqno = oseq -1 ;
		
		goto out;
	}
		
	/*any message with lower sequence than q->first_msg_seq
	  will be dropped*/
	if (q->first_msg_seq != 0 && msg_oseq_compare(q->first_msg_seq,
						      q->first_msg_gen,
						      seq, gen) > 0 ) {
		return NULL;
	}
	


	if ( q->curr_oseqno == 0){
		q->curr_gen = gen;
		q->curr_client_gen = cligen;
		goto out;
	}

	if ( gen > q->curr_gen ){
		/*heartbeat restart, clean everything up*/		
		
		reset_orderQ(q);
		
		q->first_msg_seq = seq;
		q->first_msg_client_gen = cligen;
		q->first_msg_gen = gen;
		q->curr_gen = gen;
		q->curr_client_gen = cligen;
		q->curr_oseqno = oseq - 1;
		
		goto out;
		
	} else if (gen < q->curr_gen){
		/*
		 * message from previous heartbeat generation, 
		 * drop the message
		 */
		return NULL;

	} else if(cligen > q->curr_client_gen ){
		/*client restarted*/
		
		if (q->backupQ == NULL){
			if ( (q->backupQ = ha_malloc(sizeof(struct orderQ))) 
			     ==NULL  ){
				
				cl_log(LOG_ERR, "process_ordered_msg: "
				       "allocating memory for backupQ failed");
				return NULL;			
			}
			memset(q->backupQ, 0, sizeof(struct orderQ));
		}
		
		process_ordered_msg(q->backupQ, msg, gen, cligen, seq, oseq, 0);
		
		return NULL;

	} else if (cligen < q->curr_client_gen){
		/*Message from a previous client*/
		/*this should never happend*/
		
		cl_log(LOG_ERR, "process_ordered_msg: Received message"
		       " from previous client. This should never happen");
		cl_log_message(LOG_ERR, msg);
		return NULL;
	}else if (oseq - q->curr_oseqno >= MAXMSGHIST){
		/*
		 * receives a very big sequence number, the
		 * message is not reliable at this point
		 */
		if (DEBUGORDER) {
			cl_log(LOG_DEBUG
			       ,	"lost at least one unretrievable "
			       "packet! [%lx:%lx], force reset"
			       ,	q->curr_oseqno
			       ,	oseq);
		}
		q->curr_oseqno = oseq - 1;
		
		for (i = 0; i < MAXMSGHIST; i++) {
			/* Clear order queue, msg obsoleted */
			if (q->orderQ[i]){
				ha_msg_del(q->orderQ[i]);
				q->orderQ[i] = NULL;
			}
		}
		q->curr_index = 0;


	}
	

 out:
	/* Put the new received packet in queue */
	q->orderQ[(q->curr_index + oseq - q->curr_oseqno -1 ) % MAXMSGHIST] = msg;
	
	/* if this is the packet we are expecting, pop it*/
	if (popmsg && msg_oseq_compare(q->curr_oseqno + 1, 
				       q->curr_gen,oseq, gen) == 0){
		return pop_orderQ(q);
	}
	return NULL;
	
}

static struct ha_msg* 
process_client_status_msg(llc_private_t* pi, struct ha_msg* msg,
			   const char* from_node)
{
	const char*	status = ha_msg_value(msg, F_STATUS);
	order_queue_t *	oq;
	struct ha_msg*	retmsg;
	
	if (status && (strcmp(status, LEAVESTATUS) == 0 
		       || strcmp(status, JOINSTATUS) == 0) ){
		for (oq = pi->order_queue_head; oq != NULL; oq = oq->next){
			if (strcmp(oq->from_node, from_node) == 0){
				break;
			}
		}
		if (oq == NULL){
			/*no ordered queue found, good, 
			 *simply return the message
			 */
			return msg;
		}

		if (strcmp(status, LEAVESTATUS) == 0 ){
			
			if (oq->leave_msg != NULL){
				cl_log(LOG_ERR, "process_client_status_msg: "
				       " the previous leave msg "
				       "is not delivered yet");
				cl_log_message(LOG_ERR, oq->leave_msg);
				cl_log_message(LOG_ERR, msg);
				return NULL;
			}
			
			oq->leave_msg = msg;
			
			if ((retmsg = pop_orderQ(&oq->node))){				
				return retmsg;
			}
			if ((retmsg = pop_orderQ(&oq->cluster))){
				return retmsg;
			}
			
			oq->leave_msg = NULL;
			moveup_backupQ(&oq->node);
			moveup_backupQ(&oq->cluster);
			return msg;
		}else { /*join message*/
			return msg;
			
		}					
	}else{
		cl_log(LOG_ERR, "process_client_status_msg: "
		       "no status found in client status msg");
		cl_log_message(LOG_ERR, msg);
		return NULL;
	}		
	
	return msg;

}

/*
 *	Process msg gotten from FIFO or msgQ.
 */
static struct ha_msg *
process_hb_msg(llc_private_t* pi, struct ha_msg* msg)
{
	const char *	from_node;
	const char *	to_node;
	order_queue_t * oq;
	const char *	coseq;
	seqno_t		oseq;
	const char *	cgen;
	seqno_t		gen;	
	const char*	cseq;
	seqno_t		seq;
	const char*	ccligen;
	seqno_t		cligen;
	
	
	if ((cseq = ha_msg_value(msg, F_SEQ)) == NULL
	    ||	sscanf(cseq, "%lx", &seq) != 1){
		return msg;
	}
	
	
	if ((cgen = ha_msg_value(msg, F_HBGENERATION)) == NULL
	    ||	sscanf(cgen, "%lx", &gen) != 1){
		return msg;
	}
	
	
	if ((ccligen = ha_msg_value(msg, F_CLIENT_GENERATION)) == NULL
	    ||	sscanf(ccligen, "%lx", &cligen) != 1){
		return msg;
	}
	
	if ((from_node = ha_msg_value(msg, F_ORIG)) == NULL){
		ha_api_log(LOG_ERR
			   ,	"%s: extract F_ORIG failed", __FUNCTION__);
		ZAPMSG(msg);
		return NULL;
	}	
	
	if ((coseq = ha_msg_value(msg, F_ORDERSEQ)) != NULL
	&&	sscanf(coseq, "%lx", &oseq) == 1){
		/* find the order queue by from_node */

		for (oq = pi->order_queue_head; oq != NULL; oq = oq->next){
			if (strcmp(oq->from_node, from_node) == 0)
				break;
		}
		if (oq == NULL){
			oq = (order_queue_t *) 
				ha_malloc(sizeof(order_queue_t));
			if (oq == NULL){
				ha_api_log(LOG_ERR
				,	"%s: order_queue_t malloc failed"
				,	__FUNCTION__);
				ZAPMSG(msg);
				return NULL;
			}
			memset(oq, 0, sizeof(*oq));
			strncpy(oq->from_node, from_node, HOSTLENG);
			

			oq->next = pi->order_queue_head;
			pi->order_queue_head = oq;
		}
		if ((to_node = ha_msg_value(msg, F_TO)) == NULL)
			return process_ordered_msg(&oq->cluster, msg, gen,
						   cligen, seq, oseq, 1);
		else
			return process_ordered_msg(&oq->node, msg, gen, 
						   cligen, seq, oseq, 1);
	}else {
		const char* type = ha_msg_value(msg, F_TYPE);
		
		if ( type && strcmp(type, T_APICLISTAT) == 0){
			return process_client_status_msg(pi, msg, from_node);
		}
		
		/* Simply return no order required msg */
		return msg;
	}
}
/*
 * Read a heartbeat message.  Read from the queue first.
 */
static struct ha_msg *
read_hb_msg(ll_cluster_t* llc, int blocking)
{
	llc_private_t*	pi;
	struct ha_msg*	msg;
	struct ha_msg*	retmsg;
	order_queue_t*	oq;

	if (!ISOURS(llc)) {
		ha_api_log(LOG_ERR, "read_hb_msg: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)llc->ll_cluster_private;

	if (!pi->SignedOn) {
		return NULL;
	}

	/* Process msg from msgQ */
	while ((msg = dequeue_msg(pi))){
		if ((retmsg = process_hb_msg(pi, msg)))
			return retmsg;
	}
	for (oq = pi->order_queue_head; oq != NULL; oq = oq->next){

	process_oq:
		if ((retmsg = pop_orderQ(&oq->node))){
			return retmsg;
		}
		if ((retmsg = pop_orderQ(&oq->cluster))){
			return retmsg;
		}
		
		if (oq->leave_msg != NULL){
			retmsg = oq->leave_msg;
			oq->leave_msg = NULL;
			oq->client_leaving = 1;
			return retmsg;
		}
		if (oq->client_leaving){
			moveup_backupQ(&oq->node);
			moveup_backupQ(&oq->cluster);			
			oq->client_leaving = 0;
			goto process_oq;
		}		
	}
	/* Process msg from FIFO */
	while (msgready(llc)){
		msg = msgfromIPC(pi->chan, 0);
		if (msg == NULL) {
			if (pi->chan->ch_status != IPC_CONNECT) {
				pi->SignedOn = FALSE;
				return NULL;
			}
		}else if ((retmsg = process_hb_msg(pi, msg))) {
			return retmsg;
		}
	}
	/* Process msg from orderQ */

	if (!blocking)
		return NULL;

	/* If this is a blocking call, we keep on reading from FIFO, so
         * that we can finally return a non-NULL msg to user.
         */
	for(;;) {
		pi->chan->ops->waitin(pi->chan);
		msg = msgfromIPC(pi->chan, 0);
		if (msg == NULL) {
			if (pi->chan->ch_status != IPC_CONNECT) {
				pi->SignedOn = FALSE;
			}
			return NULL;
		}
		if ((retmsg = process_hb_msg(pi, msg))) {
			return retmsg;
		}
	}
}

/*
 * Add a callback for the given message type.
 */
static int
set_msg_callback(ll_cluster_t* ci, const char * msgtype
,			llc_msg_callback_t callback, void * p)
{

	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "set_msg_callback: bad cinfo");
		return HA_FAIL;
	}
	return(add_gen_callback(msgtype,
	(llc_private_t*)ci->ll_cluster_private, callback, p));
}

/*
 * Set the node status change callback.
 */
static int
set_nstatus_callback (ll_cluster_t* ci
,		llc_nstatus_callback_t cbf, 	void * p)
{
	llc_private_t*	pi = ci->ll_cluster_private;
	pi->node_callback = cbf;
	pi->node_private = p;
	return(HA_OK);
}

/*
 * Set the interface status change callback.
 */
static int
set_ifstatus_callback (ll_cluster_t* ci
,		llc_ifstatus_callback_t cbf, void * p)
{
	llc_private_t*	pi = ci->ll_cluster_private;
	pi->if_callback = cbf;
	pi->if_private = p;
	return(HA_OK);
}

/*
 * Set the client status change callback.
 */
static int
set_cstatus_callback (ll_cluster_t* ci
,		llc_cstatus_callback_t cbf, void * p)
{
	llc_private_t*	pi = ci->ll_cluster_private;

	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "%s: bad cinfo", __FUNCTION__);
		return HA_FAIL;
	}
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	pi->cstatus_callback = cbf;
	pi->client_private = p;

	return HA_OK;
}

/*
 * Call the callback associated with this message (if any)
 * Return TRUE if a callback was called.
 */
static int
CallbackCall(llc_private_t* p, struct ha_msg * msg)
{
	const char *	mtype=  ha_msg_value(msg, F_TYPE);
	struct gen_callback*	gcb;

	if (mtype == NULL) {
		return(0);
	}
	
	/* Special case: node status (change) */

	if ((strcasecmp(mtype, T_STATUS) == 0
	||	strcasecmp(mtype, T_NS_STATUS) == 0)) {
		/* If DEADSTATUS, cleanup order queue for the node */
		if (strcmp(ha_msg_value(msg, F_STATUS), DEADSTATUS) == 0) {
			order_queue_t *	oq = p->order_queue_head;
			order_queue_t *	prev;
			order_queue_t *	next;
			int		i;

			for (prev = NULL; oq != NULL; prev = oq, oq = oq->next){
				if (strcmp(oq->from_node
				,	ha_msg_value(msg, F_ORIG)) == 0)
					break;
			}
			if (oq){
				next = oq->next;
				for (i = 0; i < MAXMSGHIST; i++){
					if (oq->node.orderQ[i])
						ZAPMSG(oq->node.orderQ[i]);
					if (oq->cluster.orderQ[i])
						ZAPMSG(oq->cluster.orderQ[i]);
				}
				ha_free(oq);
				if (prev)
					prev->next = next;
				else
					p->order_queue_head = next;
			}
		}
		if (p->node_callback) {
			p->node_callback(ha_msg_value(msg, F_ORIG)
			,	ha_msg_value(msg, F_STATUS), p->node_private);
			return(1);
		}
	}

	/* Special case: interface status (change) */

	if (p->if_callback && strcasecmp(mtype, T_IFSTATUS) == 0) {
		p->if_callback(ha_msg_value(msg, F_NODE)
		,	ha_msg_value(msg, F_IFNAME)
		,	ha_msg_value(msg, F_STATUS)
		,	p->if_private);
		return(1);
	}

	/* Special case: client status (change) */

	if (p->cstatus_callback && strcasecmp(mtype, T_APICLISTAT) == 0) {
		p->cstatus_callback(ha_msg_value(msg, F_ORIG)
		,	ha_msg_value(msg, F_FROMID)
		,       ha_msg_value(msg, F_STATUS)
		,       p->client_private);
		return(1);
	}
	if (p->cstatus_callback && strcasecmp(mtype, T_RCSTATUS) == 0) {
		p->cstatus_callback(ha_msg_value(msg, F_ORIG)
		,	ha_msg_value(msg, F_CLIENTNAME)
		,       ha_msg_value(msg, F_CLIENTSTATUS)
		,       p->client_private);
		return(1);
	}

	/* The general case: Any other message type */

	if ((gcb = search_gen_callback(mtype, p)) != NULL) {
		gcb->cf(msg, gcb->pd);
		return 1;
	}
	return(0);
}
/*
 * Return the next message not handled by a callback.
 * Invoke callbacks for messages encountered along the way.
 */
static struct ha_msg *
read_msg_w_callbacks(ll_cluster_t* llc, int blocking)
{
	struct ha_msg*	msg = NULL;
	llc_private_t* pi;

	if (!ISOURS(llc)) {
		ha_api_log(LOG_ERR, "read_msg_w_callbacks: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*) llc->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "read_msg_w_callbacks: Not signed on");
		return NULL;
	}
	do {
		if (msg) {
			ZAPMSG(msg);
		}
		msg = read_hb_msg(llc, blocking);

	}while (msg && CallbackCall(pi, msg));
	return(msg);
}
/*
 * Receive messages.  Activate callbacks.  Messages without callbacks
 * are ignored.  Potentially several messages could be acted on.
 */
static int
rcvmsg(ll_cluster_t* llc, int blocking)
{
	struct ha_msg*	msg = NULL;
	
	msg=read_msg_w_callbacks(llc, blocking);

	if (msg) {
		ZAPMSG(msg);
		return(1);
	}
	return(0);
}

/*
 * Initialize nodewalk. (mainly retrieve list of nodes)
 */
static int
init_nodewalk (ll_cluster_t* ci)
{
	llc_private_t*	pi;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "init_nodewalk: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	zap_nodelist(pi);

	return(get_nodelist(pi));
}

/*
 * Return the next node in the list, or NULL if none.
 */
static const char *
nextnode (ll_cluster_t* ci)
{
	llc_private_t*	pi;
	const char *	ret;

	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "nextnode: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return NULL;
	}
	if (pi->nextnode == NULL) {
		return(NULL);
	}
	ret = pi->nextnode->value;

	pi->nextnode = pi->nextnode->next;
	return(ret);
}
/*
 * Clean up after a nodewalk (throw away node list)
 */
static int
end_nodewalk(ll_cluster_t* ci)
{
	llc_private_t*	pi;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "end_nodewalk: bad cinfo");
		return HA_FAIL;
	}
	pi = ci->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	zap_nodelist(pi);
	return(HA_OK);
}

/*
 * Initialize interface walk. (mainly retrieve list of interfaces)
 */
static int
init_ifwalk (ll_cluster_t* ci, const char * host)
{
	llc_private_t*	pi;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "init_ifwalk: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	zap_iflist(pi);
	return(get_iflist(pi, host));
}

/*
 * Return the next interface in the iflist, or NULL if none.
 */
static const char *
nextif (ll_cluster_t* ci)
{
	llc_private_t*	pi = ci->ll_cluster_private;
	const char *	ret;

	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "nextif: bad cinfo");
		return HA_FAIL;
	}
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	if (pi->nextif == NULL) {
		return(NULL);
	}
	ret = pi->nextif->value;

	pi->nextif = pi->nextif->next;
	return(ret);
}

/*
 * Clean up after a ifwalk (throw away interface list)
 */
static int
end_ifwalk(ll_cluster_t* ci)
{
	llc_private_t*	pi = ci->ll_cluster_private;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "end_ifwalk: bad cinfo");
		return HA_FAIL;
	}
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	zap_iflist(pi);
	return HA_OK;
}

/*
 * Return the input file descriptor associated with this object.
 */
static int
get_inputfd(ll_cluster_t* ci)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "get_inputfd: bad cinfo");
		return(-1);
	}
	pi = (llc_private_t*)ci->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return -1;
	}
	return pi->chan->ops->get_recv_select_fd(pi->chan);
}
/*
 * Return the IPC channel associated with this object.
 */
static IPC_Channel*
get_ipcchan(ll_cluster_t* ci)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "get_ipcchan: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return NULL;
	}
	return pi->chan;
}


/*
 * Return TRUE (1) if there is a message ready to read.
 */
static int

msgready(ll_cluster_t*ci )
{
	llc_private_t* pi;

	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "msgready: bad cinfo");
		return 0;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return 0;
	}
	if (pi->firstQdmsg) {
		return 1;
	}

	return pi->chan->ops->is_message_pending(pi->chan);
}

/*
 * Set message filter mode
 */
static int
setfmode(ll_cluster_t* lcl, unsigned mode)
{
	unsigned	filtermask;
	llc_private_t* pi;

	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "setfmode: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	switch(mode) {

		case LLC_FILTER_DEFAULT:
			filtermask = DEFAULTREATMENT;
			break;
		case LLC_FILTER_PMODE:
			filtermask = (KEEPIT|DUPLICATE|DROPIT);
			break;
		case LLC_FILTER_ALLHB:
			filtermask = (KEEPIT|DUPLICATE|DROPIT|NOCHANGE);
			break;
		case LLC_FILTER_RAW:
			filtermask = ALLTREATMENTS;
			break;
		default:
			return(HA_FAIL);
	}
	return(hb_api_setfilter(lcl, filtermask));
	
}

/*
 * Send a message to the cluster.
 */

static int
sendclustermsg(ll_cluster_t* lcl, struct ha_msg* msg)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "sendclustermsg: bad cinfo");
		return HA_FAIL;
	}

	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if (pi->iscasual) {
		ha_api_log(LOG_ERR, "sendclustermsg: casual client");
		return HA_FAIL;
	}

	return(msg2ipcchan(msg, pi->chan));
}

/*
 * Send a message to a specific node in the cluster.
 */
static int
sendnodemsg(ll_cluster_t* lcl, struct ha_msg* msg
,			const char * nodename)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "sendnodemsg: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	if (pi->iscasual) {
		ha_api_log(LOG_ERR, "sendnodemsg: casual client");
		return HA_FAIL;
	}
	if (*nodename == EOS) {
		ha_api_log(LOG_ERR, "sendnodemsg: bad nodename");
		return HA_FAIL;
	}
	if (ha_msg_mod(msg, F_TO, nodename) != HA_OK) {
		ha_api_log(LOG_ERR, "sendnodemsg: cannot set F_TO field");
		return(HA_FAIL);
	}
	return(msg2ipcchan(msg, pi->chan));
}

static int
sendnodemsg_byuuid(ll_cluster_t* lcl, struct ha_msg* msg,
		   uuid_t uuid)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "sendnodemsg_byuuid: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	if (pi->iscasual) {
		ha_api_log(LOG_ERR, "sendnodemsg_byuuid: casual client");
		return HA_FAIL;
	}
	if (!uuid){
		ha_api_log(LOG_ERR, "uuid is NULL");
		return HA_FAIL;
	}
	if (cl_msg_moduuid(msg, F_TOUUID, uuid) != HA_OK) {
		ha_api_log(LOG_ERR, "sendnodemsg_byuuid: "
			   "cannot set F_TOUUID field");
		return(HA_FAIL);
	}
	return(msg2ipcchan(msg, pi->chan));	
}


static int
get_uuid(llc_private_t* pi, const char* nodename, uuid_t uuid)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	uuid_t			tmp;
	
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if ((request = hb_api_boilerplate(API_GETUUID)) == NULL) {
		ha_api_log(LOG_ERR, "get_uuid: can't create msg");
		return HA_FAIL;
	}
	if (ha_msg_add(request, F_QUERYNAME, nodename) != HA_OK) {
		ha_api_log(LOG_ERR, "get_uuid: cannot add field");
		ZAPMSG(request);
		return HA_FAIL;
	}

	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return HA_FAIL;
	}
	ZAPMSG(request);
	
	if ((reply=read_api_msg(pi)) != NULL
	    && 	(result = ha_msg_value(reply, F_APIRESULT)) != NULL
	    &&	(strcmp(result, API_OK) == 0)
	    &&	(cl_get_uuid(reply, F_QUERYUUID, tmp)) == HA_OK){
		
		uuid_copy(uuid, tmp);
		ZAPMSG(reply);
		
		return HA_OK;
		
	}
	
	if (reply != NULL) {
		ZAPMSG(reply);
	}
	
	return HA_FAIL;
}

static int
get_uuid_by_name(ll_cluster_t* ci, const char* nodename, uuid_t uuid)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "get_nodeID_from_name: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}	
	
	if(!uuid || !nodename){
		ha_api_log(LOG_ERR, "get_uuid_by_name: uuid or nodename is NULL");
		return HA_FAIL;
	}
	return get_uuid(pi, nodename, uuid);
}



static int
get_name(llc_private_t* pi, const uuid_t uuid, char* name, int maxnamlen)
{
	struct ha_msg*		request;
	struct ha_msg*		reply;
	const char *		result;
	const char *		tmp;
	
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	
	if ((request = hb_api_boilerplate(API_GETNAME)) == NULL) {
		ha_api_log(LOG_ERR, "get_name: can't create msg");
		return HA_FAIL;
	}
	if (ha_msg_adduuid(request, F_QUERYUUID, uuid) != HA_OK) {
		ha_api_log(LOG_ERR, "get_uuid: cannot add field");
		ZAPMSG(request);
		return HA_FAIL;
	}
	
	/* Send message */
	if (msg2ipcchan(request, pi->chan) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to IPC Channel");
		return HA_FAIL;
	}
	ZAPMSG(request);
	
	if ((reply=read_api_msg(pi)) != NULL
	    && 	(result = ha_msg_value(reply, F_APIRESULT)) != NULL
	    &&	(strcmp(result, API_OK) == 0)
	    &&	(tmp =  ha_msg_value(reply, F_QUERYNAME)) != NULL){
		
		strncpy(name, tmp, maxnamlen -1 );
		name[maxnamlen-1] = 0;
		ZAPMSG(reply);
		
		return HA_OK;		
	}
	
	if (reply != NULL) {
		ZAPMSG(reply);
	}
	
	return HA_FAIL;
}


static int
get_name_by_uuid(ll_cluster_t* ci, uuid_t uuid, 
		 char* nodename, size_t  maxnamlen){
	
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(ci)) {
		ha_api_log(LOG_ERR, "get_nodeID_from_name: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)ci->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}	
	
	if(!uuid || !nodename || maxnamlen <= 0){
		ha_api_log(LOG_ERR, "get_name_by_uuid: bad paramter");
		return HA_FAIL;
	}
	return get_name(pi, uuid, nodename, maxnamlen);
}


/* Add order sequence number field */
STATIC  void
add_order_seq(llc_private_t* pi, struct ha_msg* msg)
{
        order_seq_t *	order_seq = &pi->order_seq_head;
        const char *	to_node;
        char		seq[32];

	to_node = ha_msg_value(msg, F_TO);
	if (to_node != NULL){
		for (order_seq = pi->order_seq_head.next; order_seq != NULL
		;	order_seq = order_seq->next){
			if (strcmp(order_seq->to_node, to_node) == 0)
				break;
		}
	}
	if (order_seq == NULL && to_node != NULL){
		order_seq = (order_seq_t *) ha_malloc(sizeof(order_seq_t));
		if (order_seq == NULL){
			ha_api_log(LOG_ERR
			,	"add_order_seq: order_seq_t malloc failed!");
			return;
            	}
		strncpy(order_seq->to_node, to_node, HOSTLENG);
		order_seq->seqno = 1;
		order_seq->next = pi->order_seq_head.next;
		pi->order_seq_head.next = order_seq;
        }
        sprintf(seq, "%lx", order_seq->seqno);
        order_seq->seqno++;
        ha_msg_mod(msg, F_ORDERSEQ, seq);
}

/*
 * Send an ordered message to the cluster.
 */

static int
send_ordered_clustermsg(ll_cluster_t* lcl, struct ha_msg* msg)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "%s: bad cinfo", __FUNCTION__);
		return HA_FAIL;
	}

	pi = (llc_private_t*)lcl->ll_cluster_private;

	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if (pi->iscasual) {
		ha_api_log(LOG_ERR, "%s: casual client", __FUNCTION__);
		return HA_FAIL;
	}

	add_order_seq(pi, msg);

	return(msg2ipcchan(msg, pi->chan));
}

static int
send_ordered_nodemsg(ll_cluster_t* lcl, struct ha_msg* msg
,			const char * nodename)
{
	llc_private_t* pi;
	ClearLog();
	if (!ISOURS(lcl)) {
		ha_api_log(LOG_ERR, "sendnodemsg: bad cinfo");
		return HA_FAIL;
	}
	pi = (llc_private_t*)lcl->ll_cluster_private;
	if (!pi->SignedOn) {
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}
	if (pi->iscasual) {
		ha_api_log(LOG_ERR, "sendnodemsg: casual client");
		return HA_FAIL;
	}
	if (*nodename == EOS) {
		ha_api_log(LOG_ERR, "sendnodemsg: bad nodename");
		return HA_FAIL;
	}
	if (ha_msg_mod(msg, F_TO, nodename) != HA_OK) {
		ha_api_log(LOG_ERR, "sendnodemsg: cannot set F_TO field");
		return(HA_FAIL);
	}
	add_order_seq(pi, msg);
	return(msg2ipcchan(msg, pi->chan));
}

static char	APILogBuf[MAXLINE] = "";
size_t		BufLen = 0;

static void
ClearLog(void)
{
        memset(APILogBuf, 0, sizeof(APILogBuf));
	APILogBuf[0] = EOS;
	BufLen = 1;
}

static gboolean
chan_is_connected(ll_cluster_t* lcl)
{
	llc_private_t* pi;
	if (lcl == NULL){
		cl_log(LOG_ERR, "Invalid argument, "
		       "lcl is NULL");
		return FALSE;		
	}
	
	if(lcl->ll_cluster_private == NULL){
		cl_log(LOG_ERR, "Invalid argument, "
		       "lcl->llc_cluster_private is NULL");
		return FALSE;
	}
	
	pi  = (llc_private_t*) lcl->ll_cluster_private;
	
	if (pi->chan == NULL){
		cl_log(LOG_ERR, "Invalid argument: chan is NULL");
		return FALSE;
	}
	
	return (pi->chan->ch_status == IPC_CONNECT);	
}


static const char *
APIError(ll_cluster_t* lcl)
{
	return(APILogBuf);
}

static void
ha_api_log(int priority, const char * fmt, ...)
{
	size_t	len;
        va_list ap;
        char buf[MAXLINE];
 
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
	len = strlen(buf);

	if ((BufLen + len) >= sizeof(APILogBuf)) {
		ClearLog();
	}
		
	if (APILogBuf[0] != EOS && APILogBuf[BufLen-1] != '\n') {
		strncat(APILogBuf, "\n", sizeof(APILogBuf)-BufLen-1);
		BufLen++;
	}

	strncat(APILogBuf, buf, sizeof(APILogBuf)-BufLen-1);
	BufLen += len;
}


static void
ha_api_perror(const char * fmt, ...)
{
	const char *	err;

	va_list ap;
	char buf[MAXLINE];

	err = strerror(errno);
	va_start(ap, fmt);
	vsnprintf(buf, MAXLINE, fmt, ap);
	va_end(ap);

	ha_api_log(LOG_ERR, "%s: %s", buf, err);

}

/*
 *	Our vector of member functions...
 */
static struct llc_ops heartbeat_ops = {
	signon:			hb_api_signon,		
	signoff:		hb_api_signoff,		
	delete:			hb_api_delete,		
	set_msg_callback:	set_msg_callback,	
	set_nstatus_callback:	set_nstatus_callback,	
	set_ifstatus_callback:	set_ifstatus_callback,	
	set_cstatus_callback:	set_cstatus_callback,	
	init_nodewalk:		init_nodewalk,		
	nextnode:		nextnode,		
	end_nodewalk:		end_nodewalk,		
	node_status:		get_nodestatus,		
	node_type:		get_nodetype,		
	init_ifwalk:		init_ifwalk,		
	nextif:			nextif,			
	end_ifwalk:		end_ifwalk,		
	if_status:		get_ifstatus,		
	client_status:		get_clientstatus,	
	get_uuid_by_name:	get_uuid_by_name,
	get_name_by_uuid:	get_name_by_uuid,
	sendclustermsg:		sendclustermsg,		
	sendnodemsg:		sendnodemsg,		
	sendnodemsg_byuuid:	sendnodemsg_byuuid,	
	send_ordered_clustermsg:send_ordered_clustermsg,
	send_ordered_nodemsg:	send_ordered_nodemsg,	
	inputfd:		get_inputfd,		
	ipcchan:		get_ipcchan,		
	msgready:		msgready,		
	setmsgsignal:		hb_api_setsignal,	
	rcvmsg:			rcvmsg,			
	readmsg:		read_msg_w_callbacks,	
	setfmode:		setfmode,		
	get_parameter:		get_parameter,		
	get_deadtime:		get_deadtime,		
	get_keepalive:		get_keepalive,		
	get_mynodeid:		get_mynodeid,		
	get_logfacility:	get_logfacility,	
	get_resources:		get_resources,		
	chan_is_connected:	chan_is_connected,
	errmsg:			APIError,		
};



/*
 * Create a new heartbeat API object
 */
static ll_cluster_t*
hb_cluster_new()
{
	ll_cluster_t*	ret;
	struct llc_private* hb;

	if ((hb = MALLOCT(struct llc_private)) == NULL) {
		return(NULL);
	}
	memset(hb, 0, sizeof(*hb));
	if ((ret = MALLOCT(ll_cluster_t)) == NULL) {
		ha_free(hb);
		hb = NULL;
		return(NULL);
	}
	memset(ret, 0, sizeof(*ret));

	hb->PrivateId = OurID;
	ret->ll_cluster_private = hb;
	ret->llc_ops = &heartbeat_ops;
	

	return ret;
}

/*
 * Create a new low-level cluster object of the specified type.
 */
ll_cluster_t*
ll_cluster_new(const char * llctype)
{
	if (strcmp(llctype, "heartbeat") == 0) {
		return hb_cluster_new();
	}
	return NULL;
}
