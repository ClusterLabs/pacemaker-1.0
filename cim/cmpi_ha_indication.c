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
#include <signal.h>

#include <pthread.h>
#include <glib.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include <hb_api.h>
#include <clplumbing/cl_malloc.h>

#include "linuxha_info.h"
#include "cmpi_utils.h"
#include "cmpi_ha_indication.h"

#define DEFAULT_TIME_OUT        5
#define LOGGER_ENTITY           "cim-ind"
#define HB_CLIENT_ID            "cmpi_indication"
#define IND_CLASSNAME           "LinuxHA_InstModification"
#define IND_NAMESPACE           "root/cimv2"

typedef struct {
        CMPIBroker  * broker;
        CMPIContext * context;
        CMPIString  * filter;
} cmpi_ind_env_t;


static int nodestatus_event_hook(const char * node, const char * status);
static int ifstatus_event_hook(const char * node, 
                        const char * lnk, const char * status);
static int membership_event_hook(const char * node, SaClmClusterChangesT status);

static int ind_update_instances (char * class_name);
static int ind_generate_indication (void * data);
static void ind_stop_thread(int a);
static int ind_main_loop (void);
static void * ind_thread_func(void * param);


static cmpi_ind_env_t * ind_env = NULL;
static int keep_running = 0;
static CMPIInstance * source_instance = NULL;
static CMPIInstance * previous_instance = NULL;


/* using pthread */
static pthread_t ind_thread_id = 0;

static int
ind_update_instances (char * class_name)
{
        CMPIObjectPath * op = NULL;
        CMPIInstance * new_instance = NULL;
        CMPIStatus rc;
        
        ASSERT(ind_env);

        /* FIXME: key:value not presented */
        op = CMNewObjectPath(ind_env->broker, IND_NAMESPACE, class_name, &rc);
        
        if ( CMIsNullObject(op) ) {
                cl_log(LOG_ERR, "%s: can't create op", __FUNCTION__);
                return HA_FAIL;
        }

        new_instance = CBGetInstance(ind_env->broker, 
                                     ind_env->context, op, NULL, &rc);

        if ( CMIsNullObject(new_instance) ) {
                cl_log(LOG_ERR, "%s: failed to get instance", __FUNCTION__);
                return HA_FAIL;
        }

        if ( previous_instance ) {
                CMRelease ( previous_instance);
                previous_instance = NULL;
        }

        if ( source_instance ) {
                previous_instance = CMClone(source_instance, &rc);
                CMRelease ( source_instance );
        }

        source_instance = CMClone(new_instance, &rc);
        
        return HA_OK;
}

static int
ind_generate_indication (void * data)
{
        CMPIInstance * instance = NULL;
        CMPIObjectPath * op = NULL;
        CMPIStatus rc;
        CMPIDateTime * date_time = NULL;
        CMPIArray * array = NULL;
        char * class_name = NULL;

        cl_log(LOG_INFO, "%s: nodestatus changed", __FUNCTION__);

        ASSERT(ind_env);

        /* update previous_instance and source_instance */
        ind_update_instances (class_name);

        /* create indication instance and set properties */
        op = CMNewObjectPath(ind_env->broker, 
                             IND_NAMESPACE, IND_CLASSNAME, &rc);

        instance = CMNewInstance(ind_env->broker, op, &rc);
        
        
        date_time = CMNewDateTime(ind_env->broker, &rc);
        array = CMNewArray(ind_env->broker, 0, CMPI_string, &rc);

        class_name = (char *)data;
        CMSetProperty(instance, "SourceInstance", 
                      source_instance, CMPI_instance); 

        CMSetProperty(instance, "PreviousInstance",
                      previous_instance, CMPI_instance);

        CMSetProperty(instance, "IndicationTime",
                      &date_time, CMPI_dateTime);

        CMSetProperty(instance, "IndicationIdentifier",
                      class_name, CMPI_chars);

        CMSetProperty(instance, "CorrelatedIndications",
                      &array, CMPI_stringA);
                
        /* deliver indication */
        CBDeliverIndication(ind_env->broker, ind_env->context, 
                            IND_NAMESPACE, instance);


        return HA_OK;
}

static int 
nodestatus_event_hook(const char * node, const char * status)
{
        /* FIXME: not completed */
        char class_name [] = "LinuxHA_ClusterNode";
        int ret = 0;

        ret = ind_generate_indication(class_name);
        return HA_OK;
}

static int 
ifstatus_event_hook(const char * node, const char * lnk, const char * status)
{
        /* FIXME: not completed */
        cl_log(LOG_INFO, "%s: ifstatus changed", __FUNCTION__);
        return HA_OK;
}

static int 
membership_event_hook(const char * node, SaClmClusterChangesT status)
{
        /* FIXME: not completed */
        char class_name [] = "LinuxHA_ClusterNode";
        int ret = 0;

        cl_log(LOG_INFO, "%s: membership changed", __FUNCTION__);

        ret = ind_generate_indication(class_name);
        return HA_OK;
}


static void
ind_stop_thread(int a) 
{
        keep_running = 0;
}

static void *
ind_thread_func(void * param)
{
        int ret = 0;

        DEBUG_ENTER();
        DEBUG_PID();

        if ( ! get_hb_initialized() ) {
                if ( linuxha_initialize(HB_CLIENT_ID, 0) != HA_OK ){
                        DEBUG_LEAVE();
                        return NULL;
                }
        }

        if ((ret = init_resource_table()) != HA_OK) {
                cl_log(LOG_ERR, "%s: failed to init resource table", 
                       __FUNCTION__);
                return NULL;
        }

        ret = init_membership();
        if (ret != HA_OK) {
                cl_log(LOG_ERR, 
                       "%s: failed to init  membership", __FUNCTION__);
                return NULL;
        }  

        keep_running = 1;
        signal(SIGTERM, ind_stop_thread);
        signal(SIGINT, ind_stop_thread);

        /* main loop */
        ret = ind_main_loop();

        if ( get_hb_initialized() ) {
                linuxha_finalize();
        }

        DEBUG_LEAVE();

        return NULL;
}


static int
ind_main_loop ()
{
        int heartbeat_fd = 0, membership_fd = 0;
        int ret = 0, numfds = 0;

        struct timeval tv;
        fd_set fdset;

        heartbeat_fd =  get_heartbeat_fd ();
        membership_fd = get_membership_fd ();

        while ( keep_running ) {

                FD_ZERO ( &fdset );
                FD_SET ( heartbeat_fd, &fdset );

                numfds = heartbeat_fd + 1;

                if ( clm_initialized ) {
                        FD_SET ( membership_fd, &fdset );
                        if ( membership_fd > heartbeat_fd ) {
                                numfds = membership_fd + 1;
                        }
                }

                tv.tv_sec = DEFAULT_TIME_OUT;
                tv.tv_usec = 0;

                ret = select(numfds, &fdset, 0, 0, &tv);

                if ( ret < 0 ) {                 /* error */
                        cl_log(LOG_ERR, 
                               "%s: select() error. exit", __FUNCTION__);
                        return HA_FAIL;
                } else if ( ret == 0 ) {         /* timeout */
                        ping_membership(&membership_fd);
                        continue;
                } 

                if ( FD_ISSET(heartbeat_fd, &fdset) ) { /* heartbeat event */
                        if ((ret = handle_heartbeat_msg()) == HA_FAIL) {
                                cl_log(LOG_DEBUG, 
                                       "%s: no heartbeat. exit", __FUNCTION__);
                                return HA_FAIL;
                        }
                } else  if ( clm_initialized && 
                             FD_ISSET(membership_fd, &fdset) ) {
                                                       /* membership event */
                        if ( (ret = handle_membership_msg()) == HA_FAIL) {
                                cl_log(LOG_DEBUG, "%s: membership error. exit", 
                                       __FUNCTION__);
                                return HA_FAIL;
                        }
                } else {                        /* unknown event */
                        cl_log(LOG_WARNING, 
                               "%s: unknown event occured", __FUNCTION__);
                        continue;
                }
        }

        return HA_OK;
}


/*****************************************************************
 * interface
 ****************************************************************/


int 
ha_indication_initialize(CMPIBroker * broker, 
                              CMPIContext * context, 
                              CMPISelectExp * filter, CMPIStatus * rc)
{
        pthread_attr_t tattr;

        init_logger(LOGGER_ENTITY);
        DEBUG_ENTER();

        ind_env = malloc(sizeof(cmpi_ind_env_t));

        ind_env->broker = broker;
        ind_env->context = context;
        ind_env->filter = CMGetSelExpString(filter, rc);

        ha_set_event_hooks(nodestatus_event_hook, 
                          ifstatus_event_hook, membership_event_hook);


        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

        pthread_create(&ind_thread_id, &tattr, ind_thread_func, ind_env);

        DEBUG_LEAVE();

        return HA_OK;
}


int 
ha_indication_finalize(CMPIBroker * broker, 
                            CMPIContext * context, 
                            CMPISelectExp * filter, CMPIStatus * rc)
{
        /* destroy the indication thread */
        keep_running = 0;
        
        free (ind_env);
        return HA_OK;
}


