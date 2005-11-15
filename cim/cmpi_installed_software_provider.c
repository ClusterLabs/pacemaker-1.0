/*
 * cmpi_installed_software_provider.c: 
 *                    LinuxHA_InstalledSoftwareIdentity provider
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
#include <cmpi_utils.h>
#include <hb_api.h>
#include "linuxha_info.h"


#define PROVIDER_ID                  "cim-ins-sw" 
static CMPIBroker * Broker         = NULL;
static char ClassName           [] = "LinuxHA_InstalledSoftwareIdentity"; 
static char cluster_ref         [] = "System";
static char software_ref        [] = "InstalledSoftware";
static char cluster_class_name  [] = "LinuxHA_Cluster";
static char software_class_name [] = "LinuxHA_SoftwareIdentity";

/***************** instance interfaces *******************/
CMPIStatus 
InstalledSoftware_Cleanup(CMPIInstanceMI * mi, 
                CMPIContext * ctx);
CMPIStatus 
InstalledSoftware_EnumInstanceNames(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
CMPIStatus 
InstalledSoftware_EnumInstances(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * ref, char ** properties);
CMPIStatus 
InstalledSoftware_GetInstance(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * cop,  char ** properties);
CMPIStatus 
InstalledSoftware_CreateInstance(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * cop, CMPIInstance * ci);
CMPIStatus 
InstalledSoftware_SetInstance(CMPIInstanceMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * cop, CMPIInstance * ci,
		char ** properties);
CMPIStatus 
InstalledSoftware_DeleteInstance(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);
CMPIStatus 
InstalledSoftware_ExecQuery(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * ref, char * lang, char * query);

/*********************** association interfaces ***********************/
CMPIStatus 
InstalledSoftware_AssociationCleanup(CMPIAssociationMI * mi, 
                CMPIContext * ctx);
CMPIStatus
InstalledSoftware_Associators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * op, 
                const char * asscClass, const char * resultClass,
                const char * role, const char * resultRole, char ** properties);
CMPIStatus
InstalledSoftware_AssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * op, const char * asscClass, 
                const char * resultClass, const char * role, 
                const char * resultRole);
CMPIStatus
InstalledSoftware_References(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * op, const char * resultClass,
                const char * role, char ** properties);

CMPIStatus
InstalledSoftware_ReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * cop, const char * assocClass,
                const char * role);

/**********************************************
 * Instance
 **********************************************/

CMPIStatus 
InstalledSoftware_Cleanup(CMPIInstanceMI* mi, CMPIContext* ctx)
{
	CMReturn(CMPI_RC_OK);
}


CMPIStatus 
InstalledSoftware_EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext* ctx, 
                                    CMPIResult * rslt, CMPIObjectPath * ref)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;

        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        ret = enum_inst_assoc(Broker, ClassName, cluster_ref, software_ref, 
                              cluster_class_name, software_class_name,
                              ctx, rslt, ref, NULL, 0, &rc);
        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
InstalledSoftware_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * ref,
                                char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = enum_inst_assoc(Broker, ClassName, cluster_ref, software_ref, 
                              cluster_class_name, software_class_name,
                              ctx, rslt, ref, NULL, 1, &rc);
        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
InstalledSoftware_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * cop,
                              char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        int ret = 0;
        init_logger(PROVIDER_ID);

        DEBUG_ENTER();
        ret = get_inst_assoc(Broker, ClassName, cluster_ref, software_ref, 
                             cluster_class_name, software_class_name,
                             ctx, rslt, cop, &rc);

        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


CMPIStatus 
InstalledSoftware_CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                 CMPIResult * rslt, CMPIObjectPath * cop,
                                 CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


CMPIStatus 
InstalledSoftware_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * cop,
                              CMPIInstance * ci, char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}


CMPIStatus 
InstalledSoftware_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                 CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

CMPIStatus 
InstalledSoftware_ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                            CMPIResult * rslt, CMPIObjectPath * ref,
                            char * lang, char * query)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}

/****************************************************
 * Association
 ****************************************************/
CMPIStatus 
InstalledSoftware_AssociationCleanup(CMPIAssociationMI * mi, 
                                     CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
InstalledSoftware_Associators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * op, 
                              const char * assocClass, const char * resultClass,
                              const char * role, const char * resultRole, 
                              char ** properties)
{
        CMPIStatus rc;
        int ret = 0;
        init_logger(PROVIDER_ID);

        DEBUG_ENTER();

        cl_log(LOG_INFO, 
                "asscClass, resultClass, role, resultRole = %s, %s, %s, %s",
                assocClass, resultClass, role, resultRole);

        ret = enum_associators(Broker, ClassName, software_ref, cluster_ref,
                               software_class_name, cluster_class_name, ctx, 
                               rslt, op, assocClass, resultClass, 
                               role, resultRole, NULL, 1, &rc);
 
        DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


CMPIStatus
InstalledSoftware_AssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                  CMPIResult * rslt, CMPIObjectPath * cop, 
                                  const char * assocClass, const char * resultClass,
                                  const char * role, const char * resultRole)
{
        CMPIStatus rc;
        int ret = 0;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        cl_log(LOG_INFO, 
                "asscClass, resultClass, role, resultRole = %s, %s, %s, %s",
                assocClass, resultClass, role, resultRole);

        ret = enum_associators(Broker, ClassName, software_ref, cluster_ref,
                               software_class_name, cluster_class_name, ctx, 
                               rslt, cop, assocClass, resultClass, 
                               role, resultRole, NULL, 0, &rc);
         DEBUG_LEAVE();
        if ( ret != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
InstalledSoftware_References(CMPIAssociationMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop, 
                             const char * resultClass, const char * role, 
                             char ** properties)
{
        int ret = 0;
        CMPIStatus rc;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        cl_log(LOG_INFO, 
                "resultClass, role = %s, %s", resultClass, role);

        ret = enum_references(Broker, ClassName, cluster_ref, software_ref,
                              cluster_class_name, software_class_name,
                              ctx, rslt, cop, resultClass, role, NULL, 1, &rc);
         DEBUG_LEAVE();
        if ( ret != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
InstalledSoftware_ReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx,
                                 CMPIResult * rslt, CMPIObjectPath * cop,
                                 const char * resultClass, const char * role)
{
        int ret = 0;
        CMPIStatus rc;
        ret = enum_references(Broker, ClassName, cluster_ref, software_ref,
                              cluster_class_name, software_class_name,
                              ctx, rslt, cop, resultClass, role, NULL, 0, &rc);
        if ( ret != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}                

/**************************************************************
 *   MI stub
 *************************************************************/
DeclareInstanceMI(InstalledSoftware_, LinuxHA_InstalledSoftwareIdentityProvider,
                  Broker);
DeclareAssociationMI(InstalledSoftware_, 
                     LinuxHA_InstalledSoftwareIdentityProvider, Broker);



