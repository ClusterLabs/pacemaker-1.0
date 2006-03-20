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
static const mapping_t ClusterMap [] = { MAPPING_HA_Cluster };

static CMPIInstance * make_cluster_instance(CMPIObjectPath * op, 
			CMPIStatus * rc);

static int      get_cluster(CMPIContext * ctx, CMPIResult * rslt,
			CMPIObjectPath * cop, CMPIStatus * rc);

static int      enumerate_cluster(CMPIContext * ctx, CMPIResult * rslt, 
                        CMPIObjectPath * cop, char ** properties,
                        int enum_inst, CMPIStatus * rc);

static uint32_t auth_value2map(const char * method);
static const char *   auth_map2value(uint32_t map);

static void     config_set_properties(CMPIInstance * ci, CMPIStatus * rc);
static void     auth_set_properties(CMPIInstance * ci, CMPIStatus * rc);
static void     crm_set_properties(CMPIInstance * ci, CMPIStatus * rc);

/* set instance */
static int	process_config_data(const mapping_t * property, 
			CIMTable * cfginfo, CMPIData cmpidata, 
			CMPIStatus * rc);

static int      update_inst_cluster(CMPIContext * ctx, CMPIResult * rslt, 
                        CMPIObjectPath * op, CMPIInstance * inst, 
                        CMPIStatus * rc);
static int      request_state_change(int state);
static int      cleanup_cluster(void); 

DeclareInstanceFunctions(Cluster);


static uint32_t
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
config_set_properties(CMPIInstance * ci, CMPIStatus * rc)
{
	CIMTable * info;

        DEBUG_ENTER();
	/* get cluster config values */
        info = cim_get_hacf_config ();
        if ( info == NULL ) {
                cl_log(LOG_ERR, "%s:can not get cluster.", __FUNCTION__);
        	return;
	}
	dump_cim_table(info, 0);
	cmpi_set_properties(Broker, ci, info, 
			ClusterMap, MAPDIM(ClusterMap), rc); 
	DEBUG_LEAVE();
	cim_table_free(info);
}

static void
auth_set_properties(CMPIInstance * ci, CMPIStatus * rc)
{
	CIMTable * table = NULL;
        uint32_t   num;
        char*      method;
	char*      key;

	DEBUG_ENTER();
        if ( (table = cim_get_cluster_auth()) == NULL  ){
		cl_log(LOG_WARNING, "Can not get auth message");
                 return;
        }

	dump_cim_table(table, 0);
        method = cim_table_lookup_v(table, "authmethod").v.str;
        key = cim_table_lookup_v(table, "authkey").v.str;

        num = auth_value2map(method);
        CMSetProperty(ci, "AuthMethod", &num, CMPI_uint32);
        CMSetProperty(ci, "AuthKey", key, CMPI_chars);
	
	DEBUG_LEAVE();
	cim_table_free(table);
}

static void
crm_set_properties(CMPIInstance * ci, CMPIStatus * rc)
{
        CIMTable * crm_config;
        char * it, * sc, * se;
        char * nq, * rs, * hq;

        if (( crm_config = cim_get(GET_CRM_CONFIG, NULL, NULL)) == NULL ) {
		return;
	}
                
        it = cim_table_lookup_v(crm_config, "transition_idle_timeout").v.str;
        sc = cim_table_lookup_v(crm_config, "symmetric_cluster").v.str;
        se = cim_table_lookup_v(crm_config, "stonith_enabled").v.str;
        nq = cim_table_lookup_v(crm_config, "no_quorum_policy").v.str;
        rs = cim_table_lookup_v(crm_config, "default_resource_stickiness").v.str;
        hq = cim_table_lookup_v(crm_config, "have_quorum").v.str;
                
        if (it && sc && se && nq && rs && hq) {
        	CMSetProperty(ci, "TransitionIdleTimeout",     it, CMPI_chars);
                CMSetProperty(ci, "SymmetricCluster",          sc, CMPI_chars);
	        CMSetProperty(ci, "StonithEnabled",            se, CMPI_chars);
                CMSetProperty(ci, "NoQuorumPolicy",            nq, CMPI_chars);
                CMSetProperty(ci, "DefaultResourceStickiness", rs, CMPI_chars);
                CMSetProperty(ci, "HaveQuorum",                hq, CMPI_chars);
	}

	cim_table_free(crm_config);
}

/* always only one cluster instance */
static CMPIInstance *
make_cluster_instance(CMPIObjectPath * op, CMPIStatus * rc)
{
        CMPIInstance *         ci = NULL;
	uint32_t               state = 0;
	CIMTable *           table;

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
	config_set_properties(ci, rc);

        /* set auth properties */
        auth_set_properties(ci, rc);

	state = 3;	/* Disabled */
        if( cim_get_hb_status() == HB_RUNNING) {
		state = 2; /* Enabled */
	       	/* set crm properties */
		crm_set_properties(ci, rc); 
	} 

	CMSetProperty(ci, "EnabledState", &state, CMPI_uint32);
	CMSetProperty(ci, "RequestedState", &last_requested_state, CMPI_uint32);

	if ( (table = cim_get_software_identity() )){
		char * hv;
		hv = cim_table_lookup_v(table, "hbversion").v.str;
		dump_cim_table(table, 0);
		if (hv) {
			CMSetProperty(ci, "HBVersion", hv, CMPI_chars);
		}
	}
	DEBUG_LEAVE();
        return ci;        
}

/* get a cluster instance */
static int
get_cluster(CMPIContext * ctx, CMPIResult * rslt, 
                 CMPIObjectPath * cop, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIString *     cmpi_namespace = NULL;
        char *           namespace = NULL;
        CMPIInstance *   ci = NULL;
        int              ret = 0;

        DEBUG_ENTER();
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
	DEBUG_LEAVE();
        return ret;
}

/* enumerate cluster instances or instance names */
static int
enumerate_cluster(CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                  char ** properties, int enum_inst, CMPIStatus * rc)
{
        CMPIInstance *   ci = NULL;
        CMPIObjectPath * op = NULL;
        CMPIString *     cmpi_namespace = NULL;
        char *           namespace = NULL;
        int              ret = 0;

	DEBUG_ENTER();
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
	DEBUG_LEAVE();
	return ret;
}

static int
process_config_data(const mapping_t * property, CIMTable * cfginfo, 
		CMPIData cmpidata, CMPIStatus * rc)
{
	const char * key = property->key;
        const char * val = NULL;

	DEBUG_ENTER();
	if ( key == NULL ) {
		return HA_FAIL;
	}
	if ( property->type == CMPI_chars ){
		if ( cmpidata.value.string == NULL ) {
			return HA_FAIL;
		}
		val = CMGetCharPtr(cmpidata.value.string);
		/* insert a data */
		cim_table_strdup_replace(cfginfo, key, val);
	} else if(property->type == CMPI_charsA) { /* array */
		CMPIArray * array = cmpidata.value.array;
		int         i;
		int         len;
		char        buf[MAXLEN] = "";

		len = CMGetArrayCount(array, rc);
		for ( i = 0; i < len; i++){
			char * option;
			cmpidata = CMGetArrayElementAt(array, i, rc);
			if (cmpidata.value.string && rc->rc==CMPI_RC_OK){
				option=CMGetCharPtr(cmpidata.value.string);
				strncat(buf, option, MAXLEN);
				strcat(buf, "\n");
			}
		}
		val = buf;
		/* insert into table */
		cim_table_strdup_replace(cfginfo, key, val);
	}

	cl_log(LOG_INFO, "process_config_data: %s:%s", key, val); 
	DEBUG_LEAVE();
}

/* update cluster instance, this will result in modification to the 
   ha.cf and authkeys */
static int 
update_inst_cluster(CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * op, CMPIInstance * inst,
                   CMPIStatus * rc)
{
        int          ret;
        int          authnum;
        const char * authkey;
	const char * authmethod;
	CIMTable * cfginfo;
	CIMTable * authinfo;
	CMPIData     cmpidata;
	int 		i;

        /* ha.cf */
	DEBUG_ENTER();
	cfginfo = cim_get_hacf_config();
	if ( cfginfo == NULL ) { /* ha.cf not exist yet */
		cfginfo = cim_table_new();
		if ( cfginfo == NULL ) {
			return HA_FAIL;
		}
	}

	for ( i=0; i<sizeof(ClusterMap)/sizeof(mapping_t); i++) {
		CMPIData cmpidata;
		cmpidata = CMGetProperty(inst, ClusterMap[i].name, rc);
		if ( rc->rc == CMPI_RC_OK ) {
			process_config_data(&ClusterMap[i], cfginfo, cmpidata, rc);
                }
        }

	/* update the ha.cf file */        
        ret = cim_set_cluster_config(cfginfo);
	cim_table_free(cfginfo);

	/* authkeys */
	if ( (authinfo = cim_get_cluster_auth()) == NULL ) {
		authinfo = cim_table_new();
		if ( authinfo == NULL ) {
			return HA_FAIL;
		}
	}
        
	authnum = CMGetProperty(inst, "AuthMethod", rc).value.uint32;
	if ( rc->rc == CMPI_RC_OK ) {
        	authmethod = auth_map2value(authnum);
        	cim_table_strdup_replace(authinfo, "authmethod", authmethod);
	}
        cmpidata = CMGetProperty(inst, "AuthKey", rc);
	if ( rc->rc == CMPI_RC_OK ) {
		authkey = CMGetCharPtr(cmpidata.value.string);
        	cim_table_strdup_replace(authinfo, "authkey", authkey);
	}
	
	/* update the authkeys file */
        ret = cim_set_cluster_auth(authinfo); 
	cim_table_free(authinfo);
	DEBUG_LEAVE();
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
        enumerate_cluster(ctx, rslt, ref, NULL, FALSE, &rc);
	return rc;
}

static CMPIStatus 
ClusterEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                     CMPIObjectPath* ref, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();        
        enumerate_cluster(ctx, rslt, ref, NULL, TRUE, &rc);
	return rc;
}

static CMPIStatus 
ClusterGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                   CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
        get_cluster(ctx, rslt, cop, &rc);
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
		update_inst_cluster(ctx, rslt, cop, ci, &rc);
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
	update_inst_cluster(ctx, rslt, op, inst, &rc); 
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
