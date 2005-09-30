/*
 * CIM Provider Header File 
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


#ifndef _CMPI_HA_INDICATION_H
#define _CMPI_HA_INDICATION_H


#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>
 
#include <hb_api.h>
#include "linuxha_info.h"



int nodestatus_event_handler(const char * node, const char *status);
int ifstatus_event_handler(const char * node, 
                        const char * lnk, const char * status);

int membership_event_handler(const char *node, SaClmClusterChangesT status);

int cluster_indication_initialize(char * classname, CMPIBroker * broker, 
                CMPIContext * ctx, CMPISelectExp * filter, CMPIStatus * rc);

int cluster_indication_finalize(char * classname, CMPIBroker * broker, 
                CMPIContext * ctx, CMPISelectExp * filter, CMPIStatus * rc);
 
                        
#endif
