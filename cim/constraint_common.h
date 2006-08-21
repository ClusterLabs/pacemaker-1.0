/*
 * constraint_common.h: common functions for constraint providers
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

#ifndef _CONSTRAINT_COMMON_H
#define _CONSTRAINT_COMMON_H

int	constraing_get_inst(CMPIBroker * broker, char * classname, 
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,
		char ** properties, uint32_t type, CMPIStatus * rc);

int 	constraint_enum_insts(CMPIBroker * broker, char * classname, 
		CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop, 
               int need_inst, uint32_t type, CMPIStatus * rc);

int	delete_constraint(CMPIBroker * broker, char * classname, 
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, uint32_t type, CMPIStatus * rc);

int	constraint_update_inst(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, char ** properties,
		uint32_t type, CMPIStatus * rc);

int	constraint_create_inst(CMPIBroker * broker, char * classname,
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, CMPIInstance * ci, uint32_t type, 
		CMPIStatus * rc);

int	constraint_delete_inst(CMPIBroker * broker, char * classname, 
		CMPIInstanceMI * mi, CMPIContext * ctx, CMPIResult * rslt, 
		CMPIObjectPath * cop, uint32_t type, CMPIStatus * rc);
#endif
