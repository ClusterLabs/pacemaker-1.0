/*
 * resource_common.h: common functions for resource providers
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

#ifndef _RESROUCE_COMMON_H
#define _RESOURCE_COMMON_H


/* get a resource instance */
int resource_get_inst(CMPIBroker * broker, char * classname, CMPIContext * ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref,
		char ** properties, uint32_t type, CMPIStatus * rc);

/* enumerate instances or instance names */
int resource_enum_insts(CMPIBroker* broker, char* classname, CMPIContext* ctx, 
		CMPIResult * rslt, CMPIObjectPath * ref, int need_inst, 
		uint32_t type, CMPIStatus * rc);

/* cleanup provider */
int resource_cleanup(CMPIBroker* broker, char* classname, CMPIInstanceMI* mi, 
		CMPIContext * ctx, uint32_t type, CMPIStatus * rc);

/* delete resource */
int resource_del_inst(CMPIBroker* broker, char* classname, CMPIContext* ctx,
                CMPIResult * rslt, CMPIObjectPath * ref, CMPIStatus * rc);

/* update resource */
int resource_update_inst(CMPIBroker* broker, char* classname, CMPIContext* ctx,
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci, 
		char ** properties, uint32_t type, CMPIStatus * rc);

/* create a resource */
int resource_create_inst(CMPIBroker* broker, char* classname, CMPIContext* ctx, 
		CMPIResult * rslt, CMPIObjectPath * cop, CMPIInstance * ci, 
		uint32_t type, CMPIStatus * rc);

#if 0
/* add a operation to resource */
int resource_add_operation(CMPIBroker* broker, char*classname, CMPIContext* ctx,
                CMPIResult * rslt, CMPIObjectPath * ref, uint32_t type,
		CMPIArgs *in, CMPIArgs *out, CMPIStatus * rc);
#endif

/* add sub resource to group/clone/master-slave */
int resource_add_subrsc(CMPIBroker* broker, char* classname, CMPIContext* ctx,
                CMPIResult * rslt, CMPIObjectPath * ref, uint32_t type,
		CMPIArgs *in, CMPIArgs *out, CMPIStatus * rc);
#endif
