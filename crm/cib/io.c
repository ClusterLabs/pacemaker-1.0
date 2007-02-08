/* $Id: io.c,v 1.81 2006/07/18 06:15:54 andrew Exp $ */
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

#include <portability.h>

#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>

#include <heartbeat.h>
#include <crm/crm.h>

#include <cibio.h>
#include <crm/cib.h>
#include <crm/common/util.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/common/util.h>
#include <clplumbing/cl_misc.h>
#include <clplumbing/lsb_exitcodes.h>

#include <cibprimatives.h>

#include <crm/dmalloc_wrapper.h>

const char * local_resource_path[] =
{
	XML_CIB_TAG_STATUS,
};

const char * resource_path[] =
{
	XML_CIB_TAG_RESOURCES,
};

const char * node_path[] =
{
	XML_CIB_TAG_NODES,
};

const char * constraint_path[] =
{
	XML_CIB_TAG_CONSTRAINTS,
};

gboolean initialized = FALSE;
crm_data_t *the_cib = NULL;
crm_data_t *node_search = NULL;
crm_data_t *resource_search = NULL;
crm_data_t *constraint_search = NULL;
crm_data_t *status_search = NULL;

extern gboolean cib_writes_enabled;
extern char *ccm_transition_id;
extern gboolean cib_have_quorum;
extern GHashTable *peer_hash;
extern GHashTable *ccm_membership;
extern GTRIGSource *cib_writer;
extern enum cib_errors cib_status;

int set_connected_peers(crm_data_t *xml_obj);
void GHFunc_count_peers(gpointer key, gpointer value, gpointer user_data);
int write_cib_contents(gpointer p);



static gboolean
validate_cib_digest(crm_data_t *local_cib)
{
	int s_res = -1;
	struct stat buf;
	char *digest = NULL;
	char *expected = NULL;
	gboolean passed = FALSE;
	FILE *expected_strm = NULL;
	int start = 0, length = 0, read_len = 0;
	
	s_res = stat(CIB_FILENAME ".sig", &buf);
	
	if (s_res != 0) {
		crm_warn("No on-disk digest present");
		return TRUE;
	}

	if(local_cib != NULL) {
		digest = calculate_xml_digest(local_cib, FALSE);
	}
	
	expected_strm = fopen(CIB_FILENAME ".sig", "r");
	start  = ftell(expected_strm);
	fseek(expected_strm, 0L, SEEK_END);
	length = ftell(expected_strm);
	fseek(expected_strm, 0L, start);
	
	CRM_ASSERT(start == ftell(expected_strm));

	crm_debug_3("Reading %d bytes from file", length);
	crm_malloc0(expected, (length+1));
	read_len = fread(expected, 1, length, expected_strm);
	CRM_ASSERT(read_len == length);

	if(expected == NULL) {
		crm_err("On-disk digest is empty");
		
	} else if(safe_str_eq(expected, digest)) {
		crm_debug("Digest comparision passed: %s", digest);
		passed = TRUE;

	} else {
		crm_err("Digest comparision failed: %s vs. %s",
			expected, digest);
	}

 	crm_free(digest);
 	crm_free(expected);
	return passed;
}

static int
write_cib_digest(crm_data_t *local_cib, char *digest)
{
	int rc = 0;
	FILE *digest_strm = fopen(CIB_FILENAME ".sig", "w");
	char *local_digest = NULL;
	CRM_ASSERT(digest_strm != NULL);

	if(digest == NULL) {
		local_digest = calculate_xml_digest(local_cib, FALSE);
		CRM_ASSERT(digest != NULL);
		digest = local_digest;
	}
	
	rc = fprintf(digest_strm, "%s", digest);
	if(rc < 0) {
		cl_perror("Cannot write output to %s.sig", CIB_FILENAME);
	}

	fflush(digest_strm);
	fclose(digest_strm);
	crm_free(local_digest);
	return rc;
}

static gboolean
validate_on_disk_cib(const char *filename, crm_data_t **on_disk_cib)
{
	int s_res = -1;
	struct stat buf;
	FILE *cib_file = NULL;
	gboolean passed = TRUE;
	crm_data_t *root = NULL;
	
	if(filename != NULL) {
		s_res = stat(filename, &buf);
	}
	
	if (s_res == 0) {
		cib_file = fopen(filename, "r");
		crm_debug_2("Reading cluster configuration from: %s", filename);
		root = file2xml(cib_file, FALSE);
		fclose(cib_file);
		
		if(validate_cib_digest(root) == FALSE) {
			passed = FALSE;
		}
	}
	
	if(on_disk_cib != NULL) {
		*on_disk_cib = root;
	} else {
		free_xml(root);
	}
	return passed;
}

/*
 * It is the callers responsibility to free the output of this function
 */
crm_data_t*
readCibXmlFile(const char *dir, const char *file, gboolean discard_status)
{
	struct stat buf;
	FILE *cib_file = NULL;
	gboolean dtd_ok = TRUE;

	char *filename = NULL;
	const char *name = NULL;
	const char *value = NULL;
	const char *ignore_dtd = NULL;
	
	crm_data_t *root = NULL;
	crm_data_t *status = NULL;

	if(!crm_is_writable(dir, file, HA_CCMUSER, HA_APIGROUP, FALSE)) {
		cib_status = cib_bad_permissions;
		return NULL;
	}
	
	filename = crm_concat(dir, file, '/');
	if(stat(filename, &buf) != 0) {
		crm_warn("Cluster configuration not found: %s."
			 "  Creating an empty one.", filename);

	} else {
		crm_info("Reading cluster configuration from: %s", filename);
		cib_file = fopen(filename, "r");
		root = file2xml(cib_file, FALSE);
		fclose(cib_file);

		if(root == NULL) {
			crm_err("%s exists but does NOT contain valid XML. ",
				filename);
			crm_err("Continuing with an empty configuration."
				"  %s will NOT be overwritten.", filename);
			cib_writes_enabled = FALSE;

		} else if(validate_cib_digest(root) == FALSE) {
			crm_err("%s has been manually changed! If this was"
				" intended, remove the digest in %s.sig",
				filename, filename);
			cib_status = cib_bad_digest;
		}
	}

	if(root == NULL) {
		root = createEmptyCib();	
	} else {
		crm_xml_add(root, "generated", XML_BOOLEAN_FALSE);	
	}

	status = find_xml_node(root, XML_CIB_TAG_STATUS, FALSE);
	if(discard_status && status != NULL) {
		/* strip out the status section if there is one */
		free_xml_from_parent(root, status);
		status = NULL;
	}
	create_xml_node(root, XML_CIB_TAG_STATUS);		
	
	/* Do this before DTD validation happens */

	/* fill in some defaults */
	name = XML_ATTR_GENERATION_ADMIN;
	value = crm_element_value(root, name);
	if(value == NULL) {
		crm_xml_add_int(root, name, 0);
	}
	
	name = XML_ATTR_GENERATION;
	value = crm_element_value(root, name);
	if(value == NULL) {
		crm_xml_add_int(root, name, 0);
	}
	
	name = XML_ATTR_NUMUPDATES;
	value = crm_element_value(root, name);
	if(value == NULL) {
		crm_xml_add_int(root, name, 0);
	}
	
	/* unset these and require the DC/CCM to update as needed */
	update_counters(__FILE__, __PRETTY_FUNCTION__, root);
	xml_remove_prop(root, XML_ATTR_DC_UUID);

	if(discard_status) {
		crm_log_xml_info(root, "[on-disk]");
	}
	
	ignore_dtd = crm_element_value(root, "ignore_dtd");
	dtd_ok = validate_with_dtd(root, TRUE, HA_LIBDIR"/heartbeat/crm.dtd");
	if(dtd_ok == FALSE) {
		if(ignore_dtd == NULL
		   && crm_is_true(ignore_dtd) == FALSE) {
			cib_status = cib_dtd_validation;
		}
		
	} else if(ignore_dtd == NULL) {
		crm_notice("Enabling DTD validation on"
			   " the existing (sane) configuration");
		crm_xml_add(root, "ignore_dtd", XML_BOOLEAN_FALSE);	
	}	
	
	if(do_id_check(root, NULL, TRUE, FALSE)) {
		crm_err("%s does not contain a vaild configuration:"
			" ID check failed",
			 filename);
		cib_status = cib_id_check;
	}

	if (verifyCibXml(root) == FALSE) {
		crm_err("%s does not contain a vaild configuration:"
			" structure test failed",
			 filename);
		cib_status = cib_bad_config;
	}

	crm_free(filename);
	return root;
}

/*
 * The caller should never free the return value
 */
crm_data_t*
get_the_CIB(void)
{
	return the_cib;
}

gboolean
uninitializeCib(void)
{
	crm_data_t *tmp_cib = the_cib;
	
	
	if(tmp_cib == NULL) {
		crm_debug("The CIB has already been deallocated.");
		return FALSE;
	}
	
	initialized = FALSE;
	the_cib = NULL;
	node_search = NULL;
	resource_search = NULL;
	constraint_search = NULL;
	status_search = NULL;

	crm_debug("Deallocating the CIB.");
	
	free_xml(tmp_cib);

	crm_info("The CIB has been deallocated.");
	
	return TRUE;
}




/*
 * This method will not free the old CIB pointer or the new one.
 * We rely on the caller to have saved a pointer to the old CIB
 *   and to free the old/bad one depending on what is appropriate.
 */
gboolean
initializeCib(crm_data_t *new_cib)
{
	gboolean is_valid = TRUE;
	crm_data_t *tmp_node = NULL;

	if(new_cib == NULL) {
		return FALSE;
	}
	
	xml_validate(new_cib);

	tmp_node = get_object_root(XML_CIB_TAG_NODES, new_cib);
	if (tmp_node == NULL) { is_valid = FALSE; }

	tmp_node = get_object_root(XML_CIB_TAG_RESOURCES, new_cib);
	if (tmp_node == NULL) { is_valid = FALSE; }

	tmp_node = get_object_root(XML_CIB_TAG_CONSTRAINTS, new_cib);
	if (tmp_node == NULL) { is_valid = FALSE; }

	tmp_node = get_object_root(XML_CIB_TAG_CRMCONFIG, new_cib);
	if (tmp_node == NULL) { is_valid = FALSE; }

	tmp_node = get_object_root(XML_CIB_TAG_STATUS, new_cib);
	if (is_valid && tmp_node == NULL) {
		create_xml_node(new_cib, XML_CIB_TAG_STATUS);
	}

	if(is_valid == FALSE) {
		crm_warn("CIB Verification failed");
		return FALSE;
	}

	update_counters(__FILE__, __PRETTY_FUNCTION__, new_cib);
	
	the_cib = new_cib;
	initialized = TRUE;
	return TRUE;
}

static int
archive_file(const char *oldname, const char *newname, const char *ext)
{
	/* move 'oldname' to 'newname' by creating a hard link to it
	 *  and then removing the original hard link
	 */
	int rc = 0;
	int res = 0;
	struct stat tmp;
	int s_res = 0;
	char *backup_file = NULL;
	static const char *back_ext = "bak";

	/* calculate the backup name if required */
	if(newname != NULL) {
		backup_file = crm_strdup(newname);

	} else {
		int max_name_len = 1024;
		crm_malloc0(backup_file, max_name_len);
		if (ext == NULL) {
			ext = back_ext;
		}
		snprintf(backup_file, max_name_len - 1, "%s.%s", oldname, ext);
	}

	if(backup_file == NULL || strlen(backup_file) == 0) {
		crm_err("%s backup filename was %s",
			newname == NULL?"calculated":"supplied",
			backup_file == NULL?"null":"empty");
		rc = -4;		
	}
	
	s_res = stat(backup_file, &tmp);
	
	/* unlink the old backup */
	if (rc == 0 && s_res >= 0) {
		res = unlink(backup_file);
		if (res < 0) {
			cl_perror("Could not unlink %s", backup_file);
			rc = -1;
		}
	}
    
	s_res = stat(oldname, &tmp);

	/* copy */
	if (rc == 0 && s_res >= 0) {
		res = link(oldname, backup_file);
		if (res < 0) {
			cl_perror("Could not create backup %s from %s",
				  backup_file, oldname);
			rc = -2;
		}
	}

	/* unlink the original */
	if (rc == 0 && s_res >= 0) {
		res = unlink(oldname);
		if (res < 0) {
			cl_perror("Could not unlink %s", oldname);
			rc = -3;
		}
	}

	crm_free(backup_file);
	return rc;
    
}

/*
 * This method will free the old CIB pointer on success and the new one
 * on failure.
 */
int
activateCibXml(crm_data_t *new_cib, const char *ignored)
{
	int error_code = cib_ok;
	crm_data_t *saved_cib = the_cib;
	const char *ignore_dtd = NULL;

	crm_log_xml_debug_4(new_cib, "Attempting to activate CIB");

	CRM_ASSERT(new_cib != saved_cib);
	if(saved_cib != NULL) {
		crm_validate_data(saved_cib);
	}

	ignore_dtd = crm_element_value(new_cib, "ignore_dtd");
	if(
#if CRM_DEPRECATED_SINCE_2_0_4
	   ignore_dtd != NULL &&
#endif
	   crm_is_true(ignore_dtd) == FALSE
	   && validate_with_dtd(
		   new_cib, TRUE, HA_LIBDIR"/heartbeat/crm.dtd") == FALSE) {
 		error_code = cib_dtd_validation;
		crm_err("Ignoring invalid CIB");
	}

	if(error_code == cib_ok && initializeCib(new_cib) == FALSE) {
		error_code = cib_ACTIVATION;
		crm_err("Ignoring invalid or NULL CIB");
	}

	if(error_code != cib_ok) {
		if(saved_cib != NULL) {
			crm_warn("Reverting to last known CIB");
			if (initializeCib(saved_cib) == FALSE) {
				/* oh we are so dead  */
				crm_crit("Couldn't re-initialize the old CIB!");
				cl_flush_logs();
				exit(1);
			}
			
		} else {
			crm_crit("Could not write out new CIB and no saved"
				 " version to revert to");
		}
		
	} else if(per_action_cib && cib_writes_enabled && cib_status == cib_ok) {
		crm_err("Per-action CIB");
		write_cib_contents(the_cib);
		
	} else if(cib_writes_enabled && cib_status == cib_ok) {
		crm_debug_2("Triggering CIB write");
		G_main_set_trigger(cib_writer);
	}
#if CIB_MEM_STATS
	/* this chews through a bunch of CPU */
	if(the_cib == new_cib) {
		long new_bytes, new_allocs, new_frees;
		long old_bytes, old_allocs, old_frees;
		crm_xml_nbytes(new_cib, &new_bytes, &new_allocs, &new_frees);
		crm_xml_nbytes(saved_cib, &old_bytes, &old_allocs, &old_frees);

		if(new_bytes != old_bytes) {
			crm_info("CIB size is %ld bytes (was %ld)", new_bytes, old_bytes);
			crm_adjust_mem_stats(NULL, new_bytes - old_bytes,
					     new_allocs - old_allocs, new_frees - old_frees);
			if(crm_running_stats != NULL) {
				crm_adjust_mem_stats(
					crm_running_stats, new_bytes - old_bytes,
					new_allocs - old_allocs, new_frees - old_frees);
			}
		}
	}
#endif
	
	if(the_cib != saved_cib && the_cib != new_cib) {
		CRM_DEV_ASSERT(error_code != cib_ok);
		CRM_DEV_ASSERT(the_cib == NULL);
	}
	
	if(the_cib != new_cib) {
		free_xml(new_cib);
		CRM_DEV_ASSERT(error_code != cib_ok);
	}

	if(the_cib != saved_cib) {
		free_xml(saved_cib);
	}
	
	return error_code;
    
}

int
write_cib_contents(gpointer p) 
{
	int rc = 0;
	char *digest = NULL;
	crm_data_t *cib_status_root = NULL;
	const char *digest_filename = CIB_FILENAME ".sig";

	/* we can scribble on "the_cib" here and not affect the parent */
	const char *epoch = crm_element_value(the_cib, XML_ATTR_GENERATION);
	const char *updates = crm_element_value(the_cib, XML_ATTR_NUMUPDATES);
	const char *admin_epoch = crm_element_value(
		the_cib, XML_ATTR_GENERATION_ADMIN);

	/* check the admin didnt modify it underneath us */
	if(validate_on_disk_cib(CIB_FILENAME, NULL) == FALSE) {
		crm_err("%s was manually modified while Heartbeat was active!",
			CIB_FILENAME);
		exit(LSB_EXIT_GENERIC);
	}

	rc = archive_file(CIB_FILENAME, NULL, "last");
	if(rc != 0) {
		crm_err("Could not make backup of the existing CIB: %d", rc);
		exit(LSB_EXIT_GENERIC);
	}

	rc = archive_file(digest_filename, NULL, "last");
	if(rc != 0) {
		crm_warn("Could not make backup of the existing CIB digest: %d",
			rc);
	}

	/* Given that we discard the status section on startup
	 *   there is no point writing it out in the first place
	 *   since users just get confused by it
	 *
	 * Although, it does help me once in a while
	 *
	 * So delete the status section before we write it out
	 */
	if(p == NULL) {
		cib_status_root = find_xml_node(
			the_cib, XML_CIB_TAG_STATUS, TRUE);
		CRM_DEV_ASSERT(cib_status_root != NULL);
		
		if(cib_status_root != NULL) {
			free_xml_from_parent(the_cib, cib_status_root);
		}
	}
	
	rc = write_xml_file(the_cib, CIB_FILENAME, FALSE);
	if(rc <= 0) {
		crm_err("Changes couldn't be written to disk");
		exit(LSB_EXIT_GENERIC);
	}

	digest = calculate_xml_digest(the_cib, FALSE);
	crm_info("Wrote version %s.%s.%s of the CIB to disk (digest: %s)",
		 admin_epoch?admin_epoch:"0",
		 epoch?epoch:"0", updates?updates:"0", digest);	
	
	rc = write_cib_digest(the_cib, digest);
	crm_free(digest);

	if(rc <= 0) {
		crm_err("Digest couldn't be written to disk");
		exit(LSB_EXIT_GENERIC);
	}

#if 0
	if(validate_on_disk_cib(CIB_FILENAME, NULL) == FALSE) {
		crm_err("wrote incorrect digest");
		exit(LSB_EXIT_GENERIC);
	}
#endif
	if(p == NULL) {
		exit(LSB_EXIT_OK);
	}
	
	return HA_OK;
}

gboolean
set_transition(crm_data_t *xml_obj)
{
	const char *current = NULL;
	if(xml_obj == NULL) {
		return FALSE;
	}

	current = crm_element_value(xml_obj, XML_ATTR_CCM_TRANSITION);
	if(safe_str_neq(current, ccm_transition_id)) {
		crm_debug("CCM transition: old=%s, new=%s",
			  current, ccm_transition_id);
		crm_xml_add(xml_obj, XML_ATTR_CCM_TRANSITION,ccm_transition_id);
		return TRUE;
	}
	return FALSE;
}

gboolean
set_connected_peers(crm_data_t *xml_obj)
{
	int active = 0;
	int current = 0;
	char *peers_s = NULL;
	const char *current_s = NULL;
	if(xml_obj == NULL) {
		return FALSE;
	}
	
	current_s = crm_element_value(xml_obj, XML_ATTR_NUMPEERS);
	g_hash_table_foreach(peer_hash, GHFunc_count_peers, &active);
	current = crm_parse_int(current_s, "0");
	if(current != active) {
		peers_s = crm_itoa(active);
		crm_xml_add(xml_obj, XML_ATTR_NUMPEERS, peers_s);
		crm_debug("We now have %s active peers", peers_s);
		crm_free(peers_s);
		return TRUE;
	}
	return FALSE;
}

gboolean
update_quorum(crm_data_t *xml_obj) 
{
	const char *quorum_value = XML_BOOLEAN_FALSE;
	const char *current = NULL;
	if(xml_obj == NULL) {
		return FALSE;
	}
	
	current = crm_element_value(xml_obj, XML_ATTR_HAVE_QUORUM);
	if(cib_have_quorum) {
		quorum_value = XML_BOOLEAN_TRUE;
	}
	if(safe_str_neq(current, quorum_value)) {
		crm_debug("CCM quorum: old=%s, new=%s",
			  current, quorum_value);
		crm_xml_add(xml_obj, XML_ATTR_HAVE_QUORUM, quorum_value);
		return TRUE;
	}
	return FALSE;
}


gboolean
update_counters(const char *file, const char *fn, crm_data_t *xml_obj) 
{
	gboolean did_update = FALSE;

	did_update = did_update || update_quorum(xml_obj);
	did_update = did_update || set_transition(xml_obj);
	did_update = did_update || set_connected_peers(xml_obj);
	
	if(did_update) {
		do_crm_log(LOG_DEBUG, "Counters updated by %s", fn);
	}
	return did_update;
}



void GHFunc_count_peers(gpointer key, gpointer value, gpointer user_data)
{
	int *active = user_data;
	if(safe_str_eq(value, ONLINESTATUS)) {
		(*active)++;
		
	} else if(safe_str_eq(value, JOINSTATUS)) {
		(*active)++;
	}
}

