#ifndef _MOF_MAP_H
#define _MOF_MAP_H

typedef struct mapping_t_s {
	const char * key;
        const char * name;
        int	type;
} mapping_t;

#define MAPDIM(x)	(sizeof(x)/sizeof(mapping_t))

#define MAPPING_HA_Cluster 						\
	{KEY_HBVERSION,	   "HBVersion",     CMPI_chars},		\
	{KEY_HOST, 	   "Node",          CMPI_charsA},		\
	{KEY_HOPS, 	   "HOPFudge",      CMPI_chars},		\
	{KEY_KEEPALIVE,    "KeepAlive",     CMPI_chars},		\
	{KEY_DEADTIME, 	   "DeadTime",      CMPI_chars},		\
	{KEY_DEADPING, 	   "DeadPing",      CMPI_chars},		\
	{KEY_WARNTIME, 	   "WarnTime",      CMPI_chars},		\
	{KEY_INITDEAD, 	   "InitDead",      CMPI_chars},		\
	{KEY_WATCHDOG, 	   "WatchdogTimer", CMPI_chars},		\
	{KEY_BAUDRATE,	   "BaudRate",      CMPI_chars},		\
	{KEY_UDPPORT,  	   "UDPPort",       CMPI_chars},		\
	{KEY_FACILITY, 	   "LogFacility",   CMPI_chars},		\
	{KEY_LOGFILE, 	   "LogFile",       CMPI_chars},		\
	{KEY_DBGFILE,	   "DebugFile",     CMPI_chars},		\
	{KEY_FAILBACK, 	   "NiceFailBack",  CMPI_chars},		\
	{KEY_AUTOFAIL, 	   "AutoFailBack",  CMPI_chars},		\
	{KEY_STONITH, 	   "Stonith",       CMPI_chars},		\
	{KEY_STONITHHOST,  "StonithHost",   CMPI_chars},		\
	{KEY_CLIENT_CHILD, "Respawn",       CMPI_chars},		\
	{KEY_RT_PRIO, 	   "RTPriority",    CMPI_chars},		\
	{KEY_GEN_METH, 	   "GenMethod",     CMPI_chars},		\
	{KEY_REALTIME, 	   "RealTime",      CMPI_chars},		\
	{KEY_DEBUGLEVEL,   "DebugLevel",    CMPI_chars},		\
	{KEY_NORMALPOLL,   "NormalPoll",    CMPI_chars},		\
	{KEY_APIPERM, 	   "APIAuth",       CMPI_charsA},		\
	{KEY_MSGFMT, 	   "MsgFmt",        CMPI_chars},		\
	{KEY_LOGDAEMON,    "UseLogd",       CMPI_chars},		\
	{KEY_CONNINTVAL,   "ConnLogdTime",  CMPI_chars},		\
	{KEY_BADPACK, 	   "LogBadPack",    CMPI_chars},		\
	{KEY_REGAPPHBD,    "NormalPoll",    CMPI_chars},		\
	{KEY_COREDUMP, 	   "CoreDump",      CMPI_chars},		\
	{KEY_COREROOTDIR,  "CoreRootDir",   CMPI_chars},		\
	{KEY_REL2, 	   "WithCrm",       CMPI_chars},		\
	{"pingnode",       "PingNode",      CMPI_charsA},		\
	{"pinggroup",      "PingGroup",     CMPI_charsA}	
			
#define MAPPING_HA_ClusterNode					\
	{"uname",	"Name",		CMPI_chars},		\
	{"online",	"OnLine",	CMPI_chars},		\
        {"standby",	"Standby",	CMPI_chars},		\
	{"unclean",	"Unclean",	CMPI_chars},		\
	{"shutdown",	"Shutdown",	CMPI_chars},		\
	{"expected_up",	"ExpectedUp",	CMPI_chars},		\
	{"node_ping",	"NodePing",	CMPI_chars},		\
	{"is_dc",	"IsDC",		CMPI_chars},

#define MAPPING_HA_PrimitiveResource				\
	{"id",		"Id",		CMPI_chars},		\
	{"class",	"ResourceClass",CMPI_chars},		\
	{"type",	"Type",		CMPI_chars},		\
	{"provider",	"Provider",	CMPI_chars},		\
	{"groupid",	"GroupId",	CMPI_chars}


#define MAPPING_HA_ResourceClone 				\
	{"id",		"Id",		CMPI_chars},		\
        {"notify",	"Notify",	CMPI_chars},		\
        {"ordered",	"Ordered",	CMPI_chars},		\
        {"interleave",	"Interleave",	CMPI_chars},		\
        {"clone_max",	"CloneMax",	CMPI_chars},		\
        {"clone_node_max","CloneNodeMax",CMPI_chars}

#define MAPPING_HA_MasterSlaveResource				\
	{"id",			"Id",		CMPI_chars},	\
        {"clone_max",        	"CloneMax",       CMPI_chars},	\
        {"clone_node_max",   	"CloneNodeMax",   CMPI_chars},	\
        {"master_max",	     	"MaxMasters",     CMPI_chars},	\
        {"master_node_max", 	"MaxNodeMasters", CMPI_chars}

#define MAPPING_HA_Operation					\
	{"id",		"Id",		CMPI_chars},		\
	{"name",	"Name",		CMPI_chars},		\
	{"interval",	"Interval",	CMPI_chars},		\
	{"timeout",	"Timeout",	CMPI_chars}

#define MAPPING_HA_OrderConstraint				\
	{"id",		"Id",		CMPI_chars},		\
	{"from",	"From",		CMPI_chars},		\
	{"type",	"OrderType",	CMPI_chars},		\
        {"",		"Action",	CMPI_chars},		\
        {"to",		"To",		CMPI_chars},		\
        {"",		"Symmetrical",  CMPI_chars}


#define MAPPING_HA_ColocationConstraint				\
	{"id",		"Id",		CMPI_chars},		\
	{"from",	"From",		CMPI_chars},		\
        {"to",		"To",		CMPI_chars},		\
        {"score",	"Score",	CMPI_chars}

#define MAPPING_HA_LocationConstraint				\
	{"id",		"Id",		CMPI_chars},		\
        {"score",	"Score",	CMPI_chars},		\
        {"resource",	"Resource",	CMPI_chars}

#endif
