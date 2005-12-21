/*
 * CIM Provider association Utils Header File
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


#ifndef _ASSOC_UTILS_H
#define _ASSOC_UTILS_H

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
#include <glib.h>


/***********************************************************
 *  assocation functions 
 **********************************************************/

typedef int 
(* assoc_func_t)(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                 CMPIObjectPath * left, CMPIObjectPath * right, /*void * user_data,*/
                 CMPIStatus * rc);

struct assoc_env {
        GPtrArray * left_op_tbl;
        GPtrArray * right_op_tbl;
        int (*done)(struct assoc_env * env); 
};


/* target_name: which should be enumerated
   source_op  : generate enumeration according to this object path */

typedef CMPIArray * 
(* assoc_enum_func_t) (CMPIBroker * broker, char * classname, CMPIContext * ctx,
                       char * namespace, char * target_name, char * target_role, 
                       CMPIObjectPath * source_op, /*void * user_data,*/
                       CMPIStatus * rc);

CMPIArray *
default_enum_func (CMPIBroker * broker, char * classname, CMPIContext * ctx,
                   char * namespace, char * target_name, char * target_role, 
                   CMPIObjectPath * source_op, CMPIStatus * rc);
/* get instance */
int
cm_assoc_get_inst(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
                  CMPIResult * rslt, CMPIObjectPath * cop, 
                  char * left, char * right, CMPIStatus * rc);

/* enumerate cop's associators */
int 
cm_enum_associators(CMPIBroker * broker, char * classname, 
                    CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                    char * left, char * right, char * lclass, char * rclass,
                    const char * assoc_class, const char * result_class, 
                    const char * role, const char * result_role, 
                    assoc_func_t func, assoc_enum_func_t enum_func,
                    int inst, CMPIStatus * rc);

/* enum cop's references */
int
cm_enum_references(CMPIBroker * broker, char * classname,
                   CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
                   char * left, char * right, char * lclass, char * rclass,
                   const char * result_class, const char * role,
                   assoc_func_t func, assoc_enum_func_t enum_func,
                   int inst, CMPIStatus * rc);

/* enum association's instances */
int
cm_assoc_enum_insts(CMPIBroker * broker, char * classname,
                    CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
                    char * left, char * right, char * lclass, char * rclass,
                    assoc_func_t func, assoc_enum_func_t enum_func,
                    int inst, CMPIStatus * rc);

#endif   /* _CMPI_UTILS_H */
