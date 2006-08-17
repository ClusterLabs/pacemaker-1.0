/*
 * ha_mgmt_client.i: swig interface file
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
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

%module LHAMgmtLib
%{
#include "ha_mgmt_client.h" 
%}

%pragma(java) jniclasscode=%{
  static {
    try {
      System.loadLibrary("lhamgmt");
    } catch (UnsatisfiedLinkError e) {
      System.exit(1);
   }
  }
  
%}

%pragma(java) modulecode=%{
  public static final String MSG_OK                 	=	"ok";
  public static final String MSG_FAIL               	=	"fail";
  public static final String MSG_LOGIN              	=	"login";
  public static final String MSG_LOGOUT             	=	"logout";
  public static final String MSG_ECHO               	=	"echo";
  public static final String MSG_REGEVT             	=	"regevt";
  public static final String MSG_CIB_VERSION        	=	"cib_version";
  public static final String MSG_CRM_CONFIG         	=	"crm_config";
  public static final String MSG_UP_CRM_CONFIG      	=	"up_crm_config";
  public static final String MSG_HB_CONFIG          	=	"hb_config";
  public static final String MSG_ALLNODES           	=	"all_nodes";
  public static final String MSG_ACTIVENODES        	=	"active_nodes";
  public static final String MSG_CRMNODES	    	=	"crm_nodes";
  public static final String MSG_DC                 	=	"dc";
  public static final String MSG_NODE_CONFIG        	=	"node_config";
  public static final String MSG_STANDBY            	=	"standby";
  public static final String MSG_RUNNING_RSC        	=	"running_rsc";
  public static final String MSG_ALL_RSC            	=	"all_rsc";
  public static final String MSG_RSC_ATTRS          	=	"rsc_attrs";
  public static final String MSG_RSC_TYPE           	=	"rsc_type";
  public static final String MSG_SUB_RSC            	=	"sub_rsc";
  public static final String MSG_RSC_RUNNING_ON     	=	"rsc_running_on";
  public static final String MSG_RSC_STATUS         	=	"rsc_status";
  public static final String MSG_ADD_RSC            	=	"add_rsc";
  public static final String MSG_ADD_GRP            	=	"add_grp";
  public static final String MSG_DEL_RSC            	=	"del_rsc";
  public static final String MSG_CLEANUP_RSC        	=	"cleanup_rsc";
  public static final String MSG_MOVE_RSC           	=	"move_rsc";
  public static final String MSG_UP_RSC_ATTR        	=	"up_rsc_attr";
  public static final String MSG_RSC_PARAMS         	=	"rsc_params";
  public static final String MSG_UP_RSC_PARAMS      	=	"up_rsc_params";
  public static final String MSG_DEL_RSC_PARAM      	=	"del_rsc_param";
  public static final String MSG_SET_TARGET_ROLE    	=	"set_target_role";
  public static final String MSG_RSC_OPS            	=	"rsc_ops";
  public static final String MSG_UP_RSC_OPS         	=	"up_rsc_ops";
  public static final String MSG_DEL_RSC_OP         	=	"del_rsc_op";
  public static final String MSG_UPDATE_CLONE       	=	"up_clone";
  public static final String MSG_GET_CLONE          	=	"get_clone";
  public static final String MSG_UPDATE_MASTER      	=	"up_master";
  public static final String MSG_GET_MASTER         	=	"get_master";
  public static final String MSG_GET_CONSTRAINTS    	=	"get_cos";
  public static final String MSG_GET_CONSTRAINT     	=	"get_co";
  public static final String MSG_DEL_CONSTRAINT     	=	"del_co";
  public static final String MSG_UP_CONSTRAINT      	=	"up_co";
  public static final String MSG_RSC_CLASSES        	=	"rsc_classes";
  public static final String MSG_RSC_TYPES          	=	"rsc_types";
  public static final String MSG_RSC_PROVIDERS      	=	"rsc_providers";
  public static final String MSG_RSC_METADATA       	=	"rsc_metadata";
  public static final String EVT_CIB_CHANGED        	=	"evt:cib_changed";
  public static final String EVT_DISCONNECTED       	=	"evt:disconnected";
  public static final String EVT_TEST               	=	"evt:test";

  public static final int    LOG_INFO			=	6;
  public static final int    LOG_DEBUG			=	7;
  public static final int    LOG_WARNING		=	4;
  public static final int    LOG_ERR			=	3;

%}


char*	process_cmnd_native(const char* cmd);
char*	process_cmnd_external(const char *cmd);
char*	process_cmnd_eventd(const char *cmd);

char*	wait_for_events(void);
void	clLog(int priority, const char* logs);

void	start_heartbeat(const char* node);
void	stop_heartbeat(const char* node);

