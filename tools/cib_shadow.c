
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

#include <sys/param.h>

#include <crm/crm.h>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>




#include <crm/msg_xml.h>
#include <crm/common/xml.h>

#include <crm/common/ipc.h>

#include <crm/cib.h>

int exit_code = cib_ok;
GMainLoop *mainloop = NULL;
IPC_Channel *crmd_channel = NULL;

const char *host = NULL;
void usage(const char *cmd, int exit_status);

int command_options = cib_sync_call;
const char *cib_action = NULL;

cib_t *real_cib = NULL;

int dump_data_element(
    int depth, char **buffer, int *max, int *offset, const char *prefix, xmlNode *data, gboolean formatted);

void print_xml_diff(FILE *where, xmlNode *diff);

static int force_flag = 0;
static int batch_flag = 0;

static char *get_shadow_prompt(const char *name)
{
    int len = 16;
    char *prompt = NULL;
    CRM_ASSERT(name != NULL);
    
    len += strlen(name);
    crm_malloc0(prompt, len);
    
    snprintf(prompt, len, "shadow[%s] # ", name);
    return prompt;
}


static void shadow_setup(char *name, gboolean do_switch)
{
    const char *prompt = getenv("PS1");
    const char *shell = getenv("SHELL");
    char *new_prompt = get_shadow_prompt(name);
    printf("Setting up shadow instance\n");

    if(safe_str_eq(new_prompt, prompt)) {
	/* nothing to do */
	goto done;
	
    } else if(batch_flag == FALSE && shell != NULL) {
	setenv("PS1", new_prompt, 1);
	setenv("CIB_shadow", name, 1);
	printf("Type Ctrl-D to exit the crm_shadow shell\n");

	if(strstr(shell, "bash")) {
	    execl(shell, "--norc", "--noprofile", NULL);
	} else {
	    execl(shell, "--noprofile", NULL);
	}
	
    } else if (do_switch) {
	printf("To switch to the named shadow instance, paste the following into your shell:\n");

    } else {
	printf("A new shadow instance was created.  To begin using it paste the following into your shell:\n");
    }
    printf("  CIB_shadow=%s ; export CIB_shadow\n", name);

  done:
    crm_free(new_prompt);
}

static void shadow_teardown(char *name)
{
    const char *prompt = getenv("PS1");
    char *our_prompt = get_shadow_prompt(name);
    
    if(prompt != NULL && strstr(prompt, our_prompt)) {
	printf("Now type Ctrl-D to exit the crm_shadow shell\n");
	
    } else {
	printf("Please remember to unset the CIB_shadow variable by pasting the following into your shell:\n");
	printf("  unset CIB_shadow\n");
    }
    crm_free(our_prompt);
}

static struct crm_option long_options[] = {
    /* Top-level Options */
    {"help",    0, 0, '?', "\t\tThis text"},
    {"version", 0, 0, '$', "\t\tVersion information"  },
    {"verbose", 0, 0, 'V', "\t\tIncrease debug output"},
    
    {"-spacer-",	1, 0, '-', "\nQueries:"},
    {"which",   no_argument,       NULL, 'w', "\t\tIndicate the active shadow copy"},
    {"display", no_argument,       NULL, 'p', "\t\tDisplay the contents of the shadow copy"},
    {"edit",    no_argument,       NULL, 'E', "\t\tEdit the contents of the named shadow copy with your favorite $EDITOR"},
    {"diff",    no_argument,       NULL, 'd', "\t\tDisplay the changes in the shadow copy\n"},

    {"-spacer-",	1, 0, '-', "\nCommands:"},
    {"create",		required_argument, NULL, 'c', "\tCreate the named shadow copy of the active cluster configuration"},
    {"create-empty",	required_argument, NULL, 'e', "Create the named shadow copy with an empty cluster configuration"},
    {"commit",  required_argument, NULL, 'C', "\tUpload the contents of the named shadow copy to the cluster"},
    {"delete",  required_argument, NULL, 'D', "\tDelete the contents of the named shadow copy"},
    {"reset",   required_argument, NULL, 'r', "\tRecreate the named shadow copy from the active cluster configuration"},
    {"switch",  required_argument, NULL, 's', "\t(Advanced) Switch to the named shadow copy"},
    
    {"-spacer-",	1, 0, '-', "\nAdditional Options:"},
    {"force",	no_argument, NULL, 'f', "\t\t(Advanced) Force the action to be performed"},
    {"batch",   no_argument, NULL, 'b', "\t\t(Advanced) Don't spawn a new shell" },

    {"-spacer-",	1, 0, '-', "\nExamples:", pcmk_option_paragraph},
    {"-spacer-",	1, 0, '-', "Create a blank shadow configuration:", pcmk_option_paragraph},
    {"-spacer-",	1, 0, '-', " crm_shadow --create-empty myShadow", pcmk_option_example},
    {"-spacer-",	1, 0, '-', "Create a shadow configuration from the running cluster:", pcmk_option_paragraph},
    {"-spacer-",	1, 0, '-', " crm_shadow --create myShadow", pcmk_option_example},
    {"-spacer-",	1, 0, '-', "Display the current shadow configuration:", pcmk_option_paragraph},
    {"-spacer-",	1, 0, '-', " crm_shadow --display", pcmk_option_example},
    {"-spacer-",	1, 0, '-', "Discard the current shadow configuration (named myShadow):", pcmk_option_paragraph},
    {"-spacer-",	1, 0, '-', " crm_shadow --delete myShadow", pcmk_option_example},
    {"-spacer-",	1, 0, '-', "Upload the current shadow configuration (named myShadow) to the running cluster:", pcmk_option_paragraph},
    {"-spacer-",	1, 0, '-', " crm_shadow --commit myShadow", pcmk_option_example},
    
    {0, 0, 0, 0}
};

int
main(int argc, char **argv)
{
    int rc = 0;
    int flag;
    int argerr = 0;
    static int command = '?';
    char *shadow = NULL;
    char *shadow_file = NULL;
    gboolean dangerous_cmd = FALSE;
    struct stat buf;
    int option_index = 0;

    crm_log_init("crm_shadow", LOG_CRIT, FALSE, FALSE, argc, argv);
    crm_set_options("V$?bfwc:dr:C:D:ps:Ee:", "(query|command) [modifiers]", long_options,
		    "Perform configuration changes in a sandbox before updating the live cluster."
		    "\n\nSets up an environment in which configuration tools (cibadmin, crm_resource, etc) work"
		    " offline instead of against a live cluster, allowing changes to be previewed and tested"
		    " for side-effects.\n");
    
    if(argc < 2) {
	crm_help('?', LSB_EXIT_EINVAL);
    }

    while (1) {
	flag = crm_get_option(argc, argv, &option_index);
	if (flag == -1 || flag == 0)
	    break;

	switch(flag) {
	    case 'd':
	    case 'E':
	    case 'p':
	    case 'w':
		command = flag;
		shadow = crm_strdup(getenv("CIB_shadow"));
		break;
	    case 'e':
	    case 'c':
	    case 's':
	    case 'r':
		command = flag;
		shadow = crm_strdup(optarg);
		break;
	    case 'C':
	    case 'D':
		command = flag;
		dangerous_cmd = TRUE;
		shadow = crm_strdup(optarg);
		break;
	    case 'V':
		command_options = command_options | cib_verbose;
		cl_log_enable_stderr(TRUE);
		alter_debug(DEBUG_INC);
		break;
	    case '$':
	    case '?':
		crm_help(flag, LSB_EXIT_OK);
		break;
	    case 'f':
		command_options |= cib_quorum_override;
		force_flag = 1;
		break;
	    case 'b':
		batch_flag = 1;
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
	crm_help('?', LSB_EXIT_EINVAL);
    }

    if (optind > argc) {
	++argerr;
    }
	
    if (argerr) {
	crm_help('?', LSB_EXIT_GENERIC);
    }

    if(command == 'w') {
	/* which shadow instance is active? */
	const char *local = getenv("CIB_shadow");
	if(local == NULL) {
	    fprintf(stderr, "No shadow instance provided\n");
	    return cib_NOTEXISTS;
	}
	fprintf(stdout, "%s\n", local);
	return 0;
    }
    
    if(shadow == NULL) {
	fprintf(stderr, "No shadow instance provided\n");
	fflush(stderr);
	return CIBRES_MISSING_FIELD;

    } else if(command != 's' && command != 'c') {
	const char *local = getenv("CIB_shadow");
	if(local != NULL && safe_str_neq(local, shadow) && force_flag == FALSE) {
	    fprintf(stderr, "The supplied shadow instance (%s) is not the same as the active one (%s).\n"
		    "  To prevent accidental destruction of the cluster,"
		    " the --force flag is required in order to proceed.\n", shadow, local);
	    fflush(stderr);
	    exit(LSB_EXIT_GENERIC);
	}
    }

    if(dangerous_cmd && force_flag == FALSE) {
	fprintf(stderr, "The supplied command is considered dangerous."
		"  To prevent accidental destruction of the cluster,"
		" the --force flag is required in order to proceed.\n");
	fflush(stderr);
	exit(LSB_EXIT_GENERIC);
    }

    shadow_file = get_shadow_file(shadow);
    if(command == 'D') {
	/* delete the file */
	rc = stat(shadow_file, &buf);
	if(rc == 0) {
	    rc = unlink(shadow_file);
	    if(rc != 0) {
		fprintf(stderr, "Could not remove shadow instance '%s': %s\n", shadow, strerror(errno));
		return rc;
	    }
	}

	shadow_teardown(shadow);
	return rc;

    }

    if(command == 'd' || command == 'r' || command == 'c' || command == 'C') {
	real_cib = cib_new_no_shadow();
	rc = real_cib->cmds->signon(real_cib, crm_system_name, cib_command);
	if(rc != cib_ok) {
	    fprintf(stderr, "Signon to CIB failed: %s\n", cib_error2string(rc));
	    return rc;
	}
    }
    
    rc = stat(shadow_file, &buf);

    if(command == 'e' || command == 'c') {
	if (rc == 0 && force_flag == FALSE) {
	    fprintf(stderr, "A shadow instance '%s' already exists.\n"
		   "  To prevent accidental destruction of the cluster,"
		   " the --force flag is required in order to proceed.\n", shadow);
	    return cib_EXISTS;
	}

    } else if(rc != 0) {
	fprintf(stderr, "Could not access shadow instance '%s': %s\n", shadow, strerror(errno));
	return cib_NOTEXISTS;
    }

    rc = cib_ok;
    if(command == 'c' || command == 'e') {
	xmlNode *output = NULL;

	/* create a shadow instance based on the current cluster config */
	if(command == 'c') {
	    rc = real_cib->cmds->query(real_cib, NULL, &output, command_options);
	    if(rc != cib_ok) {
		fprintf(stderr, "Could not connect to the CIB: %s\n", cib_error2string(rc));
		return rc;
	    }

	} else {
	    output = createEmptyCib();
	    crm_xml_add(output, XML_ATTR_GENERATION, "0");
	    crm_xml_add(output, XML_ATTR_NUMUPDATES, "0");
	    crm_xml_add(output, XML_ATTR_GENERATION_ADMIN, "0");
	    crm_xml_add(output, XML_ATTR_VALIDATION, LATEST_SCHEMA_VERSION);
	}
	
	rc = write_xml_file(output, shadow_file, FALSE);
	if(rc < 0) {
	    fprintf(stderr, "Could not create the shadow instance '%s': %s\n",
		    shadow, strerror(errno));
	    return rc;
	}
	shadow_setup(shadow, FALSE);
	rc = cib_ok;
	
    } else if(command == 'E') {
	const char *err = NULL;
	char *editor = getenv("EDITOR");
	if(editor == NULL) {
	    fprintf(stderr, "No value for $EDITOR defined\n");
	    return cib_missing;
	}

	execlp(editor, "--", shadow_file, NULL);
	err = strerror(errno);
	fprintf(stderr, "Could not invoke $EDITOR (%s %s)\n", editor, shadow_file);
	return cib_missing;
	
    } else if(command == 's') {
	shadow_setup(shadow, TRUE);
	return 0;
    
    } else if(command == 'P') {
	/* display the current contents */
	char *output_s = NULL;
	xmlNode *output = filename2xml(shadow_file);
	
	output_s = dump_xml_formatted(output);
	printf("%s", output_s);
	
	crm_free(output_s);
	free_xml(output);
	
    } else if(command == 'd') {
	/* diff against cluster */
	xmlNode *diff = NULL;
	xmlNode *old_config = NULL;
	xmlNode *new_config = filename2xml(shadow_file);
	
	rc = real_cib->cmds->query(real_cib, NULL, &old_config, command_options);
	
	if(rc != cib_ok) {
	    fprintf(stderr, "Could not query the CIB: %s\n", cib_error2string(rc));
	    return rc;
	}

	diff = diff_xml_object(old_config, new_config, FALSE);
	if(diff != NULL) {
	    print_xml_diff(stdout, diff);
	    return 1;
	}
	return 0;	
	
    } else if(command == 'C') {
	/* commit to the cluster */
	xmlNode *input = filename2xml(shadow_file);
	rc = real_cib->cmds->replace(real_cib, NULL, input, command_options);
	if(rc != cib_ok) {
	    fprintf(stderr, "Could not commit shadow instance '%s' to the CIB: %s\n",
		    shadow, cib_error2string(rc));
	    return rc;
	}	
	shadow_teardown(shadow);
    }

    return rc;
}

#define bhead(buffer, offset) ((*buffer) + (*offset)) 
#define bremain(max, offset) ((*max) - (*offset)) 
#define update_buffer_head(len) do {					\
	int total = (*offset) + len + 1;				\
	if(total >= (*max)) { /* too late */				\
	    (*buffer) = EOS; return -1;					\
	} else if(((*max) - total) < 256) {				\
	    (*max) *= 10;						\
	    crm_realloc(*buffer, (*max));				\
	}								\
	(*offset) += len;						\
    } while(0)

extern int print_spaces(char *buffer, int depth, int max);

int
dump_data_element(
    int depth, char **buffer, int *max, int *offset, const char *prefix, xmlNode *data, gboolean formatted) 
{
    int printed = 0;
    int has_children = 0;
    const char *name = NULL;
    
    CRM_CHECK(data != NULL, return 0);
    
    name = crm_element_name(data);
    
    CRM_CHECK(name != NULL, return 0);
    CRM_CHECK(buffer != NULL && *buffer != NULL, return 0);
    
    crm_debug_5("Dumping %s...", name);

    if(prefix) {
	printed = snprintf(bhead(buffer, offset), bremain(max, offset), "%s", prefix);
	update_buffer_head(printed);
    }

    if(formatted) {
	printed = print_spaces(bhead(buffer, offset), depth, bremain(max, offset));
	update_buffer_head(printed);
    }
    
    printed = snprintf(bhead(buffer, offset), bremain(max, offset), "<%s", name);
    update_buffer_head(printed);
    
    xml_prop_iter(data, prop_name, prop_value,
		  crm_debug_5("Dumping <%s %s=\"%s\"...",
			      name, prop_name, prop_value);
		  printed = snprintf(bhead(buffer, offset), bremain(max, offset),  " %s=\"%s\"", prop_name, prop_value);
		  update_buffer_head(printed);
	);
    
    has_children = xml_has_children(data);
    printed = snprintf(bhead(buffer, offset), bremain(max, offset),  "%s>%s",
		       has_children==0?"/":"", formatted?"\n":"");
    update_buffer_head(printed);
    
    if(has_children == 0) {
	return 0;
    }
    
    xml_child_iter(data, child, 
		   if(dump_data_element(depth+1, buffer, max, offset, prefix, child, formatted) < 0) {
		       return -1;
		   }
	);
    
    if(prefix) {
	printed = snprintf(bhead(buffer, offset), bremain(max, offset), "%s", prefix);
	update_buffer_head(printed);
    }

    if(formatted) {
	printed = print_spaces(bhead(buffer, offset), depth, bremain(max, offset));
	update_buffer_head(printed);
    }
    
    printed = snprintf(bhead(buffer, offset), bremain(max, offset),  "</%s>%s", name, formatted?"\n":"");
    update_buffer_head(printed);
    crm_debug_5("Dumped %s...", name);
    
    return has_children;
}

void
print_xml_diff(FILE *where, xmlNode *diff)
{
	char *buffer = NULL;
	int max = 1024, len = 0;
	gboolean is_first = TRUE;
	xmlNode *added = find_xml_node(diff, "diff-added", FALSE);
	xmlNode *removed = find_xml_node(diff, "diff-removed", FALSE);

	is_first = TRUE;
	xml_child_iter(
		removed, child, 

		len = 0;
		max = 1024;
		crm_free(buffer);
		crm_malloc0(buffer, max);

		if(is_first) {
		    is_first = FALSE;
		} else {
		    fprintf(where, " --- \n");
		}

		CRM_CHECK(dump_data_element(
			      0, &buffer, &max, &len, "-", child, TRUE) >= 0,
			  continue);
		fprintf(where, "%s", buffer);
		);

	is_first = TRUE;
	xml_child_iter(
		added, child, 

		len = 0;
		max = 1024;
		crm_free(buffer);
		crm_malloc0(buffer, max);

		if(is_first) {
		    is_first = FALSE;
		} else {
		    fprintf(where, " +++ \n");
		}

		CRM_CHECK(dump_data_element(
			      0, &buffer, &max, &len, "+", child, TRUE) >= 0,
			  continue);
		fprintf(where, "%s", buffer);
		);
}

