/* $Id: ccmmain.c,v 1.17 2004/08/29 03:01:14 msoffen Exp $ */
/* 
 * ccm.c: Consensus Cluster Service Program 
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
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
#include <ccm.h>
#include <clplumbing/cl_signal.h>

#define SECOND   1000
#define OPTARGS  "dv"

int global_debug=0;
int global_verbose=0;

/*
 * hearbeat event source.
 *   
 */
static gboolean hb_input_dispatch(int, gpointer);
static void hb_input_destroy(gpointer);
static gboolean hb_timeout_dispatch(gpointer);

typedef struct hb_usrdata_s {
	void		*ccmdata;
	GMainLoop	*mainloop;
} hb_usrdata_t;

static gboolean
hb_input_dispatch(int fd, gpointer user_data)
{
	if (ccm_take_control(((hb_usrdata_t *)user_data)->ccmdata)) {
		g_main_quit(((hb_usrdata_t *)user_data)->mainloop);
		return FALSE;
	}
	return TRUE;
}

static void
hb_input_destroy(gpointer user_data)
{
	/* close connections to all the clients */
	client_delete_all();
	return;
}

static gboolean
hb_timeout_dispatch(gpointer user_data)
{	
	if(global_debug) {
		ccm_check_memoryleak();
	}
	return hb_input_dispatch(-1, user_data);
}


/*
 * client messaging  events sources...
 *   
 */
static gboolean clntCh_input_dispatch(IPC_Channel *
		,       gpointer);
static void clntCh_input_destroy(gpointer );

static gboolean
clntCh_input_dispatch(IPC_Channel *client, 
	      gpointer        user_data)
{
	if(client->ch_status == IPC_DISCONNECT){
		cl_log(LOG_INFO, "dispatch:received HUP");
		client_delete(client);
		return FALSE;
	}
	return TRUE; /* TOBEDONE */
}


static void
clntCh_input_destroy(gpointer user_data)
{
	cl_log(LOG_INFO, "clntCh_input_destroy:received HUP");
	return;
}



/*
 * client connection events source..
 *   
 */
static gboolean waitCh_input_dispatch(IPC_Channel *, gpointer);
static void waitCh_input_destroy(gpointer);

static gboolean
waitCh_input_dispatch(IPC_Channel *newclient, gpointer user_data)
{
	client_add(newclient);

	G_main_add_IPC_Channel(G_PRIORITY_LOW, newclient, FALSE, 
				clntCh_input_dispatch, newclient, 
				clntCh_input_destroy);
	return TRUE;
}

static void
waitCh_input_destroy(gpointer user_data)
{
	IPC_WaitConnection *wait_ch = 
			(IPC_WaitConnection *)user_data;

	wait_ch->ops->destroy(wait_ch);
	return;
}

static IPC_WaitConnection *
wait_channel_init(void)
{
	IPC_WaitConnection *wait_ch;
	mode_t mask;
	char path[] = IPC_PATH_ATTR;
	char ccmfifo[] = CCMFIFO;
	char domainsocket[] = IPC_DOMAIN_SOCKET;

	GHashTable * attrs = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(attrs, path, ccmfifo);

	mask = umask(0);
	wait_ch = ipc_wait_conn_constructor(domainsocket, attrs);
	if (wait_ch == NULL){
		cl_perror("Can't create wait channel");
		exit(1);
	}
	mask = umask(mask);

	g_hash_table_destroy(attrs);

	return wait_ch;
}

static void
usage(const char *cmd)
{
	fprintf(stderr, "\nUsage: %s [-dv]\n", cmd);
}


/* */
/* debug facilitator. */
/* */
static void
ccm_debug(int signum) 
{
	switch(signum) {
	case SIGUSR1:
		global_debug = !global_debug;
		break;
	case SIGUSR2:
		global_verbose = !global_verbose;
		break;
	}

	if(global_debug || global_verbose){
		cl_log_enable_stderr(TRUE);
	} else {
		cl_log_enable_stderr(FALSE);
	}

}


/* */
/* The main function! */
/* */
int
main(int argc, char **argv)
{
	IPC_WaitConnection *wait_ch;

	char *cmdname;
	char *tmp_cmdname = strdup(argv[0]);
	int  flag;
	hb_usrdata_t	usrdata;

	if ((cmdname = strrchr(tmp_cmdname, '/')) != NULL) {
		++cmdname;
	} else {
		cmdname = tmp_cmdname;
	}
	cl_log_set_entity(cmdname);
	cl_log_set_facility(LOG_DAEMON);
	while ((flag = getopt(argc, argv, OPTARGS)) != EOF) {
		switch(flag) {
			case 'v':
				global_verbose = 1;
				break;
			case 'd': 
				global_debug = 1;
				break;
			default:
				usage(cmdname);
				return 1;
		}
	}
	if(global_verbose || global_debug)
		cl_log_enable_stderr(TRUE);


	CL_SIGNAL(SIGUSR1, ccm_debug);
	CL_SIGNAL(SIGUSR2, ccm_debug);
	CL_IGNORE_SIG(SIGPIPE);

	/* initialize the client tracking system */
	client_init();

	usrdata.mainloop = g_main_new(TRUE);

	/* 
	 * heartbeat is the main source of events. 
	 * This source must be listened 
	 * at high priority 
	 */
	usrdata.ccmdata = ccm_initialize();
	if(!usrdata.ccmdata) {
		exit(1);
	}

	/* we want hb_input_dispatch to be called when some input is
	 * pending on the heartbeat fd, and every 1 second 
	 */
	G_main_add_fd(G_PRIORITY_HIGH, ccm_get_fd(usrdata.ccmdata), 
			FALSE, hb_input_dispatch, &usrdata, hb_input_destroy);
	Gmain_timeout_add_full(G_PRIORITY_HIGH, SECOND, hb_timeout_dispatch,
				&usrdata, hb_input_destroy);

	/* the clients wait channel is the other source of events.
	 * This source delivers the clients connection events.
	 * listen to this source at a relatively lower priority.
	 */
	wait_ch = wait_channel_init();
	G_main_add_IPC_WaitConnection(G_PRIORITY_LOW, wait_ch, NULL,
		FALSE, waitCh_input_dispatch, wait_ch,
		waitCh_input_destroy);


	g_main_run(usrdata.mainloop);
	g_main_destroy(usrdata.mainloop);

	free(tmp_cmdname);
	/*this program should never terminate,unless killed*/
	return(1);
}
