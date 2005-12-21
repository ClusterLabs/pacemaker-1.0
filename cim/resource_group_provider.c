/*
 * resource_group_provider.c: HA_ClusterResourceGroup provider
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
#include "cmpi_utils.h"
#include "cluster_info.h"
#include "resource_common.h"

static const char * PROVIDER_ID    = "cim-res-grp";
static CMPIBroker * G_broker       = NULL;
static char         G_classname [] = "HA_ResourceGroup"; 

DeclareInstanceFunctions(ResourceGroup);

/**********************************************
 * Instance
 **********************************************/

static CMPIStatus 
ResourceGroupCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMPIStatus rc;
        resource_cleanup(G_broker, G_classname, mi, ctx, TID_RES_GROUP, &rc);
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ResourceGroupEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * ref)
{
        CMPIStatus rc;
        init_logger( PROVIDER_ID );
        cl_log(LOG_INFO,"%s", G_classname);

        if ( enum_inst_resource(G_broker, G_classname, ctx, rslt, ref, 0, 
                                TID_RES_GROUP, &rc) != HA_OK ){
                return rc;
        }
        CMReturn(CMPI_RC_OK);        
}

static CMPIStatus 
ResourceGroupEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * ref, 
                           char ** properties)
{
        CMPIStatus rc;
        init_logger( PROVIDER_ID );

        if ( enum_inst_resource(G_broker, G_classname, ctx, rslt, ref, 1, 
                                TID_RES_GROUP, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ResourceGroupGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                         CMPIResult * rslt, CMPIObjectPath * cop, 
                         char ** properties)
{
        CMPIStatus rc;
        if ( get_inst_resource(G_broker, G_classname, ctx, rslt, cop, 
                               properties, TID_RES_GROUP, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ResourceGroupCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                    CMPIResult * rslt, CMPIObjectPath * cop, 
                                    CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
ResourceGroupSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                         CMPIResult * rslt, CMPIObjectPath * cop, 
                         CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
ResourceGroupDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
ResourceGroupExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath *ref,
                       char * lang, char * query)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, CMPI_RC_ERR_NOT_SUPPORTED, 
                             "CIM_ERR_NOT_SUPPORTED");
        return rc;
}
                

/**************************************************************
 *   Entry
 *************************************************************/

DeclareInstanceMI(ResourceGroup, HA_ResourceGroupProvider, G_broker);
