/* $Id: cibadmin.c,v 1.25 2005/02/20 14:38:54 andrew Exp $ */

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

#include <sys/param.h>

#include <crm/crm.h>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <hb_api.h>
#include <clplumbing/uids.h>
#include <clplumbing/Gmain_timeout.h>

#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/common/ctrl.h>
#include <crm/common/ipc.h>

#include <crm/cib.h>

#include <getopt.h>
#include <ha_msg.h> /* someone complaining about _ha_msg_mod not being found */
#include <crm/dmalloc_wrapper.h>

int exit_code = cib_ok;
int message_timer_id = -1;
int message_timeout_ms = 30*1000;

GMainLoop *mainloop = NULL;
const char *crm_system_name = "cibadmin";
IPC_Channel *crmd_channel = NULL;

const char *host = NULL;
void usage(const char *cmd, int exit_status);
enum cib_errors do_init(void);
int do_work(const char *xml_text, int command_options, crm_data_t **output);

gboolean admin_msg_callback(IPC_Channel * source_data, void *private_data);
crm_data_t *handleCibMod(const char *xml);
gboolean admin_message_timeout(gpointer data);
void cib_connection_destroy(gpointer user_data);
void cibadmin_op_callback(
	const HA_Message *msg, int call_id, int rc, crm_data_t *output);

int command_options = 0;
const char *cib_action = NULL;

typedef struct str_list_s
{
		int num_items;
		char *value;
		struct str_list_s *next;
} str_list_t;

char *id = NULL;
char *this_msg_reference = NULL;
char *obj_type = NULL;
char *clear = NULL;
char *status = NULL;
char *migrate_from = NULL;
char *migrate_res = NULL;
char *subtype = NULL;
char *reset = NULL;

int request_id = 0;
int operation_status = 0;
cib_t *the_cib = NULL;

#define OPTARGS	"V?i:o:QDUCEX:t:Srwlsh:MB"

int
main(int argc, char **argv)
{
	int option_index = 0;
	int argerr = 0;
	int flag;
	char *admin_input_xml = NULL;
	crm_data_t *output = NULL;
	
	static struct option long_options[] = {
		/* Top-level Options */
		{CRM_OP_CIB_ERASE,   0, 0, 'E'},
		{CRM_OP_CIB_QUERY,   0, 0, 'Q'},
		{CRM_OP_CIB_CREATE,  0, 0, 'C'},
		{CRM_OP_CIB_REPLACE, 0, 0, 'R'},
		{CRM_OP_CIB_UPDATE,  0, 0, 'U'},
		{CRM_OP_CIB_DELETE,  0, 0, 'D'},
		{CRM_OP_CIB_BUMP,    0, 0, 'B'},
		{CRM_OP_CIB_SYNC,    0, 0, 'S'},
		{CRM_OP_CIB_SLAVE,   0, 0, 'r'},
		{CRM_OP_CIB_MASTER,  0, 0, 'w'},
		{CRM_OP_CIB_ISMASTER,0, 0, 'M'},
		
		{"local",	 0, 0, 'l'},
		{"sync-call",	 0, 0, 's'},
		{"host",	 0, 0, 'h'},
		{F_CRM_DATA,          1, 0, 'X'},
		{"verbose",      0, 0, 'V'},
		{"help",         0, 0, '?'},
		{"reference",    1, 0, 0},
		{XML_ATTR_TIMEOUT,	 1, 0, 't'},

		/* common options */
		{XML_ATTR_ID, 1, 0, 'i'},
		{"obj_type", 1, 0, 'o'},

		{0, 0, 0, 0}
	};

	crm_log_init(crm_system_name);
	
	if(argc < 2) {
		usage(crm_system_name, LSB_EXIT_EINVAL);
	}

	while (1) {
		flag = getopt_long(argc, argv, OPTARGS,
				   long_options, &option_index);
		if (flag == -1)
			break;

		switch(flag) {
			case 0:
				printf("option %s",
				       long_options[option_index].name);
				if (optarg)
					printf(" with arg %s", optarg);
				printf("\n");
	if (safe_str_eq("reference", long_options[option_index].name)) {
		this_msg_reference = crm_strdup(optarg);

	} else {
		printf("Long option (--%s) is not (yet?) properly supported\n",
		       long_options[option_index].name);
		++argerr;
	}
	break;
			case 't':
				message_timeout_ms = atoi(optarg);
				if(message_timeout_ms < 1) {
					message_timeout_ms = 30*1000;
				}
				break;
				
			case 'E':
				cib_action = CRM_OP_CIB_ERASE;
				break;
			case 'Q':
				cib_action = CRM_OP_CIB_QUERY;
				break;
			case 'S':
				cib_action = CRM_OP_CIB_SYNC;
				break;
			case 'U':
				cib_action = CRM_OP_CIB_UPDATE;
				break;
			case 'R':
				cib_action = CRM_OP_CIB_REPLACE;
				break;
			case 'C':
				cib_action = CRM_OP_CIB_CREATE;
				break;
			case 'D':
				cib_action = CRM_OP_CIB_DELETE;
				break;
			case 'M':
				cib_action = CRM_OP_CIB_ISMASTER;
				command_options |= cib_scope_local;
				break;
			case 'B':
				cib_action = CRM_OP_CIB_BUMP;
				break;
			case 'r':
				cib_action = CRM_OP_CIB_SLAVE;
				break;
			case 'w':
				cib_action = CRM_OP_CIB_MASTER;
				command_options |= cib_scope_local;
				break;
			case 'V':
				command_options = command_options | cib_verbose;
				cl_log_enable_stderr(TRUE);
				alter_debug(DEBUG_INC);
				break;
			case '?':
				usage(crm_system_name, LSB_EXIT_OK);
				break;
			case 'i':
				crm_verbose("Option %c => %s", flag, optarg);
				id = crm_strdup(optarg);
				break;
			case 'o':
				crm_verbose("Option %c => %s", flag, optarg);
				obj_type = crm_strdup(optarg);
				break;
			case 'X':
				admin_input_xml = crm_strdup(optarg);
				break;
			case 'h':
				host = crm_strdup(optarg);
				break;
			case 'l':
				command_options |= cib_scope_local;
				break;
			case 's':
				command_options |= cib_sync_call;
				break;
			default:
				printf("Argument code 0%o (%c)"
				       " is not (?yet?) supported\n",
				       flag, flag);
				++argerr;
				break;
		}
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		printf("\n");
	}

	if (optind > argc) {
		++argerr;
	}

	if(cib_action == NULL) {
		usage(crm_system_name, cib_operation);
	}
	
	if (argerr) {
		usage(crm_system_name, LSB_EXIT_GENERIC);
	}

	exit_code = do_init();
	if(exit_code != cib_ok) {
		crm_err("Init failed, could not perform requested operations");
		fprintf(stderr, "Init failed, could not perform requested operations\n");
		return -exit_code;
	}

	exit_code = do_work(admin_input_xml, command_options, &output);
	if (exit_code > 0) {
		/* wait for the reply by creating a mainloop and running it until
		 * the callbacks are invoked...
		 */
		IPC_Channel *ch = the_cib->cmds->channel(the_cib);
		request_id = exit_code;
		
		if(ch == NULL) {
			crm_err("Connection to CIB is corrupt");
			return 2;
		}

		mainloop = g_main_new(FALSE);

		crm_devel("Setting operation timeout to %dms",
			  message_timeout_ms);

		message_timer_id = Gmain_timeout_add(
			message_timeout_ms, admin_message_timeout, NULL);

		crm_devel("%s waiting for reply from the local CIB",
			 crm_system_name);
		
		crm_info("Starting mainloop");
		g_main_run(mainloop);
		
	} else if(exit_code < 0) {
		crm_err("Call failed: %s", cib_error2string(exit_code));
		fprintf(stderr, "Call failed: %s\n",
			cib_error2string(exit_code));
		operation_status = exit_code;
	}


	if(output != NULL) {
		char *buffer = dump_xml_formatted(output);
		fprintf(stdout, "%s", crm_str(buffer));
		crm_free(buffer);
	}
	
	crm_devel("%s exiting normally", crm_system_name);
	return -exit_code;
}

crm_data_t*
handleCibMod(const char *xml)
{
	const char *attr_name = NULL;
	const char *attr_value = NULL;
	crm_data_t *fragment = NULL;
	crm_data_t *cib_object = NULL;

	if(xml == NULL) {
		cib_object = file2xml(stdin);
	} else {
		cib_object = string2xml(xml);
	}
	
	if(cib_object == NULL) {
		return NULL;
	}
	
	attr_name = XML_ATTR_ID;
	
	attr_value = crm_element_value(cib_object, attr_name);
	if(attr_name == NULL || strlen(attr_name) == 0) {
		crm_err("No value for %s specified.", attr_name);
		return NULL;
	}
	
	crm_trace("Object creation complete");

	/* create the cib request */
	fragment = create_cib_fragment(cib_object, NULL);

	return fragment;
}


int
do_work(const char *admin_input_xml, int call_options, crm_data_t **output) 
{
	/* construct the request */
	crm_data_t *msg_data = NULL;
	char *obj_type_parent = NULL;

	obj_type_parent = cib_pluralSection(obj_type);

	if(strcmp(CRM_OP_CIB_QUERY, cib_action) == 0) {
		crm_verbose("Querying the CIB for section: %s",
			    obj_type_parent);

		return the_cib->cmds->query_from(
			the_cib, host, obj_type_parent, output, call_options);
		
	} else if (strcmp(CRM_OP_CIB_ERASE, cib_action) == 0) {
		crm_trace("CIB Erase op in progress");
		return the_cib->cmds->erase(the_cib, output, call_options);
		
	} else if (strcmp(CRM_OP_CIB_CREATE, cib_action) == 0) {
		enum cib_errors rc = cib_ok;
		crm_trace("Performing %s op...", cib_action);
		msg_data = handleCibMod(admin_input_xml);
		rc = the_cib->cmds->create(
			the_cib, obj_type_parent, msg_data, output, call_options);
		free_xml(msg_data);
		return rc;

	} else if (strcmp(CRM_OP_CIB_UPDATE, cib_action) == 0) {
		enum cib_errors rc = cib_ok;
		crm_trace("Performing %s op...", cib_action);
		msg_data = handleCibMod(admin_input_xml);
		rc = the_cib->cmds->modify(
			the_cib, obj_type_parent, msg_data, output, call_options);
		free_xml(msg_data);
		return rc;

	} else if (strcmp(CRM_OP_CIB_DELETE, cib_action) == 0) {
		enum cib_errors rc = cib_ok;
		crm_trace("Performing %s op...", cib_action);
		msg_data = handleCibMod(admin_input_xml);
		rc = the_cib->cmds->delete(
			the_cib, obj_type_parent, msg_data, output, call_options);
		free_xml(msg_data);
		return rc;

	} else if (strcmp(CRM_OP_CIB_SYNC, cib_action) == 0) {
		crm_trace("Performing %s op...", cib_action);
		return the_cib->cmds->sync_from(
			the_cib, host, obj_type_parent, call_options);

	} else if (strcmp(CRM_OP_CIB_SLAVE, cib_action) == 0
		   && (call_options ^ cib_scope_local) ) {
		crm_trace("Performing %s op on all nodes...", cib_action);
		return the_cib->cmds->set_slave_all(the_cib, call_options);

	} else if (strcmp(CRM_OP_CIB_MASTER, cib_action) == 0) {
		crm_trace("Performing %s op on all nodes...", cib_action);
		return the_cib->cmds->set_master(the_cib, call_options);

	} else if(cib_action != NULL) {
		crm_trace("Passing \"%s\" to variant_op...", cib_action);
		return the_cib->cmds->variant_op(
			the_cib, cib_action, host, obj_type_parent,
			NULL, output, call_options);
		
	} else {
		crm_err("You must specify an operation");
	}
	return cib_operation;
}

enum cib_errors
do_init(void)
{
	enum cib_errors rc = cib_ok;
	
#ifdef USE_LIBXML
	/* docs say only do this once, but in their code they do it every time! */
	xmlInitParser(); 
#endif
	the_cib = cib_new();
	rc = the_cib->cmds->signon(the_cib, cib_command);
	if(rc != cib_ok) {
		crm_err("Signon to CIB failed: %s",
			cib_error2string(rc));
		fprintf(stderr, "Signon to CIB failed: %s\n",
			cib_error2string(rc));
	} else {
		rc = the_cib->cmds->set_op_callback(
			the_cib, cibadmin_op_callback);
		if(rc != cib_ok) {
			crm_err("Failed to set callback: %s",
				cib_error2string(rc));
			fprintf(stderr,"Failed to set callback: %s\n",
				cib_error2string(rc));
		}
	}
	
	return rc;
}


void
usage(const char *cmd, int exit_status)
{
	FILE *stream;

	stream = exit_status != 0 ? stderr : stdout;

	fprintf(stream, "usage: %s [-?Vio] command\n"
		"\twhere necessary, XML data will be expected using -X"
		" or on STDIN if -X isnt specified\n", cmd);

	fprintf(stream, "Options\n");
	fprintf(stream, "\t--%s (-%c) <id>\tid of the object being operated on\n",
		XML_ATTR_ID, 'i');
	fprintf(stream, "\t--%s (-%c) <type>\tobject type being operated on\n",
		"obj_type", 'o');
	fprintf(stream, "\t--%s (-%c)\tturn on debug info."
		"  additional instance increase verbosity\n", "verbose", 'V');
	fprintf(stream, "\t--%s (-%c)\tthis help message\n", "help", '?');
	fprintf(stream, "\nCommands\n");
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_ERASE,  'E');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_QUERY,  'Q');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_CREATE, 'C');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_REPLACE,'R');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_UPDATE, 'U');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_DELETE, 'D');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_BUMP,   'B');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_ISMASTER,'M');
	fprintf(stream, "\t--%s (-%c)\t\n", CRM_OP_CIB_SYNC,   'S');
	fprintf(stream, "\nXML data\n");
	fprintf(stream, "\t--%s (-%c) <string>\t\n", F_CRM_DATA, 'X');
	fprintf(stream, "\nAdvanced Options\n");
	fprintf(stream, "\t--%s (-%c)\tsend command to specified host."
		" Applies to %s and %s commands only\n", "host", 'h',
		CRM_OP_CIB_QUERY, CRM_OP_CIB_SYNC);
	fprintf(stream, "\t--%s (-%c)\tcommand only takes effect locally"
		" on the specified host\n", "local", 'l');
	fprintf(stream, "\t--%s (-%c)\twait for call to complete before"
		" returning\n", "sync-call", 's');

	fflush(stream);

	exit(exit_status);
}


gboolean
admin_message_timeout(gpointer data)
{
	if(safe_str_eq(cib_action, CRM_OP_CIB_SLAVE)) {
		exit_code = cib_ok;
		fprintf(stdout, "CIB service(s) are in slave mode.\n");
		
	} else {
		exit_code = cib_reply_failed;
		fprintf(stderr,
			"No messages received in %d seconds.. aborting\n",
			(int)message_timeout_ms/1000);
		crm_err("No messages received in %d seconds",
			(int)message_timeout_ms/1000);
	}
	
	
	
	g_main_quit(mainloop);
	return FALSE;
}


void
cib_connection_destroy(gpointer user_data)
{
	crm_err("Connection to the CIB terminated... exiting");
	g_main_quit(mainloop);
	return;
}

void cibadmin_op_callback(
	const HA_Message *msg, int call_id, int rc, crm_data_t *output)
{
	char *admin_input_xml = NULL;
	
	crm_info("our callback was invoked");
	crm_log_message(LOG_MSG, msg);
	exit_code = rc;

	if(output != NULL) {
		admin_input_xml = dump_xml_formatted(output);
	}
	
	if(safe_str_eq(cib_action, CRM_OP_CIB_ISMASTER) && rc != cib_ok) {
		crm_info("Local CIB is _not_ the master instance");
		fprintf(stderr, "Local CIB is _not_ the master instance\n");
		
	} else if(safe_str_eq(cib_action, CRM_OP_CIB_ISMASTER)) {
		crm_info("Local CIB _is_ the master instance\n");
		fprintf(stderr, "Local CIB _is_ the master instance\n");
		
	} else if(rc != 0) {
		crm_warn("Call %s failed (%d): %s\n",
			cib_action, rc, cib_error2string(rc));
		fprintf(stderr, "Call %s failed (%d): %s\n",
			cib_action, rc, cib_error2string(rc));
		fprintf(stdout, "%s\n",	crm_str(admin_input_xml));

	} else if(safe_str_eq(cib_action, CRM_OP_CIB_QUERY) && output==NULL) {
		crm_err("Output expected in query response");
		crm_log_message(LOG_ERR, msg);

	} else if(output == NULL) {
		crm_info("Call passed");

	} else {
		crm_info("Call passed");
		fprintf(stdout, "%s\n", crm_str(admin_input_xml));
	}
	crm_free(admin_input_xml);

	if(call_id == request_id) {
		g_main_quit(mainloop);

	} else {
		crm_info("Message was not the response we were looking for (%d vs. %d", call_id, request_id);
	}
}
