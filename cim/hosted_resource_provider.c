/*
 * hosted_resource_provider.c: HA_HostedResource provider
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
#include "assoc_utils.h"
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * PROVIDER_ID    = "cim-hosted-res";
static CMPIBroker * G_broker       = NULL;
static char G_classname         [] = "HA_HostedResource"; 
static char G_left              [] = "Antecedent";
static char G_right             [] = "Dependent"; 
static char G_left_class        [] = "HA_ClusterNode"; 
static char G_right_class       [] = "HA_PrimitiveResource";

static int
node_host_res(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * node_op, CMPIObjectPath * res_op, 
              CMPIStatus * rc);
DeclareInstanceFunctions(HostedResource);
DeclareAssociationFunctions(HostedResource);


/* here resource can be primitive, group, clone and master */
static int
node_host_res(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * node_op, CMPIObjectPath * res_op, 
              CMPIStatus * rc)
{
        char * rsc_id, * uname;
        CMPIString * cmpi_rsc_id, * cmpi_uname;
        int host = 0;
        char * running_node;

        /* resource name */
        cmpi_rsc_id = CMGetKey(res_op, "Id", rc).value.string;
        if ( CMIsNullObject(cmpi_rsc_id) ) {
                cl_log(LOG_INFO, "%s: resource_name =  NULL", __FUNCTION__);
                return 0;
        }
        rsc_id = CMGetCharPtr(cmpi_rsc_id);

        /* uname */
        cmpi_uname = CMGetKey(node_op, "Name", rc).value.string;
        if ( CMIsNullObject( cmpi_uname ) ) {
                cl_log(LOG_INFO, "%s: invalid uname", __FUNCTION__);
                return 0;
        }
        uname = CMGetCharPtr(cmpi_uname);

        /* get running node */
        if ((running_node = ci_get_res_running_node(rsc_id)) == NULL ) {
                cl_log(LOG_WARNING, "running node of %s is NULL", rsc_id);
                return 0;
        }
        
        cl_log(LOG_INFO, "running node of %s is %s, this node is %s", 
               rsc_id, running_node, uname);
        /* campare running node with uname */
        if ( strcmp(running_node, uname) == 0 ) {
                host = 1; 
        }

        ci_safe_free(running_node);

        return host;
}


/**********************************************
 * Instance Provider Interface
 **********************************************/

static CMPIStatus 
HostedResourceCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
HostedResourceEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext* ctx, 
                                CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                node_host_res, NULL, 0, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
HostedResourceEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                node_host_res, NULL, 1, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus 
HostedResourceGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop, 
                          char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_get_inst(G_broker, G_classname, ctx, rslt, cop, 
                              G_left, G_right, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
HostedResourceCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop, 
                             CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
HostedResourceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop, 
                          CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
HostedResourceDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
HostedResourceExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                        CMPIResult * rslt, CMPIObjectPath *ref,
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
HostedResourceAssociationCleanup(CMPIAssociationMI * mi, 
                                  CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
HostedResourceAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop, 
                          const char * assoc_class,  const char * result_class,
                          const char * role, const char * result_role, 
                          char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, node_host_res, NULL, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus
HostedResourceAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop, 
                              const char * assoc_class, const char * result_class,
                              const char * role, const char * result_role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, node_host_res, NULL, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
HostedResourceReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                         CMPIResult * rslt, CMPIObjectPath * cop, 
                         const char * result_class, const char * role, 
                         char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, node_host_res, NULL, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
HostedResourceReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop,
                             const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, node_host_res, NULL, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                


/***********************************************
 * Install MIs
 **********************************************/
DeclareInstanceMI(HostedResource, HA_HostedResourceProvider, G_broker);
DeclareAssociationMI(HostedResource, HA_HostedResourceProvider, G_broker);
