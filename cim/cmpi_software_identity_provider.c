/*
 * cmpi_software_identity_provider.c: LinuxHA_SoftwareIdentity provider
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
#include "linuxha_info.h"

/* #define HB_CLIENT_ID   "cim-provider-sw" */
#define HB_CLIENT_ID   NULL   /* casual signon */
#define PROVIDER_ID    "cim-sw-id"

static CMPIBroker * Broker = NULL;
static char ClassName []   = "LinuxHA_SoftwareIdentity";

/**************************************************************************/
CMPIStatus 
SoftwareIdentity_Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx);
CMPIStatus 
SoftwareIdentity_EnumInstanceNames(CMPIInstanceMI* mi, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * ref);
CMPIStatus 
SoftwareIdentity_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * ref, char ** properties); 
CMPIStatus 
SoftwareIdentity_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * cop, char ** properties); 
CMPIStatus 
SoftwareIdentity_CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci);
CMPIStatus 
SoftwareIdentity_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * cop, 
               CMPIInstance * ci, char ** properties);
CMPIStatus 
SoftwareIdentity_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * cop);
CMPIStatus 
SoftwareIdentity_ExecQuery(CMPIInstanceMI * mi,	CMPIContext * ctx, 
               CMPIResult * rslt, CMPIObjectPath * ref, 
               char * lang, char * query);
/****************************************************************************/

CMPIStatus 
SoftwareIdentity_Cleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{

        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
SoftwareIdentity_EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                   CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIObjectPath * op = NULL;
        char * namespace = NULL;
        CMPIStatus rc;
        char instance_id [] = "LinuxHA:Cluster";


        init_logger(PROVIDER_ID);
        namespace = (char *)CMGetNameSpace(ref, &rc)->hdl;
 
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        CMAddKey(op, "InstanceID", instance_id, CMPI_chars); 
        CMReturnObjectPath(rslt, op);

        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
SoftwareIdentity_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * ref, 
                               char ** properties)
{
        CMPIObjectPath * op = NULL;
        CMPIEnumeration * en = NULL;
        char * namespace = NULL;
        CMPIStatus rc;


        namespace = (char *)CMGetNameSpace(ref, &rc)->hdl;
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        en = CBEnumInstanceNames(Broker, ctx, ref, &rc);
        if ( CMIsNullObject(en) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        while ( CMHasNext(en, &rc) ){
                CMPIData ref_data;
                CMPIInstance * inst = NULL;

                ref_data = CMGetNext(en, &rc);
                if (ref_data.value.ref == NULL) {
                        cl_log(LOG_INFO, "failed to get ref");
                        CMReturn(CMPI_RC_ERR_FAILED);
                }               
 
                inst = CBGetInstance(Broker, ctx,
                                ref_data.value.ref, properties, &rc);

                if ( CMIsNullObject(inst) ) {
                        return rc;
                }

                CMReturnInstance(rslt, inst); 

        }

        CMReturnDone(rslt);
	CMReturn(CMPI_RC_OK);
}


CMPIStatus 
SoftwareIdentity_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop, 
                             char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMPIInstance * ci = NULL;
        CMPIObjectPath * op = NULL;
        CMPIData key;
        char * instance_id = NULL;
        char * namespace = NULL;
        char * hbversion = NULL;
        char ** match = NULL;
        char caption [] = "Software Identity";
        int ret = 0;

        init_logger(PROVIDER_ID);
        key = CMGetKey(cop, "InstanceID", &rc);
        if ( CMIsNullObject(key.value.string) ) {
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        instance_id = CMGetCharPtr(key.value.string);
        namespace = CMGetCharPtr(CMGetNameSpace(cop, &rc));
        
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        ci = CMNewInstance(Broker, op, &rc);

        if ( CMIsNullObject(ci) ) {
                CMReturn(CMPI_RC_ERR_FAILED);
        }


	if ( ! get_hb_initialized() ) {
                if ( linuxha_initialize(HB_CLIENT_ID, 0) != HA_OK ){
                        char err_info [] = "Intialize heartbeat failed";
	                CMSetStatusWithChars(Broker, &rc, 
                                             CMPI_RC_ERR_FAILED, err_info);

                        return rc;
                }
	}


        if (hbconfig_get_str_value(KEY_HBVERSION, &hbversion) != HA_OK) {
                char err_info [] = "Initialize heartbeat failed";
	        CMSetStatusWithChars(Broker, &rc, 
                                     CMPI_RC_ERR_FAILED, err_info);
                if (get_hb_initialized () ) {
                        linuxha_finalize();
                }
                return rc;
        }

        CMSetProperty(ci, "InstanceID", instance_id, CMPI_chars);
        CMSetProperty(ci, "VersionString", hbversion, CMPI_chars);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
        

        ret = regex_search("(.*)\\.(.*)\\.(.*)", hbversion, &match);
        if ( ret == HA_OK ){
                int major = 0, minor = 0, revision = 0;
                if ( match[1] )
                        major = atoi(match[1]);
                if ( match[2] )
                        minor = atoi(match[2]);
                if ( match[3] )
                        revision = atoi(match[3]);

                cl_log(LOG_INFO, "major, minor, revision = %d, %d, %d",
                                        major, minor, revision);

                CMSetProperty(ci, "MajorVersion",  &major, CMPI_uint16);
                CMSetProperty(ci, "MinorVersion",  &minor, CMPI_uint16);
                CMSetProperty(ci, "RevisionNumber",  &revision, CMPI_uint16);

        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        
        ha_free(hbversion);
        free_2d_array(match);

        if (get_hb_initialized () ) {
                linuxha_finalize();
        }

        ASSERT( !get_hb_initialized() );

        CMReturn(CMPI_RC_OK);

}


CMPIStatus 
SoftwareIdentity_CreateInstance(CMPIInstanceMI* mi, CMPIContext * ctx, 
                                CMPIResult * rslt, CMPIObjectPath * cop,
                                CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


CMPIStatus 
SoftwareIdentity_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             CMPIInstance * ci,	char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}


CMPIStatus 
SoftwareIdentity_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

CMPIStatus 
SoftwareIdentity_ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * ref,
                           char * lang, char * query)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

/*****************************************************
 * instance MI
 ****************************************************/
DeclareInstanceMI(SoftwareIdentity_, LinuxHA_SoftwareIdentityProvider, Broker);


