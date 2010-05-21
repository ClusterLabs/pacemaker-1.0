/* 
 * Copyright (C) 2008 Andrew Beekhof <andrew@beekhof.net>
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

#include <crm_internal.h>

#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/ipc.h>
#include <crm/common/cluster.h>

#include <crm/common/xml.h>
#include <crm/common/msg.h>

#include <cibio.h>
#include <callbacks.h>
#include <cibmessages.h>
#include "common.h"

extern gboolean cib_is_master;
extern const char* cib_root;
gboolean stand_alone = FALSE;
extern enum cib_errors cib_status;
extern gboolean can_write(int flags);
extern enum cib_errors cib_perform_command(
    xmlNode *request, xmlNode **reply, xmlNode **cib_diff, gboolean privileged);

static xmlNode *
cib_prepare_common(xmlNode *root, const char *section)
{
    xmlNode *data = NULL;
	
    /* extract the CIB from the fragment */
    if(root == NULL) {
	return NULL;

    } else if(safe_str_eq(crm_element_name(root), XML_TAG_FRAGMENT)
	      || safe_str_eq(crm_element_name(root), F_CRM_DATA)
	      || safe_str_eq(crm_element_name(root), F_CIB_CALLDATA)) {
	data = first_named_child(root, XML_TAG_CIB);

    } else {
	data = root;
    }

    /* grab the section specified for the command */
    if(section != NULL
       && data != NULL
       && crm_str_eq(crm_element_name(data), XML_TAG_CIB, TRUE)){
	data = get_object_root(section, data);
    }

    /* crm_log_xml_debug_4(root, "cib:input"); */
    return data;
}

static enum cib_errors
cib_prepare_none(xmlNode *request, xmlNode **data, const char **section)
{
    *data = NULL;
    *section = crm_element_value(request, F_CIB_SECTION);
    return cib_ok;
}

static enum cib_errors
cib_prepare_data(xmlNode *request, xmlNode **data, const char **section)
{
    xmlNode *input_fragment = get_message_xml(request, F_CIB_CALLDATA);
    *section = crm_element_value(request, F_CIB_SECTION);
    *data = cib_prepare_common(input_fragment, *section);
    /* crm_log_xml_debug(*data, "data"); */
    return cib_ok;
}

static enum cib_errors
cib_prepare_sync(xmlNode *request, xmlNode **data, const char **section)
{
    *data = NULL;
    *section = crm_element_value(request, F_CIB_SECTION);
    return cib_ok;
}

static enum cib_errors
cib_prepare_diff(xmlNode *request, xmlNode **data, const char **section)
{
    xmlNode *input_fragment = NULL;
    const char *update = crm_element_value(request, F_CIB_GLOBAL_UPDATE);

    *data = NULL;
    *section = NULL;

    if(crm_is_true(update)) {
	input_fragment = get_message_xml(request,F_CIB_UPDATE_DIFF);
		
    } else {
	input_fragment = get_message_xml(request, F_CIB_CALLDATA);
    }

    CRM_CHECK_AND_STORE(input_fragment != NULL,crm_log_xml(LOG_WARNING, "no input", request));
    *data = cib_prepare_common(input_fragment, NULL);
    return cib_ok;
}

static enum cib_errors
cib_cleanup_query(int options, xmlNode **data, xmlNode **output) 
{
    CRM_DEV_ASSERT(*data == NULL);
    if((options & cib_no_children)
       || safe_str_eq(crm_element_name(*output), "xpath-query")) {
	free_xml(*output);
    }
    return cib_ok;
}

static enum cib_errors
cib_cleanup_data(int options, xmlNode **data, xmlNode **output) 
{
    free_xml(*output);
    *data = NULL;
    return cib_ok;
}

static enum cib_errors
cib_cleanup_output(int options, xmlNode **data, xmlNode **output) 
{
    free_xml(*output);
    return cib_ok;
}

static enum cib_errors
cib_cleanup_none(int options, xmlNode **data, xmlNode **output) 
{
    CRM_DEV_ASSERT(*data == NULL);
    CRM_DEV_ASSERT(*output == NULL);
    return cib_ok;
}

static enum cib_errors
cib_cleanup_sync(int options, xmlNode **data, xmlNode **output) 
{
    /* data is non-NULL but doesnt need to be free'd */
    CRM_DEV_ASSERT(*data == NULL);
    CRM_DEV_ASSERT(*output == NULL);
    return cib_ok;
}

/*
  typedef struct cib_operation_s
  {
  const char* 	operation;
  gboolean	modifies_cib;
  gboolean	needs_privileges;
  gboolean	needs_quorum;
  enum cib_errors (*prepare)(xmlNode *, xmlNode**, const char **);
  enum cib_errors (*cleanup)(xmlNode**, xmlNode**);
  enum cib_errors (*fn)(
  const char *, int, const char *,
  xmlNode*, xmlNode*, xmlNode**, xmlNode**);
  } cib_operation_t;
*/
/* technically bump does modify the cib...
 * but we want to split the "bump" from the "sync"
 */
static cib_operation_t cib_server_ops[] = {
    {NULL,             FALSE, FALSE, FALSE, cib_prepare_none, cib_cleanup_none,   cib_process_default},
    {CIB_OP_QUERY,     FALSE, FALSE, FALSE, cib_prepare_none, cib_cleanup_query,  cib_process_query},
    {CIB_OP_MODIFY,    TRUE,  TRUE,  TRUE,  cib_prepare_data, cib_cleanup_data,   cib_process_modify},
    {CIB_OP_APPLY_DIFF,TRUE,  TRUE,  TRUE,  cib_prepare_diff, cib_cleanup_data,   cib_server_process_diff},
    {CIB_OP_REPLACE,   TRUE,  TRUE,  TRUE,  cib_prepare_data, cib_cleanup_data,   cib_process_replace_svr},
    {CIB_OP_CREATE,    TRUE,  TRUE,  TRUE,  cib_prepare_data, cib_cleanup_data,   cib_process_create},
    {CIB_OP_DELETE,    TRUE,  TRUE,  TRUE,  cib_prepare_data, cib_cleanup_data,   cib_process_delete},
    {CIB_OP_SYNC,      FALSE, TRUE,  FALSE, cib_prepare_sync, cib_cleanup_sync,   cib_process_sync},
    {CIB_OP_BUMP,      TRUE,  TRUE,  TRUE,  cib_prepare_none, cib_cleanup_output, cib_process_bump},
    {CIB_OP_ERASE,     TRUE,  TRUE,  TRUE,  cib_prepare_none, cib_cleanup_output, cib_process_erase},
    {CRM_OP_NOOP,      FALSE, FALSE, FALSE, cib_prepare_none, cib_cleanup_none,   cib_process_default},
    {CIB_OP_DELETE_ALT,TRUE,  TRUE,  TRUE,  cib_prepare_data, cib_cleanup_data,   cib_process_delete_absolute},
    {CIB_OP_UPGRADE,   TRUE,  TRUE,  TRUE,  cib_prepare_none, cib_cleanup_output, cib_process_upgrade},
    {CIB_OP_SLAVE,     FALSE, TRUE,  FALSE, cib_prepare_none, cib_cleanup_none,   cib_process_readwrite},
    {CIB_OP_SLAVEALL,  FALSE, TRUE,  FALSE, cib_prepare_none, cib_cleanup_none,   cib_process_readwrite},
    {CIB_OP_SYNC_ONE,  FALSE, TRUE,  FALSE, cib_prepare_sync, cib_cleanup_sync,   cib_process_sync_one},
    {CIB_OP_MASTER,    TRUE,  TRUE,  FALSE, cib_prepare_data, cib_cleanup_data,   cib_process_readwrite},
    {CIB_OP_ISMASTER,  FALSE, TRUE,  FALSE, cib_prepare_none, cib_cleanup_none,   cib_process_readwrite},
    {"cib_shutdown_req",FALSE, TRUE, FALSE, cib_prepare_sync, cib_cleanup_sync,   cib_process_shutdown_req},
    {CRM_OP_QUIT,      FALSE, TRUE,  FALSE, cib_prepare_none, cib_cleanup_none,   cib_process_quit},
    {CRM_OP_PING,      FALSE, FALSE, FALSE, cib_prepare_none, cib_cleanup_output, cib_process_ping},
};

enum cib_errors
cib_get_operation_id(const char *op, int *operation) 
{
    int lpc = 0;
    static int max_msg_types = DIMOF(cib_server_ops);

    if(op != NULL) {
	for (lpc = 1; lpc < max_msg_types; lpc++) {
	    if (strcmp(op, cib_server_ops[lpc].operation) == 0) {
		*operation = lpc;
		return cib_ok;
	    }
	}
    }
    crm_err("Operation %s is not valid", op);
    *operation = -1;
    return cib_operation;
}

xmlNode *
cib_msg_copy(xmlNode *msg, gboolean with_data) 
{
	int lpc = 0;
	const char *field = NULL;
	const char *value = NULL;
	xmlNode *value_struct = NULL;

	static const char *field_list[] = {
		F_XML_TAGNAME	,
		F_TYPE		,
		F_CIB_CLIENTID  ,
		F_CIB_CALLOPTS  ,
		F_CIB_CALLID    ,
		F_CIB_OPERATION ,
		F_CIB_ISREPLY   ,
		F_CIB_SECTION   ,
		F_CIB_HOST	,
		F_CIB_RC	,
		F_CIB_DELEGATED	,
		F_CIB_OBJID	,
		F_CIB_OBJTYPE	,
		F_CIB_EXISTING	,
		F_CIB_SEENCOUNT	,
		F_CIB_TIMEOUT	,
		F_CIB_CALLBACK_TOKEN	,
		F_CIB_GLOBAL_UPDATE	,
		F_CIB_CLIENTNAME	,
		F_CIB_NOTIFY_TYPE	,
		F_CIB_NOTIFY_ACTIVATE
	};
	
	static const char *data_list[] = {
		F_CIB_CALLDATA  ,
		F_CIB_UPDATE	,
		F_CIB_UPDATE_RESULT
	};

	xmlNode *copy = create_xml_node(NULL, "copy");
	CRM_ASSERT(copy != NULL);
	
	for(lpc = 0; lpc < DIMOF(field_list); lpc++) {
		field = field_list[lpc];
		value = crm_element_value(msg, field);
		if(value != NULL) {
			crm_xml_add(copy, field, value);
		}
	}
	for(lpc = 0; with_data && lpc < DIMOF(data_list); lpc++) {
		field = data_list[lpc];
		value_struct = get_message_xml(msg, field);
		if(value_struct != NULL) {
			add_message_xml(copy, field, value_struct);
		}
	}

	return copy;
}

cib_op_t *cib_op_func(int call_type) 
{
    return &(cib_server_ops[call_type].fn);
}

gboolean cib_op_modifies(int call_type) 
{
    return cib_server_ops[call_type].modifies_cib;
}

int cib_op_can_run(
    int call_type, int call_options, gboolean privileged, gboolean global_update)
{
    int rc = cib_ok;
    
    if(rc == cib_ok &&
       cib_server_ops[call_type].needs_privileges
       && privileged == FALSE) {
	/* abort */
	return cib_not_authorized;
    }
#if 0
    if(rc == cib_ok
       && stand_alone == FALSE
       && global_update == FALSE
       && (call_options & cib_quorum_override) == 0
       && cib_server_ops[call_type].needs_quorum) {
	return cib_no_quorum;
    }
#endif
    return cib_ok;
}


int cib_op_prepare(
    int call_type, xmlNode *request, xmlNode **input, const char **section) 
{
    return cib_server_ops[call_type].prepare(request, input, section);
}

int cib_op_cleanup(
    int call_type, int options, xmlNode **input, xmlNode **output) 
{
    return cib_server_ops[call_type].cleanup(options, input, output);
}

