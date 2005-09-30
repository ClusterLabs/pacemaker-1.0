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


#ifndef _CMPI_UTIL_H
#define _CMPI_UTIL_H

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

void cmpi_assert(const char * assertion, int line, const char * file);

int run_shell_command(const char * cmnd, int * ret, 
				char *** std_out, char *** std_err);
int regex_search(const char * reg, const char * str, char *** match);
int free_2d_array(char ** array);
char * uuid_to_str(const cl_uuid_t * uuid);

int assoc_source_class_is_a(const char * source_class_name, char * class_name,
                        CMPIBroker * broker, CMPIObjectPath * cop); 


typedef int (*relation_pred) 
                (CMPIInstance * first, CMPIInstance * second, CMPIStatus * rc);

int assoc_enumerate_associators(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, const char * assocClass, 
                const char * resultClass, const char * role,
                const char * resultRole, relation_pred pred,
                int add_inst, CMPIStatus * rc);

int assoc_enumerate_references(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop,  
                const char * resultClass, const char * role,
                relation_pred pred, int add_inst, CMPIStatus * rc);


int assoc_enumerate_instances(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, relation_pred pred,
                int add_inst, CMPIStatus * rc);

int assoc_get_instance(CMPIBroker * broker, char * classname,
                char * first_ref, char * second_ref,
                char * first_class_name, char * second_class_name,
                CMPIContext * ctx, CMPIResult * rslt,
                CMPIObjectPath * cop, CMPIStatus * rc);

#endif

