/*
 * hb_api: Server-side heartbeat API code
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 * Copyright (C) 2000 Marcelo Tosatti <marcelo@conectiva.com.br>
 *
 * Thanks to Conectiva S.A. for sponsoring Marcelo Tosatti work
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
 *	A little about the API FIFO structure...
 *
 *	We have two kinds of API clients:  casual and named
 *
 *	Casual clients just attach and listen in to messages, and ask
 *	the status of things. Casual clients are typically used as status
 *	agents, or debugging agents.
 *
 *	They can't send messages, and they are known only by their PID.
 *	Anyone in the group that owns the casual FIFO directory can use
 *	the casual API.  Casual clients create and delete their own
 *	FIFOs for the API (or are cleaned up after by heartbeat ;-))
 *	Hence, the casual client FIFO directory must be group writable,
 *	and sticky.
 *
 *	Named clients attach and listen in to messages, and they are also
 *	allowed to send messages to the other clients in the cluster with
 *	the same name. Named clients typically provide persistent services
 *	in the cluster.  A cluster manager would be an example
 *	of such a persistent service.
 *
 *	Their FIFOs are pre-created for them, and they neither create nor
 *	delete them - nor should they be able to.
 *	The named client FIFO directory must not be writable by group or other.
 *
 *	We deliver messages from named clients to clients in the cluster
 *	which are registered with the same name.  Each named client
 *	also receives the messages it sends.  I could allow them to send
 *	to any other service that they want, but right now that's overridden.
 *	We mark each packet with the service name that the packet came from.
 *
 *	A client can only register for a given name if their userid is the
 *	owner of the named FIFO for that name.
 *
 *	If a client has permissions to snoop on packets (debug mode),
 *	then they are allowed to receive all packets, but otherwise only
 *	clients registered with the same name will receive these messages.
 *
 *	It is important to make sure that each named client FIFO is owned by the
 *	same UID on each machine.
 */

#include <portability.h>
#include <sys/time.h>
#define	time FOOtime
#include <glib.h>
#undef time
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <heartbeat.h>
#include <ha_msg.h>
#include <hb_api.h>
#include <hb_api_core.h>
#include <hb_config.h>
#include <hb_resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <clplumbing/cl_poll.h>
#include <clplumbing/cl_signal.h>

/* Definitions of API query handlers */
static int api_ping_iflist(const struct ha_msg* msg, struct node_info * node
,	struct ha_msg* resp
,	client_proc_t* client, const char** failreason);


static int api_signoff (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char **failreason);

static int api_setfilter (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char **failreason);

static int api_setsignal (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_nodelist (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_nodestatus (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_nodetype (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_ifstatus (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_iflist (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_get_parameter (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_get_resources (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

gboolean ProcessAnAPIRequest(client_proc_t* client);

struct api_query_handler query_handler_list [] = {
	{ API_SIGNOFF, api_signoff },
	{ API_SETFILTER, api_setfilter },
	{ API_SETSIGNAL, api_setsignal },
	{ API_NODELIST, api_nodelist },
	{ API_NODESTATUS, api_nodestatus },
	{ API_NODETYPE, api_nodetype },
	{ API_IFSTATUS, api_ifstatus },
	{ API_IFLIST, api_iflist },
	{ API_GETPARM, api_get_parameter},
	{ API_GETRESOURCES, api_get_resources},
};

extern int	UseOurOwnPoll;
int		debug_client_count = 0;
int		total_client_count = 0;
client_proc_t*	client_list = NULL;	/* List of all our API clients */
			/* TRUE when any client output still pending */
struct node_info *curnode;

static void	api_process_request(client_proc_t* client, struct ha_msg *msg);
static void	api_send_client_msg(client_proc_t* client, struct ha_msg *msg);
static void	api_send_client_status(client_proc_t* client
,	const char * status, const char *	reason);
static void	api_remove_client_int(client_proc_t* client, const char * rsn);
static int	api_add_client(client_proc_t* chan, struct ha_msg* msg);
static client_proc_t*	find_client(const char * fromid, const char * pid);
static void	G_remove_client(gpointer Client);
static gboolean	APIclients_input_dispatch(IPC_Channel* chan, gpointer udata);
static void	api_process_registration_msg(client_proc_t*, struct ha_msg *);
static gboolean	api_check_client_authorization(client_proc_t* client);

extern GHashTable*	APIAuthorization;


/*
 *	One client pointer per input FIFO.  It's indexed by file descriptor, so
 *	it's not densely populated.  We use this in conjunction with select(2)
 */

/*
 * The original structure of this code was due to
 * Marcelo Tosatti <marcelo@conectiva.com.br>
 *
 * It has been significantly and repeatedly mangled into nearly unrecognizable
 * oblivion by Alan Robertson <alanr@unix.sh>
 *
 */

/*
 *	Monitor messages.  Pass them along to interested clients (if any)
 */
void
api_heartbeat_monitor(struct ha_msg *msg, int msgtype, const char *iface)
{
	const char*	clientid;
	client_proc_t*	client;
	client_proc_t*	nextclient;

	(void)_heartbeat_h_Id;
	(void)_ha_msg_h_Id;
	(void)_hb_config_h_Id;
	(void)_heartbeat_private_h_Id;


	/* This kicks out most messages, since debug clients are rare */

	if ((msgtype&DEBUGTREATMENTS) != 0 && debug_client_count <= 0) {
		return;
	}

	/* Verify that we understand what kind of message we've got here */

	if ((msgtype & ALLTREATMENTS) != msgtype || msgtype == 0) {
		ha_log(LOG_ERR, "heartbeat_monitor: unknown msgtype [%d]"
		,	msgtype);
		return;
	}

	/* See who this message is addressed to (if anyone) */

	clientid = ha_msg_value(msg, F_TOID);

	for (client=client_list; client != NULL; client=nextclient) {
		/*
		 * "client" might be removed by api_send_client_msg()
		 * so, we'd better fetch the next client now!
		 */
		nextclient=client->next;
	
		/* Is this message addressed to us? */
		if (clientid != NULL
		&&	strcmp(clientid, client->client_id) != 0) {
			continue;
		}
		if (client->chan->ch_status != IPC_CONNECT) {
			continue;
		}

		/* Is this one of the types of messages we're interested in? */

		if ((msgtype & client->desired_types) != 0) {
			api_send_client_msg(client, msg);
			if (client->removereason && !client->isindispatch) {
				api_remove_client_pid(client->pid
				,	client->removereason);
			}
		}

		/* If this is addressed to us, then no one else should get it */
		if (clientid != NULL) {
			break;	/* No one else should get it */
		}
	}
}
/*
 *	Periodically clean up after dead clients...
 *	In case we somehow miss them...
 */
gboolean
api_audit_clients(gpointer p)
{
	client_proc_t*	client;
	client_proc_t*	nextclient;

	for (client=client_list; client != NULL; client=nextclient) {
		nextclient=client->next;

		if (CL_KILL(client->pid, 0) < 0 && errno == ESRCH) {
			ha_log(LOG_INFO, "api_audit_clients: client %ld died"
			,	(long) client->pid);
			client->removereason = NULL;
			api_remove_client_pid(client->pid, "died-audit");
			client=NULL;
		}
	}
	return TRUE;
}


/**********************************************************************
 * API_SETFILTER: Set the types of messages we want to see
 **********************************************************************/
static int
api_setfilter(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char **failreason)
{                                                            
	const char *	cfmask;
	unsigned	mask;
/*
 *	Record the types of messages desired by this client
 *		(desired_types)
 */
	if ((cfmask = ha_msg_value(msg, F_FILTERMASK)) == NULL
	||	(sscanf(cfmask, "%x", &mask) != 1)
	||	(mask&ALLTREATMENTS) == 0) {
		*failreason = "EINVAL";
		return I_API_BADREQ;
	}

	if ((client->desired_types  & DEBUGTREATMENTS)== 0
	&&	(mask&DEBUGTREATMENTS) != 0) {

		/* Only allowed to root and to our uid */
		if (client->uid != 0 && client->uid != getuid()) {
			*failreason = "EPERM";
			return I_API_BADREQ;
		}
		++debug_client_count;
	}else if ((client->desired_types & DEBUGTREATMENTS) != 0
	&&	(mask & DEBUGTREATMENTS) == 0) {
		--debug_client_count;
	}
	client->desired_types = mask;
	return I_API_RET;
}

/**********************************************************************
 * API_SIGNOFF: Sign off as a client
 **********************************************************************/

static int
api_signoff(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char **failreason) 
{ 
		/* We send them no reply */
		if (ANYDEBUG) {
			ha_log(LOG_DEBUG, "Signing client %ld off"
			,	(long) client->pid);
		}
		client->removereason = API_SIGNOFF;
		return I_API_IGN;
}

/**********************************************************************
 * API_SETSIGNAL: Record the type of signal they want us to send.
 **********************************************************************/

static int
api_setsignal(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
		const char *	csignal;
		unsigned	oursig;

		if ((csignal = ha_msg_value(msg, F_SIGNAL)) == NULL
		||	(sscanf(csignal, "%u", &oursig) != 1)) {
			return I_API_BADREQ;
		}
		/* Validate the signal number in the message ... */
		if (oursig < 0 || oursig == SIGKILL || oursig == SIGSTOP
		||	oursig >= 32) {
			/* These can't be caught (or is a bad signal). */
			*failreason = "EINVAL";
			return I_API_BADREQ;
		}

		client->signal = oursig;
		return I_API_RET;
}

/***********************************************************************
 * API_NODELIST: List the nodes in the cluster
 **********************************************************************/

static int
api_nodelist(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
		int	j;
		int	last = config->nodecount-1;

		for (j=0; j <= last; ++j) {
			if (ha_msg_mod(resp, F_NODENAME
			,	config->nodes[j].nodename) != HA_OK) {
				ha_log(LOG_ERR
				,	"api_nodelist: "
				"cannot mod field/5");
				return I_API_IGN;
			}
			if (ha_msg_mod(resp, F_APIRESULT
			,	(j == last ? API_OK : API_MORE))
			!=	HA_OK) {
				ha_log(LOG_ERR
				,	"api_nodelist: "
				"cannot mod field/6");
				return I_API_IGN;
			}
			api_send_client_msg(client, resp);
		}
		return I_API_IGN;
}

/**********************************************************************
 * API_NODESTATUS: Return the status of the given node
 *********************************************************************/

static int
api_nodestatus(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
		const char *		cnode;
		struct node_info *	node;

		if ((cnode = ha_msg_value(msg, F_NODENAME)) == NULL
		|| (node = lookup_node(cnode)) == NULL) {
			*failreason = "EINVAL";
			return I_API_BADREQ;
		}
		if (ha_msg_add(resp, F_STATUS, node->status) != HA_OK) {
			ha_log(LOG_ERR
			,	"api_nodestatus: cannot add field");
			return I_API_IGN;
		}
		return I_API_RET;
}

/**********************************************************************
 * API_NODESTATUS: Return the status of the given node
 *********************************************************************/

static int
api_nodetype(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
	const char *		cnode;
	struct node_info *	node;
	const char *		ntype;

	if ((cnode = ha_msg_value(msg, F_NODENAME)) == NULL
	|| (node = lookup_node(cnode)) == NULL) {
		*failreason = "EINVAL";
		return I_API_BADREQ;
	}
	switch (node->nodetype) {
		case PINGNODE_I:	ntype = PINGNODE;
					break;
		case NORMALNODE_I:	ntype = NORMALNODE;
					break;
		default:		ntype = UNKNOWNNODE;
					break;
	}
			
	if (ha_msg_add(resp, F_NODETYPE, ntype) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_nodetype: cannot add field");
		return I_API_IGN;
	}
	return I_API_RET;
}

/**********************************************************************
 * API_IFLIST: List the interfaces for the given machine
 *********************************************************************/

static int
api_iflist(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
		struct link * lnk;
		int	j;
		int	last = config->nodecount-1;
		const char *		cnode;
		struct node_info *	node;

		if ((cnode = ha_msg_value(msg, F_NODENAME)) == NULL
		||	(node = lookup_node(cnode)) == NULL) {
			*failreason = "EINVAL";
			return I_API_BADREQ;
		}
		if (node->nodetype == PINGNODE_I) {
			return api_ping_iflist
			(	msg, node, resp ,client, failreason);
		}

		/* Find last link... */
 		for(j=0; (lnk = &node->links[j]) && lnk->name; ++j) {
			last = j;
                }
		/* Don't report on ping links */
		if (node->links[last].isping) {
			--last;
		}

		for (j=0; j <= last; ++j) {
			if (node->links[j].isping) {
				continue;
			}
			if (ha_msg_mod(resp, F_IFNAME
			,	node->links[j].name) != HA_OK) {
				ha_log(LOG_ERR
				,	"api_iflist: "
				"cannot mod field/1");
				return I_API_IGN;
			}
			if (ha_msg_mod(resp, F_APIRESULT
			,	(j == last ? API_OK : API_MORE))
			!=	HA_OK) {
				ha_log(LOG_ERR
				,	"api_iflist: "
				"cannot mod field/2");
				return I_API_IGN;
			}
			api_send_client_msg(client, resp);
		}
	return I_API_IGN;
}

static int
api_ping_iflist(const struct ha_msg* msg, struct node_info * node
,	struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
	int	j;
	struct link * lnk;

 	for(j=0; (lnk = &node->links[j]) && lnk->name; ++j) {
		if (strcmp(lnk->name, node->nodename) == 0) {
			if (ha_msg_mod(resp, F_IFNAME
			,	lnk->name) != HA_OK) {
				ha_log(LOG_ERR
				,	"api_ping_iflist: "
				"cannot mod field/1");
				return I_API_IGN;
			}
			if (ha_msg_mod(resp, F_APIRESULT, API_OK)!= HA_OK) {
				ha_log(LOG_ERR
				,	"api_ping_iflist: "
				"cannot mod field/2");
				return I_API_IGN;
			}
			return I_API_RET;
		}
	}
	*failreason = "UhOh";
	return I_API_BADREQ;
}

/**********************************************************************
 * API_IFSTATUS: Return the status of the given interface...
 *********************************************************************/

static int
api_ifstatus(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
	const char *		cnode;
	struct node_info *	node;
	const char *		ciface;
	struct link *		iface;

	if ((cnode = ha_msg_value(msg, F_NODENAME)) == NULL
	||	(node = lookup_node(cnode)) == NULL
	||	(ciface = ha_msg_value(msg, F_IFNAME)) == NULL
	||	(iface = lookup_iface(node, ciface)) == NULL) {
		*failreason = "EINVAL";
		return I_API_BADREQ;
	}
	if (ha_msg_mod(resp, F_STATUS,	iface->status) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_ifstatus: cannot add field/1");
		ha_log(LOG_ERR
		,	"name: %s, value: %s (if=%s)"
		,	F_STATUS, iface->status, ciface);
		return I_API_IGN;
	}
	return I_API_RET;
}

/**********************************************************************
 * API_GET_PARAMETER: Return the value of the given parameter...
 *********************************************************************/

static int
api_get_parameter (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
	const char *		pname;
	const char *		pvalue;

	if ((pname = ha_msg_value(msg, F_PNAME)) == NULL) {
		*failreason = "EINVAL";
		return I_API_BADREQ;
	}
	if ((pvalue = GetParameterValue(pname)) != NULL) {
		if (ha_msg_mod(resp, F_PVALUE, pvalue) != HA_OK) {
			ha_log(LOG_ERR
			,	"api_parameter: cannot add "
			F_PVALUE " field to message");
		}
	}
	return I_API_RET;
}
static int
api_get_resources (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
	const char *		ret;

	if (!DoManageResources) {
		*failreason = "EINVAL";
		return I_API_BADREQ;
	}

	ret = hb_rsc_resource_state();
	if (ha_msg_mod(resp, F_RESOURCES, ret) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_get_resources: cannot add " F_RESOURCES
		" field to message");
	}
	return I_API_RET;
}

/*
 * Process an API request message from one of our clients
 */
static void
api_process_request(client_proc_t* fromclient, struct ha_msg * msg)
{
	const char *	msgtype;
	const char *	reqtype;
	const char *	fromid;
	const char *	pid;
	client_proc_t*	client;
	struct ha_msg *	resp = NULL;
	const char *	failreason = NULL;
	int x;

	if (msg == NULL || (msgtype = ha_msg_value(msg, F_TYPE)) == NULL) {
		ha_log(LOG_ERR, "api_process_request: bad message type");
		goto freeandexit;
	}

	/* Things that aren't T_APIREQ are general packet xmit requests... */
	if (strcmp(msgtype, T_APIREQ) != 0) {

		/* Only named clients can send out packets to clients */

		if (fromclient->iscasual) {
			ha_log(LOG_INFO, "api_process_request: "
			"general message from casual client!");
			/* Bad Client! */
			fromclient->removereason = "badclient";
			goto freeandexit;
		}

		/* We put their client ID info in the packet as the F_FROMID*/
		if (ha_msg_mod(msg, F_FROMID, fromclient->client_id) !=HA_OK){
			ha_log(LOG_ERR, "api_process_request: "
			"cannot add F_FROMID field");
			goto freeandexit;
		}
		/* Is this too restrictive? */
		/* We also put their client ID info in the packet as F_TOID */

		if (ha_msg_mod(msg, F_TOID, fromclient->client_id) != HA_OK) {
			ha_log(LOG_ERR, "api_process_request: "
			"cannot add F_TOID field");
			goto freeandexit;
		}
		if (DEBUGDETAILS) {
			ha_log(LOG_DEBUG, "Sending API message to cluster...");
			ha_log_message(msg);
		}

		/* Mikey likes it! */
		if (send_cluster_msg(msg) != HA_OK) {
			ha_log(LOG_ERR, "api_process_request: "
			"cannot forward message to cluster");
		}
		msg = NULL;
		return;
	}

	/* It must be a T_APIREQ request */

	fromid = ha_msg_value(msg, F_FROMID);
	pid = ha_msg_value(msg, F_PID);
	reqtype = ha_msg_value(msg, F_APIREQ);

	if ((fromid == NULL && pid == NULL) || reqtype == NULL) {
		ha_log(LOG_ERR, "api_process_request: no fromid/pid/reqtype"
		" in message.");
		goto freeandexit;
	}

	/*
	 * Create the response message
	 */
	if ((resp = ha_msg_new(4)) == NULL) {
		ha_log(LOG_ERR, "api_process_request: out of memory/1");
		goto freeandexit;
	}

	/* API response messages are of type T_APIRESP */
	if (ha_msg_add(resp, F_TYPE, T_APIRESP) != HA_OK) {
		ha_log(LOG_ERR, "api_process_request: cannot add field/2");
		goto freeandexitresp;
	}
	/* Echo back the type of API request we're responding to */
	if (ha_msg_add(resp, F_APIREQ, reqtype) != HA_OK) {
		ha_log(LOG_ERR, "api_process_request: cannot add field/3");
		goto freeandexitresp;
	}


	if ((client = find_client(fromid, pid)) == NULL) {
		ha_log(LOG_ERR, "api_process_request: msg from non-client");
		goto freeandexitresp;
	}

	/* See if they correctly stated their client id information... */
	if (client != fromclient) {
		ha_log(LOG_ERR, "Client mismatch! (impersonation?)");
		ha_log(LOG_INFO, "pids (%ld vs %ld), Client IDs (%s vs %s)"
		,	(long) client->pid
		,	(long) fromclient->pid
		,	client->client_id
		,	fromclient->client_id);
		goto freeandexitresp;
	}

	for(x = 0 ; x < DIMOF(query_handler_list); x++) { 

		int ret;

		if(strcmp(reqtype, query_handler_list[x].queryname) == 0) {
			ret = query_handler_list[x].handler(msg, resp, client
						, &failreason);
			switch(ret) {
			case I_API_IGN:
				goto freeandexitresp;
			case I_API_RET:
				if (ha_msg_mod(resp, F_APIRESULT, API_OK)
				!=	HA_OK) {
					ha_log(LOG_ERR
					,	"api_process_request:"
					" cannot add field/8.1");
					goto freeandexitresp;
				}
				api_send_client_msg(client, resp);
				goto freeandexitresp;

			case I_API_BADREQ:
				goto bad_req;
			}
		}
	}


	/********************************************************************
	 * Unknown request type...
	 ********************************************************************/
	ha_log(LOG_ERR, "Unknown API request");

	/* Common error return handling */
bad_req:
	ha_log(LOG_ERR, "api_process_request: bad request [%s]"
	,	reqtype);
	ha_log_message(msg);
	if (ha_msg_add(resp, F_APIRESULT, API_BADREQ) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_process_request: cannot add field/11");
		goto freeandexitresp;
	}
	if (failreason) {
		if (ha_msg_add(resp, F_COMMENT,	failreason) != HA_OK) {
			ha_log(LOG_ERR
			,	"api_process_request: cannot add failreason");
		}
	}
	api_send_client_msg(client, resp);
freeandexitresp:
	ha_msg_del(resp);
	resp=NULL;
freeandexit:
	ha_msg_del(msg); msg=NULL;
}

/* Process a registration request from a potential client */
void
process_registerevent(IPC_Channel* chan,  gpointer user_data)
{
	client_proc_t*	client;

	if ((client = MALLOCT(client_proc_t)) == NULL) {
		ha_log(LOG_ERR
		,	"unable to add client [no memory]");
		chan->ops->destroy(chan);
		return;
	}
	/* Zap! */
	memset(client, 0, sizeof(*client));
	client->pid = 0;
	client->desired_types = DEFAULTREATMENT;
	client->signal = 0;
	client->chan = chan;
	client->gsource = G_main_add_IPC_Channel(PRI_CLIENTMSG
	,	chan, FALSE
	,	APIclients_input_dispatch
	,	client, G_remove_client);

	client->next = client_list;
	client_list = client;
	total_client_count++;
}

/*
 *	Register a new client.
 */
static void
api_process_registration_msg(client_proc_t* client, struct ha_msg * msg)
{
	const char *	msgtype;
	const char *	reqtype;
	const char *	fromid;
	const char *	pid;
	struct ha_msg *	resp;
	client_proc_t*	fcli;
	
	char		deadtime[64];
	char		keepalive[64];
	char		logfacility[64];

	if (msg == NULL
	||	(msgtype = ha_msg_value(msg, F_TYPE)) == NULL
	||	(reqtype = ha_msg_value(msg, F_APIREQ)) == NULL
	||	strcmp(msgtype, T_APIREQ) != 0
	||	strcmp(reqtype, API_SIGNON) != 0)  {
		ha_log(LOG_ERR, "api_process_registration_msg: bad message");
		ha_log_message(msg);
		return;
	}
	fromid = ha_msg_value(msg, F_FROMID);
	pid = ha_msg_value(msg, F_PID);

	if (fromid == NULL && pid == NULL) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: no fromid in msg");
		return;
	}

	/*
	 *	Create the response message
	 */
	if ((resp = ha_msg_new(4)) == NULL) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: out of memory/1");
		return;
	}
	if (ha_msg_add(resp, F_TYPE, T_APIRESP) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: cannot add field/2");
		ha_msg_del(resp); resp=NULL;
		return;
	}
	if (ha_msg_add(resp, F_APIREQ, reqtype) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: cannot add field/3");
		ha_msg_del(resp); resp=NULL;
		return;
	}

	client->pid = atoi(pid);
	/*
	 *	Sign 'em up.
	 */
	if (!api_add_client(client, msg)) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: cannot add client(1)");
	}

	/* Make sure we can find them in the table... */
	if ((fcli = find_client(fromid, pid)) == NULL) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: cannot find client");
		ha_msg_del(resp); resp=NULL;
		/* We can't properly reply to them. They'll hang. Sorry... */
		return;
	}
	if (fcli != client) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: found wrong client");
		ha_msg_del(resp); resp=NULL;
		return;
	}
	if (ha_msg_mod(resp, F_APIRESULT, API_OK) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_process_registration_msg: cannot add field/4");
		ha_msg_del(resp); resp=NULL;
		return;
	}

	snprintf(deadtime, sizeof(deadtime), "%lx", config->deadtime_ms);
	snprintf(keepalive, sizeof(keepalive), "%lx", config->heartbeat_ms);
	snprintf(logfacility, sizeof(logfacility), "%d", config->log_facility);

	/* Add deadtime and keepalive time to the response */
	if (	(ha_msg_add(resp, F_DEADTIME, deadtime) != HA_OK) 
	|| 	(ha_msg_add(resp, F_KEEPALIVE, keepalive) != HA_OK)
	||	(ha_msg_mod(resp, F_NODENAME, localnodename) != HA_OK)
	|| 	(ha_msg_add(resp, F_LOGFACILITY, logfacility) != HA_OK)) {
		ha_log(LOG_ERR, "api_process_registration_msg: cannot add field/4");
		ha_msg_del(resp); resp=NULL;
		return;
	}
	if (ANYDEBUG) {
		ha_log(LOG_DEBUG, "Signing on API client %ld (%s)"
		,	(long) client->pid
		,	(client->iscasual? "'casual'" : client->client_id));
	}
	api_send_client_msg(client, resp);
	ha_msg_del(resp); resp=NULL;
}

static void
api_send_client_status(client_proc_t* client, const char * status
,	const char *	reason)
{
	struct ha_msg*	msg;

	if (client->iscasual) {
		return;
	}

	/*
	 * Create the status message
	 */
	if ((msg = ha_msg_new(4)) == NULL) {
		ha_log(LOG_ERR, "api_send_client_status: out of memory/1");
		return;
	}

	if (ha_msg_add(msg, F_TYPE, T_APICLISTAT) != HA_OK
	||	ha_msg_add(msg, F_STATUS, status) != HA_OK
	||	ha_msg_add(msg, F_FROMID, client->client_id) != HA_OK
	||	ha_msg_add(msg, F_TOID, client->client_id) != HA_OK
	||	ha_msg_add(msg, F_ORIG, curnode->nodename) != HA_OK
	||	(reason != NULL && ha_msg_add(msg, F_COMMENT, reason)
	!= HA_OK)) {
		ha_log(LOG_ERR, "api_send_client_status: cannot add fields");
		ha_msg_del(msg); msg=NULL;
		return;
	}
	if (strcmp(status, LEAVESTATUS) == 0) {
		/* Make sure they know they're signed off... */
		api_send_client_msg(client, msg);
	}
	if (send_cluster_msg(msg) != HA_OK) {
		ha_log(LOG_ERR, "api_send_client_status: "
			"cannot send message to cluster");
	}
	msg = NULL;
}

/*
 *	Send a message to a client process.
 */
static void
api_send_client_msg(client_proc_t* client, struct ha_msg *msg)
{
	if (msg2ipcchan(msg, client->chan) != HA_OK) {
		if (!client->removereason) {
			client->removereason = "sendfail";
		}
	}

	if (CL_KILL(client->pid, client->signal) < 0 && errno == ESRCH) {
		ha_log(LOG_INFO, "api_send_client: client %ld died"
		,	(long) client->pid);
		if (!client->removereason) {
			client->removereason = "died";
		}
	}
}


int
api_remove_client_pid(pid_t c_pid, const char * reason)
{
	char		cpid[20];
	client_proc_t* 	client;

	snprintf(cpid, sizeof(cpid)-1, "%d", c_pid);
	if ((client = find_client(cpid, NULL)) == NULL) {
		return 0;
	}

	client->removereason = reason;
	G_main_del_IPC_Channel(client->gsource);
	return 1;
}
static void
G_remove_client(gpointer Client)
{
	client_proc_t*	client = Client;
	const char *	reason;

	reason = client->removereason ? client->removereason : "?";

	api_remove_client_int(client, reason);
}
/*
 *	Make this client no longer a client ;-)
 *	Should only be called by G_remove_client().
 *	G_remove_client gets called by the API code when the API object
 *	gets removed. It can also get called by G_main_del_fd().
 */

static void
api_remove_client_int(client_proc_t* req, const char * reason)
{
	client_proc_t*	prev = NULL;
	client_proc_t*	client;

		
	api_send_client_status(req, LEAVESTATUS, reason);

	--total_client_count;

	if ((req->desired_types & DEBUGTREATMENTS) != 0) {
		--debug_client_count;
	}

	/* Locate the client data structure in our list */

	for (client=client_list; client != NULL; client=client->next) {
		/* Is this the client? */
		if (client->pid == req->pid) {
			if (ANYDEBUG) {
				ha_log(LOG_DEBUG
				,	"api_remove_client_int: removing"
				" pid [%ld] reason: %s"
				,	(long)req->pid, reason);
			}
			if (prev == NULL) {
				client_list = client->next;
			}else{
				prev->next = client->next;
			}

			/* Channel is automatically destroyed 
			 * by the G_CH* code...
			 */
			client->chan = NULL;

			/* Zap! */
			memset(client, 0, sizeof(*client));
			ha_free(client); client = NULL;
			return;
		}
		prev = client;
	}
	ha_log(LOG_ERR,	"api_remove_client_int: could not find pid [%ld]"
	,	(long) req->pid);
}


/*
 *	Add the process described in this message to our list of clients.
 *
 *	The following fields are used:
 *	F_PID:		Mandantory.  The client process id.
 *	F_FROMID:	The client's identifying handle.
 *			If omitted, it defaults to the F_PID field as a
 *			decimal integer.
 */
static int
api_add_client(client_proc_t* client, struct ha_msg* msg)
{
	pid_t		pid = 0;
	const char*	cpid;
	const char *	fromid;

	
	if ((cpid = ha_msg_value(msg, F_PID)) != NULL) {
		pid = atoi(cpid);
	}
	if (pid <= 0  || (CL_KILL(pid, 0) < 0 && errno == ESRCH)) {
		ha_log(LOG_WARNING
		,	"api_add_client: bad pid [%ld]", (long) pid);
		return FALSE;
	}

	fromid = ha_msg_value(msg, F_FROMID);

	
	if (find_client(fromid, NULL) != NULL) {
		ha_log(LOG_WARNING
		,	"duplicate client add request [%s] [%s]"
		,	(fromid ? fromid : "(nullfromid)")
		,	(cpid ? cpid : "(nullcpid)"));
		client->removereason = "duplicate add request";
		return FALSE;
	}

	if (fromid != NULL) {
		strncpy(client->client_id, fromid, sizeof(client->client_id));
		if (atoi(client->client_id) == pid) {
			client->iscasual = 1;
		}else{
			client->iscasual = 0;
		}
	}else{
		snprintf(client->client_id, sizeof(client->client_id)
		,	"%d", pid);
		client->iscasual = 1;
	}
	if (api_check_client_authorization(client)) {
		api_send_client_status(client, JOINSTATUS, API_SIGNON);
	}else{
		ha_log(LOG_WARNING
		,	"Client [%s] pid %d failed authorization [%s]"
		,	client->client_id, pid, client->removereason);
		api_send_client_status(client, JOINSTATUS, API_SIGNOFF);
		return FALSE;
	}
	return TRUE;
}
static gboolean
api_check_client_authorization(client_proc_t* client)
{
	gpointer	gauth;
	IPC_Auth*	auth;
	if (client->iscasual
	||	(gauth = g_hash_table_lookup(APIAuthorization
	,	client->client_id))	==  NULL) {
		if ((gauth = g_hash_table_lookup(APIAuthorization, "default"))
		== NULL) {
			client->removereason = "no default client auth";
			return FALSE;
		}
		
	}
	auth = gauth;
	if (client->chan->ops->verify_auth(client->chan, gauth)) {
		if (client->chan->farside_pid != 0) {
			if (client->chan->farside_pid != client->pid) {
				client->removereason = "pid mismatch";
				cl_log(LOG_INFO
				,	"PID mismatch: %d vs farside_pid: %d"
				,	client->pid
				,	client->chan->farside_pid);
				return FALSE;
			}
		}
		return TRUE;
	}
	client->removereason = "client failed authorization";
	return FALSE;
}



/*
 *	Find the client that goes with this client id/pid
 */
static client_proc_t*
find_client(const char * fromid, const char * cpid)
{
	pid_t	pid = -1;
	client_proc_t* client;

	if (cpid != NULL) {
		pid = atoi(cpid);
	}

	for (client=client_list; client != NULL; client=client->next) {
		if (cpid && client->pid == pid) {
			return(client);
		}
		if (fromid && strcmp(fromid, client->client_id) == 0) {
			return(client);
		}
	}
	return(NULL);
}



static gboolean
APIclients_input_dispatch(IPC_Channel* chan, gpointer user_data)
{
	client_proc_t*	client = user_data;

	if (chan != client->chan) {
		/* Bad boojum! */
		ha_log(LOG_ERR
		,	"APIclients_input_dispatch chan mismatch");
		return FALSE;
	}
	if (client->removereason) {
		return FALSE;
	}

	/* Process a single API client request */
	client->isindispatch = TRUE;
	while (ProcessAnAPIRequest(client)) {
		/* Do nothing */;
	}
	client->isindispatch = FALSE;

	if (client->removereason) {
		return FALSE;
	}

	return TRUE;
}


gboolean
ProcessAnAPIRequest(client_proc_t*	client)
{
	struct ha_msg*	msg;
	static int		consecutive_failures = 0;

	/* Supposedly got a message from 'client' */
	if (CL_KILL(client->pid, 0) < 0 &&	errno == ESRCH) {
		/* Oops... he's dead */
		ha_log(LOG_INFO
		,	"Client pid %ld died (input)"
		,	(long)client->pid);
		client->removereason = "died";
		return FALSE;
	}

	/* See if we can read the message */
	if ((msg = msgfromIPC(client->chan)) == NULL) {

		/* EOF? */
		if (client->chan->ch_status == IPC_DISCONNECT) {
			ha_log(LOG_INFO
			,	"EOF from client pid %ld"
			,	(long)client->pid);
			client->removereason = "EOF";
			return FALSE;
		}

		/* None of the above... */
		ha_log(LOG_INFO, "No message from pid %ld"
		,	(long)client->pid);
		++consecutive_failures;
		/*
		 * This used to happen because of EOF,
		 * which is now handled above.  This is
		 * good protection to have anyway ;-)
		 */
		if (consecutive_failures >= 10) {
			ha_log(LOG_ERR
			,	"Removing client pid %ld"
			,	(long)client->pid);
			client->removereason = "noinput";
			consecutive_failures = 0;
		}
		return FALSE;
	}
	consecutive_failures = 0;

	/* Process the API request message... */
	api_heartbeat_monitor(msg, APICALL, "<api>");


	/* First message must be a registration msg */
	if (client->pid == 0) {
		api_process_registration_msg(client, msg);
	}else{
		api_process_request(client, msg);
	}
	msg = NULL;

	return TRUE;
}
