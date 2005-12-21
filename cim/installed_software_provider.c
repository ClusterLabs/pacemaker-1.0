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
#include "cluster_info.h"
#include "cmpi_utils.h"
#include "assoc_utils.h"

static const char * PROVIDER_ID    = "cim-ins-sw"; 
static CMPIBroker * G_broker       = NULL;
static char G_classname         [] = "HA_InstalledSoftwareIdentity"; 
static char G_left              [] = "System";
static char G_right             [] = "InstalledSoftware";
static char G_left_class        [] = "HA_Cluster";
static char G_right_class       [] = "HA_SoftwareIdentity";

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

        init_logger(PROVIDER_ID);

        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                NULL, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
InstalledSoftwareEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                               CMPIResult * rslt, CMPIObjectPath * cop,
                               char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);

        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                NULL, NULL, 1, &rc) != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
InstalledSoftwareGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * cop,
                              char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);

        if (cm_assoc_get_inst(G_broker, G_classname, ctx, rslt, cop, 
                              G_left, G_right, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
InstalledSoftwareCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop,
                                CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}


static CMPIStatus 
InstalledSoftwareSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             CMPIInstance * ci, char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}


static CMPIStatus 
InstalledSoftwareDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
InstalledSoftwareExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
                           CMPIResult * rslt, CMPIObjectPath * ref,
                           char * lang, char * query)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(G_broker, &rc, 
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
        init_logger(PROVIDER_ID);

        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, NULL, NULL, 1, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


static CMPIStatus
InstalledSoftwareAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                 CMPIResult * rslt, CMPIObjectPath * cop, 
                                 const char * assoc_class, const char * result_class,
                                 const char * role, const char * result_role)
{
        CMPIStatus rc;
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, NULL, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }      

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
InstalledSoftwareReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * result_class, const char * role, 
                            char ** properties)
{
        CMPIStatus rc;
        init_logger(PROVIDER_ID);

        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, NULL, NULL, 1, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
InstalledSoftwareReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop,
                                const char * result_class, const char * role)
{
        CMPIStatus rc;
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, NULL, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}                

/**************************************************************
 *   MI stub
 *************************************************************/
DeclareInstanceMI(InstalledSoftware, HA_InstalledSoftwareIdentityProvider, 
                  G_broker);
DeclareAssociationMI(InstalledSoftware, 
                     HA_InstalledSoftwareIdentityProvider, G_broker);



