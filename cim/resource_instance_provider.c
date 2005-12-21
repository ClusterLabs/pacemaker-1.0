/*
 * resource_instance_provider.c: HA_ClusterResourceInstance provider
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
#include "assoc_utils.h"
#include "cluster_info.h"
#include "cmpi_utils.h"

#define PROVIDER_ID                   "cim-res-inst"
static CMPIBroker * G_broker        = NULL;

static char G_classname         []  = "HA_ResourceInstance"; 
static char G_left              []  = "Resource"; 
static char G_right             []  = "Attributes";
static char G_left_class        []  = "HA_ClusterResource";
static char G_right_class       []  = "HA_InstanceAttributes"; 

DeclareInstanceFunctions(ResourceInstance);
DeclareAssociationFunctions(ResourceInstance);

static CMPIArray *
make_left_enum(CMPIBroker * broker, char * classname, CMPIContext * ctx,
               char * namespace, char * target_name, char * target_role,
               CMPIObjectPath * source_op, CMPIStatus * rc)
{
        return CMNewArray(broker, 0, CMPI_ref, rc);
}


static CMPIArray *
make_right_enum(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                char * namespace, char * target_name, char * target_role,
                CMPIObjectPath * source_op, CMPIStatus * rc)
{
        char * rsc_id, * cr_name;
        CMPIArray * array;
        CMPIString * key_id,  * key_cr_name;
        CMPIObjectPath * target_op;

        /* we need to make a enumeration that contains sub resource
           according to source_op */
        if ( source_op == NULL ) {
                cl_log(LOG_ERR, "group object path can't be NULL");
                return NULL;
        }
        
        /* get keys */
        key_id = CMGetKey(source_op, "Id", rc).value.string;
        if ( key_id == NULL ) {
                cl_log(LOG_ERR, "group id is NULL");
                return NULL;
        }

        key_cr_name = CMGetKey(source_op,"CreationClassName", rc).value.string;
        if ( key_cr_name == NULL ) {
                cl_log(LOG_ERR, "SystemCreationClassName is NULL");
                return NULL;
        }
        
        rsc_id = CMGetCharPtr(key_id);
        cr_name = CMGetCharPtr(key_cr_name);
        
        /* we are ready to make object path for Attributes */
        if (( array = CMNewArray(broker, 1, CMPI_ref, rc)) == NULL ) {
                return NULL;
        }

        target_op = CMNewObjectPath(broker, namespace, target_name, rc);
        if (target_op == NULL ) {
                return NULL;
        }
        
        /* FIXME: not attributes id */
        CMAddKey(target_op, "Id", rsc_id, CMPI_chars);
        CMAddKey(target_op, "SystemName", rsc_id, CMPI_chars);
        CMAddKey(target_op, "SystemCreationClassName", cr_name, CMPI_chars);
        CMAddKey(target_op, "CreationClassName", G_classname, CMPI_chars);
        
        CMSetArrayElementAt(array, 0, &target_op, CMPI_ref);
        return array;
}

static CMPIArray *
this_enum_func(CMPIBroker * broker, char * classname, CMPIContext * ctx,
               char * namespace, char * target_name, char * target_role,
               CMPIObjectPath * source_op, CMPIStatus * rc)
{
        /* if it is resource, enumerate it's instance names */
        if ( strcmp(target_name, G_left_class) == 0 ) {
                CMPIArray * array;
                if ( source_op == NULL ) {
                        array = default_enum_func(broker, classname, 
                                                  ctx, namespace, 
                                                  target_name, target_role,
                                                  source_op, rc);
                } else {
                         array = make_left_enum(broker, classname, 
                                                ctx, namespace, 
                                                target_name, target_role,
                                                  source_op, rc);
                }

                if ( CMIsNullObject(array) ) {
                        cl_log(LOG_ERR, "group array is NULL ");
                        return NULL;
                }
                return array;

        } else if ( strcmp(target_name, G_right_class) == 0 ) {
                return make_right_enum(broker, classname, ctx, namespace, 
                                       target_name, target_role, 
                                       source_op, rc);
        }
        
        return NULL;
}

static int 
res_has_attrs(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * group_op, CMPIObjectPath * res_op,  
              CMPIStatus * rc)
{
        return 1;
}

/**********************************************
 * Instance 
 **********************************************/

static CMPIStatus 
ResourceInstanceCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
ResourceInstanceEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                  CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                res_has_attrs, this_enum_func, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}

static CMPIStatus 
ResourceInstanceEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop, 
                              char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                res_has_attrs, this_enum_func, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus 
ResourceInstanceGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
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
ResourceInstanceCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop, 
                               CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
ResourceInstanceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
ResourceInstanceDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
ResourceInstanceExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
                          CMPIResult * rslt, CMPIObjectPath * cop,
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
ResourceInstanceAssociationCleanup(CMPIAssociationMI * mi, 
                                   CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
ResourceInstanceAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * assoc_class, const char * result_class,
                            const char * role, const char * result_role, 
                            char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, res_has_attrs, this_enum_func, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus
ResourceInstanceAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                CMPIResult * rslt, CMPIObjectPath * cop, 
                                const char * assoc_class, const char * result_class,
                                const char * role, const char * result_role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, res_has_attrs, this_enum_func, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}

static CMPIStatus
ResourceInstanceReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop, 
                           const char * result_class, const char * role, 
                           char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID); 
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, res_has_attrs, 
                                this_enum_func, 1, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

static CMPIStatus
ResourceInstanceReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop,
                               const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID); 
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, res_has_attrs, 
                                this_enum_func, 0, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                

/**************************************************************
 *      MI stub
 *************************************************************/

DeclareInstanceMI(ResourceInstance, HA_ResourceInstanceProvider, G_broker);
DeclareAssociationMI(ResourceInstance, HA_ResourceInstanceProvider, G_broker);
