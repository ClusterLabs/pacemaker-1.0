/* $Id: msg_xml.h,v 1.37 2005/07/06 12:34:26 andrew Exp $ */
/* 
 * Copyright (C) 2004 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef XML_TAGS__H
#define XML_TAGS__H

#define F_CRM_TASK			"crm_task"
#define F_CRM_HOST_TO			"crm_host_to"
#define F_CRM_MSG_TYPE			F_SUBTYPE
#define F_CRM_SYS_TO			"crm_sys_to"
#define F_CRM_SYS_FROM			"crm_sys_from"
#define F_CRM_HOST_FROM			F_ORIG
#define F_CRM_REFERENCE			XML_ATTR_REFERENCE
#define F_CRM_VERSION			XML_ATTR_VERSION
#define F_CRM_ORIGIN			"origin"
#define F_CRM_JOIN_ID			"join_id"

/*---- Common tags/attrs */
#define XML_ATTR_TAGNAME		F_XML_TAGNAME
#define XML_ATTR_PARENT			F_XML_PARENT
#define XML_TAG_CIB			"cib"
#define XML_TAG_FAILED			"failed"

#define XML_ATTR_NUMPEERS		"num_peers"
#define XML_ATTR_HAVE_QUORUM		"have_quorum"
#define XML_ATTR_CCM_TRANSITION		"ccm_transition"
#define XML_ATTR_GENERATION		"epoche"
#define XML_ATTR_GENERATION_ADMIN	"admin_epoche"
#define XML_ATTR_NUMUPDATES		"num_updates"
#define XML_ATTR_TIMEOUT		"timeout"
#define XML_ATTR_TSTAMP			"timestamp"
#define XML_ATTR_VERSION		"version"
#define XML_ATTR_DESC			"description"
#define XML_ATTR_ID			"id"
#define XML_ATTR_TYPE			"type"
#define XML_ATTR_FILTER_TYPE		"type_filter"
#define XML_ATTR_FILTER_ID		"id_filter"
#define XML_ATTR_FILTER_PRIORITY	"priority_filter"
#define XML_ATTR_VERBOSE		"verbose"
#define XML_ATTR_OP			"op"
#define XML_ATTR_DC			"is_dc"
#define XML_ATTR_DC_UUID		"dc_uuid"
#define XML_ATTR_CIB_REVISION		"cib_feature_revision"
#define XML_ATTR_CIB_REVISION_MAX	"cib_feature_revision_max"

#define XML_BOOLEAN_TRUE		"true"
#define XML_BOOLEAN_FALSE		"false"
#define XML_BOOLEAN_YES			XML_BOOLEAN_TRUE
#define XML_BOOLEAN_NO			XML_BOOLEAN_FALSE

#define XML_TAG_OPTIONS			"options"

/*---- top level tags/attrs */
#define XML_MSG_TAG			"crm_message"
#define XML_MSG_TAG_DATA		"msg_data"
#define XML_ATTR_REQUEST		"request"
#define XML_ATTR_RESPONSE		"response"

#define XML_ATTR_UNAME			"uname"
#define XML_ATTR_UUID			"id"
#define XML_ATTR_REFERENCE		"reference"

#define XML_FAIL_TAG_RESOURCE		"failed_resource"

#define XML_FAILRES_ATTR_RESID		"resource_id"
#define XML_FAILRES_ATTR_REASON		"reason"
#define XML_FAILRES_ATTR_RESSTATUS	"resource_status"

#define XML_CRM_TAG_PING		"ping_response"
#define XML_PING_ATTR_STATUS		"result"
#define XML_PING_ATTR_SYSFROM		"crm_subsystem"

#define XML_TAG_FRAGMENT		"cib_fragment"
#define XML_ATTR_RESULT			"result"
#define XML_ATTR_SECTION		"section"

#define XML_FAIL_TAG_CIB		"failed_update"

#define XML_FAILCIB_ATTR_ID		"id"
#define XML_FAILCIB_ATTR_OBJTYPE	"object_type"
#define XML_FAILCIB_ATTR_OP		"operation"
#define XML_FAILCIB_ATTR_REASON		"reason"

/*---- CIB specific tags/attrs */
#define XML_CIB_TAG_SECTION_ALL		"all"
#define XML_CIB_TAG_CONFIGURATION	"configuration"
#define XML_CIB_TAG_STATUS       	"status"
#define XML_CIB_TAG_RESOURCES		"resources"
#define XML_CIB_TAG_NODES         	"nodes"
#define XML_CIB_TAG_CONSTRAINTS   	"constraints"
#define XML_CIB_TAG_CRMCONFIG   	"crm_config"

#define XML_CIB_TAG_STATE         	"node_state"
#define XML_CIB_TAG_NODE          	"node"
#define XML_CIB_TAG_CONSTRAINT    	"constraint"
#define XML_CIB_TAG_NVPAIR        	"nvpair"

#define XML_TAG_ATTR_SETS	   	"instance_attributes"
#define XML_TAG_ATTRS			"attributes"

#define XML_CIB_TAG_RESOURCE	  	"primitive"
#define XML_CIB_TAG_GROUP	  	"group"
#define XML_CIB_TAG_INCARNATION		"clone"

#define XML_RSC_ATTR_STOPFAIL	   	"on_stopfail"
#define XML_RSC_ATTR_RESTART	  	"restart_type"
#define XML_RSC_ATTR_START_TIMEOUT	"start_timeout"
#define XML_RSC_ATTR_STOP_TIMEOUT	"stop_timeout"
#define XML_RSC_ATTR_ORDERED		"ordered"
#define XML_RSC_ATTR_INTERLEAVE		"interleave"
#define XML_RSC_ATTR_INCARNATION	"clone"
#define XML_RSC_ATTR_INCARNATION_MAX	"clone_max"
#define XML_RSC_ATTR_INCARNATION_NODEMAX	"clone_node_max"

#define XML_CIB_TAG_LRM		  	"lrm"
#define XML_LRM_TAG_RESOURCES     	"lrm_resources"
#define XML_LRM_TAG_RESOURCE     	"lrm_resource"
#define XML_LRM_TAG_AGENTS	     	"lrm_agents"
#define XML_LRM_TAG_AGENT		"lrm_agent"
#define XML_LRM_TAG_RSC_OP		"lrm_rsc_op"
#define XML_AGENT_ATTR_CLASS		"class"
#define XML_AGENT_ATTR_PROVIDER		"provider"
#define XML_LRM_TAG_ATTRIBUTES		"attributes"

#define XML_CIB_ATTR_REPLACE       	"replace"
#define XML_CIB_ATTR_SOURCE       	"source"

#define XML_CIB_ATTR_HEALTH       	"health"
#define XML_CIB_ATTR_WEIGHT       	"weight"
#define XML_CIB_ATTR_PRIORITY     	"priority"
#define XML_CIB_ATTR_CLEAR        	"clear_on"
#define XML_CIB_ATTR_SOURCE       	"source"

#define XML_CIB_ATTR_JOINSTATE    	"join"
#define XML_CIB_ATTR_EXPSTATE     	"expected"
#define XML_CIB_ATTR_INCCM        	"in_ccm"
#define XML_CIB_ATTR_CRMDSTATE    	"crmd"
#define XML_CIB_ATTR_HASTATE    	"ha"

#define XML_CIB_ATTR_SHUTDOWN       	"shutdown"
#define XML_CIB_ATTR_CLEAR_SHUTDOWN 	"clear_shutdown"
#define XML_CIB_ATTR_STONITH	    	"stonith"
#define XML_CIB_ATTR_CLEAR_STONITH  	"clear_stonith"

#define XML_LRM_ATTR_TASK		"operation"
#define XML_LRM_ATTR_TARGET		"on_node"
#define XML_LRM_ATTR_TARGET_UUID	"on_node_uuid"
#define XML_LRM_ATTR_RSCSTATE		"rsc_state"
#define XML_LRM_ATTR_RSCID		"rsc_id"
#define XML_LRM_ATTR_LASTOP		"last_op"
#define XML_LRM_ATTR_OPSTATUS		"op_status"
#define XML_LRM_ATTR_RC			"rc_code"
#define XML_LRM_ATTR_CALLID		"call_id"

#define XML_TAG_GRAPH			"transition_graph"
#define XML_GRAPH_TAG_RSC_OP		"rsc_op"
#define XML_GRAPH_TAG_PSEUDO_EVENT	"pseudo_event"
#define XML_GRAPH_TAG_CRM_EVENT		"crm_event"

#define XML_TAG_RULE			"rule"
#define XML_RULE_ATTR_SCORE		"score"
#define XML_RULE_ATTR_RESULT		"result"
#define XML_RULE_ATTR_BOOLEAN_OP	"boolean_op"

#define XML_TAG_EXPRESSION		"expression"
#define XML_EXPR_ATTR_ATTRIBUTE		"attribute"
#define XML_EXPR_ATTR_OPERATION		"operation"
#define XML_EXPR_ATTR_VALUE		"value"
#define XML_EXPR_ATTR_TYPE		"type"

#define XML_CONS_TAG_RSC_DEPEND		"rsc_colocation"
#define XML_CONS_TAG_RSC_ORDER		"rsc_order"
#define XML_CONS_TAG_RSC_LOCATION	"rsc_location"
#define XML_CONS_ATTR_FROM		"from"
#define XML_CONS_ATTR_TO		"to"
#define XML_CONS_ATTR_ACTION		"action"
#define XML_CONS_ATTR_SYMMETRICAL	"symmetrical"


#define XML_NVPAIR_ATTR_NAME        	"name"
#define XML_NVPAIR_ATTR_VALUE        	"value"

#define XML_STRENGTH_VAL_MUST		"must"
#define XML_STRENGTH_VAL_SHOULD		"should"
#define XML_STRENGTH_VAL_SHOULDNOT	"should_not"
#define XML_STRENGTH_VAL_MUSTNOT	"must_not"

#define XML_NODE_ATTR_STATE		"state"

#define XML_CONFIG_ATTR_DC_BEAT		"dc_heartbeat"
#define XML_CONFIG_ATTR_DC_DEADTIME	"dc_deadtime"
#define XML_CONFIG_ATTR_FORCE_QUIT	"shutdown_escalation"
#define XML_CONFIG_ATTR_REANNOUNCE	"join_reannouce"

#define XML_CIB_TAG_GENERATION_TUPPLE	"generation_tuple"

#include <crm/common/xml.h> 

#define ID(x) crm_element_value(x, XML_ATTR_ID)
#define INSTANCE(x) crm_element_value(x, XML_CIB_ATTR_INSTANCE)
#define TSTAMP(x) crm_element_value(x, XML_ATTR_TSTAMP)
#define TYPE(x) crm_element_name(x) 

#endif
