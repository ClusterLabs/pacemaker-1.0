/*
 * cms_cmstools.c: cmstools utility
 *
 * Copyright (c) 2004 Intel Corp.
 *
 * Author: Zou Yixiong (yixiong.zou@intel.com)
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <clplumbing/ipc.h>
#include <clplumbing/GSource.h>
#include <saf/ais.h>
#include "cmstools.h"

#define CMDS_MAX_LENGTH		32
#define dprintf(arg...)		do { if (option_verbose) fprintf(stderr, \
					##arg); } while (0)

typedef struct {
	const char * name;
	SaErrorT (*func)(SaMsgHandleT *, int, char **, const char *);
	const char * optstr;
} command_t;

static SaErrorT queue_open(SaMsgHandleT *, int, char **, const char *);
static SaErrorT queue_status(SaMsgHandleT *, int, char **, const char *);
static SaErrorT queue_create(SaMsgHandleT *, int, char **, const char *);
static SaErrorT queue_close(SaMsgHandleT *, int, char **, const char *);
static SaErrorT queue_unlink(SaMsgHandleT *, int, char **, const char *);
static SaErrorT group_create(SaMsgHandleT *, int, char **, const char *);
static SaErrorT group_delete(SaMsgHandleT *, int, char **, const char *);
static SaErrorT group_insert(SaMsgHandleT *, int, char **, const char *);
static SaErrorT group_remove(SaMsgHandleT *, int, char **, const char *);
static SaErrorT group_track(SaMsgHandleT *, int, char **, const char *);
static SaErrorT group_trackstop(SaMsgHandleT *, int, char **, const char *);
static SaErrorT message_send(SaMsgHandleT *, int, char **,const char *);
static SaErrorT message_get(SaMsgHandleT *, int, char **,const char *);

static const command_t commands[] = {
	{ "openqueue",		queue_open,	"vc:n:" },
	{ "createqueue",	queue_create,	"vc:n:pr:s:t:" },
	{ "status",		queue_status,	"vc:n:" },
	{ "closequeue",		queue_close,	"vc:n:" },
	{ "unlink",		queue_unlink,	"vc:n:" },

	{ "creategroup",	group_create,	"vc:n:" },
	{ "deletegroup",	group_delete,	"vc:n:" },
	{ "insert",		group_insert,	"vc:q:g:" },
	{ "remove",		group_remove,	"vc:q:g:" },
	{ "track",		group_track,	"vc:n:" },
	{ "trackstop",		group_trackstop,"vc:n:" },

	{ "send",		message_send,	"vc:n:m:p" },
	{ "get",		message_get,	"vc:n:a:" },

	{ NULL,			NULL,		NULL },
};

static const char * option_help =
"Usage: cmstools -c <command> [<options> [<parameters>]] ...\n";

int option_verbose;
const char * next_command;

SaMsgQueueHandleT mqueue_handle;

static void
usage(void)
{
	fprintf(stderr, "%s", option_help);
	exit(0);
}

static SaErrorT
queue_open(SaMsgHandleT * handle, int argc, char **argv, const char * optstr)
{
	const char * name = DEFAULT_MQUEUE_NAME;
	SaNameT saname;
	char c;
	SaMsgQueueCreationFlagsT flag;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	flag = SA_MSG_QUEUE_SELECTION_OBJECT_SET;

	if ((ret = saMsgQueueOpen(handle, &saname, NULL, flag, &mqueue_handle,
			SA_TIME_END)) != SA_OK)
		dprintf("saMsgQueueOpen failed, error = [%d]\n", ret);
	else
		dprintf("saMsgQueueOpen Succeed!\n");

	return ret;
}


static SaErrorT
queue_create(SaMsgHandleT * handle, int argc, char **argv, const char * optstr)
{
	const char * name = DEFAULT_MQUEUE_NAME;
	SaNameT saname;
	char c;
	SaMsgQueueCreationFlagsT flag;
	SaMsgQueueCreationAttributesT attr;
	SaErrorT ret;
	SaTimeT timeout;
	int i;

	flag = SA_MSG_QUEUE_SELECTION_OBJECT_SET;
	attr.creationFlags = DEFAULT_CREATION_FLAGS;
	timeout = DEFAULT_TIMEOUT;

	for (i = 0; i <= SA_MSG_MESSAGE_LOWEST_PRIORITY; i++)
		attr.size[i] = DEFAULT_MQUEUE_SIZE;

	attr.retentionTime = DEFAULT_RETENTION_TIME;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			case 'p':
				attr.creationFlags = SA_MSG_QUEUE_PERSISTENT;
				break;

			case 'r':
				attr.retentionTime = atoi(optarg) * 1E9;
				attr.creationFlags = 0;
				break;

			case 's':
				for (i = 0; i <= SA_MSG_MESSAGE_LOWEST_PRIORITY
				;	i++)
					attr.size[i] = atoi(optarg);
				break;

			case 't':
				timeout = atoi(optarg) * 1E9;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	if ((ret = saMsgQueueOpen(handle, &saname, &attr, flag,
			&mqueue_handle, timeout)) != SA_OK)
		dprintf("saMsgQueueOpen failed, error = [%d]\n", ret);
	else
		dprintf("saMsgQueueOpen Succeed!\n");

	return ret;

}

static SaErrorT
queue_status(SaMsgHandleT * handle, int argc, char ** argv, const char *optstr)
{
	const char * name = DEFAULT_MQUEUE_NAME;
	SaNameT saname;
	char c;
	SaErrorT ret;
	int i;
	SaMsgQueueStatusT qst;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	/* dump mqueue status */
	if ((ret = saMsgQueueStatusGet(handle, &saname, &qst)) != SA_OK) {
		dprintf("saMsgQueueStatus failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgQueueStatus is [%d]\n", qst.sendingState);

	dprintf("sendingState %d\n", qst.sendingState);
	dprintf("creationFlags %lu\n", qst.creationFlags);
	dprintf("openFlags %lu\n", qst.openFlags);
	dprintf("retentionTime %lld\n", qst.retentionTime);
	dprintf("closeTime %lld\n", qst.closeTime);
	dprintf("headerLength %d\n", qst.headerLength);

	for (i = SA_MSG_MESSAGE_HIGHEST_PRIORITY
	;	i <= SA_MSG_MESSAGE_LOWEST_PRIORITY
	;	i++) {

		dprintf("saMsgQueueUsage[%d].queueSize %lu\n"
		,	i, qst.saMsgQueueUsage[i].queueSize);
		dprintf("saMsgQueueUsage[%d].queueUsed %d\n"
		,	i, qst.saMsgQueueUsage[i].queueUsed);
		dprintf("saMsgQueueUsage[%d].numberOfMessages %lu\n"
		,	i, qst.saMsgQueueUsage[i].numberOfMessages);
	}

	return ret;
}

static SaErrorT
queue_close(SaMsgHandleT * handle, int argc, char **argv, const char * optstr)
{
	const char * name = DEFAULT_MQUEUE_NAME;
	SaNameT saname;
	char c;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;


			case 'n':
				name = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	if ((ret = saMsgQueueClose(&mqueue_handle)) != SA_OK)
		dprintf("saMsgQueueClose failed, error = [%d]\n", ret);
        else
        	dprintf("saMsgQueueClose Succeed!\n");

	return ret;
}

static SaErrorT
queue_unlink(SaMsgHandleT * handle, int argc, char **argv, const char * optstr)
{
	const char * name = DEFAULT_MQUEUE_NAME;
	SaNameT saname;
	char c;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;


			case 'n':
				name = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	if ((ret = saMsgQueueUnlink(handle, &saname)) != SA_OK)
		dprintf("saMsgQueueUnlink failed, error = [%d]\n", ret);
        else
        	dprintf("saMsgQueueUnlink Succeed!\n");

	return ret;
}

static SaErrorT
group_create(SaMsgHandleT * handle, int argc, char **argv,
		  const char * optstr)
{
	const char * name = DEFAULT_MQGROUP_NAME;
	SaNameT saname;
	char c;
	SaMsgQueueCreationFlagsT flag;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	flag = SA_MSG_QUEUE_SELECTION_OBJECT_SET;

	if ((ret = saMsgQueueGroupCreate(handle, &saname
	,	SA_MSG_QUEUE_GROUP_ROUND_ROBIN)) != SA_OK) {
		
		dprintf("saMsgQueueGroupCreate failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgQueueGroupCreate Succeed!\n");

	return ret;
}

static SaErrorT
group_delete(SaMsgHandleT * handle, int argc, char **argv,
		  const char * optstr)
{
	const char * name = DEFAULT_MQGROUP_NAME;
	SaNameT saname;
	char c;
	SaMsgQueueCreationFlagsT flag;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	flag = SA_MSG_QUEUE_SELECTION_OBJECT_SET;

	if ((ret = saMsgQueueGroupDelete(handle, &saname)) != SA_OK) {
		
		dprintf("saMsgQueueGroupDelete failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgQueueGroupDelete Succeed!\n");

	return ret;
}

static SaErrorT
group_insert(SaMsgHandleT * handle, int argc, char **argv,
		  const char * optstr)
{
	const char * qname = DEFAULT_MQUEUE_NAME;
	const char * gname = DEFAULT_MQGROUP_NAME;
	SaNameT saqname, sagname;
	char c;
	SaMsgQueueCreationFlagsT flag;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'q':
				qname = optarg;
				break;

			case 'g':
				gname = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saqname.value, qname);
	saqname.length = strlen(saqname.value);
	strcpy(sagname.value, gname);
	sagname.length = strlen(sagname.value);

	flag = SA_MSG_QUEUE_SELECTION_OBJECT_SET;

	if ((ret = saMsgQueueGroupInsert(handle, &sagname, &saqname))
		!= SA_OK) {
		
		dprintf("saMsgQueueGroupInsert failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgQueueGroupInsert Succeed!\n");

	return ret;
}

static SaErrorT
group_remove(SaMsgHandleT * handle, int argc, char **argv,
		  const char * optstr)
{
	const char * qname = DEFAULT_MQUEUE_NAME;
	const char * gname = DEFAULT_MQGROUP_NAME;
	SaNameT saqname, sagname;
	char c;
	SaMsgQueueCreationFlagsT flag;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'q':
				qname = optarg;
				break;

			case 'g':
				gname = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saqname.value, qname);
	saqname.length = strlen(saqname.value);
	strcpy(sagname.value, gname);
	sagname.length = strlen(sagname.value);

	flag = SA_MSG_QUEUE_SELECTION_OBJECT_SET;

	if ((ret = saMsgQueueGroupRemove(handle, &sagname, &saqname))
		!= SA_OK) {
		
		dprintf("saMsgQueueGroupRemove failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgQueueGroupRemove Succeed!\n");

	return ret;
}

static SaErrorT
group_track(SaMsgHandleT * handle, int argc, char **argv,
		  const char * optstr)
{
	const char * name = DEFAULT_MQGROUP_NAME;
	SaNameT saname;
	char c;
	SaMsgQueueCreationFlagsT flag;
	SaErrorT ret;

	flag = DEFAULT_MQGROUP_TRACK_FLAG;
	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			case 'f':
				flag = atoi(optarg);
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	if ((ret = saMsgQueueGroupTrackStart(handle, &saname, flag, NULL, 0))
		!= SA_OK) {
		
		dprintf("saMsgQueueGroupTrack failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgQueueGroupTrack Succeed!\n");

	return ret;
}

static SaErrorT
group_trackstop(SaMsgHandleT * handle, int argc, char **argv,
		     const char * optstr)
{
	const char * name = DEFAULT_MQGROUP_NAME;
	SaNameT saname;
	char c;
	SaErrorT ret;

	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	if ((ret = saMsgQueueGroupTrackStop(handle, &saname)) != SA_OK) {
		
		dprintf("saMsgQueueGroupTrackStop failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgQueueGroupTrackStop Succeed!\n");

	return ret;
}

static SaErrorT
message_send(SaMsgHandleT * handle, int argc, char **argv, const char * optstr)
{
	const char * name = DEFAULT_MQUEUE_NAME;
	const char * msgstr = DEFAULT_MESSAGE_STRING;
	SaNameT saname;
	char c;
	SaMsgQueueCreationFlagsT flag = SA_MSG_MESSAGE_DELIVERED_ACK;
	SaMsgMessageT message;
	SaTimeT timeout = SA_TIME_END;
	SaErrorT ret;
	int size, priority;

	size = DEFAULT_MESSAGE_SIZE;
	priority = DEFAULT_MESSAGE_PRIORITY;
	next_command = NULL;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			case 'm':
				msgstr = optarg;
				break;

			case 'p':
				priority = atoi(optarg);
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	if (priority > SA_MSG_MESSAGE_LOWEST_PRIORITY || priority < 0)
		priority = DEFAULT_MESSAGE_PRIORITY;

	message.priority = priority;
	message.size = strlen(msgstr) + 1;
	message.data = strdup(msgstr);

	dprintf("saMsgMessageSend: [%s]\n", (char *)message.data);

	if ((ret = saMsgMessageSend(handle, &saname, &message, flag, timeout))
		!= SA_OK) {
		
		dprintf("saMsgMessageSend failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgMessageSend Succeed!\n");

	free(message.data);
	return ret;
}

static SaErrorT
message_get(SaMsgQueueHandleT * handle, int argc, char **argv,
	    const char * optstr)
{
	const char * name = DEFAULT_MQUEUE_NAME;
	SaNameT saname;
	char c;
	SaErrorT ret = SA_OK;
	SaMsgMessageT message;
	SaTimeT timeout = SA_TIME_END;
	int count = 1;
	char buff[DEFAULT_MESSAGE_SIZE];

	next_command = NULL;
	bzero(&message, sizeof(SaMsgMessageT));
	bzero(buff, DEFAULT_MESSAGE_SIZE);
	message.data = buff;
	message.size = DEFAULT_MESSAGE_SIZE;

	while (!next_command) {
		c = getopt(argc, argv, optstr);

		if (c == -1) {
			next_command = NULL;
			break;
		}

		switch (c) {
			case 'v':
				option_verbose = 1;
				break;

			case 'c':
				next_command = optarg;
				break;

			case 'n':
				name = optarg;
				break;

			case 'a':
				count = atoi(optarg);
				break;

			default:
				dprintf("Error: unknown option %c\n", c);
				return -1;
		}
	}

	strcpy(saname.value, name);
	saname.length = strlen(saname.value);

	while (count > 0) {
		if ((ret = saMsgMessageGet(&mqueue_handle, &message, NULL,				timeout)) != SA_OK) {
		
			dprintf("saMsgMessageGet failed, error = [%d]\n", ret);
			return ret;
		}
		count--;
		dprintf("saMsgMessageGet: [%s]\n", (char *)message.data);
	}
	dprintf("saMsgMessageGet Succeed!\n");

	return ret;
}

static void
deliver_cb(SaInvocationT invocation,  SaErrorT error)
{
	fprintf(stderr, "In Function %s ...\n", __FUNCTION__);
	return;
}

int
main(int argc, char * argv[])
{
	SaErrorT ret;
	SaMsgHandleT handle;
	char c;

	SaVersionT version = {'A', 0x01, 0x0};
	SaMsgCallbacksT callbacks = {
		.saMsgQueueOpenCallback = NULL,
		.saMsgMessageReceivedCallback = NULL,
		.saMsgQueueGroupTrackCallback = NULL,
		.saMsgMessageDeliveredCallback = deliver_cb,
	};

	/* parsing options */
	while (!next_command) {
		c = getopt(argc, argv, "vc:");

		switch (c) {

			case 'v':
				option_verbose = 1;
				break;
			case 'c':
				next_command = optarg;
				break;
			default:
				usage();
		}
	}

	if ((ret = saMsgInitialize(&handle, &callbacks, &version)) != SA_OK) {
		dprintf("saMsgInitialize failed, error = [%d]\n", ret);
		return ret;
	}
	dprintf("saMsgInitialize succeed!\n");

	while (next_command) {
		int found = 0;
		int i;

		for (i = 0; commands[i].name != NULL; i++) {

			if (strncmp(next_command, commands[i].name
			,	CMDS_MAX_LENGTH) == 0) {
		
				ret = (commands[i].func)(&handle, argc, argv,
							commands[i].optstr);

				if (ret != SA_OK)
					return ret;

				found = 1;
				break;
			}
		}

		if (!found)
			usage();
	}

	if (optind < argc)
		usage();

	if ((ret = saMsgFinalize(&handle)) != SA_OK) {
		dprintf("saMsgFinalize failed, error = [%d]\n", ret);
		return ret;
	}

	return ret;
}
