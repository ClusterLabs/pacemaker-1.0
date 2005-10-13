/*
 * Utils for CIM Providers
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


#include <stdio.h>
#include <regex.h>

#include <hb_api.h>
#include <clplumbing/cl_log.h>


#include "cmpi_utils.h"

static int assoc_get_lr_instances(
                CMPIInstance ** first_inst, CMPIInstance ** second_inst,
                CMPIObjectPath * first_op, CMPIObjectPath * second_op,
                CMPIBroker * broker, CMPIContext * ctx, CMPIStatus * rc);


void
cmpi_assert(const char * assertion, int line, const char * file)
{
        cl_log(LOG_ERR, "Assertion \"%s\" failed on line %d in file \"%s\""
        ,       assertion, line, file);
        exit(1);
}

int 
run_shell_command(const char * cmnd, int * ret, 
			char *** std_out, char *** std_err)
				/* std_err not used currently */
{
	FILE * fstream = NULL;
	char * buffer = NULL;
	int cmnd_rc, rc, i;

	if ( (fstream = popen(cmnd, "r")) == NULL ){
		return HA_FAIL;
	}

	buffer = malloc(4096);
	if ( buffer == NULL ) {
		rc = HA_FAIL;
		goto exit;
	}

	*std_out = malloc(sizeof(char*));
	(*std_out)[0] = NULL;

	i = 0;
	while (!feof(fstream)) {

		if ( fgets(buffer, 4096, fstream) != NULL ){
			/** add buffer to std_out **/
			*std_out = realloc(*std_out, (i+2) * sizeof(char*));	
			(*std_out)[i] = strdup(buffer);
			(*std_out)[i+1] = NULL;		
		}else{
			continue;
		}
		i++;

	}
	
	free(buffer); 

	rc = HA_OK;
exit:
	if ( (cmnd_rc = pclose(fstream)) == -1 ){
		/*** WARNING log ***/
                cl_log(LOG_WARNING, "failed to close pipe.");
	}
	*ret = cmnd_rc;
	return rc;
}



int
regex_search(const char * reg, const char * str, char *** match)
{
	regex_t regexp;
	const size_t nmatch = 16;	/* max match times */
	regmatch_t pm[16];
	int i;
	int ret;

	ret = regcomp(&regexp, reg, REG_EXTENDED);

	if ( ret != 0) {
		cl_log(LOG_ERR, "Error regcomp regex %s", reg);
		return HA_FAIL;
	}

	ret = regexec(&regexp, str, nmatch, pm, 0);

	if ( ret == REG_NOMATCH ){
		regfree(&regexp);
		return HA_FAIL;
	}else if (ret != 0){
        	cl_log(LOG_ERR, "Error regexec\n");
		regfree(&regexp);
		return HA_FAIL;
	}


	*match = malloc(sizeof(char*));
	(*match)[0] = NULL;


	for(i = 0; i < nmatch && pm[i].rm_so != -1; i++){
		
		int str_len = pm[i].rm_eo - pm[i].rm_so;

		*match = realloc(*match, (i+2) * sizeof(char*));

		(*match)[i] = malloc(str_len + 1);
		strncpy( (*match)[i], str + pm[i].rm_so, str_len);
		(*match)[i][str_len] = '\0';

		(*match)[i+1] = NULL;
	}

	regfree(&regexp);

	return HA_OK;
} 


int free_2d_array(char ** array){
	if (array) {
		int i = 0;
		while ( array[i] ){
			free(array[i]);
			i++;
		}
		
		free(array);
	}

	return HA_OK;
}




char * 
uuid_to_str(const cl_uuid_t * uuid){
        int i, len = 0;
        char * str = malloc(256);
        
        memset(str, 0, 256);

        for ( i = 0; i < sizeof(cl_uuid_t); i++){
                len += sprintf(str + len, "%.2X", uuid->uuid[i]);
        }
        return str;
}



int 
assoc_source_class_is_a(const char * source_class_name, char * class_name,
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

/******************************************************
 * association provider helper functions
 ******************************************************/

static int
assoc_get_lr_instances(
                CMPIInstance ** first_inst, CMPIInstance ** second_inst,
                CMPIObjectPath * first_op, CMPIObjectPath * second_op,
                CMPIBroker * broker, CMPIContext * ctx, CMPIStatus * rc) 
{
        *first_inst =  CBGetInstance(broker, ctx, first_op, NULL, rc);
        *second_inst =  CBGetInstance(broker, ctx, second_op, NULL, rc);


        if ( CMIsNullObject( *second_inst ) || CMIsNullObject ( *first_inst ) ) {
                cl_log(LOG_ERR, 
                       "%s: failed to get instance", __FUNCTION__);
                return HA_FAIL;
        }

        return HA_OK;
}


int
assoc_enumerate_associators(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * assocClass, 
                const char * resultClass, const char * role,
                const char * resultRole, assoc_pred_func_t pred,
                int add_inst, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        char * nsp = NULL;

        CMPIData data;
        CMPIObjectPath * t_op = NULL;
        CMPIEnumeration * en = NULL;
        char * source_class = NULL;
        char * target_class = NULL;
        int source_is_first = 0;
        CMPIInstance * inst = NULL;

        DEBUG_ENTER();

        cl_log(LOG_INFO, 
                "%s: asscClass, resultClass, role, resultRole = %s, %s, %s, %s",
                __FUNCTION__,
                assocClass, resultClass, role, resultRole);

        nsp = CMGetCharPtr(CMGetNameSpace(cop, rc));

        if ( assocClass ) {
                op = CMNewObjectPath(broker, nsp, classname, rc);
                if ( CMIsNullObject(op) ){
                        return HA_FAIL;
                }               
        }

        if ( assocClass && !CMClassPathIsA(broker, op, assocClass, rc) ) {

                CMReturnDone(rslt);
                return HA_OK;
        }

        /* if ( !assocClass || CMClassPathIsA(broker, op, assocClass, rc) ) */
        if ( CBGetInstance( broker, ctx, cop, NULL, rc) == NULL ){
                cl_log(LOG_ERR, "%s: failed to get instance", __FUNCTION__);
                return HA_FAIL;
        }
        
        source_class = CMGetCharPtr(CMGetClassName(cop, rc));
        
        /* determine target_class */
        if ( assoc_source_class_is_a(source_class, first_class_name, 
                                     broker, cop) ){
                target_class = second_class_name;
                source_is_first = 1;
        } else if ( assoc_source_class_is_a(source_class, second_class_name, 
                                            broker, cop)) {
                target_class = first_class_name;
                source_is_first = 0;
        }
        
        RETURN_FAIL_IFNULL_OBJ(target_class, "target_class");
         
        t_op = CMNewObjectPath(broker, nsp, target_class, rc);
        RETURN_FAIL_IFNULL_OBJ(t_op, "t_op");
        
        en = CBEnumInstanceNames(broker, ctx, t_op, rc);
        RETURN_FAIL_IFNULL_OBJ(en, "t_op");
                
        while ( CMHasNext(en, rc) ){  
                /* enumerate target instances */
                CMPIInstance * first_inst = NULL;
                CMPIInstance * second_inst = NULL;
                int ret = 0;
                
                data = CMGetNext(en, rc);
        
                RETURN_FAIL_IFNULL_OBJ(data.value.ref, "target object path");

                if ( source_is_first ){
                        ret = assoc_get_lr_instances(&second_inst, &first_inst,
                                           data.value.ref, cop, broker, ctx, rc);
                        
                } else {
                        ret = assoc_get_lr_instances(&second_inst, &first_inst,
                                           cop, data.value.ref, broker, ctx, rc);
                        
                }
                
                if ( ret != HA_OK ){
                        cl_log(LOG_ERR, "%s: failed to get lr instances.", 
                               __FUNCTION__);
                        return HA_FAIL;
                }
                
                if ( pred && ! pred(first_inst, second_inst, rc) ){
                        continue;
                }
                
                if ( !add_inst) {
                        CMReturnObjectPath(rslt, data.value.ref);
                        continue;
                }        
                
                /*
                  if ( source_is_first ){
                  inst = second_inst;
                  } else {
                  inst = first_inst;
                  }
                */
                inst = CBGetInstance(broker, ctx, data.value.ref, NULL, rc); 
                
                RETURN_FAIL_IFNULL_OBJ(inst, "instance");

                CMReturnInstance(rslt, inst);
                
        } /* whlie */
                 
       
        CMReturnDone(rslt);
        DEBUG_LEAVE();

        return HA_OK;
}



int
assoc_enumerate_references(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop,  
                const char * resultClass, const char * role,
                assoc_pred_func_t pred, int add_inst, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;

        char * nsp = NULL;
        CMPIData data;
        CMPIObjectPath * t_op = NULL;
        CMPIEnumeration * en = NULL;
        char * source_class = NULL;
        char * target_class = NULL;
        int source_is_first = 0;

        nsp = CMGetCharPtr(CMGetNameSpace(cop, rc));

        if ( resultClass ) {
                op = CMNewObjectPath(broker, nsp, classname, rc);
                RETURN_FAIL_IFNULL_OBJ(op, "op");
        }

        if ( resultClass && !CMClassPathIsA(broker, op, resultClass, rc) ){
                CMReturnDone(rslt);
                return HA_OK;
        }

        /* if ( !resultClass || CMClassPathIsA(broker, op, resultClass, rc) ){ */

        RETURN_FAIL_IFNULL_OBJ( CBGetInstance( broker, ctx, cop, NULL, rc), 
                                "get instance");

        source_class = CMGetCharPtr(CMGetClassName(cop, rc));
                
        if ( assoc_source_class_is_a(source_class, first_class_name, 
                                     broker, cop) ){
                target_class = second_class_name;
                source_is_first = 1;
        } else if ( assoc_source_class_is_a(source_class, 
                                            second_class_name, broker, cop)) {
                target_class = first_class_name;
                source_is_first = 0;
        }

        RETURN_FAIL_IFNULL_OBJ(target_class, "target_class");

        t_op = CMNewObjectPath(broker, nsp, target_class, rc);
        RETURN_FAIL_IFNULL_OBJ(t_op, "t_op");
        
        en = CBEnumInstanceNames(broker, ctx, t_op, rc);
        RETURN_FAIL_IFNULL_OBJ(en, "en");

        while ( CMHasNext(en, rc) ){

                CMPIObjectPath * top = NULL;
                CMPIInstance * second_inst = NULL;
                CMPIInstance * first_inst = NULL;
                int ret = 0;

                data = CMGetNext(en, rc);
                RETURN_FAIL_IFNULL_OBJ(data.value.ref, "ref");

                top = CMNewObjectPath(broker, nsp, classname, rc);
                RETURN_FAIL_IFNULL_OBJ(top, "top");


                if ( source_is_first ){
                        /* source class is LinuxHA_ClusterNode */
                        ret = assoc_get_lr_instances(&second_inst, 
                                                 &first_inst, data.value.ref, 
                                                 cop, broker, ctx, rc);
                        
                } else {  /* source class is LinuxHA_ClusterResource */
                        ret = assoc_get_lr_instances(&second_inst, 
                                            &first_inst, cop, 
                                            data.value.ref,broker, ctx, rc);
                                 
                }

                if ( ret != HA_OK ){
                        cl_log(LOG_ERR, "%s: failed to get lr instances.",
                               __FUNCTION__);
                        return HA_FAIL;
                }
                 
                if ( pred && ! pred(first_inst, second_inst, rc) ){
                        continue;
                }
                        
                if ( source_is_first ){ 
                        CMAddKey(top, second_ref, 
                                 (CMPIValue *)&data.value.ref, CMPI_ref);
                        CMAddKey(top, first_ref, 
                                 (CMPIValue *)&cop, CMPI_ref);
                }else{
                        CMAddKey(top, first_ref, 
                                 (CMPIValue *)&data.value.ref, CMPI_ref);
                        CMAddKey(top, second_ref, 
                                 (CMPIValue *)&cop, CMPI_ref);
                } 
                
                if ( add_inst ){
                        CMPIInstance * inst = NULL;

                        inst = CMNewInstance(broker, top, rc);
                        RETURN_FAIL_IFNULL_OBJ(inst, "inst");
                                
                        CMSetProperty(inst, first_ref, 
                                      &data.value.ref, CMPI_ref);
                        CMSetProperty(inst, second_ref,
                                      &cop, CMPI_ref);

                        CMReturnInstance(rslt, inst);
                } else {
                        /* ReferenceNames */
                        CMReturnObjectPath(rslt, top);
                }
        } /* while */
                 
        CMReturnDone(rslt);

        return HA_OK;
}

int
assoc_enumerate_instances(
                CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, assoc_pred_func_t pred, 
                int add_inst, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIObjectPath * first_op = NULL;
        CMPIEnumeration * first_en = NULL;
        CMPIData first_data;
        CMPIData second_data;

        CMPIObjectPath * second_op = NULL;
        CMPIEnumeration * second_en = NULL;
        char * namespace = NULL;

        DEBUG_ENTER();

        namespace = CMGetCharPtr(CMGetNameSpace(cop, rc));

        first_op = CMNewObjectPath(broker, namespace, first_class_name, rc);
        second_op = CMNewObjectPath(broker, namespace, second_class_name, rc);

        RETURN_FAIL_IFNULL_OBJ(first_op, "frist_op");
        RETURN_FAIL_IFNULL_OBJ(second_op, "second_op");
       
        second_en = CBEnumInstanceNames(broker, ctx, second_op, rc);
        RETURN_FAIL_IFNULL_OBJ(second_en, "second_en");

        op = CMNewObjectPath(broker, namespace, classname, rc);
        RETURN_FAIL_IFNULL_OBJ(op, "op");
        
        while ( CMHasNext(second_en, rc) ){
                CMPIInstance * second_inst = NULL;
                CMPIInstance * first_inst = NULL;
 
                second_data = CMGetNext(second_en, rc);
                RETURN_FAIL_IFNULL_OBJ(second_data.value.ref, "ref");
                 
                second_inst = CBGetInstance(broker, ctx,
                                second_data.value.ref, NULL, rc);
                
                RETURN_FAIL_IFNULL_OBJ(second_inst, "second_inst");
                first_en = 
                        CBEnumInstanceNames(broker, ctx, first_op, rc);
               
                RETURN_FAIL_IFNULL_OBJ(first_en, "first_en");

                while ( CMHasNext(first_en, rc) ){
                        
                        first_data = CMGetNext(first_en, rc);
                        RETURN_FAIL_IFNULL_OBJ(first_data.value.ref, "ref");

                        first_inst = CBGetInstance(broker, ctx,
                                        first_data.value.ref, NULL, rc);

                        RETURN_FAIL_IFNULL_OBJ(first_inst, "first_inst");

                        if ( pred && !pred(first_inst, second_inst, rc) ) {
                                continue;
                        }
                       
                        CMAddKey(op, first_ref, 
                                 (CMPIValue *)&first_data.value.ref, CMPI_ref);
                        
                        CMAddKey(op, second_ref,
                                 (CMPIValue *)&second_data.value.ref, CMPI_ref); 
                        

                        if ( add_inst ) {    /* instances */
                                CMPIInstance * inst = NULL;

                                inst = CMNewInstance(broker, op, rc);
                                RETURN_FAIL_IFNULL_OBJ(inst, "inst");
                                
                                CMSetProperty(inst, first_ref, 
                                              &first_data.value.ref, CMPI_ref);
                                CMSetProperty(inst, second_ref, 
                                              &second_data.value.ref, CMPI_ref);

                                CMReturnInstance(rslt, inst);

                        } else {   /* instance names only */ 
                                CMReturnObjectPath(rslt, op);
                        }
               } /* while */ 
        } /* while */


        DEBUG_LEAVE();
        CMReturnDone(rslt); 

        return HA_OK;
}


int
assoc_get_instance(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIStatus * rc)
{
        CMPIObjectPath * op = NULL;
        CMPIData data_first;
        CMPIData data_second;
        CMPIInstance * ci = NULL;
        char * nsp = NULL;


        DEBUG_ENTER();

        data_first = CMGetKey(cop, first_ref, rc);
        data_second = CMGetKey(cop, second_ref, rc);

        if ( data_first.value.ref == NULL ){
                cl_log(LOG_ERR, "%s: failed to get key %s", 
                                        __FUNCTION__, first_ref);
                return HA_FAIL;
        }

        if ( data_second.value.ref == NULL){
                cl_log(LOG_ERR, "%s: failed to get key %s", 
                                        __FUNCTION__, second_ref);
                return HA_FAIL;
        }

        nsp = CMGetCharPtr(CMGetNameSpace(cop, rc));


        CMSetNameSpace(data_first.value.ref, nsp);
        CMSetNameSpace(data_second.value.ref, nsp);

        if ( !CBGetInstance(broker, ctx, data_first.value.ref, NULL, rc)  ||
             !CBGetInstance(broker, ctx, data_second.value.ref, NULL, rc) ){
                return HA_FAIL;
        }

        op = CMNewObjectPath(broker, nsp, classname, rc);
        if ( CMIsNullObject(op) ) {
                cl_log(LOG_ERR, 
                        "%s: failed to create object path", __FUNCTION__);
                return HA_FAIL;
        }

        ci = CMNewInstance(broker, op, rc);

        if ( CMIsNullObject(ci) ) {
                cl_log(LOG_ERR, 
                        "%s: failed to create instance", __FUNCTION__);
                return HA_FAIL;
        }

        CMSetProperty(ci, first_ref, 
                        (CMPIValue *)&(data_first.value.ref), CMPI_ref);
        CMSetProperty(ci, second_ref, 
                        (CMPIValue *)&(data_second.value.ref), CMPI_ref);        

        CMReturnInstance(rslt, ci);
        CMReturnDone(rslt);

        DEBUG_LEAVE();
        return HA_OK;
}
