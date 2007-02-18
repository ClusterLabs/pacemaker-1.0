/*
 * Parse various heartbeat configuration files...
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
 *	portions (c) 1999,2000 Mitja Sarp
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <portability.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <netdb.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <heartbeat.h>
#include <heartbeat_private.h>
#include <ha_msg.h>
#include <pils/plugin.h>
#include <clplumbing/realtime.h>
#include <clplumbing/netstring.h>
#include <clplumbing/coredumps.h>
#include <stonith/stonith.h>
#include <HBcomm.h>
#include <compress.h>
#include <hb_module.h>
#include <hb_api.h>
#include <hb_config.h>
#include <hb_api_core.h>
#include <clplumbing/cl_syslog.h>
#include  <clplumbing/cl_misc.h>

#define	DIRTYALIASKLUDGE

void dellist_destroy(void);
int dellist_add(const char* nodename);

static int set_cluster_name(const char * value);
static int add_normal_node(const char *);
static int set_hopfudge(const char *);
static int set_keepalive_ms(const char *);
static int set_deadtime_ms(const char *);
static int set_deadping_ms(const char *);
static int set_initial_deadtime_ms(const char *);
static int set_watchdogdev(const char *);
static int set_baudrate(const char *);
static int set_udpport(const char *);
static int set_facility(const char *);
static int set_logfile(const char *);
static int set_dbgfile(const char *);
static int set_nice_failback(const char *);
static int set_auto_failback(const char *);
static int set_warntime_ms(const char *);
static int set_stonith_info(const char *);
static int set_stonith_host_info(const char *);
static int set_realtime_prio(const char *);
static int add_client_child(const char *);
static int set_compression(const char *);
static int set_compression_threshold(const char *);
static int set_traditional_compression(const char *);
static int set_env(const char *);
static int set_max_rexmit_delay(const char *);
static int set_generation_method(const char *);
static int set_realtime(const char *);
static int set_debuglevel(const char *);
static int set_api_authorization(const char *);
static int set_msgfmt(const char*);
static int set_logdaemon(const char*);
static int set_logdconntime(const char *);
static int set_register_to_apphbd(const char *);
static int set_badpack_warn(const char*);
static int set_coredump(const char*);
static int set_corerootdir(const char*);
static int set_release2mode(const char*);
static int set_autojoin(const char*);
static int set_uuidfrom(const char*);
static int ha_config_check_boolean(const char *);
static int set_memreserve(const char *);
static int set_quorum_server(const char * value);
#ifdef ALLOWPOLLCHOICE
  static int set_normalpoll(const char *);
#endif


void hb_set_max_rexmit_delay(int);
/*
 * Each of these parameters is is automatically recorded by
 * SetParameterValue().  They are then passed to the plugins
 * for their use later.  This avoids coupling through global
 * variables.
 */
struct directive {
	const char *	name;
	int		(*add_func) (const char *);
	int		record_value;
	const char *	defaultvalue;
	const char *	explanation;
}Directives[] =
{ {KEY_CLUSTER,	set_cluster_name, TRUE, "linux-ha", "the name of cluster"}
, {KEY_HOST,	add_normal_node, FALSE, NULL, NULL}
, {KEY_HOPS,	set_hopfudge, TRUE, "1", "# of hops above cluster size"}
, {KEY_KEEPALIVE, set_keepalive_ms, TRUE, "1000ms", "keepalive time"}
, {KEY_DEADTIME,  set_deadtime_ms,  TRUE, "30000ms", "node deadtime"}
, {KEY_DEADPING,  set_deadping_ms,  TRUE, NULL, "ping deadtime"}
, {KEY_INITDEAD,  set_initial_deadtime_ms, TRUE, NULL, "initial deadtime"}
, {KEY_WARNTIME,  set_warntime_ms, TRUE, NULL, "warning time"}
, {KEY_WATCHDOG,  set_watchdogdev, TRUE, NULL, "watchdog device"}
, {KEY_BAUDRATE,  set_baudrate, TRUE, "19200", "baud rate"}
, {KEY_UDPPORT,	  set_udpport, TRUE, NULL, "UDP port number"}
, {KEY_FACILITY,  set_facility, TRUE, NULL, "syslog log facility"}
, {KEY_LOGFILE,   set_logfile, TRUE, NULL, "log file"}
, {KEY_DBGFILE,   set_dbgfile, TRUE, NULL, "debug file"}
, {KEY_FAILBACK,  set_nice_failback, FALSE, NULL, NULL}
, {KEY_AUTOFAIL,  set_auto_failback, TRUE, "legacy","auto failback"}
, {KEY_RT_PRIO,	  set_realtime_prio, TRUE, NULL, "realtime priority"}
, {KEY_GEN_METH,  set_generation_method, TRUE, "file", "protocol generation computation method"}
, {KEY_REALTIME,  set_realtime, TRUE, "true", "enable realtime behavior?"}
, {KEY_DEBUGLEVEL,set_debuglevel, TRUE, NULL, "debug level"}
#ifdef ALLOWPOLLCHOICE
, {KEY_NORMALPOLL,set_normalpoll, TRUE, "true", "Use system poll(2) function?"}
#endif
, {KEY_MSGFMT,    set_msgfmt, TRUE, "classic", "message format in the wire"}
, {KEY_LOGDAEMON, set_logdaemon, TRUE, NULL, "use logging daemon"}  
, {KEY_CONNINTVAL,set_logdconntime, TRUE, "60", "the interval to reconnect to logd"}  
, {KEY_REGAPPHBD, set_register_to_apphbd, FALSE, NULL, "register with apphbd"}
, {KEY_BADPACK,   set_badpack_warn, TRUE, "true", "warn about bad packets"}
, {KEY_COREDUMP,  set_coredump, TRUE, "true", "enable Linux-HA core dumps"}
, {KEY_COREROOTDIR,set_corerootdir, TRUE, NULL, "set root directory of core dump area"}
, {KEY_REL2,      set_release2mode, TRUE, "false"
,				"enable release 2 style resource management"}
, {KEY_AUTOJOIN,  set_autojoin, TRUE, "none" ,	"set automatic join mode/style"}
, {KEY_UUIDFROM,  set_uuidfrom, TRUE, "file" ,	"set the source for uuid"}
,{KEY_COMPRESSION,   set_compression, TRUE ,"zlib", "set compression module"}
,{KEY_COMPRESSION_THRESHOLD, set_compression_threshold, TRUE, "2", "set compression threshold"}
,{KEY_TRADITIONAL_COMPRESSION, set_traditional_compression, TRUE, "no", "set traditional_compression"}
,{KEY_ENV, set_env, FALSE, NULL, "set environment variable for respawn clients"}
,{KEY_MAX_REXMIT_DELAY, set_max_rexmit_delay, TRUE,"250", "set the maximum rexmit delay time"}
,{KEY_LOG_CONFIG_CHANGES, ha_config_check_boolean, TRUE,"on", "record changes to the cib (valid only with: "KEY_REL2" on)"}
,{KEY_LOG_PENGINE_INPUTS, ha_config_check_boolean, TRUE,"on", "record the input used by the policy engine (valid only with: "KEY_REL2" on)"}
,{KEY_CONFIG_WRITES_ENABLED, ha_config_check_boolean, TRUE,"on", "write configuration changes to disk (valid only with: "KEY_REL2" on)"}
,{KEY_MEMRESERVE, set_memreserve, TRUE, "6500", "number of kbytes to preallocate in heartbeat"}
,{KEY_QSERVER,set_quorum_server, TRUE, NULL, "the name or ip of quorum server"}
};


static const struct WholeLineDirective {
	const char * type;
	int (*parse) (const char *line);
}WLdirectives[] =
{	{KEY_STONITH,  	   set_stonith_info}
,	{KEY_STONITHHOST,  set_stonith_host_info}
,	{KEY_APIPERM,	   set_api_authorization}
,	{KEY_CLIENT_CHILD,  add_client_child}
};

extern const char *			cmdname;
extern int				parse_only;
extern struct hb_media*			sysmedia[MAXMEDIA];
extern struct sys_config *		config;
extern struct sys_config  		config_init_value;
extern volatile struct pstat_shm *	procinfo;
extern volatile struct process_info *	curproc;
extern char *				watchdogdev;
extern int				nummedia;
extern int                              nice_failback;
extern int                              auto_failback;
extern int				DoManageResources;
extern int				hb_realtime_prio;
extern PILPluginUniv*			PluginLoadingSystem;
extern GHashTable*			CommFunctions;
extern GHashTable*			CompressFuncs;
GHashTable*				APIAuthorization = NULL;
extern struct node_info *   			curnode;
extern int    				timebasedgenno;
int    					enable_realtime = TRUE;
extern int    				debug_level;
int					netstring_format = FALSE;
extern int				UseApphbd;
GSList*					del_node_list;


static int	islegaldirective(const char *directive);
static int	parse_config(const char * cfgfile, char *nodename);
static long	get_msec(const char * input);
static int	add_option(const char *	option, const char * value);


int	num_hb_media_types;

struct hb_media_fns**	hbmedia_types;


#ifdef IRIX
	void setenv(const char *name, const char * value, int);
#endif


static void
check_logd_usage(int* errcount)
{
	const char* value;
	int	truefalse = FALSE;

	/*we set uselogd to TRUE here so the next message can be logged*/
	value = GetParameterValue(KEY_LOGDAEMON);
	if (value != NULL){
		if(cl_str_to_boolean(value, &truefalse) == HA_FAIL){
			cl_log(LOG_ERR, "cl_str_to_boolean failed[%s]", value);
			(*errcount)++;
			return;
		}
		
	}
	
	if (*(config->logfile) == EOS 
	    && *(config->dbgfile) == EOS
	    && config->log_facility <= 0){
		cl_log_set_uselogd(TRUE);
		if (value == NULL){
			cl_log(LOG_INFO, "No log entry found in ha.cf -- use logd");
			add_option(KEY_LOGDAEMON,"yes");
			return;
		}
		
		if (truefalse == FALSE){
			(*errcount)++;
			cl_log(LOG_ERR, "No log entry found in ha.cf "
			       "and use_logd is set to off");				
			return;
		}		
	}else if (value == NULL || truefalse == FALSE){
		cl_log(LOG_WARNING, "Logging daemon is disabled --"
		       "enabling logging daemon is recommended");
	}else{
		cl_log(LOG_WARNING, "logd is enabled but %s%s%s is still"
		       " configured in ha.cf",
		       config->logfile?"logfile":"",
		       config->dbgfile?"/debugfile":"",
		       config->log_facility > 0?"/logfacility":""
		       );
	}
}

static gboolean
r1_style_valid(void)
{
	/* we cannot set autojoin to HB_JOIN_ANY or HB_JOIN_OTHER
	 * in R1 style
	 */
	
	if (!DoManageResources){
		return TRUE;
	}
	
	if (config->rtjoinconfig == HB_JOIN_NONE){
		return TRUE;
	}

	cl_log(LOG_ERR, "R1 style resource management conflicts with "
	       " autojoin set");
	cl_log(LOG_ERR, "You need either unset autojoin or enable crm");
	return FALSE;
}

/*
 *	Read in and validate the configuration file.
 *	Act accordingly.
 */

int
init_config(const char * cfgfile)
{
	int	errcount = 0;
	int	j;
	int	err;

/*
 *	'Twould be good to move this to a shared memory segment
 *	Then we could share this information with others
 */
	/* config = (struct sys_config *)cl_calloc(1
	,	sizeof(struct sys_config)); */
	memset(&config_init_value, 0, sizeof(config_init_value));
	config = &config_init_value;
	if (config == NULL) {
		ha_log(LOG_ERR, "Heartbeat not started: "
			"Out of memory during configuration");
		return(HA_FAIL);
	}
	config->format_vers = 100;
	config->heartbeat_ms = 1000;
	config->deadtime_ms = 30000;
	config->initial_deadtime_ms = -1;
	config->deadping_ms = -1;
	config->hopfudge = 1;
	config->log_facility = -1;
	config->client_list = NULL;
	config->last_client = NULL;
	config->uuidfromname = FALSE;
	
	curnode = NULL;

	if (!parse_config(cfgfile, localnodename)) {
		err = errno;
		ha_log(LOG_ERR, "Heartbeat not started: configuration error.");
		errno=err;
		return(HA_FAIL);
	}
	if (parse_authfile() != HA_OK) {
		err = errno;
		ha_log(LOG_ERR, "Authentication configuration error.");
		errno=err;
		return(HA_FAIL);
	}
	if (config->log_facility >= 0) {
		cl_log_set_entity(cmdname);
		cl_log_set_facility(config->log_facility);
	}


	/* Set any "fixed" defaults */
	for (j=0; j < DIMOF(Directives); ++j) {
		if (!Directives[j].defaultvalue
		||	GetParameterValue(Directives[j].name)) {
			continue;
		}
		add_option(Directives[j].name, Directives[j].defaultvalue);
		
	}

	if (GetParameterValue(KEY_DEBUGLEVEL) == NULL) {
		char	debugstr[10];
		snprintf(debugstr, sizeof(debugstr), "%d", debug_level);
		add_option(KEY_DEBUGLEVEL, debugstr);
	}

	if (nummedia < 1) {
		ha_log(LOG_ERR, "No heartbeat media defined");
		++errcount;
	}

	if (config->warntime_ms <= 0) {
		char tmp[32];
		config->warntime_ms = config->deadtime_ms/2;
		snprintf(tmp, sizeof(tmp), "%ldms", config->warntime_ms);
		SetParameterValue(KEY_WARNTIME, tmp);
	}
	
	/* We should probably complain if there aren't at least two... */
	if (config->nodecount < 1 && config->rtjoinconfig != HB_JOIN_ANY) {
		ha_log(LOG_ERR, "no nodes defined");
		++errcount;
	}
	if (config->authmethod == NULL) {
		ha_log(LOG_ERR, "No authentication specified.");
		++errcount;
	}
	if (access(HOSTUUIDCACHEFILE, R_OK) >= 0) {
		if (read_cache_file(config) != HA_OK) {
			cl_log(LOG_ERR
			,	"Invalid host/uuid map file [%s] - removed."
			,	HOSTUUIDCACHEFILE);
			if (unlink(HOSTUUIDCACHEFILE) < 0) {
				cl_perror("unlink(%s) failed"
				,	HOSTUUIDCACHEFILE);
			}
		}
		write_cache_file(config);
		unlink(HOSTUUIDCACHEFILETMP); /* Can't hurt. */
	}
	if ((curnode = lookup_node(localnodename)) == NULL) {
		if (config->rtjoinconfig == HB_JOIN_ANY) {
			add_normal_node(localnodename);
			curnode = lookup_node(localnodename);
			ha_log(LOG_NOTICE, "Current node [%s] added to configuration."
			,	localnodename);
			write_cache_file(config);
		}else{
			ha_log(LOG_ERR, "Current node [%s] not in configuration!"
			,	localnodename);
			ha_log(LOG_INFO, "By default, cluster nodes are named"
			" by `uname -n` and must be declared with a 'node'"
			" directive in the ha.cf file.");
			ha_log(LOG_INFO, "See also: " HAURL("ha.cf/NodeDirective"));
			++errcount;
		}
	}
	setenv(CURHOSTENV, localnodename, 1);
	if (config->deadtime_ms <= 2 * config->heartbeat_ms) {
		ha_log(LOG_ERR
		,	"Dead time [%ld] is too small compared to keeplive [%ld]"
		,	config->deadtime_ms, config->heartbeat_ms);
		++errcount;
	}
	if (config->initial_deadtime_ms < 0) {
		char tmp[32];
		if (config->deadtime_ms > 10000) {
			config->initial_deadtime_ms = config->deadtime_ms;
		}else{
			if (config->deadtime_ms < 6000) {
				config->initial_deadtime_ms = 12000;
			}else{
				config->initial_deadtime_ms = 
					2 * config->deadtime_ms;
			}
		}
		snprintf(tmp, sizeof(tmp), "%ldms"
		,	config->initial_deadtime_ms);
		SetParameterValue(KEY_INITDEAD, tmp);
	}

	/* Check deadtime parameters */
	if (config->initial_deadtime_ms < config->deadtime_ms) {
		ha_log(LOG_ERR
		,	"Initial dead time [%ld] is smaller than"
	        " deadtime [%ld]"
		,	config->initial_deadtime_ms, config->deadtime_ms);
		++errcount;
	}else if (config->initial_deadtime_ms < 10000) {
		ha_log(LOG_WARNING, "Initial dead time [%ld ms] may be too small!"
		,	config->initial_deadtime_ms);
		ha_log(LOG_INFO
		, "Initial dead time accounts for slow network startup time");
		ha_log(LOG_INFO
		, "It should be >= deadtime and >= 10 seconds");
	}
	if (config->deadping_ms < 0 ){
		char tmp[32];
		config->deadping_ms = config->deadtime_ms;
		snprintf(tmp, sizeof(tmp), "%ldms", config->deadping_ms);
		SetParameterValue(KEY_DEADPING, tmp);
	}else if (config->deadping_ms <= 2 * config->heartbeat_ms) {
		ha_log(LOG_ERR
		,	"Ping dead time [%ld] is too small"
		" compared to keeplive [%ld]"
		,	config->deadping_ms, config->heartbeat_ms);
		++errcount;
	}
	if (GetParameterValue(KEY_UDPPORT) == NULL) {
		struct servent*	service;
		int		tmpudpport;
		char		tmp[32];
 		/* If our service name is in /etc/services, then use it */
		if ((service=getservbyname(HA_SERVICENAME, "udp")) != NULL){
			tmpudpport = ntohs(service->s_port);
		}else{
			tmpudpport = UDPPORT;
		}
		snprintf(tmp, (sizeof(tmp)-1), "%d", tmpudpport);
		SetParameterValue(KEY_UDPPORT, tmp);
	}

	if (!nice_failback && DoManageResources) {
		ha_log(LOG_WARNING
		,	"Deprecated 'legacy' auto_failback option selected.");
		ha_log(LOG_WARNING
		,	"Please convert to 'auto_failback on'.");
		ha_log(LOG_WARNING
		,	"See documentation for conversion details.");
	}

	if (*(config->logfile) == EOS) {
                 if (config->log_facility > 0) {
                        /* 
                         * Set to DEVNULL in case a stray script outputs logs
                         */
			 strncpy(config->logfile, DEVNULL
				, 	sizeof(config->logfile));
                        config->use_logfile=0;
                  }
	}
	if (*(config->dbgfile) == EOS) {
	        if (config->log_facility > 0) {
		        /* 
			 * Set to DEVNULL in case a stray script outputs errors
		        */
		        strncpy(config->dbgfile, DEVNULL
			,	sizeof(config->dbgfile));
                        config->use_dbgfile=0;
	        }
        }
	
	check_logd_usage(&errcount);
	if ( !r1_style_valid()){
		errcount++;
	}
	
	if (!RestartRequested && errcount == 0 && !parse_only) {
		ha_log(LOG_INFO, "**************************");
		ha_log(LOG_INFO, "Configuration validated."
		" Starting heartbeat %s", VERSION);
	}
	for (j=0; j < config->nodecount; ++j) {
		config->nodes[j].has_resources = DoManageResources;
		if (config->nodes[j].nodetype == PINGNODE_I) {
			config->nodes[j].dead_ticks
			=	msto_longclock(config->deadping_ms);
		}else{
			config->nodes[j].dead_ticks
			=	msto_longclock(config->deadtime_ms);
		}
	}

	if (errcount == 0 && DoManageResources) {
		init_resource_module();
	}
	
	return(errcount ? HA_FAIL : HA_OK);
}

static void
init_node_link_info(struct node_info *   node)
{
	longclock_t	cticks = time_longclock();
	int		j;

	if (node->nodetype == PINGNODE_I) {
		node->nlinks = 1;
		for (j=0; j < nummedia; j++) {
			struct link *lnk = &node->links[0];
			if (!sysmedia[j]->vf->isping()
			||	strcmp(node->nodename
			,	sysmedia[j]->name) != 0) {
				continue;
			}
			lnk->name = node->nodename;
			lnk->lastupdate = cticks;
			strncpy(lnk->status, DEADSTATUS
			,	sizeof(lnk->status));
			lnk[1].name = NULL;
			break;
		}
		return;
	}
	node->nlinks = 0;
	for (j=0; j < nummedia; j++) {
		int nc = node->nlinks;
		struct link *lnk = &node->links[nc];
		if (sysmedia[j]->vf->isping()) {
			continue;
		}
		lnk->name = sysmedia[j]->name;
		lnk->lastupdate = cticks;
		strncpy(lnk->status, DEADSTATUS, sizeof(lnk->status));
		lnk[1].name = NULL;
		++node->nlinks;
	}
}

/*
 *	Parse the configuration file and stash away the data
 */
static int
parse_config(const char * cfgfile, char *nodename)
{
	FILE	*	f;
	char		buf[MAXLINE];
	char *		cp;
	char		directive[MAXLINE];
	size_t		dirlength;
	char		option[MAXLINE];
	size_t		optionlength;
	int		errcount = 0;
	int		j;
	int		i;
	struct stat	sbuf;
	struct DefServices {
		const char *	name;
		const char *	authspec;
	} defserv[] = 
	{	{"ipfail",	"uid=" HA_CCMUSER}
	,	{"ccm",		"uid=" HA_CCMUSER}
	,	{"ping",	"gid=" HA_APIGROUP}
	,	{"lha-snmpagent","uid=root"}
	,	{"anon",	"gid=" HA_APIGROUP}
	};

	if ((f = fopen(cfgfile, "r")) == NULL) {
		ha_log(LOG_ERR, "Cannot open config file [%s]", cfgfile);
		ha_log(LOG_INFO
		,       "An annotated sample %s file is provided in"
		" the documentation."
		,       cfgfile);
		ha_log(LOG_INFO
		,       "Please copy it to %s, read it, customize it"
		", and try again."
		,       cfgfile);

		return(HA_FAIL);
	}
	APIAuthorization = g_hash_table_new(g_str_hash, g_str_equal);

	fstat(fileno(f), &sbuf);
	config->cfg_time = sbuf.st_mtime;

	/* It's ugly, but effective  */

	while (fgets(buf, MAXLINE, f) != NULL) {
		char *  bp = buf; 
		int	IsOptionDirective=1;
		struct hb_media_fns*	funs = NULL;

		/* Skip over white space */
		bp += strspn(bp, WHITESPACE);
		
		/* Zap comments on the line */
		if ((cp = strchr(bp, COMMENTCHAR)) != NULL)  {
			*cp = EOS;
		}
		
		/* Strip '\n' and '\r' chars */
		if ((cp = strpbrk(bp, CRLF)) != NULL) {
			*cp = EOS;
		}

		/* Ignore blank (and comment) lines */
		if (*bp == EOS) {
			continue;
		}

		/* Now we expect a directive name */

		dirlength = strcspn(bp, WHITESPACE);
		strncpy(directive, bp, dirlength);
		directive[dirlength] = EOS;
#ifdef DIRTYALIASKLUDGE
		if (strcmp(directive, "udp") == 0) {
			ha_log(LOG_WARNING
			,	"WARNING: directive 'udp' replaced by 'bcast'");
			strncpy(directive, "bcast", sizeof("bcast"));
		}
#endif
		if (!islegaldirective(directive)) {
			ha_log(LOG_ERR, "Illegal directive [%s] in %s"
			,	directive, cfgfile);
			++errcount;
			continue;
		}

		bp += dirlength;

		/* Skip over Delimiters */
		bp += strspn(bp, DELIMS);

		/* Load the medium plugin if its not already loaded... */
		if ((funs=g_hash_table_lookup(CommFunctions, directive))
		==	NULL) {
			if (PILPluginExists(PluginLoadingSystem
			,	HB_COMM_TYPE_S, directive) == PIL_OK) {
				PIL_rc rc;
				if ((rc = PILLoadPlugin(PluginLoadingSystem
				,	HB_COMM_TYPE_S, directive, NULL))
				!=	PIL_OK) {
					ha_log(LOG_ERR, "Cannot load comm"
					" plugin %s [%s]", directive
					,	PIL_strerror(rc));
					continue;
				}
				
				funs=g_hash_table_lookup(CommFunctions
				,	directive);
			}
		}else{
			PILIncrIFRefCount(PluginLoadingSystem
			,	HB_COMM_TYPE_S, directive, +1);
		}


		/* Check first for whole line media-type  directives */
		if (funs && funs->parse)  {
			int num_save = nummedia;
			IsOptionDirective=0;
			if (funs->parse(bp) != HA_OK) {
				PILIncrIFRefCount(PluginLoadingSystem
				,	HB_COMM_TYPE_S, directive, -1);
				errcount++;
				*bp = EOS;	/* Stop parsing now */
				continue;
			}
			sysmedia[num_save]->vf = funs;
			if(!sysmedia[num_save]->name) {
				char *		pname = cl_strdup(bp);
				sysmedia[num_save]->name = pname;
			}
			funs->mtype(&sysmedia[num_save]->type);
			funs->descr(&sysmedia[num_save]->description);
			g_assert(sysmedia[num_save]->type);
			g_assert(sysmedia[num_save]->description);

			*bp = EOS;
		}

		/* Check for "parse" type (whole line) directives */

		for (j=0; j < DIMOF(WLdirectives); ++j) {
			if (WLdirectives[j].parse == NULL)  {
				continue;
			}
			if (strcmp(directive, WLdirectives[j].type) == 0) {
				IsOptionDirective=0;
				if (WLdirectives[j].parse(bp) != HA_OK) {
					errcount++;
				}
				*bp = EOS;
			}
		}
		/* Now Check for  the options-list stuff */
		while (IsOptionDirective && *bp != EOS) {
			optionlength = strcspn(bp, DELIMS);
			strncpy(option, bp, optionlength);
			option[optionlength] = EOS;
			bp += optionlength;
			if (add_option(directive, option) != HA_OK) {
				errcount++;
			}

			/* Skip over Delimiters */
			bp += strspn(bp, DELIMS);
		}
	}

	/* Provide default authorization information for well-known services */
	for (i=0; i < DIMOF(defserv); ++i) {
		char	buf[100];
		/* Allow users to override our defaults... */
		if (g_hash_table_lookup(APIAuthorization, defserv[i].name)
		 == NULL) {
			snprintf(buf, sizeof(buf), "%s %s"
			,	defserv[i].name
			,	defserv[i].authspec);
			set_api_authorization(buf);
		}
	}


	for (i=0; i < config->nodecount; ++i) {
		/*
		 * We need to re-do this now, after all the
		 * media directives were parsed.
		 */
		init_node_link_info(&config->nodes[i]);
	}


	fclose(f);
	return(errcount ? HA_FAIL : HA_OK);
}

/*
 *	Dump the configuration file - as a configuration file :-)
 *
 *	This does not include every directive at this point.
 */
void
dump_config(void)
{
	int	j;
	struct node_info *	hip;


	printf("#\n#	Linux-HA heartbeat configuration (on %s)\n#\n"
	,	localnodename);

	printf("\n#---------------------------------------------------\n");

	printf("#\n#	HA configuration and status\n#\n");

	for (j=0; j < DIMOF(Directives); ++j) {
		const char *	v;
		if (!Directives[j].record_value
		||	(v = GetParameterValue(Directives[j].name)) == NULL) {
			continue;
		}
		printf("%s\t%s", Directives[j].name, v);
		if (Directives[j].explanation) {
			printf("\t#\t%s", Directives[j].explanation);
		}
		printf("\n");
			
	}

	printf("#\n");
	printf("#\tHA Cluster nodes:\n");
	printf("#\n");

	for (j=0; j < config->nodecount; ++j) {
		hip = &config->nodes[j];
		printf("%s %s\t#\t current status: %s\n"
		,	KEY_HOST
		,	hip->nodename
		,	hip->status);
	}

	printf("#\n");
	printf("#\tCommunications media:\n");
	for(j=0; j < nummedia; ++j) {
		g_assert(sysmedia[j]->type);
		g_assert(sysmedia[j]->description);
		puts("#");
		printf("# %s heartbeat channel -------------\n"
		,	sysmedia[j]->description);
		printf("%s %s\n", sysmedia[j]->type
		,	sysmedia[j]->name);
	}
	printf("#---------------------------------------------------\n");
}


/*
 *	Dump the default configuration file values for those directives that
 *	have them
 *
 *	This does not include every directive at this point.
 */
void
dump_default_config(int wikiout)
{
	int		j, k, lmaxlen = 0, cmaxlen = 0, rmaxlen = 0;
	const char *	dashes = "----------------------------------------"
				 "----------------------------------------";
	const char *	lcolhdr = "Directive";
	const char *	ccolhdr = "Default";
	const char *	rcolhdr = "Description";

	/* First determine max name lens to help make things look nice */
	for (j=0; j < DIMOF(Directives); ++j) {
		struct directive * pdir = &Directives[j];
		if (pdir->defaultvalue != NULL) {
			if ((k = strlen(pdir->name)) > lmaxlen) {
				lmaxlen = k;
			}
			if ((k = strlen(pdir->defaultvalue)) > cmaxlen) {
				cmaxlen = k;
			}
			if ((pdir->explanation != NULL)
			&& ((k = strlen(pdir->explanation)) > rmaxlen)) {
				rmaxlen = k;
			}
		}
	}

	/* Don't do anything if there are no default values */
	if (!lmaxlen) {
		printf("There are no default values for ha.cf directives\n");
		return;
	}

	if (wikiout) {
		printf("##Put this output in the ha.cf/DefaultValues"
		" page\n");
		printf("The [wiki:ha.cf ha.cf] directives with default"
		" values are shown below - along with a brief description.\n");
		printf("This was produced by {{{heartbeat -DW}}}"
		" ''# (version %s)''\n\n"
		,	VERSION);

		printf("||\'\'%s\'\'||\'\'%s\'\'||\'\'%s\'\'||\n"
		,	lcolhdr, ccolhdr, rcolhdr);

		for (j=0; j < DIMOF(Directives); ++j) {
			char	WikiName[lmaxlen+1];
			char *	pch;

			if (Directives[j].defaultvalue) {
				strcpy(WikiName, Directives[j].name);
				WikiName[0] = toupper(WikiName[0]);

				/* wiki convention is to remove underscores,
				   slide chars to left, and capitalize */
				while ((pch = strchr(WikiName, '_')) != NULL) {
					char *pchplus1 = pch + 1;
					*pch = toupper(*pchplus1);
					while (*pchplus1) {
						*++pch = *++pchplus1;
					}
				}

				printf("||[wiki:ha.cf/%sDirective"
				" %s]||%s||%s||\n"
				,	WikiName
				,	Directives[j].name
				,	Directives[j].defaultvalue
				,	Directives[j].explanation
				?	Directives[j].explanation : "");
			}
		}
	} else {
		if ((k = strlen(lcolhdr)) > lmaxlen) {
			lmaxlen = k;
		}
		if ((k = strlen(ccolhdr)) > cmaxlen) {
			cmaxlen = k;
		}
		if ((k = strlen(rcolhdr)) > rmaxlen) {
			rmaxlen = k;
		}

		printf("%-*.*s  %-*.*s  %s\n", lmaxlen, lmaxlen, lcolhdr
		,	cmaxlen, cmaxlen, ccolhdr, rcolhdr);
		/* this 4 comes from the pair of 2 blanks between columns */
		printf("%-*.*s\n", (int)sizeof(dashes)
		,	lmaxlen + cmaxlen + rmaxlen + 4, dashes);

		for (j=0; j < DIMOF(Directives); ++j) {
			if (Directives[j].defaultvalue) {
				printf("%-*.*s  %-*.*s  %s\n"
				,	lmaxlen, lmaxlen
				,	Directives[j].name
				,	cmaxlen, cmaxlen
				,	Directives[j].defaultvalue
				,	Directives[j].explanation
				?	Directives[j].explanation : "");
			}
		}
	}
}


/*
 *	Check the /etc/ha.d/haresources file
 *
 *	All we check for now is the set of node names.
 *
 *	It would be good to check the resource names, too...
 *
 *	And for that matter, to compute an md5 checksum of the haresources
 *	file so we can complain if they're different.
 */
int
parse_ha_resources(const char * cfgfile)
{
	char		buf[MAXLINE];
	struct stat	sbuf;
	int		rc = HA_OK;
	FILE *		f;

	if ((f = fopen(cfgfile, "r")) == NULL) {
		ha_log(LOG_ERR, "Cannot open resources file [%s]", cfgfile);
		ha_log(LOG_INFO
		,       "An annotated sample %s file is provided in the"
		" documentation.", cfgfile);
		ha_log(LOG_INFO
		,       "Please copy it to %s, read it, customize it"
		", and try again."
		,       cfgfile);
		return(HA_FAIL);
	}

	fstat(fileno(f), &sbuf);
	config->rsc_time = sbuf.st_mtime;
	
	while (fgets(buf, MAXLINE-1, f) != NULL) {
		char *	bp = buf;
		char *	endp;
		char	token[MAXLINE];

		/* Skip over white space */
		bp += strspn(bp, WHITESPACE);

		if (*bp == COMMENTCHAR) {
			continue;
		}
		
		if (*bp == EOS) {
			continue;
		}
		endp = bp + strcspn(bp, WHITESPACE);
		strncpy(token, bp, endp - bp);
		token[endp-bp] = EOS;
		if (lookup_node(token) == NULL) {
			ha_log(LOG_ERR, "Bad nodename in %s [%s]", cfgfile
			,	token);
			rc = HA_FAIL;
			break;
		}

		/*
		 * FIXME: Really ought to deal with very long lines
		 * correctly.
		 */
		while (buf[strlen(buf)-2]=='\\') {
			if (fgets(buf, MAXLINE-1, f)==NULL)
				break;
		}
	}
	fclose(f);
	return(rc);
}

/*
 *	Is this a legal directive name?
 */
static int
islegaldirective(const char *directive)
{
	int	j;

	/*
	 * We have four kinds of directives to deal with:
	 *
	 *	1) Builtin directives which are keyword value value value...
	 *		"Directives[]"
	 *	2) Builtin directives which are one per line...
	 *		WLdirectives[]
	 *	3) media declarations which are media value value value
	 *		These are dynamically loaded plugins...
	 *		of type HBcomm
	 *	4) media declarations which are media rest-of-line
	 *		These are dynamically loaded plugins...
	 *		of type HBcomm
	 *
	 */
	for (j=0; j < DIMOF(Directives); ++j) {
		if (DEBUGDETAILS) {
			ha_log(LOG_DEBUG
			,	"Comparing directive [%s] against [%s]"
			,	 directive, Directives[j].name);
		}

		if (strcmp(directive, Directives[j].name) == 0) {
			return(HA_OK);
		}
	}
	for (j=0; j < DIMOF(WLdirectives); ++j) {
		if (DEBUGDETAILS) {
			ha_log(LOG_DEBUG
			, "Comparing directive [%s] against WLdirective[%s]"
			,	 directive, WLdirectives[j].type);
		}
		if (strcmp(directive, WLdirectives[j].type) == 0) {
			return(HA_OK);
		}
	}
	if (PILPluginExists(PluginLoadingSystem,  HB_COMM_TYPE_S, directive)
	== PIL_OK){
		return HA_OK;
	}
	return(HA_FAIL);
}

/*
 *	Add the given option/value pair to the configuration
 */
static int
add_option(const char *	option, const char * value)
{
	int	j;
	struct hb_media_fns*	funs = NULL;

	if (ANYDEBUG)  {
		ha_log(LOG_DEBUG, "add_option(%s,%s)", option, value);
	}

	for (j=0; j < DIMOF(Directives); ++j) {
		if (strcmp(option, Directives[j].name) == 0) {
			int rc;
			rc = ((*Directives[j].add_func)(value));
			if (rc == HA_OK && Directives[j].record_value) {
				SetParameterValue(option, value);
			}
			return rc;
		}
	}

	if ((funs=g_hash_table_lookup(CommFunctions, option)) != NULL
		&&	funs->new != NULL) {
		struct hb_media* mp = funs->new(value);
		char*		type;
		char*		descr;

		funs->descr(&descr);
		funs->mtype(&type);

		if (nummedia >= MAXMEDIA) {
			cl_log(LOG_ERR, "Too many media specified (> %d)"
			,	MAXMEDIA);
			cl_log(LOG_INFO, "Offending command: %s %s"
			,	option, value);
			return HA_FAIL;
		}

		sysmedia[nummedia] = mp;
		if (mp == NULL) {
			ha_log(LOG_ERR, "Illegal %s [%s] in config file [%s]"
			,	type, descr, value);
			PILIncrIFRefCount(PluginLoadingSystem
			,	HB_COMM_TYPE_S, option, -1);
			/* Does this come from cl_malloc? FIXME!! */
			g_free(descr); descr = NULL;
			g_free(type);  type = NULL;
			return(HA_FAIL);
		}else{
			mp->type = type;
			mp->description = descr;
			g_assert(mp->type);
			g_assert(mp->description);
			g_assert(mp->type[0] != '(');
			g_assert(mp->description[0] != '(');
			mp->vf = funs;
			if (!mp->name)
				mp->name = cl_strdup(value);
			++nummedia;
			PILIncrIFRefCount(PluginLoadingSystem
			,	HB_COMM_TYPE_S, option, +1);
		}
		g_assert(sysmedia[nummedia-1]->type);
		g_assert(sysmedia[nummedia-1]->description);
		return(HA_OK);
	}
	ha_log(LOG_ERR, "Illegal configuration directive [%s]", option);
	return(HA_FAIL);
}


void 
dellist_destroy(void){
	
	GSList* list = del_node_list;

	while (list != NULL){
		cl_free(list->data);
		list->data=NULL;
		list= list->next;
	}

	g_slist_free(del_node_list);
	del_node_list = NULL;
	return;
}

static void
dellist_append(struct node_info* hip)
{
	struct node_info* dup_hip;
	
	dup_hip = cl_malloc(sizeof(struct node_info));
	if (dup_hip == NULL){
		cl_log(LOG_ERR, "%s: malloc failed",
		       __FUNCTION__);
		return;
	}

	memcpy(dup_hip, hip, sizeof(struct node_info));
	
	del_node_list = g_slist_append(del_node_list, dup_hip);
	
	
}
int 
dellist_add(const char* nodename){
	struct node_info node;
	int i;

	for (i=0; i < config->nodecount; i++){
		if (strncmp(nodename, config->nodes[i].nodename,HOSTLENG) == 0){
			dellist_append(&config->nodes[i]);
			return HA_OK;
		}
	}
	
	memset(&node, 0, sizeof(struct node_info));
	strncpy(node.nodename, nodename, HOSTLENG);
	
	dellist_append(&node);
	return HA_OK;
}

static gint
dellist_match(gconstpointer data, gconstpointer nodename)
{
	const struct node_info* node = (const struct node_info*) data;
	
	if (data == NULL){
		/* the list is empty,i.e. not found*/
		return 1;
	}
	return strncasecmp(node->nodename,nodename, HOSTLENG);
}

void
remove_from_dellist( const char* nodename)
{
	GSList* listitem;
	
	listitem = g_slist_find_custom(del_node_list, nodename, dellist_match);
	
	if (listitem!= NULL){
		if (listitem->data){
			cl_free(listitem->data);
		}
		del_node_list = g_slist_delete_link(del_node_list, listitem);
	}
	
	return;
	
}



/*
 * For reliability reasons, we should probably require nodename
 * to be in /etc/hosts, so we don't lose our mind if (when) DNS goes out...
 * This would also give us an additional sanity check for the config file.
 *
 * This is only the administrative interface, whose IP address never moves
 * around.
 */

/* Process a node declaration */
int
add_node(const char * value, int nodetype)
{
	struct node_info *	hip;
	
	if (config->nodecount >= MAXNODE) {
		return(HA_FAIL);
	}
	
	remove_from_dellist(value);
	
	hip = &config->nodes[config->nodecount];
	memset(hip, 0, sizeof(*hip));
	++config->nodecount;
	strncpy(hip->status, INITSTATUS, sizeof(hip->status));
	strncpy(hip->nodename, value, sizeof(hip->nodename));
	g_strdown(hip->nodename);
	cl_uuid_clear(&hip->uuid);
	hip->rmt_lastupdate = 0L;
	hip->has_resources = TRUE;
	hip->anypacketsyet  = 0;
	hip->local_lastupdate = time_longclock();
	hip->track.nmissing = 0;
	hip->track.last_seq = NOSEQUENCE;
	hip->track.ackseq = 0;
	hip->weight = 100;
	srand(time(NULL));
	hip->track.ack_trigger = rand()%ACK_MSG_DIV;
	hip->nodetype = nodetype;
	add_nametable(hip->nodename, hip);
	init_node_link_info(hip);
	if (nodetype == PINGNODE_I) {
		hip->dead_ticks
			=	msto_longclock(config->deadping_ms);
	}else{
		hip->dead_ticks
			=	msto_longclock(config->deadtime_ms);
	}
	return(HA_OK);
}

int 
set_node_weight(const char* value, int weight)
{
	int i;
	struct node_info * hip = NULL;

	if (value == NULL){
		cl_log(LOG_ERR, "%s: invalid nodename",
		       __FUNCTION__);
		return HA_FAIL;
	}
	
	for (i = 0; i < config->nodecount; i++){
		hip = &config->nodes[i];
		if (strncasecmp(hip->nodename, value, sizeof(hip->nodename)) ==0){
			break;
		}
	}

	if (i == config->nodecount){
		cl_log(LOG_DEBUG,"set weight to non-existing node %s", value);
		return HA_FAIL;
	}
	
	hip->weight = weight;
	return HA_OK;	
}

int 
set_node_site(const char* value, const char* site)
{
	int i;
	struct node_info * hip = NULL;
	
	if (value == NULL){
		cl_log(LOG_ERR, "%s: invalid nodename",
		       __FUNCTION__);
		return HA_FAIL;
	}
	
	for (i = 0; i < config->nodecount; i++){
		hip = &config->nodes[i];
		if (strncasecmp(hip->nodename, value, sizeof(hip->nodename)) ==0){
			break;
		}
	}

	if (i == config->nodecount){
		cl_log(LOG_DEBUG,"set site to non-existing node %s", value);
		return HA_FAIL;
	}
	strncpy(hip->site, site, sizeof(hip->site));
	return HA_OK;	
}

int 
remove_node(const char* value, int deletion)
{
	int i;
	struct node_info *	hip = NULL;
	int j;

	if (value == NULL){
		cl_log(LOG_ERR, "%s: invalid nodename",
		       __FUNCTION__);
		return HA_FAIL;
	}
	
	for (i = 0; i < config->nodecount; i++){
		hip = &config->nodes[i];
		if (strncasecmp(hip->nodename, value, sizeof(hip->nodename)) ==0){
			break;
		}
	}
	

	if (i == config->nodecount){
		if (deletion){
			cl_log(LOG_DEBUG,"Adding node(%s) to deletion list", value);
			dellist_add(value);
		}	
		
		return HA_OK;
	}

	
	if (STRNCMP_CONST(hip->status, DEADSTATUS) != 0
	    && STRNCMP_CONST(hip->status, INITSTATUS) != 0){
		cl_log(LOG_ERR, "%s: node %s is %s. Cannot remove alive node",
		       __FUNCTION__, value, hip->status);
		return HA_FAIL;
	}
	
	if (deletion){
		cl_log(LOG_DEBUG,"Adding this node to deletion list");
		dellist_append(hip);
	}

	for (j = i; j < config->nodecount; j++){
		memcpy(&config->nodes[j], &config->nodes[j + 1], 
		       sizeof(config->nodes[0]));
	}
	
	config->nodecount -- ;

	tables_remove(hip->nodename, &hip->uuid);		
	
	curnode = lookup_node(localnodename);
	if (!curnode){
		cl_log(LOG_ERR, "localnode not found");
	}

	return(HA_OK);	
	
}
/* Set the name of cluster */
static int 
set_cluster_name(const char * value)
{
	strncpy(config->cluster, value, PATH_MAX);
	return(HA_OK);
}
/* Set the quorum server */
static int 
set_quorum_server(const char * value)
{
	strncpy(config->cluster, value, PATH_MAX);
	strncpy(config->quorum_server, value, PATH_MAX);
	return(HA_OK);
}

/* Process a node declaration */
static int
add_normal_node(const char * value)
{
	return add_node(value, NORMALNODE_I);
}



/* Set the hopfudge variable */
static int
set_hopfudge(const char * value)
{
	config->hopfudge = atoi(value);

	if (config->hopfudge >= 0 && config->hopfudge < 256) {
		return(HA_OK);
	}
	return(HA_FAIL);
}

/* Set the keepalive time */
static int
set_keepalive_ms(const char * value)
{
	config->heartbeat_ms = get_msec(value);

	if (config->heartbeat_ms > 0) {
		return(HA_OK);
	}
	return(HA_FAIL);

}

/* Set the dead timeout */
static int
set_deadtime_ms(const char * value)
{
	config->deadtime_ms = get_msec(value);
	if (config->deadtime_ms >= 0) {
		return(HA_OK);
	}
	return(HA_FAIL);
}

/* Set the dead ping timeout */
static int
set_deadping_ms(const char * value)
{
	config->deadping_ms = get_msec(value);
	if (config->deadping_ms >= 0) {
		return(HA_OK);
	}
	return(HA_FAIL);
}

/* Set the initial dead timeout */
static int
set_initial_deadtime_ms(const char * value)
{
	config->initial_deadtime_ms = get_msec(value);
	if (config->initial_deadtime_ms >= 0) {
		return(HA_OK);
	}
	return(HA_FAIL);
}

/* Set the watchdog device */
static int
set_watchdogdev(const char * value)
{

	if (watchdogdev != NULL) {
		fprintf(stderr, "%s: Watchdog device multiply specified.\n"
		,	cmdname);
		return(HA_FAIL);
	}
	if ((watchdogdev = cl_strdup(value)) == NULL) {
		fprintf(stderr, "%s: Out of memory for watchdog device\n"
		,	cmdname);
		return(HA_FAIL);
	}
	return(HA_OK);
}

int
StringToBaud(const char * baudstr)
{
	int	baud;

	baud = atoi(baudstr);
	switch(baud)  {
		case 9600:	return  B9600;
		case 19200:	return  B19200;
#ifdef B38400
		case 38400:	return  B38400;
#endif
#ifdef B57600
		case 57600:	return  B57600;
#endif
#ifdef B115200
		case 115200:	return  B115200;
#endif
#ifdef B230400
		case 230400:	return  B230400;
#endif
#ifdef B460800
		case 460800:	return  B460800;
#endif
		default:	return 0;
	}
}

/*
 * All we do here is *validate* the baudrate.
 * This parameter is automatically recorded by SetParameterValue()
 * for later use by the plugins.
 */
static int
set_baudrate(const char * value)
{
	static int	baudset = 0;
	int		serial_baud =  0;
	if (baudset) {
		fprintf(stderr, "%s: Baudrate multiply specified.\n"
		,	cmdname);
		return(HA_FAIL);
	}
	++baudset;
	serial_baud = StringToBaud(value);
	if (serial_baud <= 0) {
		fprintf(stderr, "%s: invalid baudrate [%s] specified.\n"
		,	cmdname, value);
		return(HA_FAIL);
	}
	return(HA_OK);
}

/*
 * All we do here is *validate* the udpport number.
 * This parameter is automatically recorded by SetParameterValue()
 * for later use by the plugins.
 */
static int
set_udpport(const char * value)
{
	int		port = atoi(value);
	struct servent*	service;

	if (port <= 0) {
		fprintf(stderr, "%s: invalid port [%s] specified.\n"
		,	cmdname, value);
		return(HA_FAIL);
	}

	/* Make sure this port isn't reserved for something else */
	if ((service=getservbyport(htons(port), "udp")) != NULL) {
		if (strcmp(service->s_name, HA_SERVICENAME) != 0) {
			ha_log(LOG_WARNING
			,	"%s: udp port %s reserved for service \"%s\"."
			,	cmdname, value, service->s_name);
		}
	}
	endservent();
	return(HA_OK);
}

/* set syslog facility config variable */
static int
set_facility(const char * value)
{
	int		i;

	i = cl_syslogfac_str2int(value);
	if (i >= 0) {
		config->log_facility = i;
		strncpy(config->facilityname, value,
		  sizeof(config->facilityname)-1);
		config->facilityname[sizeof(config->facilityname)-1] = EOS;
		cl_log_set_facility(config->log_facility);
		return(HA_OK);
	}else {
		ha_log(LOG_ERR, "Log facility(%s) not valid", value);
		return(HA_FAIL);
	}
}

/* set syslog facility config variable */
static int
set_dbgfile(const char * value)
{
	strncpy(config->dbgfile, value, PATH_MAX);
	cl_log_set_debugfile(config->dbgfile);
	config->use_dbgfile=1;
	return(HA_OK);
}

/* set syslog facility config variable */
static int
set_logfile(const char * value)
{
	strncpy(config->logfile, value, PATH_MAX);
	cl_log_set_logfile(config->logfile);
	config->use_logfile=1;
	return(HA_OK);
}

/* sets nice_failback behavior on/off */
static int
set_nice_failback(const char * value)
{
	int	rc;
	int	failback = 0;

	rc = cl_str_to_boolean(value, &failback);

	cl_log(LOG_ERR, "nice_failback flag is obsolete."
	". Use auto_failback {on, off, legacy} instead.");

	if (rc) {
		if (nice_failback) {
			cl_log(LOG_ERR
			,	"'%s %s' has been changed to '%s off'"
			,	KEY_FAILBACK, value, KEY_AUTOFAIL);
			set_auto_failback("off");
		}else{
			cl_log(LOG_ERR
			,	"%s %s has been strictly interpreted as"
			" '%s legacy'"
			,	KEY_FAILBACK, value, KEY_AUTOFAIL);
			cl_log(LOG_ERR
			,	"Consider converting to '%s on'."
			,	KEY_AUTOFAIL);
			cl_log(LOG_ERR
			,	"When you do, then you can use ipfail"
			", and hb_standby");
			set_auto_failback("legacy");
		}
	}
	cl_log(LOG_ERR, "See documentation for details.");
	return rc;
}

/* sets auto_failback behavior on/off */
static int
set_auto_failback(const char * value)
{
	int	rc;
	rc = cl_str_to_boolean(value, &auto_failback);
	if (rc == HA_FAIL) {
		if (strcasecmp(value, "legacy") == 0) {
			nice_failback = FALSE;
			auto_failback = FALSE;
			rc = HA_OK;
		}
	}else{
		nice_failback = TRUE;
	}
	return rc;
}

static int 
set_register_to_apphbd(const char * value)
{
	return cl_str_to_boolean(value, &UseApphbd);
}

/*
 *	Convert a string into a positive, rounded number of milliseconds.
 *
 *	Returns -1 on error.
 *
 *	Permissible forms:
 *		[0-9]+			units are seconds
 *		[0-9]*.[0-9]+		units are seconds
 *		[0-9]+ *[Mm][Ss]	units are milliseconds
 *		[0-9]*.[0-9]+ *[Mm][Ss]	units are milliseconds
 *		[0-9]+ *[Uu][Ss]	units are microseconds
 *		[0-9]*.[0-9]+ *[Uu][Ss]	units are microseconds
 *
 *	Examples:
 *
 *		1		= 1000 milliseconds
 *		1000ms		= 1000 milliseconds
 *		1000000us	= 1000 milliseconds
 *		0.1		= 100 milliseconds
 *		100ms		= 100 milliseconds
 *		100000us	= 100 milliseconds
 *		0.001		= 1 millisecond
 *		1ms		= 1 millisecond
 *		1000us		= 1 millisecond
 *		499us		= 0 milliseconds
 *		501us		= 1 millisecond
 */
#define	NUMCHARS	"0123456789."
static long
get_msec(const char * input)
{
	const char *	cp = input;
	const char *	units;
	long		multiplier = 1000;
	long		divisor = 1;
	long		ret = -1;
	double		dret;

	cp += strspn(cp, WHITESPACE);
	units = cp + strspn(cp, NUMCHARS);
	units += strspn(units, WHITESPACE);

	if (strchr(NUMCHARS, *cp) == NULL) {
		return ret;
	}

	if (strncasecmp(units, "ms", 2) == 0
	||	strncasecmp(units, "msec", 4) == 0) {
		multiplier = 1;
		divisor = 1;
	}else if (strncasecmp(units, "us", 2) == 0
	||	strncasecmp(units, "usec", 4) == 0) {
		multiplier = 1;
		divisor = 1000;
	}else if (*units != EOS && *units != '\n'
	&&	*units != '\r') {
		return ret;
	}
	dret = atof(cp);
	dret *= (double)multiplier;
	dret /= (double)divisor;
	dret += 0.5;
	ret = (long)dret;
	return(ret);
}

/* Set warntime interval */
static int
set_warntime_ms(const char * value)
{
	long	warntime;
	warntime = get_msec(value);

	if (warntime <= 0) {
		fprintf(stderr, "Warn time [%s] is invalid.\n", value);
		return(HA_FAIL);
	}
	config->warntime_ms = warntime;
	return(HA_OK);
}
/*
 * Set Stonith information
 * 
 * Expect a line that looks like:
 * stonith <type> <configfile>
 *
 */
static int
set_stonith_info(const char * value)
{
	const char *	vp = value;
	const char *	evp;
	Stonith *	s;
	char		StonithType [MAXLINE];
	char		StonithFile [MAXLINE];
	size_t		tlen;
	int		rc;

	vp += strspn(vp, WHITESPACE);
	tlen = strcspn(vp, WHITESPACE);

	evp = vp + tlen;

	if (tlen < 1) {
		ha_log(LOG_ERR, "No Stonith type given");
		return(HA_FAIL);
	}
	if (tlen >= sizeof(StonithType)) {
		ha_log(LOG_ERR, "Stonith type too long");
		return(HA_FAIL);
	}

	strncpy(StonithType, vp, tlen);
	StonithType[tlen] = EOS;

	if ((s = stonith_new(StonithType)) == NULL) {
		ha_log(LOG_ERR, "Invalid Stonith type [%s]", StonithType);
		return(HA_FAIL);
	}

	vp = evp + strspn(evp, WHITESPACE);
	if (sscanf(vp, "%[^\r\n]",  StonithFile) <= 0) {
	};

	switch ((rc=stonith_set_config_file(s, StonithFile))) {
		case S_OK:
			/* This will have to change to a list !!! */
			config->stonith = s;
			stonith_get_status(s);
			return(HA_OK);

		case S_BADCONFIG:
			ha_log(LOG_ERR, "Invalid Stonith config file [%s]"
			,	StonithFile);
			break;
		

		default:
			ha_log(LOG_ERR, "Unknown Stonith config error [%s] [%d]"
			,	StonithFile, rc);
			break;
	}
	return(HA_FAIL);
}


/*
 * Set Stonith information
 * 
 * Expect a line that looks like:
 * stonith_host <hostname> <type> <params...>
 *
 */
static int
set_stonith_host_info(const char * value)
{
	const char *	vp = value; /* points to the current token */
	const char *	evp;        /* points to the next token */
	Stonith *	s;
	char		StonithType [MAXLINE];
	char		StonithHost [HOSTLENG];
	size_t		tlen;
	int		rc;
	
	vp += strspn(vp, WHITESPACE);
	tlen = strcspn(vp, WHITESPACE);

	/* Save the pointer just past the hostname field */
	evp = vp + tlen;

	/* Grab the hostname */
	if (tlen < 1) {
		ha_log(LOG_ERR, "No Stonith hostname argument given");
		return(HA_FAIL);
	}	
	if (tlen >= sizeof(StonithHost)) {
		ha_log(LOG_ERR, "Stonith hostname too long");
		return(HA_FAIL);
	}
	strncpy(StonithHost, vp, tlen);
	StonithHost[tlen] = EOS;
	g_strdown(StonithHost);

	/* Verify that this host is valid to create this stonith
	 *  object.  Expect the hostname listed to match this host or '*'
	 */
	
	if (strcmp ("*", StonithHost) != 0 
	&&	strcmp (localnodename, StonithHost)) {
		/* This directive is not valid for this host */
		return HA_OK;
	}
	
	/* Grab the next field */
	vp = evp + strspn(evp, WHITESPACE);
	tlen = strcspn(vp, WHITESPACE);
	
	/* Save the pointer just past the stonith type field */
	evp = vp + tlen;
	
	/* Grab the stonith type */
	if (tlen < 1) {
		ha_log(LOG_ERR, "No Stonith type given");
		return(HA_FAIL);
	}
	if (tlen >= sizeof(StonithType)) {
		ha_log(LOG_ERR, "Stonith type too long");
		return(HA_FAIL);
	}
	
	strncpy(StonithType, vp, tlen);
	StonithType[tlen] = EOS;

	if ((s = stonith_new(StonithType)) == NULL) {
		ha_log(LOG_ERR, "Invalid Stonith type [%s]", StonithType);
		return(HA_FAIL);
	}

	/* Grab the parameters list */
	vp = evp;
	vp += strspn(vp, WHITESPACE);
	
	switch ((rc=stonith_set_config_info(s, vp))) {
		case S_OK:
			/* This will have to change to a list !!! */
			config->stonith = s;
			stonith_get_status(s);
			return(HA_OK);

		case S_BADCONFIG:
			ha_log(LOG_ERR
			,	"Invalid Stonith configuration parameter [%s]"
			,	evp);
			break;
		

		default:
			ha_log(LOG_ERR
			,	"Unknown Stonith config error parsing [%s] [%d]"
			,	evp, rc);
			break;
	}
	return(HA_FAIL);
}
static int
set_realtime_prio(const char * value)
{
#ifdef _POSIX_PRIORITY_SCHEDULING
	int	foo;
	foo = atoi(value);

	if (	foo < sched_get_priority_min(SCHED_FIFO)
	||	foo > sched_get_priority_max(SCHED_FIFO)) {
		ha_log(LOG_ERR, "Illegal realtime priority [%s]", value);
		return HA_FAIL;
	}
	hb_realtime_prio = foo;
#else
	ha_log(LOG_WARNING
	,	"Realtime scheduling not supported on this platform.");
#endif
	return HA_OK;
}
static int
set_generation_method(const char * value)
{
	if (strcmp(value, "file") == 0) {
		timebasedgenno = FALSE;
		return HA_OK;
	}
	if (strcmp(value, "time") != 0) {
		ha_log(LOG_ERR, "Illegal hb generation method [%s]", value);
		return HA_FAIL;
	}
	timebasedgenno = TRUE;
	return HA_OK;
}
static int
set_realtime(const char * value)
{
	int	ret = cl_str_to_boolean(value, &enable_realtime);
	if (ret == HA_OK) {
		if (enable_realtime) {
			cl_enable_realtime();
#ifndef _POSIX_PRIORITY_SCHEDULING
			ha_log(LOG_WARNING
			,	"Realtime scheduling not supported on this platform.");
#endif
		}else{
			cl_disable_realtime();
		}
	}
	return ret;
		
}

static int
set_debuglevel(const char * value)
{
	debug_level = atoi(value);
	if (debug_level >= 0 && debug_level < 256) {
		if (debug_level > 0) {
			static char cdebug[8];
			snprintf(cdebug, sizeof(debug_level), "%d", debug_level);
			setenv(HADEBUGVAL, cdebug, TRUE);
		}
		return(HA_OK);
	}
	return(HA_FAIL);
}

#ifdef ALLOWPOLLCHOICE
static int
set_normalpoll(const char * value)
{
	int	normalpoll=TRUE;
	int	ret = cl_str_to_boolean(value, &normalpoll);
	if (ret == HA_OK) {
		extern int UseOurOwnPoll;
		UseOurOwnPoll = !normalpoll;
	}
	return ret;
}
#endif
static int
set_msgfmt(const char* value)
{
	if( strcmp(value, "classic") ==0 ){
		netstring_format = FALSE;
		cl_set_msg_format(MSGFMT_NVPAIR);
		return HA_OK;
	}
	if( strcmp(value,"netstring") == 0){
		netstring_format = TRUE;
		cl_set_msg_format(MSGFMT_NETSTRING);
		return HA_OK;
	}
	
	return HA_FAIL;
}

static int
set_logdaemon(const char * value)
{
	int	rc;
	int	uselogd;
	rc = cl_str_to_boolean(value, &uselogd);
	
	cl_log_set_uselogd(uselogd);
	
	if (!uselogd){
		cl_log(LOG_WARNING, "Logging daemon is disabled --"
		       "enabling logging daemon is recommended");
	}else{
		cl_log(LOG_INFO, "Enabling logging daemon ");
		cl_log(LOG_INFO, "logfile and debug file are those specified "
		       "in logd config file (default /etc/logd.cf)");
	}
	
	return rc;
}

static int
set_logdconntime(const char * value)
{
	int logdtime;
	logdtime = get_msec(value);
	
	cl_log_set_logdtime(logdtime);
	
	if (logdtime > 0) {
		return(HA_OK);
	}
	return(HA_FAIL);

}

static int
set_badpack_warn(const char* value)
{
	int	warnme = TRUE;
	int	rc;
	rc = cl_str_to_boolean(value, &warnme);

	if (HA_OK == rc) {
		cl_msg_quiet_fmterr = !warnme;
	}
	return rc;
}

static int
add_client_child(const char * directive)
{
	struct client_child*	child;
	const char *		uidp;
	const char *		cmdp;
	char			chuid[64];
	size_t			uidlen;
	size_t			cmdlen;
	size_t			pathlen;
	char*			command;
	char*			path;
	struct passwd*		pw;

	if (ANYDEBUG) {
		ha_log(LOG_INFO, "respawn directive: %s", directive);
	}

	/* Skip over initial white space, so we can get the uid */
	uidp = directive;
	uidp += strspn(uidp, WHITESPACE);
	uidlen = strcspn(uidp, WHITESPACE);

	cmdp = uidp + uidlen+1;

	/* Skip over white space, find the command */
	cmdp += strspn(cmdp, WHITESPACE);
	cmdlen = strcspn(cmdp, CRLF);
	pathlen = strcspn(cmdp, WHITESPACE);
	
	if (uidlen >= sizeof(chuid)) {
		ha_log(LOG_ERR
		,	"UID specified for client child is too long");
		return HA_FAIL;
	}
	memcpy(chuid, uidp, uidlen);
	chuid[uidlen] = EOS;

	if ((pw = getpwnam(chuid)) == NULL) {
		ha_log(LOG_ERR
		,	"Invalid uid [%s] specified for client child"
		,	chuid);
		return HA_FAIL;
	}

	if (*cmdp != '/') {
		ha_log(LOG_ERR
		,	"Client child command [%s] is not full pathname"
		,	cmdp);
		return HA_FAIL;
	}

	command = cl_malloc(cmdlen+1);
	if (command == NULL) {
		ha_log(LOG_ERR, "Out of memory in add_client_child (command)");
		return HA_FAIL;
	}
	memcpy(command, cmdp, cmdlen);
	command[cmdlen] = EOS;

	path = cl_malloc(pathlen+1);
	if (path == NULL) {
		ha_log(LOG_ERR, "Out of memory in add_client_child "
				"(path)");
		cl_free(command); command=NULL;
		return HA_FAIL;
	}
	memcpy(path, cmdp, pathlen);
	path[pathlen] = EOS;

	if (access(path, X_OK|F_OK) < 0) {
		ha_log(LOG_ERR
		,	"Client child command [%s] is not executable"
		,	path);
		cl_free(command); command=NULL;
		cl_free(path); path=NULL;
		return HA_FAIL;
	}

 	child = MALLOCT(struct client_child);
	if (child == NULL) {
		ha_log(LOG_ERR, "Out of memory in add_client_child (child)");
		cl_free(command); command=NULL;
		cl_free(path); path=NULL;
		return HA_FAIL;
	}
	memset(child, 0, sizeof(*child));
	child->respawn = 1;
	child->u_runas = pw->pw_uid;
	child->g_runas = pw->pw_gid;
	child->command = command;
	child->path = path;
	config->client_list = g_list_append(config->client_list, child);
	config->last_client = g_list_last(config->client_list);

	return HA_OK;
}

static int
set_compression(const char * directive)
{		
	return cl_set_compress_fns(directive);
}

static int
set_compression_threshold(const char * value)
{		
	int threshold = atoi(value);
	
	if (threshold <=0){
		cl_log(LOG_ERR, "%s: compress_threshhold(%s)"
		       " invalid",
		       __FUNCTION__,
		       value);
		return HA_FAIL;
	}

	cl_set_compression_threshold(threshold *1024);
	
	return HA_OK;
}


static int
set_traditional_compression(const char * value)
{		
	int result;

	if (value == NULL){
		cl_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return HA_FAIL;
	}
	if (cl_str_to_boolean(value, &result)!= HA_OK){
		cl_log(LOG_ERR, "%s:Invalid directive value %s", 
		       __FUNCTION__,value);
		return HA_FAIL;
	}
	
	cl_set_traditional_compression(result);
	
	return HA_OK;
}

static int
set_env(const char * nvpair)
{		

	int nlen;
	int vlen;
	char* env_name;
	char*	value;

	nlen = strcspn(nvpair, "=");
	if (nlen >= MAXLINE || nlen <=0){
		cl_log(LOG_ERR, "%s: invalid nvpair(%s)",
		       __FUNCTION__, nvpair);
		return HA_FAIL;
	}
	
	env_name = cl_malloc(nlen + 4);
	if (env_name == NULL){
		cl_log(LOG_ERR, "%s: malloc failed",
		       __FUNCTION__);
		return HA_FAIL;
	}
	
	memcpy(env_name, "HA_", 3);
	memcpy(env_name + 3, nvpair, nlen);
	env_name[nlen + 3] = 0;
	
	vlen = strlen(nvpair + nlen + 1);
	if (vlen >= MAXLINE || nlen <=0){
		cl_log(LOG_ERR, "%s: invalid value(%s)",
		       __FUNCTION__, nvpair);
		return HA_FAIL;
	}
	
	value = cl_malloc(vlen + 1);
	if (value == NULL){
		cl_log(LOG_ERR, "%s: malloc failed for value",
		       __FUNCTION__);
		return HA_FAIL;
	}	
	memcpy(value, nvpair+nlen +1  , vlen);
	value[vlen] = 0;
	/*
	 * It is unclear whether any given version of setenv
	 * makes a copy of the name or value, or both.
	 * Therefore it is UNSAFE to free either one.
	 * Fortunately the size of the resulting potential memory leak
	 * is small for this particular situation.
	 */
	setenv(env_name, value, 1);
	if (ANYDEBUG){
		cl_log(LOG_DEBUG, "setting env(%s=%s), nvpair(%s)", env_name, value,nvpair);
	}
	
	return HA_OK;
}
static int
set_max_rexmit_delay(const char * value)
{
	int foo;

	foo =  atoi(value);
	if (foo <= 0){
		cl_log(LOG_ERR, "Invalid max_rexmit_delay time(%s)",
		       value);
		return HA_FAIL;
	}
	
	hb_set_max_rexmit_delay(foo);

	return HA_OK;
}

#if 0
static void
id_table_dump(gpointer key, gpointer value, gpointer user_data)
{
	unsigned int	ikey = GPOINTER_TO_UINT(key);

	cl_log(LOG_DEBUG, "%s %u"
	,	(const char *)user_data, ikey);
	if (value == NULL) {
		cl_log(LOG_ERR, "Key %u has NULL data!!", ikey);
	}
}

static void
dump_auth_tables(struct IPC_AUTH* auth, const char * clientname)
{
	char	uid [] = "uid = ";
	char	gid [] = "gid = ";


	if (auth->uid ) {
		cl_log(LOG_DEBUG, "Dumping uid authorization info for client %s"
		,	clientname);
		g_hash_table_foreach(auth->uid, id_table_dump, uid);
	}
	if (auth->gid) {
		cl_log(LOG_DEBUG, "Dumping gid authorization info for client %s"
		,	clientname);
		g_hash_table_foreach(auth->gid, id_table_dump, gid);
	}
}
#endif



/*
 * apiauth client-name gid=gidlist uid=uidlist
 *
 * Record API permissions for use in API client authorization
 */

static int
set_api_authorization(const char * directive)
{
	const char *		bp;
	const char *		client;
	int			clientlen;
	const char *		gidlist = NULL;
	int			gidlen = 0;
	const char *		uidlist = NULL;
	int			uidlen = 0;
	struct IPC_AUTH*	auth = NULL;
	char* 			clname = NULL;
	client_proc_t	dummy;
	

	/* String processing in 'C' is *so* ugly...   */
	
	/* Skip over any initial white space -- to the client name */
	bp = directive;
	bp += strspn(bp, WHITESPACE);
	if (*bp == EOS) {
		goto baddirective;
	}
	client = bp;
	clientlen = strcspn(bp, WHITESPACE);

	if (clientlen <= 0) {
		goto baddirective;
	}
	if (clientlen >= (int)sizeof(dummy.client_id)) {
		cl_log(LOG_ERR, "client name [%*s] too long"
		,	clientlen, client);
		goto baddirective;
	}
	clname = cl_malloc(clientlen+1);
	if (clname == NULL) {
		cl_log(LOG_ERR, "out of memory for client name");
		goto baddirective;
	}
	strncpy(clname, client, clientlen);
	clname[clientlen] = EOS;

	bp += clientlen;

	bp += strspn(bp, WHITESPACE);

	while (*bp != EOS) {

		bp += strspn(bp, WHITESPACE);

		if (strncmp(bp, "uid=", 4) == 0) {
			if (uidlist != NULL) {
				cl_log(LOG_ERR 
				,	"Duplicate uid list in " KEY_APIPERM);
				goto baddirective;
			}
			bp += 4;
			uidlist=bp;
			uidlen = strcspn(bp, WHITESPACE);
			bp += uidlen;
		}else if (strncmp(bp, "gid=", 4) == 0) {
			if (gidlist != NULL) {
				cl_log(LOG_ERR 
				,	"Duplicate gid list in " KEY_APIPERM);
				goto baddirective;
			}
			bp += 4;
			gidlist=bp;
			gidlen = strcspn(bp, WHITESPACE);
			bp += gidlen;
		}else if (*bp != EOS) {
			cl_log(LOG_ERR 
			,	"Missing uid or gid in " KEY_APIPERM);
			goto baddirective;
		}
	}

	if (uidlist == NULL && gidlist == NULL) {
		goto baddirective;
	}

	if (ANYDEBUG) {
		cl_log(LOG_DEBUG, "uid=%s, gid=%s"
		,	(uidlist == NULL ? "<null>" : uidlist)
		,	(gidlist == NULL ? "<null>" : gidlist));
	}
	auth = ipc_str_to_auth(uidlist, uidlen, gidlist, gidlen);
	if (auth == NULL){
		goto baddirective;
	}

	if (g_hash_table_lookup(APIAuthorization, clname) != NULL) {
		cl_log(LOG_ERR
		,	"Duplicate %s directive for API client %s: [%s]"
		,	KEY_APIPERM, clname, directive);
		goto baddirective;
	}
	g_hash_table_insert(APIAuthorization, clname, auth);
	if (DEBUGDETAILS) {
		cl_log(LOG_DEBUG
		,	"Creating authentication: uidptr=0x%lx gidptr=0x%lx"
		,	(unsigned long)auth->uid, (unsigned long)auth->gid);
	}
	
	return HA_OK;
	
 baddirective:
	cl_log(LOG_ERR, "Invalid %s directive [%s]", KEY_APIPERM, directive);
	cl_log(LOG_INFO, "Syntax: %s client [uid=uidlist] [gid=gidlist]"
	,	KEY_APIPERM);
	cl_log(LOG_INFO, "Where uidlist is a comma-separated list of uids,");
	cl_log(LOG_INFO, "and gidlist is a comma-separated list of gids");
	cl_log(LOG_INFO, "One or the other must be specified.");
	if (auth != NULL) {
		if (auth->uid) {
			/* Ought to destroy the strings too */
			g_hash_table_destroy(auth->uid);
			auth->uid = NULL;
		}
		if (auth->gid) {
			/* Ought destroy the strings too */
			g_hash_table_destroy(auth->gid);
			auth->gid = NULL;
		}
		memset(auth, 0, sizeof(*auth));
		cl_free(auth);
		auth = NULL;
	}
	if (clname) {
		cl_free(clname);
		clname = NULL;
	}
	return HA_FAIL;

}

static int
set_coredump(const char* value)
{
	gboolean	docore;
	int		rc;
	if ((rc = cl_str_to_boolean(value, &docore)) == HA_OK) {
		if (cl_enable_coredumps(docore) < 0 ) {
			rc = HA_FAIL;
		}
	}
	return rc;
}

static int
set_corerootdir(const char* value)
{
	if (cl_set_corerootdir(value) < 0) {
		cl_perror("Invalid core directory [%s]", value);
		return HA_FAIL;
	}
	return HA_OK;
}

/*
 *  Enable all these flags when  KEY_RELEASE2 is enabled...
 *	apiauth lrmd   		uid=root
 *	apiauth stonithd	uid=root
 *	apiauth crmd		uid=hacluster
 *	apiauth cib		uid=hacluster
 *	respawn root		/usr/lib/heartbeat/lrmd
 *	respawn root		/usr/lib/heartbeat/stonithd
 *	respawn hacluster       /usr/lib/heartbeat/ccm
 *	respawn hacluster       /usr/lib/heartbeat/cib
 *	respawn hacluster       /usr/lib/heartbeat/crmd
 */


#define VALGRIND_PREFIX VALGRIND_BIN" --show-reachable=yes --leak-check=full --time-stamp=yes --suppressions="VALGRIND_SUPP" "VALGRIND_LOG

static int
set_release2mode(const char* value)
{
	struct do_directive {
		const char * dname;
		const char * dval;
	}; 
    
    struct do_directive *r2dirs;
    
    struct do_directive r2auto_dirs[] =
	/*
	 *	To whom it may concern:  Please keep the apiauth and respawn
	 *	lines in the same order to make auditing the two against each
	 *	other easier.
	 *	Thank you.
	 */
	
	{	/* CCM apiauth already implicit elsewhere */
		{"apiauth", "cib 	uid=" HA_CCMUSER}
		/* LRMd is not a heartbeat API client */
	,	{"apiauth", "stonithd  	uid=root" }
	,	{"apiauth", "attrd   	uid=" HA_CCMUSER}
	,	{"apiauth", "crmd   	uid=" HA_CCMUSER}
#ifdef MGMT_ENABLED
	,	{"apiauth", "mgmtd   	uid=root" }
#endif
	,	{"apiauth", "pingd   	uid=root"}

	,	{"respawn", " "HA_CCMUSER " " HALIB "/ccm"}
	,	{"respawn", " "HA_CCMUSER " " HALIB "/cib" }
		
	,	{"respawn", "root "           HALIB "/lrmd -r"}
	,	{"respawn", "root "	      HALIB "/stonithd"}
	,	{"respawn", " "HA_CCMUSER " " HALIB "/attrd" }
	,	{"respawn", " "HA_CCMUSER " " HALIB "/crmd" }
#ifdef MGMT_ENABLED
	,	{"respawn", "root "  	      HALIB "/mgmtd -v"}
#endif
		/* Don't 'respawn' pingd - it's a resource agent */
	};

    struct do_directive r2manual_dirs[] =	
	{	/* CCM apiauth already implicit elsewhere */
		{"apiauth", "cib 	uid=" HA_CCMUSER}
	,	{"apiauth", "crmd   	uid=" HA_CCMUSER}

	,	{"respawn", " "HA_CCMUSER " " HALIB "/ccm"}
	,	{"respawn", " "HA_CCMUSER " " HALIB "/cib"}
	,	{"respawn", "root "           HALIB "/lrmd"}
	,	{"respawn", " "HA_CCMUSER " " HALIB "/crmd"}
		/* Don't 'respawn' pingd - it's a resource agent */
	};

    struct do_directive r2valgrind_dirs[] =	
	{	/* CCM apiauth already implicit elsewhere */
		{"apiauth", "cib 	uid=" HA_CCMUSER}
	,	{"apiauth", "stonithd  	uid=root" }
	,	{"apiauth", "attrd   	uid=" HA_CCMUSER}
	,	{"apiauth", "crmd   	uid=" HA_CCMUSER}

	,	{"respawn", " "HA_CCMUSER                   " "HALIB"/ccm"}
	,	{"respawn", " "HA_CCMUSER " "VALGRIND_PREFIX" "HALIB"/cib"}
	,	{"respawn", "root "                            HALIB"/lrmd -r"}
	,	{"respawn", "root "	                       HALIB"/stonithd"}
	,	{"respawn", " "HA_CCMUSER " "VALGRIND_PREFIX" "HALIB"/attrd" }
	,	{"respawn", " "HA_CCMUSER " "VALGRIND_PREFIX" "HALIB"/crmd"}
		/* Don't 'respawn' pingd - it's a resource agent */
	};
    
	gboolean	dorel2;
	int		rc;
	int		j, r2size;
	int		rc2 = HA_OK;

	r2dirs = &r2auto_dirs[0]; r2size = DIMOF(r2auto_dirs);
    
	if ((rc = cl_str_to_boolean(value, &dorel2)) == HA_OK) {
		if (!dorel2) {
			return HA_OK;
		}
	} else {
		if (0 == strcasecmp("manual", value)) {
			r2dirs = &r2manual_dirs[0];
			r2size = DIMOF(r2manual_dirs);
			
		} else if (0 == strcasecmp("valgrind", value)) {
			r2dirs = &r2valgrind_dirs[0];
			r2size = DIMOF(r2valgrind_dirs);
			setenv("HA_VALGRIND_ENABLED", "1", 1);
			
		} else {
			return rc;
		}
	}

	DoManageResources = FALSE;
	if (cl_file_exists(RESOURCE_CFG)){
		cl_log(LOG_WARNING, "File %s exists.", RESOURCE_CFG);
		cl_log(LOG_WARNING, "This file is not used because crm is enabled");
	}
	

	/* Enable release 2 style cluster management */
	for (j=0; j < r2size ; ++j) {
		int	k;
		for (k=0; k < DIMOF(WLdirectives); ++k) {
			if (0 != strcmp(r2dirs->dname, WLdirectives[k].type)) {
				continue;
			}
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG, "Implicit directive: %s %s"
				,	 r2dirs->dname
				,	 r2dirs->dval);
			}
			if (HA_OK
			!= (rc2 = WLdirectives[k].parse(r2dirs->dval))) {
				cl_log(LOG_ERR, "Directive %s %s failed"
				,	r2dirs->dname, r2dirs->dval);
			}            
		}
        r2dirs++;
	}
    
	return rc2;
}

static int
set_autojoin(const char* value)
{
	if (strcasecmp(value, "none") == 0) {
		config->rtjoinconfig = HB_JOIN_NONE;
		return HA_OK;
	}
	if (strcasecmp(value, "other") == 0) {
		config->rtjoinconfig = HB_JOIN_OTHER;
		return HA_OK;
	}
	if (strcasecmp(value, "any") == 0) {
		config->rtjoinconfig = HB_JOIN_ANY;
		return HA_OK;
	}
	cl_log(LOG_ERR, "Invalid %s directive [%s]", KEY_AUTOJOIN, value);
	return HA_FAIL;
}


static int
set_uuidfrom(const char* value)
{
	if (strcmp(value, "file") == 0) {
		config->uuidfromname = FALSE;
		return HA_OK;
	}
	if (strcmp(value, "nodename") == 0) {
		config->uuidfromname =  TRUE;
		return HA_OK;
	}
	cl_log(LOG_ERR, "Invalid %s directive [%s]", KEY_UUIDFROM, value);
	return HA_FAIL;
}
/* Set the memory reserve amount for heartbeat (in kbytes) */
static int
set_memreserve(const char * value)
{
	config->memreserve = atoi(value);

	if (config->memreserve > 0) {
		return(HA_OK);
	}
	return(HA_FAIL);
}

static int
ha_config_check_boolean(const char *value)
{
	int result;

	if (value == NULL){
		cl_log(LOG_ERR, "%s: NULL pointer",
		       __FUNCTION__);
		return HA_FAIL;
	}

	if (cl_str_to_boolean(value, &result)!= HA_OK){
		cl_log(LOG_ERR, "%s:Invalid directive value %s", 
		       __FUNCTION__,value);
		return HA_FAIL;
	}
	
	return HA_OK;
}

