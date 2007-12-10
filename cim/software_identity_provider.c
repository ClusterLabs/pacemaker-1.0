/*
 * cmpi_software_identity_provider.c: HA_SoftwareIdentity provider
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

#include <crm_internal.h>
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

static const char * PROVIDER_ID = "cim_soft";
static CMPIBroker * Broker    = NULL;
static char ClassName []      = "HA_SoftwareIdentity";
DeclareInstanceFunctions(SoftwareIdentity);


static CMPIStatus 
SoftwareIdentityCleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
SoftwareIdentityEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                  CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIObjectPath * op = NULL;
        char * namespace = NULL;
        CMPIStatus rc;
        char instance_id [] = "LinuxHA:Cluster";

	PROVIDER_INIT_LOGGER();
	DEBUG_ENTER();
        namespace = CMGetCharPtr(CMGetNameSpace(ref, &rc));
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        CMAddKey(op, "InstanceID", instance_id, CMPI_chars); 
        CMReturnObjectPath(rslt, op);
	DEBUG_LEAVE();
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
SoftwareIdentityEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * ref, 
                              char ** properties)
{
        CMPIObjectPath * op = NULL;
        CMPIEnumeration * en = NULL;
        char * namespace = NULL;
        CMPIStatus rc;

	PROVIDER_INIT_LOGGER();
	DEBUG_ENTER();
        namespace = CMGetCharPtr(CMGetNameSpace(ref, &rc));
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
	DEBUG_LEAVE();
	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
SoftwareIdentityGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMPIInstance * ci = NULL;
        CMPIObjectPath * op = NULL;
        CMPIString * key;
        char * instance_id = NULL;
        char * namespace = NULL;
        char * hbversion = NULL;
        char ** match = NULL;
        char caption [] = "SoftwareIdentity";
        int len = 0;
	struct ha_msg * info = NULL;

	PROVIDER_INIT_LOGGER();
	DEBUG_ENTER();
        key = CMGetKey(cop, "InstanceID", &rc).value.string;
        if ( CMIsNullObject(key) ) {
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        instance_id = CMGetCharPtr(key);
        namespace = CMGetCharPtr(CMGetNameSpace(cop, &rc));
        
        op = CMNewObjectPath(Broker, namespace, ClassName, &rc);
        if ( CMIsNullObject(op) ){
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        ci = CMNewInstance(Broker, op, &rc);
        if ( CMIsNullObject(ci) ) {
                CMReturn(CMPI_RC_ERR_FAILED);
        }

        /* get config table */
        if ( ( info = cim_get_software_identity () ) == NULL ) {
		cl_log(LOG_ERR, "can't get software_identity");
                CMReturn(CMPI_RC_ERR_FAILED);
        }
        
        /* search KEY_HBVERSION in keys */
        hbversion = cim_strdup(cl_get_string(info, "hbversion"));

        /* set properties */
        CMSetProperty(ci, "InstanceID", instance_id, CMPI_chars);
        CMSetProperty(ci, "VersionString", hbversion, CMPI_chars);
        CMSetProperty(ci, "Caption", caption, CMPI_chars);
        
        /* convert char * to int */
	match = split_string(hbversion, &len, ".");
        if ( match && len == 3 ){
                int major = 0, minor = 0, revision = 0;
                if ( match[0] )
                        major = atoi(match[0]);
                if ( match[1] )
                        minor = atoi(match[1]);
                if ( match[2] )
                        revision = atoi(match[2]);

                CMSetProperty(ci, "MajorVersion",  &major, CMPI_uint16);
                CMSetProperty(ci, "MinorVersion",  &minor, CMPI_uint16);
                CMSetProperty(ci, "RevisionNumber",  &revision, CMPI_uint16);
        	free_2d_array(match, len, cim_free);
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
	cim_free(hbversion);
	ha_msg_del(info);
	DEBUG_LEAVE();
	CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
SoftwareIdentityCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop,
                               CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
SoftwareIdentitySetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             CMPIInstance * ci,	char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}


static CMPIStatus 
SoftwareIdentityDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                               CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
SoftwareIdentityExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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
DeclareInstanceMI(SoftwareIdentity, HA_SoftwareIdentityProvider, Broker);


