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

#ifndef CRM_AIS_COMMON__H
#define CRM_AIS_COMMON__H

#include <glib.h>
#include <string.h>
#include <openais/ais_util.h>
#include <openais/ipc_gen.h>
#include <openais/saAis.h>

#define MAX_NAME	256
#define AIS_IPC_NAME  "ais-crm-ipc"

#define CRM_NODE_LOST      "lost"
#define CRM_NODE_MEMBER    "member"
#define CRM_NODE_ACTIVE    CRM_NODE_MEMBER
#define CRM_NODE_INACTIVE  CRM_NODE_LOST
#define CRM_NODE_EVICTED   "evicted"


typedef struct crm_ais_host_s AIS_Host;
typedef struct crm_ais_msg_s AIS_Message;

enum crm_ais_msg_class {
    crm_class_cluster = 0,
    crm_class_members = 1,
    crm_class_notify  = 2,
};

/* order here matters - its used to index into the crm_children array */
enum crm_ais_msg_types {
    crm_msg_none = 0,
    crm_msg_ais  = 1,
    crm_msg_cib  = 2,
    crm_msg_lrmd = 3,
    crm_msg_crmd = 4,
    crm_msg_te   = 5,
    crm_msg_pe   = 6,
};

enum crm_proc_flag {
    crm_proc_none    = 0x00000000,
    crm_proc_ais     = 0x00000001,
    crm_proc_lrmd    = 0x00000010,
    crm_proc_stontih = 0x00000020,
    crm_proc_cib     = 0x00000100,
    crm_proc_crmd    = 0x00000200,
    crm_proc_pe      = 0x00001000,
    crm_proc_te      = 0x00002000,
};

typedef struct crm_peer_node_s 
{
	unsigned int id;
	unsigned long long born;

	int32_t votes;
	uint32_t processes;

	char *uname;
	char *state;
	char *uuid;
	char *addr;
} crm_node_t;

struct crm_ais_host_s
{
	uint32_t		id;
	uint32_t		pid;
	gboolean		local;
	enum crm_ais_msg_types	type;
	uint32_t		size;
	char			uname[256];

} __attribute__((packed));

struct crm_ais_msg_s
{
	mar_req_header_t	header __attribute__((aligned(8)));
	uint32_t		id;
	gboolean		is_compressed;
	
	AIS_Host		host;
	AIS_Host		sender;
	
	uint32_t		size;
	uint32_t		compressed_size;
	/* 584 bytes */
	char			data[0];
	
} __attribute__((packed));

static inline const char *msg_type2text(enum crm_ais_msg_types type) 
{
	const char *text = "unknown";
	switch(type) {
		case crm_msg_none:
			text = "unknown";
			break;
		case crm_msg_ais:
			text = "ais";
			break;
		case crm_msg_cib:
			text = "cib";
			break;
		case crm_msg_crmd:
			text = "crmd";
			break;
		case crm_msg_pe:
			text = "pengine";
			break;
		case crm_msg_te:
			text = "tengine";
			break;
		case crm_msg_lrmd:
			text = "lrmd";
			break;
	}
	return text;
}

static inline const char *ais_dest(struct crm_ais_host_s *host) 
{
    if(host->local) {
	return "local";
    } else if(host->size > 0) {
	return host->uname;
    } else {
	return "<all>";
    }
}


#define ais_data_len(msg) (msg->is_compressed?msg->compressed_size:msg->size)

#endif
