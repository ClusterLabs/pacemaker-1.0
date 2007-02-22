/*
 * recoverymgr.c: recovery manager client library code.
 *
 * Copyright (C) 2002 Intel Corporation 
 *
 * The majority of this code was taken from the apphb.c
 * library code
 * (which is Copyright (C) 2002 Alan Robertson )
 * and modified to provide a recoverymgr API.
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
 *  
 */
#include <lha_internal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define	time	footime
#define	index	fooindex
#include	<glib.h>
#undef time
#undef index

#include <recoverymgr.h>
#include <clplumbing/ipc.h>
#include <clplumbing/recoverymgr_cs.h>
#include <clplumbing/cl_log.h>

#include <stdio.h>

static struct IPC_CHANNEL*	comm = NULL;
static GHashTable *		attrs;
static int			status = -1;

static int recoverymgr_getrc(void);
	
/** 
 * Get return code from last operation 
 */
static int
recoverymgr_getrc(void)
{
	struct recoverymgr_rc * rcs;
	int		rc;

	struct IPC_MESSAGE * msg;

	/* Wait for a message... */
	comm->ops->waitin(comm);

	if (comm->ops->recv(comm, &msg) != IPC_OK) {
		perror("Receive failure:");
		return errno;
	}
	rcs = msg->msg_body;
	rc = rcs->rc;
	msg->msg_done(msg);
	return rc;
}

/**
 * Packages the client and event info into a message that is then
 * sent to the recovery manager.  If ESRCH is returned, 
 * then the client may wish to attempt to connect again
 * and resend the event.  This would be necessary if the 
 * recovery manager daemon is ever restarted, thus losing
 * the connection.
 *
 * @param event The event for the client specified
 *
 * @return zero on error, non-zero on success
 */
int 
recoverymgr_send_event(const char *appname, const char *appinst,
		pid_t pid, uid_t uid, gid_t gid, 
		apphb_event_t event)
{

        struct recoverymgr_event_msg msg;
        struct IPC_MESSAGE      Msg;
		
        /*if (comm == NULL || status != IPC_OK)  */
	if (NULL == comm)
	{
		cl_log(LOG_CRIT, "comm=%p, status=%d", comm, status);
                errno = ESRCH;
                return -1;
        }

	if (appname == NULL || appinst == NULL)
	{
		errno = EINVAL;
		return -1;
	}
	if (strlen(appname) >= RECOVERYMGR_OLEN ||
		strlen(appinst) >= RECOVERYMGR_OLEN)
	{
 		errno = ENAMETOOLONG;
		return -1;
	}

	/* create and send the event message */
        strncpy(msg.msgtype, RECOVERYMGR_EVENT, sizeof(msg.msgtype));
        strncpy(msg.appname, appname, sizeof(msg.appname));
        strncpy(msg.appinstance, appinst, sizeof(msg.appinstance));	
	msg.pid = pid;
	msg.uid = uid;
	msg.gid = gid;
	msg.event = event;	
	
	memset(&Msg, 0, sizeof(Msg));
	
        Msg.msg_body = &msg;
        Msg.msg_len = sizeof(msg);
        Msg.msg_done = NULL;
        Msg.msg_private = NULL;
        Msg.msg_ch = comm;

        if (comm->ops->send(comm, &Msg) != IPC_OK) {
                errno = EBADF;
		cl_log(LOG_CRIT, "Failed to send message to recovery mgr");
                return -1;
        }
        /* NOTE: we do not expect a return code from server */
        return 0;
}

/**
 *
 * @param appname The name of the process that is registering 
 * @param appinstance 
 *
 * @return EEXIST if the connection already exists
 * 	EINVAl if the appname or instance is NULL
 * 	ENAMETOOLONG if the appname or appinstance is greater than
 * 		RECOVERYMGR_OLEN
 * 	EBADF	
 * 	negative on any other error
 *
 * 	0 on success
 *
 */
int
recoverymgr_connect(const char * appname, 
			const char *appinstance)
{
	int	err;
	struct IPC_MESSAGE Msg;
	struct recoverymgr_connectmsg msg;
	static char path [] = IPC_PATH_ATTR;
	static char sockpath []	= RECOVERYMGRSOCKPATH;

	if (comm != NULL) {
		errno = EEXIST;
		return EEXIST;
	}

	if (appname == NULL || appinstance == NULL ) {
		errno = EINVAL;
		return EINVAL;
	}

	if (strlen(appname) >= RECOVERYMGR_OLEN
	||	strlen(appinstance) >= RECOVERYMGR_OLEN) {
		errno = ENAMETOOLONG;
		return ENAMETOOLONG;
	}

	/* Create communication channel with server... */

	attrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(attrs, path, sockpath);
  
	comm = ipc_channel_constructor(IPC_ANYTYPE, attrs);
  
	if (comm == NULL
	||	(status = comm->ops->initiate_connection(comm)
	!=	IPC_OK)) {
	
		recoverymgr_disconnect();
  		errno = EBADF;
		return EBADF;
	}

	/* Send registration message ... */
	strncpy(msg.msgtype, RECOVERYMGR_CONNECT, sizeof(msg.msgtype));
	strncpy(msg.appname, appname, sizeof(msg.appname));
	strncpy(msg.appinstance, appinstance, sizeof(msg.appinstance));
	msg.pid = getpid();
	msg.uid = getuid();
	msg.gid = getgid();
	
	memset(&Msg, 0, sizeof(Msg));
	
	Msg.msg_body = &msg;
	Msg.msg_len = sizeof(msg);
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = comm;

	if (comm->ops->send(comm, &Msg) != IPC_OK) {
		recoverymgr_disconnect();
  		errno = EBADF;
		return -1;
	}
	if ((err = recoverymgr_getrc()) != 0) {
		status = -1;
		errno = err;
		return -1;
	}
	return 0;
}

/**
 *  disconnect from recovery manager daemon
 */
int
recoverymgr_disconnect(void)
{
	int	rc = 0;
	int	err;
	struct recoverymgr_msg msg;
	struct IPC_MESSAGE Msg;


	if (comm == NULL || status != IPC_OK) {
		errno = ESRCH;
		rc = -1;
	}

	/* Send an unregister message to the server... */
	if (comm != NULL && status == IPC_OK) {
		strncpy(msg.msgtype, RECOVERYMGR_DISCONNECT, sizeof(msg.msgtype));

		memset(&Msg, 0, sizeof(Msg));
		
		Msg.msg_body = &msg;
		Msg.msg_len = sizeof(msg);
		Msg.msg_done = NULL;
		Msg.msg_private = NULL;
		Msg.msg_ch = comm;

		if (comm->ops->send(comm, &Msg) != IPC_OK) {
			rc = -1;
			rc = EBADF;
		}else if ((err = recoverymgr_getrc()) != 0) {
			errno = err;
			rc = -1;
		}
	}

	/* Destroy and NULL out comm */
	if (comm) {
  		comm->ops->destroy(comm);
		comm = NULL;
	}else{
		errno = ESRCH;
		rc = -1;
	}
	/* Destroy and NULL out attrs */
	if (attrs) {
		g_hash_table_destroy(attrs);
		attrs = NULL;
	}
	
	return rc;
}

