/*
 * mof_map.c: map Class properties to msg attributes
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2005 International Business Machines
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

#include <hb_config.h>
#include <hb_api.h>
#include <cmpidt.h>
#include "mof_map.h"

#define MAPDIM(x)	(sizeof(x)/sizeof(map_entry_t))


static const map_entry_t HA_CLUSTER_entry [] = {
	{KEY_HBVERSION,	   "HBVersion",     CMPI_chars},
	{KEY_HOST, 	   "Node",          CMPI_charsA},
	{KEY_HOPS, 	   "HOPFudge",      CMPI_chars},
	{KEY_KEEPALIVE,    "KeepAlive",     CMPI_chars},
	{KEY_DEADTIME, 	   "DeadTime",      CMPI_chars},
	{KEY_DEADPING, 	   "DeadPing",      CMPI_chars},
	{KEY_WARNTIME, 	   "WarnTime",      CMPI_chars},
	{KEY_INITDEAD, 	   "InitDead",      CMPI_chars},
	{KEY_WATCHDOG, 	   "WatchdogTimer", CMPI_chars},
	{KEY_BAUDRATE,	   "BaudRate",      CMPI_chars},
	{KEY_UDPPORT,  	   "UDPPort",       CMPI_chars},
	{KEY_FACILITY, 	   "LogFacility",   CMPI_chars},
	{KEY_LOGFILE, 	   "LogFile",       CMPI_chars},
	{KEY_DBGFILE,	   "DebugFile",     CMPI_chars},
	{KEY_FAILBACK, 	   "NiceFailBack",  CMPI_chars},
	{KEY_AUTOFAIL, 	   "AutoFailBack",  CMPI_chars},
	{KEY_STONITH, 	   "Stonith",       CMPI_chars},
	{KEY_STONITHHOST,  "StonithHost",   CMPI_chars},
	{KEY_CLIENT_CHILD, "Respawn",       CMPI_chars},
	{KEY_RT_PRIO, 	   "RTPriority",    CMPI_chars},
	{KEY_GEN_METH, 	   "GenMethod",     CMPI_chars},
	{KEY_REALTIME, 	   "RealTime",      CMPI_chars},
	{KEY_DEBUGLEVEL,   "DebugLevel",    CMPI_chars},
	{KEY_NORMALPOLL,   "NormalPoll",    CMPI_chars},
	{KEY_APIPERM, 	   "APIAuth",       CMPI_charsA},
	{KEY_MSGFMT, 	   "MsgFmt",        CMPI_chars},
	{KEY_LOGDAEMON,    "UseLogd",       CMPI_chars},
	{KEY_CONNINTVAL,   "ConnLogdTime",  CMPI_chars},
	{KEY_BADPACK, 	   "LogBadPack",    CMPI_chars},
	{KEY_REGAPPHBD,    "NormalPoll",    CMPI_chars},
	{KEY_COREDUMP, 	   "CoreDump",      CMPI_chars},
	{KEY_COREROOTDIR,  "CoreRootDir",   CMPI_chars},
	{KEY_REL2, 	   "WithCrm",       CMPI_chars},
	{"pingnode",       "PingNode",      CMPI_charsA},	
	{"pinggroup",      "PingGroup",     CMPI_charsA}	
};

static const map_entry_t HA_CLUSTER_NODE_entry [] = {			
	{"uname",	"Name",		CMPI_chars},
	{"online",	"OnLine",	CMPI_chars},
        {"standby",	"Standby",	CMPI_chars},
	{"unclean",	"Unclean",	CMPI_chars},
	{"shutdown",	"Shutdown",	CMPI_chars},
	{"expected_up",	"ExpectedUp",	CMPI_chars},
	{"node_ping",	"NodePing",	CMPI_chars},
	{"is_dc",	"IsDC",		CMPI_chars},
};

static const map_entry_t HA_PRIMITIVE_RESOURCE_entry [] = {
	{"id",		"Id",		CMPI_chars},
	{"class",	"ResourceClass",CMPI_chars},
	{"type",	"Type",		CMPI_chars},
	{"provider",	"Provider",	CMPI_chars},
	{"enabled",	"Enabled",	CMPI_chars},
};

static const map_entry_t HA_RESOURCE_CLONE_entry [] = {
	{"id",		"Id",		CMPI_chars},
        {"notify",	"Notify",	CMPI_chars},
        {"ordered",	"Ordered",	CMPI_chars},
        {"interleave",	"Interleave",	CMPI_chars},
        {"clone_max",	"CloneMax",	CMPI_chars},
        {"clone_node_max","CloneNodeMax",CMPI_chars},
	{"enabled",	"Enabled",	CMPI_chars},
};

static const map_entry_t HA_MASTERSLAVE_RESOURCE_entry [] = {
	{"id",			"Id",		CMPI_chars},
        {"clone_max",        	"CloneMax",       CMPI_chars},
        {"clone_node_max",   	"CloneNodeMax",   CMPI_chars},
        {"master_max",	     	"MaxMasters",     CMPI_chars},
        {"master_node_max", 	"MaxNodeMasters", CMPI_chars},
	{"enabled",		"Enabled",	CMPI_chars},
};

static const map_entry_t HA_RESOURCE_GROUP_entry [] = {
	{"id",			"Id",		CMPI_chars},
	{"enabled",		"Enabled",	CMPI_chars},
};


static const map_entry_t HA_OPERATION_entry [] = { 
	{"id",		"Id",		CMPI_chars},
	{"name",	"Name",		CMPI_chars},
	{"interval",	"Interval",	CMPI_chars},
	{"timeout",	"Timeout",	CMPI_chars}
};


static const map_entry_t HA_ORDER_CONSTRAINT_entry [] = {
	{"id",		"Id",		CMPI_chars},
	{"from",	"From",		CMPI_chars},
	{"type",	"OrderType",	CMPI_chars},
        {"to",		"To",		CMPI_chars},
};

static const map_entry_t HA_COLOCATION_CONSTRAINT_entry [] = {
	{"id",		"Id",		CMPI_chars},
	{"from",	"From",		CMPI_chars},
        {"to",		"To",		CMPI_chars},
        {"score",	"Score",	CMPI_chars}
};

static const map_entry_t HA_LOCATION_CONSTRAINT_entry [] = {
	{"id",		"Id",		CMPI_chars},
        {"score",	"Score",	CMPI_chars},
        {"resource",	"Resource",	CMPI_chars}
};

static const map_entry_t HA_INSTANCE_ATTRIBUTES_entry [] = {
	{"id",		"Id",		CMPI_chars},
	{"name",	"Name",		CMPI_chars},
	{"value",	"Value",	CMPI_chars}
};

static const map_entry_t HA_LOCATION_CONSTRAINT_RULE_entry [] = {
	{"id",		"Id",		CMPI_chars},
	{"attribute",	"Attribute",	CMPI_chars},
	{"operation",	"Operation",	CMPI_chars},
	{"value",	"Value",	CMPI_chars},
};

#define MAKE_ENTRY(id,entry) {id, MAPDIM(entry), entry}
static const struct map_t map_table [] =
{
	MAKE_ENTRY(HA_CLUSTER, HA_CLUSTER_entry),
	MAKE_ENTRY(HA_CLUSTER_NODE, HA_CLUSTER_NODE_entry),
	MAKE_ENTRY(HA_PRIMITIVE_RESOURCE, HA_PRIMITIVE_RESOURCE_entry),
	MAKE_ENTRY(HA_RESOURCE_CLONE, HA_RESOURCE_CLONE_entry),
	MAKE_ENTRY(HA_RESOURCE_GROUP, HA_RESOURCE_GROUP_entry),
	MAKE_ENTRY(HA_MASTERSLAVE_RESOURCE, HA_MASTERSLAVE_RESOURCE_entry),
	MAKE_ENTRY(HA_OPERATION, HA_OPERATION_entry),
	MAKE_ENTRY(HA_ORDER_CONSTRAINT, HA_ORDER_CONSTRAINT_entry),
	MAKE_ENTRY(HA_COLOCATION_CONSTRAINT, HA_COLOCATION_CONSTRAINT_entry),
	MAKE_ENTRY(HA_LOCATION_CONSTRAINT, HA_LOCATION_CONSTRAINT_entry),
	MAKE_ENTRY(HA_INSTANCE_ATTRIBUTES, HA_INSTANCE_ATTRIBUTES_entry),
	MAKE_ENTRY(HA_LOCATION_CONSTRAINT_RULE, HA_LOCATION_CONSTRAINT_RULE_entry),
};



const struct map_t *	
cim_query_map(int mapid)
{
	int i;
	for(i = 0; i < sizeof(map_table)/sizeof(struct map_t); i++) {
		if ( mapid == map_table[i].id) {
			return &map_table[i];
		} 
	}
	return NULL;
}


