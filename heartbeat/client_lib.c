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
 *	Have each application have it's own FIFO for
 *		sending requests to heartbeat, and one for getting responses.
 *
 *	Set the permissions on this fifo directory to require
 *		root permissions to create/delete.  The apps don't create
 *		these FIFOs, and heartbeat doesn't remove them.
 *
 *	Create a casual subdirectory which is sticky and writable
 *		by whoever you want.  The apps create/delete these FIFOs,
 *		and heartbeat can clean them up too (like it does now).
 *
 *	Whenever it reads a message it checks the permissions of the
 *		FIFO it came from, and validates the request accordingly.
 *
 *	You can validate the pid from comparing the owner of the FIFO
 *		with the uid of the owner of /proc/pid.  They should
 *		match.
 *
 *	You can validate "root" permissions for "sniffing" in a similar way
 *		On the other hand, sniffing these is no worse than sniffing
 *		the ethernet, which you already assume might happen...
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
#include <lock.h>
#include <glib.h>

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

/*
 *	Our heartbeat private data
 */
typedef struct llc_private {
	const char *		PrivateId;	/* A "magic cookie */
	llc_nstatus_callback_t	node_callback;	/* Node status callback fcn */
	void*			node_private;	/* node status callback data */
	llc_ifstatus_callback_t	if_callback;	/* IF status callback fcn */
	void*			if_private;	/* IF status callback data */
	struct gen_callback*	genlist;	/* List of general callbacks */
	char			ReqFIFOName[MXFIFOPATH];/* Request FIFO name */
	FILE*			RequestFIFO;	/* Request FIFO (write-only) */
	char			ReplyFIFOName[MXFIFOPATH];/* Reply FIFO name */
	FILE*			ReplyFIFO;	/* Reply FIFO (read-only) */
	struct stringlist *	nodelist;	/* List of nodes from query */
	struct stringlist *	iflist;		/* List of IFs from query */
	struct MsgQueue *	firstQdmsg;	/* Message Queue */
	struct MsgQueue *	lastQdmsg;	/* End of msg Queue */
	int			SignedOn;	/* 1 if we're signed on */
	int			iscasual;	/* 1 if casual client */
	long			deadtime_ms;	/* heartbeat's deadtime */
	long			keepalive_ms;	/* heartbeat's keepalive time*/
	int			logfacility;	/* heartbeat's logging facility */
	struct stringlist*	nextnode;	/* Next node for walknode */
	struct stringlist*	nextif;		/* Next interface for walkif */
}llc_private_t;

static const char * OurID = "Heartbeat private data";	/* "Magic cookie" */

#define ISOURS(l) (l && l->ll_cluster_private &&			\
		(((llc_private_t*)(l->ll_cluster_private))->PrivateId) == OurID)

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
static int		enqueue_msg(llc_private_t*,struct ha_msg*);
static struct ha_msg*	dequeue_msg(llc_private_t*);
static gen_callback_t*	search_gen_callback(const char * type, llc_private_t*);
static int		add_gen_callback(const char * msgtype
,	llc_private_t*, llc_msg_callback_t, void*);
static int		del_gen_callback(llc_private_t*, const char * msgtype);

static struct ha_msg*	read_api_msg(llc_private_t*);
static struct ha_msg*	read_hb_msg(ll_cluster_t*, int blocking);

static int		hb_api_setsignal(ll_cluster_t*, int nsig);
static int set_msg_callback
			(ll_cluster_t*, const char * msgtype
,			llc_msg_callback_t callback, void * p);
static int
set_nstatus_callback (ll_cluster_t*
,		llc_nstatus_callback_t cbf, 	void * p);
static int
		set_ifstatus_callback (ll_cluster_t* ci
,		llc_ifstatus_callback_t cbf, void * p);
static int init_nodewalk (ll_cluster_t*);
static const char * nextnode (ll_cluster_t* ci);
static int init_ifwalk (ll_cluster_t* ci, const char * host);
static const char *	get_nodestatus(ll_cluster_t*, const char *host);
static const char *	get_nodetype(ll_cluster_t*, const char *host);
static const char *	get_ifstatus(ll_cluster_t*, const char *host
,	const char * intf);
static char *		get_parameter(ll_cluster_t*, const char* pname);
static const char *	get_resources(ll_cluster_t*);
static int		get_inputfd(ll_cluster_t*);
static int		msgready(ll_cluster_t*);
static int		setfmode(ll_cluster_t*, int mode);
static int		sendclustermsg(ll_cluster_t*, struct ha_msg* msg);
static int		sendnodemsg(ll_cluster_t*, struct ha_msg* msg
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

	(void)_heartbeat_h_Id;
	(void)_ha_msg_h_Id;

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
	int		fd, Regfd;
	struct utsname	un;
	int		rc;
	const char *	result;
	const char *	directory;
	int		iscasual;
	FILE*		RegFIFO;
	struct stat	sbuf;
	llc_private_t* pi;
	const char	*tmpstr;

	/*
	 * A little explanation about our FIFOs...
	 *
	 * There is a Registration FIFO, a Request FIFO and a Reply FIFO
	 * We write the Registration FIFO only once, to register ourselves
	 * as a client.  As a result, it's a local variable (RegFIFO), in this
	 * routine.
	 * 
	 * Whenever we make a request of heartbeat, we write it to the request
	 * FIFO (ReqFIFO), and then heartbeat replies to us on our Reply FIFO,
	 * (ReplyFIFO) which we then read to get the reply from heartbeat.
	 *
	 * So, the usage of FIFOs by the client library is as follows:
	 *
	 * RegFIFO	write-only (only used once)
	 * ReqFIFO	write-only (used once per request)
	 * ReplyFIFO	read-only (used per request or other msg received)
	 */

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

	if (!iscasual && DoLock(HBPREFIX, OurClientID) != 0) {
		ha_api_perror("Cannot lock FIFO for %s", OurClientID);
	}
	pi->iscasual = iscasual;
	directory = (iscasual ? CASUALCLIENTDIR : NAMEDCLIENTDIR);

	snprintf(pi->ReplyFIFOName, sizeof(pi->ReplyFIFOName)
	,	"%s/%s%s", directory, OurClientID, RSP_SUFFIX);

	snprintf(pi->ReqFIFOName, sizeof(pi->ReqFIFOName)
	,	"%s/%s%s", directory, OurClientID, REQ_SUFFIX);

	/*
	 * We need to provide locking for named clients to ensure that only
	 * one client is accessing the request/response FIFOs simultaneously.
	 *	(for now, it's a wee bug ;-))
	 */

	if (uname(&un) < 0) {
		ha_api_perror("uname failure");
		return HA_FAIL;
	}
        memset(OurNode, 0, sizeof(OurNode));
	strncpy(OurNode, un.nodename, sizeof(OurNode) -1 );

	/* Crank out the boilerplate */
	if ((request = hb_api_boilerplate(API_SIGNON)) == NULL) {
		return HA_FAIL;
	}

	/* Make sure the registration FIFO exists */
	if (stat(API_REGFIFO, &sbuf) < 0 || !S_ISFIFO(sbuf.st_mode)) {
		ha_api_log(LOG_ERR, "FIFO %s does not exist", API_REGFIFO);
		ZAPMSG(request);
		return HA_FAIL;
	}
	
	if (iscasual) {
		/* Make our reply FIFO */
		if (mkfifo(pi->ReplyFIFOName, 0600) < 0) {
			ha_api_perror("hb_api_signon: Can't create fifo %s"
			,	pi->ReplyFIFOName);
			ZAPMSG(request);
			return HA_FAIL;
		}

		/* Make our request FIFO */
		if (mkfifo(pi->ReqFIFOName, 0600) < 0) {
			ha_api_perror("hb_api_signon: Can't create fifo %s"
			,	pi->ReqFIFOName);
			ZAPMSG(request);
			return HA_FAIL;
		}
	}
	/* We open it this way to keep the open from hanging...
	 * We really only need to read it (see the fdopen below), but if
	 * we open it only for reading then we'll get an EPIPE until it
	 * is opened by heartbeat (or something like that).  In any case,
	 * we only need to read it, but we have to open it this way for
	 * things to work right.
	 */
	if ((fd = open(pi->ReplyFIFOName, O_RDWR)) < 0) {
		ha_api_log(LOG_ERR, "hb_api_signon: Can't open reply fifo %s"
		,	pi->ReplyFIFOName);
		ZAPMSG(request);
		return HA_FAIL;
	}
	/*
	 * Although we opened it for r/w as explained above, there's no need
	 * to tell stdio that, because we're only going to read it.
	 * So, we do an open(2), then an fdopen of the fd gotten from open().
	 */
	if ((pi->ReplyFIFO = fdopen(fd, "r")) == NULL) {
		ha_api_log(LOG_ERR, "hb_api_signon: Can't fdopen reply fifo %s"
		,	pi->ReplyFIFOName);
		ZAPMSG(request);
		return HA_FAIL;
	}

	/*
	 * We cannot allow the ReplyFIFO to be buffered or we can get
	 * get very confused if it has more than one message in it.
	 * In particular, we have no way to tell if stdio has read in
	 * a message that we'd like to report to the client via
	 * msgready().  So, we do this to make msgready() accurate
	 * (which is hardly an optional property).
	 */
	setvbuf(pi->ReplyFIFO, NULL, _IONBF, 0);

	/*  Open up the registration FIFO. We will open it in nonblocking
	 *  mode, to verify if heartbeat has already opened the other end.
	 *  If we do not use nonblocking open, we will end up getting 
	 *  blocked for ever till heartbeat opens the other end of the fifo.
	 */
	if((Regfd = open(API_REGFIFO, O_WRONLY|O_NONBLOCK)) == -1) {
		ha_api_log(LOG_ERR, "hb_api_signon: Can't open register fifo "
			API_REGFIFO);
		ZAPMSG(request);
		return HA_FAIL;
	}

	if ((RegFIFO = fdopen(Regfd, "w")) == NULL) {
		ha_api_perror("Can't fopen " API_REGFIFO);
		ZAPMSG(request);
		return HA_FAIL;
	}

	/* Send the registration request message */
	if (msg2stream(request, RegFIFO) != HA_OK) {
		fclose(RegFIFO); RegFIFO=NULL;
		ha_api_perror("can't send message to %s", API_REGFIFO);
		ZAPMSG(request);
		return HA_FAIL;
	}
	fclose(RegFIFO); RegFIFO=NULL;
		
	ZAPMSG(request);

	/* Read the reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		return HA_FAIL;
	}

	/* Get the return code */
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0) {
		rc = HA_OK;
		pi->SignedOn = 1;

		/*
		 * Heartbeat has now opened our request FIFO (for reading)
		 * So it's now safe for us to open our end of it for writing.
		 */
		if ((pi->RequestFIFO = fopen(pi->ReqFIFOName, "w")) == NULL) {
			ha_api_log(LOG_ERR, "hb_api_signon: Can't open req fifo %s"
			,	pi->ReqFIFOName);
			ZAPMSG(reply);
			return HA_FAIL;
		}
		if ((tmpstr = ha_msg_value(reply, F_DEADTIME)) == NULL
		||	sscanf(tmpstr, "%lx", &(pi->deadtime_ms)) != 1) {
			ha_api_log(LOG_ERR
			,	"hb_api_signon: Can't get deadtime ");
			ZAPMSG(reply);
			return HA_FAIL;
		}
		if ((tmpstr = ha_msg_value(reply, F_KEEPALIVE)) == NULL
		||	sscanf(tmpstr, "%lx", &(pi->keepalive_ms)) != 1) {
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
		ha_api_log(LOG_ERR, "not signed on");
		return HA_FAIL;
	}

	if ((request = hb_api_boilerplate(API_SIGNOFF)) == NULL) {
		ha_api_log(LOG_ERR, "hb_api_signoff: can't create msg");
		return HA_FAIL;
	}
	
	/* Send the message */
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("can't send message to RequestFIFO");
		return HA_FAIL;
	}
	if (!pi->iscasual && DoUnlock(HBPREFIX, OurClientID) != 0) {
		ha_api_log(LOG_ERR, "Cannot unlock FIFO for %s", OurClientID);
	}
	ZAPMSG(request);
	OurClientID = NULL;
	(void)fclose(pi->RequestFIFO);	pi->RequestFIFO = NULL;
	(void)fclose(pi->ReplyFIFO);	pi->ReplyFIFO = NULL;
	if (pi->iscasual) {
		(void)unlink(pi->ReplyFIFOName);
		(void)unlink(pi->ReqFIFOName);
	}
	pi->ReplyFIFOName[0] = EOS;
	pi->ReqFIFOName[0] = EOS;
	pi->SignedOn = 0;

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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("can't send message to RequestFIFO");
		return HA_FAIL;
	}
	ZAPMSG(request);


	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		ZAPMSG(request);
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ha_api_perror("can't send message to RequestFIFO");
		ZAPMSG(request);
		return HA_FAIL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		ZAPMSG(request);
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
	const char *		result;
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("can't send message to RequestFIFO");
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to RequestFIFO");
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to RequestFIFO");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		ZAPMSG(request);
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to RequestFIFO");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		ZAPMSG(request);
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to RequestFIFO");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		ZAPMSG(request);
		return NULL;
	}
	if ((result = ha_msg_value(reply, F_APIRESULT)) != NULL
	&&	strcmp(result, API_OK) == 0
	&&	(pvalue = ha_msg_value(reply, F_PVALUE)) != NULL) {
		ret = strdup(pvalue);
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to RequestFIFO");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		ZAPMSG(request);
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
	if (msg2stream(request, pi->RequestFIFO) != HA_OK) {
		ZAPMSG(request);
		ha_api_perror("Can't send message to RequestFIFO");
		return NULL;
	}
	ZAPMSG(request);

	/* Read reply... */
	if ((reply=read_api_msg(pi)) == NULL) {
		ZAPMSG(request);
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

	if ((cp = (char *)ha_malloc(strlen(s)+1)) == NULL) {
		return(NULL);
	}
	if ((ret = MALLOCT(struct stringlist)) == NULL) {
		ha_free(cp);
		return(NULL);
	}
	ret->next = NULL;
	ret->value = cp;
	strcpy(cp, s);
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
		type = ha_malloc(strlen(msgtype)+1);
		if (type == NULL) {
			ha_free(gcb);
			return(HA_FAIL);
		}
		strcpy(type, msgtype);
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
		if ((msg=msgfromstream(pi->ReplyFIFO)) == NULL) {
			ha_api_perror("read_api_msg: "
			"Cannot read reply from ReplyFIFO");
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
 * Read a heartbeat message.  Read from the queue first.
 */
static struct ha_msg *
read_hb_msg(ll_cluster_t* llc, int blocking)
{
	struct ha_msg*	msg;
	llc_private_t* pi;

	if (!ISOURS(llc)) {
		ha_api_log(LOG_ERR, "read_hb_msg: bad cinfo");
		return NULL;
	}
	pi = (llc_private_t*)llc->ll_cluster_private;

	if (!pi->SignedOn) {
		return NULL;
	}

	msg = dequeue_msg(pi);

	if (msg != NULL) {
		return(msg);
	}
	if (!blocking && !msgready(llc)) {
		return(NULL);
	}
	msg = msgfromstream(pi->ReplyFIFO);
	return msg;
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

	if (p->node_callback
	&&	(strcasecmp(mtype, T_STATUS) == 0
	||		strcasecmp(mtype, T_NS_STATUS) == 0)) {

		p->node_callback(ha_msg_value(msg, F_ORIG)
		,	ha_msg_value(msg, F_STATUS), p->node_private);
		return(1);
	}

	/* Special case: interface status (change) */

	if (p->if_callback && strcasecmp(mtype, T_IFSTATUS) == 0) {
		p->if_callback(ha_msg_value(msg, F_NODE)
		,	ha_msg_value(msg, F_IFNAME)
		,	ha_msg_value(msg, F_STATUS)
		,	p->if_private);
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
 * Return the file descriptor associated with this object.
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
	return(fileno(pi->ReplyFIFO));
}

/*
 * Return TRUE (1) if there is a message ready to read.
 */
static int
msgready(ll_cluster_t*ci )
{
	fd_set		fds;
	int             fd = get_inputfd(ci);
	struct timeval	tv;
	int		rc;
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
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	rc = select(fd+1, &fds, NULL, NULL, &tv);
	
	return (rc > 0);
}

/*
 * Set message filter mode
 */
static int
setfmode(ll_cluster_t* lcl, int mode)
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

	return(msg2stream(msg, pi->RequestFIFO));
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
	return(msg2stream(msg, pi->RequestFIFO));
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

extern int	sys_nerr;
static void
ha_api_perror(const char * fmt, ...)
{
	const char *	err;
	char	errornumber[16];

	va_list ap;
	char buf[MAXLINE];

	if (errno < 0 || errno >= sys_nerr) {
		snprintf(errornumber, sizeof(errornumber), "error %d\n", errno);
		err = errornumber;
	}else{
#ifdef HAVE_STRERROR
		err = strerror(errno);
#else
		err = sys_errlist[errno];
#endif
	}
	va_start(ap, fmt);
	vsnprintf(buf, MAXLINE, fmt, ap);
	va_end(ap);

	ha_api_log(LOG_ERR, "%s: %s", buf, err);

}

/*
 *	Our vector of member functions...
 */
static struct llc_ops heartbeat_ops = {
	hb_api_signon,		/* signon */
	hb_api_signoff,		/* signon */
	hb_api_delete,		/* delete */
	set_msg_callback,	/* set_msg_callback */
	set_nstatus_callback,	/* set_nstatus_callback */
	set_ifstatus_callback,	/* set_ifstatus_callback */
	init_nodewalk,		/* init_nodewalk */
	nextnode,		/* nextnode */
	end_nodewalk,		/* end_nodewalk */
	get_nodestatus,		/* node_status */
	get_nodetype,		/* node_type */
	init_ifwalk,		/* init_ifwalk */
	nextif,			/* nextif */
	end_ifwalk,		/* end_ifwalk */
	get_ifstatus,		/* if_status */
	sendclustermsg,		/* sendclustermsg */
	sendnodemsg,		/* sendnodemsg */
	get_inputfd,		/* inputfd */
	msgready,		/* msgready */
	hb_api_setsignal,	/* setmsgsignal */
	rcvmsg,			/* rcvmsg */
	read_msg_w_callbacks,	/* readmsg */
	setfmode,		/* setfmode */
	get_parameter,		/* get_parameter */
	get_deadtime,		/* get_deadtime */
	get_keepalive,		/* get_keepalive */
	get_mynodeid,		/* get_mynodeid */
	get_logfacility,	/* suggested logging facility */
	get_resources,		/* Get current resource allocation */
	APIError,		/* errormsg */
};


void *
ha_malloc(size_t size)
{
	return(malloc(size));
}

void *
ha_calloc(size_t size, size_t nrep)
{
	return(calloc(size, nrep));
}

void
ha_free(void * ptr)
{
	free(ptr);
}

int
ha_is_allocated(const void *p)
{
	return TRUE;
}

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
