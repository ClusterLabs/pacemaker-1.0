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
#include <crm_internal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <glib.h>

#include <crm/crm.h>
#include <crm/cib.h>
#include <crm/msg_xml.h>
#include <crm/common/ipc.h>
#include <cib_private.h>


typedef struct cib_file_opaque_s 
{
	int flags;
	char *filename;
	
} cib_file_opaque_t;

int cib_file_perform_op(
    cib_t *cib, const char *op, const char *host, const char *section,
    xmlNode *data, xmlNode **output_data, int call_options);

int cib_file_signon(cib_t* cib, const char *name, enum cib_conn_type type);
int cib_file_signoff(cib_t* cib);
int cib_file_free(cib_t* cib);

static int cib_file_inputfd(cib_t* cib) { return cib_NOTSUPPORTED; }

static int cib_file_set_connection_dnotify(
    cib_t *cib, void (*dnotify)(gpointer user_data))
{
    return cib_NOTSUPPORTED;
}


static int cib_file_register_notification(cib_t* cib, const char *callback, int enabled) 
{
    return cib_NOTSUPPORTED;
}

cib_t*
cib_file_new (const char *cib_location)
{
    cib_file_opaque_t *private = NULL;
    cib_t *cib = cib_new_variant();

    crm_malloc0(private, sizeof(cib_file_opaque_t));

    cib->variant = cib_file;
    cib->variant_opaque = private;

    if(cib_location == NULL) {
	cib_location = getenv("CIB_file");
    }
    private->filename = crm_strdup(cib_location);

    /* assign variant specific ops*/
    cib->cmds->variant_op = cib_file_perform_op;
    cib->cmds->signon     = cib_file_signon;
    cib->cmds->signoff    = cib_file_signoff;
    cib->cmds->free       = cib_file_free;
    cib->cmds->inputfd    = cib_file_inputfd;

    cib->cmds->register_notification = cib_file_register_notification;
    cib->cmds->set_connection_dnotify = cib_file_set_connection_dnotify;

    return cib;
}

static xmlNode *in_mem_cib = NULL;
static int load_file_cib(const char *filename) 
{
    int rc = cib_ok;
    struct stat buf;
    xmlNode *root = NULL;
    gboolean dtd_ok = TRUE;
    const char *ignore_dtd = NULL;
    xmlNode *status = NULL;
    
    rc = stat(filename, &buf);
    if (rc == 0) {
	root = filename2xml(filename);
	if(root == NULL) {
	    return cib_dtd_validation;
	}

    } else {
	return cib_NOTEXISTS;
    }

    rc = 0;
    
    status = find_xml_node(root, XML_CIB_TAG_STATUS, FALSE);
    if(status == NULL) {
	create_xml_node(root, XML_CIB_TAG_STATUS);		
    }

    ignore_dtd = crm_element_value(root, XML_ATTR_VALIDATION);
    dtd_ok = validate_xml(root, NULL, TRUE);
    if(dtd_ok == FALSE) {
	crm_err("CIB does not validate against %s", ignore_dtd);
	rc = cib_dtd_validation;
	goto bail;
    }	
    
    in_mem_cib = root;
    return rc;

  bail:
    free_xml(root);
    root = NULL;
    return rc;
}

int
cib_file_signon(cib_t* cib, const char *name, enum cib_conn_type type)
{
    int rc = cib_ok;
    cib_file_opaque_t *private = cib->variant_opaque;

    if(private->filename == FALSE) {
	rc = cib_missing;
    } else {
	rc = load_file_cib(private->filename);
    }
    
    if(rc == cib_ok) {
	crm_debug("%s: Opened connection to local file '%s'", name, private->filename);
	cib->state = cib_connected_command;
	cib->type  = cib_command;

    } else {
	fprintf(stderr, "%s: Connection to local file '%s' failed: %s\n",
		name, private->filename, cib_error2string(rc));
    }
    
    return rc;
}
	
int
cib_file_signoff(cib_t* cib)
{
    int rc = cib_ok;
    cib_file_opaque_t *private = cib->variant_opaque;

    crm_debug("Signing out of the CIB Service");
    
    if(strstr(private->filename, ".bz2") != NULL) {
	rc = write_xml_file(in_mem_cib, private->filename, TRUE);
	
    } else {
	rc = write_xml_file(in_mem_cib, private->filename, FALSE);
    }

    if(rc > 0) {
	crm_info("Wrote CIB to %s", private->filename);
	rc = cib_ok;
	
    } else {
	crm_err("Could not write CIB to %s", private->filename);
    }
    free_xml(in_mem_cib);
    
    cib->state = cib_disconnected;
    cib->type  = cib_none;
    
    return rc;
}

int
cib_file_free (cib_t* cib)
{
    int rc = cib_ok;

    if(cib->state != cib_disconnected) {
	rc = cib_file_signoff(cib);
	if(rc == cib_ok) {
	    cib_file_opaque_t *private = cib->variant_opaque;
	    crm_free(private->filename);
	    crm_free(cib->cmds);
	    crm_free(private);
	    crm_free(cib);
	}
    }
	
    return rc;
}

struct cib_func_entry 
{
	const char *op;
	gboolean    read_only;
	cib_op_t    fn;
};

static struct cib_func_entry cib_file_ops[] = {
    {CIB_OP_QUERY,      TRUE,  cib_process_query},
    {CIB_OP_MODIFY,     FALSE, cib_process_modify},
    {CIB_OP_APPLY_DIFF, FALSE, cib_process_diff},
    {CIB_OP_BUMP,       FALSE, cib_process_bump},
    {CIB_OP_REPLACE,    FALSE, cib_process_replace},
    {CIB_OP_CREATE,     FALSE, cib_process_create},
    {CIB_OP_DELETE,     FALSE, cib_process_delete},
    {CIB_OP_ERASE,      FALSE, cib_process_erase},
    {CIB_OP_UPGRADE,    FALSE, cib_process_upgrade},
};


int
cib_file_perform_op(
    cib_t *cib, const char *op, const char *host, const char *section,
    xmlNode *data, xmlNode **output_data, int call_options) 
{
    int rc = cib_ok;
    gboolean query = FALSE;
    gboolean changed = FALSE;
    xmlNode *output = NULL;
    xmlNode *cib_diff = NULL;
    xmlNode *result_cib = NULL;
    cib_op_t *fn = NULL;
    int lpc = 0;
    static int max_msg_types = DIMOF(cib_file_ops);

    crm_info("%s on %s", op, section);
    
    if(cib->state == cib_disconnected) {
	return cib_not_connected;
    }

    if(output_data != NULL) {
	*output_data = NULL;
    }
	
    if(op == NULL) {
	return cib_operation;
    }

    for (lpc = 0; lpc < max_msg_types; lpc++) {
	if (safe_str_eq(op, cib_file_ops[lpc].op)) {
	    fn = &(cib_file_ops[lpc].fn);
	    query = cib_file_ops[lpc].read_only;
	    break;
	}
    }
    
    if(fn == NULL) {
	return cib_NOTSUPPORTED;
    }

    cib->call_id++;
    rc = cib_perform_op(op, call_options, fn, query,
    			section, NULL, data, TRUE, &changed, in_mem_cib, &result_cib, &cib_diff, &output);

    if(rc == cib_dtd_validation) {
	validate_xml_verbose(result_cib);
    }
    
    if(rc != cib_ok) {
	free_xml(result_cib);
	    
    } else if(query == FALSE) {
	log_xml_diff(LOG_INFO, cib_diff, "cib:diff");	
	free_xml(in_mem_cib);
	in_mem_cib = result_cib;
    }

    free_xml(cib_diff);

    if(cib->op_callback != NULL) {
	cib->op_callback(NULL, cib->call_id, rc, output);
    }
	
    if(output_data && output) {
	*output_data = copy_xml(output);
    }

    if(query == FALSE) {
	free_xml(output);
    }
    
    return rc;
}


