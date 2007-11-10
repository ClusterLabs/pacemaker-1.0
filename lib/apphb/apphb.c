/*
 * apphb.c: application heartbeat library code.
 *
 * Copyright (C) 2002 Alan Robertson <alanr@unix.sh>
 *
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
#include <lha_internal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <apphb.h>
#define	time	footime
#define	index	fooindex
#include	<glib.h>
#undef time
#undef index

#include <clplumbing/ipc.h>
#include <clplumbing/apphb_cs.h>

#include <stdio.h>

#ifdef THREAD_SAFE

#define G_STATIC_MUTEX_LOCK(lock) g_static_mutex_lock(lock)
#define G_STATIC_MUTEX_UNLOCK(lock) g_static_mutex_unlock(lock)

/*
 * G_DECLARE_STATIC_MUTEX():
 *	An empty ";" declaration in code breaks some compilers.
 *	So this non-empty macro definition supplies its trailing ";".
 *	Uses of this macro should not include the ";".
 */
#define G_DECLARE_STATIC_MUTEX(var)	\
	static GStaticMutex var = G_STATIC_MUTEX_INIT;

#define G_THREAD_INIT(vtable)		\
	if ( !g_thread_supported() ) {	\
		g_thread_init(vtable);	\
	}

#else

#define G_STATIC_MUTEX_LOCK(lock)
#define G_STATIC_MUTEX_UNLOCK(lock)
#define G_DECLARE_STATIC_MUTEX(var)
#define G_THREAD_INIT(vtable)

#endif

static struct IPC_CHANNEL*	hbcomm = NULL;
static GHashTable *		hbattrs;
static int			hbstatus = -1;

static int apphb_getrc(void);

/* Get return code from last operation */
static int
apphb_getrc(void)
{
	G_DECLARE_STATIC_MUTEX(lock)
	struct apphb_rc * rcs;
	int		rc;
	struct IPC_MESSAGE * msg;

	G_STATIC_MUTEX_LOCK(&lock);
	hbcomm->ops->waitin(hbcomm);
	if (hbcomm->ops->recv(hbcomm, &msg) != IPC_OK) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		perror("Receive failure:");
		return errno;
	}
	G_STATIC_MUTEX_UNLOCK(&lock);

	rcs = msg->msg_body;
	rc = rcs->rc;
	msg->msg_done(msg);

	return rc;
}

/* Register for application heartbeat services */
int
apphb_register(const char * appname, const char * appinstance)
{
	G_DECLARE_STATIC_MUTEX(lock)
	int	err;
	struct IPC_MESSAGE Msg;
	struct apphb_signupmsg msg;
	static char path [] = IPC_PATH_ATTR;
	static char sockpath []	= APPHBSOCKPATH;

	G_THREAD_INIT(NULL);

	if (appname == NULL || appinstance == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (strlen(appname) >= APPHB_OLEN
	||	strlen(appinstance) >= APPHB_OLEN) {
		errno = ENAMETOOLONG;
		return -1;
	}

	G_STATIC_MUTEX_LOCK(&lock);
	if (hbcomm != NULL) {
		errno = EEXIST;
		G_STATIC_MUTEX_UNLOCK(&lock);
		return -1;
	}

	/* Create communication channel with server... */

	hbattrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(hbattrs, path, sockpath);
  
	hbcomm = ipc_channel_constructor(IPC_ANYTYPE, hbattrs);
  
	if (hbcomm == NULL
	||	(hbstatus = hbcomm->ops->initiate_connection(hbcomm)
	!=	IPC_OK)) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		apphb_unregister();
  		errno = EBADF;
		return -1;
	}

	/* Send registration message ... */
	strncpy(msg.msgtype, REGISTER, sizeof(msg.msgtype));
	strncpy(msg.appname, appname, sizeof(msg.appname));
	strncpy(msg.appinstance, appinstance, sizeof(msg.appinstance));
	/* Maybe we need current starting directory instead of 
	 * current work directory. 
	 */
	if ( getcwd(msg.curdir, APPHB_OLEN) == NULL)  {
		apphb_unregister();
		G_STATIC_MUTEX_UNLOCK(&lock);
		return -1;
	}
	msg.pid = getpid();
	msg.uid = getuid();
	msg.gid = getgid();
	
	memset(&Msg, 0, sizeof(Msg));

	Msg.msg_buf = NULL;
	Msg.msg_body = &msg;
	Msg.msg_len = sizeof(msg);
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = hbcomm;

	if (hbcomm->ops->send(hbcomm, &Msg) != IPC_OK) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		apphb_unregister();
  		errno = EBADF;
		return -1;
	}
	G_STATIC_MUTEX_UNLOCK(&lock);

	if ((err = apphb_getrc()) != 0) {
		hbstatus = -1;
		errno = err;
		return -1;
	}
	return 0;
}

/* Unregister from application heartbeat services */
int
apphb_unregister(void)
{
	G_DECLARE_STATIC_MUTEX(lock)
	int	rc = 0;
	int	err;
	struct apphb_msg msg;
	struct IPC_MESSAGE Msg;

	G_THREAD_INIT(NULL);
	G_STATIC_MUTEX_LOCK(&lock);
	if (hbcomm == NULL || hbstatus != IPC_OK) {
		errno = ESRCH;
		rc = -1;
	}

	/* Send an unregister message to the server... */
	if (hbcomm != NULL && hbstatus == IPC_OK) {
		strncpy(msg.msgtype, UNREGISTER, sizeof(msg.msgtype));

		memset(&Msg, 0, sizeof(Msg));
	
		Msg.msg_buf = NULL;
		Msg.msg_body = &msg;
		Msg.msg_len = sizeof(msg);
		Msg.msg_done = NULL;
		Msg.msg_private = NULL;
		Msg.msg_ch = hbcomm;

		if (hbcomm->ops->send(hbcomm, &Msg) != IPC_OK) {
			rc = -1;
			rc = EBADF;
		}else {
			if ((err = apphb_getrc()) != 0) {
				errno = err;
				rc = -1;
			}
		}
	}

	/* Destroy and NULL out hbcomm */
	if (hbcomm) {
  		hbcomm->ops->destroy(hbcomm);
		hbcomm = NULL;
	}else{
		errno = ESRCH;
		rc = -1;
	}

	/* Destroy and NULL out hbattrs */
	if (hbattrs) {
		g_hash_table_destroy(hbattrs);
		hbattrs = NULL;
	}

	G_STATIC_MUTEX_LOCK(&lock);
	
	return rc;
}

/* Set application heartbeat interval (in milliseconds) */
int
apphb_setinterval(unsigned long hbms)
{
	G_DECLARE_STATIC_MUTEX(lock)
	struct apphb_msmsg	msg;
	struct IPC_MESSAGE	Msg;
	int			err;

	G_THREAD_INIT(NULL);
	G_STATIC_MUTEX_LOCK(&lock);
	if (hbcomm == NULL || hbstatus != IPC_OK) {
		errno = ESRCH;
		G_STATIC_MUTEX_UNLOCK(&lock);
		return -1;
	}

	strncpy(msg.msgtype, SETINTERVAL, sizeof(msg.msgtype));
	msg.ms = hbms;

	memset(&Msg, 0, sizeof(Msg));
	
	Msg.msg_buf = NULL;
	Msg.msg_body = &msg;
	Msg.msg_len = sizeof(msg);
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = hbcomm;

	if (hbcomm->ops->send(hbcomm, &Msg) != IPC_OK) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		errno = EBADF;
		return -1;
	}
	G_STATIC_MUTEX_UNLOCK(&lock);

	if ((err = apphb_getrc()) != 0) {
		errno = err;
		return -1;
	}
	return  0;
}
/* Set application heartbeat warning time (in milliseconds) */
int
apphb_setwarn(unsigned long hbms)
{
	G_DECLARE_STATIC_MUTEX(lock)
	struct apphb_msmsg	msg;
	struct IPC_MESSAGE	Msg;
	int			err;

	if (hbms <= 0) {
		errno = EINVAL;
		return -1;
	}

	G_THREAD_INIT(NULL);
	G_STATIC_MUTEX_LOCK(&lock);
	if (hbcomm == NULL || hbstatus != IPC_OK) {
		errno = ESRCH;
		G_STATIC_MUTEX_UNLOCK(&lock);
		return -1;
	}
	strncpy(msg.msgtype, SETWARNTIME, sizeof(msg.msgtype));
	msg.ms = hbms;

	memset(&Msg, 0, sizeof(Msg));
	
	Msg.msg_buf = NULL;
	Msg.msg_body = &msg;
	Msg.msg_len = sizeof(msg);
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = hbcomm;

	if (hbcomm->ops->send(hbcomm, &Msg) != IPC_OK) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		errno = EBADF;
		return -1;
	}
	G_STATIC_MUTEX_UNLOCK(&lock);

	if ((err = apphb_getrc()) != 0) {
		errno = err;
		return -1;
	}
	return  0;
}
int
apphb_setreboot(unsigned int truefalse)
{
	G_DECLARE_STATIC_MUTEX(lock)
	struct apphb_msmsg	msg;
	struct IPC_MESSAGE	Msg;
	int			err;


	G_THREAD_INIT(NULL);
	G_STATIC_MUTEX_LOCK(&lock);
	if (hbcomm == NULL || hbstatus != IPC_OK) {
		errno = ESRCH;
		G_STATIC_MUTEX_UNLOCK(&lock);
		return -1;
	}
	strncpy(msg.msgtype, SETREBOOT, sizeof(msg.msgtype));
	msg.ms = truefalse ? 1UL : 0UL;

	memset(&Msg, 0, sizeof(Msg));
	
	Msg.msg_buf = NULL;
	Msg.msg_body = &msg;
	Msg.msg_len = sizeof(msg);
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = hbcomm;

	if (hbcomm->ops->send(hbcomm, &Msg) != IPC_OK) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		errno = EBADF;
		return -1;
	}
	G_STATIC_MUTEX_UNLOCK(&lock);

	if ((err = apphb_getrc()) != 0) {
		errno = err;
		return -1;
	}
	return  0;
}
/* Perform application heartbeat */
int
apphb_hb(void)
{
	G_DECLARE_STATIC_MUTEX(lock)
	struct apphb_msg msg;
	struct IPC_MESSAGE	Msg;

	G_THREAD_INIT(NULL);
	G_STATIC_MUTEX_LOCK(&lock);
	if (hbcomm == NULL || hbstatus != IPC_OK) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		errno = ESRCH;
		return -1;
	}
	strncpy(msg.msgtype, HEARTBEAT, sizeof(msg.msgtype));
	
	memset(&Msg, 0, sizeof(Msg));
	
	Msg.msg_buf = NULL;
	Msg.msg_body = &msg;
	Msg.msg_len = sizeof(msg);
	Msg.msg_done = NULL;
	Msg.msg_private = NULL;
	Msg.msg_ch = hbcomm;

	if (hbcomm->ops->send(hbcomm, &Msg) != IPC_OK) {
		G_STATIC_MUTEX_UNLOCK(&lock);
		errno = EBADF;
		return -1;
	}
	G_STATIC_MUTEX_UNLOCK(&lock);
	/* NOTE: we do not expect a return code from server */
	return 0;
}
