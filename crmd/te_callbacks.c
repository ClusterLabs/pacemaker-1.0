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

#include <sys/stat.h>

#include <crm/crm.h>
#include <crm/common/xml.h>
#include <crm/msg_xml.h>
#include <crm/cib.h>

#include <tengine.h>
#include <te_callbacks.h>
#include <crmd_fsa.h>

#include <crm/common/cluster.h> /* For ONLINESTATUS etc */

void te_update_confirm(const char *event, xmlNode *msg);

extern char *te_uuid;
gboolean shuttingdown = FALSE;
crm_graph_t *transition_graph;
crm_trigger_t *transition_trigger = NULL;

/* #define rsc_op_template "//"XML_TAG_DIFF_ADDED"//"XML_TAG_CIB"//"XML_CIB_TAG_STATE"[@uname='%s']"//"XML_LRM_TAG_RSC_OP"[@id='%s]" */
#define rsc_op_template "//"XML_TAG_DIFF_ADDED"//"XML_TAG_CIB"//"XML_LRM_TAG_RSC_OP"[@id='%s']"

static const char *get_node_id(xmlNode *rsc_op) 
{
    xmlNode *node = rsc_op;
    while(node != NULL && safe_str_neq(XML_CIB_TAG_STATE, TYPE(node))) {
	node = node->parent;
    }
    
    CRM_CHECK(node != NULL, return NULL);
    return ID(node);
}


static void process_resource_updates(xmlXPathObject *xpathObj) 
{
/*
    <status>
       <node_state id="node1" state=CRMD_STATE_ACTIVE exp_state="active">
          <lrm>
             <lrm_resources>
        	<rsc_state id="" rsc_id="rsc4" node_id="node1" rsc_state="stopped"/>
*/
    int lpc = 0, max = xpathObj->nodesetval->nodeNr;
    for(lpc = 0; lpc < max; lpc++) {
	xmlNode *rsc_op = getXpathResult(xpathObj, lpc);
	const char *node = get_node_id(rsc_op);
	process_graph_event(rsc_op, node);
    }
}

void
te_update_diff(const char *event, xmlNode *msg)
{
	int rc = -1;
	const char *op = NULL;

	xmlNode *diff = NULL;
	xmlNode *cib_top = NULL;
	xmlXPathObject *xpathObj = NULL;

	int diff_add_updates     = 0;
	int diff_add_epoch       = 0;
	int diff_add_admin_epoch = 0;

	int diff_del_updates     = 0;
	int diff_del_epoch       = 0;
	int diff_del_admin_epoch = 0;
	
	CRM_CHECK(msg != NULL, return);
	crm_element_value_int(msg, F_CIB_RC, &rc);	

	if(transition_graph == NULL) {
	    crm_debug_3("No graph");
	    return;

	} else if(rc < cib_ok) {
	    crm_debug_3("Filter rc=%d (%s)", rc, cib_error2string(rc));
	    return;

	} else if(transition_graph->complete == TRUE
		  && fsa_state != S_IDLE
		  && fsa_state != S_TRANSITION_ENGINE
		  && fsa_state != S_POLICY_ENGINE) {
	    crm_debug_2("Filter state=%s, complete=%d", fsa_state2string(fsa_state), transition_graph->complete);
	    return;
	} 	

	op = crm_element_value(msg, F_CIB_OPERATION);
	diff = get_message_xml(msg, F_CIB_UPDATE_RESULT);

	cib_diff_version_details(
		diff,
		&diff_add_admin_epoch, &diff_add_epoch, &diff_add_updates, 
		&diff_del_admin_epoch, &diff_del_epoch, &diff_del_updates);
	
	crm_debug("Processing diff (%s): %d.%d.%d -> %d.%d.%d (%s)", op,
		  diff_del_admin_epoch,diff_del_epoch,diff_del_updates,
		  diff_add_admin_epoch,diff_add_epoch,diff_add_updates,
		  fsa_state2string(fsa_state));
	log_cib_diff(LOG_DEBUG_2, diff, op);

	/* Process anything that was added */
	cib_top = get_xpath_object("//"F_CIB_UPDATE_RESULT"//"XML_TAG_DIFF_ADDED"//"XML_TAG_CIB, diff, LOG_ERR);
	if(need_abort(cib_top)) {
	    goto bail; /* configuration changed */
	}

	/* Process anything that was removed */
	cib_top = get_xpath_object("//"F_CIB_UPDATE_RESULT"//"XML_TAG_DIFF_REMOVED"//"XML_TAG_CIB, diff, LOG_ERR);
	if(need_abort(cib_top)) {
	    goto bail; /* configuration changed */
	}

	/* Transient Attributes - Added/Updated */
	xpathObj = xpath_search(diff,"//"F_CIB_UPDATE_RESULT"//"XML_TAG_DIFF_ADDED"//"XML_TAG_TRANSIENT_NODEATTRS"//"XML_CIB_TAG_NVPAIR);
	if(xpathObj && xpathObj->nodesetval->nodeNr > 0) {
	    int lpc;
	    for(lpc = 0; lpc < xpathObj->nodesetval->nodeNr; lpc++) {
		xmlNode *attr = getXpathResult(xpathObj, lpc);
		const char *name = crm_element_value(attr, XML_NVPAIR_ATTR_NAME);
		const char *value = NULL;
		
		if(safe_str_eq(CRM_OP_PROBED, name)) {
		    value = crm_element_value(attr, XML_NVPAIR_ATTR_VALUE);
		}

		if(crm_is_true(value) == FALSE) {
		    abort_transition(INFINITY, tg_restart, "Transient attribute: update", attr);
		    crm_log_xml_debug_2(attr, "Abort");
		    goto bail;
		}
	    }

	} else if(xpathObj) {
	    xmlXPathFreeObject(xpathObj);
	}
	
	/* Transient Attributes - Removed */
	xpathObj = xpath_search(diff,"//"F_CIB_UPDATE_RESULT"//"XML_TAG_DIFF_REMOVED"//"XML_TAG_TRANSIENT_NODEATTRS);
	if(xpathObj && xpathObj->nodesetval->nodeNr > 0) {
	    xmlNode *aborted = getXpathResult(xpathObj, 0);
	    abort_transition(INFINITY, tg_restart, "Transient attribute: removal", aborted);
	    goto bail;

	} else if(xpathObj) {
	    xmlXPathFreeObject(xpathObj);
	}

	/* Check for node state updates... possibly from a shutdown we requested */
	xpathObj = xpath_search(diff, "//"F_CIB_UPDATE_RESULT"//"XML_TAG_DIFF_ADDED"//"XML_CIB_TAG_STATE);
	if(xpathObj) {
	    int lpc = 0, max = xpathObj->nodesetval->nodeNr;
	    for(lpc = 0; lpc < max; lpc++) {
		xmlNode *node = getXpathResult(xpathObj, lpc);
		const char *event_node = crm_element_value(node, XML_ATTR_ID);
		const char *ccm_state  = crm_element_value(node, XML_CIB_ATTR_INCCM);
		const char *ha_state   = crm_element_value(node, XML_CIB_ATTR_HASTATE);
		const char *shutdown_s = crm_element_value(node, XML_CIB_ATTR_SHUTDOWN);
		const char *crmd_state = crm_element_value(node, XML_CIB_ATTR_CRMDSTATE);

		if(safe_str_eq(ccm_state, XML_BOOLEAN_FALSE)
		   || safe_str_eq(ha_state, DEADSTATUS)
		   || safe_str_eq(crmd_state, CRMD_JOINSTATE_DOWN)) {
		    crm_action_t *shutdown = match_down_event(0, event_node, NULL);
		    
		    if(shutdown != NULL) {
			const char *task = crm_element_value(shutdown->xml, XML_LRM_ATTR_TASK);
			if(safe_str_neq(task, CRM_OP_FENCE)) {
			    /* Wait for stonithd to tell us it is complete via tengine_stonith_callback() */
			    update_graph(transition_graph, shutdown);
			    trigger_graph();
			}
			
		    } else {
			crm_info("Stonith/shutdown of %s not matched", event_node);
			abort_transition(INFINITY, tg_restart, "Node failure", node);
		    }			
		    fail_incompletable_actions(transition_graph, event_node);
		}
	 
		if(shutdown_s) {
		    int shutdown = crm_parse_int(shutdown_s, NULL);
		    if(shutdown > 0) {
			crm_info("Aborting on "XML_CIB_ATTR_SHUTDOWN" attribute for %s", event_node);
			abort_transition(INFINITY, tg_restart, "Shutdown request", node);
		    }
		}
	    }
	    xmlXPathFreeObject(xpathObj);
	}

	/*
	 * Check for and fast-track the processing of LRM refreshes
	 * In large clusters this can result in _huge_ speedups
	 *
	 * Unfortunately we can only do so when there are no pending actions
	 * Otherwise we could miss updates we're waiting for and stall 
	 *
	 */
	xpathObj = NULL;
	if(transition_graph->pending == 0) {
	    xpathObj = xpath_search(diff, "//"F_CIB_UPDATE_RESULT"//"XML_TAG_DIFF_ADDED"//"XML_LRM_TAG_RESOURCE);
	}
	
	if(xpathObj) {
	    int updates = xpathObj->nodesetval->nodeNr;
	    if(updates > 1) {
		/* Updates by, or in response to, TE actions will never contain updates
		 * for more than one resource at a time
		 */
		crm_info("Detected LRM refresh - %d resources updated: Skipping all resource events", updates);
		abort_transition(INFINITY, tg_restart, "LRM Refresh", diff);
		goto bail;
	    }
	    xmlXPathFreeObject(xpathObj);
	}

	/* Process operation updates */
	xpathObj = xpath_search(diff, "//"F_CIB_UPDATE_RESULT"//"XML_TAG_DIFF_ADDED"//"XML_LRM_TAG_RSC_OP);
	if(xpathObj) {
	    process_resource_updates(xpathObj);
	    xmlXPathFreeObject(xpathObj);
	}
	
	/* Detect deleted (as opposed to replaced or added) actions - eg. crm_resource -C */ 
	xpathObj = xpath_search(diff, "//"XML_TAG_DIFF_REMOVED"//"XML_LRM_TAG_RSC_OP);
	if(xpathObj) {
	    int lpc = 0, max = xpathObj->nodesetval->nodeNr;
	    
	    for(lpc = 0; lpc < max; lpc++) {
		int max = 0;
		const char *op_id = NULL;
		char *rsc_op_xpath = NULL;
		xmlXPathObject *op_match = NULL;
		xmlNode *match = getXpathResult(xpathObj, lpc);
		CRM_CHECK(match != NULL, continue);

		op_id = ID(match);

		max = strlen(rsc_op_template) + strlen(op_id) + 1;
		crm_malloc0(rsc_op_xpath, max);
		snprintf(rsc_op_xpath, max, rsc_op_template, op_id);
		
		op_match = xpath_search(diff, rsc_op_xpath);
		if(op_match == NULL || op_match->nodesetval->nodeNr == 0) {
		    /* Prevent false positives by matching cancelations too */
		    const char *node = get_node_id(match);
		    crm_action_t *cancelled = get_cancel_action(op_id, node);

		    if(cancelled == NULL) {
			crm_debug("No match for deleted action %s (%s on %s)", rsc_op_xpath, op_id, node);
			abort_transition(INFINITY, tg_restart, "Resource op removal", match);
			if(op_match) {
			    xmlXPathFreeObject(op_match);
			}
			crm_free(rsc_op_xpath);
			goto bail;

		    } else {
			crm_debug("Deleted lrm_rsc_op %s on %s was for graph event %d",
				  op_id, node, cancelled->id);
		    }
		}

		if(op_match) {
		    xmlXPathFreeObject(op_match);
		}
		crm_free(rsc_op_xpath);
	    }
	}

  bail:
	if(xpathObj) {
	    xmlXPathFreeObject(xpathObj);
	}
}

gboolean
process_te_message(xmlNode *msg, xmlNode *xml_data)
{
	const char *from     = crm_element_value(msg, F_ORIG);
	const char *sys_to   = crm_element_value(msg, F_CRM_SYS_TO);
	const char *sys_from = crm_element_value(msg, F_CRM_SYS_FROM);
	const char *ref      = crm_element_value(msg, XML_ATTR_REFERENCE);
	const char *op       = crm_element_value(msg, F_CRM_TASK);
	const char *type     = crm_element_value(msg, F_CRM_MSG_TYPE);

	crm_debug_2("Processing %s (%s) message", op, ref);
	crm_log_xml(LOG_DEBUG_3, "ipc", msg);
	
	if(op == NULL){
		/* error */

	} else if(sys_to == NULL || strcasecmp(sys_to, CRM_SYSTEM_TENGINE) != 0) {
		crm_debug_2("Bad sys-to %s", crm_str(sys_to));
		return FALSE;
		
	} else if(safe_str_eq(op, CRM_OP_INVOKE_LRM)
		  && safe_str_eq(sys_from, CRM_SYSTEM_LRMD)
/* 		  && safe_str_eq(type, XML_ATTR_RESPONSE) */
		){
	    xmlXPathObject *xpathObj = NULL;
	    crm_log_xml(LOG_DEBUG_2, "Processing (N)ACK", msg);
	    crm_info("Processing (N)ACK %s from %s",
		     crm_element_value(msg, XML_ATTR_REFERENCE), from);
	    
	    xpathObj = xpath_search(xml_data, "//"XML_LRM_TAG_RSC_OP);
	    if(xpathObj) {
		process_resource_updates(xpathObj);
		xmlXPathFreeObject(xpathObj);
		xpathObj = NULL;
		
	    } else {
		crm_log_xml(LOG_ERR, "Invalid (N)ACK", msg);
		return FALSE;
	    }
		
	} else {
		crm_err("Unknown command: %s::%s from %s", type, op, sys_from);
	}

	crm_debug_3("finished processing message");
	
	return TRUE;
}

void
tengine_stonith_callback(stonith_ops_t * op)
{
	const char *allow_fail  = NULL;
	int target_rc = -1;
	int stonith_id = -1;
	int transition_id = -1;
	char *uuid = NULL;
	crm_action_t *stonith_action = NULL;

	if(op == NULL) {
		crm_err("Called with a NULL op!");
		return;
	}
	
	crm_info("call=%d, optype=%d, node_name=%s, result=%d, node_list=%s, action=%s",
		 op->call_id, op->optype, op->node_name, op->op_result,
		 (char *)op->node_list, op->private_data);
	
	/* this will mark the event complete if a match is found */
	CRM_CHECK(op->private_data != NULL, return);

	/* filter out old STONITH actions */

	CRM_CHECK(decode_transition_key(
		      op->private_data, &uuid, &transition_id, &stonith_id, &target_rc),
		  crm_err("Invalid event detected");
		  goto bail;
		);
	
	if(transition_graph->complete
	   || stonith_id < 0
	   || safe_str_neq(uuid, te_uuid)
	   || transition_graph->id != transition_id) {
		crm_info("Ignoring STONITH action initiated outside"
			 " of the current transition");
	}

	stonith_action = get_action(stonith_id, TRUE);
	
	if(stonith_action == NULL) {
		crm_err("Stonith action not matched");
		goto bail;
	}

	switch(op->op_result) {
		case STONITH_SUCCEEDED:
			send_stonith_update(op);
			break;
		case STONITH_CANNOT:
		case STONITH_TIMEOUT:
		case STONITH_GENERIC:
			stonith_action->failed = TRUE;
			allow_fail = crm_meta_value(stonith_action->params, XML_ATTR_TE_ALLOWFAIL);

			if(FALSE == crm_is_true(allow_fail)) {
				crm_err("Stonith of %s failed (%d)..."
					" aborting transition.",
					op->node_name, op->op_result);
				abort_transition(INFINITY, tg_restart,
						 "Stonith failed", NULL);
			}
			break;
		default:
			crm_err("Unsupported action result: %d", op->op_result);
			abort_transition(INFINITY, tg_restart,
					 "Unsupport Stonith result", NULL);
	}
	
	update_graph(transition_graph, stonith_action);
	trigger_graph();

  bail:
	crm_free(uuid);
	return;
}

void
cib_fencing_updated(xmlNode *msg, int call_id, int rc,
		    xmlNode *output, void *user_data)
{
    if(rc < cib_ok) {
	crm_err("CIB update failed: %s", cib_error2string(rc));
	crm_log_xml_warn(msg, "Failed update");
    }
    crm_free(user_data);
}

void
cib_action_updated(xmlNode *msg, int call_id, int rc,
		   xmlNode *output, void *user_data)
{
	if(rc < cib_ok) {
		crm_err("Update %d FAILED: %s", call_id, cib_error2string(rc));
	}
}

void
cib_failcount_updated(xmlNode *msg, int call_id, int rc,
		      xmlNode *output, void *user_data)
{
	if(rc < cib_ok) {
		crm_err("Update %d FAILED: %s", call_id, cib_error2string(rc));
	}
}

gboolean
action_timer_callback(gpointer data)
{
	crm_action_timer_t *timer = NULL;
	
	CRM_CHECK(data != NULL, return FALSE);
	
	timer = (crm_action_timer_t*)data;
	stop_te_timer(timer);

	crm_warn("Timer popped (timeout=%d, abort_level=%d, complete=%s)",
		 timer->timeout,
		 transition_graph->abort_priority,
		 transition_graph->complete?"true":"false");

	CRM_CHECK(timer->action != NULL, return FALSE);

	if(transition_graph->complete) {
		crm_warn("Ignoring timeout while not in transition");
		
	} else if(timer->reason == timeout_action_warn) {
		print_action(
			LOG_WARNING,"Action missed its timeout: ", timer->action);

	/* Don't check the FSA state
	 *
	 * We might also be in S_INTEGRATION or some other state waiting for this
	 * action so we can close the transition and continue
	 */
		
	} else {
	    /* fail the action */
	    gboolean send_update = TRUE;
	    const char *task = crm_element_value(timer->action->xml, XML_LRM_ATTR_TASK);
	    print_action(LOG_ERR, "Aborting transition, action lost: ", timer->action);

	    timer->action->failed = TRUE;
	    timer->action->confirmed = TRUE;
	    abort_transition(INFINITY, tg_restart, "Action lost", NULL);
	    
	    update_graph(transition_graph, timer->action);
	    trigger_graph();

	    if(timer->action->type != action_type_rsc) {
		send_update = FALSE;
	    } else if(safe_str_eq(task, "cancel")) {
		/* we dont need to update the CIB with these */
		send_update = FALSE;
	    }

	    if(send_update) {
		/* cib_action_update(timer->action, LRM_OP_PENDING, EXECRA_STATUS_UNKNOWN); */
		cib_action_update(timer->action, LRM_OP_TIMEOUT, EXECRA_UNKNOWN_ERROR);
	    }
	}

	return FALSE;
}



