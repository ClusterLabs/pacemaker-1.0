/* 
 * clmtest.c: AIS membership service client application
 *
 * Copyright (c) 2003 Intel Corp.
 * Author: Zhu Yi (yi.zhu@intel.com)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "portability.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <saf/ais.h>

#define MAX_ITEMS	5 /* for a max 5-nodes cluster */

/* global variables */
SaClmHandleT hd;
SaClmClusterNotificationT nbuf[MAX_ITEMS];
SaClmClusterNodeT cn;

static void
track_start(int sig)
{
	int ret;
	SaUint8T flag = SA_TRACK_CHANGES;

	signal(SIGUSR1, &track_start);
	printf("-------------------------------------------------\n");
	fprintf(stderr, "Start to Track Cluster Membership\n");
	if ((ret = saClmClusterTrackStart(&hd, flag, nbuf
	,	MAX_ITEMS)) != SA_OK) {
		fprintf(stderr, "saClmClusterTrackStart error, errno [%d]\n"
		,	ret);
		exit(1);
	}
}

static void
track_stop(int sig)
{
	int ret;

	signal(SIGUSR2, &track_start);
	fprintf(stderr,	"Stop to Track Cluster Membership\n");
	if ((ret = saClmClusterTrackStop(&hd)) != SA_OK) {
		fprintf(stderr, "saClmClusterTrackStop error, errno [%d]\n"
		,	ret);
		exit(1);
	}
}

static void
track_callback(SaClmClusterNotificationT *nbuf, SaUint32T nitem
,	SaUint32T nmem, SaUint64T nview, SaErrorT error)
{
	uint i;

	if (error != SA_OK) {
		fprintf(stderr, "Track Callback failed [%d]\n", error);
		exit(1);
	}
	printf("-------------------------------------------------\n");
	printf("SA CLM Track Callback BEGIN\n");
	printf("viewNumber = %llu\n", nview);
	printf("numberOfItems = %lu\n", nitem);
	printf("numberOfMembers = %lu\n", nmem);

	for (i = 0; i < nitem; i++) {
		printf("\n");
		printf("\tclusterChanges = %s [%d]\n"
		,	nbuf[i].clusterChanges == 1 ? "SA_CLM_NODE_NO_CHANGE"
		:	nbuf[i].clusterChanges == 2 ? "SA_CLM_NODE_JOINED"
		:	nbuf[i].clusterChanges == 3 ? "SA_CLM_NODE_LEFT":"ERROR"
		,	nbuf[i].clusterChanges);
		printf("\tnodeId = %ld\n", nbuf[i].clusterNode.nodeId);
		printf("\tnodeAddress = %s\n"
		,	nbuf[i].clusterNode.nodeAddress.length > 0
		?	(char *)nbuf[i].clusterNode.nodeAddress.value : "N/A");
		printf("\tnodeName = %s\n"
		,	nbuf[i].clusterNode.nodeName.length > 0
		?	(char *)nbuf[i].clusterNode.nodeName.value : "N/A");
		printf("\tclusterName = %s\n"
		,	nbuf[i].clusterNode.clusterName.length > 0
		?	(char *)nbuf[i].clusterNode.clusterName.value : "N/A");
		printf("\tmember = %d\n", nbuf[i].clusterNode.member);
		printf("\tbootTimestamp = %lld\n"
		,	nbuf[i].clusterNode.bootTimestamp);
	}
	printf("\nSA CLM Track Callback END\n");
}

static void
dump_nodeinfo(SaClmClusterNodeT *cn)
{
	printf("Dump information from SaClmClusterNodeGet\n");
	printf("\n");
	printf("nodeId = %ld\n", cn->nodeId);
	printf("nodeAddress = %s\n"
	,	cn->nodeAddress.length > 0
	?	(char *)cn->nodeAddress.value : "N/A");
	printf("nodeName = %s\n"
	,	cn->nodeName.length > 0
	?	(char *)cn->nodeName.value : "N/A");
	printf("clusterName = %s\n"
	,	cn->clusterName.length > 0
	?	(char *)cn->clusterName.value : "N/A");
	printf("member = %d\n", cn->member);
	printf("bootTimestamp = %lld\n", cn->bootTimestamp);
	printf("\n");
}

static void
getnode_callback(SaInvocationT invocation, SaClmClusterNodeT *clusterNode
,	SaErrorT error)
{
	if (error != SA_OK) {
		fprintf(stderr, "Get Node Callback failed [%d]\n", error);
		exit(1);
	}
	fprintf(stderr, "Invocation [%d]\n", invocation);
	dump_nodeinfo(clusterNode);
}

int main(void)
{
	SaSelectionObjectT st;
	SaErrorT ret;
	SaClmNodeIdT nid;

	SaClmCallbacksT my_callbacks = {
		.saClmClusterTrackCallback
		=	(SaClmClusterTrackCallbackT)track_callback,
		.saClmClusterNodeGetCallback
		=	(SaClmClusterNodeGetCallbackT)getnode_callback
	};

	if ((ret = saClmInitialize(&hd, &my_callbacks, NULL)) != SA_OK) {
		fprintf(stderr, "saClmInitialize error, errno [%d]\n",ret);
		return 1;
	}
	if ((ret = saClmSelectionObjectGet(&hd, &st)) != SA_OK) {
		fprintf(stderr, "saClmSelectionObjectGet error, errno [%d]\n"
		,	ret);
		return 1;
	}

	nid = 0;

	/* Synchronously get nodeId information */
	printf("-------------------------------------------------\n");
	printf("Get nodeId [%lu] info by SaClmClusterNodeGet\n", nid);
	if ((ret = saClmClusterNodeGet(nid, 10, &cn)) != SA_OK) {
		if (ret == SA_ERR_INVALID_PARAM) {
			fprintf(stderr, "NodeId [%lu] record not found!\n",nid);
		} else {
			fprintf(stderr
			,	"saClmClusterNodeGet error, errno [%d]\n"
			,	ret);
			return 1;
		}
	} else {
		dump_nodeinfo(&cn);
	}

	/* Asynchronously get my nodeId information */
	nid = 1;
	printf("-------------------------------------------------\n");
	printf("Get nodeId [%lu] info by SaClmClusterNodeGetAsync\n", nid);
	if ((ret = saClmClusterNodeGetAsync(&hd, 1234, nid, &cn)) != SA_OK) {
		if (ret == SA_ERR_INVALID_PARAM) {
			fprintf(stderr, "NodeId [%lu] record not found!\n",nid);
		} else {
			fprintf(stderr
			,	"saClmClusterNodeGet error, errno [%d]\n"
			,	ret);
			return 1;
		}
	}

	/* Start to track cluster membership changes events */
	track_start(SIGUSR1);
	signal(SIGUSR2, &track_stop);

	for (;;) {
		fd_set rset;

		FD_ZERO(&rset);
		FD_SET(st, &rset);

		if (select(st + 1, &rset, NULL, NULL, NULL) == -1) {
			/* TODO should we use pselect here? */
			if (errno == EINTR)
				continue;
			else {
				perror("select");
				return 1;
			}
		}
		if ((ret = saClmDispatch(&hd, SA_DISPATCH_ALL)) != SA_OK) {
			if (ret == SA_ERR_LIBRARY) {
				fprintf(stderr, "I am evicted!\n");
				return 1;
			}
			fprintf(stderr, "saClmDispatch error, errno [%d]\n"
			,	ret);
			return 1;
		}
	}

	return 0;
}
