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
#define IND_NAMESPACE           "root/cimv2"

struct cmpi_ind_env{
        char * classname;
        CMPIBroker  * broker;
        CMPIContext * context;
        CMPIString  * filter;
};


enum ind_type { IND_TYPE_CLUSTER = 0, /* cluster-type indicatoin */
                IND_TYPE_NODE,        /* node-type indication */
                IND_TYPE_MEMBERSHIP,  /* membership-type indication */       
                IND_TYPE_RESOURCE     /* resource-type indication */
};

struct cmpi_ind_data {
        char * message;   /* indication message */
        enum ind_type type;    /* type */
};

static int nodestatus_event_hook(const char * node, const char * status);
static int ifstatus_event_hook(const char * node, const char * lnk, 
                               const char * status);
static int membership_event_hook(const char * node, 
                                 SaClmClusterChangesT status);

static int ind_generate_indication (void * data);
static void ind_stop_thread(int ano);
static int ind_main_loop (void);
static void * ind_thread_func(void * param);


static struct cmpi_ind_env * ind_env = NULL;
static int keep_running = 0;
static pthread_t ind_thread_id = 0;

static int
ind_generate_indication (void * data)
{
        CMPIInstance * instance = NULL;
        CMPIObjectPath * op = NULL;
        CMPIStatus rc;
        CMPIDateTime * date_time = NULL;
        CMPIArray * array = NULL;
        struct cmpi_ind_data * ind_data = NULL;


        ind_data = (struct cmpi_ind_data *) data;

        cl_log(LOG_INFO, "%s: status changed, genereate indication.", __FUNCTION__);

        ASSERT(ind_env);

        /* create indication instance and set properties */
        op = CMNewObjectPath(ind_env->broker, 
                             IND_NAMESPACE, ind_env->classname, &rc);

        instance = CMNewInstance(ind_env->broker, op, &rc);
        
        
        date_time = CMNewDateTime(ind_env->broker, &rc);
        array = CMNewArray(ind_env->broker, 0, CMPI_string, &rc);


        CMSetProperty(instance, "Message", ind_data->message, CMPI_chars);
        CMSetProperty(instance, "Time", &date_time, CMPI_dateTime);
        CMSetProperty(instance, "Type", &ind_data->type, CMPI_uint16);
                
        /* deliver indication */

        cl_log(LOG_INFO, "%s: deliver indication", __FUNCTION__);

        CBDeliverIndication(ind_env->broker, ind_env->context, 
                            IND_NAMESPACE, instance);


        return HA_OK;
}

static int 
nodestatus_event_hook(const char * node, const char * status)
{
        struct cmpi_ind_data * data = NULL;
        int ret = 0;

        cl_log(LOG_INFO, "%s: node status changed", __FUNCTION__);

        data = (struct cmpi_ind_data *) 
                malloc(sizeof(struct cmpi_ind_data));

        data->type = IND_TYPE_NODE;
        data->message = strdup("node status changed");

        ret = ind_generate_indication(data);

        free (data);

        return ret;
}

static int 
ifstatus_event_hook(const char * node, const char * lnk, const char * status)
{
        struct cmpi_ind_data * data = NULL;
        int ret = 0;

        cl_log(LOG_INFO, "%s: ifstatus changed", __FUNCTION__);

        data = (struct cmpi_ind_data *) 
                malloc(sizeof(struct cmpi_ind_data));

        data->type = IND_TYPE_NODE;
        data->message = strdup("if status changed");

        ret = ind_generate_indication(data);

        free (data);

        return ret;
}

static int 
membership_event_hook(const char * node, SaClmClusterChangesT status)
{
        struct cmpi_ind_data * data = NULL;
        int ret = 0;

        cl_log(LOG_INFO, "%s: membership status changed", __FUNCTION__);

        data = (struct cmpi_ind_data *) 
                malloc(sizeof(struct cmpi_ind_data));

        data->type = IND_TYPE_MEMBERSHIP;
        data->message = strdup("membership status changed");

        ret = ind_generate_indication(data);

        free (data);

        return ret;
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
        
        /* attach thread */
        CBAttachThread(ind_env->broker, ind_env->context);

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

        /* detach thread */
        CBDetachThread(ind_env->broker, ind_env->context);

        free(ind_env);

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
                        cl_log(LOG_INFO, "%s: i am alive", __FUNCTION__);
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
haind_activate(char * classname, CMPIBroker * broker, CMPIContext * ctx, 
               CMPIResult * rslt, CMPISelectExp * filter, const char * type, 
               CMPIObjectPath * classpath, CMPIBoolean firstactivation, 
               CMPIStatus * rc)
{
        pthread_attr_t tattr;

        init_logger(LOGGER_ENTITY);
        DEBUG_ENTER();

        ind_env = (struct cmpi_ind_env *)malloc(sizeof(struct cmpi_ind_env));

        ind_env->broker = broker;
        ind_env->context = ctx;
        ind_env->filter = CMGetSelExpString(filter, rc);
        ind_env->classname = strdup(classname);

        /* set event hooks, noly one hook for each event currently */
        ha_set_event_hooks(nodestatus_event_hook, 
                          ifstatus_event_hook, membership_event_hook);


        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

        /* should use broker->xft->newThread(...) ? */
        CBPrepareAttachThread(broker, ctx);
        pthread_create(&ind_thread_id, &tattr, ind_thread_func, NULL);

        DEBUG_LEAVE();

        return HA_OK;
}


int 
haind_deactivate(char * classname, CMPIBroker * broker, CMPIContext * ctx,
                 CMPIResult * rslt, CMPISelectExp * filter, const char * type,
                 CMPIObjectPath * classpath, CMPIBoolean lastactivation,
                 CMPIStatus * rc)
{
        /* destroy the indication thread */
        keep_running = 0;
        
        free (ind_env);
        return HA_OK;
}


