/*
 * assoc_utils.c: Utils for Association CMPI Providers
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
#include <stdio.h>
#include <hb_api.h>
#include <heartbeat.h>
#include <clplumbing/cl_log.h>
#include <glib.h>
#include "cmpi_utils.h"
#include "assoc_utils.h"
#include "cluster_info.h"

static int class_is_a(const char * source_class_name, char * class_name,
                      CMPIBroker * broker, CMPIObjectPath * cop);

static CMPIInstance *
assoc_make_inst(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
                char * left, char * right, 
                CMPIObjectPath * lop, CMPIObjectPath * rop, CMPIStatus * rc);



/****************************************
 * new association functions
 ****************************************/
static int 
class_is_a(const char * source_class_name, char * class_name,
           CMPIBroker * broker, CMPIObjectPath * cop)
{
        CMPIStatus rc;
        int is_a = 0;

        if ( strcmp (source_class_name, class_name) == 0){
                is_a = 1;
        }else if (CMClassPathIsA(broker, cop, class_name, &rc)){
                is_a = 1;
        } else {
                is_a = 0;
        }        

        return is_a;
}

/* make a instance */

static CMPIInstance *
assoc_make_inst(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
                char * left, char * right, 
                CMPIObjectPath * lop, CMPIObjectPath * rop, CMPIStatus * rc)
{
        CMPIInstance * inst = NULL;

        if ( (inst = CMNewInstance(broker, op, rc) ) == NULL ) {
                return NULL;
        }

        /* set properties */
        CMSetProperty(inst, left, (CMPIValue *)&lop, CMPI_ref);
        CMSetProperty(inst, right, (CMPIValue *)&rop, CMPI_ref);
        /*
        CMSetProperty(inst, "Caption", classname, CMPI_chars);        
        */
        return inst;
}

/* enumerate instance names */
CMPIArray *
default_enum_func (CMPIBroker * broker, char * classname, CMPIContext * ctx,
                   char * namespace, char * target_name, char * target_role,
                   CMPIObjectPath * source_op, /*void * user_data,*/
                   CMPIStatus * rc)
{
        /* Max length of this Array, remember: CMGetArraySize return this value */
        int max_len = 256;
        CMPIEnumeration * en;
        CMPIObjectPath * op = NULL;
        CMPIArray * array;
        int i = 0;

        /* create paths for left */
        op = CMNewObjectPath(broker, namespace, target_name, rc);
        RETURN_FAIL_IFNULL_OBJ(op, "target object path");

        en = CBEnumInstanceNames(broker, ctx, op, rc);
        if ( CMIsNullObject(en) ){
                cl_log(LOG_ERR, "default_enum_func: en is NULL");
                return NULL;
        }
        
        /* CMToArray does not seem to work */
        if ((array = CMNewArray(broker, max_len, CMPI_ref, rc)) == NULL ) {
                cl_log(LOG_ERR, "default_enum_func: create array failed");
                return NULL;
        }
        
        i = 0;
        while(CMHasNext(en, rc)) {
                CMPIObjectPath * top = CMGetNext(en, rc).value.ref;
                if ( CMIsNullObject(top) ) continue;
                if ( i == max_len ) break;

                CMSetArrayElementAt(array, i, &top, CMPI_ref);
                i ++;
        }

        cl_log (LOG_INFO, "default_enum_func: array length is %d", i);
        CMRelease(op);
        return array;
}

/* get a association instance */ 
int
cm_assoc_get_inst(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                  CMPIResult * rslt, CMPIObjectPath * cop, 
                  char * left, char * right, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIObjectPath * left_op, * right_op;
        CMPIInstance * ci = NULL;
        char * nsp = NULL;

        left_op = CMGetKey(cop, left, rc).value.ref;
        right_op = CMGetKey(cop, right, rc).value.ref;

        if ( CMIsNullObject(left_op) ) {
                cl_log(LOG_ERR, "assoc_get_inst couldn't get key %s", left);
                return HA_FAIL;
        }

        if ( CMIsNullObject(right_op) ) {
                cl_log(LOG_ERR, "assoc_get_inst couldn't get key %s", right);
                return HA_FAIL;
        }

        nsp = CMGetCharPtr(CMGetNameSpace(cop, rc));

        CMSetNameSpace(left_op, nsp);
        CMSetNameSpace(right_op, nsp);

        /* make sure these two instance do exist */
        if ( !CBGetInstance(broker, ctx, left_op, NULL, rc)  ||
             !CBGetInstance(broker, ctx, right_op, NULL, rc) ){
                return HA_FAIL;
        }

        op = CMNewObjectPath(broker, nsp, classname, rc);
        if ( CMIsNullObject(op) ) {
                cl_log(LOG_ERR, 
                        "%s: failed to create object path", __FUNCTION__);
                return HA_FAIL;
        }
        
        ci = assoc_make_inst(broker, classname, op, left, right,
                             left_op, right_op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, 
                        "%s: failed to create instance", __FUNCTION__);
                return HA_FAIL;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
        CMRelease(op);
        return HA_OK;
}


/* enum a path's associators(counterpart) */
int
cm_enum_associators(CMPIBroker * broker, char * classname, 
                    CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                    char * left, char * right, char * lclass, char * rclass,
                    const char * assoc_class, const char * result_class, 
                    const char * role, const char * result_role, 
                    assoc_func_t func, assoc_enum_func_t enum_func,
                    int need_inst, CMPIStatus * rc)
{

        char * nsp = NULL;
        CMPIArray * array = NULL;
        char * source_class = NULL, * target_class = NULL;
        char * target_role;
        int left_is_source = 0;
        int i, array_size;
        CMPIInstance * inst = NULL;

        cl_log(LOG_INFO, 
                "%s: asscClass, resultClass, role, resultRole = %s, %s, %s, %s",
                __FUNCTION__,
                assoc_class, result_class, role, result_role);

        nsp = CMGetCharPtr(CMGetNameSpace(cop, rc));
        if ( assoc_class ) {
                CMPIObjectPath * op = NULL;
                op = CMNewObjectPath(broker, nsp, classname, rc);
                if ( CMIsNullObject(op) ){
                        return HA_FAIL;
                }               
                if ( !CMClassPathIsA(broker, op, assoc_class, rc) ) {
                        CMReturnDone(rslt);
                        CMRelease(op);
                        return HA_OK;
                }
                CMRelease(op);
        }

        /* to verify the cop is valid */
        if ( CBGetInstance( broker, ctx, cop, NULL, rc) == NULL ){
                cl_log(LOG_ERR, "%s: failed to get instance", __FUNCTION__);
                return HA_FAIL;
        }

        source_class = CMGetCharPtr(CMGetClassName(cop, rc));
        
        if ( class_is_a(source_class, lclass, broker, cop) ){
                /* if left is source, then right is target */
                target_class = rclass;
                target_role = right;
                left_is_source = 1;
        } else if ( class_is_a(source_class, rclass, broker, cop)) {
                /* if right is source, then left is target */
                target_class = lclass;
                target_role = left;
                left_is_source = 0;
        } else {
                cl_log(LOG_ERR, "assoc: neither lclass nor rclass");
                return HA_FAIL;
        }

        if ( enum_func ) {
                array = enum_func(broker, classname, ctx, nsp, 
                                  target_class, target_role, cop, rc);
        } else {
                array = default_enum_func(broker, classname, ctx, nsp, 
                                          target_class, target_role, cop, rc);
        }
        RETURN_FAIL_IFNULL_OBJ(array, "target enumeration");
        
        array_size = CMGetArrayCount(array, rc);
        for ( i = 0; i < array_size; i ++){  
                CMPIObjectPath * target_path;
                CMPIObjectPath * left_op = NULL;
                CMPIObjectPath * right_op = NULL;

                target_path = CMGetArrayElementAt(array, i, rc).value.ref;
                RETURN_FAIL_IFNULL_OBJ(target_path, "target object path");

                /* if left is source, then left op is cop */
                left_op = left_is_source? cop : target_path;
                right_op = left_is_source? target_path : cop;

                /* now we have an source object path, and an target object path,
                   we can tell if these 2 paths are related */

                if ( func && ! func(broker, classname, ctx, left_op, 
                                    right_op, rc) ){
                        continue;
                }
                
                if ( !need_inst ) {
                        CMReturnObjectPath(rslt, target_path);
                        continue;
                } else {
                        inst = CBGetInstance(broker, ctx, target_path, NULL, rc); 
                        RETURN_FAIL_IFNULL_OBJ(inst, "target instance");
                        CMReturnInstance(rslt, inst);
                }
        } /* whlie */
                 
       
        CMReturnDone(rslt);
        CMRelease(array);
        return HA_OK;
}

/* enum a path's references */
int
cm_enum_references(CMPIBroker * broker, char * classname,
                   CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                   char * left, char * right, char * lclass, char * rclass,
                   const char * result_class, const char * role,
                   assoc_func_t func, assoc_enum_func_t enum_func, 
                   int need_inst, CMPIStatus * rc)
{
        char * nsp = NULL;
        char * source_classname = NULL, * target_classname = NULL;
        char * target_role;
        int left_is_source = 0;
        CMPIArray * array;
        int i, array_size;

        nsp = CMGetCharPtr(CMGetNameSpace(cop, rc));
        if ( result_class ) {
                CMPIObjectPath * op = NULL;
                op = CMNewObjectPath(broker, nsp, classname, rc);
                RETURN_FAIL_IFNULL_OBJ(op, "op");
                if ( !CMClassPathIsA(broker, op, result_class, rc) ){
                        CMReturnDone(rslt);
                        CMRelease(op);
                        return HA_OK;
                }
                CMRelease(op);
        }

        RETURN_FAIL_IFNULL_OBJ( CBGetInstance( broker, ctx, cop, NULL, rc), 
                                "get instance");
        /* get source class */
        source_classname = CMGetCharPtr(CMGetClassName(cop, rc));
        if ( class_is_a(source_classname, lclass, broker, cop) ){
                target_classname = rclass;
                target_role = right;
                left_is_source = 1;
        } else if ( class_is_a(source_classname, rclass, broker, cop)) {
                target_classname = lclass;
                target_role = left;
                left_is_source = 0;
        } else {
                cl_log(LOG_ERR, "assoc: neither lclass nor rclass");
                return HA_FAIL;
        }

        cl_log(LOG_INFO, "source_classname is %s", source_classname);

        /* enumerate taget object paths */

        if ( enum_func ) {
                array = enum_func(broker, classname, ctx, nsp, 
                                  target_classname, target_role, cop, rc);
        } else {
                array = default_enum_func(broker, classname, ctx, nsp, 
                                          target_classname, target_role, cop, rc);

        }
        RETURN_FAIL_IFNULL_OBJ(array, "target instance names");
        array_size = CMGetArrayCount(array, rc);

        for ( i = 0; i < array_size; i ++){
                CMPIObjectPath * top = NULL;
                CMPIObjectPath * target_path;
                CMPIObjectPath * left_op = NULL;
                CMPIObjectPath * right_op = NULL;

                /* next object path */
                target_path = CMGetArrayElementAt(array,i, rc).value.ref;
                RETURN_FAIL_IFNULL_OBJ(target_path, "target path");

                
                /* if left is source, then left op is cop */
                left_op = left_is_source? cop : target_path;
                right_op = left_is_source? target_path : cop;

                
                if ( func && ! func(broker, classname, ctx, 
                                    left_op, right_op, rc) ){
                        continue;
                }
                /* create a ob for the association class */
                top = CMNewObjectPath(broker, nsp, classname, rc);
                RETURN_FAIL_IFNULL_OBJ(top, "top");

                if ( !need_inst ) { 
                        /* add keys to the association path */
                        CMAddKey(top, left, &left_op, CMPI_ref);
                        CMAddKey(top, right, &right_op, CMPI_ref);
                        CMReturnObjectPath(rslt, top);
                } else {
                        CMPIInstance * inst = NULL;
                        inst = assoc_make_inst(broker, classname, top, left, 
                                               right, left_op, right_op, rc);
                        RETURN_FAIL_IFNULL_OBJ(inst, "association instance");
                        CMReturnInstance(rslt, inst);
                }
                CMRelease(top);
        } /* while */
                 
        CMReturnDone(rslt);
        
        return HA_OK;
}


/* enum association's instances */
int
cm_assoc_enum_insts(CMPIBroker * broker, char * classname,
                    CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                    char * left, char * right, char * lclass, char * rclass,
                    assoc_func_t func, assoc_enum_func_t enum_func,
                    int need_inst, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIArray * left_array = NULL, * right_array = NULL;
        int i, left_array_size;
        char * namespace = NULL;

        namespace = CMGetCharPtr(CMGetNameSpace(cop, rc));


        /* get the instance anems enumeration of the left class */
        if ( enum_func ) {
                left_array = enum_func(broker, classname, ctx, namespace, 
                                       lclass, left, NULL, rc);
        } else {
                left_array = default_enum_func(broker, classname, ctx, namespace, 
                                               lclass, left, NULL, rc);
                /* avoid invoking default_enum_func left_array_size times */
                right_array = default_enum_func(broker, classname, ctx, 
                                                namespace, rclass, right, NULL, rc); 
        }

        RETURN_FAIL_IFNULL_OBJ(left_array, "left_array");

        /* create path for this association */
        op = CMNewObjectPath(broker, namespace, classname, rc);
        RETURN_FAIL_IFNULL_OBJ(op, "association object path");

        left_array_size = CMGetArrayCount(left_array, rc);
        cl_log(LOG_INFO, "left_array_size for %s is %d", lclass, left_array_size);

        for ( i = 0; i < left_array_size; i ++) {
                CMPIObjectPath * left_op = NULL;
                int j, right_array_size;

                left_op = CMGetArrayElementAt(left_array, i, rc).value.ref;
                /*
                RETURN_FAIL_IFNULL_OBJ(left_op, "left object path");
                */
                if ( CMIsNullObject(left_op ) ) {
                        break;
                }

                /* now it's turn to get the right enumeration */
                if ( enum_func ) {
                        /* if we have enum_func, use enum_func to get enumeration,
                         the result may be various for different left_op, 
                         so we must call it everytime */
                        right_array = enum_func(broker, classname, ctx, namespace, 
                                                rclass, right, left_op, rc); 
                } else {
                        /* we have got right_array, need not retrive it again */
                }

                RETURN_FAIL_IFNULL_OBJ(right_array, "right_array");
                
                right_array_size = CMGetArrayCount(right_array, rc);
                cl_log(LOG_INFO, "array size for %s is %d", rclass, right_array_size);

                for (j = 0; j < right_array_size; j ++){
                        CMPIObjectPath * right_op = NULL;
                        right_op = CMGetArrayElementAt(right_array, j, rc).value.ref;
                        /*
                        RETURN_FAIL_IFNULL_OBJ(right_op, "the right object path");
                        */

                        if ( CMIsNullObject(right_op) ) {
                                break;
                        }

                        cl_log(LOG_INFO, "Got object path for %s", rclass);

                        /* associated ? */
                        if ( func && !func(broker, classname, ctx, 
                                           left_op, right_op, rc) ) {
                                continue;
                        }

                        if ( ! need_inst ) {
                                CMAddKey(op, left, &left_op, CMPI_ref);
                                CMAddKey(op, right, &right_op, CMPI_ref); 
                                CMReturnObjectPath(rslt, op);
                        
                        } else {    /* instances */
                                CMPIInstance * inst = NULL;
                                inst = assoc_make_inst(broker, classname, op, left, 
                                                       right, left_op, right_op, rc);
                        
                                RETURN_FAIL_IFNULL_OBJ(inst, "the association instance");
                                CMReturnInstance(rslt, inst);

                        }
               } /* while */ 
        } /* while */


        CMReturnDone(rslt); 
        CMRelease(left_array);
        CMRelease(right_array);
        return HA_OK;
}
