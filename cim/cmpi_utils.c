/*
 * assoc_utils.c: Utilities for CMPI Providers
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
#include <stdio.h>
#include <hb_api.h>
#include <ha_msg.h>
#include <heartbeat.h>
#include <clplumbing/cl_log.h>
#include <glib.h>
#include "cmpi_utils.h"
#include "cluster_info.h"

static inline int ClassIsA(const char * source_class, char * name,
               	CMPIBroker * broker, CMPIObjectPath * cop);

static CMPIInstance * MakeInstance(CMPIBroker * broker, char * classname,
		CMPIObjectPath * op, char * left, char * right, 
		CMPIObjectPath * lop, CMPIObjectPath * rop, CMPIStatus * rc);

int
cmpi_msg2inst(CMPIBroker * broker, CMPIInstance * inst, int mapid,
		struct ha_msg *msg, CMPIStatus * rc)
{
	int i = 0;
	const struct map_t *map = NULL;
	
	if ( (map = cim_query_map(mapid)) == NULL ) {
		cl_log(LOG_ERR, "%s: map is NULL.", __FUNCTION__);
		return HA_FAIL;
	}

	for ( i=0; i < map->len; i++) {
		if(map->entry[i].type ==  CMPI_chars) {
			const char *value;
			value = cl_get_string(msg, map->entry[i].key);
			if ( value == NULL ) {
				cl_log(LOG_WARNING, "%s: key %s not found.",
					__FUNCTION__, map->entry[i].key);
				continue;
			}
			cim_debug2(LOG_INFO, "%s: got %s:%s [CMPI_chars]",
					map->entry[i].key, value, __FUNCTION__);

			CMSetProperty(inst,map->entry[i].name,
					cim_strdup(value),CMPI_chars);
		} else if ( map->entry[i].type ==  CMPI_charsA ) {
			/* set Array */
			CMPIArray * array=NULL;
			int j = 0, len = 0;

			len = cl_msg_list_length(msg, map->entry[i].key);
			if (len <= 0) {
				continue;
			}
			array = CMNewArray(broker, len, CMPI_chars, rc);	
			if ( array == NULL ) {
				DEBUG_LEAVE();
				return HA_FAIL;
			}
			for ( j = 0; j < len; j++ ) {
				char *value = NULL; 
				value = (char*)cl_msg_list_nth_data(msg, 
							map->entry[i].key, j);
				if ( value == NULL ) {
					continue;
				}	
				CMSetArrayElementAt(array, j, &value, CMPI_chars);
			}
			
			cim_debug2(LOG_INFO, "%s: got %s [CMPI_charsA]", 
					__FUNCTION__, map->entry[i].key);

			CMSetProperty(inst, map->entry[i].name, &array, CMPI_charsA);
		} else if (map->entry[i].type == CMPI_uint32){
			/* set CMPI_uint32 */
		}
	}
	return HA_OK;
}


int 
cmpi_inst2msg(CMPIInstance *inst, int mapid, struct ha_msg *msg, CMPIStatus *rc)
{
        int i = 0;                                                      	
	const struct map_t *map = cim_query_map(mapid);
	CMPIData data;
	
	DEBUG_ENTER();
	if ( map == NULL ) {
		return HA_FAIL;
	}

        for( i =0; i < map->len; i++){                                       	
		if ( strncmp(map->entry[i].key , "", MAXLEN) == 0 ) {
			continue;
		}
                data = CMGetProperty(inst, map->entry[i].name, rc); 
                if (rc->rc != CMPI_RC_OK){                                	
                        cl_log(LOG_WARNING, "Property %s missing.", 
				map->entry[i].name);
			continue;
		}

		if ( data.type == CMPI_string) {
			char * value = CMGetCharPtr(data.value.string);
			cl_msg_modstring(msg, map->entry[i].key, value);

		} else if (data.type == CMPI_stringA){
			CMPIArray * array = data.value.array;
			int j, len = CMGetArrayCount(array, rc);
			cl_msg_remove(msg, map->entry[i].key);
			for (j=0; j<len; j++) {
				char * value;
				data = CMGetArrayElementAt(array, j, rc);
				value = CMGetCharPtr(data.value.string);
				cl_msg_list_add_string(msg, 
					map->entry[i].key, value); 
			}	
		} else if (data.type == CMPI_uint32){
			cl_log(LOG_ERR, "%s: Not support.", __FUNCTION__);	
		}
        }

	return HA_OK;
}


/****************************************
 * new association functions
 ****************************************/

/* is 'name' a 'source_class'? */
static inline int 
ClassIsA(const char * source_class, char * classname, 
		CMPIBroker * broker, CMPIObjectPath * cop)
{
        CMPIStatus rc;
        int is_a = 0;
        if ( strcmp (source_class, classname) == 0){
                is_a = 1;
        }else if (CMClassPathIsA(broker, cop, classname, &rc)){
                is_a = 1;
        } 
        return is_a;
}

/* make a instance */
static CMPIInstance *
MakeInstance(CMPIBroker * broker, char * classname, CMPIObjectPath * op, 
		char * left, char * right, CMPIObjectPath * lop, 
		CMPIObjectPath * rop, CMPIStatus * rc)
{
        CMPIInstance * inst = NULL;

        if ( (inst = CMNewInstance(broker, op, rc) ) == NULL ) {
		cl_log(LOG_ERR, "%s:%d: create instance failed for %s", 
			__FUNCTION__, __LINE__, classname);
                return NULL;
        }

        /* set properties */
        CMSetProperty(inst, left, (CMPIValue *)&lop, CMPI_ref);
        CMSetProperty(inst, right, (CMPIValue *)&rop, CMPI_ref);

        return inst;
}

/* enumerate instance names */
CMPIArray *
cmpi_instance_names(CMPIBroker * broker, char * namespace, char * classname, 
		CMPIContext * ctx, CMPIStatus * rc)
{
        int max_len = 256;
	CMPIObjectPath * refs[max_len];
        CMPIEnumeration * en;
        CMPIObjectPath * op = NULL;
        CMPIArray * array;
        int len = 0, i = 0;

	DEBUG_ENTER();
        /* create ObjectPath for the class */
        op = CMNewObjectPath(broker, namespace, classname, rc);
        RETURN_FAIL_IFNULL_OBJ(op, "ObjectPath");

	/* enumerate instance names of the class */
        en = CBEnumInstanceNames(broker, ctx, op, rc);
        if ( CMIsNullObject(en) ){
               cl_log(LOG_ERR, 	"cmpi_instance_names: "
				"enumerate instance names for %s failed.", 
				classname);
                return NULL;
        }
       
        len = 0;
        while(CMHasNext(en, rc)) {
                CMPIObjectPath * top = CMGetNext(en, rc).value.ref;
                if ( CMIsNullObject(top) ) {
                        continue;
                }
                if ( len == max_len ) {
                        break;
                }
		refs[len] = top;
                len ++;
        }

        if ((array = CMNewArray(broker, len, CMPI_ref, rc)) == NULL ) {
                cl_log(LOG_ERR, "cmpi_instance_names: create array failed");
                return NULL;
        }

	for (i=0; i<len; i++) {
                CMSetArrayElementAt(array, i, &refs[i], CMPI_ref);
	}

        cl_log (LOG_INFO, "cmpi_instance_names: array length is %d", i);
	DEBUG_LEAVE();
        return array;
}

/* get a association instance */ 
int
assoc_get_inst(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                  CMPIResult * rslt, CMPIObjectPath * cop, 
                  char * left, char * right, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIObjectPath * left_op, * right_op;
        CMPIInstance * ci = NULL;
        char * nsp = NULL;

	DEBUG_ENTER();
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
        
        ci = MakeInstance(broker, classname, op, left, right,
                             left_op, right_op, rc);
        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, 
                        "%s: failed to create instance", __FUNCTION__);
                return HA_FAIL;
        }

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);
	DEBUG_LEAVE();
        return HA_OK;
}


/* enum a path's associators(counterpart) */
int
assoc_enum_associators(CMPIBroker * broker, char * classname, 
                    CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                    char * left, char * right, char * lclass, char * rclass,
                    const char * assoc_class, const char * result_class, 
                    const char * role, const char * result_role, 
                    assoc_pred_func_t pred_func, assoc_enum_func_t enum_func,
                    int EnumInst, CMPIStatus * rc)
{
        char * 		namespace = NULL;
        CMPIArray * 	array = NULL;
        char * 		source_class = NULL;
	char * 		target_class = NULL;
        char * 		target_role;
        int 		left_is_source = 0;
        int 		i, array_size;
        CMPIInstance * 	inst = NULL;

	DEBUG_ENTER();
        namespace = CMGetCharPtr(CMGetNameSpace(cop, rc));
        if ( assoc_class ) {
                CMPIObjectPath * op = CMNewObjectPath(broker, namespace, classname, rc);
                if ( CMIsNullObject(op) ){
                        return HA_FAIL;
                }               
                if ( !CMClassPathIsA(broker, op, assoc_class, rc) ) {
                        CMReturnDone(rslt);
                        return HA_OK;
                }
        }

        /* to verify the cop is valid */
        if ( CBGetInstance( broker, ctx, cop, NULL, rc) == NULL ){
                cl_log(LOG_ERR, "%s: failed to get instance", __FUNCTION__);
                return HA_FAIL;
        }

        source_class = CMGetCharPtr(CMGetClassName(cop, rc));
        if ( ClassIsA(source_class, lclass, broker, cop) ){
                /* if left is source, then right is target */
                target_class = rclass;
                target_role = right;
                left_is_source = 1;
        } else if ( ClassIsA(source_class, rclass, broker, cop)) {
                /* if right is source, then left is target */
                target_class = lclass;
                target_role = left;
                left_is_source = 0;
        } else {
                cl_log(LOG_ERR, "source neither lclass nor rclass"
			"lclass: %s, rclass: %s, source class: %s", 
                      	lclass, rclass, source_class);
                return HA_FAIL;
        }

	array = enum_func? enum_func(broker, classname, ctx, namespace, 
                                  target_class, target_role, cop, rc)
			: cmpi_instance_names(broker, namespace, 
				target_class, ctx, rc);
        RETURN_FAIL_IFNULL_OBJ(array, "Array");
        array_size = CMGetArrayCount(array, rc);

        for ( i = 0; i < array_size; i ++){  
                CMPIObjectPath * target_path;
                CMPIObjectPath * left_op = NULL;
                CMPIObjectPath * right_op = NULL;

                target_path = CMGetArrayElementAt(array, i, rc).value.ref;
                RETURN_FAIL_IFNULL_OBJ(target_path, "target ObjectPath");

                /* if left is source, then left op is cop */
                left_op = left_is_source? cop : target_path;
                right_op = left_is_source? target_path : cop;

                if ( pred_func && ! pred_func(broker, classname, ctx, left_op,
                                    right_op, rc) ){
                        continue;
                }
                
                if (!EnumInst) {
                        CMReturnObjectPath(rslt, target_path);
                        continue;
                } else {
                        inst = CBGetInstance(broker, ctx, target_path, NULL, rc); 
                        RETURN_FAIL_IFNULL_OBJ(inst, "target Instance");
                        CMReturnInstance(rslt, inst);
                }
        } /* whlie */
                 
        CMReturnDone(rslt);
	DEBUG_LEAVE();
        return HA_OK;
}

/* enumerate an ObjectPath's references */
int
assoc_enum_references(CMPIBroker * broker, char * classname,
                   CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                   char * left, char * right, char * lclass, char * rclass,
                   const char * result_class, const char * role,
                   assoc_pred_func_t pred_func, assoc_enum_func_t enum_func, 
                   int EnumInst, CMPIStatus * rc)
{
        char * 		namespace = NULL;
        char * 		source_class = NULL;
	char * 		target_class = NULL;
        char * 		target_role;
        int		left_is_source = 0;
        CMPIArray * 	array;
        int 		i, array_size;
	DEBUG_ENTER();
        namespace = CMGetCharPtr(CMGetNameSpace(cop, rc));
#if 0
        if ( result_class ) {
                CMPIObjectPath * op = NULL;
                op = CMNewObjectPath(broker, namespace, classname, rc);
                RETURN_FAIL_IFNULL_OBJ(op, "ObjectPath");
                if ( !CMClassPathIsA(broker, op, result_class, rc) ){
                        CMReturnDone(rslt);
                        return HA_OK;
                }
        }
#endif
        RETURN_FAIL_IFNULL_OBJ( CBGetInstance( broker, ctx, cop, NULL, rc), 
                                "Instance of cop");
        /* get source class */
        source_class = CMGetCharPtr(CMGetClassName(cop, rc));
        if ( ClassIsA(source_class, lclass, broker, cop) ){ /* source is left */
                target_class = rclass;
                target_role = right;
                left_is_source = 1;
        } else if ( ClassIsA(source_class, rclass, broker, cop)) { /* source is right */
                target_class = lclass;
                target_role = left;
                left_is_source = 0;
        } else {
                cl_log(LOG_ERR, "Source class is neither lclass nor rclass"
				"lclass: %s, rclass: %s, source name: %s.", 
				lclass, rclass, source_class);
                return HA_FAIL;
        }

        /* enumerate taget object paths */
	array = enum_func ? enum_func(broker, classname, ctx, namespace, 
				target_class, target_role, cop, rc) 
		  : cmpi_instance_names(broker, namespace, target_class, 
				ctx, rc);
        RETURN_FAIL_IFNULL_OBJ(array, "target ObjectPath array");
        array_size = CMGetArrayCount(array, rc);

	/* for each ObjectPath */
        for ( i = 0; i < array_size; i ++){
                CMPIObjectPath * assoc_op = NULL;
                CMPIObjectPath * target_path;
                CMPIObjectPath * left_op = NULL;
                CMPIObjectPath * right_op = NULL;

                /* next object path */
                target_path = CMGetArrayElementAt(array,i, rc).value.ref;
		if(target_path == NULL ) { 
			cl_log(LOG_WARNING, "Got a NULL ObjectPath.");
			continue ; 
		}

                /* if left is source, then left op is cop */
                left_op = left_is_source? cop : target_path;
                right_op = left_is_source? target_path : cop;

		/* left_op and right_op is not related if pred_func return 0 */
                if ( pred_func && ! pred_func(broker, classname, ctx, left_op, 
				right_op, rc) ){
                        continue;
                }

                /* create a ObjectPath for the association class */
                assoc_op = CMNewObjectPath(broker, namespace, classname, rc);
                RETURN_FAIL_IFNULL_OBJ(assoc_op, "association ObjectPath");

                if ( !EnumInst ) { 
                        /* add keys to the association path */
                        CMAddKey(assoc_op, left, &left_op, CMPI_ref);
                        CMAddKey(assoc_op, right, &right_op, CMPI_ref);
                        CMReturnObjectPath(rslt, assoc_op);
                } else {
                        CMPIInstance * inst = NULL;
                        inst = MakeInstance(broker, classname, assoc_op, left, 
                                               right, left_op, right_op, rc);
                        RETURN_FAIL_IFNULL_OBJ(inst, "association Instance");
                        CMReturnInstance(rslt, inst);
                }
        } /* while */
                 
        CMReturnDone(rslt);
	DEBUG_LEAVE();
        return HA_OK;
}


/* enum association's instances */
int
assoc_enum_insts(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
			CMPIResult * rslt, CMPIObjectPath * cop, 
			char * left, char * right, char * lclass, char * rclass,
			assoc_pred_func_t pred_func, assoc_enum_func_t enum_func,
			int EnumInst, CMPIStatus * rc)
{
        CMPIObjectPath *	assoc_op = NULL;
        CMPIArray * 		left_array = NULL;
	CMPIArray * 		right_array = NULL;
        int 			i, left_array_size;
        char * 			namespace = NULL;

	DEBUG_ENTER();
        namespace = CMGetCharPtr(CMGetNameSpace(cop, rc));
        /* get the instance anems enumeration of the left class */
	left_array = enum_func ?  enum_func(broker, classname, ctx, namespace,
					lclass, left, NULL, rc)
			: cmpi_instance_names(broker,namespace,lclass,ctx,rc);

	/* avoid invoking cmpi_instance_names left_array_size times */
	right_array = enum_func ? NULL 
			: cmpi_instance_names(broker,namespace,rclass,ctx,rc);
        RETURN_FAIL_IFNULL_OBJ(left_array, "LeftArray");

        /* create path for this association */
        assoc_op = CMNewObjectPath(broker, namespace, classname, rc);
        RETURN_FAIL_IFNULL_OBJ(assoc_op, "association ObjectPath");

        left_array_size = CMGetArrayCount(left_array, rc);
        for ( i = 0; i < left_array_size; i ++) {
                CMPIObjectPath *	left_op = NULL;
                int 			j, right_array_size;

                left_op = CMGetArrayElementAt(left_array, i, rc).value.ref;
                if ( CMIsNullObject(left_op ) ) {
                        break;
                }

                /* now it's turn to get the right enumeration */
                /* if we have enum_func, use enum_func to get enumeration,
                   the result may be various for different left_op, 
                   so we must call it everytime */
                right_array = enum_func?  
			enum_func(broker, classname, ctx, namespace, 
				rclass, right, left_op, rc) : right_array; 
                RETURN_FAIL_IFNULL_OBJ(right_array, "RightArray");
                right_array_size = CMGetArrayCount(right_array, rc);

                for (j = 0; j < right_array_size; j ++){
                        CMPIObjectPath * right_op = NULL;
                        right_op = CMGetArrayElementAt(right_array, 
							j, rc).value.ref;
                        if ( CMIsNullObject(right_op) ) {
                                break;
                        }
                        /* associated ? */
                        if ( pred_func && !pred_func(broker, classname, ctx,
	                                           left_op, right_op, rc) ) {
                                continue;
                        }
                        if (!EnumInst ) {
                                CMAddKey(assoc_op, left, &left_op, CMPI_ref);
                                CMAddKey(assoc_op, right, &right_op, CMPI_ref); 
                                CMReturnObjectPath(rslt, assoc_op);
                        } else {    /* instances */
                                CMPIInstance * inst = NULL;
                                inst = MakeInstance(broker,classname,assoc_op,
					left, right, left_op, right_op, rc);
                                CMReturnInstance(rslt, inst);
                        }
               } /* for */ 
        } /* for */
        CMReturnDone(rslt); 
	DEBUG_LEAVE();
        return HA_OK;
}
