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
static gboolean	client_output_pending = FALSE;
			/* TRUE when any client output still pending */
struct node_info *curnode;

static void	api_process_request(client_proc_t* client, struct ha_msg *msg);
static void	api_send_client_msg(client_proc_t* client, struct ha_msg *msg);
static void	api_send_client_status(client_proc_t* client
,	const char * status, const char *	reason);
static void	api_flush_msgQ(client_proc_t* client);
static void	api_clean_clientQ(client_proc_t* client);
static void		api_flush_pending_msgQ(void);
static void	api_remove_client_int(client_proc_t* client, const char * rsn);
static int	api_add_client(struct ha_msg* msg);
static client_proc_t*	find_client(const char * fromid, const char * pid);
static FILE*		open_reqfifo(client_proc_t* client);
static const char *	client_fifo_name(client_proc_t* client, int isreq);
static	uid_t		pid2uid(pid_t pid);
static int		ClientSecurityIsOK(client_proc_t* client);
static int		HostSecurityIsOK(void);


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

	api_flush_pending_msgQ();

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

	/* See if this client FIFOs are (still) properly secured */

	if (!ClientSecurityIsOK(client)) {
		client->removereason = "security";
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

/*
 *	Register a new client.
 */
void
api_process_registration(struct ha_msg * msg)
{
	const char *	msgtype;
	const char *	reqtype;
	const char *	fromid;
	const char *	pid;
	struct ha_msg *	resp;
	client_proc_t*	client;
	char		deadtime[64];
	char		keepalive[64];
	char		logfacility[64];

	if (msg == NULL
	||	(msgtype = ha_msg_value(msg, F_TYPE)) == NULL
	||	(reqtype = ha_msg_value(msg, F_APIREQ)) == NULL
	||	strcmp(msgtype, T_APIREQ) != 0
	||	strcmp(reqtype, API_SIGNON) != 0)  {
		ha_log(LOG_ERR, "api_process_registration: bad message");
		return;
	}
	fromid = ha_msg_value(msg, F_FROMID);
	pid = ha_msg_value(msg, F_PID);

	if (fromid == NULL && pid == NULL) {
		ha_log(LOG_ERR, "api_process_registration: no fromid in msg");
		return;
	}

	/*
	 *	Create the response message
	 */
	if ((resp = ha_msg_new(4)) == NULL) {
		ha_log(LOG_ERR, "api_process_registration: out of memory/1");
		return;
	}
	if (ha_msg_add(resp, F_TYPE, T_APIRESP) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_process_registration: cannot add field/2");
		ha_msg_del(resp); resp=NULL;
		return;
	}
	if (ha_msg_add(resp, F_APIREQ, reqtype) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_process_registration: cannot add field/3");
		ha_msg_del(resp); resp=NULL;
		return;
	}

	/*
	 *	Sign 'em up.
	 */
	if (!api_add_client(msg)) {
		ha_log(LOG_ERR
		,	"api_process_registration: cannot add client");
	}

	/* Make sure we can find them in the table... */
	if ((client = find_client(fromid, pid)) == NULL) {
		ha_log(LOG_ERR
		,	"api_process_registration: cannot add client");
		ha_msg_del(resp); resp=NULL;
		/* We can't properly reply to them.  Sorry they'll hang... */
		return;
	}
	if (ha_msg_mod(resp, F_APIRESULT, API_OK) != HA_OK) {
		ha_log(LOG_ERR
		,	"api_process_registration: cannot add field/4");
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
		ha_log(LOG_ERR, "api_process_registration: cannot add field/4");
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
	char *		msgstring;


	if ((msgstring = msg2string(msg)) == NULL) {
		ha_log(LOG_ERR
		,	"api_send_client_message: msg2string failed client %ld"
		,	(long) client->pid);
		return;
	}

	/*
	 * We should enforce some kind of a limit on messages we queue
	 * up for clients.  Sick clients shouldn't use infinite resources.
	 */

	client->msgQ = g_list_append(client->msgQ, msgstring);
	++client->msgcount;
	api_flush_msgQ(client);
}


static void
api_flush_msgQ(client_proc_t* client)
{
	const char*	fifoname;
	int		rc;
	int		nsig;
	int		writeok=0;
	pid_t		clientpid = client->pid;

	fifoname = client_fifo_name(client, 0);

	if (client->output_fifofd < 0) {
		client->output_fifofd = open(fifoname, O_WRONLY|O_NDELAY);
	}
	if(client->output_fifofd < 0) {
		if (client->removereason) {
			return;
		}

		/* Sometimes they've gone before we know it */
		/* Then we get ENXIO.  So we ignore those. */
		if (errno != ENXIO && errno != EINTR) {
			/*
			 * FIXME:  ???
			 * It seems like with the O_NDELAY on the
			 * open we ought not get EINTR.  But we
			 * do anyway...
			 */
			ha_perror("api_flush_msgQ: can't open %s", fifoname);
		}
		client->removereason = "FIFOerr";

		return;
	}

	/* Write out each message in the queue */
	while (client->msgQ != NULL) {
		char *		msgstring;
		int		msglen;

		msgstring = (char*)(client->msgQ->data);
		msglen = strlen(msgstring);
		rc=write(client->output_fifofd, msgstring, msglen);
		if (rc != msglen) {
			if (rc < 0 && errno == EPIPE) {
				client->removereason = "EPIPE";
				break;
			}
			if (rc < 0 && errno == EINTR) {
				continue;
			}
			if (rc >= 0 || errno != EAGAIN) {
				ha_perror("Cannot write message to client"
				" %ld (write failure %d)"
				,	(long) clientpid, rc);
			}
			client_output_pending = TRUE;
			break;
		}
		if (DEBUGPKTCONT) {
			cl_log(LOG_DEBUG, "Sending message to client pid %d: "
					"msg [%s]", client->pid, msgstring);
		}

		/* If the write succeeded, remove msg from queue */
		++writeok;
		--client->msgcount;
		client->msgQ = g_list_remove(client->msgQ, msgstring);
	}
	nsig = (writeok ? client->signal : 0);

	if (CL_KILL(clientpid, nsig) < 0 && errno == ESRCH) {
		ha_log(LOG_INFO, "api_send_client: client %ld died"
		,	(long) client->pid);
		client->removereason = "died";
	}else if (!ClientSecurityIsOK(client)) {
		client->removereason = "security";
	}

}

/*
 * Try to  deliver messages that were not deliverable to clients earlier
 */
static void
api_flush_pending_msgQ(void)
{
      client_proc_t* client;

	if (!client_output_pending) {
		return;
	}
	client_output_pending = FALSE;
	for (client=client_list; client != NULL; client=client->next) {
		/*
		 * NOTE: api_flush_msgQ() sets client_output_pending
		 * if any messages cannot be delivered.
		 */
       		if(client->msgcount) {
			api_flush_msgQ(client);
		}
	}
}

static void
api_clean_clientQ(client_proc_t* client)
{
	while (client->msgQ != NULL) {
		char *	msg;
		--client->msgcount;
		msg = client->msgQ->data;
		client->msgQ = g_list_remove(client->msgQ, msg);
		ha_free(msg); msg = NULL;
	}
}

/*
 *	The range of file descriptors we have open for the request FIFOs
 */

static int	maxfd = -1;
static int	minfd = -1;

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
	G_main_del_fd(client->g_source_id);
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
			/* Close the input FIFO */
			if (client->input_fifo != NULL) {
				int	fd = client->fd;
				if (fd == maxfd) {
					--maxfd;
				}
				fclose(client->input_fifo);
				client->input_fifo = NULL;

				if (UseOurOwnPoll) {
					cl_poll_ignore(client->fd);
				}
			}
			close(client->output_fifofd);
			client->output_fifofd = -1;
			/* Clean up after casual clients */
			if (client->iscasual) {
				unlink(client_fifo_name(client, 0));
				unlink(client_fifo_name(client, 1));
			}
			if (prev == NULL) {
				client_list = client->next;
			}else{
				prev->next = client->next;
			}

			/* Throw away any Queued messages */
			api_clean_clientQ(client);

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
api_add_client(struct ha_msg* msg)
{
	pid_t		pid = 0;
	int		fifoifd;
	FILE*		fifofp;
	client_proc_t*	client;
	const char*	cpid;
	const char *	fromid;

	
	/* Not a wonderful place to call it, but not too bad either... */

	if (!HostSecurityIsOK()) {
		return 0;
	}

	if ((cpid = ha_msg_value(msg, F_PID)) != NULL) {
		pid = atoi(cpid);
	}
	if (pid <= 0  || (CL_KILL(pid, 0) < 0 && errno == ESRCH)) {
		ha_log(LOG_WARNING
		,	"api_add_client: bad pid [%ld]", (long) pid);
		return 0;
	}
	fromid = ha_msg_value(msg, F_FROMID);

	client = find_client(cpid, fromid);

	if (client != NULL) {
		if (CL_KILL(client->pid, 0) == 0 || errno != ESRCH) {
			ha_log(LOG_WARNING
			,	"duplicate client add request");
			return 0;
		}else{
			ha_log(LOG_ERR
			,	"client pid %ld [%s] died (api_add_client)"
			,	(long) client->pid, fromid);
		}
		client->removereason = "bad add request";
	}
	if ((client = MALLOCT(client_proc_t)) == NULL) {
		ha_log(LOG_ERR
		,	"unable to add client pid %ld [no memory]", (long) pid);
		return 0;
	}
	/* Zap! */
	memset(client, 0, sizeof(*client));
	client->input_fifo = NULL;
	client->output_fifofd = -1;
	client->msgQ = NULL;
	client->pid = pid;
	client->desired_types = DEFAULTREATMENT;
	client->signal = 0;

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

	client->next = client_list;
	client_list = client;
	total_client_count++;

	/* Make sure their FIFOs are properly secured */
	if (!ClientSecurityIsOK(client)) {
		/* No insecure clients allowed! */
		client->removereason = "security";
		return 0;
	}
	if ((fifofp=open_reqfifo(client)) <= 0) {
		ha_log(LOG_ERR
		,	"Unable to open API FIFO for client %s"
		,	client->client_id);
		client->removereason = "fifo open";
		return 0;
	}
	fifoifd=fileno(fifofp);
	client->input_fifo = fifofp;
	if (fifoifd > maxfd) {
		maxfd = fifoifd;
	}
	if (minfd < 0 || fifoifd < minfd) {
		minfd = fifoifd;
	}
	api_send_client_status(client, JOINSTATUS, API_SIGNON);
	return 1;
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

/*
 *	Return the name of the client FIFO of the given type
 *		(request or response)
 */
static const char *
client_fifo_name(client_proc_t* client, int isrequest)
{
	static char	fifoname[PATH_MAX];
	const char *	dirprefix;
	const char *	fifosuffix;

	dirprefix = (client->iscasual ? CASUALCLIENTDIR : NAMEDCLIENTDIR);
	fifosuffix = (isrequest ? REQ_SUFFIX : RSP_SUFFIX);
	
	snprintf(fifoname, sizeof(fifoname), "%s/%s%s"
	,	dirprefix, client->client_id, fifosuffix);
	return(fifoname);
}


/*
 * Our Goal: To be as big a pain in the posterior as we can be :-)
 */

static int
HostSecurityIsOK(void)
{
	uid_t		our_uid = geteuid();
	struct stat	s;

	/*
	 * Check out the Heartbeat internal-use FIFO...
	 */

	if (stat(FIFONAME, &s) < 0) {
		ha_log(LOG_ERR
		,	"FIFO %s does not exist", FIFONAME);
		return(0);
	}

	/* Is the heartbeat FIFO internal-use pathname a FIFO? */

	if (!S_ISFIFO(s.st_mode)) {
		ha_log(LOG_ERR
		,	"%s is not a FIFO", FIFONAME);
		unlink(FIFONAME);
		return 0;
	}
	/*
	 * Check to make sure it isn't readable or writable by group or other.
	 */

	if ((s.st_mode&(S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) != 0) {
		ha_log(LOG_ERR
		,	"FIFO %s is not secure.", FIFONAME);
		return 0;
	}

	/* Let's make sure it's owned by us... */

	if (s.st_uid != our_uid) {
		ha_log(LOG_ERR
		,	"FIFO %s not owned by uid %ld.", FIFONAME
		,	(long) our_uid);
		return 0;
	}

	/*
	 *	Now, let's check out the API registration FIFO
	 */

	if (stat(API_REGFIFO, &s) < 0) {
		ha_log(LOG_ERR
		,	"FIFO %s does not exist", API_REGFIFO);
		return(0);
	}

	/* Is the registration FIFO pathname a FIFO? */

	if (!S_ISFIFO(s.st_mode)) {
		ha_log(LOG_ERR
		,	"%s is not a FIFO", API_REGFIFO);
		unlink(FIFONAME);
		return 0;
	}
	/*
	 * Check to make sure it isn't readable or writable by other
	 * or readable by group.
	 */

	if ((s.st_mode&(S_IRGRP|S_IROTH|S_IWOTH)) != 0) {
		ha_log(LOG_ERR
		,	"FIFO %s is not secure.", API_REGFIFO);
		return 0;
	}

	/* Let's make sure it's owned by us... */
	if (s.st_uid != our_uid) {
		ha_log(LOG_ERR
		,	"FIFO %s not owned by uid %ld.", API_REGFIFO
		,	(long) our_uid);
		return 0;
	}


	/* 
	 * Check out the casual client FIFO directory
	 */

	if (stat(CASUALCLIENTDIR, &s) < 0) {
		ha_log(LOG_ERR
		,	"Directory %s does not exist", CASUALCLIENTDIR);
		return(0);
	}

	/* Is the Casual Client FIFO directory pathname really a directory? */

	if (!S_ISDIR(s.st_mode)) {
		ha_log(LOG_ERR
		,	"%s is not a Directory", CASUALCLIENTDIR);
		return(0);
	}

	/* Let's make sure it's owned by us... */

	if (s.st_uid != our_uid) {
		ha_log(LOG_ERR
		,	"Directory %s not owned by uid %ld.", CASUALCLIENTDIR
		,	(long) our_uid);
		return 0;
	}

	/* Make sure it isn't R,W or X by other. */

	if ((s.st_mode&(S_IROTH|S_IWOTH|S_IXOTH)) != 0) {
		ha_log(LOG_ERR
		,	"Directory %s is not secure.", CASUALCLIENTDIR);
		return 0;
	}

	/* Make sure it *is* executable and writable by group */

	if ((s.st_mode&(S_IXGRP|S_IWGRP)) != (S_IXGRP|S_IWGRP)){
		ha_log(LOG_ERR
		,	"Directory %s is not usable.", CASUALCLIENTDIR);
		return 0;
	}

	/* Make sure the casual client FIFO directory is sticky */

	if ((s.st_mode&(S_IXGRP|S_IWGRP|S_ISVTX)) != (S_IXGRP|S_IWGRP|S_ISVTX)){
		ha_log(LOG_ERR
		,	"Directory %s is not sticky.", CASUALCLIENTDIR);
		return 0;
	}

	/* 
	 * Check out the Named Client FIFO directory
	 */

	if (stat(NAMEDCLIENTDIR, &s) < 0) {
		ha_log(LOG_ERR
		,	"Directory %s does not exist", NAMEDCLIENTDIR);
		return(0);
	}

	/* Is the Named Client FIFO directory pathname actually a directory? */

	if (!S_ISDIR(s.st_mode)) {
		ha_log(LOG_ERR
		,	"%s is not a Directory", NAMEDCLIENTDIR);
		return(0);
	}

	/* Let's make sure it's owned by us... */

	if (s.st_uid != our_uid) {
		ha_log(LOG_ERR
		,	"Directory %s not owned by uid %ld.", NAMEDCLIENTDIR
		,	(long) our_uid);
		return 0;
	}

	/* Make sure it isn't R,W or X by other, or writable by group */

	if ((s.st_mode&(S_IXOTH|S_IROTH|S_IWOTH|S_IWGRP)) != 0) {
		ha_log(LOG_ERR
		,	"Directory %s is not secure.", NAMEDCLIENTDIR);
		return 0;
	}

	/* Make sure it *is* executable by group */

	if ((s.st_mode&(S_IXGRP)) != (S_IXGRP)) {
		ha_log(LOG_ERR
		,	"Directory %s is not usable.", NAMEDCLIENTDIR);
		return 0;
	}
	return 1;
}

/*
 *	We are the security tough-guys.  Or so we hope ;-)
 */
static int
ClientSecurityIsOK(client_proc_t* client)
{
	const char *	fifoname;
	struct stat	s;
	uid_t		client_uid;
	uid_t		our_uid;

	/* Does this client even exist? */

	if (CL_KILL(client->pid, 0) < 0 && errno == ESRCH) {
		ha_log(LOG_ERR
		,	"Client pid %ld does not exist", (long) client->pid);
		return(0);
	}
	client_uid = pid2uid(client->pid);
	our_uid = geteuid();


	/*
	 * Check the security of the Client's Request FIFO
	 */

	fifoname = client_fifo_name(client, 1);

	if (stat(fifoname, &s) < 0) {
		ha_log(LOG_ERR
		,	"FIFO %s does not exist", fifoname);
		return(0);
	}

	/* Is the request FIFO pathname a FIFO? */

	if (!S_ISFIFO(s.st_mode)) {
		ha_log(LOG_ERR
		,	"%s is not a FIFO", fifoname);
		unlink(fifoname);
		return 0;
	}

	/*
	 * Check to make sure it isn't writable by group or other,
	 * or readable by others.
	 */
	if ((s.st_mode&(S_IWGRP|S_IWOTH|S_IROTH)) != 0) {
		ha_log(LOG_ERR
		,	"FIFO %s is not secure.", fifoname);
		return 0;
	}

	/*
	 * The request FIFO shouldn't be group readable unless it's
 	 * grouped to our effective group id, and we aren't root. 
	 * If we're root, we can read it anyway, so there's no reason
	 * we should allow it to be group readable.
	 */

	if ((s.st_mode&S_IRGRP) != 0 && s.st_gid != getegid()
	&&	geteuid() != 0) {
		ha_log(LOG_ERR
		,	"FIFO %s is not secure (g+r).", fifoname);
		return 0;
	}

	/* Does it look like the given client pid can write this FIFO? */

	if (client_uid != s.st_uid) {
		ha_log(LOG_ERR
		,	"Client pid %ld is not uid %ld like they"
		" must be to write FIFO %s"
		,	(long)client->pid, (long)s.st_uid, fifoname);
		return 0;
	}

	/*
	 * Check the security of the Client's Response FIFO
	 */

	fifoname = client_fifo_name(client, 0);
	if (stat(fifoname, &s) < 0) {
		ha_log(LOG_ERR
		,	"FIFO %s does not exist", fifoname);
		return 0;
	}

	/* Is the response FIFO pathname a FIFO? */

	if (!S_ISFIFO(s.st_mode)) {
		ha_log(LOG_ERR
		,	"%s is not a FIFO", fifoname);
		unlink(fifoname);
		return 0;
	}

	/*
	 * Is the response FIFO secure?
	 */

	/*
	 * Check to make sure it isn't readable by group or other,
	 * or writable by others.
	 */
	if ((s.st_mode&(S_IRGRP|S_IROTH|S_IWOTH)) != 0) {
		ha_log(LOG_ERR
		,	"FIFO %s is not secure.", fifoname);
		return 0;
	}

	/*
	 * The response FIFO shouldn't be group writable unless it's
 	 * grouped to our effective group id, and we aren't root. 
	 * If we're root, we can write it anyway, so there's no reason
	 * we should allow it to be group writable.
	 */
	if ((s.st_mode&S_IWGRP) != 0 && s.st_gid != getegid()
	&&	geteuid() != 0) {
		ha_log(LOG_ERR
		,	"FIFO %s is not secure (g+w).", fifoname);
		return 0;
	}

	/* Does it look like the given client pid can read this FIFO? */

	if (client_uid != s.st_uid) {
		ha_log(LOG_ERR
		,	"Client pid %ld is not uid %ld like they"
		" must be to read FIFO %s"
		,	(long)client->pid, (long)s.st_uid, fifoname);
		return 0;
	}
	return 1;
}

static gboolean
APIclients_input_dispatch(int fd, gpointer user_data);

/*
 * Open the request FIFO for the given client.
 */
static FILE*
open_reqfifo(client_proc_t* client)
{
	struct stat	s;
	const char *	fifoname = client_fifo_name(client, 1);
	int		fd;
	FILE *		ret;


	if (client->input_fifo != NULL) {
		return(client->input_fifo);
	}

	/* How about that! */
	client->uid = s.st_uid;
	/*
	 *	FIXME realtime:
	 *	This code costs us realtime.
	 *	To fix it we need to switch to sockets which we only open
	 *	once when we first start up.  Our socket-based IPC library
	 *	is lots nicer, but it will take some work to get there...
	 */
	fd = open(fifoname, O_RDONLY|O_NDELAY);
	if (fd < 0) {
		return(NULL);
	}
	if ((ret = fdopen(fd, "r")) != NULL) {
#if 0
		/*FIXME!!  WHY DID WE DO THIS? */
	 	/* FIXME realtime!! */
		setbuf(ret, NULL);
#endif
	}
	client->fd = fd;
	client->g_source_id = G_main_add_fd(G_PRIORITY_DEFAULT, fd, FALSE
	,	APIclients_input_dispatch, client, G_remove_client);
	return ret;
}

#define	PROC	"/proc/"

/* Return the uid of the given pid */

static	uid_t
pid2uid(pid_t pid)
{
	struct stat	s;
	char	procpath[sizeof(PROC)+20];

	snprintf(procpath, sizeof(procpath), "%s%ld", PROC, (long)pid);

	if (stat(procpath, &s) < 0) {
		return(-1);
	}
	/*
	 * This isn't a perfect test.  On Linux we could look at the
	 * /proc/$pid/status file for the line that says:
	 *	Uid:    500     500     500     500 
	 * and parse it for find out whatever we want to know.
	 */
	return s.st_uid;
}

static gboolean
APIclients_input_dispatch(int fd, gpointer user_data)
{
	client_proc_t*	client = user_data;

	if (fd != client->fd) {
		/* Bad boojum! */
		ha_log(LOG_ERR
		,	"APIclients_input_dispatch fd mismatch"
		": %d vs %d for pid %ld"
		,	fd, client->fd, (long)client->pid);
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
	if ((msg = msgfromstream(client->input_fifo)) == NULL) {

		/* EOF? */
		if (feof(client->input_fifo)) {
			ha_log(LOG_INFO
			,	"EOF from client pid %ld"
			,	(long)client->pid);
			client->removereason = "EOF";
			return FALSE;
		}

		/* Interrupted read or no data? */
		if (ferror(client->input_fifo)
		&&	(errno == EINTR || errno == EAGAIN)) {
			clearerr(client->input_fifo);
			return FALSE;
		}

		/* None of the above... */
		ha_log(LOG_INFO, "No message from pid %ld"
		,	(long)client->pid);

		if (ferror(client->input_fifo)) {
			ha_perror("API FIFO read error: pid %ld"
			,	(long)client->pid);
			clearerr(client->input_fifo);
		}
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
	api_process_request(client, msg);
	msg = NULL;

	return TRUE;
}
