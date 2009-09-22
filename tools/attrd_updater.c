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
#include <libgen.h>

#include <sys/param.h>
#include <sys/types.h>

#include <crm/crm.h>
#include <crm/common/ipc.h>

#include <attrd.h>

const char *attr_name = NULL;
const char *attr_value = NULL;
const char *attr_set = NULL;
const char *attr_section = NULL;
const char *attr_dampen = NULL;
char command = 'q';

static struct crm_option long_options[] = {
    /* Top-level Options */
    {"help",    0, 0, '?', "\tThis text"},
    {"version", 0, 0, '$', "\tVersion information"  },
    {"verbose", 0, 0, 'V', "\tIncrease debug output\n"},
    {"quiet",   0, 0, 'q', "\tPrint only the value on stdout\n"},

    {"name",    1, 0, 'n', "The attribute's name"},

    {"-spacer-",1, 0, '-', "\nCommands:"},
    {"update",  1, 0, 'U', "Update the attribute's value in attrd.  If this causes the value to change, it will also be updated in the cluster configuration"},
    {"query",   0, 0, 'Q', "\tQuery the attribute's value from attrd"},
    {"delete",  0, 0, 'D', "\tDelete the attribute in attrd.  If a value was previously set, it will also be removed from the cluster configuration"},
    {"refresh", 0, 0, 'R', "\t(Advanced) Force the attrd daemon to resend all current values to the CIB\n"},    
    
    {"-spacer-",1, 0, '-', "\nAdditional options:"},
    {"lifetime",1, 0, 'l', "Lifetime of the node attribute.  Allowed values: forever, reboot"},
    {"delay",   1, 0, 'd', "The time to wait (dampening) in seconds further changes occur"},
    {"set",     1, 0, 's', "(Advanced) The attribute set in which to place the value"},

    /* Legacy options */
    {"update",  1, 0, 'v', NULL, 1},
    {"section", 1, 0, 'S', NULL, 1},
    {0, 0, 0, 0}
};

int
main(int argc, char ** argv)
{
    int index = 0;
    int argerr = 0;
    int flag;
    int BE_QUIET = FALSE;
	
    crm_system_name = basename(argv[0]);
    crm_set_options("?$Vqn:v:d:s:S:RDQU:l:", "command -n attribute [options]", long_options, "Tool for updating cluster node attributes");

    if(argc < 2) {
	crm_help('?', LSB_EXIT_EINVAL);
    }

    while (1) {
	flag = crm_get_option(argc, argv,  &index);
	if (flag == -1)
	    break;

	switch(flag) {
	    case 'V':
		alter_debug(DEBUG_INC);
		break;
	    case '?':
	    case '$':
		crm_help(flag, LSB_EXIT_OK);
	    break;
	    case 'n':
		attr_name = crm_strdup(optarg);
		break;
	    case 's':
		attr_set = crm_strdup(optarg);
		break;
	    case 'd':
		attr_dampen = crm_strdup(optarg);
		break;
	    case 'l':
	    case 'S':
		attr_section = crm_strdup(optarg);
		break;
	    case 'q':
		BE_QUIET = TRUE;
		break;
	    case 'Q':
	    case 'R':
	    case 'D':
	    case 'U':
	    case 'v':
		command = flag;
		attr_value = optarg;
		break;
	    default:
		++argerr;
		break;
	}
    }

    if(BE_QUIET == FALSE) {
	crm_log_init(basename(argv[0]), LOG_ERR, FALSE, FALSE, argc, argv);
    } else {
	crm_log_init(basename(argv[0]), LOG_ERR, FALSE, FALSE, 0, NULL);
    }
    
    if (optind > argc) {
	++argerr;
    }

    if(command != 'R' && attr_name == NULL) {
	++argerr;
    }
	
    if (argerr) {
	crm_help('?', LSB_EXIT_GENERIC);
    }
    
    if(command == 'Q') {
	fprintf(stderr, "-Q,--query is not yet implemented, use -D to delete existing values\n\n");
	crm_help('?', LSB_EXIT_GENERIC);

    } else if(attrd_lazy_update(command, NULL, attr_name, attr_value, attr_section, attr_set, attr_dampen) == FALSE) {
	fprintf(stderr, "Could not update %s=%s\n", attr_name, attr_value);
	return 1;
    }
    
    return 0;
}
