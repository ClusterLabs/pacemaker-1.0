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
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <libgen.h> /* for basename() */

#include <crm/crm.h>
#include <crm/ais.h>
#include <crm/common/cluster.h>
#include <crm/cib.h>

int command = 0;
int ccm_fd = 0;
int try_hb = 1;
int try_ais = 1;
gboolean do_quiet = FALSE;

char *target_uuid = NULL;
char *target_uname = NULL;
const char *standby_value = NULL;
const char *standby_scope = NULL;

#include <../lib/common/stack.h>

static struct crm_option long_options[] = {
    /* Top-level Options */
    {"help",       0, 0, '?', "\tThis text"},
    {"version",    0, 0, '$', "\tVersion information"  },
    {"verbose",    0, 0, 'V', "\tIncrease debug output"},
    {"quiet",      0, 0, 'Q', "\tEssential output only"},

    {"-spacer-",   1, 0, '-', "\nStack:", SUPPORT_HEARTBEAT},
    {"openais",    0, 0, 'A', "\tOnly try connecting to an OpenAIS-based cluster", SUPPORT_HEARTBEAT},
    {"heartbeat",  0, 0, 'H', "Only try connecting to a Heartbeat-based cluster", SUPPORT_HEARTBEAT},
    
    {"-spacer-",      1, 0, '-', "\nCommands:"},
    {"epoch",	      0, 0, 'e', "\tDisplay the epoch during which this node joined the cluster"},
    {"quorum",        0, 0, 'q', "\tDisplay a 1 if our partition has quorum, 0 if not"},
    {"list",          0, 0, 'l', "(AIS-Only) Display all known members (past and present) of this cluster"},
    {"partition",     0, 0, 'p', "Display the members of this partition"},
    {"cluster-id",    0, 0, 'i', "Display this node's cluster id"},
    {"remove",        1, 0, 'R', "(Advanced, AIS-Only) Remove the (stopped) node with the specified nodeid from the cluster"},

    {"-spacer-", 1, 0, '-', "\nAdditional Options:"},
    {"force",	 0, 0, 'f'},

    {0, 0, 0, 0}
};

int local_id = 0;


#if SUPPORT_HEARTBEAT
#  include <ocf/oc_event.h>
#  include <ocf/oc_membership.h>
#  include <clplumbing/cl_uuid.h>
#  define UUID_LEN 16

oc_ev_t *ccm_token = NULL;
void oc_ev_special(const oc_ev_t *, oc_ev_class_t , int );

static gboolean read_local_hb_uuid(void) 
{
    cl_uuid_t uuid;
    char *buffer = NULL;
    long start = 0, read_len = 0;

    FILE *input = fopen(UUID_FILE, "r");
	
    if(input == NULL) {
	crm_info("Could not open UUID file %s\n", UUID_FILE);
	return FALSE;
    }
	
    /* see how big the file is */
    start  = ftell(input);
    fseek(input, 0L, SEEK_END);
    if(UUID_LEN != ftell(input)) {
	fprintf(stderr, "%s must contain exactly %d bytes\n", UUID_FILE, UUID_LEN);
	abort();
    }
	
    fseek(input, 0L, start);
    if(start != ftell(input)) {
	fprintf(stderr, "fseek not behaving: %ld vs. %ld\n", start, ftell(input));
	exit(2);
    }

    buffer = malloc(50);
    read_len = fread(uuid.uuid, 1, UUID_LEN, input);
    if(read_len != UUID_LEN) {
	fprintf(stderr, "Expected and read bytes differ: %d vs. %ld\n",
		UUID_LEN, read_len);
	exit(3);
		
    } else if(buffer != NULL) {
	cl_uuid_unparse(&uuid, buffer);
	fprintf(stdout, "%s\n", buffer);
	return TRUE;
	
    } else {
	fprintf(stderr, "No buffer to unparse\n");
	exit(4);
    }

    free(buffer);
    fclose(input);

    return FALSE;
}

static void 
ccm_age_callback(oc_ed_t event, void *cookie, size_t size, const void *data)
{
    int lpc;
    int node_list_size;
    const oc_ev_membership_t *oc = (const oc_ev_membership_t *)data;

    node_list_size = oc->m_n_member;
    if(command == 'q') {
	crm_debug("Processing \"%s\" event.", 
		  event==OC_EV_MS_NEW_MEMBERSHIP?"NEW MEMBERSHIP":
		  event==OC_EV_MS_NOT_PRIMARY?"NOT PRIMARY":
		  event==OC_EV_MS_PRIMARY_RESTORED?"PRIMARY RESTORED":
		  event==OC_EV_MS_EVICTED?"EVICTED":
		  "NO QUORUM MEMBERSHIP");
	if(ccm_have_quorum(event)) {
	    fprintf(stdout, "1\n");
	} else {
	    fprintf(stdout, "0\n");
	}
		
    } else if(command == 'e') {
	crm_debug("Searching %d members for our birth", oc->m_n_member);
    }
    for(lpc=0; lpc<node_list_size; lpc++) {
	if(command == 'p') {
	    fprintf(stdout, "%s ",
		    oc->m_array[oc->m_memb_idx+lpc].node_uname);

	} else if(command == 'e') {
	    if(oc_ev_is_my_nodeid(ccm_token, &(oc->m_array[lpc]))){
		crm_debug("MATCH: nodeid=%d, uname=%s, born=%d",
			  oc->m_array[oc->m_memb_idx+lpc].node_id,
			  oc->m_array[oc->m_memb_idx+lpc].node_uname,
			  oc->m_array[oc->m_memb_idx+lpc].node_born_on);
		fprintf(stdout, "%d\n",
			oc->m_array[oc->m_memb_idx+lpc].node_born_on);
	    }
	}
    }

    oc_ev_callback_done(cookie);

    if(command == 'p') {
	fprintf(stdout, "\n");
    }
    fflush(stdout);
    exit(0);
}

static gboolean
ccm_age_connect(int *ccm_fd) 
{
    gboolean did_fail = FALSE;
    int ret = 0;
	
    crm_debug("Registering with CCM");
    ret = oc_ev_register(&ccm_token);
    if (ret != 0) {
	crm_info("CCM registration failed: %d", ret);
	did_fail = TRUE;
    }
	
    if(did_fail == FALSE) {
	crm_debug("Setting up CCM callbacks");
	ret = oc_ev_set_callback(ccm_token, OC_EV_MEMB_CLASS,
				 ccm_age_callback, NULL);
	if (ret != 0) {
	    crm_warn("CCM callback not set: %d", ret);
	    did_fail = TRUE;
	}
    }
    if(did_fail == FALSE) {
	oc_ev_special(ccm_token, OC_EV_MEMB_CLASS, 0/*don't care*/);
		
	crm_debug("Activating CCM token");
	ret = oc_ev_activate(ccm_token, ccm_fd);
	if (ret != 0){
	    crm_warn("CCM Activation failed: %d", ret);
	    did_fail = TRUE;
	}
    }
	
    return !did_fail;
}

static gboolean try_heartbeat(int command)
{
    crm_debug("Attempting to process %c command", command);
    
    if(command == 'i') {
	if(read_local_hb_uuid()) {
	    exit(0);
	}
	
    } else if(ccm_age_connect(&ccm_fd)) {
	int rc = 0;
	fd_set rset;	
	while (1) {
	    
	    sleep(1);
	    FD_ZERO(&rset);
	    FD_SET(ccm_fd, &rset);
	    
	    errno = 0;
	    rc = select(ccm_fd + 1, &rset, NULL,NULL,NULL);
	    
	    if(rc > 0 && oc_ev_handle_event(ccm_token) != 0) {
		crm_err("oc_ev_handle_event failed");
		exit(1);
		
	    } else if(rc < 0 && errno != EINTR) {
		crm_perror(LOG_ERR, "select failed");
		exit(1);
	    }
	    
	}
    }
    return FALSE;
}
#endif


#if SUPPORT_AIS
static void
ais_membership_destroy(gpointer user_data)
{
    crm_err("AIS connection terminated");
    ais_fd_sync = -1;
    exit(1);
}

static gint member_sort(gconstpointer a, gconstpointer b) 
{
    const crm_node_t *node_a = a;
    const crm_node_t *node_b = b;
    return strcmp(node_a->uname, node_b->uname);
}

static void crm_add_member(
    gpointer key, gpointer value, gpointer user_data)
{
    GList **list = user_data;
    crm_node_t *node = value;
    if(node->uname != NULL) {
	*list = g_list_insert_sorted(*list, node, member_sort);
    }
}

static gboolean
ais_membership_dispatch(AIS_Message *wrapper, char *data, int sender) 
{
    switch(wrapper->header.id) {
	case crm_class_members:
	case crm_class_notify:
	case crm_class_quorum:
	    break;
	default:
	    return TRUE;
	    
	    break;
    }

    if(command == 'q') {
	if(crm_have_quorum) {
	    fprintf(stdout, "1\n");
	} else {
	    fprintf(stdout, "0\n");
	}
		
    } else if(command == 'l') {
	GList *nodes = NULL;
	g_hash_table_foreach(crm_peer_cache, crm_add_member, &nodes);
	slist_iter(node, crm_node_t, nodes, lpc,
		   fprintf(stdout, "%u %s %s\n", node->id, node->uname, node->state);
	    );
	fprintf(stdout, "\n");

    } else if(command == 'p') {
	GList *nodes = NULL;
	g_hash_table_foreach(crm_peer_cache, crm_add_member, &nodes);
	slist_iter(node, crm_node_t, nodes, lpc,
		   if(node->uname && crm_is_member_active(node)) {
		       fprintf(stdout, "%s ", node->uname);
		   }
	    );
	fprintf(stdout, "\n");
    }

    exit(0);
    
    return TRUE;
}

static gboolean try_corosync(int command)
{
    crm_debug("Attempting to process %c command", command);

    if(CS_OK == init_ais_connection_once(
	   ais_membership_dispatch, ais_membership_destroy, NULL, NULL, &local_id)) {

	GMainLoop*  amainloop = NULL;
	switch(command) {
	    case 'R':
		send_ais_text(crm_class_rmpeer, target_uname, TRUE, NULL, crm_msg_ais);
		exit(0);
		    
	    case 'e':
		/* Age makes no sense (yet) in an AIS cluster */
		fprintf(stdout, "1\n");
		exit(0);
			
	    case 'q':
		send_ais_text(crm_class_quorum, NULL, TRUE, NULL, crm_msg_ais);
		break;

	    case 'l':
	    case 'p':
		crm_info("Requesting the list of configured nodes");
		send_ais_text(crm_class_members, __FUNCTION__, TRUE, NULL, crm_msg_ais);
		break;

	    case 'i':
		printf("%u\n", local_id);
		exit(0);

	    default:
		fprintf(stderr, "Unknown option '%c'\n", command);
		crm_help('?', LSB_EXIT_GENERIC);
	}
	amainloop = g_main_new(FALSE);
	g_main_run(amainloop);
    }
    return FALSE;
}
#endif

int
main(int argc, char ** argv)
{
    int flag = 0;
    int argerr = 0;
    gboolean force_flag = FALSE;
    gboolean dangerous_cmd = FALSE;

    int option_index = 0;

    crm_peer_init();
    crm_log_init(basename(argv[0]), LOG_WARNING, FALSE, FALSE, argc, argv);
    crm_set_options("?V$qepHAR:ifl", "command [options]", long_options,
		    "Tool for displaying low-level node information");
	
    while (flag >= 0) {
	flag = crm_get_option(argc, argv, &option_index);
	switch(flag) {
	    case -1:
		break;
	    case 'V':
		cl_log_enable_stderr(TRUE);
		alter_debug(DEBUG_INC);
		break;
	    case '$':
	    case '?':
		crm_help(flag, LSB_EXIT_OK);
		break;
	    case 'Q':
		do_quiet = TRUE;
		break;	
	    case 'H':
		try_ais = 0;
		break;
	    case 'A':
		try_hb = 0;
		break;
	    case 'f':
		force_flag = TRUE;
		break;
	    case 'R':
		dangerous_cmd = TRUE;
		command = flag;
		target_uname = optarg;
		break;
	    case 'p':
	    case 'e':
	    case 'q':
	    case 'i':
	    case 'l':
		command = flag;
	    break;
	    default:
		++argerr;
		break;
	}
    }
    
    if (optind > argc) {
	++argerr;
    }
    
    if (argerr) {
	crm_help('?', LSB_EXIT_GENERIC);
    }

    if(dangerous_cmd && force_flag == FALSE) {
	fprintf(stderr, "The supplied command is considered dangerous."
		"  To prevent accidental destruction of the cluster,"
		" the --force flag is required in order to proceed.\n");
	fflush(stderr);
	exit(LSB_EXIT_GENERIC);
    }
    
#if SUPPORT_AIS
    if(try_ais) {
	try_corosync(command);
    }
#endif

#if SUPPORT_HEARTBEAT
    if(try_hb) {
	try_heartbeat(command);
    }
#endif
    return(1);    
}
