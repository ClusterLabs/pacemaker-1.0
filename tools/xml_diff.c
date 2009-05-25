
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
#include <sys/param.h>
#include <sys/types.h>

#include <crm/crm.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/common/ipc.h>
#include <crm/cib.h>

static struct crm_option long_options[] = {
    /* Top-level Options */
    {"help",           0, 0, '?', "\t\tThis text"},
    {"version",        0, 0, '$', "\t\tVersion information"  },
    {"verbose",        0, 0, 'V', "\t\tIncrease debug output\n"},

    {"-spacer-",	1, 0, '-', "\nOriginal XML:"},
    {"original",	1, 0, 'o', "\tXML is contained in the named file"},
    {"original-string", 1, 0, 'O', "XML is contained in the supplied string"},

    {"-spacer-",	1, 0, '-', "\nOperation:"},
    {"new",		1, 0, 'n', "\tCompare the original XML to the contents of the named file"},
    {"new-string",      1, 0, 'N', "\tCompare the original XML to the contents of the supplied string"},
    {"patch",		1, 0, 'p', "\tPatch the original XML with the contents of the named file"},

    {"-spacer-", 1, 0, '-', "\nAdditional Options:"},
    {"cib",	 0, 0, 'c', "\t\tCompare/patch the inputs as a CIB (includes versions details)"},
    {"filter",	 0, 0, 'f', "\t\tSuppress irrelevant differences between the two inputs"},
    {"stdin",	 0, 0, 's', NULL, 1},
    {"-spacer-", 1, 0, '-', "\nExamples:", pcmk_option_paragraph},
    {"-spacer-", 1, 0, '-', "Obtain the two different configuration files by running cibadmin on the two cluster setups to compare:", pcmk_option_paragraph},
    {"-spacer-", 1, 0, '-', " cibadmin --query > cib-old.xml", pcmk_option_example},
    {"-spacer-", 1, 0, '-', " cibadmin --query > cib-new.xml", pcmk_option_example},
    {"-spacer-", 1, 0, '-', "Calculate and save the difference between the two files:", pcmk_option_paragraph},
    {"-spacer-", 1, 0, '-', " crm_diff --original cib-old.xml --new cib-new.xml > patch.xml", pcmk_option_example },
    {"-spacer-", 1, 0, '-', "Apply the patch to the original file:", pcmk_option_paragraph },
    {"-spacer-", 1, 0, '-', " crm_diff --original cib-old.xml --patch patch.xml > updated.xml", pcmk_option_example },
    {"-spacer-", 1, 0, '-', "Apply the patch to the running cluster:", pcmk_option_paragraph },
    {"-spacer-", 1, 0, '-', " cibadmin --patch patch.xml", pcmk_option_example },

    {0, 0, 0, 0}
};

int
main(int argc, char **argv)
{
	gboolean apply = FALSE;
	gboolean raw_1  = FALSE;
	gboolean raw_2  = FALSE;
	gboolean filter = FALSE;
	gboolean use_stdin = FALSE;
	gboolean as_cib = FALSE;
	int argerr = 0;
	int flag;
	xmlNode *object_1 = NULL;
	xmlNode *object_2 = NULL;
	xmlNode *output = NULL;
	const char *xml_file_1 = NULL;
	const char *xml_file_2 = NULL;

	int option_index = 0;

	crm_log_init("crm_diff", LOG_CRIT-1, FALSE, FALSE, 0, NULL);
	crm_set_options("V?$o:n:p:scfO:N:", "original_xml operation [options]", long_options,
			"A tool for determining the differences between two xml files and/or applying the differences like a patch\n");

	if(argc < 2) {
		crm_help('?', LSB_EXIT_EINVAL);
	}

	while (1) {
		flag = crm_get_option(argc, argv, &option_index);
		if (flag == -1)
			break;

		switch(flag) {
			case 'o':
				xml_file_1 = optarg;
				break;
			case 'O':
				xml_file_1 = optarg;
				raw_1 = TRUE;
				break;
			case 'n':
				xml_file_2 = optarg;
				break;
			case 'N':
				xml_file_2 = optarg;
				raw_2 = TRUE;
				break;
			case 'p':
				xml_file_2 = optarg;
				apply = TRUE;
				break;
			case 'f':
				filter = TRUE;
				break;
			case 's':
				use_stdin = TRUE;
				break;
			case 'c':
				as_cib = TRUE;
				break;
			case 'V':
				cl_log_enable_stderr(TRUE);
				alter_debug(DEBUG_INC);
				break;				
			case '?':
			case '$':
				crm_help(flag, LSB_EXIT_OK);
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

	if (argerr) {
		crm_help('?', LSB_EXIT_GENERIC);
	}

	if(raw_1) {
	    object_1 = string2xml(xml_file_1);

	} else if(use_stdin) {
	    fprintf(stderr, "Input first XML fragment:");
	    object_1 = stdin2xml();

	} else if(xml_file_1 != NULL) {
	    object_1 = filename2xml(xml_file_1);
	}
	
	if(raw_2) {
	    object_2 = string2xml(xml_file_2);
	    
	} else if(use_stdin) {
	    fprintf(stderr, "Input second XML fragment:");
	    object_2 = stdin2xml();

	} else if(xml_file_2 != NULL) {
	    object_2 = filename2xml(xml_file_2);
	}
	
	if(object_1 == NULL) {
	    fprintf(stderr, "Could not parse the first XML fragment");
	    return 1;
	}
	if(object_2 == NULL) {
	    fprintf(stderr, "Could not parse the second XML fragment");
	    return 1;
	}

	if(apply) {
		if(as_cib == FALSE) {
			apply_xml_diff(object_1, object_2, &output);
		} else {
			apply_cib_diff(object_1, object_2, &output);
		}
		
	} else {
		if(as_cib == FALSE) {
			output = diff_xml_object(object_1, object_2, filter);
		} else {
			output = diff_cib_object(object_1, object_2, filter);
		}
	}
	
	if(output != NULL) {
		char *buffer = dump_xml_formatted(output);
		fprintf(stdout, "%s", crm_str(buffer));
		crm_free(buffer);
	}

	free_xml(object_1);
	free_xml(object_2);
	free_xml(output);
	
	if(apply == FALSE && output != NULL) {
		return 1;
	}
	
	return 0;	
}
