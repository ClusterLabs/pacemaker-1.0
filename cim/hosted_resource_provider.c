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

#include <hb_config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * 	PROVIDER_ID    = "cim-hosted-res";
static CMPIBroker * 	Broker       = NULL;
static char 		ClassName         [] = "HA_HostedResource"; 
static char 		Left              [] = "Antecedent";
static char 		Right             [] = "Dependent"; 
static char 		LeftClassName     [] = "HA_ClusterNode"; 
static char 		RightClassName    [] = "HA_PrimitiveResource";

static int 		node_host_res(CMPIBroker * broker, char * classname, 
				CMPIContext * ctx, CMPIObjectPath * node_op, 
				CMPIObjectPath * res_op, CMPIStatus * rc);

DeclareInstanceFunctions   (HostedResource);
DeclareAssociationFunctions(HostedResource);


/* here resource can be primitive, group, clone and master */
static int
node_host_res(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * node_op, CMPIObjectPath * res_op, 
              CMPIStatus * rc)
{
        char *		rsc_id;
	char * 		uname;
        int 		host = FALSE;
        const char * 	running_node;
	struct ha_msg * msg;

        rsc_id = CMGetKeyString(res_op, "Id", rc); 
        uname  = CMGetKeyString(node_op, "Name", rc);

        /* get running node */
        if ((msg = cim_query_dispatch(GET_RSC_HOST, rsc_id, NULL)) == NULL ) {
                cl_log(LOG_WARNING, "running node of %s is NULL", rsc_id);
                return FALSE;
        }
        
	running_node = cl_get_string(msg, "host");
        /* campare running node with uname */
        if ( strncmp(running_node, uname, MAXLEN) == 0 ) {
                host = TRUE; 
        }

	ha_msg_del(msg);
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
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName, 
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
        if (assoc_enum_insts(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName, 
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
        if (assoc_get_inst(Broker, ClassName, ctx, rslt, cop, 
                              Left, Right, &rc) != HA_OK ) {
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
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
HostedResourceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop, 
                          CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
HostedResourceDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(Broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
HostedResourceExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                        CMPIResult * rslt, CMPIObjectPath *ref,
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
        PROVIDER_INIT_LOGGER();
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
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
        PROVIDER_INIT_LOGGER();
        if (assoc_enum_associators(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
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
        PROVIDER_INIT_LOGGER();
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
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
        PROVIDER_INIT_LOGGER();
        if ( assoc_enum_references(Broker, ClassName, ctx, rslt, cop, 
                                Left, Right, LeftClassName, RightClassName,
                                result_class, role, node_host_res, NULL, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                


/***********************************************
 * Install MIs
 **********************************************/
DeclareInstanceMI(HostedResource, HA_HostedResourceProvider, Broker);
DeclareAssociationMI(HostedResource, HA_HostedResourceProvider, Broker);
