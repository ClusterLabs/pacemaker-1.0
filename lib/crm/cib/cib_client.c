/*
 * Copyright (c) 2004 International Business Machines
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <portability.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <glib.h>
#include <heartbeat.h>
#include <clplumbing/ipc.h>

#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>

gboolean verify_cib_cmds(cib_t *cib);

int cib_client_set_op_callback(
	cib_t *cib, void (*callback)(const struct ha_msg *msg, int call_id,
				     int rc, crm_data_t *output));
int cib_client_noop(cib_t *cib, int call_options);
int cib_client_ping(cib_t *cib, crm_data_t **output_data, int call_options);

int cib_client_query(cib_t *cib, const char *section,
	     crm_data_t **output_data, int call_options);
int cib_client_query_from(cib_t *cib, const char *host, const char *section,
			  crm_data_t **output_data, int call_options);

int cib_client_sync(cib_t *cib, const char *section, int call_options);
int cib_client_sync_from(
	cib_t *cib, const char *host, const char *section, int call_options);

gboolean cib_client_is_master(cib_t *cib);
int cib_client_set_slave(cib_t *cib, int call_options);
int cib_client_set_slave_all(cib_t *cib, int call_options);
int cib_client_set_master(cib_t *cib, int call_options);

int cib_client_bump_epoch(cib_t *cib, int call_options);
int cib_client_create(cib_t *cib, const char *section, crm_data_t *data,
	      crm_data_t **output_data, int call_options) ;
int cib_client_modify(cib_t *cib, const char *section, crm_data_t *data,
	      crm_data_t **output_data, int call_options) ;
int cib_client_replace(cib_t *cib, const char *section, crm_data_t *data,
	       crm_data_t **output_data, int call_options) ;
int cib_client_delete(cib_t *cib, const char *section, crm_data_t *data,
	      crm_data_t **output_data, int call_options) ;
int cib_client_erase(
	cib_t *cib, crm_data_t **output_data, int call_options);
int cib_client_quit(cib_t *cib,   int call_options);

int cib_client_add_notify_callback(
	cib_t *cib, const char *event, void (*callback)(
		const char *event, struct ha_msg *msg));

int cib_client_del_notify_callback(
	cib_t *cib, const char *event, void (*callback)(
		const char *event, struct ha_msg *msg));

gint ciblib_GCompareFunc(gconstpointer a, gconstpointer b);

extern cib_t *cib_native_new(cib_t *cib);

static enum cib_variant configured_variant = cib_native;

/* define of the api functions*/
cib_t*
cib_new(void)
{
	cib_t* new_cib = NULL;

	if(configured_variant != cib_native) {
		crm_err("Only the native CIB type is currently implemented");
		return NULL;
	}

	crm_malloc(new_cib, sizeof(cib_t));

	new_cib->call_id = 1;

	new_cib->type  = cib_none;
	new_cib->state = cib_disconnected;

	new_cib->op_callback	= NULL;
	new_cib->variant_opaque = NULL;
	new_cib->notify_list    = NULL;

	/* the rest will get filled in by the variant constructor */
	crm_malloc(new_cib->cmds, sizeof(cib_api_operations_t));
	memset(new_cib->cmds, 0, sizeof(cib_api_operations_t));

	new_cib->cmds->set_op_callback     = cib_client_set_op_callback;
	new_cib->cmds->add_notify_callback = cib_client_add_notify_callback;
	new_cib->cmds->del_notify_callback = cib_client_del_notify_callback;
	
	new_cib->cmds->noop    = cib_client_noop;
	new_cib->cmds->ping    = cib_client_ping;
	new_cib->cmds->query   = cib_client_query;
	new_cib->cmds->sync    = cib_client_sync;

	new_cib->cmds->query_from = cib_client_query_from;
	new_cib->cmds->sync_from  = cib_client_sync_from;
	
	new_cib->cmds->is_master  = cib_client_is_master;
	new_cib->cmds->set_master = cib_client_set_master;
	new_cib->cmds->set_slave  = cib_client_set_slave;
	new_cib->cmds->set_slave_all = cib_client_set_slave_all;

	new_cib->cmds->bump_epoch = cib_client_bump_epoch;

	new_cib->cmds->create  = cib_client_create;
	new_cib->cmds->modify  = cib_client_modify;
	new_cib->cmds->replace = cib_client_replace;
	new_cib->cmds->delete  = cib_client_delete;
	new_cib->cmds->erase   = cib_client_erase;
	new_cib->cmds->quit    = cib_client_quit;

	cib_native_new(new_cib);
	if(verify_cib_cmds(new_cib) == FALSE) {
		return NULL;
	}
	
	return new_cib;
}

int
cib_client_set_op_callback(
	cib_t *cib, void (*callback)(const struct ha_msg *msg, int call_id,
				     int rc, crm_data_t *output)) 
{
	if(callback == NULL) {
		crm_info("Un-Setting operation callback");
		
	} else {
		crm_debug("Setting operation callback");
	}
	cib->op_callback = callback;
	return cib_ok;
}
	
int cib_client_noop(cib_t *cib, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	}
	
	return cib->cmds->variant_op(
		cib, CRM_OP_NOOP, NULL, NULL, NULL, NULL, call_options);
}

int cib_client_ping(cib_t *cib, crm_data_t **output_data, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	}
	
	return cib->cmds->variant_op(
		cib, CRM_OP_PING, NULL,NULL,NULL, output_data, call_options);
}


int cib_client_query(cib_t *cib, const char *section,
		     crm_data_t **output_data, int call_options)
{
	return cib->cmds->query_from(
		cib, NULL, section, output_data, call_options);
}

int cib_client_query_from(cib_t *cib, const char *host, const char *section,
			  crm_data_t **output_data, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	}
	
	return cib->cmds->variant_op(cib, CRM_OP_CIB_QUERY, host, section,
				     NULL, output_data, call_options);
}


gboolean cib_client_is_master(cib_t *cib)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(
		cib, CRM_OP_CIB_ISMASTER, NULL, NULL,NULL,NULL,
		cib_scope_local|cib_sync_call);
}

int cib_client_set_slave(cib_t *cib, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(
		cib, CRM_OP_CIB_SLAVE, NULL,NULL,NULL,NULL, call_options);
}

int cib_client_set_slave_all(cib_t *cib, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(
		cib, CRM_OP_CIB_SLAVEALL, NULL,NULL,NULL,NULL, call_options);
}

int cib_client_set_master(cib_t *cib, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	crm_debug("Adding cib_scope_local to options");
	return cib->cmds->variant_op(
		cib, CRM_OP_CIB_MASTER, NULL,NULL,NULL,NULL,
		call_options|cib_scope_local);
}



int cib_client_bump_epoch(cib_t *cib, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(
		cib, CRM_OP_CIB_BUMP, NULL, NULL, NULL, NULL, call_options);
}

int cib_client_sync(cib_t *cib, const char *section, int call_options)
{
	return cib->cmds->sync_from(cib, NULL, section, call_options);
}

int cib_client_sync_from(
	cib_t *cib, const char *host, const char *section, int call_options)
{
	enum cib_errors rc = cib_ok;
	crm_data_t *stored_cib  = NULL;
	crm_data_t *current_cib = NULL;

	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	}

	crm_debug("Retrieving current CIB from %s", host);
	rc = cib->cmds->query_from(
		cib, host, section, &current_cib, call_options|cib_sync_call);

	if(current_cib == NULL) {
		crm_err("Could not retrive current CIB.");
		
	} else if(rc == cib_ok) {
		if(call_options & cib_scope_local) {
			/* having scope == local makes no sense from here on */
			call_options ^= cib_scope_local;
		}
		
		crm_debug("Storing current CIB (should trigger a store everywhere)");
		crm_xml_debug(current_cib, "XML to store");
		rc = cib->cmds->replace(
			cib, section, current_cib, &stored_cib, call_options);
	}
	free_xml(current_cib);
	free_xml(stored_cib);
	
	return rc;
	
}

int cib_client_create(cib_t *cib, const char *section, crm_data_t *data,
		      crm_data_t **output_data, int call_options) 
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(cib, CRM_OP_CIB_CREATE, NULL, section,
				     data, output_data, call_options);
}


int cib_client_modify(cib_t *cib, const char *section, crm_data_t *data,
	   crm_data_t **output_data, int call_options) 
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(cib, CRM_OP_CIB_UPDATE, NULL, section,
				     data, output_data, call_options);
}


int cib_client_replace(cib_t *cib, const char *section, crm_data_t *data,
	    crm_data_t **output_data, int call_options) 
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} else if(data == NULL) {
		return cib_missing_data;
	}
	
	return cib->cmds->variant_op(cib, CRM_OP_CIB_REPLACE, NULL, section,
				     data, output_data, call_options);
}


int cib_client_delete(cib_t *cib, const char *section, crm_data_t *data,
	   crm_data_t **output_data, int call_options) 
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	}
	
	return cib->cmds->variant_op(cib, CRM_OP_CIB_DELETE, NULL, section,
				     data, output_data, call_options);
}


int cib_client_erase(
	cib_t *cib, crm_data_t **output_data, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(cib, CRM_OP_CIB_ERASE, NULL, NULL, NULL,
				     output_data, call_options);
}


int cib_client_quit(cib_t *cib, int call_options)
{
	if(cib == NULL) {
		return cib_missing;
	} else if(cib->state == cib_disconnected) {
		return cib_not_connected;
	} else if(cib->cmds->variant_op == NULL) {
		return cib_variant;
	} 

	return cib->cmds->variant_op(
		cib, CRM_OP_QUIT, NULL, NULL, NULL, NULL, call_options);
}

int cib_client_add_notify_callback(
	cib_t *cib, const char *event, void (*callback)(
		const char *event, struct ha_msg *msg))
{
	GList *list_item = NULL;
	cib_notify_client_t *new_client = NULL;
	
	crm_debug("Adding callback for %s events (%d)",
		  event, g_list_length(cib->notify_list));

	crm_malloc(new_client, sizeof(cib_notify_client_t));
	new_client->event = event;
	new_client->callback = callback;

	list_item = g_list_find_custom(
		cib->notify_list, new_client, ciblib_GCompareFunc);
	
	if(list_item != NULL) {
		crm_warn("Callback already present");

	} else {
		cib->notify_list = g_list_append(
			cib->notify_list, new_client);

		crm_debug("Callback added (%d)", g_list_length(cib->notify_list));
	}
	return cib_ok;
}


int cib_client_del_notify_callback(
	cib_t *cib, const char *event, void (*callback)(
		const char *event, struct ha_msg *msg))
{
	GList *list_item = NULL;
	cib_notify_client_t *new_client = NULL;

	crm_debug("Removing callback for %s events", event);

	crm_malloc(new_client, sizeof(cib_notify_client_t));
	new_client->event = event;
	new_client->callback = callback;

	list_item = g_list_find_custom(
		cib->notify_list, new_client, ciblib_GCompareFunc);
	
	if(list_item != NULL) {
		cib_notify_client_t *list_client = list_item->data;
		cib->notify_list =
			g_list_remove(cib->notify_list, list_client);
		crm_free(list_client);
		crm_debug("Removed callback");

	} else {
		crm_debug("Callback not present");
	}
	crm_free(new_client);
	return cib_ok;
}

gint ciblib_GCompareFunc(gconstpointer a, gconstpointer b)
{
	const cib_notify_client_t *a_client = a;
	const cib_notify_client_t *b_client = b;
	if(a_client->callback == b_client->callback
	   && safe_str_neq(a_client->event, b_client->event)) {
		return 0;
	} else if(((long)a_client->callback) < ((long)b_client->callback)) {
		return -1;
	}
	return 1;
}


char *
cib_pluralSection(const char *a_section)
{
	char *a_section_parent = NULL;
	if (a_section == NULL) {
		a_section_parent = crm_strdup("all");

	} else if(strcmp(a_section, XML_TAG_CIB) == 0) {
		a_section_parent = crm_strdup("all");

	} else if(strcmp(a_section, XML_CIB_TAG_NODE) == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_NODES);

	} else if(strcmp(a_section, XML_CIB_TAG_STATE) == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_STATUS);

	} else if(strcmp(a_section, XML_CIB_TAG_CONSTRAINT) == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_CONSTRAINTS);
		
	} else if(strcmp(a_section, "rsc_location") == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_CONSTRAINTS);
		
	} else if(strcmp(a_section, "rsc_dependancy") == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_CONSTRAINTS);
		
	} else if(strcmp(a_section, "rsc_order") == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_CONSTRAINTS);
		
	} else if(strcmp(a_section, XML_CIB_TAG_RESOURCE) == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_RESOURCES);

	} else if(strcmp(a_section, XML_CIB_TAG_NVPAIR) == 0) {
		a_section_parent = crm_strdup(XML_CIB_TAG_CRMCONFIG);

	} else {
		crm_err("Unknown section %s", a_section);
		a_section_parent = crm_strdup("all");
	}
	
	crm_verbose("Plural of %s is %s", crm_str(a_section), a_section_parent);

	return a_section_parent;
}

const char *
cib_error2string(enum cib_errors return_code)
{
	const char *error_msg = NULL;
	switch(return_code) {
		case cib_msg_field_add:
			error_msg = "failed adding field to cib message";
			break;			
		case cib_operation:
			error_msg = "invalid operation";
			break;
		case cib_create_msg:
			error_msg = "couldnt create cib message";
			break;
		case cib_client_gone:
			error_msg = "client left before we could send reply";
			break;
		case cib_not_connected:
			error_msg = "not connected";
			break;
		case cib_not_authorized:
			error_msg = "not authorized";
			break;
		case cib_send_failed:
			error_msg = "send failed";
			break;
		case cib_reply_failed:
			error_msg = "reply failed";
			break;
		case cib_return_code:
			error_msg = "no return code";
			break;
		case cib_output_ptr:
			error_msg = "nowhere to store output";
			break;
		case cib_output_data:
			error_msg = "corrupt output data";
			break;
		case cib_connection:
			error_msg = "connection failed";
			break;
		case cib_callback_register:
			error_msg = "couldnt register callback channel";
			break;
		case cib_authentication:
			error_msg = "";
			break;
		case cib_registration_msg:
			error_msg = "invalid registration msg";
			break;
		case cib_callback_token:
			error_msg = "callback token not found";
			break;
		case cib_missing:
			error_msg = "cib object missing";
			break;
		case cib_variant:
			error_msg = "unknown/corrupt cib variant";
			break;
		case CIBRES_MISSING_ID:
			error_msg = "The id field is missing";
			break;
		case CIBRES_MISSING_TYPE:
			error_msg = "The type field is missing";
			break;
		case CIBRES_MISSING_FIELD:
			error_msg = "A required field is missing";
			break;
		case CIBRES_OBJTYPE_MISMATCH:
			error_msg = "CIBRES_OBJTYPE_MISMATCH";
			break;
		case cib_EXISTS:
			error_msg = "The object already exists";
			break;
		case cib_NOTEXISTS:
			error_msg = "The object does not exist";
			break;
		case CIBRES_CORRUPT:
			error_msg = "The CIB is corrupt";
			break;
		case cib_NOOBJECT:
			error_msg = "The update was empty";
			break;
		case cib_NOPARENT:
			error_msg = "The parent object does not exist";
			break;
		case cib_NODECOPY:
			error_msg = "Failed while copying update";
			break;
		case CIBRES_OTHER:
			error_msg = "CIBRES_OTHER";
			break;
		case cib_ok:
			error_msg = "ok";
			break;
		case cib_unknown:
			error_msg = "Unknown error";
			break;
		case cib_STALE:
			error_msg = "Discarded old update";
			break;
		case cib_ACTIVATION:
			error_msg = "Activation Failed";
			break;
		case cib_NOSECTION:
			error_msg = "Required section was missing";
			break;
		case cib_NOTSUPPORTED:
			error_msg = "Supplied information is not supported";
			break;
		case cib_not_master:
			error_msg = "Local service is not the master instance";
			break;
		case cib_client_corrupt:
			error_msg = "Service client not valid";
			break;
		case cib_master_timeout:
			error_msg = "No master service is currently active";
			break;
		case cib_revision_unsupported:
			error_msg = "The required CIB revision number is not supported";
			break;
		case cib_revision_unknown:
			error_msg = "The CIB revision number could not be determined";
			break;
		case cib_missing_data:
			error_msg = "Required data for this CIB API call not found";
			break;
	}
			
	if(error_msg == NULL) {
		crm_err("Unknown CIB Error Code: %d", return_code);
		error_msg = "<unknown error>";
	}
	
	return error_msg;
}

const char *
cib_op2string(enum cib_op operation)
{
	const char *operation_msg = NULL;
	switch(operation) {
		case 0:
			operation_msg = "none";
			break;
		case 1:
			operation_msg = "add";
			break;
		case 2:
			operation_msg = "modify";
			break;
		case 3:
			operation_msg = "delete";
			break;
		case CIB_OP_MAX:
			operation_msg = "invalid operation";
			break;
			
	}

	if(operation_msg == NULL) {
		crm_err("Unknown CIB operation %d", operation);
		operation_msg = "<unknown operation>";
	}
	
	return operation_msg;
}




int
cib_section2enum(const char *a_section) 
{
	if(a_section == NULL || strcmp(a_section, "all") == 0) {
		return cib_section_all;

	} else if(strcmp(a_section, XML_CIB_TAG_NODES) == 0) {
		return cib_section_nodes;

	} else if(strcmp(a_section, XML_CIB_TAG_STATUS) == 0) {
		return cib_section_status;

	} else if(strcmp(a_section, XML_CIB_TAG_CONSTRAINTS) == 0) {
		return cib_section_constraints;
		
	} else if(strcmp(a_section, XML_CIB_TAG_RESOURCES) == 0) {
		return cib_section_resources;

	} else if(strcmp(a_section, XML_CIB_TAG_CRMCONFIG) == 0) {
		return cib_section_crmconfig;

	}
	crm_err("Unknown CIB section: %s", a_section);
	return cib_section_none;
}


int
cib_compare_generation(crm_data_t *left, crm_data_t *right)
{
	int int_elem_l = -1;
	int int_elem_r = -1;
	const char *elem_l = crm_element_value(left, XML_ATTR_GENERATION);
	const char *elem_r = NULL;

	crm_xml_trace(left, "left");
	if(right != NULL) {
		elem_r = crm_element_value(right, XML_ATTR_GENERATION);
		crm_xml_trace(right, "right");
	}
	
	if(elem_l != NULL) int_elem_l = atoi(elem_l);
	if(elem_r != NULL) int_elem_r = atoi(elem_r);
	
	if(int_elem_l < int_elem_r) {
		crm_verbose("lt - XML_ATTR_GENERATION");
		return -1;
		
	} else if(int_elem_l > int_elem_r) {
		crm_verbose("gt - XML_ATTR_GENERATION");
		return 1;
	}

	int_elem_l = -1;
	int_elem_r = -1;
	elem_l = crm_element_value(left, XML_ATTR_NUMUPDATES);
	if(right != NULL) {
		elem_r = crm_element_value(right, XML_ATTR_NUMUPDATES);
	}
	
	if(elem_l != NULL) int_elem_l = atoi(elem_l);
	if(elem_r != NULL) int_elem_r = atoi(elem_r);

	crm_xml_trace(left, "left");
	crm_xml_trace(left, "right");
	
	if(int_elem_l < int_elem_r) {
		crm_verbose("lt - XML_ATTR_NUMUPDATES");
		return -1;
		
	} else if(int_elem_l > int_elem_r) {
		crm_verbose("gt - XML_ATTR_NUMUPDATES");
		return 1;
	}
	
	crm_verbose("eq - XML_ATTR_NUMUPDATES");

	int_elem_l = -1;
	int_elem_r = -1;
	elem_l = crm_element_value(left, XML_ATTR_NUMPEERS);
	if(right != NULL) {
		elem_r = crm_element_value(right, XML_ATTR_NUMPEERS);
	}
	
	if(elem_l != NULL) int_elem_l = atoi(elem_l);
	if(elem_r != NULL) int_elem_r = atoi(elem_r);

	if(int_elem_l < int_elem_r) {
		crm_verbose("lt - XML_ATTR_NUMPEERS");
		return -1;
		
	} else if(int_elem_l > int_elem_r) {
		crm_verbose("gt - XML_ATTR_NUMPEERS");
		return 1;
	}
	
	crm_verbose("eq - XML_ATTR_NUMPEERS");

	elem_l = crm_element_value(left, XML_ATTR_NUMPEERS);
	if(right != NULL) {
		elem_r = crm_element_value(right, XML_ATTR_NUMPEERS);
	}
	
	if(elem_l == NULL && elem_r == NULL) {

	} else if(elem_l == NULL) {
		crm_verbose("lt - XML_ATTR_NUMPEERS");
		return -1;

	} else if(elem_r == NULL) {
		crm_verbose("gt - XML_ATTR_NUMPEERS");
		return 1;

	} else if(safe_str_neq(elem_l, elem_r)) {

		if(safe_str_eq(elem_l, XML_BOOLEAN_TRUE)) {
			crm_verbose("gt - XML_ATTR_NUMPEERS");
			return 1;
			
		} else if(safe_str_eq(elem_r, XML_BOOLEAN_TRUE)) {
			crm_verbose("lt - XML_ATTR_NUMPEERS");
			return -1;
		}
	}
	
	crm_verbose("eq - XML_ATTR_NUMPEERS");
	return 0;
}


crm_data_t*
get_cib_copy(cib_t *cib)
{
	crm_data_t *xml_cib;
	int options = cib_scope_local|cib_sync_call;
	if(cib->cmds->query(cib, NULL, &xml_cib, options) != cib_ok) {
		crm_err("Couldnt retrieve the CIB");
		return NULL;
	} else if(xml_cib == NULL) {
		crm_err("The CIB result was empty");
		return NULL;
	}
	return find_xml_node(xml_cib, XML_TAG_CIB, TRUE);
}

crm_data_t*
cib_get_generation(cib_t *cib)
{
	crm_data_t *the_cib = get_cib_copy(cib);
	crm_data_t *generation = create_xml_node(
		NULL, XML_CIB_TAG_GENERATION_TUPPLE);

	if(the_cib != NULL) {
		copy_in_properties(generation, the_cib);
		free_xml(the_cib);
	}
	
	return generation;
}

/*
 * The caller should never free the return value
 */
crm_data_t*
get_object_root(const char *object_type, crm_data_t *the_root)
{
	const char *node_stack[2];
	crm_data_t *tmp_node = NULL;
	
	if(the_root == NULL) {
		crm_err("CIB root object was NULL");
		return NULL;
	}
	
	node_stack[0] = XML_CIB_TAG_CONFIGURATION;
	node_stack[1] = object_type;

	if(object_type == NULL
	   || strlen(object_type) == 0
	   || safe_str_eq("all", object_type)
	   || safe_str_eq(XML_TAG_CIB, object_type)) {
		/* get the whole cib */
		return the_root;

	} else if(strcmp(object_type, XML_CIB_TAG_STATUS) == 0) {
		/* these live in a different place */
		tmp_node = find_xml_node(the_root, XML_CIB_TAG_STATUS, TRUE);

		node_stack[0] = XML_CIB_TAG_STATUS;
		node_stack[1] = NULL;

	} else {
		tmp_node = find_xml_node_nested(the_root, node_stack, 2);
	}

	if (tmp_node == NULL) {
		crm_err("Section [%s [%s]] not present in %s",
			node_stack[0],
			node_stack[1]?node_stack[1]:"",
			crm_element_name(the_root));
	}
	return tmp_node;
}


crm_data_t*
create_cib_fragment_adv(
	crm_data_t *update, const char *section, const char *source)
{
	crm_data_t *cib = NULL;
	gboolean whole_cib = FALSE;
	crm_data_t *fragment = create_xml_node(NULL, XML_TAG_FRAGMENT);
	crm_data_t *object_root  = NULL;
	char *auto_section = NULL;
	const char *update_name = NULL;
	if(update != NULL) {
		update_name = crm_element_name(update);
	}
	auto_section = cib_pluralSection(update_name);
	
	if(update == NULL && section == NULL) {
		crm_debug("Creating a blank fragment");
		update = createEmptyCib();
		section = auto_section;

	} else if(update == NULL) {
		crm_err("No update to create a fragment for");
		cib = createEmptyCib();
		return NULL;
		
	} else if(section == NULL) {
		section = auto_section;

	} else if(strcmp(auto_section, section) != 0) {
		crm_err("Values for update (tag=%s) and section (%s)"
			" were not consistent", crm_element_name(update), section);
		crm_free(auto_section);
		return NULL;
		
	}

	if(safe_str_eq(section, "all")
	   && safe_str_eq(crm_element_name(update), XML_TAG_CIB)) {
		whole_cib = TRUE;
	}
	
	set_xml_property_copy(fragment, XML_ATTR_SECTION, section);

	if(whole_cib == FALSE) {
		cib = createEmptyCib();
		set_xml_property_copy(cib, "debug_source", source);
		object_root = get_object_root(section, cib);
		add_node_copy(object_root, update);
		add_node_copy(fragment, cib);
		free_xml(cib);
		cib = find_xml_node(fragment, XML_TAG_CIB, TRUE);

	} else {
		add_node_copy(fragment, update);
		cib = find_xml_node(fragment, XML_TAG_CIB, TRUE);
		set_xml_property_copy(cib, "debug_source", source);
	}
	
	crm_free(auto_section);

	crm_debug("Verifying created fragment");
	if(verifyCibXml(cib) == FALSE) {
		crm_err("Fragment creation failed");
		crm_xml_err(update, "[src]");
		crm_xml_err(fragment, "[created]");
		free_xml(fragment);
		fragment = NULL;
	}
	
	return fragment;
}

/*
 * It is the callers responsibility to free both the new CIB (output)
 *     and the new CIB (input)
 */
crm_data_t*
createEmptyCib(void)
{
	crm_data_t *cib_root = NULL, *config = NULL, *status = NULL;
	
	cib_root = create_xml_node(NULL, XML_TAG_CIB);

	config = create_xml_node(cib_root, XML_CIB_TAG_CONFIGURATION);
	status = create_xml_node(cib_root, XML_CIB_TAG_STATUS);

	set_node_tstamp(cib_root);
	set_node_tstamp(config);
	set_node_tstamp(status);
	
/* 	set_xml_property_copy(cib_root, "version", "1"); */
	set_xml_property_copy(cib_root, "generated", XML_BOOLEAN_TRUE);
	set_xml_property_copy(
		cib_root, XML_ATTR_CIB_REVISION, cib_feature_revision_s);

	create_xml_node(config, XML_CIB_TAG_CRMCONFIG);
	create_xml_node(config, XML_CIB_TAG_NODES);
	create_xml_node(config, XML_CIB_TAG_RESOURCES);
	create_xml_node(config, XML_CIB_TAG_CONSTRAINTS);
	
	if (verifyCibXml(cib_root)) {
		return cib_root;
	}

	free_xml(cib_root);
	crm_crit("The generated CIB did not pass integrity testing!!"
		 "  All hope is lost.");
	return NULL;
}


gboolean
verifyCibXml(crm_data_t *cib)
{
	gboolean is_valid = TRUE;
	crm_data_t *tmp_node = NULL;
	
	if (cib == NULL) {
		crm_warn("CIB was empty.");
		return FALSE;
	}
	
	tmp_node = get_object_root(XML_CIB_TAG_NODES, cib);
	if (tmp_node == NULL) is_valid = FALSE;

	tmp_node = get_object_root(XML_CIB_TAG_RESOURCES, cib);
	if (tmp_node == NULL) is_valid = FALSE;

	tmp_node = get_object_root(XML_CIB_TAG_CONSTRAINTS, cib);
	if (tmp_node == NULL) is_valid = FALSE;

	tmp_node = get_object_root(XML_CIB_TAG_STATUS, cib);
 	if (tmp_node == NULL) is_valid = FALSE;

	tmp_node = get_object_root(XML_CIB_TAG_CRMCONFIG, cib);
 	if (tmp_node == NULL) is_valid = FALSE;

	/* more integrity tests */

	return is_valid;
}


gboolean verify_cib_cmds(cib_t *cib) 
{
	gboolean valid = TRUE;
	if(cib->cmds->variant_op == NULL) {
		crm_err("Operation variant_op not set");
		valid = FALSE;
	}	
	if(cib->cmds->signon == NULL) {
		crm_err("Operation signon not set");
		valid = FALSE;
	}
	if(cib->cmds->signoff == NULL) {
		crm_err("Operation signoff not set");
		valid = FALSE;
	}
	if(cib->cmds->free == NULL) {
		crm_err("Operation free not set");
		valid = FALSE;
	}
	if(cib->cmds->set_op_callback == NULL) {
		crm_err("Operation set_op_callback not set");
		valid = FALSE;
	}
	if(cib->cmds->add_notify_callback == NULL) {
		crm_err("Operation add_notify_callback not set");
		valid = FALSE;
	}
	if(cib->cmds->del_notify_callback == NULL) {
		crm_err("Operation del_notify_callback not set");
		valid = FALSE;
	}
	if(cib->cmds->set_connection_dnotify == NULL) {
		crm_err("Operation set_connection_dnotify not set");
		valid = FALSE;
	}
	if(cib->cmds->channel == NULL) {
		crm_err("Operation channel not set");
		valid = FALSE;
	}
	if(cib->cmds->inputfd == NULL) {
		crm_err("Operation inputfd not set");
		valid = FALSE;
	}
	if(cib->cmds->noop == NULL) {
		crm_err("Operation noop not set");
		valid = FALSE;
	}
	if(cib->cmds->ping == NULL) {
		crm_err("Operation ping not set");
		valid = FALSE;
	}
	if(cib->cmds->query == NULL) {
		crm_err("Operation query not set");
		valid = FALSE;
	}
	if(cib->cmds->query_from == NULL) {
		crm_err("Operation query_from not set");
		valid = FALSE;
	}
	if(cib->cmds->is_master == NULL) {
		crm_err("Operation is_master not set");
		valid = FALSE;
	}
	if(cib->cmds->set_master == NULL) {
		crm_err("Operation set_master not set");
		valid = FALSE;
	}
	if(cib->cmds->set_slave == NULL) {
		crm_err("Operation set_slave not set");
		valid = FALSE;
	}		
	if(cib->cmds->set_slave_all == NULL) {
		crm_err("Operation set_slave_all not set");
		valid = FALSE;
	}		
	if(cib->cmds->sync == NULL) {
		crm_err("Operation sync not set");
		valid = FALSE;
	}		if(cib->cmds->sync_from == NULL) {
		crm_err("Operation sync_from not set");
		valid = FALSE;
	}
	if(cib->cmds->bump_epoch == NULL) {
		crm_err("Operation bump_epoch not set");
		valid = FALSE;
	}		
	if(cib->cmds->create == NULL) {
		crm_err("Operation create not set");
		valid = FALSE;
	}
	if(cib->cmds->modify == NULL) {
		crm_err("Operation modify not set");
		valid = FALSE;
	}
	if(cib->cmds->replace == NULL) {
		crm_err("Operation replace not set");
		valid = FALSE;
	}
	if(cib->cmds->delete == NULL) {
		crm_err("Operation delete not set");
		valid = FALSE;
	}
	if(cib->cmds->erase == NULL) {
		crm_err("Operation erase not set");
		valid = FALSE;
	}
	if(cib->cmds->quit == NULL) {
		crm_err("Operation quit not set");
		valid = FALSE;
	}
	
	if(cib->cmds->msgready == NULL) {
		crm_err("Operation msgready not set");
		valid = FALSE;
	}
	if(cib->cmds->rcvmsg == NULL) {
		crm_err("Operation rcvmsg not set");
		valid = FALSE;
	}
	if(cib->cmds->dispatch == NULL) {
		crm_err("Operation dispatch not set");
		valid = FALSE;
	}

	return valid;
}
