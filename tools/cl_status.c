/* File: cl_status.c
 * Description: 
 * 	A small tool for acquire the state information of heartbeat cluster.
 * TODO: Map string output to return value?
 *
 * Author: Sun Jiang Dong <sunjd@cn.ibm.com>
 * Copyright (c) 2004 International Business Machines
 * Referred to the following tools
 *	api_test Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *	hbinfo   Copyright (C) 2004 Mike Neuhauser <mike@firmix.at>
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_GET_OPT_H
#include <getopt.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <clplumbing/cl_log.h>
#include <hb_api.h>

/*
 * Important
 * General return value for the following functions, and it is actually 
 * as this program cl_status' return value:
 * 	0: on success.
 * 	>10: on error.
 *		11: heartbeat is stopped or cannot be signed on.   
 *		12: parameter error.
 *		13: other errors.
 */

/* 
 * Description: 
 * 	Detect if heartbeat is running.
 *
 * Parameters:
 *	Obvious. ;-) 
 *
 * Return Value:
 * 	<10: on success.
 *		0: In local machine, heartbeat is stopped.
 *		1: In local machine, heartbeat is running.
 */
static int hbstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: success
 */
static int listnodes(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: stopped
 *		1: running
 *		3: unknown status
 * Notes: not map string std_output to return value yet 
 */
static int nodestatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: normal
 *		1: ping
 *		3: unknown type
 * Notes: not map string std_output to return value yet 
 */
static int nodetype(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: success
 */
static int listhblinks(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: dead
 *		1: startup
 * Notes: not map string std_output to return value yet 
 */
static int hblinkstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: offline
 *		1: online
 *		2: join
 *		3: leave
 * Notes: not map string std_output to return value yet 
 */
static int clientstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: none
 *		1: local
 *		2: foreign
 *		3: all
 * Notes: not map string std_output to return value yet 
 */
static int rscstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	<10: on success.
 *		0: success
 */
static int hbparameter(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/* miscellaneous functions */
static int test(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);
static int general_simple_opt_deal(int argc, char ** argv, const char * optstr);

typedef struct {
	const char * name;
	int (*func)(ll_cluster_t *hb, int, char **, const char *);
	const char * optstr;
} cmd_t;

static const size_t CMDS_MAX_LENGTH = 16;
static gboolean FOR_HUMAN_READ = FALSE;
static const cmd_t cmds[] = {
	{ "hbstatus",      hbstatus, 	  "m" },
	{ "listnodes",     listnodes, 	  "mpn" },
	{ "nodestatus",    nodestatus, 	  "m" },
	{ "nodetype",      nodetype, 	  "m" },
	{ "listhblinks",   listhblinks,   "m" },
	{ "hblinkstatus",  hblinkstatus,  "m" },
	{ "clientstatus",  clientstatus,  "m" },
	{ "rscstatus",     rscstatus, 	  "m"}, 
	{ "hbparameter",   hbparameter,	  "mp:"},
	{ "test",	   test,	  NULL},
	{ NULL, NULL, NULL },
};

static const char * simple_help_screen =
"Usage: cl_status <sub-command> [<options>] [<parameters>]\n"
"\n"
"The sub-commands usage is showed as below.\n"
"Note: There is a general option -m for all subcommands, to make the output\n"
"      more human readable. The default output should be easier for scripts\n"
"      to parse.\n"
"\n"
"hbstatus\n"
"	Indicate if heartbeat is running on the local system.\n"
"\n"
"listnodes\n"
"	List the nodes in the cluster.\n"
"	Options:\n"
"	-p	list only 'ping' type nodes\n"
"	-n	list only 'normal' type nodes\n"
"\n"
"nodestatus <node-name>\n"
"	List the node status.\n"
"\n"
"nodetype <node-name>\n"
"	List the nodes of a given type.\n"
"       Curretly the available types are ping and normal.\n"
"\n"
"listhblinks <node-name>\n"
"	List the network interfaces used as hearbeat links.\n"
"\n"
"hblinkstatus <node-name> <link-name>\n"
"	Show the status of a heartbeat link\n"
"\n"
"clientstatus <node-name> <client-id> [<timeout>]\n"
"	Show the status of heartbeat clients.\n"
"       For example, ping or ipfail.\n"
"\n"
"rscstatus\n"
"	Show the status of cluster resources.\n"
"       Status will be one of: local, foreign, all or none.\n"
"\n"
"hbparamter -p <parameter-name>\n"
"       Retrieve the value of cluster parameters.\n"
"       The parameters may be one of the following, however some parameters\n"
"       may not retrieved: hbversion, node, hopfudge, keepalive, deadtime,\n"
"	deadping, warntime, initdead, watchdog, baud, udpport, logfacility,\n"
"	logfile, debugfile, nice_failback, auto_failback, stonith,\n"
"	stonith_host, respawn, rtprio, hbgenmethod, realtime, debug,\n"
"	normalpoll, apiauth, msgfmt.\n\n"
"	get the value of heartbeat parameters.\n"
"	the parameters can be the one of the following, but maybe not all\n"
"	can be actually gotten.\n"
"	hbversion, node, hopfudge, keepalive, deadtime, deadping,\n"
"	warntime, initdead, watchdog, baud, udpport, logfacility,\n"
"	logfile, debugfile, nice_failback, auto_failback, stonith,\n"
"	stonith_host, respawn, rtprio, hbgenmethod, realtime, debug,\n"
"	normalpoll, apiauth, msgfmt\n"
"\n";

static const char * cl_status_name = "cl_status";
static gboolean HB_SIGNON = FALSE;

int
main(int argc, char ** argv)
{
	ll_cluster_t *hb = NULL;
	int ret_value = 0; 
	int i = -1;
	gboolean GOOD_CMD = FALSE;

	if ((argc == 1) || ( argc == 2 && strncmp(argv[1],"-h",3) == 0)){
		printf("%s",simple_help_screen);
		return 0;
	}

	cl_log_set_entity(cl_status_name);
	/* 
	 * You can open it for debugging conveniently.
	 * When the program is finished formly, all redundant cl_log clauses 
	 * will be removed
	 */ 
	/* cl_log_enable_stderr(TRUE); */
	cl_log_set_facility(LOG_USER);

	cl_log(LOG_INFO, "start:optind: %d  argv[optind+1]: %s", optind, 
		argv[optind+1]);
	/*
	 * Don't use getopt_long since its portibility is not good.
	 * Why to using long command, because long commands are natural and 
	 * good to be remembered.
	 */
	while ( cmds[++i].name != NULL ) {
		if ( strncmp(argv[1], cmds[i].name, CMDS_MAX_LENGTH) == 0 ) {
			GOOD_CMD = TRUE;
			hb = ll_cluster_new("heartbeat");
			if ( hb == NULL ) {
				return 11;
			}

			//cl_log(LOG_INFO, "Signing in with heartbeat.");
			if (hb->llc_ops->signon(hb, cl_status_name)!= HA_OK) {
				cl_log(LOG_ERR, "Cannot sign on with heartbeat");
				cl_log(LOG_ERR, "REASON: %s", 
					hb->llc_ops->errmsg(hb));
				ret_value = 11;
			}else{
				HB_SIGNON = TRUE;
			}
			ret_value = (cmds[i].func)(hb, argc, argv, 
				cmds[i].optstr);
			break;
		}
	}
	if (GOOD_CMD == FALSE) {
		cl_log(LOG_ERR, "%s is not a correct sub-command.", argv[1]);
		ret_value = 12;
	}

	cl_log(LOG_INFO, "End: optind: %d  argv[optind+1]: %s", optind, 
		argv[optind+1]);

	if (HB_SIGNON == TRUE) {
		if (hb->llc_ops->signoff(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot sign off from heartbeat.");
			cl_log(LOG_ERR, "REASON: %s\n", hb->llc_ops->errmsg(hb));
			ret_value = 13;
		}
	}
	if (hb != NULL) {
		if (hb->llc_ops->delete(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot delete API object.");
			cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			ret_value = 13;
		}
	}

	cl_log(LOG_ERR ,"return value:%d", ret_value);
	return ret_value;
}

static int 
hbstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	// Is it ok to judge if heartbeat is running via signon status?
	if ( HB_SIGNON == TRUE ) {
		printf("Heartbeat is running on this machine.\n");
		return 0;
	} else {
		printf("Heartbeat is stopped on this machine.\n");
		return 1;
	}
}

static int 
listnodes(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	gboolean LIST_ALL = TRUE,
		 ONLY_LIST_PING = FALSE, 
		 ONLY_LIST_NORAMAL = FALSE;
	int option_char;
	const char * node, * type;

	do {
		option_char = getopt(argc, argv, optstr);

		if (option_char == -1) {
			break;
		}

		switch (option_char) {
			case 'm':
				FOR_HUMAN_READ = TRUE;
				break;

			case 'p':
				ONLY_LIST_PING = TRUE;
				LIST_ALL = FALSE;	
				break;

			case 'n':
				ONLY_LIST_NORAMAL = TRUE;
				LIST_ALL = FALSE;	
				break;

			default:
				cl_log(LOG_ERR, "Error:getopt returned" 
					"character code %c.", option_char);
				return 12;
		}
	} while (1);

	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot start node walk.");
			cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			return 13;
	}

	if ( (ONLY_LIST_NORAMAL == TRUE) && (ONLY_LIST_PING == TRUE) ) {
		LIST_ALL = TRUE;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("The nodes are as follow:\n");
	}
	while ((node = hb->llc_ops->nextnode(hb))!= NULL) {
		if ( LIST_ALL == TRUE ) {
			printf("%s\n", node);
		} else {
			type = hb->llc_ops->node_type(hb, node);
			if (( ONLY_LIST_NORAMAL == TRUE )
			      && (strncmp(type, "normal", 8)==0)) {
				printf("%s\n", node);
			} else if (( ONLY_LIST_PING == TRUE ) 
				    && (strncmp(type, "ping", 8)==0)) {
				printf("%s\n", node);
			}
		} 
	}

	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk.");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return 13;
	}
	return 0;
}

/* Map string std_output to return value ? 
 * Active
 */
static int 
nodestatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * status;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return 12;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "No enough parameter.\n");
		return 12;
	}

	cl_log(LOG_INFO, "optind: %d   argv[optindex+1]: %s", optind, 
		argv[optind+1]);
	status = hb->llc_ops->node_status(hb, argv[optind+1]);
	if ( status == NULL ) {
		fprintf(stderr, "Error. May be due to incorrect node name\n");
		return 12;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("The cluster nodes %s is %s\n", argv[optind+1], status);
	} else {
		printf("%s\n", status);
	}

	return 0;
}

/* Map string std_output to return value ? 
 * NORMAL PING UNKNOWN
 */
static int 
nodetype(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * type;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return 12;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "No enough parameter.\n");
		return 12;
	}

	cl_log(LOG_INFO, "optind: %d   argv[optindex+1]: %s", optind, 
		argv[optind+1]);
	type = hb->llc_ops->node_type(hb, argv[optind+1]);
	if ( type == NULL ) {
		fprintf(stderr, "Error. May be due to incorrect node name\n");
		return 12;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("The node %s's type: %s\n", argv[optind+1], type);
	} else {
		printf("%s\n", type);
	}
	
	return 0;
}

static int 
listhblinks(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * intf;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return 12;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "No enough parameter.\n");
		return 12;
	}

	cl_log(LOG_INFO, "optind: %d   argv[optindex+1]: %s", optind, 
		argv[optind+1]);

	if (hb->llc_ops->init_ifwalk(hb, argv[optind+1]) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start heartbeat link interface walk.");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return 13;
	}

	if (FOR_HUMAN_READ == TRUE) {
		printf("\tthis node have the following heartbeat links:\n");
	}

	while ((intf = hb->llc_ops->nextif(hb))) {
		printf("\t%s\n", intf); 
	}

	if (hb->llc_ops->end_ifwalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end heartbeat link interface walk");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return 13;
	}

	return 0;
}

static int 
hblinkstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * if_status;	

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return 12;
	};

	if (argc <= optind+2) {
		fprintf(stderr, "No enough parameter.\n");
		return 12;
	}

	if_status = hb->llc_ops->if_status(hb, argv[optind+1], argv[optind+2]);
	if (if_status == NULL) { /* Shoud be error ? */
		cl_log(LOG_ERR, "Cannot get heartbeat link status");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return 13;
	}

	if (FOR_HUMAN_READ == TRUE) {
		printf("The node %s's heartbeat link %s is %s\n", 
			argv[optind+1], argv[optind+2], if_status);
	} else {
		printf("%s\n", if_status);
	}

	return 0;
}

static int 
clientstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	/* Default value, its unit is milliseconds */
	int timeout = 100;  
	const char * cstatus;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return 12;
	};

	if (argc <= optind+2) {
		fprintf(stderr, "No enough parameter.\n");
		return 12;
	}

	if ( argc > optind+3 ) {
		timeout = atoi(argv[optind+3]);
	}

	cstatus = hb->llc_ops->client_status(hb, argv[optind+1], 
		argv[optind+2], timeout);
	if (cstatus == NULL) { /* Error */
		cl_log(LOG_ERR, "Cannot get client %s's status", argv[optind+2]);
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return 13;
	} else {
		/* offline or online */
		printf("%s\n", cstatus);
	}
	
	return 0;
}

static int 
rscstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * rstatus;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return 12;
	};

	rstatus = hb->llc_ops->get_resources(hb);
	if ( rstatus == NULL ) {
		cl_log(LOG_ERR, "Cannot get client %s's status", argv[optind+2]);
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return 13;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("This node is holding %s resources.\n", rstatus);
	} else {
		printf("%s", rstatus);
	}
	return 1;
}

static int
hbparameter(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	int option_char;
	int ret_value = 0;
	gchar * paramname = NULL;
	do {
		option_char = getopt(argc, argv, optstr);

		if (option_char == -1) {
			break;
		}

		switch (option_char) {
			case 'm':
				FOR_HUMAN_READ = TRUE;
				break;

			case 'p':
				if (optarg) {
					paramname = g_strdup(optarg);
					cl_log(LOG_ERR,"parameter: %s", 
						paramname);
				}
				break;

			default:
				cl_log(LOG_ERR, "Error:getopt returned" 
					"character code %c.", option_char);
				if (paramname != NULL) {
					g_free(paramname);
				}
				return 12;
		}
	} while (1);

	cl_log(LOG_INFO,"paramname: %s", paramname);
	if ( paramname != NULL ) {
		char * pvalue;
		pvalue = hb->llc_ops->get_parameter(hb, paramname);
		if (pvalue == NULL) {
			cl_log(LOG_ERR, "Cannot get parameter %s's value", 
				argv[optind+2]);
			cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			ret_value = 13;
		} else {
			if (FOR_HUMAN_READ == TRUE) {
				printf("Heartbeat parameter %s's value: %s\n",
					 paramname, pvalue);
			} else {
				printf("%s\n",pvalue);
			}
			free(pvalue);
		}
		g_free(paramname);
	} else {
		cl_log(LOG_ERR, "Need parameter's name");
		ret_value = 12;
	}

	return ret_value;
}

static int 
general_simple_opt_deal(int argc, char ** argv, const char * optstr)
{
	int option_char;
	do {
		option_char = getopt(argc, argv, optstr);

		if (option_char == -1) {
			break;
		}

		switch (option_char) {
			case 'm':
				FOR_HUMAN_READ = TRUE;
				break;

			default:
				cl_log(LOG_ERR, "Error:getopt returned"
					"character code %c.", option_char);
				return 12;
		}
	} while (1);
	return 0;
}

static int test(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	printf("Dead time: %d\n keeplive time: %d\n mynodeid: %s\n rsc: %s\n",
		(int)hb->llc_ops->get_deadtime(hb), 
		(int)hb->llc_ops->get_keepalive(hb),
		hb->llc_ops->get_mynodeid(hb), hb->llc_ops->get_resources(hb));
	return 0;
}
