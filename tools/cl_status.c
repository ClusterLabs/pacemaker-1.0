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

#include <hb_config.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <glib.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_malloc.h>
#include <hb_api.h>


/* exit code */
const int OK = 0,
	  NORMAL_FAIL = 1,		/* such as the local node is down */
	  PARAMETER_ERROR = 11,
	  TIMEOUT = 12,
	  UNKNOWN_ERROR = 13;		/* error due to unkown causes */ 
/*
 * The exit values under some situations proposed by Alan.
 * nodestatus    fail when the node is down
 * clientstatus  fail when client not accessible (offline?)
 * hbstatus      fail when heartbeat not running locally
 * hblinkstatus  fail if the given heartbeat link is down
 * hbparameter   fail if the given parameter is not set to any value
*/

/*
 * Important
 * General return value for the following functions, and it is actually 
 * as this program cl_status' return value:
 * 	0(OK):   on success, including the node status is ok.
 * 	<>0: on fail
		1(NORMAL_FAIL):     for a "normal" failure (like node is down)
 * 		2(PARAMETER_ERROR):  
 *		3(OTHER_ERROR):	    error due to unkown causes
 */

/* 
 * Description: 
 * 	Detect if heartbeat is running.
 *
 * Parameters:
 *	Obvious. ;-) 
 *
 * Return Value:
 *	OK: 		In local machine, heartbeat is running.
 *	NORMAL_FAIL:    In local machine, heartbeat is stopped.
 */
static int
hbstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	OK:  	      In local machine, this operation succeed.
 *	NORMAL_FAIL:  In local machine, heartbeat is stopped.
 */
static int
listnodes(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	OK: 	     the node is active.
 *	NORMAL_FAIL: the node is down
 */
static int
nodestatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 *	the weight of the node
 */
static int
nodeweight(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 *	the site of the node
 */
static int
nodesite(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 *	0: normal
 *	1: ping
 *	3: unknown type
 * Notes: not map string std_output to return value yet 
 */
static int
nodetype(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 *	0(OK):		sucess
 * 	1(NORMAL_FAIL): the node is down.
 */
static int
listhblinks(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 *	0(OK):		the link is up
 *	1(NORMAL_FAIL): the link is down
 */
static int
hblinkstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	0(OK): 		online
 *	1(NORMAL_FAIL): offline
 *	2:		join
 *	3: 		leave
 *
 *   When sucess and without -m option, at the meantime on stdout print one 
 *   of the following string to reflect the status of the client: 
 *	online, offline, join, leave
 */
static int
clientstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 * 	0(OK): 		on success.
 *	1(NORMAL_FAIL): the node is down.
 *
 *   When sucess and without -m option, on stdout print one of the following
 *   string to reflect the status of the resource: 
 * 	none, local, foreign, all
 */
static int
rscstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/*
 * Return Value:
 *	0: success
 *	1: fail if the given parameter is not set to any value
 */
static int
hbparameter(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);

/* miscellaneous functions */
static int test(ll_cluster_t *hb, int argc, char ** argv, const char * optstr);
static int general_simple_opt_deal(int argc, char ** argv, const char * optstr);

typedef struct {
	const char *	name;
	int 		(*func)(ll_cluster_t *hb, int, char **, const char *);
	const char *	optstr;
	gboolean	needsignon;
} cmd_t;

static const size_t CMDS_MAX_LENGTH = 16;
static gboolean FOR_HUMAN_READ = FALSE;
static const cmd_t cmds[] = {
	{ "hbstatus",      hbstatus, 	  "m" ,		FALSE},
	{ "listnodes",     listnodes, 	  "mpn",	TRUE},
	{ "nodestatus",    nodestatus, 	  "m",		TRUE},
	{ "nodeweight",    nodeweight, 	  "m",		TRUE},
	{ "nodesite",	   nodesite, 	  "m",		TRUE},
	{ "nodetype",      nodetype, 	  "m",		TRUE },
	{ "listhblinks",   listhblinks,   "m",		TRUE },
	{ "hblinkstatus",  hblinkstatus,  "m",		TRUE },
	{ "clientstatus",  clientstatus,  "m",		TRUE },
	{ "rscstatus",     rscstatus, 	  "m",		TRUE}, 
	{ "hbparameter",   hbparameter,	  "mp:,		TRUE"},
	{ "test",	   test,	  NULL,		TRUE},
	{ NULL, NULL, NULL },
};

static const char * simple_help_screen =
"Usage: cl_status <sub-command> [<options>] [<parameters>]\n"
"\n"
"Sub-commands:\n"
"clientstatus <node-name> <client-id> [<timeout>]\n"
"	Show the status of heartbeat clients.\n"
"hblinkstatus <node-name> <link-name>\n"
"	Show the status of a heartbeat link\n"
"hbstatus\n"
"	Indicate if heartbeat is running on the local system.\n"
"listhblinks <node-name>\n"
"	List the network interfaces used as hearbeat links.\n"
"listnodes [<option>]\n"
"	List the nodes in the cluster.\n"
"	Options:\n"
"		-p	list only 'ping' type nodes\n"
"		-n	list only 'normal' type nodes\n"
"nodestatus <node-name>\n"
"	List the node status.\n"
"nodeweight <node-name>\n"
"	List the node weight.\n"
"nodesite <node-name>\n"
"	List the node site.\n"
"nodetype <node-name>\n"
"	List the nodes of a given type.\n"
"rscstatus\n"
"	Show the status of cluster resources.\n"
"hbparameter -p <parameter-name>\n"
"	Retrieve the value of a cluster parameter.\n"
"	As for the valid parameter names, please refer to the man page.\n";

static gboolean HB_SIGNON = FALSE;

static const char * cl_status_name = "cl_status";
/*
 * The following is to avoid cl_status sleeping forever. This is due to the 
 * hearbeat's abnormal status or even its crash.  
 */
unsigned int DEFAULT_TIMEOUT = 5;
/* the handler of signal SIGALRM */
static void quit(int signum);

int
main(int argc, char ** argv)
{
	ll_cluster_t *hb = NULL;
	int ret_value = 0; 
	int i = -1;
	gboolean GOOD_CMD = FALSE;

	if ((argc == 1) || ( argc == 2 && STRNCMP_CONST(argv[1],"-h") == 0)){
		printf("%s", simple_help_screen);
		return 0;
	}

	cl_log_set_entity(cl_status_name);
	/* 
	 * You can open it for debugging conveniently.
	 * When the program is finished formly, all redundant cl_log clauses 
	 * will be removed
	 */ 
	cl_log_enable_stderr(TRUE);
	cl_log_set_facility(LOG_USER);

	/*
	 * To avoid cl_status sleep forever, trigger a timer and dealing with 
	 * signal SIGALRM. This sleep is due to hearbeat's abnormal status or
	 * its crash during cl_status' execution.  
	 */
	 alarm(DEFAULT_TIMEOUT);
	 signal(SIGALRM, quit);
	
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
				return UNKNOWN_ERROR;
			}

			if (hb->llc_ops->signon(hb, NULL)!= HA_OK) {
				ret_value = 1;
				HB_SIGNON = FALSE;
				if (cmds[i].needsignon) {
					cl_log(LOG_ERR
					,	"Cannot signon with heartbeat");
					cl_log(LOG_ERR
					,	"REASON: %s"
					,	hb->llc_ops->errmsg(hb));
					break;
				}
			}else{
				HB_SIGNON = TRUE;
			}
			ret_value = (cmds[i].func)(hb, argc, argv, 
				cmds[i].optstr);
			break;
		}
	}
	if (GOOD_CMD == FALSE) {
		cl_log(LOG_ERR, "%s: invalid sub-command.", argv[1]);
		ret_value = PARAMETER_ERROR;
	}

	if (HB_SIGNON == TRUE) {
		if (hb->llc_ops->signoff(hb, TRUE) != HA_OK) {
			cl_log(LOG_ERR, "Cannot sign off from heartbeat.");
			cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			/* Comment it to avoid to mask the subcommand's return
			 * ret_value = UNKNOWN_ERROR;
			 */
		}
	}
	if (hb != NULL) {
		if (hb->llc_ops->delete(hb) != HA_OK) {
			cl_log(LOG_ERR, "Cannot delete API object.");
			cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
			/* Comment it to avoid to mask the subcommand's return
			 * ret_value = UNKNOWN_ERROR;
			 */
		}
	}

	return ret_value;
}

static int 
hbstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	/* Is it ok to judge if heartbeat is running via signon status? */
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
				cl_log(LOG_ERR, "Error: getopt returned" 
					"character code %c.", option_char);
				return PARAMETER_ERROR;
		}
	} while (1);

	if (hb->llc_ops->init_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start node walk.");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return UNKNOWN_ERROR;
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
			      && (STRNCMP_CONST(type, "normal")==0)) {
				printf("%s\n", node);
			} else if (( ONLY_LIST_PING == TRUE ) 
				    && (STRNCMP_CONST(type, "ping")==0)) {
				printf("%s\n", node);
			}
		} 
	}

	if (hb->llc_ops->end_nodewalk(hb) != HA_OK) {
		cl_log(LOG_ERR, "Cannot end node walk.");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return UNKNOWN_ERROR;
	}
	return 0;
}

/* Map string std_output to return value ? 
 * Active
 */
static int 
nodestatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char *	status;
	int		ret = UNKNOWN_ERROR;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return PARAMETER_ERROR;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "Not enough parameters.\n");
		return PARAMETER_ERROR;
	}

	status = hb->llc_ops->node_status(hb, argv[optind+1]);
	if ( status == NULL ) {
		fprintf(stderr, "Error. May be due to incorrect node name\n");
		return PARAMETER_ERROR;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("The cluster node %s is %s\n", argv[optind+1], status);
	} else {
		printf("%s\n", status);
	}

       if ( STRNCMP_CONST(status, "active") == 0 
       ||      STRNCMP_CONST(status, "up") == 0
       ||      STRNCMP_CONST(status, "ping") == 0) {
		ret = OK;   /* the node is active */
	} else {
		ret = NORMAL_FAIL;  /* the status = "dead" */
	}
	/* Should there be other status? According the comment in heartbeat.c
	 * three status: active, up, down. But it's not like that. 
	 */
	
	return ret;
}
static int 
nodeweight(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	int	weight;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return PARAMETER_ERROR;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "Not enough parameters.\n");
		return PARAMETER_ERROR;
	}

	weight = hb->llc_ops->node_weight(hb, argv[optind+1]);
	if ( weight == -1 ) {
		fprintf(stderr, "Error. Maybe due to incorrect node name.\n");
		return PARAMETER_ERROR;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("The weight of the cluster node %s is %d\n", argv[optind+1], weight);
	} else {
		printf("%d\n", weight);
	}

	return OK;
}
/* Map string std_output to return value ? 
 * Active
 */
static int 
nodesite(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char *	site;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return PARAMETER_ERROR;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "Not enough parameters.\n");
		return PARAMETER_ERROR;
	}

	site = hb->llc_ops->node_site(hb, argv[optind+1]);
	if ( site == NULL ) {
		fprintf(stderr, "Error. May be due to incorrect node name\n");
		return PARAMETER_ERROR;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("The site of the cluster node %s is %s\n", argv[optind+1], site);
	} else {
		printf("%s\n", site);
	}
	return  OK;
}

/* Map string std_output to return value ? No
 * NORMAL PING UNKNOWN
 */
static int 
nodetype(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * type;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return PARAMETER_ERROR;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "No enough parameter.\n");
		return PARAMETER_ERROR;
	}

	type = hb->llc_ops->node_type(hb, argv[optind+1]);
	if ( type == NULL ) {
		fprintf(stderr, "Error. May be due to incorrect node name\n");
		return PARAMETER_ERROR;
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
		return PARAMETER_ERROR;
	};

	if (argc <= optind+1) {
		fprintf(stderr, "No enough parameter.\n");
		return PARAMETER_ERROR;
	}


	if (hb->llc_ops->init_ifwalk(hb, argv[optind+1]) != HA_OK) {
		cl_log(LOG_ERR, "Cannot start heartbeat link interface walk.");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return UNKNOWN_ERROR;
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
		return UNKNOWN_ERROR;
	}

	return 0;
}

static int 
hblinkstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * if_status;	
	int 	ret = UNKNOWN_ERROR;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return PARAMETER_ERROR;
	};

	if (argc <= optind+2) {
		fprintf(stderr, "No enough parameter.\n");
		return PARAMETER_ERROR;
	}

	if_status = hb->llc_ops->if_status(hb, argv[optind+1], argv[optind+2]);
	if (if_status == NULL) { /* Should be error ? */
		cl_log(LOG_ERR, "Cannot get heartbeat link status");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return UNKNOWN_ERROR;
	}

	if (FOR_HUMAN_READ == TRUE) {
		printf("The node %s's heartbeat link %s is %s\n", 
			argv[optind+1], argv[optind+2], if_status);
	} else {
		printf("%s\n", if_status);
	}

	if ( STRNCMP_CONST(if_status, "up") == 0 ) {
		ret = OK; /* the link is up */
	} else {
		ret = NORMAL_FAIL; /* the link is dead */
	}

	return ret;
}

static int 
clientstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	/* Default value, its unit is milliseconds */
	int timeout = 500;
	const char * cstatus;
	int ret = UNKNOWN_ERROR;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return PARAMETER_ERROR;
	};

	if (argc <= optind+2) {
		fprintf(stderr, "No enough parameter.\n");
		return PARAMETER_ERROR;
	}

	if ( argc > optind+3 ) {
		timeout = atoi(argv[optind+3]);
	}

	cstatus = hb->llc_ops->client_status(hb, argv[optind+1], 
		argv[optind+2], timeout);
	if (cstatus == NULL) { /* Error */
		cl_log(LOG_ERR, "Cannot get heartbeat client %s's status"
		,	argv[optind+2]);
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return UNKNOWN_ERROR;
	} else {
		/* online, offline, join, leave */
		printf("%s\n", cstatus);
	}

	if ( STRNCMP_CONST(cstatus, "online") == 0 ) {
		ret = OK; 
	} else if ( STRNCMP_CONST(cstatus, "offline") == 0 ) {
		ret = NORMAL_FAIL; /* the client is offline */
	} else if ( STRNCMP_CONST(cstatus, "join") == 0 ) {
		ret = 2;
	} else if ( STRNCMP_CONST(cstatus, "leave") == 0 ) {
		ret = 3;
	}
	
	return ret;
}

static int 
rscstatus(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	const char * rstatus;

	if ( general_simple_opt_deal(argc, argv, optstr) < 0 ) {
		/* There are option errors */
		return PARAMETER_ERROR;
	};

	rstatus = hb->llc_ops->get_resources(hb);
	if ( rstatus == NULL ) {
		cl_log(LOG_ERR
		,	"Cannot get cluster resource status");
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return UNKNOWN_ERROR;
	}
	if (FOR_HUMAN_READ == TRUE) {
		printf("This node is holding %s resources.\n", rstatus);
	} else {
		printf("%s\n", rstatus);
	}
	return 0;
}

static int
hbparameter(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	int		option_char;
	int		ret_value = 0;
	const char*	paramname = NULL;
	char *		pvalue;

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
					paramname = optarg;
				}
				break;

			default:
				cl_log(LOG_ERR, "Error: getopt returned" 
					"character code %c.", option_char);
				return PARAMETER_ERROR;
		}
	} while (1);

	if ( NULL == paramname) {
		cl_log(LOG_ERR, "parameter name required");
		return PARAMETER_ERROR;
	}

	pvalue = hb->llc_ops->get_parameter(hb, paramname);

	if (pvalue == NULL) {
		cl_log(LOG_ERR, "Cannot get parameter %s's value"
		,	paramname);
		cl_log(LOG_ERR, "REASON: %s", hb->llc_ops->errmsg(hb));
		return NORMAL_FAIL;
	}

	if (FOR_HUMAN_READ == TRUE) {
		printf("Heartbeat parameter %s's value: %s\n",
			 paramname, pvalue);
	} else {
		printf("%s\n",pvalue);
	}
	cl_free(pvalue);
	pvalue = NULL;

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
				cl_log(LOG_ERR, "Error: getopt returned"
					"character %c.", option_char);
				return -1;
		}
	} while (1);
	return 0;
}

static int
test(ll_cluster_t *hb, int argc, char ** argv, const char * optstr)
{
	printf("Dead time: %d\n keeplive time: %d\n mynodeid: %s\n rsc: %s\n",
		(int)hb->llc_ops->get_deadtime(hb), 
		(int)hb->llc_ops->get_keepalive(hb),
		hb->llc_ops->get_mynodeid(hb), hb->llc_ops->get_resources(hb));
	return 0;
}

/* the handler of signal SIGALRM */
static void
quit(int signum)
{
	if ( signum == SIGALRM ) {
		exit(TIMEOUT);
	}
}
