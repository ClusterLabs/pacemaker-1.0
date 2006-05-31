/* File: stonithd_lib.c
 * Description: Client library to STONITH deamon.
 *
 * Author: Sun Jiang Dong <sunjd@cn.ibm.com>
 * Copyright (c) 2004 International Business Machines
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <glib.h>
#include <ha_msg.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/GSource.h>
#include <clplumbing/uids.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/proctrack.h>
#include <fencing/stonithd_api.h>
#include <fencing/stonithd_msg.h>

static const char * CLIENT_NAME = NULL;
static pid_t CLIENT_PID = 0;
static char CLIENT_PID_STR[16];
static gboolean DEBUG_MODE	   = FALSE;
static IPC_Channel * chan	   = NULL;

static gboolean INT_BY_ALARM = FALSE;
static unsigned int DEFAULT_TIMEOUT = 6;

/* Must correspond to stonith_type_t */
/* Not use it yet 
static const char * stonith_op_strname[] =
{
         "QUERY", "RESET", "POWERON", "POWEROFF"
};
*/

static stonith_ops_callback_t stonith_ops_cb = NULL; 
static stonithRA_ops_callback_t stonithRA_ops_cb = NULL;
static void * stonithRA_ops_cb_private_data = NULL;

static struct ha_msg * create_basic_reqmsg_fields(const char * apitype);
static gboolean is_expected_msg(const struct ha_msg * msg,
				const char * field_name1,
				const char * field_content1,
				const char * field_name2,
				const char * field_content2 );
static int chan_waitin_timeout(IPC_Channel * chan, unsigned int timeout);
static int chan_waitout_timeout(IPC_Channel * chan, unsigned int timeout);
static void sigalarm_handler(int signum);
static void stdlib_log(int priority, const char * fmt, ...)G_GNUC_PRINTF(2,3);
static void free_stonith_ops_t(stonith_ops_t * st_op);
static void free_stonithRA_ops_t(stonithRA_ops_t * ra_op);

int
stonithd_signon(const char * client_name)
{
	int rc = ST_FAIL;
	char	path[] = IPC_PATH_ATTR;
	char	sock[] = STONITHD_SOCK;
	struct  ha_msg * request;
	struct  ha_msg * reply;
	GHashTable *	 wchanattrs;
	uid_t	my_euid;
	gid_t	my_egid;
	const char * tmpstr;

	if (chan == NULL || chan->ch_status == IPC_DISCONNECT) {
		wchanattrs = g_hash_table_new(g_str_hash, g_str_equal);
        	g_hash_table_insert(wchanattrs, path, sock);
		/* Connect to the stonith deamon */
		chan = ipc_channel_constructor(IPC_ANYTYPE, wchanattrs);
		g_hash_table_destroy(wchanattrs);
	
		if (chan == NULL) {
			stdlib_log(LOG_ERR, "stonithd_signon: Can't connect "
				   " to stonithd");
			return ST_FAIL;
		}

	        if (chan->ops->initiate_connection(chan) != IPC_OK) {
			stdlib_log(LOG_ERR, "stonithd_signon: Can't initiate "
				   "connection to stonithd");
	                return ST_FAIL;
       		}
	}

	CLIENT_PID = getpid();
	snprintf(CLIENT_PID_STR, sizeof(CLIENT_PID_STR), "%d", CLIENT_PID);
	if ( client_name != NULL ) {
		CLIENT_NAME = client_name;
	} else {
		CLIENT_NAME = CLIENT_PID_STR;
	}

	if ( (request = create_basic_reqmsg_fields(ST_SIGNON)) == NULL) {
		return ST_FAIL;
	}

	/* important error check client name length */
	my_euid = geteuid();
	my_egid = getegid();
	if (  (	ha_msg_add_int(request, F_STONITHD_CEUID, my_euid) != HA_OK )
	    ||(	ha_msg_add_int(request, F_STONITHD_CEGID, my_egid) != HA_OK )
	   ) {
		stdlib_log(LOG_ERR, "stonithd_signon: "
			   "cannot add field to ha_msg.");
		ZAPMSG(request);
		return ST_FAIL;
	}

	/* Send the registration request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		ZAPMSG(request);
		stdlib_log(LOG_ERR, "can't send signon message to IPC");
		return ST_FAIL;
	}

	/* waiting for the output to finish */
	chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	ZAPMSG(request);

	/* Read the reply... */
	stdlib_log(LOG_DEBUG, "waiting for the signon reply msg.");
        if ( IPC_OK != chan_waitin_timeout(chan, DEFAULT_TIMEOUT) ) {
		stdlib_log(LOG_ERR, "waitin failed."); /*  how to deal. important */
		return ST_FAIL;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_signon: to fetch reply failed.");
		return ST_FAIL;
	}
	
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RSIGNON) ) {
		if ( ((tmpstr=cl_get_string(reply, F_STONITHD_APIRET)) != NULL)
	   	    && (STRNCMP_CONST(tmpstr, ST_APIOK) == 0) ) {
			rc = ST_OK;
			stdlib_log(LOG_DEBUG, "signoned to the stonithd.");
		} else {
			stdlib_log(LOG_DEBUG, "failed to signon to the "
				   "stonithd.");
		}
	} else {
		stdlib_log(LOG_DEBUG, "stonithd_signon: "
			   "Got an unexpected message.");
		/* Handle it furtherly ? */
	}

	ZAPMSG(reply);
	return rc;
}

int 
stonithd_signoff(void)
{
	int rc = ST_FAIL;
	struct ha_msg * request, * reply;
	const char * tmpstr;
	
	if (chan == NULL || chan->ch_status == IPC_DISCONNECT) {
		stdlib_log(LOG_NOTICE, "Has been in signoff status.");
		return ST_OK;
	}

	if ( (request = create_basic_reqmsg_fields(ST_SIGNOFF)) == NULL) {
		return ST_FAIL;
	}

	/* Send the signoff request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		ZAPMSG(request);
		stdlib_log(LOG_ERR, "can't send signoff message to IPC");
		return ST_FAIL;
	}

	/*  waiting for the output to finish */
	chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	ZAPMSG(request);

	/* Read the reply... */
	stdlib_log(LOG_DEBUG, "waiting for the signoff reply msg.");
        if ( IPC_OK != chan_waitin_timeout(chan, DEFAULT_TIMEOUT) ) {
		stdlib_log(LOG_ERR, "waitin failed.");
		return ST_FAIL;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_signoff: to fetch the reply msg "
			   "failed.");
		return ST_FAIL;
	}
	
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RSIGNOFF) ) {
		if ( ((tmpstr=cl_get_string(reply, F_STONITHD_APIRET)) != NULL)
	   	    && (STRNCMP_CONST(tmpstr, ST_APIOK) == 0) ) {
			chan->ops->destroy(chan);
			chan = NULL;
			CLIENT_NAME = NULL;
			rc = ST_OK;
			stdlib_log(LOG_DEBUG, "succeeded to sign off the "
				   "stonithd.");
		} else {
			stdlib_log(LOG_NOTICE, "fail to sign off the stonithd.");
		}
	} else {
		stdlib_log(LOG_DEBUG, "stonithd_signoff: "
			   "Got an unexpected message.");
	}
	ZAPMSG(reply);
	
	return rc;
}

IPC_Channel *
stonithd_input_IPC_channel(void)
{
	if ( chan == NULL || chan->ch_status == IPC_DISCONNECT ) {
		stdlib_log(LOG_ERR, "stonithd_input_IPC_channel: not signon.");
		return NULL;
	} else {
		return chan;
	}
}

int 
stonithd_node_fence(stonith_ops_t * op)
{
	int rc = ST_FAIL;
	struct ha_msg * request, * reply;
	const char * tmpstr;

	if (op == NULL) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: op==NULL");
		return ST_FAIL;
	}
	
	if (chan == NULL || chan->ch_status == IPC_DISCONNECT) {
		stdlib_log(LOG_NOTICE, "Has been in signoff status.");
		return ST_FAIL;
	}

	if ( (request = create_basic_reqmsg_fields(ST_STONITH)) == NULL) {
		return ST_FAIL;
	}

	if (  (ha_msg_add_int(request, F_STONITHD_OPTYPE, op->optype) != HA_OK )
	    ||(ha_msg_add(request, F_STONITHD_NODE, op->node_name ) != HA_OK)
	    ||(op->node_uuid == NULL
	       || ha_msg_add(request, F_STONITHD_NODE_UUID, op->node_uuid) != HA_OK)
	    ||(ha_msg_add_int(request, F_STONITHD_TIMEOUT, op->timeout) 
		!= HA_OK) ) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "cannot add field to ha_msg.");
		ZAPMSG(request);
		return ST_FAIL;
	}
	if  (op->private_data != NULL) {
	       if ( ha_msg_add(request, F_STONITHD_PDATA, op->private_data) != HA_OK) {
			stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "Failed to add F_STONITHD_PDATA field to ha_msg.");
			ZAPMSG(request);
			return ST_FAIL;
		}
	}

	/* Send the stonith request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		ZAPMSG(request);
		stdlib_log(LOG_ERR, "can't send signoff message to IPC");
		return ST_FAIL;
	}

	/*  waiting for the output to finish */
	chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	ZAPMSG(request);
	
	/* Read the reply... */
	stdlib_log(LOG_DEBUG, "waiting for the stonith reply msg.");
        if ( IPC_OK != chan_waitin_timeout(chan, DEFAULT_TIMEOUT) ) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: waitin failed.");
		/* how to deal. important */
		return ST_FAIL;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: fail to fetch reply");
		return ST_FAIL;
	}
	
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RSTONITH) ) {
		if ( ((tmpstr = cl_get_string(reply, F_STONITHD_APIRET)) != NULL) 
	   	    && (STRNCMP_CONST(tmpstr, ST_APIOK) == 0) ) {
			rc = ST_OK;
			stdlib_log(LOG_DEBUG, "stonith msg is sent to stonithd.");
		} else {
			stdlib_log(LOG_ERR, "failed to send stonith request to "
				   "the stonithd.");
		}
	} else {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "Got an unexpected message.");
		/* Need to handle in other way? */
	}

	ZAPMSG(reply);
	return rc;
}

gboolean 
stonithd_op_result_ready(void)
{
	if ( chan == NULL || chan->ch_status == IPC_DISCONNECT ) {
		stdlib_log(LOG_ERR, "stonithd_op_result_ready: "
			   "failed due to not on signon status.");
		return FALSE;
	}
	
	/* 
	 * Regards IPC_DISCONNECT as a special result, so to prevent the caller
	 * from the possible endless waiting. That can be caused by the way
	 * in which the caller uses it.
	 */
	return (chan->ops->is_message_pending(chan)
		|| chan->ch_status == IPC_DISCONNECT);
}

int
stonithd_receive_ops_result(gboolean blocking)
{
	struct ha_msg* reply = NULL;
	const char * tmpstr = NULL;
	int tmpint = 0;
	int rc = ST_OK;

	stdlib_log(LOG_DEBUG, "stonithd_receive_ops_result: begin");

	/* If there is no msg ready and none blocking mode, then return */
	if ((stonithd_op_result_ready() == FALSE) && (blocking == FALSE)) {
		stdlib_log(LOG_DEBUG, "stonithd_receive_ops_result: "
			   "no result available.");
		return ST_OK;
	}

	if (stonithd_op_result_ready() == FALSE) {
	/* at that time, blocking must be TRUE */
		if (IPC_OK != chan->ops->waitin(chan)) {
			return ST_FAIL;
		}
	}

	reply = msgfromIPC_noauth(chan);
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_STRET)  ) {
		stonith_ops_t * st_op = NULL;

		stdlib_log(LOG_DEBUG, "received stonith final ret.");
		/* handle the stonith op result message */
		st_op = g_new(stonith_ops_t, 1);	
		st_op->node_uuid = NULL;
		st_op->private_data = NULL;
		
		if ( ha_msg_value_int(reply, F_STONITHD_OPTYPE, &tmpint)
			== HA_OK) {
			st_op->optype = tmpint;	
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply message contains no optype field.");
			rc = ST_FAIL;
		}

		if ((tmpstr = cl_get_string(reply, F_STONITHD_NODE)) != NULL) {
			st_op->node_name = g_strdup(tmpstr);
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply contains no node_name field.");
			rc = ST_FAIL;
		}

		if ((tmpstr = cl_get_string(reply, F_STONITHD_NODE_UUID)) != NULL) {
			st_op->node_uuid = g_strdup(tmpstr);

		} else {
			stdlib_log(LOG_WARNING, "stonithd_receive_ops_result: the "
				   "reply contains no node_uuid field.");
		}

		if ( ha_msg_value_int(reply, F_STONITHD_TIMEOUT, &tmpint)
			== HA_OK ) {
			st_op->timeout =  tmpint;	
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply message contains no timeout field.");
			rc = ST_FAIL;
		}

		if ( ha_msg_value_int(reply, F_STONITHD_CALLID, &tmpint)
			== HA_OK ) {
			st_op->call_id = tmpint;	
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply message contains no call_id field.");
			rc = ST_FAIL;
		}

		if (ha_msg_value_int(reply, F_STONITHD_FRC, &tmpint) == HA_OK) {
			st_op->op_result = tmpint;
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply contains no op_result field.");
			rc = ST_FAIL;
		}

		if ((tmpstr=cl_get_string(reply, F_STONITHD_NLIST)) != NULL) {
			st_op->node_list = g_strdup(tmpstr);
		} else {
			st_op->node_list = NULL;
			stdlib_log(LOG_DEBUG, "stonithd_receive_ops_result: the "
				   "reply message contains no NLIST field.");
		}
	
		if ((tmpstr=cl_get_string(reply, F_STONITHD_PDATA)) != NULL) {
			st_op->private_data = g_strdup(tmpstr);
		} else {
			stdlib_log(LOG_DEBUG, "stonithd_receive_ops_result: the "
				   "reply message contains no PDATA field.");
		}
			
		if (stonith_ops_cb != NULL) {
			stdlib_log(LOG_DEBUG, "trigger stonith op callback.");
			stonith_ops_cb(st_op);
		} else { 
			stdlib_log(LOG_DEBUG, "No stonith op callback.");
		}

		free_stonith_ops_t(st_op);
		ZAPMSG(reply);
		return ST_OK;
	}

	if (  TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL,
			     F_STONITHD_APIRPL, ST_RAOPRET ) ) {
		stonithRA_ops_t * ra_op = NULL;

		stdlib_log(LOG_DEBUG, "received stonithRA op final ret.");
		/* handle the stonithRA op result message */
		ra_op = g_new(stonithRA_ops_t, 1);

		tmpstr = cl_get_string(reply, F_STONITHD_RSCID);
		if (tmpstr != NULL) {
			ra_op->rsc_id = g_strdup(tmpstr);
			stdlib_log(LOG_DEBUG, "ra_op->rsc_id=%s.", 
				     ra_op->rsc_id);
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply message contains no rsc_id field.");
			rc = ST_FAIL;
		}

		tmpstr = cl_get_string(reply, F_STONITHD_RAOPTYPE);
		if (tmpstr != NULL) {
			ra_op->op_type = g_strdup(tmpstr);
			stdlib_log(LOG_DEBUG, "ra_op->op_type=%s.", 
				     ra_op->op_type);
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply message contains no op_type field.");
			rc = ST_FAIL;
		}

		tmpstr = cl_get_string(reply, F_STONITHD_RANAME);
		if (tmpstr != NULL) {
			ra_op->ra_name = g_strdup(tmpstr);
			stdlib_log(LOG_DEBUG, "ra_op->ra_name=%s.", 
				     ra_op->ra_name);
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply message contains no ra_name field.");
			rc = ST_FAIL;
		}

		ra_op->params = cl_get_hashtable(reply, F_STONITHD_PARAMS);
		if (ra_op->params != NULL) {
			stdlib_log(LOG_DEBUG, "ra_op->params address:=%p.", 
				     ra_op->params);
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: the "
				   "reply contains no parameter field.");
			rc = ST_FAIL;
		}	
		
		if ( ha_msg_value_int(reply, F_STONITHD_CALLID, &tmpint)
			== HA_OK ) {
			ra_op->call_id = tmpint;
			stdlib_log(LOG_DEBUG, "receive_ops_result: "
				   "ra_op->call_id=%d.", ra_op->call_id);
		} else {
			stdlib_log(LOG_ERR, "stonithd_receive_ops_result: "
				   "no call_id field in reply");
			rc = ST_FAIL;
		}

		if ( ha_msg_value_int(reply, F_STONITHD_FRC, &tmpint)
			== HA_OK ) {
			ra_op->op_result = tmpint;
			stdlib_log(LOG_DEBUG, "stonithd_receive_ops_result: "
				   "ra_op->op_result=%d.", ra_op->op_result);
		} else {
			stdlib_log(LOG_ERR, "no op_result field in reply");
			rc = ST_FAIL;
		}

		/* if ( rc == ST_OK && stonithRA_ops_cb != NULL)  */
		if ( stonithRA_ops_cb != NULL) {
			stdlib_log(LOG_DEBUG, "trigger stonithRA op callback.");
			stonithRA_ops_cb(ra_op, stonithRA_ops_cb_private_data);
		} else {
			stdlib_log(LOG_DEBUG, "No stonithRA op callback.");
		}

		free_stonithRA_ops_t(ra_op);
		ZAPMSG(reply);
		return rc;
		
	}

	ZAPMSG(reply);
	stdlib_log(LOG_DEBUG, "stonithd_receive_ops_result: "
		   "Got an unexpected message.");
	return ST_FAIL;
}

int
stonithd_set_stonith_ops_callback(stonith_ops_callback_t callback)
{
	if ( chan == NULL || chan->ch_status == IPC_DISCONNECT ) {
		stdlib_log(LOG_ERR, "stonithd_set_stonith_ops_callback: "\
		 "failed due to not on signon status.");
		return ST_FAIL;
	} else {
		stonith_ops_cb = callback;
		stdlib_log(LOG_DEBUG, "setted stonith ops callback.");
	}
	
	return ST_OK;
}

int
stonithd_virtual_stonithRA_ops( stonithRA_ops_t * op, int * call_id)
{
	int rc = ST_FAIL;
	struct ha_msg * request, * reply;
	const char * tmpstr;

	stdlib_log(LOG_DEBUG, "stonithd_virtual_stonithRA_ops: begin");

	if (op == NULL) {
		stdlib_log(LOG_ERR, "stonithd_virtual_stonithRA_ops: op==NULL");
		return ST_FAIL;
	}
	
	if (call_id == NULL) {
		stdlib_log(LOG_ERR, "stonithd_stonithd_stonithRA_ops: "
			   "call_id==NULL");
		return ST_FAIL;
	}
	
	if (chan == NULL || chan->ch_status == IPC_DISCONNECT) {
		stdlib_log(LOG_ERR, "Not in signon status.");
		return ST_FAIL;
	}

	if ( (request = create_basic_reqmsg_fields(ST_RAOP)) == NULL) {
		return ST_FAIL;
	}

	if (  (ha_msg_add(request, F_STONITHD_RSCID, op->rsc_id) != HA_OK)
	    ||(ha_msg_add(request, F_STONITHD_RAOPTYPE, op->op_type) != HA_OK)
	    ||(ha_msg_add(request, F_STONITHD_RANAME, op->ra_name) != HA_OK)
	    ||(ha_msg_addhash(request, F_STONITHD_PARAMS, op->params) != HA_OK)
	   ) {
		stdlib_log(LOG_ERR, "stonithd_virtual_stonithRA_ops: "
			   "cannot add field to ha_msg.");
		ZAPMSG(request);
		return ST_FAIL;
	}

	/* Send the request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		ZAPMSG(request);
		stdlib_log(LOG_ERR, "can't send stonithRA message to IPC");
		return ST_FAIL;
	}

	/*  waiting for the output to finish */
	chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	ZAPMSG(request);
	
	/* Read the reply... */
	stdlib_log(LOG_DEBUG, "waiting for the stonithRA reply msg.");
        if ( IPC_OK != chan_waitin_timeout(chan, DEFAULT_TIMEOUT) ) {
		stdlib_log(LOG_ERR, "stonith:waitin failed.");
		/* how to deal. important */
		return ST_FAIL;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_virtual_stonithRA_ops: "
			   "to fetch reply msg failed.");
		return ST_FAIL;
	}
	
	if ( FALSE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RRAOP) ) {
		ZAPMSG(reply); /* avoid to zap the msg ? */
		stdlib_log(LOG_DEBUG, "stonithd_virtual_stonithRA_ops: "
			   "Got an unexpected message.");
		return ST_FAIL;
	}

	if ( ((tmpstr = cl_get_string(reply, F_STONITHD_APIRET)) != NULL) 
	   	    && (STRNCMP_CONST(tmpstr, ST_APIOK) == 0) ) {
		int tmpint;

		if ( ha_msg_value_int(reply, F_STONITHD_CALLID, &tmpint)
			== HA_OK ) {
			*call_id = tmpint;
			rc = ST_OK;
			stdlib_log(LOG_DEBUG, "a stonith RA operation queue " \
				   "to run, call_id=%d.", *call_id);
		} else {
			stdlib_log(LOG_ERR, "no return call_id in reply");
			rc = ST_FAIL;
		}
	} else {
		stdlib_log(LOG_DEBUG, "failed to do the RA op.");
		rc = ST_FAIL;
		* call_id = -1;		
	}

	ZAPMSG(reply);
	return rc;
}

int
stonithd_set_stonithRA_ops_callback(stonithRA_ops_callback_t callback,
				    void * private_data)
{
	if ( chan == NULL || chan->ch_status == IPC_DISCONNECT ) {
		stdlib_log(LOG_ERR, "stonithd_set_stonithRA_ops_callback: "
		 "failed due to not on signon status.");
		return ST_FAIL;
	} else {
		stonithRA_ops_cb = callback;
		stonithRA_ops_cb_private_data = private_data;
		stdlib_log(LOG_DEBUG, "setted stonith ops callback.");
	}
	
	return ST_OK;
}

int stonithd_list_stonith_types(GList ** types)
{
	int rc = ST_FAIL;
	struct ha_msg * request, * reply;
	const char * tmpstr;

	if (chan == NULL || chan->ch_status == IPC_DISCONNECT) {
		stdlib_log(LOG_ERR, "Not in signon status.");
		return ST_FAIL;
	}

	if (*types != NULL) {
		stdlib_log(LOG_ERR, "stonithd_list_stonith_types: *types!=NULL,"
			   " Will casue memory leak.");
		*types = NULL;
	}
	
	if ( (request = create_basic_reqmsg_fields(ST_LTYPES)) == NULL) {
		return ST_FAIL;
	}

	/* Send the request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		ZAPMSG(request);
		stdlib_log(LOG_ERR, "can't send stonithRA message to IPC");
		return ST_FAIL;
	}

	/*  waiting for the output to finish */
	chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	ZAPMSG(request);
	
	/* Read the reply... */
	stdlib_log(LOG_DEBUG, "waiting for the reply to list stonith types.");
        if ( IPC_OK != chan_waitin_timeout(chan, DEFAULT_TIMEOUT) ) {
		stdlib_log(LOG_ERR, "stonithd_list_stonith_types: "
			   "chan_waitin failed.");
		/* how to deal. important */
		return ST_FAIL;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_list_stonith_types: "
			   "failed to fetch reply.");
		return ST_FAIL;
	}
	
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RLTYPES) ) {
		if ( ((tmpstr = cl_get_string(reply, F_STONITHD_APIRET)) != NULL) 
	   	    && (STRNCMP_CONST(tmpstr, ST_APIOK) == 0) ) {
			int i, len;
			if ((len=cl_msg_list_length(reply, F_STONITHD_STTYPES))
			    < 0) {
				stdlib_log(LOG_ERR, "Not field to list stonith "
					  "types.");
			} else {
				for (i = 0; i < len; i++) {
					tmpstr = cl_msg_list_nth_data(reply,
							F_STONITHD_STTYPES, i);
					*types = g_list_append(*types,
							       g_strdup(tmpstr));

				}
				stdlib_log(LOG_DEBUG, "got stonith types.");
				rc = ST_OK;
			}
		} else {
			stdlib_log(LOG_DEBUG, "failed to get stonith types.");
		}
	} else {
		stdlib_log(LOG_DEBUG, "stonithd_list_stonith_types: "
			   "Got an unexpected message.");
	}

	ZAPMSG(reply);
	return rc;
}

static struct ha_msg * 
create_basic_reqmsg_fields(const char * apitype)
{
	struct ha_msg * msg = NULL;

	if ((msg = ha_msg_new(4)) == NULL) {
		stdlib_log(LOG_ERR, "create_basic_msg_fields:out of memory.");
		return NULL;
	}

	/* important error check client name length */
	if (  (ha_msg_add(msg, F_STONITHD_TYPE, ST_APIREQ ) != HA_OK )
	    ||(	ha_msg_add(msg, F_STONITHD_APIREQ, apitype) != HA_OK) 
	    ||(	ha_msg_add(msg, F_STONITHD_CNAME, CLIENT_NAME) != HA_OK) 
	    ||(	ha_msg_add_int(msg, F_STONITHD_CPID, CLIENT_PID) != HA_OK) 
	   ) {
		stdlib_log(LOG_ERR, "create basic msg fields: "
				"cannot add field to ha_msg.");
		ZAPMSG(msg);
	}

	return msg;
}

static gboolean 
is_expected_msg(const struct ha_msg * msg,  
		const char * field_name1, const char * field_content1,
		const char * field_name2, const char * field_content2 )
{
	const char * tmpstr;
	gboolean rc = FALSE;

	if ( msg == NULL ) {
		stdlib_log(LOG_ERR, "is_expected _msg: msg==NULL");
	}

	if (   ( (tmpstr = cl_get_string(msg, field_name1)) != NULL )
	    && (strncmp(tmpstr, field_content1, 80)==0) ) {
		stdlib_log(LOG_DEBUG, "%s = %s", field_name1, tmpstr);
		if (  ( (tmpstr = cl_get_string(msg, field_name2)) != NULL )
		    && strncmp(tmpstr, field_content2, 80) 
			== 0)  {
			stdlib_log(LOG_DEBUG, "%s = %s.", field_name2, tmpstr);
			rc= TRUE;
		} else {
			stdlib_log(LOG_DEBUG, "no field %s.", field_name2);
		}
	} else {
		stdlib_log(LOG_DEBUG, "No field %s", field_name1);
	}

	return rc;
}

static void
sigalarm_handler(int signum)
{
	if ( signum == SIGALRM ) {
		INT_BY_ALARM = TRUE;
	}
}

static int
chan_waitin_timeout(IPC_Channel * chan, unsigned int timeout)
{
	int ret;
	unsigned int other_remaining;
	struct sigaction old_action;

	other_remaining = alarm(0);
	if ( other_remaining > 0 ) {
		alarm(other_remaining);
		stdlib_log(LOG_NOTICE, "chan_waitin_timeout: There are others "
			"using timer:%d. I donnot use alarm.", other_remaining);
		alarm(other_remaining);
		ret = chan->ops->waitin(chan);
	} else {
		memset(&old_action, 0, sizeof(old_action));
		cl_signal_set_simple_handler(SIGALRM, sigalarm_handler
				, 	&old_action);
		
		INT_BY_ALARM = FALSE;
		alarm(timeout);
	
		ret = chan->ops->waitin(chan);

		if ( ret == IPC_INTR && INT_BY_ALARM ) {
			stdlib_log(LOG_ERR, "chan_waitin_timeout: waitin was "
				   "interrupted by alarm signal.");
		} else {
			alarm(0);
		}

		cl_signal_set_simple_handler(SIGALRM, old_action.sa_handler
				,	&old_action);
		stdlib_log(LOG_DEBUG, "chan_waitin_timeout: ret=%d.", ret);
	}

	return ret;
}

static int
chan_waitout_timeout(IPC_Channel * chan, unsigned int timeout)
{
	int ret;
	unsigned int other_remaining = 0;
	struct sigaction old_action;

	other_remaining = alarm(0);
	if ( other_remaining > 0 ) {
		alarm(other_remaining);
		stdlib_log(LOG_NOTICE, "chan_waitout_timeout: There are others "
			   "using timer, I donnot use alarm.");
		ret = chan->ops->waitout(chan);
	} else {
		memset(&old_action, 0, sizeof(old_action));
		cl_signal_set_simple_handler(SIGALRM, sigalarm_handler
				, 	&old_action);
		INT_BY_ALARM = FALSE;
		alarm(timeout);

		ret = chan->ops->waitout(chan);

		if ( ret == IPC_INTR && INT_BY_ALARM ) {
			stdlib_log(LOG_ERR, "chan_waitout_timeout: waitout was "
				   "interrupted by alarm setted by myself.");
		} else {
			alarm(0);
		}

		cl_signal_set_simple_handler(SIGALRM, old_action.sa_handler
				,	&old_action);
		stdlib_log(LOG_DEBUG, "chan_waitout_timeout: ret=%d.", ret);
	}

	return ret;
}

void stdlib_enable_debug_mode(void)
{
	DEBUG_MODE = TRUE;
}

/* copied from cl_log.c, need to be the same */
#ifndef MAXLINE
#	define MAXLINE	512
#endif
static void
stdlib_log(int priority, const char * fmt, ...)
{
	va_list		ap;
	char		buf[MAXLINE];

	if ( DEBUG_MODE == FALSE && priority == LOG_DEBUG ) {
		return;
	}
	
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf)-1, fmt, ap);
	va_end(ap);
	cl_log(priority, "%s", buf);
}

static void
free_stonithRA_ops_t(stonithRA_ops_t * ra_op)
{
	if (ra_op == NULL) {
		stdlib_log(LOG_DEBUG, "free_stonithRA_ops_t: ra_op==NULL");
		return;
	}

	ZAPGDOBJ(ra_op->rsc_id);
       	ZAPGDOBJ(ra_op->ra_name);
	ZAPGDOBJ(ra_op->op_type);
	/* Has used g_hash_table_new_full to create params */
	g_hash_table_destroy(ra_op->params);
	ZAPGDOBJ(ra_op);
}

static void
free_stonith_ops_t(stonith_ops_t * st_op)
{
	if (st_op == NULL) {
		stdlib_log(LOG_DEBUG, "free_stonith_ops_t: st_op==NULL");
		return;
	}

	ZAPGDOBJ(st_op->node_name);
	ZAPGDOBJ(st_op->node_list);
	ZAPGDOBJ(st_op->private_data);
	ZAPGDOBJ(st_op);
}
