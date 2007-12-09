/*
 * cluster_indication_provider.c: HA_Indication provider
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

#include <hb_config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <signal.h>
#include <glib.h>
#include "cluster_info.h"
#include "cmpi_utils.h"

struct cmpi_ind_env {
        char * classname;
        CMPIBroker  * broker;
        CMPIContext * context;
        CMPIString  * filter;
};


enum ind_type { 
        IND_TYPE_CLUSTER = 0, /* cluster-type indicatoin */
        IND_TYPE_NODE,        /* node-type indication */
        IND_TYPE_MEMBERSHIP,  /* membership-type indication */       
        IND_TYPE_RESOURCE,     /* resource-type indication */
        IND_TYPE_CRM,
        IND_TYPE_UNKNOWN
};

struct cmpi_ind_data {
        const char * message;   /* indication message */
        enum ind_type type;    /* type */
};

#define PROVIDER_ID             "cim-ind"
/* 
#define HB_CLIENT_ID            "cmpi_indication" 
*/
#define HB_CLIENT_ID            NULL
#define IND_NAMESPACE           "root/cimv2"
#define DEFAULT_TIME_OUT        5

static char G_classname []           = "HA_Indication";
static CMPIBroker * G_broker         = NULL;
static int ind_enabled               = 0;
static struct cmpi_ind_env * ind_env = NULL;
static pthread_t ind_thread_id       = 0;
static GMainLoop * G_mainloop        = NULL;


static int ind_event_handler(const char * event);

static int ind_generate_indication (void * data);
static void ind_stop_thread(int ano);
static int ind_main_loop (void);
static void * ind_thread_func(void * param);

static int haind_activate(CMPIContext * ctx, CMPIResult * rslt, 
                          CMPISelectExp * filter, const char * type, 
                          CMPIObjectPath * classpath, 
                          CMPIBoolean firstactivation, CMPIStatus * rc);
static int haind_deactivate(CMPIContext * ctx, CMPIResult * rslt, 
                          CMPISelectExp * filter, const char * type,
                          CMPIObjectPath * classpath, 
                          CMPIBoolean lastactivation, CMPIStatus * rc);

DeclareIndicationFunctions(Indication);

static int
ind_generate_indication (void * data)
{
        CMPIInstance * instance = NULL;
        CMPIObjectPath * op = NULL;
        CMPIStatus rc;
        CMPIDateTime * date_time = NULL;
        struct cmpi_ind_data * ind_data = NULL;
        char msg[128];

        ind_data = (struct cmpi_ind_data *) data;

        cl_log(LOG_INFO, "%s: status changed, genereate indication.", 
               __FUNCTION__);

        ASSERT(ind_env);

        /* create indication instance and set properties */
        op = CMNewObjectPath(ind_env->broker, 
                             IND_NAMESPACE, ind_env->classname, &rc);

        instance = CMNewInstance(ind_env->broker, op, &rc);
        date_time = CMNewDateTime(ind_env->broker, &rc);

        snprintf(msg, 128, "%s", ind_data->message);
        CMSetProperty(instance, "Message", msg, CMPI_chars);
        CMSetProperty(instance, "Time", &date_time, CMPI_dateTime);
        CMSetProperty(instance, "Type", &ind_data->type, CMPI_uint16);
                
        /* deliver indication */
        cl_log(LOG_INFO, "%s: deliver indication", __FUNCTION__);
        CBDeliverIndication(ind_env->broker, ind_env->context, 
                            IND_NAMESPACE, instance);


        return HA_OK;
}

static int
ind_event_handler(const char * event)
{
        struct cmpi_ind_data data ;
        char msg[256];

        if ( event == NULL ) { return HA_FAIL; }
        cl_log(LOG_INFO, "ind_event_handler: got event: %s", event);
        snprintf(msg, 256, "Got an indication: %s", event);

        data.type = IND_TYPE_CRM;
        data.message = msg;
        return ind_generate_indication(&data);
}

static int
ind_disconnected_handler(const char * event)
{
        struct cmpi_ind_data data;
        cl_log(LOG_INFO, "heartbeat disconnected");
        
        data.type = IND_TYPE_CLUSTER;
        data.message = "Heartbeat disconnected";
        ind_generate_indication(&data);
        ind_stop_thread(11);
        return HA_OK;

}

static void
ind_stop_thread(int a) 
{
        if ( G_mainloop && g_main_is_running(G_mainloop)) {
                g_main_quit(G_mainloop);
        }
}

static void *
ind_thread_func(void * param)
{
        /* attach thread */
        CBAttachThread(ind_env->broker, ind_env->context);

        signal(SIGTERM, ind_stop_thread);
        signal(SIGINT, ind_stop_thread);

        /* main loop */
        ind_main_loop();


        /* detach thread */
        CBDetachThread(ind_env->broker, ind_env->context);

        cim_free(ind_env);

        return NULL;
}


static int
ind_main_loop ()
{
        G_mainloop = g_main_new(FALSE);
        if ( G_mainloop == NULL ) {
                cl_log(LOG_ERR, "ind_main_loop: couldn't creat mainloop");
                return HA_FAIL;
        }

        g_main_run(G_mainloop);
        return HA_OK;
}


/*****************************************************************
 * interface
 ****************************************************************/

static int
haind_activate(CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter, 
               const char * type, CMPIObjectPath * classpath, 
               CMPIBoolean firstactivation, CMPIStatus * rc)
{
        pthread_attr_t tattr;

        PROVIDER_INIT_LOGGER();

        if ( (ind_env = (struct cmpi_ind_env *)
              cim_malloc(sizeof(struct cmpi_ind_env)) ) == NULL ) {
                cl_log(LOG_ERR, "%s: could not alloc ind_env", __FUNCTION__);
                return HA_FAIL;
        }

        ind_env->broker = G_broker;
        ind_env->context = ctx;
        ind_env->filter = CMGetSelExpString(filter, rc);
        ind_env->classname = G_classname;

        /* set event hooks, noly one hook for each event currently */
        reg_event(EVT_CIB_CHANGED, ind_event_handler);
        reg_event(EVT_DISCONNECTED, ind_disconnected_handler);

        pthread_attr_init(&tattr);
        pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

        /* should use broker->xft->newThread(...) ? */
        CBPrepareAttachThread(G_broker, ctx);
        pthread_create(&ind_thread_id, &tattr, ind_thread_func, NULL);

        return HA_OK;
}


static int 
haind_deactivate(CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter, 
                 const char * type, CMPIObjectPath * classpath, 
                 CMPIBoolean lastactivation, CMPIStatus * rc)
{
        /* destroy the indication thread */
        ind_stop_thread(1) ;
        cim_free (ind_env);
        return HA_OK;
}



/**************************************************
 * Indication Interface
 *************************************************/
static CMPIStatus 
IndicationIndicationCleanup(CMPIIndicationMI * mi, 
                            CMPIContext * ctx)
{
        PROVIDER_INIT_LOGGER();
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
IndicationAuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, const char * owner)
{
        CMPIBoolean authorized = 1;
        char * filter_str = NULL;
        CMPIStatus rc;

        PROVIDER_INIT_LOGGER();

        filter_str = CMGetCharPtr( CMGetSelExpString(filter, &rc) );

        cl_log(LOG_INFO, "%s: eventype = %s, filter = %s", 
               __FUNCTION__,type, filter_str);

        CMReturnData(rslt, (CMPIValue *)&authorized, CMPI_boolean);
        CMReturnDone(rslt);

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
IndicationMustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
                const char * indType, CMPIObjectPath * classPath)
{
        CMPIStatus rc = {CMPI_RC_ERR_NOT_SUPPORTED, NULL};
        CMPIBoolean poll = 0;
        char * filter_str = NULL;

        PROVIDER_INIT_LOGGER();

        filter_str = CMGetCharPtr( CMGetSelExpString(filter, &rc) );

        cl_log(LOG_INFO, "%s: eventype = %s, filter = %s", 
               __FUNCTION__, indType, filter_str);
        
        cl_log(LOG_INFO, "%s: does not suppot poll", __FUNCTION__);

        CMReturnData(rslt, (CMPIValue *)&poll, CMPI_boolean);
        CMReturnDone(rslt);

        return rc;
}

static CMPIStatus 
IndicationActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation)
{

        CMPIBoolean activated = 1;
        CMPIStatus rc;

        PROVIDER_INIT_LOGGER();
        
        if (  haind_activate(ctx, rslt, filter, type, 
                             classPath, firstActivation, &rc) == HA_OK ) {

                CMReturnData(rslt, (CMPIValue *)&activated, CMPI_boolean);
                CMReturnDone(rslt);
 
                CMReturn(CMPI_RC_OK);
        } else {
                CMReturnDone(rslt);
                CMReturn(CMPI_RC_ERR_FAILED);
        }
}

static CMPIStatus 
IndicationDeActivateFilter(CMPIIndicationMI * mi,
               CMPIContext * ctx, CMPIResult * rslt,
               CMPISelectExp * filter, const char * type,
               CMPIObjectPath * classPath, CMPIBoolean lastActivation)
{
        CMPIBoolean deactivated = 1;
        CMPIStatus rc;

        PROVIDER_INIT_LOGGER();

        if ( haind_deactivate(ctx, rslt, filter, type, 
                              classPath, lastActivation, &rc) == HA_OK ) {
        
                CMReturnData(rslt, (CMPIValue *)&deactivated, CMPI_boolean);
                CMReturnDone(rslt);
                CMReturn(CMPI_RC_OK);
        } else {
                CMReturnDone(rslt);
                CMReturn(CMPI_RC_ERR_FAILED);
        }
}



static void 
IndicationEnableIndications(CMPIIndicationMI * mi )
{
        /* Enable indication generation */
        ind_enabled = 1;
}


static void 
IndicationDisableIndications(CMPIIndicationMI * mi )
{
        /* Disable indication generation */
        ind_enabled = 0;
}

/*****************************************************
 * Indication
 ****************************************************/
DeclareIndicationMI(Indication, HA_IndicationProvider, G_broker);

