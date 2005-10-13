/*
 * CIM Provider Utils Header File
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


#ifndef _CMPI_UTILS_H
#define _CMPI_UTILS_H

#include <clplumbing/cl_uuid.h>

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>

#define DEBUG_ENTER() cl_log(LOG_INFO, "%s: --- ENTER ---", __FUNCTION__)
#define DEBUG_LEAVE() cl_log(LOG_INFO, "%s: --- LEAVE ---", __FUNCTION__)

#define DEBUG_PID() cl_log(LOG_INFO,                    \
                        "%s: my pid is %d", __FUNCTION__, (int)getpid())


#ifndef ASSERT
#ifdef HAVE_STRINGIZE
#       define  ASSERT(X)       {if(!(X)) cmpi_assert(#X, __LINE__, __FILE__);}
#else
#       define  ASSERT(X)       {if(!(X)) cmpi_assert("X", __LINE__, __FILE__);}
#endif
#endif


/* return ret if obj is a NULL CMPI Object */
#define RETURN_IFNULL_OBJ(obj, ret, n) do {                          \
                if ( CMIsNullObject(obj) ) {                         \
                       cl_log(LOG_ERR, "%s: CMPI Object %s is NULL", \
                                        __FUNCTION__, n);            \
                       return ret;                                   \
                }                                                    \
        } while (0)

/* return HA_FAIL if obj is a NULL CMPI Object */
#define RETURN_FAIL_IFNULL_OBJ(obj, n) RETURN_IFNULL_OBJ(obj, HA_FAIL, n)
#define RETURN_NULL_IFNULL_OBJ(obj, n) RETURN_IFNULL_OBJ(obj, NULL, n)


void cmpi_assert(const char * assertion, int line, const char * file);
int run_shell_command(const char * cmnd, int * ret, 
				char *** std_out, char *** std_err);
int regex_search(const char * reg, const char * str, char *** match);
int free_2d_array(char ** array);
char * uuid_to_str(const cl_uuid_t * uuid);

int assoc_source_class_is_a(const char * source_class_name, char * class_name,
                        CMPIBroker * broker, CMPIObjectPath * cop); 


typedef int (* assoc_pred_func_t) 
                (CMPIInstance * first, CMPIInstance * second, CMPIStatus * rc);

int assoc_enumerate_associators(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * assocClass, 
                const char * resultClass, const char * role,
                const char * resultRole, assoc_pred_func_t pred,
                int add_inst, CMPIStatus * rc);

int assoc_enumerate_references(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop,  
                const char * resultClass, const char * role,
                assoc_pred_func_t pred, int add_inst, CMPIStatus * rc);


int assoc_enumerate_instances(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, assoc_pred_func_t pred,
                int add_inst, CMPIStatus * rc);

int assoc_get_instance(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIStatus * rc);

#endif

