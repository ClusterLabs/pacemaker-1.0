/*
 * CIM Provider
 * 
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include <clplumbing/cl_malloc.h>
#include "cmpi_utils.h"
#include "cmpi_ha_indication.h"

#define DEFAULT_TIME_OUT 5

#define HB_CLIENT_ID "cmpi_indication"

static int keep_running = 0;


/*******************************************************************
 * Indication specific functions
 ******************************************************************/

static CMPIInstance * source_instance = NULL;
/*
static CMPIInstance * previous_Instance = NULL;
*/

#define INSTMODIFICATION 	"CIM_InstModification"
#define INDNAMESPACE		"root/cimv2"

typedef struct {
	CMPIBroker * broker;
	CMPIContext * context;
        CMPIString * filter;
} cmpi_ind_env_t;


static cmpi_ind_env_t *  ind_env = NULL;

/* 
static const char * instmod_properties [] = {
	"IndicationIdentifier",
	"CorrelatedIndications",
	"IndicationTime",
	"SourceInstance",
	"PreviousInstance"
};
*/

int 
nodestatus_event_handler(const char * node, const char *status)
{

	CMPIInstance * instance = NULL;
	CMPIObjectPath * op = NULL;
	CMPIStatus rc;
        char * ind_namespace = NULL;
        char * ind_classname = NULL;

	cl_log(LOG_INFO, "%s: nodestatus changed", __FUNCTION__);

        ind_namespace = strdup(INDNAMESPACE);
        ind_classname = strdup(INSTMODIFICATION);

	if ( ind_env ){
                char source_instance_property[] = "SourceInstance";

		op = CMNewObjectPath(ind_env->broker, ind_namespace,
					ind_classname, &rc);

		instance = CMNewInstance(ind_env->broker, op, &rc);

		CMSetProperty(instance, source_instance_property, 
                                        source_instance, CMPI_instance); 
		
		CBDeliverIndication(ind_env->broker, ind_env->context, 
					ind_namespace, instance);

	}

        free(ind_namespace);
        free(ind_classname);

	return HA_OK;
}

int 
ifstatus_event_handler(const char * node, const char * lnk, const char * status)
{

	cl_log(LOG_INFO, "%s: ifstatus changed", __FUNCTION__);
	return HA_OK;
}

int 
membership_event_handler(const char *node, SaClmClusterChangesT status)
{
	cl_log(LOG_INFO, "%s: membership changed", __FUNCTION__);
	return HA_OK;
}

static GThread * g_ind_thread = NULL;
static gpointer ind_thread_func(gpointer args);

int 
cluster_indication_initialize(char * classname, CMPIBroker * broker, 
                CMPIContext * context, 
                CMPISelectExp * filter, CMPIStatus * rc)
{
        void ** data = NULL;
        
        DEBUG_ENTER();
	ind_env = malloc(sizeof(cmpi_ind_env_t));

	ind_env->broker = broker;
	ind_env->context = context;
	ind_env->filter = CMGetSelExpString(filter, rc);

	linuxha_register_event_handler(&nodestatus_event_handler, 
                        &ifstatus_event_handler, &membership_event_handler);


	data = (void **)malloc(sizeof(CMPIBroker*) + sizeof(CMPIContext*));

	data[0] = broker;
	data[1] = context;
	
        if (!g_thread_supported ()) {
                g_thread_init (NULL);
        }

        g_ind_thread = g_thread_create(ind_thread_func, 
                                (gpointer)data, FALSE, NULL); 


        DEBUG_LEAVE();

	return HA_OK;
}


int 
cluster_indication_finalize(char * classname, CMPIBroker * broker, 
                CMPIContext * context, 
                CMPISelectExp * filter, CMPIStatus * rc)
{
        return HA_OK;
}


static void
stop_server(int a) {
    	keep_running = 0;
}


static gpointer
ind_thread_func(gpointer args)
{
	int ret;

	fd_set fdset;
	struct timeval tv, * tvp = NULL;

	int numfds, hb_fd = 0, mem_fd = 0;
	int hb_already_dead = 0;
	int clm_initialized = 0;
	

        DEBUG_ENTER();
        DEBUG_PID();

	if ( ! get_hb_initialized() ) {
		if ( linuxha_initialize(HB_CLIENT_ID, 0) != HA_OK ){
                        DEBUG_LEAVE();
			return NULL;
		}
	}


	if ((ret = init_resource_table()) != HA_OK) {
	    	cl_log(LOG_ERR, "resource table initialization failure.");
	}

	ret = init_membership();
	hb_fd =  get_heartbeat_fd();
	mem_fd = get_membership_fd();


	if (ret != HA_OK) {
		cl_log(LOG_ERR, "fatal error during membership initialization. ");
	}  


	/* In case we recevie a request to stop (kill -TERM or kill -INT) */
	keep_running = 1;
	signal(SIGTERM, stop_server);
	signal(SIGINT, stop_server);

	while (keep_running) {

		FD_ZERO(&fdset);
                FD_SET(hb_fd, &fdset);

		numfds = hb_fd + 1;

		if (clm_initialized) {
			FD_SET(mem_fd, &fdset);

			if (mem_fd > hb_fd)
				numfds = mem_fd + 1;
		}

		tv.tv_sec = DEFAULT_TIME_OUT;
		tv.tv_usec = 0;
		tvp = &tv;

		ret = select(numfds, &fdset, 0, 0, tvp);

		if (ret < 0) {
			/* error */
			cl_log(LOG_ERR, "select() returned with an error. shutting down...");
			break;
		} else if (ret == 0) {
			/* timeout */
			ping_membership(&mem_fd);
			continue;
		} 

		if (FD_ISSET(hb_fd, &fdset)) {
			/* heartbeat */

			if ((ret = handle_heartbeat_msg()) == HA_FAIL) {
				cl_log(LOG_DEBUG, "no heartbeat. quit now.");
				hb_already_dead = 1;
				break;
			}
		} else  if (clm_initialized && FD_ISSET(mem_fd, &fdset)) {
		    	/* membership events */

		    	if ((ret = handle_membership_msg()) == HA_FAIL) {
			    	cl_log(LOG_DEBUG, "unrecoverable membership error. quit now.");
				break;
			}
		} else {

		}
	}


	if ( get_hb_initialized() ) {
		linuxha_finalize();
	}

        DEBUG_LEAVE();

	return 0;
}


