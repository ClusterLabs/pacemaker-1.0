/*
 * participating_node_provider.c:  HA_ParticipatingNode provider
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
#include "cmpi_utils.h"
#include "cluster_info.h"
#include "cmpi_utils.h"

#define PROVIDER_ID 	"cim-par-node"
static CMPIBroker * 	Broker    	= NULL;
static char 		ClassName      [] = "HA_ParticipatingNode"; 
static char 		Left           [] = "Dependent";
static char 		Right          [] = "Antecedent";
static char 		LeftClassName  [] = "HA_Cluster";
static char 		RightClassName [] = "HA_ClusterNode";

DeclareInstanceFunctions   (ParticipatingNode);
DeclareAssociationFunctions(ParticipatingNode);

/**********************************************
 * Instance Provider Interface
 **********************************************/
static CMPIStatus 
ParticipatingNodeCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
	CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ParticipatingNodeEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                   CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();

        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, NULL, NULL, 0, &rc) 
		!= HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ParticipatingNodeEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                               CMPIResult * rslt, CMPIObjectPath * cop,
                               char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, Left, Right, 
			LeftClassName, RightClassName, NULL, NULL, 1, &rc) 
		!= HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ParticipatingNodeGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
	PROVIDER_INIT_LOGGER();
        if ( assoc_get_inst(Broker, ClassName, ctx, rslt, cop, 
                              Left, Right, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ParticipatingNodeCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop,
                                CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
ParticipatingNodeSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             CMPIInstance * ci, char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}

static CMPIStatus 
ParticipatingNodeDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

static CMPIStatus 
ParticipatingNodeExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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
ParticipatingNodeAssociationCleanup(CMPIAssociationMI * mi, 
                                    CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
ParticipatingNodeAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop, 
                             const char * assoc_class, const char * result_class,
                             const char * role, const char * result_role, 
                             char ** properties)
{
        CMPIStatus rc;
	PROVIDER_INIT_LOGGER();
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                assoc_class, result_class, role, 
                                result_role, NULL, NULL, 1, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
ParticipatingNodeAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                 CMPIResult * rslt, CMPIObjectPath * cop, 
                                 const char * assoc_class, const char * result_class,
                                 const char * role, const char * result_role)
{
        CMPIStatus rc;
	PROVIDER_INIT_LOGGER();
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName, 
                                assoc_class, result_class, role, 
                                result_role, NULL, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
ParticipatingNodeReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * result_class, const char * role, 
                            char ** properties)
{
        CMPIStatus rc;
	PROVIDER_INIT_LOGGER();
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, NULL, NULL, 1, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
ParticipatingNodeReferenceNames(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                const char * result_class, const char * role)
{
        CMPIStatus rc;
	PROVIDER_INIT_LOGGER();
        if (assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                               Left, Right, LeftClassName, RightClassName,
                               result_class, role, NULL, NULL, 0, &rc) != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}                

/**************************************************************
 *      MI stub
 *************************************************************/
DeclareInstanceMI   (ParticipatingNode, HA_ParticipatingNodeProvider, Broker);
DeclareAssociationMI(ParticipatingNode, HA_ParticipatingNodeProvider, Broker);
