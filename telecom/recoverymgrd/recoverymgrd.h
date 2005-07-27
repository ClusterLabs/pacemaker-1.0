/* $Id: recoverymgrd.h,v 1.5 2005/07/27 09:03:24 panjiam Exp $ */
/*
 * Generic Recovery manager library header file
 * 
 * Copyright (c) 2002 Intel Corporation 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _RECMGRLIB_H
#define _RECMGRLIB_H

#include <clplumbing/cl_log.h>
#include <clplumbing/GSource.h>
#include <clplumbing/ipc.h>
#include <clplumbing/recoverymgr_cs.h>
#include <apphb.h>

#include "configfile.h"

#define EOS	'\0'

typedef struct recoverymgr_client recoverymgr_client_t;
/*
 * Per-client data structure.
 */
struct recoverymgr_client {
        char *                  appname;        /* application name */
        char *                  appinst;        /* application name */
        pid_t                   pid;            /* application pid */
        uid_t                   uid;            /* application UID */
        gid_t                   gid;            /* application GID */
        GCHSource*              source;
        IPC_Channel*            ch;
        struct IPC_MESSAGE      rcmsg;          /* return code msg */
        struct recoverymgr_rc   rc;             /* last return code */
        gboolean                deleteme;       /* Delete after next call */
};



/* function prototypes */
void sigchld_handler(int signum);
int register_hb(void);
int setup_hb_callback(void);
int create_connection(void);
static gboolean pending_conn_dispatch(IPC_Channel* src, gpointer user);
static recoverymgr_client_t* recoverymgr_client_new(struct IPC_CHANNEL* ch);
static gboolean recoverymgr_dispatch(IPC_Channel* src, gpointer Client);
void recoverymgr_client_remove(gpointer Client);
static gboolean recoverymgr_read_msg(recoverymgr_client_t *client);
void recoverymgr_process_msg(recoverymgr_client_t* client, void* Msg,  size_t length);
static int recoverymgr_client_connect(recoverymgr_client_t *client, void *Msg, size_t length);
static int recoverymgr_client_disconnect(recoverymgr_client_t* client , 
		void * msg, size_t msgsize);
static int recoverymgr_client_event(recoverymgr_client_t *client, void *Msg, size_t msgsize);
static void recoverymgr_putrc(recoverymgr_client_t *client, int rc);
int recover_app(RecoveryInfo *info, int eventindex);
void child_setup_function(RecoveryInfo *info);
gboolean hash_remove_func(gpointer key, gpointer value, gpointer user_data);

gboolean parseConfigFile(const char* conf_file);
void yyerror(const char *str);
int yywrap(void);
extern int yyparse(void);

#endif /* _RECMGRLIB_H */

