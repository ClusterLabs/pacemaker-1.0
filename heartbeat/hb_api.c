/* $Id: hb_api.c,v 1.152 2006/02/10 08:17:57 andrew Exp $ */
/*
 * hb_api: Server-side heartbeat API code
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 * Copyright (C) 2000 Marcelo Tosatti <marcelo@conectiva.com.br>
 *
 * Thanks to Conectiva S.A. for sponsoring Marcelo Tosatti work
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
#include <clplumbing/netstring.h>

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

static int api_clientstatus (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int
api_num_nodes(const struct ha_msg* msg, struct ha_msg* resp
	      ,	client_proc_t* client, const char** failreason);

static int api_get_parameter (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_get_resources (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_get_uuid (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);

static int api_get_nodename (const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason);
static int
api_set_sendqlen(const struct ha_msg* msg, struct ha_msg* resp,
		 client_proc_t* client, const char** failreason);

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
	{ API_CLIENTSTATUS, api_clientstatus },
	{ API_NUMNODES, api_num_nodes},
	{ API_GETPARM, api_get_parameter},
	{ API_GETRESOURCES, api_get_resources},
	{ API_GETUUID, api_get_uuid},
	{ API_GETNAME, api_get_nodename},
	{ API_SET_SENDQLEN, api_set_sendqlen}	
};

extern int	UseOurOwnPoll;
int		debug_client_count = 0;
int		total_client_count = 0;
client_proc_t*	client_list = NULL;	/* List of all our API clients */
			/* TRUE when any client output still pending */
extern struct node_info *curnode;

static unsigned long	client_generation = 0; 
#define MAX_CLIENT_GEN 64

static void	api_process_request(client_proc_t* client, struct ha_msg *msg);
static void	api_send_client_msg(client_proc_t* client, struct ha_msg *msg);
static void	api_send_client_status(client_proc_t* client
,	const char * status, const char *	reason);
static void	api_remove_client_int(client_proc_t* client, const char * rsn);
static int	api_add_client(client_proc_t* chan, struct ha_msg* msg);
static void	G_remove_client(gpointer Client);
static gboolean	APIclients_input_dispatch(IPC_Channel* chan, gpointer udata);
static void	api_process_registration_msg(client_proc_t*, struct ha_msg *);
static gboolean	api_check_client_authorization(client_proc_t* client);
static int	create_seq_snapshot_table(GHashTable** ptable) ;
static void	destroy_seq_snapshot_table(GHashTable* table);
extern GHashTable*	APIAuthorization;

struct seq_snapshot{
	seqno_t		generation;
	seqno_t		last_seq;
};



static int
should_msg_sendto_client(client_proc_t* client, struct ha_msg* msg)
{
	GHashTable* table;
	struct node_info *	thisnode = NULL;	
	const char *		from;
	cl_uuid_t		fromuuid;
	struct seq_snapshot*	snapshot;
	const char *		cseq;
	const char *		cgen;
	seqno_t			seq;
	seqno_t			gen;
	int			ret = 0;
	const char*		type;
	struct seqtrack *	t;

	if (!client || !msg){
		cl_log(LOG_ERR, "should_msg_sendto_client:"
		       " invalid arguemts");
		return FALSE;
	}
	
	

	from = ha_msg_value(msg, F_ORIG);
	cseq = ha_msg_value(msg, F_SEQ);
	cgen = ha_msg_value(msg, F_HBGENERATION);
	
	if (!from || !cseq || !cgen){
		/* some local generated status messages,
		 * e.g. node dead status message,
		 * return yes
		 */
		return TRUE;
	}
	
	if (sscanf(cseq, "%lx", &seq) <= 0 || sscanf(cgen, "%lx", &gen) <= 0) {
		cl_log(LOG_ERR, "should_msg_sendto_client:"
		       "wrong seq/gen format");
		return FALSE;
	}
	
	if (seq < 0 || gen < 0){
		cl_log(LOG_ERR, "should_msg_sendto_client:"
		       "wrong seq/gen number");
		return FALSE;
	}
	
	cl_get_uuid(msg, F_ORIGUUID, &fromuuid);
	thisnode = lookup_tables(from, &fromuuid);
	if ( thisnode == NULL){
		cl_log(LOG_ERR, "should_msg_sendto_client:"
		       "node not found in table");
		return FALSE;
	}

	t = &thisnode->track;

	/*if uuid is not found, then it always passes the first restriction*/
	if ( cl_uuid_is_null(&fromuuid)
	     || (table = client->seq_snapshot_table)== NULL
	     || (snapshot= (struct seq_snapshot*)
		 g_hash_table_lookup(table, &fromuuid)) == NULL){
		goto nextstep;
	}
		
	ret = gen > snapshot->generation ||
		(gen == snapshot->generation && seq >= snapshot->last_seq);
	

	/*check if there is any retransmission going on
	  if not, we can delete this item
	*/

	if (t->nmissing == 0){
		
		if (ANYDEBUG){
			cl_log(LOG_DEBUG,
			       "Removing one entry in seq snapshot hash table"
			       "for node %s", thisnode->nodename);
		}
		
		if(!g_hash_table_remove(table, &fromuuid)){
			cl_log(LOG_ERR,"should_msg_sendto_client:"
			       "g_hash_table_remove failed");
			return FALSE;
		}
		cl_free(snapshot);
		
		
		if ( g_hash_table_size(table) ==0){
			if (ANYDEBUG){
				cl_log(LOG_DEBUG,
				       "destroying the seq snapshot hash table");
			}

			g_hash_table_destroy(table);
			client->seq_snapshot_table = NULL;
		}
	}		
	
	if ( ret == 0  ){
		/* hmmmm.... this message is dropped */
		cl_log(LOG_WARNING, "message is dropped ");
		cl_log_message(LOG_WARNING, msg);
		return FALSE;
	}
	
 nextstep:
        /* We only worry about the ordering of certain types of messages
	 * and then only when they arrive out of order.
	 * Basically we implement a barrier at the receipt of each
	 * message of this type.
	 */
	if( (type = ha_msg_value(msg, F_TYPE)) == NULL){
		cl_log(LOG_ERR, "no type field found");
		return FALSE;
	}
	
	if ( strcmp(type, T_APICLISTAT) != 0 || t->nmissing == 0){		
		return TRUE;
	}
	

	if ( seq > t->first_missing_seq ){
		/*We cannot deliver the message now,
		  queue it*/		
		
		struct ha_msg* copymsg = ha_msg_copy(msg);
		
		if (!copymsg){
			cl_log(LOG_ERR, "msg copy failed");
			return FALSE;
		}
		
		t->client_status_msg_queue = 
			g_list_append(t->client_status_msg_queue,
				      copymsg);
		if (ANYDEBUG){
			cl_log(LOG_DEBUG,"one entry added to "
			       "client_status_msg_queue"
			       "for node %s", thisnode->nodename);
		}
		return FALSE;
	}
	

	
	return TRUE;
}



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
	

	/* This kicks out most messages, since debug clients are rare */

	if ((msgtype&DEBUGTREATMENTS) != 0 && debug_client_count <= 0) {
		return;
	}

	/* Verify that we understand what kind of message we've got here */

	if ((msgtype & ALLTREATMENTS) != msgtype || msgtype == 0) {
		cl_log(LOG_ERR, "heartbeat_monitor: unknown msgtype [%d]"
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

		/* Is this one of the types of messages we're interested in?*/
		
		if ((msgtype & client->desired_types) != 0) {		       	
			
			if (should_msg_sendto_client(client, msg)){				
				api_send_client_msg(client, msg);				
			}else {
				/*This happens when join/leave messages is
				 *received but there are messages before 
				 *that are missing. The join/leave messages
				 *will be queued and not delivered until all
				 *messages before them are received and 
				 *delivered. 
				 */
				
				/*do nothing*/
			}
			
			if (client->removereason && !client->isindispatch) {
				if (ANYDEBUG){
					cl_log(LOG_DEBUG
					,	"%s: client is %s"
					,	__FUNCTION__
					,	client->client_id);
				}
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
			cl_log(LOG_INFO, "api_audit_clients: client %ld died"
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
			cl_log(LOG_DEBUG, "Signing client %ld off"
			,	(long) client->pid);
		}
		if (client->seq_snapshot_table){
			destroy_seq_snapshot_table(client->seq_snapshot_table);
			client->seq_snapshot_table = NULL;
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
				cl_log(LOG_ERR
				,	"api_nodelist: "
				"cannot mod field/5");
				return I_API_IGN;
			}
			if (ha_msg_mod(resp, F_APIRESULT
			,	(j == last ? API_OK : API_MORE))
			!=	HA_OK) {
				cl_log(LOG_ERR
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
		const char *		savedstat;

		if ((cnode = ha_msg_value(msg, F_NODENAME)) == NULL
		|| (node = lookup_node(cnode)) == NULL) {
			*failreason = "EINVAL";
			return I_API_BADREQ;
		}
		if (ha_msg_add(resp, F_STATUS, node->status) != HA_OK) {
			cl_log(LOG_ERR
			,	"api_nodestatus: cannot add field");
			return I_API_IGN;
		}
		/* Give them the "real" (non-delayed) status */
		if (node->saved_status_msg
		&&	(savedstat
		=	ha_msg_value(node->saved_status_msg, F_STATUS))) {
			ha_msg_mod(resp, F_STATUS, savedstat);
		}
		return I_API_RET;
}

/**********************************************************************
 * API_NODETYPE: Return the type of the given node
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
		cl_log(LOG_ERR
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
 		for(j=0; (lnk = &node->links[j], lnk->name); ++j) {
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
				cl_log(LOG_ERR
				,	"api_iflist: "
				"cannot mod field/1");
				return I_API_IGN;
			}
			if (ha_msg_mod(resp, F_APIRESULT
			,	(j == last ? API_OK : API_MORE))
			!=	HA_OK) {
				cl_log(LOG_ERR
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

 	for(j=0; (lnk = &node->links[j], lnk->name); ++j) {
		if (strcmp(lnk->name, node->nodename) == 0) {
			if (ha_msg_mod(resp, F_IFNAME
			,	lnk->name) != HA_OK) {
				cl_log(LOG_ERR
				,	"api_ping_iflist: "
				"cannot mod field/1");
				return I_API_IGN;
			}
			if (ha_msg_mod(resp, F_APIRESULT, API_OK)!= HA_OK) {
				cl_log(LOG_ERR
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
		cl_log(LOG_ERR
		,	"api_ifstatus: cannot add field/1");
		cl_log(LOG_ERR
		,	"name: %s, value: %s (if=%s)"
		,	F_STATUS, iface->status, ciface);
		return I_API_IGN;
	}
	return I_API_RET;
}

/**********************************************************************
 * API_CLIENTSTATUS: Return the status of the given client on a node
 *********************************************************************/

static int
api_clientstatus(const struct ha_msg* msg, struct ha_msg* resp
,	client_proc_t* client, const char** failreason)
{
	const char *		cnode;
	const char *		cname;
	const char *		our_clientid;
	struct node_info *	node;
	struct ha_msg *		m;
	int			ret = HA_FAIL;

	if ((cnode = ha_msg_value(msg, F_NODENAME)) == NULL
	|| (cname = ha_msg_value(msg, F_CLIENTNAME)) == NULL
	|| (our_clientid = ha_msg_value(msg, F_FROMID)) == NULL
	|| (node = lookup_node(cnode)) == NULL) {
		*failreason = "EINVAL";
		return I_API_BADREQ;
	}
	if (ha_msg_add(resp, F_SUBTYPE, T_RCSTATUS) != HA_OK) {
		ha_log(LOG_ERR,	"api_clientstatus: cannot add field");
		*failreason = "ENOMEM";
		return I_API_BADREQ;
	}
	/* returns client status on local node */
	if (strcmp(cnode, curnode->nodename) == 0) {

		if (find_client(cname, NULL) != NULL)
			ret = ha_msg_add(resp, F_CLIENTSTATUS, ONLINESTATUS);
		else
			ret = ha_msg_add(resp, F_CLIENTSTATUS, OFFLINESTATUS);

		if (ret != HA_OK) {
			ha_log(LOG_ERR,	"api_clientstatus: cannot add field");
			*failreason = "ENOMEM";
			return I_API_BADREQ;
		}
		return I_API_RET;
	}

	if (strcmp(node->status, ACTIVESTATUS) != 0) {
		if (ha_msg_add(resp, F_CLIENTSTATUS, OFFLINESTATUS) != HA_OK) {
			ha_log(LOG_ERR,	"api_clientstatus: cannot add field");
			*failreason = "ENOMEM";
			return I_API_BADREQ;
		}
		return I_API_RET;
	}
	if ((m = ha_msg_new(0)) == NULL
	||	ha_msg_add(m, F_TYPE, T_QCSTATUS) != HA_OK
	||	ha_msg_add(m, F_TO, cnode) != HA_OK
	||	ha_msg_add(m, F_CLIENTNAME, cname) != HA_OK
	|| 	ha_msg_add(m, F_FROMID, our_clientid) != HA_OK) {

		ha_log(LOG_ERR, "api_clientstatus: cannot add field");
		*failreason = "ENOMEM";
		return I_API_BADREQ;
	}
	if (send_cluster_msg(m) != HA_OK) {
		ha_log(LOG_ERR, "api_clientstatus: send_cluster_msg failed");
		*failreason = "ECOMM";
		return I_API_BADREQ;
	}
	/*
	 * Here we return I_API_IGN because currently we don't know
	 * the answer yet.
	 */
	return I_API_IGN;
}
/**********************************************************************
 * API_NUM_NODES: Return the number of normal nodes
 *********************************************************************/

static int
api_num_nodes(const struct ha_msg* msg, struct ha_msg* resp
	      ,	client_proc_t* client, const char** failreason)
{
	int ret;
	int num_nodes = 0;
	int i;

	for( i = 0; i < config->nodecount; i++){
		if (config->nodes[i].nodetype == NORMALNODE_I){
			num_nodes++;
		}
	}

	ret = ha_msg_add_int(resp, F_NUMNODES, num_nodes);
	if (ret != HA_OK){
		cl_log(LOG_ERR, "%s: adding num_nodes field failed",
		       __FUNCTION__);
		*failreason= "adding msg field failed";
		return I_API_BADREQ;
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
			cl_log(LOG_ERR
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
		*failreason = "Resource is managed by crm."
			"Use crm tool to query resource";
		return I_API_BADREQ;
	}

	ret = hb_rsc_resource_state();
	if (ha_msg_mod(resp, F_RESOURCES, ret) != HA_OK) {
		cl_log(LOG_ERR
		,	"api_get_resources: cannot add " F_RESOURCES
		" field to message");
	}
	return I_API_RET;
}


static int
api_get_uuid (const struct ha_msg* msg, struct ha_msg* resp,
	      client_proc_t* client, const char** failreason)
{
	const char*	query_nodename;
	cl_uuid_t	uuid;
	
	if ((query_nodename = ha_msg_value(msg, F_QUERYNAME))== NULL){
		*failreason = "no query node name found";
		return I_API_BADREQ;
	}
	
	if (nodename2uuid(query_nodename, &uuid) != HA_OK){
		return I_API_RET;
	}
	
	if (cl_msg_moduuid(resp, F_QUERYUUID, 
			   &uuid) != HA_OK){
		cl_log(LOG_ERR, "api_get_uuid: cannnot add"
		       F_QUERYUUID " field to message");
		return I_API_RET;
	}
	
	return I_API_RET;
}


static int
api_get_nodename(const struct ha_msg* msg, struct ha_msg* resp,
		 client_proc_t* client, const char** failreason)
{
	const char*	nodename;
	cl_uuid_t	query_uuid;

	if (cl_get_uuid(msg, F_QUERYUUID, &query_uuid) != HA_OK){
		*failreason = "no query node name found";
		return I_API_BADREQ;
	}
	
	if ((nodename = uuid2nodename(&query_uuid)) == NULL){
		cl_log(LOG_ERR, "api_get_nodename: nodename not found"
		       " in map table");
		return I_API_RET;
	}
	
	if (ha_msg_mod(resp, F_QUERYNAME, 
		       nodename) != HA_OK){
		cl_log(LOG_ERR, "api_get_nodename: cannnot add"
		       F_QUERYNAME " field to message");
		return I_API_RET;
	}
	
	return I_API_RET;	
	
}


static int
api_set_sendqlen(const struct ha_msg* msg, struct ha_msg* resp,
		 client_proc_t* client, const char** failreason)
{
	int length;
	int ret = ha_msg_value_int(msg, F_SENDQLEN, &length);
	

	if (ret != HA_OK){
		cl_log(LOG_ERR, "api_set_sendqlen: getting field F_SENDQLEN failed");
		return I_API_IGN;	
	}

	if (length <= 0){
		cl_log(LOG_ERR, "api_set_sendqlen: invalid length value(%d)", 
		       length);
		return I_API_IGN;
	}

	cl_log(LOG_INFO, "the send queue length from heartbeat to client %s "
	       "is set to %d",  client->client_id, length);
	
	client->chan->ops->set_send_qlen(client->chan,length); 


	return I_API_IGN;
	
}

static int
add_client_gen(client_proc_t* client, struct ha_msg* msg)
{
	char buf[MAX_CLIENT_GEN];
	
	memset(buf, 0, MAX_CLIENT_GEN);
	snprintf(buf, MAX_CLIENT_GEN, "%d", client->cligen);	
	if (ha_msg_add(msg, F_CLIENT_GENERATION, buf) != HA_OK){
		cl_log(LOG_ERR, "api_send_client_status: cannot add fields");
		ha_msg_del(msg); msg=NULL;
		return HA_FAIL;
	}
	
	return HA_OK;
	
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
		cl_log(LOG_ERR, "api_process_request: bad message type");
		goto freeandexit;
	}

	/* Things that aren't T_APIREQ are general packet xmit requests... */
	if (strcmp(msgtype, T_APIREQ) != 0) {

		/* Only named clients can send out packets to clients */

		if (fromclient->iscasual) {
			cl_log(LOG_INFO, "api_process_request: "
			"general message from casual client!");
			/* Bad Client! */
			fromclient->removereason = "badclient";
			goto freeandexit;
		}

		/* We put their client ID info in the packet as the F_FROMID*/
		if (ha_msg_mod(msg, F_FROMID, fromclient->client_id) !=HA_OK){
			cl_log(LOG_ERR, "api_process_request: "
			"cannot add F_FROMID field");
			goto freeandexit;
		}
		/* Is this too restrictive? */
		/* We also put their client ID info in the packet as F_TOID */

		/*
		 * N.B.:  This restriction exists because of security concerns
		 * It would be imprudent to remove it without a lot
		 * of thought and proof of safety.   In fact, as of right
		 * now, without some enhancements, it would be quite unsafe.
		 *
		 * The right way to talk to something that's has an API
		 * and is running on every machine is probably to use it's
		 * API, and not talk crosstalk through the core heartbeat
		 * messaging service, and compromise interface safety for
		 * every application using the API.
 		 * (ALR - 17 January 2004)
		 */

 		if (ha_msg_mod(msg, F_TOID, fromclient->client_id) != HA_OK) {
 			cl_log(LOG_ERR, "api_process_request: "
 			"cannot add F_TOID field");
 			goto freeandexit;
 		}
		if (DEBUGDETAILS) {
			cl_log(LOG_DEBUG, "Sending API message to cluster...");
			cl_log_message(LOG_DEBUG, msg);
		}

		/* Mikey likes it! */
		if (add_client_gen(fromclient, msg) != HA_OK){
			cl_log(LOG_ERR, "api_process_request: "
			       " add client generation to ha_msg failed ");			
		}

		if (send_cluster_msg(msg) != HA_OK) {
			cl_log(LOG_ERR, "api_process_request: "
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
		cl_log(LOG_ERR, "api_process_request: no fromid/pid/reqtype"
		" in message.");
		goto freeandexit;
	}

	/*
	 * Create the response message
	 */
	if ((resp = ha_msg_new(4)) == NULL) {
		cl_log(LOG_ERR, "api_process_request: out of memory/1");
		goto freeandexit;
	}

	/* API response messages are of type T_APIRESP */
	if (ha_msg_add(resp, F_TYPE, T_APIRESP) != HA_OK) {
		cl_log(LOG_ERR, "api_process_request: cannot add field/2");
		goto freeandexitresp;
	}
	/* Echo back the type of API request we're responding to */
	if (ha_msg_add(resp, F_APIREQ, reqtype) != HA_OK) {
		cl_log(LOG_ERR, "api_process_request: cannot add field/3");
		goto freeandexitresp;
	}


	if ((client = find_client(fromid, pid)) == NULL) {
		cl_log(LOG_ERR, "api_process_request: msg from non-client");
		goto freeandexitresp;
	}

	/* See if they correctly stated their client id information... */
	if (client != fromclient) {
		cl_log(LOG_ERR, "Client mismatch! (impersonation?)");
		cl_log(LOG_INFO, "pids (%ld vs %ld), Client IDs (%s vs %s)"
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
					cl_log(LOG_ERR
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
	cl_log(LOG_ERR, "Unknown API request");

	/* Common error return handling */
bad_req:
	cl_log(LOG_ERR, "api_process_request: bad request [%s]"
	,	reqtype);
	cl_log_message(LOG_ERR, msg);
	if (ha_msg_add(resp, F_APIRESULT, API_BADREQ) != HA_OK) {
		cl_log(LOG_ERR
		,	"api_process_request: cannot add field/11");
		goto freeandexitresp;
	}
	if (failreason) {
		if (ha_msg_add(resp, F_COMMENT,	failreason) != HA_OK) {
			cl_log(LOG_ERR
			,	"api_process_request: cannot add failreason");
		}
	}
	api_send_client_msg(client, resp);
freeandexitresp:
	ha_msg_del(resp);
	resp=NULL;
freeandexit:
	if (msg != NULL) {
		ha_msg_del(msg); msg=NULL;
	}
}

/* Process a registration request from a potential client */
void
process_registerevent(IPC_Channel* chan,  gpointer user_data)
{
	client_proc_t*	client;

	if ((client = MALLOCT(client_proc_t)) == NULL) {
		cl_log(LOG_ERR
		,	"unable to add client [no memory]");
		chan->ops->destroy(chan);
		return;
	}
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"process_registerevent() {");
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
	G_main_setdescription((GSource*)client->gsource, "API client");
	G_main_setmaxdispatchdelay((GSource*)client->gsource, config->heartbeat_ms);
	G_main_setmaxdispatchtime((GSource*)client->gsource, 100);
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"client->gsource = 0x%lx"
		,	(unsigned long)client->gsource);
	}

	client->next = client_list;
	client_list = client;
	total_client_count++;
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"}/*process_registerevent*/;");
	}
}

static void     
destroy_pair(gpointer key, gpointer value, gpointer user_data)
{
	if(value){
		cl_free(value);
	}
}
static void
destroy_seq_snapshot_table(GHashTable* table) 
{
	if (ANYDEBUG){
		cl_log(LOG_DEBUG,
		       "Destroying seq snapshot hash table");
	}

	if(table){
		g_hash_table_foreach(table, destroy_pair, NULL);
		g_hash_table_destroy(table);
	}
	return ;
}
static int
create_seq_snapshot_table(GHashTable** ptable) 
{
	GHashTable*		table = NULL;
	int			i;
	
	if ( !ptable){
		cl_log(LOG_ERR, "create_seq_snapshot_table: "
		       "nvalid arguments");
		return HA_FAIL;		
	}
	
	*ptable = NULL;
	for (i = 0 ; i < config->nodecount; i++){
	
		struct node_info*	node = &config->nodes[i];
		struct seqtrack*	t = &node->track;


		if (cl_uuid_is_null(&node->uuid)){
			continue;
		}

		if (t->nmissing > 0){
			struct seq_snapshot* snapshot;
			
			snapshot = (struct seq_snapshot*) 
				cl_malloc(sizeof(struct seq_snapshot));
			
			if (snapshot == NULL){
				cl_log(LOG_ERR, "allocating memory for"
				       " seq_snapshot failed");
				return HA_FAIL;
			}
		
			snapshot->last_seq = t->last_seq;
			snapshot->generation = t->generation;

			if (table == NULL){
				if (ANYDEBUG){
					cl_log(LOG_DEBUG,
					       "Creating seq snapshot hash table");
				}
				table = g_hash_table_new(uuid_hash, uuid_equal);
				if (table == NULL){
					cl_log(LOG_ERR, "creating hashtable for"
					       " seq_snapshot failed");
					return HA_FAIL;
				}
			}
			if (ANYDEBUG){
				cl_log(LOG_DEBUG,
				       "Creating one entry in seq snapshot hash table"
				       "for node %s", node->nodename);
			}
			
			g_hash_table_insert(table, &node->uuid, snapshot);
			
		}else{
			if (ANYDEBUG){
				cl_log(LOG_DEBUG,
				       "create_seq_snapshot_table:"
				       "no missing packets found for "
				       "node %s", node->nodename);
			}	
		}
	}
	
	*ptable = table;
	
	return HA_OK;
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
	struct ha_msg *	resp = NULL;
	client_proc_t*	fcli;
	
	char		deadtime[64];
	char		keepalive[64];
	char		logfacility[64];


	/*set the client generation*/
	client->cligen =  client_generation++;

	if (msg == NULL
	||	(msgtype = ha_msg_value(msg, F_TYPE)) == NULL
	||	(reqtype = ha_msg_value(msg, F_APIREQ)) == NULL
	||	strcmp(msgtype, T_APIREQ) != 0
	||	strcmp(reqtype, API_SIGNON) != 0)  {
		cl_log(LOG_ERR, "api_process_registration_msg: bad message");
		cl_log_message(LOG_ERR, msg);
		goto del_msg;
	}
	fromid = ha_msg_value(msg, F_FROMID);
	pid = ha_msg_value(msg, F_PID);

	if (fromid == NULL && pid == NULL) {
		cl_log(LOG_ERR
		,	"api_process_registration_msg: no fromid in msg");
		goto del_msg;
	}
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"api_process_registration_msg(%s, %s, %s)"
		,	msgtype, pid, (fromid==NULL ?"nullfrom" : fromid));
	}

	/*
	 *	Create the response message
	 */
	if ((resp = ha_msg_new(4)) == NULL) {
		cl_log(LOG_ERR
		,	"api_process_registration_msg: out of memory/1");
		goto del_msg;
	}
	if (ha_msg_add(resp, F_TYPE, T_APIRESP) != HA_OK) {
		cl_log(LOG_ERR
		,	"api_process_registration_msg: cannot add field/2");
		goto del_rsp_and_msg;
	}
	if (ha_msg_add(resp, F_APIREQ, reqtype) != HA_OK) {
		cl_log(LOG_ERR
		,	"api_process_registration_msg: cannot add field/3");
		goto del_rsp_and_msg;
	}

	client->pid = atoi(pid);
	/*
	 *	Sign 'em up.
	 */
	if (!api_add_client(client, msg)) {
		cl_log(LOG_ERR
		       ,	"api_process_registration_msg: cannot add client(%s)"
		       ,	client->client_id);
	}

	/* Make sure we can find them in the table... */
	if ((fcli = find_client(fromid, pid)) == NULL) {
		cl_log(LOG_ERR
		,	"api_process_registration_msg: cannot find client");
		/* We can't properly reply to them. They'll hang. Sorry... */
		goto del_rsp_and_msg;
	}
	if (fcli != client) {
		cl_log(LOG_ERR
		,	"api_process_registration_msg: found wrong client");
		goto del_rsp_and_msg;
		return;
	}

	/*everything goes well, 
	  now create a table to record sequence/generation number
	  for each node if necessary*/
	
	client->seq_snapshot_table = NULL;
	if (create_seq_snapshot_table(&client->seq_snapshot_table) != HA_OK){
		cl_log(LOG_ERR, "api_process_registration_msg: "
		       " creating seq snapshot table failed");
		return;
	}

	if (ha_msg_mod(resp, F_APIRESULT, API_OK) != HA_OK) {
		cl_log(LOG_ERR
		,	"api_process_registration_msg: cannot add field/4");
		goto del_rsp_and_msg;
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
		cl_log(LOG_ERR, "api_process_registration_msg: cannot add field/4");
		goto del_rsp_and_msg;
	}
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "Signing on API client %ld (%s)"
		,	(long) client->pid
		,	(client->iscasual? "'casual'" : client->client_id));
	}
	api_send_client_msg(client, resp);
del_rsp_and_msg:
	if (resp != NULL) {
		ha_msg_del(resp); resp=NULL;
	}
del_msg:
	if (msg != NULL) {
		ha_msg_del(msg); msg=NULL;
	}
}



static void
api_send_client_status(client_proc_t* client, const char * status
,	const char *	reason)
{
	struct ha_msg*		msg;
	if (client->iscasual) {
		return;
	}

	/*
	 * Create the status message
	 */
	if ((msg = ha_msg_new(4)) == NULL) {
		cl_log(LOG_ERR, "api_send_client_status: out of memory/1");
		return;
	}
	
	if (ha_msg_add(msg, F_TYPE, T_APICLISTAT) != HA_OK
	||	ha_msg_add(msg, F_STATUS, status) != HA_OK
	||	ha_msg_add(msg, F_FROMID, client->client_id) != HA_OK
	||	ha_msg_add(msg, F_TOID, client->client_id) != HA_OK
	||	ha_msg_add(msg, F_ORIG, curnode->nodename) != HA_OK
	||	(reason != NULL && ha_msg_add(msg, F_COMMENT, reason)
	!= HA_OK)) {
		cl_log(LOG_ERR, "api_send_client_status: cannot add fields");
		ha_msg_del(msg); msg=NULL;
		return;
	}
	
	if (add_client_gen(client, msg) != HA_OK){
		cl_log(LOG_ERR, "api_send_client_status: cannot add fields");
		ha_msg_del(msg); msg=NULL;
		return;
	}
	
	if (strcmp(status, LEAVESTATUS) == 0) {
		/* Make sure they know they're signed off... */
		api_send_client_msg(client, msg);
	}
	
	if (send_cluster_msg(msg) != HA_OK) {
		cl_log(LOG_ERR, "api_send_client_status: "
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
			if (client->chan->failreason[0] == EOS){
				client->removereason = "sendfail";
			}else {
				client->removereason = client->chan->failreason;
			}
		}
	}

	if (CL_KILL(client->pid, client->signal) < 0 && errno == ESRCH) {
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG, "api_send_client: client %ld died"
			,	(long) client->pid);
		}
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
	if ((client = find_client(NULL, cpid)) == NULL) {
		return 0;
	}

	client->removereason = reason;
	if (client->chan->recv_queue->current_qlen > 0) {
		cl_log(LOG_ERR
		,	"%s client %d disconnected, with %d messages pending"
		,	__FUNCTION__, client->pid
		       ,	(int)client->chan->recv_queue->current_qlen);
	}

	G_main_del_IPC_Channel(client->gsource);
	/* Should trigger G_remove_client (below) */
	return 1;
}
static void
G_remove_client(gpointer Client)
{
	client_proc_t*	client = Client;
	const char *	reason;

	reason = client->removereason ? client->removereason : "?";
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"G_remove_client(pid=%d, reason='%s' gsource=0x%lx) {"
		,	client->pid, reason, (unsigned long) client->gsource);
	}

	api_remove_client_int(client, reason);
	if (ANYDEBUG) {
		cl_log(LOG_DEBUG
		,	"}/*G_remove_client;*/");
	}
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

		

	--total_client_count;

	if ((req->desired_types & DEBUGTREATMENTS) != 0) {
		--debug_client_count;
	}

	/* Locate the client data structure in our list */

	for (client=client_list; client != NULL; client=client->next) {
		/* Is this the client? */
		if (client->pid == req->pid) {
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				,	"api_remove_client_int: removing"
				" pid [%ld] reason: %s"
				,	(long)req->pid, reason);
			}
			if (prev == NULL) {
				client_list = client->next;
			}else{
				prev->next = client->next;
			}

			break;
		}
		prev = client;
	}
	

	if (req == client){
		
		api_send_client_status(req, LEAVESTATUS, reason);
		/* Zap! */
		memset(client, 0, sizeof(*client));
		ha_free(client); client = NULL;

	}else{
		cl_log(LOG_ERR,	"api_remove_client_int: could not find pid [%ld]"
		       ,	(long) req->pid);
	}
	
	return;
}


/* Validate client credentials against the real world */
 
static gboolean
api_check_client_credentials(client_proc_t* client, uid_t uid, gid_t gid)
{
	IPC_Auth	auth;
	guint		id;
	int		one = 1;
	gboolean	result = TRUE;

	int auth_result = IPC_FAIL;
	GHashTable*	uidlist
	=		g_hash_table_new(g_direct_hash, g_direct_equal);

	id = (guint) uid;
	g_hash_table_insert(uidlist, GUINT_TO_POINTER(id), &one);
	auth.uid = uidlist;
	auth.gid = NULL;
	auth_result = client->chan->ops->verify_auth(client->chan, &auth);

	if (auth_result != IPC_OK) {
		result = FALSE;

	} else {
		GHashTable*	gidlist
		=		g_hash_table_new(g_direct_hash
		,		g_direct_equal);
		id = (guint) gid;
		g_hash_table_insert(gidlist, GUINT_TO_POINTER(id), &one);
		auth.uid = NULL;
		auth.gid = gidlist;

		auth_result = client->chan->ops->verify_auth(
			client->chan, &auth);
		
		if (auth_result != IPC_OK && auth_result != IPC_BROKEN) {
			result = FALSE;
		}
		g_hash_table_destroy(gidlist);
	}
	g_hash_table_destroy(uidlist);

	return result;
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
	const char *	cgid = NULL;
	const char *	cuid = NULL;
	long		luid = -1;
	long		lgid = -1;
	int		uid = -1;
	int		gid = -1;

	
	if ((cpid = ha_msg_value(msg, F_PID)) != NULL) {
		pid = atoi(cpid);
	}
	if (pid <= 0  || (CL_KILL(pid, 0) < 0 && errno == ESRCH)) {
		cl_log(LOG_WARNING
		,	"api_add_client: bad pid [%ld]", (long) pid);
		return FALSE;
	}

	fromid = ha_msg_value(msg, F_FROMID);

	
	if (find_client(fromid, NULL) != NULL) {
		cl_log(LOG_WARNING
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

	/* Encourage better realtime behavior by heartbeat */
	client->chan->ops->set_recv_qlen(client->chan, 0);

	if ((cuid = ha_msg_value(msg, F_UID)) == NULL
	||	(cgid = ha_msg_value(msg, F_GID)) == NULL
	||	sscanf(cuid, "%ld", &luid) != 1
	||	sscanf(cgid, "%ld", &lgid) != 1) {
		cl_log_message(LOG_ERR, msg);
		client->removereason = "invalid id info";
		cl_log(LOG_ERR, "Client user/group id is incorrect"
		" [%s] => %ld [%s] => %ld"
		,	cuid == NULL ? "<null>" : cuid, luid
		,	cgid == NULL ? "<null>" : cgid, lgid);
		return FALSE;
	}

	uid = (uid_t)luid;
	gid = (gid_t)lgid;

	if (!api_check_client_credentials(client, uid, gid)) {
		client->removereason = "incorrect/false credentials";
		return FALSE;
	}
	client->uid = uid;
	client->gid = gid;
	if (api_check_client_authorization(client)) {
		api_send_client_status(client, JOINSTATUS, API_SIGNON);
	}else{
		cl_log(LOG_WARNING
		,	"Client [%s] pid %d failed authorization [%s]"
		,	client->client_id, pid, client->removereason);
		return FALSE;
	}
	return TRUE;
}

static gboolean
api_check_client_authorization(client_proc_t* client)
{
	gpointer	gauth = NULL;
	IPC_Auth*	auth;
	int auth_result = IPC_FAIL;
	
	/* If client is casual, or type of client not in authorization table
	 * then default to the "default" authorization category.
	 * otherwise, use the client type's authorization list
	 */
	if (client->iscasual){
		gauth = g_hash_table_lookup(APIAuthorization, "anon");
		if (gauth == NULL){
			cl_log(LOG_ERR, "NO auth found for anonymous");
			return FALSE;
		}
		
	}else if((gauth = g_hash_table_lookup(APIAuthorization,
					client->client_id)) ==  NULL) {
		if ((gauth = g_hash_table_lookup(APIAuthorization, "default"))
		== NULL) {
			client->removereason = "no default client auth";
			return FALSE;
		}
	}
	
	auth = gauth;
	if ((long)auth->gid == (long)-1L) {
		cl_log(LOG_DEBUG, "Darn!  -1 gid ptr in api_check_client_authorization");
		abort();
	}
	if (ANYDEBUG) {

		cl_log(LOG_DEBUG
		,	"Checking client authorization for client %s (%ld:%ld)"
		,	client->client_id
		,	(long)client->uid, (long)client->gid);
	}

	auth_result = client->chan->ops->verify_auth(
			client->chan, auth);
	
	if (auth_result == IPC_OK) {
#ifndef GETPID_INCONSISTENT
		if (client->chan->farside_pid > 0) {
			if (client->chan->farside_pid != client->pid) {
				client->removereason = "pid mismatch";
				cl_log(LOG_INFO
				,	"PID mismatch: %d vs farside_pid: %d"
				,	client->pid
				,	client->chan->farside_pid);
				return FALSE;
			}
		}
#endif
		return TRUE;

	} else if(auth_result == IPC_BROKEN) {
		return TRUE;
	}
	
	client->removereason = "client failed authorization";
	return FALSE;
}


/*
 *	Find the client that goes with this client id/pid
 */
client_proc_t*
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
	gboolean	ret = TRUE;


	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"APIclients_input_dispatch() {");
	}
	if (chan != client->chan) {
		/* Bad boojum! */
		cl_log(LOG_ERR
		,	"APIclients_input_dispatch chan mismatch");
		ret = FALSE;
		goto getout;
	}
	if (client->removereason) {
		ret = FALSE;
		goto getout;
	}

	/* Process a single API client request */
	client->isindispatch = TRUE;
	ProcessAnAPIRequest(client);
	client->isindispatch = FALSE;

	if (client->removereason) {
		ret = FALSE;
		goto getout;
	}

getout:
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"return %d;", ret);
		cl_log(LOG_DEBUG
		,	"}/*APIclients_input_dispatch*/;");
	}
	return ret;
}


static gboolean all_clients_running = TRUE;
gboolean
all_clients_pause(void)
{
	client_proc_t* client;

	if (!all_clients_running ){
		return TRUE;
	}
	
	cl_log(LOG_INFO, "all clients are now paused");
		
	for (client=client_list; client != NULL; client=client->next) {
		G_main_IPC_Channel_pause(client->gsource);
		
	}	
	all_clients_running = FALSE;
	return TRUE;
}

gboolean
all_clients_resume(void)
{
	client_proc_t* client;
	
	if (all_clients_running ){
		return TRUE;
	}
	
	cl_log(LOG_INFO, "all clients are now resumed");
	
	for (client=client_list; client != NULL; client=client->next) {
		G_main_IPC_Channel_resume(client->gsource);		
	}	
	
	all_clients_running = TRUE;
	
	return TRUE;
}

gboolean
ProcessAnAPIRequest(client_proc_t*	client)
{
	struct ha_msg*	msg;
	static int	consecutive_failures = 0;
	gboolean	rc = FALSE;

	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"ProcessAnAPIRequest() {");
	}


	if (!client->chan->ops->is_message_pending(client->chan)) {
		goto getout;
	}
	
	/* See if we can read the message */
	if ((msg = msgfromIPC(client->chan, 0)) == NULL) {
		
		/* EOF? */
		if (client->chan->ch_status == IPC_DISCONNECT) {
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG
				,	"EOF from client pid %ld"
				,	(long)client->pid);
			}
			client->removereason = "EOF";
			goto getout;
		}

		/* None of the above... */
		cl_log(LOG_INFO, "No message from pid %ld"
		,	(long)client->pid);
		++consecutive_failures;
		/*
		 * This used to happen because of EOF,
		 * which is now handled above.  This is
		 * good protection to have anyway ;-)
		 */
		if (consecutive_failures >= 10) {
			cl_log(LOG_ERR
			,	"Removing client pid %ld"
			,	(long)client->pid);
			client->removereason = "noinput";
			consecutive_failures = 0;
		}
		goto getout;
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
	rc = TRUE;
	
	if (!all_clients_running){
		/* This is a new client,
		 * this allows a client to sign on but further action will be blocked
		 */
		G_main_IPC_Channel_pause(client->gsource);
		rc = FALSE;
	}

getout:
	/* May have gotten a message from 'client' */
	if (CL_KILL(client->pid, 0) < 0 && errno == ESRCH) {
		/* Oops... he's dead */
		if (ANYDEBUG) {
			cl_log(LOG_DEBUG
			,	"Client pid %ld died (input)"
			,	(long)client->pid);
		}
		client->removereason = "died";
	}
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG, "\treturn %s;"
		,	(rc ? "TRUE" : "FALSE"));
		cl_log(LOG_DEBUG, "}/*ProcessAnAPIRequest*/;");
	}

	return rc;
}
