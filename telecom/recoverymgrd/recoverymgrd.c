/* $Id: recoverymgrd.c,v 1.13 2005/03/16 17:11:16 lars Exp $ */
/*
 * Generic Recovery manager implementation
 * 
 * Copyright (c) 2002 Intel Corporation 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 USA 
 *
 * This is a generic implementation of a recovery manager.
 * For the most basic case, the recovery manager will handle 
 * recovery of an application that misses a heartbeat with the
 * apphbd.
 * 
 * The recovery manager plug-in must be loaded by apphbd.
 * The apphbd will send notifications to the recovery manager via
 * the recovery manager plug-in.
 * 
 */

#include <portability.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <apphb.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/ipc.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/longclock.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_poll.h>
#include <clplumbing/uids.h>
#include <clplumbing/recoverymgr_cs.h>
#include <clplumbing/lsb_exitcodes.h>
#include <clplumbing/coredumps.h>
#include <apphb_notify.h>
#include <recoverymgr.h>

#include "recoverymgrd.h"
#include "configfile.h"


/* indicates how many milliseconds between heartbeats */
#define HBINTERVAL_MSEC		2000

#define CONFIG_FILE	"./recoverymgrd.conf"

#define DEBUG 
#define         DBGMIN          1
#define         DBGDETAIL       3
int             debug = 10;
const char* cmdname = "recoverymgrd";
extern FILE* yyin;
/** The main event loop */
GMainLoop*      mainloop = NULL;
GHashTable 	*scripts; /* location for recovery info */
RecoveryInfo 	*current; /* used for inserting to hash table */
int 		eventindex; 
int		length;

/* used for obtaining the basename of the script */
char 		*tempname; 

void 
yyerror(const char *str)
{
        fprintf(stderr,"error parsing config file: %s\n", str);
}

int 
yywrap(void)
{
        return 1;
}

static void 
print_hash(gpointer key, gpointer value, gpointer userdata)
{
	char* key_str = key;
	char* value_str = value;
	cl_log(LOG_INFO, "key[%s], value[%s]", key_str, value_str);
}

gboolean
parseConfigFile(const char* conf_file)
{
   gboolean retval = TRUE;
   scripts = g_hash_table_new(g_str_hash, g_int_equal);
   
   if((yyin = fopen(conf_file,"r")) == NULL){
   	cl_log(LOG_ERR, "Cannot open configure file:[%s]"
			, conf_file);
	;
	return(FALSE);
   }
   
   if(yyparse()){
   	retval = FALSE;
   };
   fclose(yyin);
   if(debug > DBGMIN){
   	g_hash_table_foreach(scripts, print_hash, NULL); 
   }
   return retval;
}


int
main(int argc, char *argv[])
{
   	int rc; 
   	int retval = 0;  
   	const char* conf_file = CONFIG_FILE;
	cl_cdtocoredir();
	
	if(argc == 2){
		conf_file = argv[1];
	}else if(argc > 2){
		printf("Usage: %s [config_file]\n", cmdname);
		exit(LSB_EXIT_NOTCONFIGED);
	}
   	
	cl_log_set_entity(argv[0]);
   	cl_log_enable_stderr(TRUE);
   	cl_log_set_facility(LOG_USER);
   	cl_log(LOG_INFO, "Starting %s", argv[0]);
   	signal(SIGCHLD, sigchld_handler);
	
	if(parseConfigFile(conf_file) == FALSE){
		exit(LSB_EXIT_NOTCONFIGED);
	};

   /* make self a daemon */
#ifndef DEBUG
   	daemon(0,0);
#else  
   	printf("Debug mode -- non daemon\n");
#endif

   	/* register with apphbd as a client and send pings */
   	retval = register_hb();
   	if (0 != retval)
   	{
     		cl_perror("Error registering -- is apphbd running??");
     		exit(retval);
   	}
   
   	create_connection();   

   	/* unregister and shutdown */
   	rc = apphb_unregister();
   	if (rc < 0) 
   	{
      		cl_perror("apphb_unregister failure");
      		exit(3);
   	}

   	return 0; 
}

/**
 * Sends a registration message to the apphbd 
 * and sets the heartbeat interval.
 * Sets up a callback for sending heartbeats.
 */
int 
register_hb(void)
{
   	int rc;

   	rc = apphb_register("recovery manager", "normal");
   	if (rc < 0) 
   	{
      		cl_perror("registration failure");
      		return 1;
   	}

   	rc = apphb_setinterval(HBINTERVAL_MSEC);
   	if (rc < 0) 
   	{
      		cl_perror("setinterval failure");
      		return 2;
   	}

   	setup_hb_callback();

   	return 0;
}

/**
 * Sets up apphb_hb as a signal handler for SIGALRM, 
 * then sets up a timer to go off on the 
 * heartbeat interval
 */
int 
setup_hb_callback(void)
{
   	struct itimerval itimerValue;
	struct itimerval itimerOldValue;

   	signal(SIGALRM, (void (*) (int)) (apphb_hb));

   	itimerValue.it_interval.tv_sec = 0;
   	itimerValue.it_interval.tv_usec = HBINTERVAL_MSEC;
   	itimerValue.it_value.tv_sec = 0;
   	itimerValue.it_value.tv_usec = HBINTERVAL_MSEC;

   	if (setitimer(ITIMER_REAL, &itimerValue, &itimerOldValue)!=0) 
  	{
       		cl_log(LOG_CRIT, "error setting up hb callback");
       		return 1;
   	}

   	return 0;
}

/**
 *
 *@return non-zero on failure
 */
int 
create_connection(void)
{
        char            path[] = IPC_PATH_ATTR;
        char            commpath[] = RECOVERYMGRSOCKPATH;

        struct IPC_WAIT_CONNECTION*     wconn;
        GHashTable*     wconnattrs;


        /* Create a "waiting for connection" object */

        wconnattrs = g_hash_table_new(g_str_hash, g_str_equal);

        g_hash_table_insert(wconnattrs, path, commpath);

        wconn = ipc_wait_conn_constructor(IPC_ANYTYPE, wconnattrs);

        if (wconn == NULL) {
                cl_log(LOG_CRIT, "Unable to create wcon of type %s", IPC_ANYTYPE
);
                return 1;
        }

        /* Create a source to handle new connection requests */
        G_main_add_IPC_WaitConnection(G_PRIORITY_HIGH, wconn
        ,       NULL, FALSE, pending_conn_dispatch, wconn, NULL);

	g_main_set_poll_func(cl_glibpoll);

        /* Create the mainloop and run it... */
        mainloop = g_main_new(FALSE);
        
        g_main_run(mainloop);

        wconn->ops->destroy(wconn);

	/* free script hash table */
	g_hash_table_foreach_remove(scripts, hash_remove_func, NULL);

        /*unlink(PIDFILE); */
        return 0;
}

/**
 * Frees memory allocated in the parser for the hash table
 * contents.
 *
 */
gboolean
hash_remove_func(gpointer key, gpointer value, gpointer user_data)
{
  	if (value)
		free(value);
	/* the key is a pointer to value->appname */

	return TRUE;
}

/**
 * This function is called for every requested connection.
 * This is where we accept it or not.
 */
static gboolean 
pending_conn_dispatch(IPC_Channel* src, gpointer user)
{

        if (debug >= DBGMIN) 
	{
		cl_log(LOG_DEBUG,"received connection request");
                cl_log(LOG_DEBUG, "recoverymgr dispatch: IPC_channel: 0x%x"
                " pid=%d"
                ,       GPOINTER_TO_UINT(src)
                ,       src->farside_pid);
        }

        if (src != NULL) 
	{
                /* This sets up comm channel w/client
                 * Ignoring the result value is OK, because
                 * the client registers itself w/event system.
                 */
                (void)recoverymgr_client_new(src);
        }
	else
	{
                cl_perror("accept_connection failed");
                sleep(1); /* WHY IS THIS HERE? */
        }
        return TRUE;
}

/**
 *  Create new client (we don't know appname or pid yet) .
 * This is called when a connection is first established.
 */
static recoverymgr_client_t*
recoverymgr_client_new(struct IPC_CHANNEL* ch)
{
        recoverymgr_client_t* ret;

        ret = g_new(recoverymgr_client_t, 1);

	memset(ret, 0, sizeof(*ret));
	
        ret->appname = NULL;
        ret->appinst = NULL;
        ret->ch = ch;
        ret->pid = 0;
        ret->deleteme = FALSE;

        ret->rcmsg.msg_body = &ret->rc;
        ret->rcmsg.msg_len = sizeof(ret->rc);
        ret->rcmsg.msg_done = NULL;
        ret->rcmsg.msg_private = NULL;
        ret->rc.rc = 0;

        if (debug >= DBGMIN) {
                cl_log(LOG_DEBUG, "recoverymgr_client_new: channel: 0x%x"
                " pid=%d"
                ,       GPOINTER_TO_UINT(ch)
                ,       ch->farside_pid);
        }
        ret->source = G_main_add_IPC_Channel(G_PRIORITY_DEFAULT
        ,       ch, FALSE, recoverymgr_dispatch, (gpointer)ret
        ,       recoverymgr_client_remove);
        if (!ret->source) {
                memset(ret, 0, sizeof(*ret));
                ret=NULL;
                return ret;
        }

        return ret;
}

/** 
 * This function is called for every incoming message on an
 * existing connection. 
 */
static gboolean
recoverymgr_dispatch(IPC_Channel* src, gpointer Client)
{
        recoverymgr_client_t*         client  = Client;

        if (debug >= DBGDETAIL) {
                cl_log(LOG_DEBUG, "recoverymgr_dispatch: client: %ld"
                ,       (long)client->pid);
        }
        if (client->ch->ch_status == IPC_DISCONNECT) {
                client->deleteme = TRUE;
                return FALSE;
        }

        while (!client->deleteme
        &&      client->ch->ops->is_message_pending(client->ch)) {

                if (client->ch->ch_status == IPC_DISCONNECT) {
                        client->deleteme = TRUE;
                }else{
                        if (!recoverymgr_read_msg(client)) {
                                break;
                        }
                }
        }

        return !client->deleteme;
}

/**
 * Shutdown the connection for this client
 */
void
recoverymgr_client_remove(gpointer Client)
{
        recoverymgr_client_t* client = Client;
        cl_log(LOG_INFO, "recoverymgr_client_remove: client: %ld"
        ,       (long)client->pid);
        if (debug >= DBGMIN) {
                cl_log(LOG_DEBUG, "recoverymgr_client_remove: client pid: %ld"
                ,       (long)client->pid);
        }
        G_main_del_IPC_Channel(client->source);
        g_free(client->appname);
        g_free(client->appinst);
        memset(client, 0, sizeof(*client));
}

/**
 * Selects and calls the appropriate function to handle the message
 * This is called from the dispatch function for each
 * message.
 */
static gboolean
recoverymgr_read_msg(recoverymgr_client_t *client)
{
        struct IPC_MESSAGE*     msg = NULL;

        switch (client->ch->ops->recv(client->ch, &msg)) {

                case IPC_OK:
                recoverymgr_process_msg(client, msg->msg_body, msg->msg_len);
                if (msg->msg_done) {
                        msg->msg_done(msg);
                }
                return TRUE;
                break;


                case IPC_BROKEN:
                client->deleteme = TRUE;
                return FALSE;
                break;


                case IPC_FAIL:
                return FALSE;
                break;
        }
        return FALSE;
}

/*
 * Mappings between commands and strings
 */
struct cmd {
        const char *    msg;
        gboolean        senderrno;
        int             (*fun)(recoverymgr_client_t* client, void* msg, size_t len);
};

/**
 */
struct cmd    cmds[] =
{
        {RECOVERYMGR_EVENT, TRUE, recoverymgr_client_event},
        {RECOVERYMGR_CONNECT, TRUE, recoverymgr_client_connect},
        {RECOVERYMGR_DISCONNECT, TRUE, recoverymgr_client_disconnect},
};

/**
 * Process a message from client process 
 */
void
recoverymgr_process_msg(recoverymgr_client_t* client, void* Msg,  size_t length)
{
        struct recoverymgr_msg *      	msg = Msg;
        const int               	sz1     = sizeof(msg->msgtype)-1;
        int                     	rc      = EINVAL;
        gboolean                	sendrc  = TRUE;
        int                     	j;


        if (length < sizeof(*msg)) {
                return;
        }

        msg->msgtype[sz1] = EOS;


        if (debug >= DBGDETAIL) {
                cl_log(LOG_DEBUG, "recoverymgr_process_msg: client: 0x%x"
                " type=%s"
                ,       GPOINTER_TO_UINT(client)
                ,       msg->msgtype);
        }
        for (j=0; j < DIMOF(cmds); ++j) {
                if (strcmp(msg->msgtype, cmds[j].msg) == 0) {
                        sendrc = cmds[j].senderrno;

                        if (client->appname == NULL
                        &&      cmds[j].fun != recoverymgr_client_connect) {
                                rc = ESRCH;
                                break;
                        }

                        rc = cmds[j].fun(client, Msg, length);
                }
        }
        if (sendrc) {
                if (debug >= DBGMIN) {
                        cl_log(LOG_DEBUG, "recoverymgr_process_msg: client: 0x%x"
                        " type=%s, rc=%d"
                        ,       GPOINTER_TO_UINT(client)
                        ,       msg->msgtype, rc);
                }
                recoverymgr_putrc(client, rc);
        }
}

/**
 * Send return code from current operation back to client.
 */
static void
recoverymgr_putrc(recoverymgr_client_t* client, int rc)
{
        client->rc.rc = rc;

        if (client->ch->ops->send(client->ch, &client->rcmsg) != IPC_OK) {
                client->deleteme = TRUE;
        }
}


/**
 * This function is called when a client issues a connection message.
 * It occurs after a connection is established.
 *
 * @todo Ensure that the app connecting is appbhd (?)
 * 
 * @return EINVAL if message is not a recoverymgr_connectmsg
 */
static int
recoverymgr_client_connect(recoverymgr_client_t *client, void *Msg, size_t length)
{
        struct recoverymgr_connectmsg* 	msg = Msg;
        int                     	namelen = -1;
        /*uid_t                   	uidlist[1];
        gid_t                   	gidlist[1]; 
        IPC_Auth*               	clientauth;*/

        if (debug >= DBGDETAIL) {
		cl_log(LOG_DEBUG, "recoverymgr_client_connect");
	}

        if (client->appname) {
                return EEXIST;
        }

        if (length < sizeof(*msg)
        ||      (namelen = strnlen(msg->appname, sizeof(msg->appname))) < 1
        ||      namelen >= (int)sizeof(msg->appname)
        ||      strnlen(msg->appinstance, sizeof(msg->appinstance))
        >=      sizeof(msg->appinstance)) {
                return EINVAL;
        }

        if (msg->pid < 2 || (CL_KILL(msg->pid, 0) < 0 && errno != EPERM)
        ||      (client->ch->farside_pid != msg->pid)) {
                return EINVAL;
        }

        client->pid = msg->pid;

#if 0
        /* Make sure client is who they claim to be... */
        uidlist[0] = msg->uid;
        gidlist[0] = msg->gid;
        clientauth = ipc_set_auth(uidlist, gidlist, 1, 1);
        if (client->ch->ops->verify_auth(client->ch, clientauth) != IPC_OK) {
                ipc_destroy_auth(clientauth);
                return EINVAL;
        }
        ipc_destroy_auth(clientauth);
#endif 
        client->appname = g_strdup(msg->appname);
        client->appinst = g_strdup(msg->appinstance);
        client->uid = msg->uid;
        client->gid = msg->gid;
        if (debug >= DBGMIN) {
                cl_log(LOG_DEBUG
                ,       "recoverymgr_client_connect: client: [%s]/[%s] pid %ld"
                " (uid,gid) = (%ld,%ld)\n"
                ,       client->appname
                ,       client->appinst
                ,       (long)client->pid
                ,       (long)client->uid
                ,       (long)client->gid);
        }

        return 0;
}

/**
 *  Client requested disconnect 
 */
static int
recoverymgr_client_disconnect(recoverymgr_client_t* client , void * msg, size_t msgsize)
{
        if (debug >= DBGDETAIL) {
		cl_log(LOG_DEBUG, "recoverymgr_client_disconnect");
	}
        client->deleteme=TRUE;
        return 0;
}

/**
 * This message is received when a client
 * of apphbd produces an event.
 * 
 * This function needs to determine if an action should
 * be taken, such as restarting the app, etc. 
 * 
 * @param client The connected client that sent the message
 * @param Msg The message
 * @param msgsize The size of the message 
 *
 * @return zero on success;
 * 	EINVAL if message size is not correct
 *
 * @todo Confirm that the client is the apphbd
 * @todo determine action to take on the given event
 *
 */
static int
recoverymgr_client_event(recoverymgr_client_t *client, void *Msg, size_t msgsize)
{
	struct recoverymgr_event_msg  	*msg = Msg;
 	RecoveryInfo 			*info = NULL;

        if (debug >= DBGDETAIL) 
	{
		cl_log(LOG_CRIT,"recoverymgr_client_event: event=%d", msg->event);
	}

	if (msgsize < sizeof(*msg))
	{
 		return EINVAL;
	}

	/* FINISH HERE  -- confirm that client is the apphbd 
   	   or at least one that is permitted to request recovery (?)*/	
	
        if (debug >= DBGMIN) {
                cl_log(LOG_DEBUG
                ,       "recoverymgr_client_event: client: [%s]/[%s] pid [%ld]"
                " (uid,gid)(%ld,%ld)"
		" message type [%s]"
                ,       msg->appname
                ,       msg->appinstance
                ,       (long)msg->pid
                ,       (long)msg->uid
                ,       (long)msg->gid
		,	msg->msgtype);
        }
	
	info = g_hash_table_lookup(scripts, msg->appname);
	if (NULL == info)	
 	{
		cl_log(LOG_INFO, "No script available to recover %s", msg->appname);
		return 0;
	}

	if (info->event[msg->event].inuse == FALSE)
 	{
		cl_log(LOG_INFO,"Script does not handle this event");

		return 0;
	}
	
        recover_app(info, msg->event); 

  	return 0;
}

/**
 *
 *@return 0 on success
 * 	-1  and errno is ENOMEM 
 */
int 
recover_app(RecoveryInfo *info, int eventindex)
{
	pid_t 	pid;

	pid = fork();
	if (pid < 0)
	{
		cl_log(LOG_CRIT,"Failed to fork recovery process");
		return -1;
	}
	else if (0 == pid)
	{
		child_setup_function(info);
        	if (debug >= DBGDETAIL) 
		{
   			cl_log(LOG_INFO, "current euid[%ld]", (long)geteuid());
   			cl_log(LOG_INFO, "current egid[%ld]", (long)getegid());
			cl_log(LOG_DEBUG,"script = %s", info->scriptname);
			cl_log(LOG_DEBUG,"args = %s", info->event[eventindex].args);
		}
	
		if (eventindex > MAXEVENTS) {
			eventindex = 0;
		}
 	 	if (execl(info->scriptname, info->scriptname,
			  info->event[eventindex].args, (const char *)NULL) < 0)
		{
			cl_perror("Failed to exec recovery script for %s", info->appname);
			_exit(EXIT_FAILURE);
		}
	}

   	return 0;
}

/**
 *  Attempts to set the uid and gid of the executing script.
 *
 * @param a_user_data A pointer to a RecoveryInfo structure
 */
void 
child_setup_function(RecoveryInfo *info)
{
	if (info == NULL)
   	{
        	if (debug >= DBGDETAIL) 
		{
      			cl_log(LOG_INFO,"Unable to get app data for recovery.");
		}
      		return;
   	}
   	/* Change both uid, and euid since we are root */
	if ( 0 != setgid(info->gid))
   	{
        	if (debug >= DBGDETAIL) 
		{
			cl_log(LOG_INFO, "error:[%s]", strerror(errno));
      			cl_log(LOG_INFO, "Unable to setgid for recovery of %s", info->appname);
		}
   	}
	if ( 0 != setuid(info->uid))
   	{
        	if (debug >= DBGDETAIL) 
		{
			cl_log(LOG_INFO, "error:[%s]", strerror(errno));
			cl_log(LOG_INFO, "Unable to setuid for recovery of %s", info->appname);
		}
   	}
}

/**
 * Used to avoid zombie processes
 */
void
sigchld_handler (int signum)
{
	int pid;
  	int status;
  	while (1)
    	{
      		pid = waitpid ((-1), &status, WNOHANG);
      		if (pid < 0)
       		{
          		break;
        	}
      		if (pid == 0)
	        	break;
    	}
}

