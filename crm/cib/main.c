/* $Id: main.c,v 1.19 2005/02/25 15:26:31 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <hb_api.h>
#include <clplumbing/uids.h>
#include <clplumbing/coredumps.h>
#include <clplumbing/Gmain_timeout.h>

/* #include <portability.h> */
#include <ocf/oc_event.h>
/* #include <ocf/oc_membership.h> */

#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/ipc.h>
#include <crm/common/ctrl.h>
#include <crm/common/xml.h>
#include <crm/common/msg.h>

#include <cibio.h>
#include <callbacks.h>

#include <crm/dmalloc_wrapper.h>


extern void oc_ev_special(const oc_ev_t *, oc_ev_class_t , int );

GMainLoop*  mainloop = NULL;
const char* crm_system_name = CRM_SYSTEM_CIB;
char *cib_our_uname = NULL;
oc_ev_t *cib_ev_token;

void usage(const char* cmd, int exit_status);
int init_start(void);
gboolean cib_register_ha(ll_cluster_t *hb_cluster, const char *client_name);
gboolean cib_shutdown(int nsig, gpointer unused);
void cib_ha_connection_destroy(gpointer user_data);
gboolean startCib(const char *filename);
extern gboolean cib_msg_timeout(gpointer data);

ll_cluster_t *hb_conn = NULL;

#define OPTARGS	"hV"

int
main(int argc, char ** argv)
{
	int argerr = 0;
	int flag;

	crm_log_init(crm_system_name);
	G_main_add_SignalHandler(
		G_PRIORITY_HIGH, SIGTERM, cib_shutdown, NULL, NULL);

	client_list = g_hash_table_new(g_str_hash, g_str_equal);
	peer_hash = g_hash_table_new(g_str_hash, g_str_equal);
	
	while ((flag = getopt(argc, argv, OPTARGS)) != EOF) {
		switch(flag) {
			case 'V':
				alter_debug(DEBUG_INC);
				break;
			case 'h':		/* Help message */
				usage(crm_system_name, LSB_EXIT_OK);
				break;
			default:
				++argerr;
				break;
		}
	}

	if (optind > argc) {
		++argerr;
	}
    
	if (argerr) {
		usage(crm_system_name,LSB_EXIT_GENERIC);
	}
    
	/* read local config file */
	return init_start();
}


int
init_start(void)
{
	gboolean was_error = FALSE;

	hb_conn = ll_cluster_new("heartbeat");
	if(cib_register_ha(hb_conn, CRM_SYSTEM_CIB) == FALSE) {
		crm_crit("Cannot sign in to heartbeat... terminating");
		fprintf(stderr, "Cannot sign in to heartbeat... terminating");
		exit(1);
	}

	if(startCib(CIB_FILENAME) == FALSE){
		crm_crit("Cannot start CIB... terminating");
		exit(1);
	}
	
	was_error = init_server_ipc_comms(
		crm_strdup(cib_channel_callback), cib_client_connect,
		default_ipc_connection_destroy);

	was_error = was_error || init_server_ipc_comms(
		crm_strdup(cib_channel_ro), cib_client_connect,
		default_ipc_connection_destroy);

	was_error = was_error || init_server_ipc_comms(
		crm_strdup(cib_channel_rw), cib_client_connect,
		default_ipc_connection_destroy);


	if(was_error == FALSE) {
		crm_devel("Be informed of CRM Client Status changes");
		if (HA_OK != hb_conn->llc_ops->set_cstatus_callback(
			    hb_conn, cib_client_status_callback, hb_conn)) {
			
			crm_err("Cannot set cstatus callback: %s\n",
				hb_conn->llc_ops->errmsg(hb_conn));
			was_error = TRUE;
		} else {
			crm_devel("Client Status callback set");
		}
	}

	if(was_error == FALSE) {
		gboolean did_fail = TRUE;
		int num_ccm_fails = 0;
		int max_ccm_fails = 30;
		int ret;
		int cib_ev_fd;
		
		while(did_fail && was_error == FALSE) {
			did_fail = FALSE;
			crm_devel("Registering with CCM");
			ret = oc_ev_register(&cib_ev_token);
			if (ret != 0) {
				crm_warn("CCM registration failed");
				did_fail = TRUE;
			}
			
			if(did_fail == FALSE) {
				crm_devel("Setting up CCM callbacks");
				ret = oc_ev_set_callback(
					cib_ev_token, OC_EV_MEMB_CLASS,
					cib_ccm_msg_callback, NULL);
				if (ret != 0) {
					crm_warn("CCM callback not set");
					did_fail = TRUE;
				}
			}
			if(did_fail == FALSE) {
				oc_ev_special(cib_ev_token, OC_EV_MEMB_CLASS, 0);
				
				crm_devel("Activating CCM token");
				ret = oc_ev_activate(cib_ev_token, &cib_ev_fd);
				if (ret != 0){
					crm_warn("CCM Activation failed");
					did_fail = TRUE;
				}
			}
			
			if(did_fail) {
				num_ccm_fails++;
				oc_ev_unregister(cib_ev_token);
				
				if(num_ccm_fails < max_ccm_fails){
					crm_warn("CCM Connection failed"
						 " %d times (%d max)",
						 num_ccm_fails, max_ccm_fails);
					sleep(1);
					
				} else {
					crm_err("CCM Activation failed %d (max) times",
						num_ccm_fails);
					was_error = TRUE;
					
				}
			}
		}

		crm_devel("CCM Activation passed... all set to go!");
		G_main_add_fd(G_PRIORITY_LOW, cib_ev_fd, FALSE, cib_ccm_dispatch,
			      cib_ev_token, default_ipc_connection_destroy);
	}

	if(was_error == FALSE) {
		/* Async get client status information in the cluster */
		crm_devel("Requesting an initial dump of CRMD client_status");
		hb_conn->llc_ops->client_status(
			hb_conn, NULL, CRM_SYSTEM_CRMD, -1);

		/* Create the mainloop and run it... */
		mainloop = g_main_new(FALSE);
		crm_info("Starting %s mainloop", crm_system_name);
		
		Gmain_timeout_add(1000, cib_msg_timeout, NULL);
		
		g_main_run(mainloop);
		return_to_orig_privs();

	} else {
		crm_err("Couldnt start all communication channels, exiting.");
	}
	
	return 0;
}

void
usage(const char* cmd, int exit_status)
{
	FILE* stream;

	stream = exit_status ? stderr : stdout;

	fprintf(stream, "usage: %s [-srkh]"
		"[-c configure file]\n", cmd);
/* 	fprintf(stream, "\t-d\tsets debug level\n"); */
/* 	fprintf(stream, "\t-s\tgets daemon status\n"); */
/* 	fprintf(stream, "\t-r\trestarts daemon\n"); */
/* 	fprintf(stream, "\t-k\tstops daemon\n"); */
/* 	fprintf(stream, "\t-h\thelp message\n"); */
	fflush(stream);

	exit(exit_status);
}


gboolean
cib_register_ha(ll_cluster_t *hb_cluster, const char *client_name)
{
	int facility;
	char *param_val = NULL;
	const char *uname = NULL;
	const char *param_name = NULL;
	
	if(safe_val3(NULL, hb_cluster, llc_ops, errmsg) == NULL) {
		crm_crit("cluster errmsg function unavailable");
	}
	
	crm_info("Signing in with Heartbeat");
	if (hb_cluster->llc_ops->signon(hb_cluster, client_name)!= HA_OK) {

		crm_err("Cannot sign on with heartbeat: %s",
			hb_cluster->llc_ops->errmsg(hb_cluster));
		return FALSE;
	}

	/* change the logging facility to the one used by heartbeat daemon */
	crm_info("Switching to Heartbeat logger");
	if (( facility =
	      hb_cluster->llc_ops->get_logfacility(hb_cluster)) > 0) {
		cl_log_set_facility(facility);
 	}	
	crm_verbose("Facility: %d", facility);	

	param_name = KEY_LOGFILE;
	param_val = hb_cluster->llc_ops->get_parameter(hb_cluster, param_name);
	crm_info("%s = %s", param_name, param_val);
	if(param_val != NULL) {
		cl_log_set_logfile(param_val);
		cl_free(param_val);
		param_val = NULL;
	}
	param_name = KEY_DBGFILE;
	param_val = hb_cluster->llc_ops->get_parameter(hb_cluster, param_name);
	if(param_val != NULL) {
		cl_log_set_debugfile(param_val);
		cl_free(param_val);
		param_val = NULL;
	}
	param_name = KEY_DEBUGLEVEL;
	param_val = hb_cluster->llc_ops->get_parameter(hb_cluster, param_name);
	crm_info("%s = %s", param_name, param_val);
	if(param_val != NULL) {
		cl_free(param_val);
		param_val = NULL;
	}
	
	crm_devel("Be informed of CIB messages");
	if (HA_OK != hb_cluster->llc_ops->set_msg_callback(
		    hb_cluster, T_CIB, cib_peer_callback, hb_cluster)){
		
		crm_err("Cannot set msg callback: %s",
			hb_cluster->llc_ops->errmsg(hb_cluster));
		return FALSE;
	}

	crm_devel("Finding our node name");
	if ((uname = hb_cluster->llc_ops->get_mynodeid(hb_cluster)) == NULL) {
		crm_err("get_mynodeid() failed");
		return FALSE;
	}
	
	cib_our_uname = crm_strdup(uname);
	crm_info("FSA Hostname: %s", cib_our_uname);

	crm_devel("Adding channel to mainloop");
	G_main_add_IPC_Channel(
		G_PRIORITY_HIGH, hb_cluster->llc_ops->ipcchan(hb_cluster),
		FALSE, cib_ha_dispatch, hb_cluster /* userdata  */,  
		cib_ha_connection_destroy);

	return TRUE;
    
}

void
cib_ha_connection_destroy(gpointer user_data)
{
}

gboolean
cib_shutdown(int nsig, gpointer unused)
{
	static int shuttingdown = 0;
  
	if (!shuttingdown) {
		shuttingdown = 1;
	}
	if (mainloop != NULL && g_main_is_running(mainloop)) {
		g_main_quit(mainloop);
	} else {
		exit(LSB_EXIT_OK);
	}
	return TRUE;
}

gboolean
startCib(const char *filename)
{
	crm_data_t *cib = readCibXmlFile(filename);
	if (initializeCib(cib)) {
		crm_info("CIB Initialization completed successfully");
	} else { 
		/* free_xml(cib); */
		crm_warn("CIB Initialization failed, "
			 "starting with an empty default.");
		activateCibXml(createEmptyCib(), filename);
	}
	return TRUE;
}
