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
#include "cmpi_cluster.h"
#include "cmpi_utils.h"

#define HB_CLIENT_ID "cim-provider-cluster"


typedef struct key_property_pair {
	const char * key;       /* hb config key */
	const char * property;  /* CIM property name */
} key_property_pair_t;

struct hb_config_info {
        char * key;             /* hb config key */
        char * property;        /* CIM property name */
        char * value;
};


/* heartbeat config directives */

const key_property_pair_t key_property_pair [] = {
		{KEY_HBVERSION,	"HBVersion"},
		{KEY_HOST, 	""},
		{KEY_HOPS, 	""},
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
		{KEY_FAILBACK, 	""},
		{KEY_AUTOFAIL, 	"AutoFailBack"},
		{KEY_STONITH, 	"Stonith"},
		{KEY_STONITHHOST, 	"StonithHost"},
		{KEY_CLIENT_CHILD, 	""},
		{KEY_RT_PRIO, 	"RTPriority"},
		{KEY_GEN_METH, 	"GenMethod"},
		{KEY_REALTIME, 	"RealTime"},
		{KEY_DEBUGLEVEL,"DebugLevel"},
		{KEY_NORMALPOLL,""},
		{KEY_APIPERM, 	""},
		{KEY_MSGFMT, 	"MsgFmt"},
		{KEY_LOGDAEMON, "UseLogd"},
		{KEY_CONNINTVAL,"ConnLogdTime"},
		{KEY_BADPACK, 	""},
		{KEY_REGAPPHBD, "NormalPoll"},
		{KEY_COREDUMP, 	"CoreDump"},
		{KEY_COREROOTDIR, 	""},
		{KEY_REL2, 	"WithCrm"},
		{0, 0}
	};


static GPtrArray * get_config_info_table (void);
static int free_config_info_table(GPtrArray * config_info_table);

static GPtrArray *
get_config_info_table ()
{
        GPtrArray * config_info_table = NULL;
        int ret = 0;
        int i = 0;

        config_info_table = g_ptr_array_new ();

        if ( config_info_table == NULL ) {
                return NULL;
        }

        if ( ! get_hb_initialized() ) {
                ret = linuxha_initialize(HB_CLIENT_ID, 0);

                if (ret != HA_OK ) {
                        return NULL;
                }
        }

        for ( i = 0; key_property_pair[i].key != 0; i++) {
                const char * key = NULL;
                char * value = NULL;
                struct hb_config_info * config_info = NULL;

                key = key_property_pair[i].key;

                if (hbconfig_get_str_value(key, &value) != HA_OK) {
                        cl_log(LOG_WARNING, 
                                "%s: get_str_value failed, continue",
                                __FUNCTION__);
                        continue;
                }
	

                config_info = (struct hb_config_info *)
                                           malloc(sizeof(struct hb_config_info));

                if ( config_info == NULL ) {
                        cl_log(LOG_WARNING, 
                                "%s: failed to alloc, continue", __FUNCTION__);
                }

                config_info->key = strdup(key);
                config_info->value = strdup(value);

                config_info->property = strdup(key_property_pair[i].property);

                g_ptr_array_add(config_info_table, config_info);

                        /*** be careful, value was malloc with ha_strdup ***/
                ha_free(value);

        }

        if ( get_hb_initialized() ) {
                linuxha_finalize();
        }

        return config_info_table;
}

static int
free_config_info_table(GPtrArray * config_info_table)
{
        struct hb_config_info * config_info = NULL;

        while (config_info_table->len) {

                config_info = (struct hb_config_info *)
                        g_ptr_array_remove_index_fast(config_info_table, 0);
                
                free(config_info->key);
                free(config_info->value);
                free(config_info->property);

                config_info = NULL;

        }

        g_ptr_array_free(config_info_table, 0); 

        return HA_OK;
}

static CMPIInstance *
make_cluster_instance(char * classname, CMPIBroker * broker, 
                CMPIObjectPath * op, CMPIStatus * rc)
{
        CMPIInstance * ci = NULL;
        GPtrArray * config_info_table = NULL;

        char key_creation[] = "CreationClassName";
        char key_name[] = "Name";
        char name[] = "LinuxHACluster";	

	int i = 0;

        config_info_table = get_config_info_table ();

        if ( config_info_table == NULL ) {
                cl_log(LOG_ERR, "%s: can not get cluster info", __FUNCTION__);

	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't get cluster info");


                return NULL;
        }

        ci = CMNewInstance(broker, op, rc);

        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, "%s: can not create instance", __FUNCTION__);

	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't create instance");

                free_config_info_table(config_info_table);

                return NULL;
        }

        for ( i = 0; i < config_info_table->len; i++ ) {
                struct hb_config_info * config_info = NULL;

                config_info = (struct hb_config_info *)
                        g_ptr_array_index(config_info_table, i); 

                CMSetProperty(ci, config_info->property,
                                config_info->value, CMPI_chars);
        }

        CMSetProperty(ci, key_creation, classname, CMPI_chars);
        CMSetProperty(ci, key_name, name, CMPI_chars);

        free_config_info_table(config_info_table);

        return ci;        
}

int
get_cluster_instance(char * classname, CMPIBroker * broker,
	       	CMPIContext * ctx, CMPIResult * rslt, 
               	CMPIObjectPath * cop, CMPIStatus * rc)
{
        CMPIObjectPath* op = NULL;
        CMPIString * cmpi_namespace = NULL;
        char * namespace = NULL;
        CMPIInstance * ci = NULL;
        int ret = 0;

        DEBUG_ENTER();

        DEBUG_PID();
        cmpi_namespace = CMGetNameSpace(cop, rc);
        namespace = CMGetCharPtr(cmpi_namespace);
        
        op = CMNewObjectPath(broker, namespace, classname, rc);
        if ( CMIsNullObject(op) ){
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't create object path");

                cl_log(LOG_INFO, 
                        "%s: can not create object path", __FUNCTION__);
                ret = HA_FAIL;

                goto out;
        }

        ci = make_cluster_instance(classname, broker, op, rc);


        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_INFO, 
                        "%s: can not make instance", __FUNCTION__);

                ret = HA_FAIL;
                goto out;
        }
        
        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        ret = HA_OK;

out:

        DEBUG_LEAVE();
        return ret;
}

int
enumerate_cluster_instances(char * classname, CMPIBroker * broker,
	       	CMPIContext * ctx, CMPIResult * rslt,  
               	CMPIObjectPath * cop, char ** properties,
                int enum_inst, CMPIStatus * rc)
{

        CMPIInstance* ci = NULL;
        CMPIObjectPath* op = NULL;
        CMPIString * cmpi_namespace = NULL;

        char key_creation[] = "CreationClassName";
        char key_name[] = "Name";
        char name[] = "LinuxHACluster";	
        char * namespace = NULL;
        int ret = 0;


        DEBUG_PID();
        cmpi_namespace = CMGetNameSpace(cop, rc);
        namespace = CMGetCharPtr(cmpi_namespace);
        
        op = CMNewObjectPath(broker, namespace, classname, rc);

        if ( CMIsNullObject(op) ){
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, "Can't create object path");

                ret = HA_FAIL;
                goto out;
        }

        if ( ! enum_inst ) {
                /* enumerate instance names */
	        CMAddKey(op, key_creation, classname, CMPI_chars);
                CMAddKey(op, key_name,  name, CMPI_chars);
	        CMReturnObjectPath(rslt, op);

        } else {
                /* enumerate instances */
               ci = make_cluster_instance(classname, broker, op, rc);

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

int
cleanup_cluster() {
        cl_log(LOG_INFO, "%s: clean up", __FUNCTION__);
        return HA_OK;
}


