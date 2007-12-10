/*
 * installed_software_provider.c: 
 *                    HA_InstalledSoftwareIdentity provider
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
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * PROVIDER_ID    = "cim-ins-sw"; 
static CMPIBroker * Broker       = NULL;
static char ClassName         [] = "HA_InstalledSoftwareIdentity"; 
static char Left              [] = "System";
static char Right             [] = "InstalledSoftware";
static char LeftClassName        [] = "HA_Cluster";
static char RightClassName       [] = "HA_SoftwareIdentity";

DeclareInstanceFunctions(InstalledSoftware);
DeclareAssociationFunctions(InstalledSoftware);

/**********************************************
 * Instance
 **********************************************/

static CMPIStatus 
InstalledSoftwareCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
	CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
InstalledSoftwareEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext* ctx, 
                                   CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
        assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName, 
                                NULL, NULL, FALSE, &rc);
	return rc;
}


static CMPIStatus 
InstalledSoftwareEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                               CMPIResult * rslt, CMPIObjectPath * cop,
                               char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
        assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, 
				Left, Right, LeftClassName, RightClassName,
				NULL, NULL, TRUE, &rc);
	return rc;
}


static CMPIStatus 
InstalledSoftwareGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * cop,
                              char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        PROVIDER_INIT_LOGGER();
        assoc_get_inst(Broker, ClassName, ctx, rslt, cop, Left, Right, &rc);
	return rc;
}


static CMPIStatus 
InstalledSoftwareCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop,
                                CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
InstalledSoftwareSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             CMPIInstance * ci, char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}


static CMPIStatus 
InstalledSoftwareDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
InstalledSoftwareExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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
static CMPIStatus 
InstalledSoftwareAssociationCleanup(CMPIAssociationMI * mi, 
                                    CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
InstalledSoftwareAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop, 
                             const char * assoc_class, const char * result_class,
                             const char * role, const char * result_role, 
                             char ** properties)
{
        CMPIStatus rc;
        PROVIDER_INIT_LOGGER();
        assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                assoc_class, result_class, role, 
                                result_role, NULL, NULL, TRUE, &rc);
	return rc;
}


static CMPIStatus
InstalledSoftwareAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                 CMPIResult * rslt, CMPIObjectPath * cop, 
                                 const char * assoc_class, const char * result_class,
                                 const char * role, const char * result_role)
{
        CMPIStatus rc;
        PROVIDER_INIT_LOGGER();
        assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                assoc_class, result_class, role, 
                                result_role, NULL, NULL, FALSE, &rc);
	return rc;
}

static CMPIStatus
InstalledSoftwareReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * result_class, const char * role, 
                            char ** properties)
{
        CMPIStatus rc;
        PROVIDER_INIT_LOGGER();
	assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, NULL, NULL, TRUE, &rc);
	return rc;
}

static CMPIStatus
InstalledSoftwareReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop,
                                const char * result_class, const char * role)
{
        CMPIStatus rc;
	assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, NULL, NULL, FALSE, &rc);
	return rc;
}                

/**************************************************************
 *   MI stub
 *************************************************************/
DeclareInstanceMI(InstalledSoftware, 
	HA_InstalledSoftwareIdentityProvider, Broker);
DeclareAssociationMI(InstalledSoftware, 
	HA_InstalledSoftwareIdentityProvider, Broker);



