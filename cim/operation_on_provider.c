/*
 * operation_on_provider.c: HA_OperationOn provider
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

static const char * PROVIDER_ID     = "cim-op-on";
static CMPIBroker * G_broker        = NULL;
static char G_classname         []  = "HA_OperationOnResource"; 
static char G_left              []  = "Resource"; 
static char G_right             []  = "Operation";
static char G_left_class        []  = "HA_PrimitiveResource";
static char G_right_class       []  = "HA_Operation"; 

static CMPIArray *
make_enum_for_right(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                    char * namespace, char * target_name, char * target_role,
                    CMPIObjectPath * source_op, CMPIStatus * rc);
static CMPIArray *
this_enum_func(CMPIBroker * broker, char * classname, CMPIContext * ctx,
               char * namespace, char * target_name, char * target_role,
               CMPIObjectPath * source_op, CMPIStatus * rc);
static int
res_has_op(CMPIBroker * broker, char * classname, CMPIContext * ctx,
           CMPIObjectPath * group_op, CMPIObjectPath * res_op, CMPIStatus * rc);

DeclareInstanceFunctions(OperationOn);
DeclareAssociationFunctions(OperationOn);

static CMPIArray *
make_enum_for_right(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                    char * namespace, char * target_name, char * target_role,
                    CMPIObjectPath * source_op, CMPIStatus * rc)
{
        CMPIString * s;
        char * rsc_id, * rsc_cr_name;
        struct ci_table * operations;
        CMPIArray * array;
        uint32_t i, len;

        if ((s = CMGetKey(source_op, "Id", rc).value.string) == NULL ) {
                return NULL;
        }
        if ((rsc_id = CMGetCharPtr(s)) == NULL ) return NULL;
        if (( s = CMGetKey(source_op, "CreationClassName", 
                           rc).value.string) == NULL ) {
                return NULL;
        }
        if ((rsc_cr_name = CMGetCharPtr(s)) == NULL ) return NULL;

        if ((operations = ci_get_inst_operations(rsc_id)) == NULL ) {
                return NULL;
        }
        len =  operations->get_data_size(operations);
        
        cl_log ( LOG_INFO, "operations has %d operation(s)", len);

        if ((array = CMNewArray(broker, len, CMPI_ref, rc)) == NULL ) {
                operations->free(operations);
                return NULL;
        }
        for (i = 0; i < len; i ++ ) {
                CMPIObjectPath * target_op;
                struct ci_table * operation;
                char * operation_id;
                
                target_op = CMNewObjectPath(broker, namespace, G_right_class, rc);
                if ( CMIsNullObject(target_op)) continue;

                operation = operations->get_data_at(operations, i).value.table;
                if ( operation == NULL )  continue;
                operation_id = operation->get_data(operation, "id").value.string;
                if (operation_id == NULL ) continue;

                CMAddKey(target_op, "Id", operation_id, CMPI_chars);
                CMAddKey(target_op, "SystemName", rsc_id, CMPI_chars);
                CMAddKey(target_op, "SystemCreationClassName", 
                         rsc_cr_name, CMPI_chars);
                CMAddKey(target_op, "CreationClassName", G_classname, CMPI_chars);

                /* add to array */
                CMSetArrayElementAt(array, i, &target_op, CMPI_ref);
        }


        operations->free(operations);
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
                if ( source_op ){
                        return CMNewArray(broker, 0, CMPI_ref, rc);
                }
                        
                array = default_enum_func(broker, classname, ctx, namespace, 
                                          target_name, target_role,
                                          source_op, rc);

                if ( CMIsNullObject(array) ) {
                        cl_log(LOG_ERR, "group array is NULL ");
                        return NULL;
                }
                return array;

        } else if ( strcmp(target_name, G_right_class) == 0 ) {
                return make_enum_for_right(broker, classname, ctx, namespace, 
                                           target_name, target_role, 
                                           source_op, rc);
        }
        
        return NULL;
}

static int 
res_has_op(CMPIBroker * broker, char * classname, CMPIContext * ctx,
           CMPIObjectPath * group_op, CMPIObjectPath * res_op, CMPIStatus * rc)
{
        return 1;
}

/**********************************************
 * Instance 
 **********************************************/

static CMPIStatus 
OperationOnCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus 
OperationOnEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx, 
                                  CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                res_has_op, this_enum_func, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}

static CMPIStatus 
OperationOnEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, 
                              CMPIResult * rslt, CMPIObjectPath * cop, 
                              char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                res_has_op, this_enum_func, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus 
OperationOnGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
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
OperationOnCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop, 
                               CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
OperationOnSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
OperationOnDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
OperationOnExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
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
OperationOnAssociationCleanup(CMPIAssociationMI * mi, 
                                   CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
OperationOnAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
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
                                result_role, res_has_op, this_enum_func, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}


static CMPIStatus
OperationOnAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                                CMPIResult * rslt, CMPIObjectPath * cop, 
                                const char * assoc_class, const char * result_class,
                                const char * role, const char * result_role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, res_has_op, this_enum_func, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}

static CMPIStatus
OperationOnReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop, 
                           const char * result_class, const char * role, 
                           char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID); 
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, res_has_op, 
                                this_enum_func, 1, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

static CMPIStatus
OperationOnReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                               CMPIResult * rslt, CMPIObjectPath * cop,
                               const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID); 
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, res_has_op, 
                                this_enum_func, 0, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}                

/**************************************************************
 *      MI stub
 *************************************************************/

DeclareInstanceMI(OperationOn, HA_OperationOnProvider, G_broker);
DeclareAssociationMI(OperationOn, HA_OperationOnProvider, G_broker);
