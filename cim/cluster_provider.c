/*
 * cluster_provider.c: HA_Cluster provider
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

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <hb_api.h>
#include <clplumbing/cl_malloc.h>
#include "cmpi_utils.h"
#include "cluster_info.h"

static const char * PROVIDER_ID = "cim-cluster";
static CMPIBroker * G_broker    = NULL;
static char G_classname []      = "HA_Cluster";
static char G_instname  []      = "LinuxHACluster";
static char G_caption   []      = "LinuxHA Cluster";

struct key_name_pair {
	const char * key;       /* hb config key */
	const char * name;      /* CIM property name */
};

/* heartbeat config directives */
static const struct key_name_pair G_keyname [] = {
		{KEY_HBVERSION,	"HBVersion"},
		{KEY_HOST, 	"Node"},
		{KEY_HOPS, 	"HOPFudge"},
		{KEY_KEEPALIVE, "KeepAlive"},
		{KEY_DEADTIME, 	"DeadTime"},
		{KEY_DEADPING, 	"DeadPing"},
		{KEY_WARNTIME, 	"WarnTime"},
		{KEY_INITDEAD, 	"InitDead"},
		{KEY_WATCHDOG, 	"WatchdogTimer"},
		{KEY_BAUDRATE,	"BaudRate"},
		{KEY_UDPPORT,  	"UDPPort"},
		{KEY_FACILITY, 	"LogFacility"},
		{KEY_LOGFILE, 	"LogFile"},
		{KEY_DBGFILE,	"DebugFile"},
		{KEY_FAILBACK, 	"NiceFailBack"},
		{KEY_AUTOFAIL, 	"AutoFailBack"},
		{KEY_STONITH, 	"Stonith"},
		{KEY_STONITHHOST, 	"StonithHost"},
		{KEY_CLIENT_CHILD, 	"Respawn"},
		{KEY_RT_PRIO, 	"RTPriority"},
		{KEY_GEN_METH, 	"GenMethod"},
		{KEY_REALTIME, 	"RealTime"},
		{KEY_DEBUGLEVEL,"DebugLevel"},
		{KEY_NORMALPOLL,"NormalPoll"},
		{KEY_APIPERM, 	"APIAuth"},
		{KEY_MSGFMT, 	"MsgFmt"},
		{KEY_LOGDAEMON, "UseLogd"},
		{KEY_CONNINTVAL,"ConnLogdTime"},
		{KEY_BADPACK, 	"LogBadPack"},
		{KEY_REGAPPHBD, "NormalPoll"},
		{KEY_COREDUMP, 	"CoreDump"},
		{KEY_COREROOTDIR, 	"CoreRootDir"},
		{KEY_REL2, 	"WithCrm"},
		{0, 0}
};

static CMPIInstance *
make_cluster_instance(CMPIObjectPath * op, CMPIStatus * rc);
static int
get_inst_cluster(CMPIContext * ctx, CMPIResult * rslt,
                 CMPIObjectPath * cop, CMPIStatus * rc);
static int
enum_inst_cluster(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                  char ** properties, int enum_inst, CMPIStatus * rc);
static int cleanup_cluster(void); 

DeclareInstanceFunctions(Cluster);

/* always only one cluster instance */
static CMPIInstance *
make_cluster_instance(CMPIObjectPath * op, CMPIStatus * rc)
{
        CMPIInstance * ci = NULL;
        struct ci_table * info = NULL;
        const struct key_name_pair * pair = G_keyname;
        struct ci_table * crm_config;

        info = ci_get_cluster_config ();

        if ( info == NULL ) {
                cl_log(LOG_ERR, "%s: couldn't get cluster's information", __FUNCTION__);
	        CMSetStatusWithChars(G_broker, rc, CMPI_RC_ERR_FAILED, 
                                     "Could not get cluster information.");

                return NULL;
        }

        ci = CMNewInstance(G_broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: couldn't create instance", __FUNCTION__);
	        CMSetStatusWithChars(G_broker, rc, 
		       CMPI_RC_ERR_FAILED, "Could not create instance.");

                return NULL;
        }

        while ( pair->key ) {
                struct ci_data  data;
                data = info->get_data(info, pair->key);
                if ( data.value.string == NULL ){
                        cl_log(LOG_WARNING, "couldn't find value for key %s", pair->key);
                        pair ++;
                        continue;
                }
                CMSetProperty(ci, pair->name, data.value.string, CMPI_chars);
                pair ++;
        }

        CMSetProperty(ci, "CreationClassName", G_classname, CMPI_chars);
        CMSetProperty(ci, "Name", G_instname, CMPI_chars);
        CMSetProperty(ci, "Caption", G_caption, CMPI_chars);

        if (( crm_config = ci_get_crm_config()) != NULL ) {
                char * idle_timeout, * symmetric_cluster;
                char * stonith_enabled, * no_quorum_policy;
                char * res_stickiness, * have_quorum;
                
                idle_timeout =  crm_config->get_data(crm_config, 
                                      "transition_idle_timeout").value.string;
                symmetric_cluster = crm_config->get_data(crm_config,
                                      "symmetric_cluster").value.string;
                stonith_enabled = crm_config->get_data(crm_config, 
                                       "stonith_enabled").value.string;
                no_quorum_policy = crm_config->get_data(crm_config,
                                       "no_quorum_policy").value.string;
                res_stickiness = crm_config->get_data(crm_config,
                                       "default_resource_stickiness").value.string;
                have_quorum = crm_config->get_data(crm_config,
                                       "have_quorum").value.string;
                
                if ( idle_timeout )
                        CMSetProperty(ci, "TransitionIdleTimeout", 
                                      idle_timeout, CMPI_chars);
                if (symmetric_cluster)
                        CMSetProperty(ci, "SymmetricCluster", 
                                      symmetric_cluster, CMPI_chars);
                if ( stonith_enabled)
                        CMSetProperty(ci, "StonithEnabled", 
                                      stonith_enabled, CMPI_chars);
                if ( no_quorum_policy)
                        CMSetProperty(ci, "NoQuorumPolicy", 
                                      no_quorum_policy, CMPI_chars);
                if ( res_stickiness )
                        CMSetProperty(ci, "DefaultResourceStickiness",
                                      res_stickiness, CMPI_chars);
                if ( have_quorum)
                        CMSetProperty(ci, "HaveQuorum", have_quorum, CMPI_chars);

                crm_config->free(crm_config);
        }

        info->free(info);

        return ci;        
}

static int
get_inst_cluster(CMPIContext * ctx, CMPIResult * rslt, 
                 CMPIObjectPath * cop, CMPIStatus * rc)
{
        CMPIObjectPath* op = NULL;
        CMPIString * cmpi_namespace = NULL;
        char * namespace = NULL;
        CMPIInstance * ci = NULL;
        int ret = 0;

        cmpi_namespace = CMGetNameSpace(cop, rc);
        namespace = CMGetCharPtr(cmpi_namespace);
        
        op = CMNewObjectPath(G_broker, namespace, G_classname, rc);
        if ( CMIsNullObject(op) ){
	        CMSetStatusWithChars(G_broker, rc, 
		       CMPI_RC_ERR_FAILED, "Create object path failed");
                ret = HA_FAIL;
                goto out;
        }

        ci = make_cluster_instance(op, rc);
        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                goto out;
        }
        
        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        ret = HA_OK;

out:
        CMRelease(op);
        return ret;
}

static int
enum_inst_cluster(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                  char ** properties, int enum_inst, CMPIStatus * rc)
{

        CMPIInstance* ci = NULL;
        CMPIObjectPath* op = NULL;
        CMPIString * cmpi_namespace = NULL;
        char * namespace = NULL;
        int ret = 0;

        cmpi_namespace = CMGetNameSpace(cop, rc);
        namespace = CMGetCharPtr(cmpi_namespace);
        
        op = CMNewObjectPath(G_broker, namespace, G_classname, rc);
        if ( CMIsNullObject(op) ){
	        CMSetStatusWithChars(G_broker, rc, 
		       CMPI_RC_ERR_FAILED, "Create object path failed");

                ret = HA_FAIL;
                goto out;
        }

        if ( ! enum_inst ) {
                /* enumerate instance names */
	        CMAddKey(op, "CreationClassName", G_classname, CMPI_chars);
                CMAddKey(op, "Name",  G_instname, CMPI_chars);
	        CMReturnObjectPath(rslt, op);

        } else {
                /* enumerate instances */
                ci = make_cluster_instance(op, rc);
                if ( CMIsNullObject(ci) ) {
                        ret = HA_FAIL;
                        goto out;
                }
                CMReturnInstance(rslt, ci);
        }

        CMReturnDone(rslt);
        ret = HA_OK;
out:
        CMRelease(op);
	return ret;
}

static int
cleanup_cluster() {
        cl_log(LOG_INFO, "%s: clean up", __FUNCTION__);
        return HA_OK;
}



/**********************************************
 * Instance Provider Interface
 **********************************************/

static CMPIStatus 
ClusterCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{

        cleanup_cluster();
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ClusterEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                         CMPIResult * rslt, CMPIObjectPath *ref)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if ( enum_inst_cluster(ctx, rslt, ref, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

static CMPIStatus 
ClusterEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                     CMPIObjectPath* ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if ( enum_inst_cluster(ctx, rslt, ref, properties, 1, &rc) == HA_OK ) {
                CMReturn(CMPI_RC_OK);                
        } else {
                return rc;
        }

}


static CMPIStatus 
ClusterGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);

        if ( get_inst_cluster(ctx, rslt, cop, &rc) == HA_OK ) {
                CMReturn(CMPI_RC_OK);
        } else {
                return rc;
        }

}


static CMPIStatus 
ClusterCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                      CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
ClusterSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * op, CMPIInstance * inst, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
ClusterDeleteInstance(CMPIInstanceMI * mi,  CMPIContext * ctx, 
                      CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
ClusterExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
                 CMPIObjectPath * ref, char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


/**************************************************
 * Method Provider Interface
 *************************************************/
static CMPIStatus 
ClusterInvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                    CMPIObjectPath * ref, const char * method_name,
                    CMPIArgs * in, CMPIArgs * out)
{

        CMPIString* class_name = NULL;
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMPIValue valrc;
        
        class_name = CMGetClassName(ref, &rc);

        if(strcasecmp(CMGetCharPtr(class_name), G_classname) == 0 &&
           strcasecmp("RequestStatusChange", method_name) == 0 ){
                cl_log(LOG_INFO, "%s: NOT IMPLEMENTED", __FUNCTION__);
        }

        CMReturnData(rslt, &valrc, CMPI_uint32);
        CMReturnDone(rslt);

        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
ClusterMethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}



/*****************************************************
 * install interface
 ****************************************************/
DeclareInstanceMI(Cluster, HA_ClusterProvider, G_broker);
DeclareMethodMI(Cluster, HA_ClusterProvider, G_broker);
