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

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include "cmpi_utils.h"
#include "cmpi_cluster.h"
#include "linuxha_info.h"
#include "cmpi_ha_indication.h"

#define PROVIDER_ID  "cim-provider-cluster"

static CMPIBroker * Broker = NULL;
static char  ClassName[] = "LinuxHA_Cluster";


/*---------------- instance interfaces ---------*/
CMPIStatus LinuxHA_ClusterProviderCleanup(CMPIInstanceMI * mi, CMPIContext * ctx);

CMPIStatus LinuxHA_ClusterProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);

CMPIStatus LinuxHA_ClusterProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath* ref, char ** properties);
                
CMPIStatus LinuxHA_ClusterProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties);
               
CMPIStatus LinuxHA_ClusterProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci);
                
CMPIStatus LinuxHA_ClusterProviderSetInstance(CMPIInstanceMI * mi, 
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, CMPIInstance * inst, char ** properties);


CMPIStatus LinuxHA_ClusterProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);

CMPIStatus LinuxHA_ClusterProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref,
                char * lang, char * query);
               
/*-------- method interfaces -------------*/
CMPIStatus LinuxHA_ClusterProviderInvokeMethod(CMPIMethodMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, const char * methodName,
                CMPIArgs * in, CMPIArgs * out);

CMPIStatus LinuxHA_ClusterProviderMethodCleanup(CMPIMethodMI* mi, CMPIContext* ctx);

/*----------- indication interfaces ----------*/
CMPIStatus LinuxHA_ClusterProviderIndicationCleanup(CMPIIndicationMI * mi, 
                CMPIContext * ctx);

CMPIStatus LinuxHA_ClusterProviderAuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, const char * owner);

CMPIStatus LinuxHA_ClusterProviderMustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
                const char * indType, CMPIObjectPath* classPath);

CMPIStatus LinuxHA_ClusterProviderActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation);

CMPIStatus LinuxHA_ClusterProviderDeActivateFilter(CMPIIndicationMI * mi,
               CMPIContext * ctx, CMPIResult * rslt,
               CMPISelectExp * filter, const char * type,
               CMPIObjectPath * classPath, CMPIBoolean lastActivation);


/************ etnries ********************************/

CMPIInstanceMI * 
LinuxHA_ClusterProvider_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx);

CMPIMethodMI *
LinuxHA_ClusterProvider_Create_MethodMI(CMPIBroker * brkr, CMPIContext * ctx);

CMPIIndicationMI *
LinuxHA_ClusterProvider_Create_IndicationMI(CMPIBroker * brkr, CMPIContext * ctx);


/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
LinuxHA_ClusterProviderCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{

        cleanup_cluster();
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterProviderEnumInstanceNames(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath *ref)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        ret = enumerate_cluster_instances(ClassName, Broker, ctx, rslt,
                        ref, NULL, 0, &rc);  

        if ( ret != HA_OK ) {
                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }

        CMReturn(CMPI_RC_OK);

}

CMPIStatus 
LinuxHA_ClusterProviderEnumInstances(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath* ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        ret = enumerate_cluster_instances(ClassName, Broker, ctx, rslt,
                        ref, properties, 1, &rc);  

        if ( ret != HA_OK ) {
                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }        
        }

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterProviderGetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);

        ret = get_cluster_instance(ClassName, Broker, ctx, rslt, cop, &rc);

        if ( ret != HA_OK ) {
                cl_log(LOG_WARNING, 
                        "%s: failed to get instance", __FUNCTION__);

                if (rc.rc == CMPI_RC_OK ) {
                        CMReturn(CMPI_RC_ERR_FAILED);
                } else {
                        return rc;
                }
        }
        CMReturn(CMPI_RC_OK);

}


CMPIStatus 
LinuxHA_ClusterProviderCreateInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


CMPIStatus 
LinuxHA_ClusterProviderSetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, CMPIInstance * inst, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


CMPIStatus 
LinuxHA_ClusterProviderDeleteInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

CMPIStatus 
LinuxHA_ClusterProviderExecQuery(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref,
                char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


/**************************************************
 * Method Provider 
 *************************************************/
CMPIStatus 
LinuxHA_ClusterProviderInvokeMethod(CMPIMethodMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * ref, const char * methodName,
                CMPIArgs * in, CMPIArgs * out)
{

        CMPIString* class_name = NULL;
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMPIData arg_data;
        CMPIValue valrc;
        char param [] = "action";

        init_logger(PROVIDER_ID);
        class_name = CMGetClassName(ref, &rc);

        arg_data = CMGetArg(in, param, &rc);

        if(strcasecmp((char *)class_name->hdl, ClassName) == 0 &&
                 strcasecmp("RequestStatusChange", methodName) == 0 ){

                char cmnd_pat[] = "/etc/init.d/heartbeat";
                char * cmnd = malloc(256);
                char * str_arg;

                str_arg = (char *)arg_data.value.string->hdl;
                sprintf(cmnd, "%s %s", cmnd_pat, str_arg);

                cl_log(LOG_INFO, "%s: run cmnd %s", __FUNCTION__, cmnd);
                valrc.uint32 = system(cmnd);

                free(cmnd);
        }

        CMReturnData(rslt, &valrc, CMPI_uint32);
        CMReturnDone(rslt);

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
LinuxHA_ClusterProviderMethodCleanup(CMPIMethodMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}


/**************************************************
 * Indication Interface Implementaion
 *************************************************/
CMPIStatus 
LinuxHA_ClusterProviderIndicationCleanup(CMPIIndicationMI * mi, 
                CMPIContext * ctx)
{
        init_logger(PROVIDER_ID);
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterProviderAuthorizeFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, const char * owner)
{

        CMPIValue valrc;

        init_logger(PROVIDER_ID);
        /*** debug ***/
        DEBUG_ENTER();

        valrc.boolean = 1;

        CMReturnData(rslt, &valrc, CMPI_boolean);
        CMReturnDone(rslt);

        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterProviderMustPoll(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,
                const char * indType, CMPIObjectPath* classPath)
{
        
        CMPIValue valrc;
        valrc.boolean = 1;

        init_logger(PROVIDER_ID);
        /*** debug ***/
        DEBUG_ENTER();
        

        CMReturnData(rslt, &valrc, CMPI_boolean);
        CMReturnDone(rslt);
        
        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterProviderActivateFilter(CMPIIndicationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPISelectExp * filter, const char * type,
                CMPIObjectPath * classPath, CMPIBoolean firstActivation)
{
        CMPIStatus rc;
        int ret = 0;

        init_logger(PROVIDER_ID);
        
        DEBUG_ENTER();
        
        ret = cluster_indication_initialize(ClassName, 
                                Broker, ctx, filter, &rc);

        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
LinuxHA_ClusterProviderDeActivateFilter(CMPIIndicationMI * mi,
               CMPIContext * ctx, CMPIResult * rslt,
               CMPISelectExp * filter, const char * type,
               CMPIObjectPath * classPath, CMPIBoolean lastActivation)
{
        int ret = 0;
        CMPIStatus rc;

        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = cluster_indication_finalize(ClassName, 
                                Broker, ctx, filter, &rc);        
        
        DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}


/*****************************************************
 * install interface
 ****************************************************/
static char inst_provider_name[] = "instanceLinuxHA_ClusterProvider";

static CMPIInstanceMIFT instMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        inst_provider_name,
        LinuxHA_ClusterProviderCleanup,
        LinuxHA_ClusterProviderEnumInstanceNames,
        LinuxHA_ClusterProviderEnumInstances,
        LinuxHA_ClusterProviderGetInstance,
        LinuxHA_ClusterProviderCreateInstance,
        LinuxHA_ClusterProviderSetInstance, 
        LinuxHA_ClusterProviderDeleteInstance,
        LinuxHA_ClusterProviderExecQuery
};

CMPIInstanceMI * 
LinuxHA_ClusterProvider_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx)
{
        static CMPIInstanceMI mi = {
                NULL,
                &instMIFT
        };
        Broker = brkr;
        CMNoHook;
        return &mi;
}

/*------------------------------------------------*/

static char method_provider_name[] = "methodLinuxHA_ClusterProvider";
static CMPIMethodMIFT methMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        method_provider_name,
        LinuxHA_ClusterProviderMethodCleanup,
        LinuxHA_ClusterProviderInvokeMethod
};

CMPIMethodMI *
LinuxHA_ClusterProvider_Create_MethodMI(CMPIBroker * brkr, CMPIContext * ctx)
{
        static CMPIMethodMI mi = {
                NULL,
                &methMIFT
        };

        Broker = brkr;
        CMNoHook;
        return &mi;
}

/*--------------------------------------------*/

static char ind_provider_name[] = "indLinuxHA_ClusterProvider";
static CMPIIndicationMIFT indMIFT = {
        CMPICurrentVersion,
        CMPICurrentVersion,
        ind_provider_name,
        LinuxHA_ClusterProviderIndicationCleanup,
        LinuxHA_ClusterProviderAuthorizeFilter,
        LinuxHA_ClusterProviderMustPoll,
        LinuxHA_ClusterProviderActivateFilter,
        LinuxHA_ClusterProviderDeActivateFilter
};

CMPIIndicationMI *
LinuxHA_ClusterProvider_Create_IndicationMI(CMPIBroker * brkr, CMPIContext * ctx)
{
        static CMPIIndicationMI mi = {
                NULL,
                &indMIFT
        };

        Broker = brkr;
        CMNoHook;
        return &mi;
}


