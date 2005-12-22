/*
 * sub_resource_provider.c: HA_SubResource provider
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
#include <hb_api.h>
#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include "assoc_utils.h"
#include "cluster_info.h"
#include "cmpi_utils.h"

static const char * PROVIDER_ID  = "cim-sub-res";
static CMPIBroker * G_broker     = NULL;
static char G_classname       [] = "HA_SubResource"; 
static char G_left            [] = "Antecedent";
static char G_right           [] = "Dependent"; 
static char G_left_class      [] = "HA_ClusterResource"; 
static char G_right_class     [] = "HA_ClusterResource";

static char cr_name_primitive [] = "HA_PrimitiveResource";
static char cr_name_group     [] = "HA_ResourceGroup";
static char cr_name_clone     [] = "HA_ResourceClone";
static char cr_name_master    [] = "HA_MasterSlaveResource";
static char cr_name_unknown   [] = "Unknown";


static CMPIArray *
enum_func_for_right(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                    char * namespace, char * target_name, char * target_role,
                    CMPIObjectPath * source_op, CMPIStatus * rc);

static CMPIArray *
group_enum_func(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                char * namespace, char * target_name, char * target_role,
                CMPIObjectPath * source_op, CMPIStatus * rc);

static int
group_contain(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * group_op, CMPIObjectPath * res_op,
              CMPIStatus * rc);

DeclareInstanceFunctions(SubResource);
DeclareAssociationFunctions(SubResource);


static CMPIArray *
enum_func_for_right(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                    char * namespace, char * target_name, char * target_role, 
                    CMPIObjectPath * source_op, CMPIStatus * rc)
{
        char * group_id, * sys_name, * sys_cr_name, * cr_name;
        int i;
        CMPIArray * array;
        GPtrArray * sub_name_tbl;
        CMPIString * key_id, * key_sys_name, * key_sys_cr, * key_cr_name;

        /* we need to make a enumeration that contains sub resource
           according to source_op */
        if ( source_op == NULL ) {
                return NULL;
        }
        
        /* get keys */
        if ((key_id = CMGetKey(source_op, "Id", rc).value.string) == NULL ) {
                return NULL;
        }
        
        if ((key_cr_name = CMGetKey(source_op, "CreationClassName", 
                                    rc).value.string) == NULL ){
                return NULL;
        }

        if (( key_sys_name = CMGetKey(source_op, "SystemName", 
                                      rc).value.string) == NULL ) {
                return NULL;
        }
        if (( key_sys_cr = CMGetKey(source_op, "SystemCreationClassName", 
                                    rc).value.string) == NULL ) {
                return NULL;
        }

        group_id = CMGetCharPtr(key_id);
        sys_name = CMGetCharPtr(key_sys_name);
        sys_cr_name = CMGetCharPtr(key_sys_cr);
        cr_name = CMGetCharPtr(key_cr_name);

        cl_log(LOG_INFO, "group_id, sys_name, sys_cr_name = %s, %s, %s",
               group_id, sys_name, sys_cr_name);
        
        if ( strcmp(cr_name, cr_name_primitive) == 0 ) {
                return CMNewArray(broker, 0, CMPI_ref, rc);
        }
        
        if ((sub_name_tbl = ci_get_sub_resource_name_table(group_id))== NULL) {
                return NULL;
        }
        
        /* create a array to hold the object path */
        array = CMNewArray(broker, sub_name_tbl->len, CMPI_ref, rc);
        
        /* for each sub primitive resource */
        for ( i = 0; i < sub_name_tbl->len; i ++ ) {
                uint32_t rsc_type;
                CMPIObjectPath * target_op;
                char * sub_id;
                
                sub_id = (char *) g_ptr_array_index(sub_name_tbl, i);
                if ( sub_id == NULL ) {
                        continue;
                }
                rsc_type = ci_get_resource_type(sub_id);
                if (rsc_type == TID_RES_PRIMITIVE ) {
                        cr_name = cr_name_primitive;
                } else if ( rsc_type == TID_RES_GROUP ) {
                        cr_name = cr_name_group;
                } else if ( rsc_type == TID_RES_GROUP ) {
                        cr_name = cr_name_clone;
                } else if ( rsc_type == TID_RES_MASTER ) {
                        cr_name = cr_name_master;
                } else {
                        cr_name = cr_name_unknown;
                }
               
                /* create object path */
                target_op = CMNewObjectPath(broker, namespace, cr_name, rc);
                if ( CMIsNullObject(target_op) ) {
                        continue;
                }
                /* set keys */
                CMAddKey(target_op, "Id", sub_id, CMPI_chars);
                CMAddKey(target_op, "SystemName", sys_name, CMPI_chars);
                CMAddKey(target_op, "SystemCreationClassName", 
                         sys_cr_name, CMPI_chars);
                CMAddKey(target_op, "CreationClassName", cr_name, CMPI_chars);
                
                /* add to array */
                CMSetArrayElementAt(array, i, &target_op, CMPI_ref);
        }
        
        ci_free_ptr_array(sub_name_tbl, ci_safe_free);
        return array;
        
}

/*
static CMPIArray * left_array_cache = NULL;
*/

static CMPIArray *
group_enum_func(CMPIBroker * broker, char * classname, CMPIContext * ctx,
                char * namespace, char * target_name, char * target_role,
                CMPIObjectPath * source_op, CMPIStatus * rc)
{
        init_logger(PROVIDER_ID);

        if ( target_name == NULL ) {
                cl_log(LOG_ERR, "target name can not be NULL");
                return NULL;
        }

        if ( strcmp(target_role, G_left) == 0 ) {
                /*
                if (!CMIsNullObject(left_array_cache) ) return left_array_cache;
                */
                CMPIArray * left_array_cache;
                left_array_cache = default_enum_func(broker, classname, ctx, namespace, 
                                          target_name, target_role, source_op, rc);

                if ( CMIsNullObject(left_array_cache) ) {
                        cl_log(LOG_ERR, "group array is NULL ");
                        return NULL;
                }
                return left_array_cache;

        } else if ( strcmp(target_role, G_right) == 0 ) {

                return enum_func_for_right(broker, classname,
                                           ctx, namespace, target_name, target_role, 
                                           source_op, rc);
        }
        
        /* else */
        return NULL;
}

static int 
group_contain(CMPIBroker * broker, char * classname, CMPIContext * ctx,
              CMPIObjectPath * group_op, CMPIObjectPath * res_op,  
              CMPIStatus * rc)
{
        CMPIString * cmpi_rsc_id = NULL;
        CMPIString * cmpi_group_id = NULL;
        char * rsc_id, * group_id;
        GPtrArray * sub_name_tbl;
        int i;

        /* resource name */
        cmpi_rsc_id = CMGetKey(res_op, "Id", rc).value.string;
        if ( CMIsNullObject(cmpi_rsc_id) ) {
                cl_log(LOG_INFO, "%s: resource_name =  NULL", __FUNCTION__);
                return 0;
        }

        /* group id */
        cmpi_group_id = CMGetKey(group_op, "Id", rc).value.string;
        if ( CMIsNullObject( cmpi_group_id ) ) {
                cl_log(LOG_INFO, "%s: invalid GroupId", __FUNCTION__);
                return 0;
        }
        rsc_id = CMGetCharPtr(cmpi_rsc_id);
        group_id = CMGetCharPtr(cmpi_group_id);

        /* get this group */

        if ((sub_name_tbl = ci_get_sub_resource_name_table(group_id)) == NULL ) {
                return 0;
        }
        
        /* look for the rsc */
        for ( i = 0; i < sub_name_tbl->len; i++ ) {
                char * sub_rsc_id;
                sub_rsc_id = (char *)  g_ptr_array_index(sub_name_tbl, i);
                if ( rsc_id == NULL ) {
                        continue;
                }
                if ( strcmp(sub_rsc_id, rsc_id ) == 0 ) {
                        /* found */
                        return 1;
                }
        }
        
        ci_free_ptr_array(sub_name_tbl, ci_safe_free);
        return 0;

}

/**********************************************
 * Instance 
 **********************************************/
static CMPIStatus 
SubResourceCleanup(CMPIInstanceMI * mi, CMPIContext * ctx)
{
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
SubResourceEnumInstanceNames(CMPIInstanceMI * mi, CMPIContext* ctx, 
                             CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class, 
                                group_contain, group_enum_func, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
SubResourceEnumInstances(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                         CMPIObjectPath * cop, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_enum_insts(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                group_contain, group_enum_func, 1, 
                                &rc) != HA_OK ){
                return rc;
        }

        CMReturn(CMPI_RC_OK);
}


static CMPIStatus 
SubResourceGetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt,
                       CMPIObjectPath * cop, char ** properties)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        if (cm_assoc_get_inst(G_broker, G_classname, ctx, rslt, cop, 
                              G_left, G_right, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
       
}

static CMPIStatus 
SubResourceCreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
                          CMPIObjectPath * cop, CMPIInstance * ci)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}


static CMPIStatus 
SubResourceSetInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                       CMPIResult * rslt, CMPIObjectPath * cop, 
                       CMPIInstance * ci, char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;

}


static CMPIStatus 
SubResourceDeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        CMSetStatusWithChars(G_broker, &rc, 
                        CMPI_RC_ERR_NOT_SUPPORTED, "CIM_ERR_NOT_SUPPORTED");
        return rc;
}

static CMPIStatus 
SubResourceExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx, 
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
SubResourceAssociationCleanup(CMPIAssociationMI * mi, CMPIContext * ctx)
{

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
SubResourceAssociators(CMPIAssociationMI * mi, CMPIContext * ctx, 
                        CMPIResult * rslt,  CMPIObjectPath * cop, 
                        const char * assoc_class, const char * result_class,
                        const char * role, const char * result_role, 
                        char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, group_contain, group_enum_func, 1, 
                                &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);
}


static CMPIStatus
SubResourceAssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                            CMPIResult * rslt, CMPIObjectPath * cop, 
                            const char * assoc_class, const char * result_class,
                            const char * role, const char * result_role)
{

        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if (cm_enum_associators(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                assoc_class, result_class, role, 
                                result_role, group_contain, group_enum_func, 0, 
                                &rc) != HA_OK ) {
                return rc;
        }      

        CMReturn(CMPI_RC_OK);
}

static CMPIStatus
SubResourceReferences(CMPIAssociationMI * mi, CMPIContext * ctx, 
                      CMPIResult * rslt, CMPIObjectPath * cop, 
                      const char * result_class, const char * role, 
                      char ** properties)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID); 
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, group_contain, 
                                group_enum_func, 1, &rc) != HA_OK ) {
                return rc;
        }

        CMReturn(CMPI_RC_OK);

}

static CMPIStatus
SubResourceReferenceNames(CMPIAssociationMI * mi, CMPIContext * ctx, 
                           CMPIResult * rslt, CMPIObjectPath * cop,
                           const char * result_class, const char * role)
{
        CMPIStatus rc = {CMPI_RC_OK, NULL};
        init_logger(PROVIDER_ID);
        if ( cm_enum_references(G_broker, G_classname, ctx, rslt, cop, 
                                G_left, G_right, G_left_class, G_right_class,
                                result_class, role, group_contain, 
                                group_enum_func, 0, &rc) != HA_OK ) {
                return rc;
        }
        CMReturn(CMPI_RC_OK);

}                

/***************************************
 * installed MIs
 **************************************/

DeclareInstanceMI(SubResource, HA_SubResourceProvider, G_broker);
DeclareAssociationMI(SubResource, HA_SubResourceProvider, G_broker);
