/* $Id: hasubagent.c,v 1.11 2004/02/17 22:12:01 lars Exp $ */
#include <portability.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_NET_SNMP
#	define	USE_NET_SNMP
#else
#	define	USE_UCD_SNMP
#endif

#ifdef USE_NET_SNMP
#	include <net-snmp/net-snmp-config.h>
#	include <net-snmp/net-snmp-includes.h>
#	include <net-snmp/agent/net-snmp-agent-includes.h>
#	define	INIT_AGENT()	init_master_agent()
#else
#	include <ucd-snmp/ucd-snmp-config.h>
#	include <ucd-snmp/ucd-snmp-includes.h>
#	include <ucd-snmp/ucd-snmp-agent-includes.h>
#       ifndef NETSNMP_DS_APPLICATION_ID
#		define NETSNMP_DS_APPLICATION_ID	DS_APPLICATION_ID
#	endif
#	ifndef NETSNMP_DS_AGENT_ROLE
#		define NETSNMP_DS_AGENT_ROLE	DS_AGENT_ROLE
#	endif
#	define netsnmp_ds_set_boolean	ds_set_boolean
#	define	INIT_AGENT()	init_master_agent(161, NULL, NULL)
#endif

#ifdef SNMP_NEED_TCPWRAPPER
#	ifdef HAVE_TCPD_H
#		include <tcpd.h>
		int allow_severity       = LOG_INFO;
		int deny_severity        = LOG_WARNING;
#	endif /* HAVE_TCPD_H */
#endif /* SNMP_NEED_TCPWRAPPER */
  
#include <signal.h>
#include <sys/select.h>

#include "haclient.h"
#include "ClusterInfo.h"
#include "NodeTable.h"
#include "HAIFTable.h"

#define LINUXHA_SUBAGENT_ENTITY_NAME "linux-ha"
#define DEFAULT_TIME_OUT 5 // default timeout value for snmp in sec.

static int keep_running;

static RETSIGTYPE stop_server(int a);

static RETSIGTYPE
stop_server(int a)
{
	keep_running = 0;
}

/*
 * As of this writing, this code does not compile correctly on
 * ucdsnmp 4.2.5-51 on SuSE Linux 8.1
 *
 * There are a few undefined symbolx I can't seem to find anywhere...
 *	hosts_ctl, and deny_severity.
 *
 * Close, but no cigar ;-)
 *
 */
int
main(int argc, char **argv)
{
	fd_set fdset;
	struct timeval tv, *tvp;
	int block, numfds, ret, hb_fd;

	/* Change this if you want to be a SNMP master agent */
	int agentx_subagent=1;

	/* Print log errors to stderr */
	snmp_enable_stderrlog();

	/* We're an agentx subagent? */
	if (agentx_subagent) {
		/* Make us an agentx client. */
		netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID
		,	NETSNMP_DS_AGENT_ROLE, 1);
	}

	/* Initialize the agent library */
	init_agent(LINUXHA_SUBAGENT_ENTITY_NAME);

	/* Initialize mib code here */

	/* mib code: init_nstAgentSubagentObject from nstAgentSubagentObject.C */
	// init_nstAgentSubagentObject();  

	/* hasubagent will be used to read hasubagent.conf files. */
	init_snmp(LINUXHA_SUBAGENT_ENTITY_NAME);

	if ((ret = init_heartbeat()) != HA_OK) {
                return -1;
        }

        if ((hb_fd = get_heartbeat_fd()) <=0) {
                return -1;
        }

	init_ClusterInfo();
	init_NodeTable();
	init_HAIFTable();


	/* If we're going to be a snmp master agent, initial the ports */
	if (!agentx_subagent) {
		/* Open the port to listen on (defaults to udp:161) */
		INIT_AGENT();
	}

	/* In case we receive a request to stop (kill -TERM or kill -INT) */
	keep_running = 1;
	signal(SIGTERM, stop_server);
	signal(SIGINT, stop_server);

	/* You're main loop here... */
	while(keep_running) {
		/* If you use select(), see snmp_select_info() in snmp_api(3) */
		/*     --- OR ---  */
		// agent_check_and_process(1); /* 0 == don't block */

		FD_ZERO(&fdset);
                FD_SET(hb_fd, &fdset);

		tv.tv_sec = DEFAULT_TIME_OUT;
		tv.tv_usec = 0;
		numfds = hb_fd+1;

		tvp = &tv;

		snmp_select_info(&numfds, &fdset, tvp, &block);

		if (block) {
			tvp = NULL;
                }

		ret = select(numfds, &fdset, 0, 0, tvp);

		// the error cases
                if (ret < 0) {
			cl_log(LOG_ERR, "select() returned error. Quitting...\n");
			break;
                }
		else if (ret == 0) {
			snmp_timeout();
                        continue;
		}

		// the normal cases
		if (FD_ISSET(hb_fd, &fdset)) {

			// handle heartbeat msgs
			if ((ret = handle_heartbeat_msg()) == -1) {
				cl_log(LOG_ERR, "heartbeat stopped. Quitting...\n");
				break;
			}
		} else {
			// handle snmp msgs
			snmp_read(&fdset);
		}
	}

	/* At shutdown time */
	snmp_shutdown(LINUXHA_SUBAGENT_ENTITY_NAME);
	return 0;
}


