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

#include <hb_config.h>
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
#include "utils.h"

static const char * 	PROVIDER_ID = "cim-hb";
static CMPIBroker * 	Broker    = NULL;
static char     	ClassName []  = "HA_Cluster";
static char     	InstName  []  = "LinuxHACluster";
static char     	Caption   []  = "LinuxHA Cluster";

/* auth map */
uint32_t        auth_valuemap [] = {0, 1, 2};
const char *    auth_values   [] = {"sha1", "md5", "crc"};
static uint32_t last_requested_state = 5; /* Not Changed */

static CMPIInstance * make_cluster_instance(CMPIObjectPath * op, 
			CMPIStatus * rc);

static int      cluster_get_inst(CMPIContext * ctx, CMPIResult * rslt,
			CMPIObjectPath * cop, CMPIStatus * rc);

static int      cluster_enum_insts(CMPIContext * ctx, CMPIResult * rslt, 
                        CMPIObjectPath * cop, char ** properties,
                        int enum_inst, CMPIStatus * rc);

static int	auth_value2map(const char * method);
static const char *   auth_map2value(uint32_t map);

static void     config_set_instance(CMPIInstance * ci, CMPIStatus * rc);
static void     auth_set_instance(CMPIInstance * ci, CMPIStatus * rc);
static void     crm_set_instance(CMPIInstance * ci, CMPIStatus * rc);

/* set instance */

static int      cluster_update_inst(CMPIContext* ctx, CMPIResult* rslt, 
                        CMPIObjectPath*, CMPIInstance*, CMPIStatus*);
static int      request_state_change(int state);
static int      cleanup_cluster(void); 

DeclareInstanceFunctions(Cluster);

static int
auth_value2map(const char * method) 
{
        int i;
        for (i = 0; i < sizeof(auth_valuemap)/sizeof(uint32_t); i++) {
                if ( strcmp(auth_values[i], method) == 0)  {
                        return auth_valuemap[i];
                }
        }
        return -1;
}

static const char *
auth_map2value(uint32_t map)
{
        int i;
        for (i = 0; i < sizeof(auth_valuemap)/sizeof(uint32_t); i++) {
                if ( map == auth_valuemap[i] ) {
                        return auth_values[i];
                }
        }
        return NULL;
}

static void
config_set_instance(CMPIInstance * ci, CMPIStatus * rc)
{
	struct ha_msg * info;

        DEBUG_ENTER();
	/* get cluster config values */
        info = cim_get_hacf_config ();
        if ( info == NULL ) {
                cl_log(LOG_ERR, "%s:can not get cluster.", __FUNCTION__);
        	return;
	}
	cmpi_msg2inst(Broker, ci, HA_CLUSTER, info, rc); 
	DEBUG_LEAVE();
	ha_msg_del(info);
}

static void
auth_set_instance(CMPIInstance * ci, CMPIStatus * rc)
{
        int	   num;
        char*      method;
	char*      key;
	struct ha_msg *msg;

	DEBUG_ENTER();
        if ( (msg = cim_get_authkeys()) == NULL  ){
		cl_log(LOG_WARNING, "Can not get auth message");
                 return;
        }

        method = cim_strdup(cl_get_string(msg, "authmethod"));
        key = cim_strdup(cl_get_string(msg, "authkey"));

        num = auth_value2map(method);
        CMSetProperty(ci, "AuthMethod", &num, CMPI_uint32);
        CMSetProperty(ci, "AuthKey", key, CMPI_chars);
	
	DEBUG_LEAVE();
	ha_msg_del(msg);
}

static void
crm_set_instance(CMPIInstance * ci, CMPIStatus * rc)
{
        char * it, * sc, * se;
        char * nq, * rs, * hq;
	struct ha_msg *msg;

        if (( msg = cim_query_dispatch(GET_CRM_CONFIG, NULL, NULL)) == NULL ){
		return;
	}
                
        it = cim_strdup(cl_get_string(msg, "transition_idle_timeout"));
        sc = cim_strdup(cl_get_string(msg, "symmetric_cluster"));
        se = cim_strdup(cl_get_string(msg, "stonith_enabled"));
        nq = cim_strdup(cl_get_string(msg, "no_quorum_policy"));
        rs = cim_strdup(cl_get_string(msg, "default_resource_stickiness"));
        hq = cim_strdup(cl_get_string(msg, "have_quorum"));
                
        if (it && sc && se && nq && rs && hq) {
        	CMSetProperty(ci, "TransitionIdleTimeout",     it, CMPI_chars);
                CMSetProperty(ci, "SymmetricCluster",          sc, CMPI_chars);
	        CMSetProperty(ci, "StonithEnabled",            se, CMPI_chars);
                CMSetProperty(ci, "NoQuorumPolicy",            nq, CMPI_chars);
                CMSetProperty(ci, "DefaultResourceStickiness", rs, CMPI_chars);
                CMSetProperty(ci, "HaveQuorum",                hq, CMPI_chars);
	}

	ha_msg_del(msg);
}

/* always only one cluster instance */
static CMPIInstance *
make_cluster_instance(CMPIObjectPath * op, CMPIStatus * rc)
{
        CMPIInstance *ci = NULL;
	uint32_t state = 0;
	struct ha_msg *msg;

	DEBUG_ENTER();
        ci = CMNewInstance(Broker, op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: couldn't create instance", __FUNCTION__);
	        CMSetStatusWithChars(Broker, rc, 
		       CMPI_RC_ERR_FAILED, "Could not create instance.");
                return NULL;
        }

        CMSetProperty(ci, "CreationClassName", ClassName, CMPI_chars);
        CMSetProperty(ci, "Name", InstName, CMPI_chars);
        CMSetProperty(ci, "Caption", Caption, CMPI_chars);

	/* set config properties */
	config_set_instance(ci, rc);

        /* set auth properties */
        auth_set_instance(ci, rc);

	state = 3;	/* Disabled */
        if( cim_get_hb_status() == HB_RUNNING) {
		state = 2; /* Enabled */
	       	/* set crm properties */
		crm_set_instance(ci, rc); 
	} 

	CMSetProperty(ci, "EnabledState", &state, CMPI_uint32);
	CMSetProperty(ci, "RequestedState", &last_requested_state, CMPI_uint32);

	if ( (msg = cim_get_software_identity() )){
		char * hv;
		hv = cim_strdup(cl_get_string(msg, "hbversion"));
		if (hv) {
			CMSetProperty(ci, "HBVersion", hv, CMPI_chars);
		}
		cim_free(hv);
	}
	DEBUG_LEAVE();
        return ci;        
}

/* get a cluster instance */
static int
cluster_get_inst(CMPIContext * ctx, CMPIResult * rslt, 
                 CMPIObjectPath * cop, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIString *     cmpi_namespace = NULL;
        char *           namespace = NULL;
        CMPIInstance *   ci = NULL;
        int              ret = 0;

	cmpi_namespace = CMGetNameSpace(cop, rc);
        namespace = CMGetCharPtr(cmpi_namespace);
        
        op = CMNewObjectPath(Broker, namespace, ClassName, rc);
        if ( CMIsNullObject(op) ){
                ret = HA_FAIL;
	        CMSetStatusWithChars(Broker, rc, 
		       CMPI_RC_ERR_FAILED, "Create object path failed");
                goto out;
        }
        /* make a cluster instance */
        ci = make_cluster_instance(op, rc);
        if ( CMIsNullObject(ci) ) {
                ret = HA_FAIL;
                goto out;
        }
        
        /* return the instance */
        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        ret = HA_OK;
out:
        return ret;
}

/* enumerate cluster instances or instance names */
static int
cluster_enum_insts(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                  char ** properties, int enum_inst, CMPIStatus * rc)
{
        CMPIInstance *   ci = NULL;
        CMPIObjectPath * op = NULL;
        CMPIString *     cmpi_namespace = NULL;
        char *           namespace = NULL;
        int              ret = 0;

        cmpi_namespace = CMGetNameSpace(cop, rc);
        namespace = CMGetCharPtr(cmpi_namespace);
        
        op = CMNewObjectPath(Broker, namespace, ClassName, rc);
        if ( CMIsNullObject(op) ){
	        CMSetStatusWithChars(Broker, rc, 
		       CMPI_RC_ERR_FAILED, "Create object path failed");
                ret = HA_FAIL;
                goto out;
        }

        if ( ! enum_inst ) {
                /* enumerate instance names */
	        CMAddKey(op, "CreationClassName", ClassName, CMPI_chars);
                CMAddKey(op, "Name",  InstName, CMPI_chars);
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
	return ret;
}

/* update cluster instance, this will result in modification to the 
   ha.cf and authkeys */
static int 
cluster_update_inst(CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * op, CMPIInstance * inst,
                   CMPIStatus * rc)
{
        int ret, authnum;
        const char *authkey, *authmethod;
	struct ha_msg *cfginfo, *authinfo;

        /* ha.cf */
        if((cfginfo = cim_get_hacf_config()) == NULL ) {
				/* ha.cf not exist yet */
		if ((cfginfo = ha_msg_new(16)) == NULL ) {
			return HA_FAIL;
		}
	}

	ret = cmpi_inst2msg(inst, HA_CLUSTER, cfginfo, rc);

	/* update the ha.cf file */        
        ret = cim_update_hacf(cfginfo);
	ha_msg_del(cfginfo);

	/* authkeys */
	if ( (authinfo = cim_get_authkeys()) == NULL ) {
		if ((authinfo = ha_msg_new(2)) == NULL ) {
			return HA_FAIL;
		}
	}
        
	authnum = CMGetProperty(inst, "AuthMethod", rc).value.uint32;
	if ( rc->rc == CMPI_RC_OK ) {
        	authmethod = auth_map2value(authnum);
        	cl_msg_modstring(authinfo, "authmethod", authmethod);
	}
        authkey = CMGetPropertyString(inst, "AuthKey", rc);
	if ( authkey ) {
        	cl_msg_modstring(authinfo, "authkey", authkey);
	}
	
	/* update the authkeys file */
        ret = cim_update_authkeys(authinfo); 
	ha_msg_del(authinfo);
        return HA_OK;
}

/* clean up */
static int
cleanup_cluster () 
{
        cl_log(LOG_INFO, "%s: clean up", __FUNCTION__);
        return HA_OK;
}


/* use
	2: Enabled, 3: Disabled, 10: Reboot 
   for  hb start    hb stop       hb restart 
*/
static int
request_state_change(int state)
{
	int action [] = {0, 0, START_HB, STOP_HB, 0, /* 4 */
		         0, 0, 0, 0, 0,              /* 9 */
		         0, RESTART_HB};

	last_requested_state = state;
	cim_change_hb_state(action[state]);

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
	PROVIDER_INIT_LOGGER();        
        /* enumerate instance names */
        cluster_enum_insts(ctx, rslt, ref, NULL, FALSE, &rc);
	return rc;
}

static CMPIStatus 
ClusterEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                     CMPIObjectPath* ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();        
        cluster_enum_insts(ctx, rslt, ref, NULL, TRUE, &rc);
	return rc;
}

static CMPIStatus 
ClusterGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
        cluster_get_inst(ctx, rslt, cop, &rc);
	return rc;
}

static CMPIStatus 
ClusterCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                      CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};

	PROVIDER_INIT_LOGGER();
	if ( cim_get_hacf_config() ) {
		cl_log(LOG_WARNING, "Only one instance is allowed.");
        	CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_FAILED, "Only one instance is allowed.");
	} else {
		cluster_update_inst(ctx, rslt, cop, ci, &rc);
		CMReturn(CMPI_RC_OK);
	}
	return rc;
}


static CMPIStatus 
ClusterSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * op, CMPIInstance * inst, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        /* initialize logger */
        PROVIDER_INIT_LOGGER();
        /* set instance */
	cluster_update_inst(ctx, rslt, op, inst, &rc); 
	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ClusterDeleteInstance(CMPIInstanceMI * mi,  CMPIContext * ctx, 
                      CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
ClusterExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
                 CMPIObjectPath * ref, char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
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
        CMPIString * 	class_name = NULL;
        CMPIStatus 	rc = {CMPI_RC_OK, NULL};
	int 		ret;
        
        class_name = CMGetClassName(ref, &rc);

        if(strcasecmp(CMGetCharPtr(class_name), ClassName) == 0 &&
           strcasecmp("RequestStateChange", method_name) == 0 ){
		int state;
		/* we ignore the TimeoutPeriod parameter */
		state = CMGetArg(in, "RequestedState", &rc).value.uint32;
		ret = request_state_change(state);

		CMAddArg(out, "Job", NULL, CMPI_ref);
        }

        CMReturnData(rslt, &ret, CMPI_uint32);
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
DeclareInstanceMI(Cluster, HA_ClusterProvider, Broker);
DeclareMethodMI(Cluster, HA_ClusterProvider, Broker);
