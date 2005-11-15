/*
 * cmpi_participating_node_provider.c:  LinuxHA_ParticipatingNode provider
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
#include "cmpi_utils.h"
#include "cmpi_cluster.h"
#include "cmpi_node.h"

#include "linuxha_info.h"

#define PROVIDER_ID "cim-par-node"

static CMPIBroker * Broker       = NULL;
static char ClassName         [] = "LinuxHA_ParticipatingNode"; 
static char node_ref          [] = "Antecedent";
static char cluster_ref       [] = "Dependent";
static char node_class_name   [] = "LinuxHA_ClusterNode";
static char cluster_class_name[] = "LinuxHA_Cluster";



/***************** instance interfaces *******************/
CMPIStatus 
ParticipatingNode_Cleanup(CMPIInstanceMI * mi, 
                CMPIContext * ctx);
CMPIStatus 
ParticipatingNode_EnumInstanceNames(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref);
CMPIStatus 
ParticipatingNode_EnumInstances(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref,
		char ** properties);
CMPIStatus 
ParticipatingNode_GetInstance(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt,
		CMPIObjectPath * cop, char ** properties);
CMPIStatus 
ParticipatingNode_CreateInstance(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath *cop,
		CMPIInstance* ci);
CMPIStatus 
ParticipatingNode_SetInstance(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
		CMPIInstance * ci, char ** proerpties);
CMPIStatus 
ParticipatingNode_DeleteInstance(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop);
CMPIStatus 
ParticipatingNode_ExecQuery(CMPIInstanceMI * mi,
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * ref,
		char * lang, char * query);

/*********************** association interfaces ***********************/

CMPIStatus 
ParticipatingNode_AssociationCleanup(CMPIAssociationMI * mi, 
                CMPIContext * ctx);
CMPIStatus
ParticipatingNode_Associators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * op, 
                const char * asscClass, const char * resultClass,
                const char * role, const char * resultRole, char ** properties);
CMPIStatus
ParticipatingNode_AssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                CMPIResult * rslt, CMPIObjectPath * op, 
                const char * asscClass, const char * resultClass,
                const char * role, const char * resultRole);
CMPIStatus
ParticipatingNode_References(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * op, const char * resultClass,
                const char * role, char ** properties);
CMPIStatus
ParticipatingNode_ReferenceNames(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                const char * resultClass, const char * role);

/**********************************************
 * Instance Provider Interface
 **********************************************/

CMPIStatus 
ParticipatingNode_Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
	CMReturn(CMPI_RC_OK);
}


CMPIStatus 
ParticipatingNode_EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                    CMPIResult * rslt, CMPIObjectPath * ref)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (enum_inst_assoc(Broker, ClassName, cluster_ref, node_ref,
                            cluster_class_name, node_class_name,
                            ctx, rslt, ref, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}


CMPIStatus 
ParticipatingNode_EnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx,
                                CMPIResult * rslt, CMPIObjectPath * ref,
                                char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (enum_inst_assoc(Broker, ClassName, cluster_ref, node_ref,
                            cluster_class_name, node_class_name,
                            ctx, rslt, ref, NULL, 1, &rc) != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

CMPIStatus 
ParticipatingNode_GetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * cop,
                              char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (get_inst_assoc(Broker, ClassName, cluster_ref, node_ref, 
                           cluster_class_name, node_class_name,
                           ctx, rslt, cop, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

CMPIStatus 
ParticipatingNode_CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                 CMPIResult * rslt, CMPIObjectPath * cop,
                                 CMPIInstance * ci)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

CMPIStatus 
ParticipatingNode_SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                              CMPIResult * rslt, CMPIObjectPath * cop,
                              CMPIInstance * ci, char ** properties)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;

}

CMPIStatus 
ParticipatingNode_DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,
                                 CMPIResult * rslt, CMPIObjectPath * cop)
{
	CMPIStatus rc = {CMPI_RC_OK, NULL};
	CMSetStatusWithChars(Broker, &rc, 
			CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
	return rc;
}

CMPIStatus 
ParticipatingNode_ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,
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
ParticipatingNode_AssociationCleanup(CMPIAssociationMI * mi, 
                                     CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
ParticipatingNode_Associators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop, 
                              const char * assocClass, const char * resultClass,
                              const char * role, const char * resultRole, 
                              char ** properties)
{
        CMPIStatus rc;
        init_logger(PROVIDER_ID);
        cl_log(LOG_INFO, 
                "%s: asscClass, resultClass, role, resultRole = %s, %s, %s, %s",
                __FUNCTION__,
                assocClass, resultClass, role, resultRole);
        if (enum_associators(Broker, ClassName, node_ref, cluster_ref,
                             node_class_name, cluster_class_name, ctx,
                             rslt, cop, assocClass, resultClass, role, 
                             resultRole, NULL, 1, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
ParticipatingNode_AssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                  CMPIResult * rslt, CMPIObjectPath * cop, 
                                  const char * assocClass, const char * resultClass,
                                  const char * role, const char * resultRole)
{
        CMPIStatus rc;
        if (enum_associators(Broker, ClassName, node_ref, cluster_ref, 
                             node_class_name, cluster_class_name, ctx, 
                             rslt, cop, assocClass, resultClass, role, 
                             resultRole, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
ParticipatingNode_References(CMPIAssociationMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * op, 
                             const char * resultClass, const char * role, 
                             char ** properties)
{
        CMPIStatus rc;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        if ( enum_references(Broker, ClassName, node_ref, cluster_ref,
                             node_class_name, cluster_class_name, ctx, 
                             rslt, op, resultClass, role, 
                             NULL, 1, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

CMPIStatus
ParticipatingNode_ReferenceNames(CMPIAssociationMI * mi,
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                const char * resultClass, const char * role)
{
        CMPIStatus rc;
        init_logger(PROVIDER_ID);
        DEBUG_ENTER();
        if (enum_references(Broker, ClassName, node_ref, cluster_ref,
                            node_class_name, cluster_class_name, ctx, 
                            rslt, cop, resultClass, role, 
                            NULL, 0, &rc) != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}                

/**************************************************************
 *      MI stub
 *************************************************************/
DeclareInstanceMI(ParticipatingNode_, 
                  LinuxHA_ParticipatingNodeProvider, Broker);
DeclareAssociationMI(ParticipatingNode_, 
                  LinuxHA_ParticipatingNodeProvider, Broker);
