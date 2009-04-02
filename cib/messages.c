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

#include <crm_internal.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <sys/param.h>
#include <sys/types.h>

#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/msg.h>
#include <crm/common/xml.h>
#include <crm/common/cluster.h>

#include <cibio.h>
#include <cibmessages.h>
#include <callbacks.h>

#define MAX_DIFF_RETRY 5

#ifdef CIBPIPE
gboolean cib_is_master = TRUE;
#else
gboolean cib_is_master = FALSE;
#endif

xmlNode *the_cib = NULL;
gboolean syncd_once = FALSE;
extern const char *cib_our_uname;
enum cib_errors revision_check(xmlNode *cib_update, xmlNode *cib_copy, int flags);
int get_revision(xmlNode *xml_obj, int cur_revision);

enum cib_errors updateList(
	xmlNode *local_cib, xmlNode *update_command, xmlNode *failed,
	int operation, const char *section);

gboolean check_generation(xmlNode *newCib, xmlNode *oldCib);

gboolean update_results(
	xmlNode *failed, xmlNode *target, const char *operation, int return_code);

enum cib_errors cib_update_counter(
	xmlNode *xml_obj, const char *field, gboolean reset);

enum cib_errors sync_our_cib(xmlNode *request, gboolean all);

extern xmlNode *cib_msg_copy(const xmlNode *msg, gboolean with_data);
extern gboolean cib_shutdown_flag;
extern void terminate_cib(const char *caller);

enum cib_errors 
cib_process_shutdown_req(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
#ifdef CIBPIPE
    return cib_invalid_argument;
#else
	enum cib_errors result = cib_ok;
	const char *host = crm_element_value(req, F_ORIG);
	
	*answer = NULL;

	if(crm_element_value(req, F_CIB_ISREPLY) == NULL) {
		crm_info("Shutdown REQ from %s", host);
		return cib_ok;

	} else if(cib_shutdown_flag) {
		crm_info("Shutdown ACK from %s", host);
		terminate_cib(__FUNCTION__);
		return cib_ok;

	} else {
		crm_err("Shutdown ACK from %s - not shutting down",host);
		result = cib_unknown;
	}
	
	return result;
#endif
}

enum cib_errors 
cib_process_default(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
	enum cib_errors result = cib_ok;
	crm_debug_2("Processing \"%s\" event", op);
	*answer = NULL;

	if(op == NULL) {
		result = cib_operation;
		crm_err("No operation specified");
		
	} else if(strcasecmp(CRM_OP_NOOP, op) == 0) {
		;

	} else {
		result = cib_NOTSUPPORTED;
		crm_err("Action [%s] is not supported by the CIB", op);
	}
	return result;
}

enum cib_errors 
cib_process_quit(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
	enum cib_errors result = cib_ok;
	crm_debug_2("Processing \"%s\" event", op);

	crm_warn("The CRMd has asked us to exit... complying");
	exit(0);
	return result;
}

enum cib_errors 
cib_process_readwrite(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
#ifdef CIBPIPE
    return cib_invalid_argument;
#else
	enum cib_errors result = cib_ok;
	crm_debug_2("Processing \"%s\" event", op);

	if(safe_str_eq(op, CIB_OP_ISMASTER)) {
		if(cib_is_master == TRUE) {
			result = cib_ok;
		} else {
			result = cib_not_master;
		}
		return result;
	}

	if(safe_str_eq(op, CIB_OP_MASTER)) {
		if(cib_is_master == FALSE) {
			crm_info("We are now in R/W mode");
			cib_is_master = TRUE;
			syncd_once = TRUE;
			
		} else {
			crm_debug("We are still in R/W mode");
		}
		
	} else if(cib_is_master) {
		crm_info("We are now in R/O mode");
		cib_is_master = FALSE;
	}

	return result;
#endif
}

enum cib_errors 
cib_process_ping(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
#ifdef CIBPIPE
    return cib_invalid_argument;
#else
	enum cib_errors result = cib_ok;
	crm_debug_2("Processing \"%s\" event", op);
	*answer = createPingAnswerFragment(CRM_SYSTEM_CIB, "ok");
	return result;
#endif
}


enum cib_errors 
cib_process_sync(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
#ifdef CIBPIPE
    return cib_invalid_argument;
#else
    return sync_our_cib(req, TRUE);
#endif
}

enum cib_errors 
cib_process_sync_one(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
#ifdef CIBPIPE
    return cib_invalid_argument;
#else
    return sync_our_cib(req, FALSE);
#endif
}

int sync_in_progress = 0;

enum cib_errors 
cib_server_process_diff(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
	int rc = cib_ok;

	if(cib_is_master) {
		/* the master is never waiting for a resync */
		sync_in_progress = 0;
	}
	
	if(sync_in_progress > MAX_DIFF_RETRY) {
		/* request another full-sync,
		 * the last request may have been lost
		 */
		sync_in_progress = 0;
	} 

	if(sync_in_progress) {
	    int diff_add_updates = 0;
	    int diff_add_epoch  = 0;
	    int diff_add_admin_epoch = 0;
	    
	    int diff_del_updates = 0;
	    int diff_del_epoch  = 0;
	    int diff_del_admin_epoch = 0;
	    
	    cib_diff_version_details(
		input,
		&diff_add_admin_epoch, &diff_add_epoch, &diff_add_updates, 
		&diff_del_admin_epoch, &diff_del_epoch, &diff_del_updates);
	    
	    sync_in_progress++;
	    crm_warn("Not applying diff %d.%d.%d -> %d.%d.%d (sync in progress)",
		     diff_del_admin_epoch,diff_del_epoch,diff_del_updates,
		     diff_add_admin_epoch,diff_add_epoch,diff_add_updates);
	    return cib_diff_resync;
	}

    
	rc = cib_process_diff(op, options, section, req, input, existing_cib, result_cib, answer);
	
	if(rc == cib_diff_resync && cib_is_master == FALSE) {
		xmlNode *sync_me = create_xml_node(NULL, "sync-me");
		free_xml(*result_cib);
		*result_cib = NULL;
		crm_info("Requesting re-sync from peer");
		sync_in_progress++;
		
		crm_xml_add(sync_me, F_TYPE, "cib");
		crm_xml_add(sync_me, F_CIB_OPERATION, CIB_OP_SYNC_ONE);
		crm_xml_add(sync_me, F_CIB_DELEGATED, cib_our_uname);

		if(send_cluster_message(NULL, crm_msg_cib, sync_me, FALSE) == FALSE) {
			rc = cib_not_connected;
		}
		free_xml(sync_me);
		
	} else if(rc == cib_diff_resync) {
	    rc = cib_diff_failed;
	    if(options & cib_force_diff) {
		crm_warn("Not requesting full refresh in slave mode.");
	    }
	}
	
	return rc;
}

enum cib_errors 
cib_process_replace_svr(
	const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
	xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
    const char *tag = crm_element_name(input);
    enum cib_errors rc = cib_process_replace(
	op, options, section, req, input, existing_cib, result_cib, answer);
    if(rc == cib_ok && safe_str_eq(tag, XML_TAG_CIB)) {
	sync_in_progress = 0;
    }
    return rc;
}

static int
delete_cib_object(xmlNode *parent, xmlNode *delete_spec)
{
	const char *object_name = NULL;
	const char *object_id = NULL;
	xmlNode *equiv_node = NULL;
	int result = cib_ok;
	
	if(delete_spec != NULL) {
		object_name = crm_element_name(delete_spec);
	}
	object_id = crm_element_value(delete_spec, XML_ATTR_ID);

	crm_debug_3("Processing: <%s id=%s>",
		    crm_str(object_name), crm_str(object_id));
	
	if(delete_spec == NULL) {
		result = cib_NOOBJECT;

	} else if(parent == NULL) {
		result = cib_NOPARENT;

	} else if(object_id == NULL) {
		/*  placeholder object */
		equiv_node = find_xml_node(parent, object_name, FALSE);
		
	} else {
		equiv_node = find_entity(parent, object_name, object_id);
	}

	if(result != cib_ok) {
		; /* nothing */
		
	} else if(equiv_node == NULL) {
		result = cib_ok;

	} else if(xml_has_children(delete_spec) == FALSE) {
		/*  only leaves are deleted */
		crm_debug("Removing leaf: <%s id=%s>",
			  crm_str(object_name), crm_str(object_id));
		zap_xml_from_parent(parent, equiv_node);

	} else {

		xml_child_iter(
			delete_spec, child, 

			int tmp_result = delete_cib_object(equiv_node, child);
			
			/*  only the first error is likely to be interesting */
			if(tmp_result != cib_ok && result == cib_ok) {
				result = tmp_result;
			}
			);
	}

	return result;
}

enum cib_errors 
cib_process_delete_absolute(
    const char *op, int options, const char *section, xmlNode *req, xmlNode *input,
    xmlNode *existing_cib, xmlNode **result_cib, xmlNode **answer)
{
    xmlNode *failed = NULL;
    enum cib_errors result = cib_ok;
    xmlNode *update_section = NULL;
	
    crm_debug_2("Processing \"%s\" event for section=%s", op, crm_str(section));
    if(safe_str_eq(XML_CIB_TAG_SECTION_ALL, section)) {
	section = NULL;

    } else if(safe_str_eq(XML_TAG_CIB, section)) {
	section = NULL;

    } else if(safe_str_eq(crm_element_name(input), XML_TAG_CIB)) {
	section = NULL;
    }

    CRM_CHECK(strcasecmp(CIB_OP_DELETE, op) == 0, return cib_operation);
	
    if(input == NULL) {
	crm_err("Cannot perform modification with no data");
	return cib_NOOBJECT;
    }
	
    failed = create_xml_node(NULL, XML_TAG_FAILED);

    update_section = get_object_root(section, *result_cib);
    result = delete_cib_object(update_section, input);
    update_results(failed, input, op, result);
	
    if(xml_has_children(failed)) {
	CRM_CHECK(result != cib_ok, result = cib_unknown);
    }

    if (result != cib_ok) {
	crm_log_xml_err(failed, "CIB Update failures");
	*answer = failed;
	    
    } else {
	free_xml(failed);
    }
	
    return result;
}

gboolean
check_generation(xmlNode *newCib, xmlNode *oldCib)
{
	if(cib_compare_generation(newCib, oldCib) >= 0) {
		return TRUE;
	}

	crm_warn("Generation from update is older than the existing one");
	return FALSE;
}

#ifndef CIBPIPE
enum cib_errors
sync_our_cib(xmlNode *request, gboolean all) 
{
	enum cib_errors result      = cib_ok;
	const char *host            = crm_element_value(request, F_ORIG);
	const char *op              = crm_element_value(request, F_CIB_OPERATION);

	xmlNode *replace_request = cib_msg_copy(request, FALSE);
	
	CRM_CHECK(the_cib != NULL, ;);
	CRM_CHECK(replace_request != NULL, ;);
	
	crm_debug("Syncing CIB to %s", all?"all peers":host);
	if(all == FALSE && host == NULL) {
	    crm_log_xml(LOG_ERR, "bad sync", request);
	}
	
	/* remove the "all == FALSE" condition
	 *
	 * sync_from was failing, the local client wasnt being notified
	 *    because it didnt know it was a reply
	 * setting this does not prevent the other nodes from applying it
	 *    if all == TRUE
	 */
	if(host != NULL) {
		crm_xml_add(replace_request, F_CIB_ISREPLY, host);
	}
	crm_xml_add(replace_request, F_CIB_OPERATION, CIB_OP_REPLACE);
	crm_xml_add(replace_request, "original_"F_CIB_OPERATION, op);
	crm_xml_add(replace_request, F_CIB_GLOBAL_UPDATE, XML_BOOLEAN_TRUE);
	add_message_xml(replace_request, F_CIB_CALLDATA, the_cib);
	
	if(send_cluster_message(all?NULL:host, crm_msg_cib, replace_request, FALSE) == FALSE) {
		result = cib_not_connected;
	}
	free_xml(replace_request);
	return result;
}
#endif
