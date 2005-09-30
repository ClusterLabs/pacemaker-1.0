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

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#include <clplumbing/cl_malloc.h>
#include "cmpi_cluster.h"
#include "cmpi_utils.h"

#define HB_CLIENT_ID "cim-provider-cluster"


typedef struct {
	const char* key;
	const char* property;
} hbconfig_map_t;



/* heartbeat config directives */

const hbconfig_map_t hbconfig_map [] = {
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


static CMPIInstance *
make_cluster_instance(char * classname, CMPIBroker * broker, 
                CMPIObjectPath * op, CMPIStatus * rc)
{
        CMPIInstance * ci = NULL;

        char key_creation[] = "CreationClassName";
        char key_name[] = "Name";
        char name[] = "LinuxHACluster";	

	int i = 0;

        if ( !get_hb_initialized() ) {
                return NULL;
        }

        ci = CMNewInstance(broker, op, rc);

        if ( CMIsNullObject(ci) ) {
                return NULL;
        }

        for ( i = 0; hbconfig_map[i].key != 0; i++) {
                const char * key = NULL;
                char * value = NULL;
                char * property = NULL;

                key = hbconfig_map[i].key;
		property = strdup(hbconfig_map[i].property);

                if (hbconfig_get_str_value(key, &value) != HA_OK) {
                        cl_log(LOG_WARNING, 
                                "%s: get_str_value failed, continue",
                                __FUNCTION__);
                        continue;
                }
	
		if ( value && property && ( strlen(property) != 0) ) {
        		CMSetProperty(ci, property, value, CMPI_chars);

                        free(property);

                        /*** be careful, value was malloc with ha_strdup ***/
                        ha_free(value);
		}
        }

        CMSetProperty(ci, key_creation, classname, CMPI_chars);
        CMSetProperty(ci, key_name, name, CMPI_chars);

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
                char err_info [] = "Cann't create object path";
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, err_info);

                cl_log(LOG_INFO, 
                        "%s: can not create object path", __FUNCTION__);
                ret = HA_FAIL;
                goto out;
        }

	if ( ! get_hb_initialized() ) {
	        ret = linuxha_initialize(HB_CLIENT_ID, 0);
                if (ret != HA_OK ) {
                        char err_info [] = "Cann't initialized LinuxHA";
                        cl_log(LOG_ERR, 
                                "%s: can not initialize linuxha", 
                                __FUNCTION__);
                        ret = HA_FAIL;

	                CMSetStatusWithChars(broker, rc, 
			        CMPI_RC_ERR_FAILED, err_info);
                        goto out;
                }
	}

        ci = make_cluster_instance(classname, broker, op, rc);

        if ( get_hb_initialized() ) {

                linuxha_finalize();
        }


        if ( CMIsNullObject(ci) ) {
                char err_info [] = "Cann't make instance";
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, err_info);

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
                char err_info [] = "Cann't create object path";
	        CMSetStatusWithChars(broker, rc, 
		       CMPI_RC_ERR_FAILED, err_info);

                ret = HA_FAIL;
                goto out;
        }

        if ( ! enum_inst ) {
	        CMAddKey(op, key_creation, classname, CMPI_chars);
                CMAddKey(op, key_name,  name, CMPI_chars);
	        CMReturnObjectPath(rslt, op);
        } else {

                if ( ! get_hb_initialized() ) {
                        ret = linuxha_initialize(HB_CLIENT_ID, 0);

                        if (ret != HA_OK ) {
                                char err_info [] = "Cann't initialize LinuxHA";

                                ret = HA_FAIL;

	                        CMSetStatusWithChars(broker, rc, 
			                CMPI_RC_ERR_FAILED, err_info);
                                goto out;
                        }
                }

                ci = make_cluster_instance(classname, broker, op, rc);

                if ( get_hb_initialized() ) {
                        linuxha_finalize();
                }

                if ( CMIsNullObject(ci) ) {

                        char err_info [] = "Cann't make instance";
	                CMSetStatusWithChars(broker, rc, 
		                CMPI_RC_ERR_FAILED, err_info);

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


