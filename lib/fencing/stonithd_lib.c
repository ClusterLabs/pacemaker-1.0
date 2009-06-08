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

#include <crm_internal.h>
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

#if HAVE_HB_CONFIG_H
#include <heartbeat/hb_config.h>
#endif

#if HAVE_GLUE_CONFIG_H
#include <glue_config.h>
#endif

#include <heartbeat/ha_msg.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/GSource.h>
#include <clplumbing/uids.h>
#include <clplumbing/cl_log.h>

#include <clplumbing/proctrack.h>
#include <fencing/stonithd_api.h>
#include <fencing/stonithd_msg.h>
#include <string.h>

#include <assert.h>

static const char * CLIENT_NAME = NULL;
static pid_t CLIENT_PID = 0;
static char CLIENT_PID_STR[16];
static gboolean DEBUG_MODE	   = FALSE;
static IPC_Channel * chan	   = NULL;
static IPC_Channel * cbchan	   = NULL;

static gboolean INT_BY_ALARM = FALSE;
static unsigned int DEFAULT_TIMEOUT = 60;

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
static gboolean cmp_field(const struct ha_msg * msg,  
		const char * field_name, const char * field_content,
		gboolean mandatory);
static gboolean is_expected_msg(const struct ha_msg * msg,
				const char * field_name1,
				const char * field_content1,
				const char * field_name2,
				const char * field_content2,
				gboolean mandatory);
static int chan_wait_timeout(IPC_Channel * chan,
		int (*waitfun)(IPC_Channel * chan), unsigned int timeout);
static int chan_waitin_timeout(IPC_Channel * chan, unsigned int timeout);
static int chan_waitout_timeout(IPC_Channel * chan, unsigned int timeout);
static void sigalarm_handler(int signum);
static void free_stonith_ops_t(stonith_ops_t * st_op);
static void free_stonithRA_ops_t(stonithRA_ops_t * ra_op);

#define stdlib_log(priority, fmt...); \
        if ( ( priority != LOG_DEBUG ) || ( debug_level > 0 ) ) { \
                cl_log(priority, fmt); \
        }
#define signed_on(ch) (ch && ch->ch_status != IPC_DISCONNECT)

#define LOG_FAILED_TO_GET_FIELD(field)					\
			stdlib_log(LOG_ERR				\
			,	"%s:%d: cannot get field %s from message." \
			,__FUNCTION__,__LINE__,field)

#define st_get_int_value(msg,fld,i) do { \
	if (HA_OK != ha_msg_value_int(msg,fld,i)) { \
		LOG_FAILED_TO_GET_FIELD(fld); \
		rc = ST_FAIL; \
	} \
} while(0)
#define st_save_string(msg,fld,v) do { \
	const char *tmp; \
	tmp = cl_get_string(msg,fld); \
	if (!tmp) { \
		LOG_FAILED_TO_GET_FIELD(fld); \
		rc = ST_FAIL; \
	} else { \
		if( !(v = g_strdup(tmp)) ) { \
			rc = ST_FAIL; \
			stdlib_log(LOG_ERR,"%s:%d: out of memory" \
			,__FUNCTION__,__LINE__); \
		} \
	} \
} while(0)
#define st_get_hashtable(msg,fld,v) do { \
	v = cl_get_hashtable(msg,fld); \
	if (!v) { \
		LOG_FAILED_TO_GET_FIELD(fld); \
		rc = ST_FAIL; \
	} \
} while(0)

static int
send_request(IPC_Channel *chan, struct ha_msg *request, int timeout)
{
	int rc;

	assert(chan != NULL);
	assert(request != NULL);

	/* Send the request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		stdlib_log(LOG_ERR, "can't send signon message to IPC");
		return ST_FAIL;
	}

	/* wait for the output to finish */
	/* XXX: the time ellapsed should be substracted from timeout 
	 * each time the call is resumed.
	 */
	do { 
		rc = chan_waitout_timeout(chan, timeout);
	} while (rc == IPC_INTR);

	if (rc != IPC_OK) {
		stdlib_log(LOG_ERR, "waitout failed.");
		return ST_FAIL;
	}
	return ST_OK;
}

static struct ha_msg *
recv_response(IPC_Channel *chan, int timeout)
{
	struct ha_msg *reply;
        if (IPC_OK != chan_waitin_timeout(chan, timeout)) {
		stdlib_log(LOG_ERR, "waitin failed."); 
		return NULL;
	}
	if (!(reply = msgfromIPC_noauth(chan))) {
		stdlib_log(LOG_ERR, "failed to recv response");
		return NULL;
	}
	return reply;
}

static int
authenticate_with_cookie(IPC_Channel *chan, cl_uuid_t *cookie) 
{
	struct ha_msg *	request;
	struct ha_msg * reply;

	assert(chan != NULL);
	assert(cookie != NULL);

	if (!(request = create_basic_reqmsg_fields(ST_SIGNON))) {
		return ST_FAIL;
	}
	if (ha_msg_adduuid(request, F_STONITHD_COOKIE, cookie) != HA_OK) {
		stdlib_log(LOG_ERR, "cannot add field to ha_msg.");
		ZAPMSG(request);
		return ST_FAIL;
	}

	/* Send request/read response */
	if (send_request(chan, request, DEFAULT_TIMEOUT) != ST_OK) {
		ZAPMSG(request);
		return ST_FAIL;
	}
	ZAPMSG(request);

	if (!(reply = recv_response(chan, DEFAULT_TIMEOUT))) {
		return ST_FAIL;
	}
	
	/* Are we signed on this time? */
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RSIGNON, TRUE) ) {
		if ( !STRNCMP_CONST(
			cl_get_string(reply,F_STONITHD_APIRET), ST_APIOK) ) {
			ZAPMSG(reply);
			return ST_OK;
		}
	}

	ZAPMSG(reply);
	return ST_FAIL;
}

int
stonithd_signon(const char * client_name)
{
	int     rc = ST_FAIL;
	char	path[] = IPC_PATH_ATTR;
	char	sock[] = STONITHD_SOCK;
	char	cbsock[] = STONITHD_CALLBACK_SOCK;
	struct  ha_msg * request;
	struct  ha_msg * reply;
	GHashTable *	 wchanattrs;
	uid_t	my_euid;
	gid_t	my_egid;
	const char * tmpstr;
	int 	rc_tmp;
	gboolean connected = TRUE;
 	cl_uuid_t cookie, *cptr = NULL;

	if (chan == NULL || chan->ch_status != IPC_CONNECT) {
	    connected = FALSE;
	} else if (cbchan == NULL || cbchan->ch_status != IPC_CONNECT) {
	    connected = FALSE;
	}

	if(!connected) {
		/* cleanup */
		if (NULL != chan) {
		    chan->ops->destroy(chan);
		    chan = NULL;
		}
		if (NULL != cbchan) {
		    cbchan->ops->destroy(cbchan);
		    cbchan = NULL;
		}
		stdlib_log(LOG_DEBUG, "stonithd_signon: creating connection");
		wchanattrs = g_hash_table_new(g_str_hash, g_str_equal);
        	g_hash_table_insert(wchanattrs, path, sock);
		/* Connect to the stonith deamon */
		chan = ipc_channel_constructor(IPC_ANYTYPE, wchanattrs);
		g_hash_table_destroy(wchanattrs);
	
		if (chan == NULL) {
			stdlib_log(LOG_ERR, "stonithd_signon: Can't connect "
				   " to stonithd");
			rc = ST_FAIL;
			goto end;
		}

	        if (chan->ops->initiate_connection(chan) != IPC_OK) {
			stdlib_log(LOG_ERR, "stonithd_signon: Can't initiate "
				   "connection to stonithd");
			rc = ST_FAIL;
			goto end;
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
		rc = ST_FAIL;
		goto end;
	}

	/* important error check client name length */
	my_euid = geteuid();
	my_egid = getegid();
	if (  (	ha_msg_add_int(request, F_STONITHD_CEUID, my_euid) != HA_OK )
	    ||(	ha_msg_add_int(request, F_STONITHD_CEGID, my_egid) != HA_OK )
	    ||( ha_msg_add(request, F_STONITHD_COOKIE, "") != HA_OK )
	   ) {
		stdlib_log(LOG_ERR, "stonithd_signon: "
			   "cannot add field to ha_msg.");
		ZAPMSG(request);
		rc = ST_FAIL;
		goto end;
	}

	stdlib_log(LOG_DEBUG, "sending out the signon msg.");
	/* Send the registration request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		ZAPMSG(request);
		stdlib_log(LOG_ERR, "can't send signon message to IPC");
		rc = ST_FAIL;
		goto end;
	}

	/* waiting for the output to finish */
	do { 
		rc_tmp= chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	} while (rc_tmp == IPC_INTR);

	ZAPMSG(request);
	if (IPC_OK != rc_tmp) {
		stdlib_log(LOG_ERR, "%s:%d: waitout failed."
			   , __FUNCTION__, __LINE__);
		rc = ST_FAIL;
		goto end;
	}

	/* Read the reply... */
        if ( IPC_OK != chan_waitin_timeout(chan, DEFAULT_TIMEOUT) ) {
		stdlib_log(LOG_ERR, "%s:%d: waitin failed."
			   , __FUNCTION__, __LINE__);
		rc = ST_FAIL;
		goto end;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_signon: failed to fetch reply.");
		rc = ST_FAIL;
		goto end;
	}
	
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RSIGNON, TRUE) ) {
		if ( ((tmpstr=cl_get_string(reply, F_STONITHD_APIRET)) != NULL)
	   	    && (STRNCMP_CONST(tmpstr, ST_APIOK) == 0) ) {
			rc = ST_OK;
			stdlib_log(LOG_DEBUG, "signed on to stonithd.");
			/* get cookie if any */
			if( cl_get_uuid(reply, F_STONITHD_COOKIE, &cookie) == HA_OK ) {
				cptr = &cookie;
			}
		} else {
			stdlib_log(LOG_WARNING, "failed to signon to the "
				   "stonithd.");
		}
	} else {
		stdlib_log(LOG_ERR, "stonithd_signon: "
			   "Got an unexpected message.");
	}
	ZAPMSG(reply);

	if (ST_OK != rc) { /* Something wrong when try to sign on to stonithd */
		goto end;
	}

	/* Connect to the stonith deamon via callback channel */
	wchanattrs = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(wchanattrs, path, cbsock);
	cbchan = ipc_channel_constructor(IPC_ANYTYPE, wchanattrs);
	g_hash_table_destroy(wchanattrs);

	if (cbchan == NULL) {
		stdlib_log(LOG_ERR, "stonithd_signon: Can't construct "
			   "callback channel to stonithd.");
		rc = ST_FAIL;
		goto end;
	}

        if (cbchan->ops->initiate_connection(cbchan) != IPC_OK) {
		stdlib_log(LOG_ERR, "stonithd_signon: Can't initiate "
			   "connection with the callback channel");
		rc = ST_FAIL;
		goto end;
 	}

	if ( (reply = msgfromIPC_noauth(cbchan)) == NULL ) {
		stdlib_log(LOG_ERR, "%s:%d: failed to fetch reply via the "
			   " callback channel"
			   , __FUNCTION__, __LINE__);
		rc = ST_FAIL;
		goto end;
	}
	
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RSIGNON, TRUE) ) {
		tmpstr=cl_get_string(reply, F_STONITHD_APIRET);
		if ( !STRNCMP_CONST(tmpstr, ST_APIOK) ) {
			/* 
			 * If the server directly authenticates us (probably 
			 * via pid-auth), go ahead.
			 */
			stdlib_log(LOG_DEBUG, "%s:%d: Got a good signon reply "
				  "via the callback channel."
				   , __FUNCTION__, __LINE__);
		} else if ( !STRNCMP_CONST(tmpstr, ST_COOKIE) ) {
			/*
			 * If the server asks for a cookie to identify myself,
			 * initiate cookie authentication.
			 */
			if (cptr == NULL) {
				stdlib_log(LOG_ERR, "server requested cookie auth on "
					"the callback channel, but it didn't "
					"provide the cookie on the main channel.");
				rc = ST_FAIL;
			} else {
				rc = authenticate_with_cookie(cbchan, cptr);
			}
		} else {
			/* Unknown response. */
			rc = ST_FAIL;
			stdlib_log(LOG_ERR, "%s:%d: Got a bad signon reply "
				  "via the callback channel."
				   , __FUNCTION__, __LINE__);
		}
	} else {
		rc = ST_FAIL;
		stdlib_log(LOG_ERR, "stonithd_signon: "
			   "Got an unexpected message via the callback chan.");
	}
	ZAPMSG(reply);

end:
	if (ST_OK != rc) {
		/* Something wrong when confirm via callback channel */
		stonithd_signoff();
	}

	return rc;
}

int 
stonithd_signoff(void)
{
	struct ha_msg * request;
	gboolean connected = TRUE;

	if (chan == NULL || chan->ch_status != IPC_CONNECT) {
	    connected = FALSE;
	} else if (cbchan == NULL || cbchan->ch_status != IPC_CONNECT) {
	    connected = FALSE;
	}
	
	if (!connected) {
		stdlib_log(LOG_NOTICE, "Not currently connected.");
		goto bail;
	}

	if ( (request = create_basic_reqmsg_fields(ST_SIGNOFF)) == NULL) {
		stdlib_log(LOG_ERR, "Couldn't create signoff message!");
		goto bail;
	}

	/* Send the signoff request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		ZAPMSG(request);
		stdlib_log(LOG_ERR, "Control channel dead - can't send signoff message");
		goto bail;
	}

	/*  waiting for the output to finish */
	chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	ZAPMSG(request);

  bail:
	if (NULL != chan) {
		chan->ops->destroy(chan);
		chan = NULL;
	}
	if (NULL != cbchan) {
		cbchan->ops->destroy(cbchan);
		cbchan = NULL;
	}
	
	return ST_OK;
}

IPC_Channel *
stonithd_input_IPC_channel(void)
{
	if ( !signed_on(cbchan) ) {
		stdlib_log(LOG_ERR, "stonithd_input_IPC_channel: not signed on");
		return NULL;
	} else {
		return cbchan;
	}
}

void
set_stonithd_input_IPC_channel_NULL(void)
{
    cbchan = NULL;
}

int 
stonithd_node_fence(stonith_ops_t * op)
{
	int rc = ST_FAIL;
	struct ha_msg *request, *reply;

	if (op == NULL) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: op==NULL");
		goto out;
	}
	
	if (!signed_on(chan)) {
		stdlib_log(LOG_NOTICE, "not signed on");
		goto out;
	}

	if (!(request = create_basic_reqmsg_fields(ST_STONITH))) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "message creation failed.");
		goto out;
	}

	if (ha_msg_add_int(request, F_STONITHD_OPTYPE, op->optype) != HA_OK) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "cannot add optype field to ha_msg.");
		goto out;
	}
	if (ha_msg_add(request, F_STONITHD_NODE, op->node_name ) != HA_OK) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "cannot add node_name field to ha_msg.");
		goto out;
	}
	if (op->node_uuid == NULL || (ha_msg_add(request, F_STONITHD_NODE_UUID, 
					op->node_uuid) != HA_OK)) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "cannot add node_uuid field to ha_msg.");
		goto out;
	}
	if (ha_msg_add_int(request, F_STONITHD_TIMEOUT, op->timeout) != HA_OK) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "cannot add timeout field to ha_msg.");
		goto out;
	}
	if  (op->private_data == NULL || (ha_msg_add(request, F_STONITHD_PDATA, 
					op->private_data) != HA_OK)) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
		   "cannot add private_data field to ha_msg.");
		goto out;
	}

	/* Send the stonith request message */
	if (msg2ipcchan(request, chan) != HA_OK) {
		stdlib_log(LOG_ERR
			   , "failed to send stonith request to stonithd");
		goto out;
	}

	/*  waiting for the output to finish */
	chan_waitout_timeout(chan, DEFAULT_TIMEOUT);
	
	/* Read the reply... */
	stdlib_log(LOG_DEBUG, "waiting for the stonith reply msg.");
        if ( IPC_OK != chan_waitin_timeout(chan, DEFAULT_TIMEOUT) ) {
		stdlib_log(LOG_ERR, "%s:%d: waitin failed."
			   , __FUNCTION__, __LINE__);
		goto out;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_node_fence: fail to fetch reply");
		goto out;
	}
	
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RSTONITH, TRUE) ) {
		if( !STRNCMP_CONST(
			cl_get_string(reply,F_STONITHD_APIRET), ST_APIOK) ) {
			rc = ST_OK;
			stdlib_log(LOG_DEBUG, "%s:%d: %s"
				 , __FUNCTION__, __LINE__
				 , "stonithd's synchronous answer is ST_APIOK");
		} else {
			stdlib_log(LOG_ERR, "%s:%d: %s"
			       , __FUNCTION__, __LINE__
			       , "stonithd's synchronous answer is ST_APIFAIL");
		}
	} else {
		stdlib_log(LOG_ERR, "stonithd_node_fence: "
			   "Got an unexpected message.");
		/* Need to handle in other way? */
	}

out:
	ZAPMSG(reply);
	ZAPMSG(request);
	return rc;
}

gboolean 
stonithd_op_result_ready(void)
{
	if ( !signed_on(cbchan) ) {
		stdlib_log(LOG_ERR, "stonithd_op_result_ready: "
			   "not signed on");
		return FALSE;
	}
	
	/* 
	 * Regards IPC_DISCONNECT as a special result, so to prevent the caller
	 * from the possible endless waiting. That can be caused by the way
	 * in which the caller uses it.
	 */
	return (cbchan->ops->is_message_pending(cbchan)
		|| cbchan->ch_status == IPC_DISCONNECT);
}

int
stonithd_receive_ops_result(gboolean blocking)
{
	struct ha_msg* reply = NULL;
	const char *reply_type;
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
		rc = cbchan->ops->waitin(cbchan);
		if (IPC_OK != rc) {
			if (cbchan->ch_status == IPC_DISCONNECT) {
				stdlib_log(LOG_INFO, "%s:%d: disconnected",
				__FUNCTION__, __LINE__); 
			} else if (IPC_INTR == rc) {
				stdlib_log(LOG_INFO, "%s:%d: waitin interrupted",
				__FUNCTION__, __LINE__); 
			} else {
				stdlib_log(LOG_WARNING, "%s:%d: waitin failed: %d",
				__FUNCTION__, __LINE__,rc); 
			}
			return ST_FAIL;
		}
	}

	reply = msgfromIPC_noauth(cbchan);
	reply_type = cl_get_string(reply, F_STONITHD_APIRPL);
	if ( !is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, reply_type, TRUE)) {
		ZAPMSG(reply);
		stdlib_log(LOG_DEBUG, "%s:%d: "
			   "got an unexpected message", __FUNCTION__, __LINE__);
		return ST_FAIL;
	}
	if( !strcmp(reply_type, ST_STRET) ) {
		stonith_ops_t *st_op = NULL;
		/* handle the stonith op result message */
		if( !(st_op = g_new(stonith_ops_t, 1)) ) {
			stdlib_log(LOG_ERR, "out of memory");
			return ST_FAIL;
		}
		st_op->node_uuid = NULL;
		st_op->private_data = NULL;

		st_get_int_value(reply, F_STONITHD_OPTYPE, (int*)&st_op->optype);	
		st_save_string(reply, F_STONITHD_NODE, st_op->node_name);
		st_save_string(reply, F_STONITHD_NODE_UUID, st_op->node_uuid);
		st_get_int_value(reply, F_STONITHD_TIMEOUT, &st_op->timeout);
		st_get_int_value(reply, F_STONITHD_CALLID, &st_op->call_id);
		st_get_int_value(reply, F_STONITHD_FRC, (int*)&st_op->op_result);
		st_save_string(reply, F_STONITHD_NLIST, st_op->node_list);
		st_save_string(reply, F_STONITHD_PDATA, st_op->private_data);

		if (stonith_ops_cb != NULL) {
			stonith_ops_cb(st_op);
		}

		free_stonith_ops_t(st_op);
	}
	else if( !strcmp(reply_type, ST_RAOPRET) ) {
		stonithRA_ops_t *ra_op = NULL;
		/* handle the stonithRA op result message */
		if( !(ra_op = g_new(stonithRA_ops_t, 1)) ) {
			stdlib_log(LOG_ERR, "out of memory");
			return ST_FAIL;
		}

		st_save_string(reply, F_STONITHD_RSCID, ra_op->rsc_id);
		st_save_string(reply, F_STONITHD_RAOPTYPE, ra_op->op_type);
		st_save_string(reply, F_STONITHD_RANAME, ra_op->ra_name);
		st_get_hashtable(reply, F_STONITHD_PARAMS, ra_op->params);
		st_get_int_value(reply, F_STONITHD_CALLID, &ra_op->call_id);
		st_get_int_value(reply, F_STONITHD_FRC, &ra_op->op_result);

		/* if ( rc == ST_OK && stonithRA_ops_cb != NULL)  */
		if ( stonithRA_ops_cb ) {
			stonithRA_ops_cb(ra_op, stonithRA_ops_cb_private_data);
		}

		free_stonithRA_ops_t(ra_op);
	}
	else {
		stdlib_log(LOG_DEBUG, "%s:%d: "
			   "got an unexpected message", __FUNCTION__, __LINE__);
		rc = ST_FAIL;
	}
	ZAPMSG(reply);
	return rc;
}

int
stonithd_set_stonith_ops_callback(stonith_ops_callback_t callback)
{
	if ( !signed_on(cbchan) ) {
		stdlib_log(LOG_ERR, "stonithd_set_stonith_ops_callback: "
		 "not signed on");
		return ST_FAIL;
	}
	stonith_ops_cb = callback;
	return ST_OK;
}

int
stonithd_virtual_stonithRA_ops( stonithRA_ops_t * op, int * call_id)
{
	int rc = ST_FAIL;
	struct ha_msg * request, * reply;
	const char * tmpstr;

	if (op == NULL) {
		stdlib_log(LOG_ERR, "stonithd_virtual_stonithRA_ops: op==NULL");
		return ST_FAIL;
	}
	
	if (call_id == NULL) {
		stdlib_log(LOG_ERR, "stonithd_stonithd_stonithRA_ops: "
			   "call_id==NULL");
		return ST_FAIL;
	}
	
	if ( !signed_on(chan) ) {
		stdlib_log(LOG_ERR, "not signed on");
		return ST_FAIL;
	}

	if ( (request = create_basic_reqmsg_fields(ST_RAOP)) == NULL) {
		return ST_FAIL;
	}

	if (  (ha_msg_add(request, F_STONITHD_RSCID, op->rsc_id) != HA_OK)
	    ||(ha_msg_add(request, F_STONITHD_RAOPTYPE, op->op_type) != HA_OK)
	    ||(ha_msg_add(request, F_STONITHD_RANAME, op->ra_name) != HA_OK)
	    ||(ha_msg_add_int(request, F_STONITHD_TIMEOUT, op->timeout) != HA_OK)
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
		stdlib_log(LOG_ERR, "%s:%d: waitin failed."
			   , __FUNCTION__, __LINE__);
		return ST_FAIL;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_virtual_stonithRA_ops: "
			   "failed to fetch reply");
		return ST_FAIL;
	}
	
	if ( FALSE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RRAOP, TRUE) ) {
		ZAPMSG(reply); /* avoid to zap the msg ? */
		stdlib_log(LOG_WARNING, "stonithd_virtual_stonithRA_ops: "
			   "got an unexpected message");
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
		stdlib_log(LOG_WARNING, "failed to do the RA op.");
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
	if ( !signed_on(cbchan) ) {
		stdlib_log(LOG_ERR, "stonithd_set_stonithRA_ops_callback: "
		 "not signed on");
		return ST_FAIL;
	}
	stonithRA_ops_cb = callback;
	stonithRA_ops_cb_private_data = private_data;
	return ST_OK;
}

int stonithd_list_stonith_types(GList ** types)
{
	int rc = ST_FAIL;
	struct ha_msg * request, * reply;
	const char * tmpstr;

	if ( !signed_on(chan) ) {
		stdlib_log(LOG_ERR, "not signed on");
		return ST_FAIL;
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
		stdlib_log(LOG_ERR, "%s:%d: chan_waitin failed."
			   , __FUNCTION__, __LINE__);
		return ST_FAIL;
	}

	if ( (reply = msgfromIPC_noauth(chan)) == NULL ) {
		stdlib_log(LOG_ERR, "stonithd_list_stonith_types: "
			   "failed to fetch reply.");
		return ST_FAIL;
	}
	
	*types = NULL;
	if ( TRUE == is_expected_msg(reply, F_STONITHD_TYPE, ST_APIRPL, 
			     F_STONITHD_APIRPL, ST_RLTYPES, TRUE) ) {
		if ( ((tmpstr = cl_get_string(reply, F_STONITHD_APIRET)) != NULL) 
	   	    && (STRNCMP_CONST(tmpstr, ST_APIOK) == 0) ) {
			int i;
			int len=cl_msg_list_length(reply, F_STONITHD_STTYPES);
			if ( len < 0 ) {
				stdlib_log(LOG_ERR, "Not field to list stonith "
					  "types.");
			} else {
				for (i = 0; i < len; i++) {
					tmpstr = cl_msg_list_nth_data(reply,
							F_STONITHD_STTYPES, i);
					if( tmpstr ) {
						*types = g_list_append(*types,
							       g_strdup(tmpstr));
					}
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
		stdlib_log(LOG_ERR, "create_basic_msg_fields: "
				"cannot add field to ha_msg.");
		ZAPMSG(msg);
	}

	return msg;
}

static gboolean 
cmp_field(const struct ha_msg * msg,  
		const char * field_name, const char * field_content,
		gboolean mandatory)
{
	const char * tmpstr;

	tmpstr = cl_get_string(msg, field_name);
	if ( tmpstr && strncmp(tmpstr, field_content, 80) == 0 ) {
		return TRUE;
	} else {
		stdlib_log(mandatory ? LOG_ERR : LOG_NOTICE
			, "field <%s> content is "
			" <%s>, expected content is: <%s>"
			, field_name
			, (NULL == tmpstr) ? "NULL" : tmpstr
			, field_content);
		return FALSE;
	}
}

static gboolean 
is_expected_msg(const struct ha_msg * msg,  
		const char * field_name1, const char * field_content1,
		const char * field_name2, const char * field_content2,
		gboolean mandatory)
{
	if ( msg == NULL ) {
		stdlib_log(LOG_ERR, "%s:%d: null message",
			__FUNCTION__, __LINE__);
		return FALSE;
	}
	return cmp_field(msg, field_name1, field_content1, mandatory)
		&& cmp_field(msg, field_name2, field_content2, mandatory);
}

static void
sigalarm_handler(int signum)
{
	if ( signum == SIGALRM ) {
		INT_BY_ALARM = TRUE;
	}
}

static int
chan_wait_timeout(IPC_Channel * chan,
		int (*waitfun)(IPC_Channel * chan), unsigned int timeout)
{
	int ret = IPC_FAIL;
	unsigned int remaining;
	struct sigaction old_action;

	remaining = alarm(0);
	if ( remaining > 0 ) {
		alarm(remaining);
		stdlib_log(LOG_NOTICE, "%s:%d: others "
			   "using alarm, can't set timeout",
			   __FUNCTION__, __LINE__);
		ret = waitfun(chan);
	} else {
		memset(&old_action, 0, sizeof(old_action));
		cl_signal_set_simple_handler(SIGALRM, sigalarm_handler
				, 	&old_action);

		INT_BY_ALARM = FALSE;
		remaining = timeout;

		while ( remaining > 0 ) {
			alarm(remaining);
			ret = waitfun(chan);
			if ( ret == IPC_INTR ) {
				if ( TRUE == INT_BY_ALARM ) {
					stdlib_log(LOG_ERR, "%s:%d: timed out"
						   , __FUNCTION__, __LINE__);
					ret = IPC_FAIL;
					break;
				} else {
					stdlib_log(LOG_NOTICE, "%s:%d: interrupted"
						   , __FUNCTION__, __LINE__);
					remaining = alarm(0);
				}
			} else {
				alarm(0);
				break;
			}
		}
		cl_signal_set_simple_handler(SIGALRM, old_action.sa_handler
				,	&old_action);
		if( ret != IPC_OK ) {
			stdlib_log(LOG_DEBUG, "%s:%d: ret=%d"
				   , __FUNCTION__, __LINE__, ret);
		}
	}
	return ret;
}

static int
chan_waitin_timeout(IPC_Channel * chan, unsigned int timeout)
{
	return chan_wait_timeout(chan,chan->ops->waitin,timeout);
}

static int
chan_waitout_timeout(IPC_Channel * chan, unsigned int timeout)
{
	return chan_wait_timeout(chan,chan->ops->waitout,timeout);
}

void stdlib_enable_debug_mode(void)
{
	DEBUG_MODE = TRUE;
}

static void
free_stonithRA_ops_t(stonithRA_ops_t * ra_op)
{
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
	ZAPGDOBJ(st_op->node_uuid);	
	ZAPGDOBJ(st_op->private_data);
	ZAPGDOBJ(st_op);
}
