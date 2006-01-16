/* $Id: config.c,v 1.191 2006/01/16 09:16:32 andrew Exp $ */
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
{ {KEY_HOST,	add_normal_node, FALSE, NULL, NULL}
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
,{KEY_TRADITIONAL_COMPRESSION, set_traditional_compression, TRUE, "yes", "set traditional_compression"}
,{KEY_ENV, set_env, FALSE, NULL, "set environment variable"}
,{KEY_MAX_REXMIT_DELAY, set_max_rexmit_delay, TRUE,"250", "set the maximum rexmit delay time"}
,{KEY_LOG_CONFIG_CHANGES, ha_config_check_boolean, TRUE,"on", "record changes to the cib (valid only with: "KEY_REL2" on)"}
,{KEY_LOG_PENGINE_INPUTS, ha_config_check_boolean, TRUE,"on", "record the input used by the policy engine (valid only with: "KEY_REL2" on)"}
,{KEY_CONFIG_WRITES_ENABLED, ha_config_check_boolean, TRUE,"on", "write configuration changes to disk (valid only with: "KEY_REL2" on)"}
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
	/* config = (struct sys_config *)ha_calloc(1
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
	if (access(HOSTUUIDCACHEFILE, F_OK) >= 0) {
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
				char *		pname = ha_strdup(bp);
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
			/* Does this come from ha_malloc? FIXME!! */
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
				mp->name = ha_strdup(value);
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
			ha_free(listitem->data);
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

void
append_to_dellist(struct node_info* hip)
{
	struct node_info* dup_hip;
	
	dup_hip = ha_malloc(sizeof(struct node_info));
	if (dup_hip == NULL){
		cl_log(LOG_ERR, "%s: malloc failed",
		       __FUNCTION__);
		return;
	}

	memcpy(dup_hip, hip, sizeof(struct node_info));
	
	del_node_list = g_slist_append(del_node_list, dup_hip);
	
	
}


int 
delete_node(const char* value)
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
		cl_log(LOG_ERR, "%s: node %s not found",
		       __FUNCTION__, value);
		return HA_FAIL;
	}

	
	if (STRNCMP_CONST(hip->status, DEADSTATUS) != 0
	    && STRNCMP_CONST(hip->status, INITSTATUS) != 0){
		cl_log(LOG_ERR, "%s: node %s is %s. Cannot delete alive node",
		       __FUNCTION__, value, hip->status);
		return HA_FAIL;
	}
	
	append_to_dellist(hip);

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
	if ((watchdogdev = ha_strdup(value)) == NULL) {
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

	command = ha_malloc(cmdlen+1);
	if (command == NULL) {
		ha_log(LOG_ERR, "Out of memory in add_client_child (command)");
		return HA_FAIL;
	}
	memcpy(command, cmdp, cmdlen);
	command[cmdlen] = EOS;

	path = ha_malloc(pathlen+1);
	if (path == NULL) {
		ha_log(LOG_ERR, "Out of memory in add_client_child "
				"(path)");
		ha_free(command); command=NULL;
		return HA_FAIL;
	}
	memcpy(path, cmdp, pathlen);
	path[pathlen] = EOS;

	if (access(path, X_OK|F_OK) < 0) {
		ha_log(LOG_ERR
		,	"Client child command [%s] is not executable"
		,	path);
		ha_free(command); command=NULL;
		ha_free(path); path=NULL;
		return HA_FAIL;
	}

 	child = MALLOCT(struct client_child);
	if (child == NULL) {
		ha_log(LOG_ERR, "Out of memory in add_client_child (child)");
		ha_free(command); command=NULL;
		ha_free(path); path=NULL;
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
	clname = ha_malloc(clientlen+1);
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
		return HA_FAIL;
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
		ha_free(auth);
		auth = NULL;
	}
	if (clname) {
		ha_free(clname);
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


static int
set_release2mode(const char* value)
{
	const struct do_directive {
		const char * dname;
		const char * dval;
	} r2dirs[] =
		/* CCM already implicit */
	{	{"apiauth", "cib 	uid=" HA_CCMUSER}
	,	{"apiauth", "stonithd   uid=root"}
		/* LRM is not a heartbeat API client */
	,	{"apiauth", "crmd   	uid=" HA_CCMUSER}

	,	{"respawn", " "HA_CCMUSER " " HALIB "/ccm"}
	,	{"respawn", " "HA_CCMUSER " " HALIB "/cib"}
	,	{"respawn", "root "	      HALIB "/stonithd"}
	,	{"respawn", "root "           HALIB "/lrmd"}
	,	{"respawn", " "HA_CCMUSER " " HALIB "/crmd"}
	};
	gboolean	dorel2;
	int		rc;
	int		j;
	int		rc2 = HA_OK;

	if ((rc = cl_str_to_boolean(value, &dorel2)) == HA_OK) {
		if (!dorel2) {
			return HA_OK;
		}
	}else{
		return rc;
	}

	DoManageResources = FALSE;
	if (cl_file_exists(RESOURCE_CFG)){
		cl_log(LOG_WARNING, "File %s exists.", RESOURCE_CFG);
		cl_log(LOG_WARNING, "This file is not used because crm is enabled");
	}
	

	/* Enable release 2 style cluster management */
	for (j=0; j < DIMOF(r2dirs); ++j) {
		int	k;
		for (k=0; k < DIMOF(WLdirectives); ++k) {
			if (0 != strcmp(r2dirs[j].dname, WLdirectives[k].type)) {
				continue;
			}
			if (ANYDEBUG) {
				cl_log(LOG_DEBUG, "Implicit directive: %s %s"
				,	 r2dirs[j].dname
				,	 r2dirs[j].dval);
			}
			if (HA_OK
			!= (rc2 = WLdirectives[k].parse(r2dirs[j].dval))) {
				cl_log(LOG_ERR, "Directive %s %s failed"
				,	r2dirs[j].dname, r2dirs[j].dval);
			}
		}
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

/*
 * $Log: config.c,v $
 * Revision 1.191  2006/01/16 09:16:32  andrew
 * Three new ha.cf options:
 *  - record_config_changes (on/off)
 *  on: the current implementation logs config changes at the value of "debug"
 *  off: the current implementation logs config changes at the value of "debug" + 1
 *  - record_pengine_inputs (on/off)
 *  on: the current implementation logs config changes at the value of "debug"
 *  off: the current implementation logs config changes at the value of "debug" + 1
 *  - enable_config_writes (on/off)
 *  on: write (CIB) config changes to disk
 *  off: do NOT write (CIB) config changes to disk
 *
 * Remove the old enable_config_writes option from the CIB, it was broken and
 *   required linking against the pengine library to fix as correct interpretation
 *   required understanding the CIB's contents.
 *
 * Revision 1.190  2005/12/21 00:01:51  gshi
 * make max rexmit delay tunable in ha.cf
 *
 * Revision 1.189  2005/12/18 21:59:12  alan
 * Changed some comments generated when printing default values.
 *
 * Revision 1.188  2005/12/16 02:11:59  gshi
 * add an entry "env" to tell heartbeat to set environment variable
 * change classic.c quorum module to majority.c
 * change grant.c tiebreaker module to twonodes.c
 *
 * Revision 1.187  2005/12/12 14:27:45  blaschke
 *
 * Fix BEAM "errors" from bug 990
 *
 * Revision 1.186  2005/12/09 22:33:11  alan
 * Disallowed the nomalpoll option.
 * Fixed some text that I broke :-(
 *
 * Revision 1.185  2005/12/09 21:41:24  alan
 * Fixed a minor complaint from amd64, plus added version information, minor URL correction, etc.
 *
 * Revision 1.184  2005/12/09 16:07:38  blaschke
 *
 * Bug 990 - Added -D option to tell heartbeat to display default directive
 * values and -W option to do so in wiki format
 *
 * Revision 1.183  2005/11/08 06:27:38  gshi
 * bug 949: this bcast message is caused by not compressing the message
 * It was so because crmd/cib does not inherit the variable traditional_compression
 * from heartbeat (clplumbing does not provide any function for that)
 *
 * I added inherit_compress() function and called that in crm/cib
 *
 * several modification are made in crm/cib since FT_UNCOMPRESS is essentially  same FT_STRUCT
 * except it will be transmitted after compression
 *
 * Revision 1.182  2005/11/07 22:52:57  gshi
 * fixed a few bugs related to deletion:
 *
 * 1. we need to update the variable curnode since it might point to a different node or invalid node now
 * 2. we need to update the tables stores uuid->node_info pointer and nodename->node_info pointer
 * because the pointer no longer points to the right node
 *
 * Revision 1.181  2005/11/04 23:20:58  gshi
 * always read hostcache file when heartbeat starts no matter what autojoin option is.
 * This is necessary because there could be nodes added using hb_addnode command
 *
 * Revision 1.180  2005/10/27 01:03:21  gshi
 * make node deletion work
 *
 * 1. maintain a delhostcache file for deleted files
 * 2. hb_delnode <node> to delete a node
 *    hb_addnode <node> to add a node
 *
 * TODO:
 * 1) make hb_delnode/hb_addnode accept multiple nodes
 * 2) make deletion only works when all other nodes are active
 * 3) make CCM work with node deletion
 *
 * Revision 1.179  2005/10/17 19:47:44  gshi
 * add an option to use "traditional" compression method
 * traditional_compression yes/no
 * in ha.cf
 *
 * Revision 1.178  2005/10/04 19:37:06  gshi
 * bug 144: UUIDs need to be generatable from nodenames for some cases
 * new directive
 * uuidfrom <file/nodename>
 * default to file
 *
 * Revision 1.177  2005/09/28 20:29:55  gshi
 * change the variable debug to debug_level
 * define it in cl_log
 * move a common function definition from lrmd/mgmtd/stonithd to cl_log
 *
 * Revision 1.176  2005/09/26 18:14:54  gshi
 * R1 style resource management and autojoin other/any should not co-exist
 *
 * Revision 1.175  2005/09/26 04:38:31  gshi
 * bug 901: we should not access hostcache file if autojoin is not set in ha.cf
 *
 * Revision 1.174  2005/09/23 22:35:26  gshi
 * It's necessary to set dead_ticks for nodes that added dynamically.
 * It is ok in initialization when config->deadping_ms/deadtime_ms is not set
 * because they will be set to the correct value at the end of init_config()
 *
 * Revision 1.173  2005/09/19 19:52:05  gshi
 * print out a warning if logfile/debugfile/logfacility is still configured
 * if use_logd is set to "yes" in ha.cf
 *
 * Revision 1.172  2005/09/15 06:13:13  alan
 * more auto-add bugfixes...
 *
 * Revision 1.171  2005/09/15 04:14:33  alan
 * Added the code to configure in the autojoin feature.
 * Of course, if you use it, it will probably break membership at the moment :-)
 *
 * Revision 1.170  2005/09/15 03:59:09  alan
 * Now all the basic pieces to bug 132 are in place - except for a config option
 * to test it with.
 * This means there are three auto-join modes one can configure:
 * 	none	- no nodes may autojoin
 * 	other	- nodes other than ourselves can autojoin
 * 	any	- any node, including ourself can autojoin
 *
 * None is the default.
 *
 * Revision 1.169  2005/09/10 21:51:14  gshi
 * If crm is enabled and haresources file exists, print out a warning
 *
 * Revision 1.168  2005/08/16 15:08:20  gshi
 * change str_to_boolean to cl_str_to_boolean
 * remove the extra copy of that function in cl_log.c
 *
 * Revision 1.167  2005/08/11 20:39:43  gshi
 * move str_to_boolean() function to cl_misc.c file
 *
 * Revision 1.166  2005/08/06 04:35:44  alan
 * Very minor message format change.
 *
 * Revision 1.165  2005/08/05 19:40:13  gshi
 * add compression capability
 *
 * Revision 1.164  2005/08/05 15:32:43  gshi
 * print out an error message if setting facility failed
 *
 * Revision 1.163  2005/07/29 06:55:37  sunjd
 * bug668: license update
 *
 * Revision 1.162  2005/07/13 14:55:41  lars
 * Compile warnings: Ignored return values from sscanf/fgets/system etc,
 * minor signedness issues.
 *
 * Revision 1.161  2005/06/16 09:24:23  davidlee
 * common code for syslog facility name/value conversion
 *
 * Revision 1.160  2005/06/11 13:42:49  alan
 * Fixed a BEAM bug, and made an "info" message into a "debug" message.
 *
 * Revision 1.159  2005/06/02 16:03:30  gshi
 * modify the error message for syntax according Steve D's suggestion
 *
 * Revision 1.158  2005/06/02 15:54:49  gshi
 * fixed a bug pointed by Steve Dobbelstein (steved@us.ibm.com)
 * In config.c we did not seperate uid and gid string before asking ipc_str_to_auth()
 * to generate auth. Now we use two more parameters uidlen and gidlen
 *
 * Revision 1.157  2005/05/19 23:31:36  gshi
 * remove the entry for cl_status
 * change apiauth for anon to gid=HA_APIGROUP
 *
 * Revision 1.156  2005/05/19 23:07:33  gshi
 * add "anon" authorization for anonymous client authorization
 * any casual client matches the "anon" uid/gid authorization will be allowed to login
 *
 * Revision 1.155  2005/05/11 00:44:56  gshi
 * pull the code config.c together to implement ipc_str_to_auth() and use it
 *
 * Revision 1.154  2005/04/27 05:31:42  gshi
 *  use struct cl_uuid_t to replace uuid_t
 * use cl_uuid_xxx to replace uuid_xxx funcitons
 *
 * Revision 1.153  2005/04/15 06:21:59  alan
 * Fixed an extern/static verbose problem pointed out by Peter Billam <peter.billam@dpiwe.tas.gov.au>.
 *
 * Revision 1.152  2005/04/14 05:56:44  gshi
 * We do not enable logd by default unless there is no
 * entry about debug/logfile/logfcility found in ha.cf
 *
 * The detailed policy is:
 *
 * 1. if there is any entry for debugfile/logfile/logfacility in ha.cf
 *      a) if use_logd is not set, logging daemon will not be used
 *      b) if use_logd is set to on, logging daemon will be used
 *      c) if use_logd is set to off, logging daemon will not be used
 *
 * 2. if there is no entry for debugfile/logfile/logfacility in ha.cf
 *      a) if use_logd is not set, logging daemon will be used
 *      b) if use_logd is set to on, logging daemon will be used
 *      c) if use_logd is set to off, config error, i.e. you can not turn
 * 	off all logging options
 *
 * Revision 1.151  2005/04/13 18:04:46  gshi
 * bug 442:
 *
 * Enable logging daemon  by default
 * use static variables in cl_log and export interfaces to get/set variables
 *
 * Revision 1.150  2005/04/13 11:47:51  zhenh
 * set the env variable of debug level
 *
 * Revision 1.149  2005/03/03 16:21:03  andrew
 * Use the common logging setup calls (change cleared by gshi)
 *
 * Revision 1.148  2005/02/21 10:27:10  alan
 * Put in the fix in config.c I meant to put in last time...
 *
 * Revision 1.147  2005/02/21 09:48:38  alan
 * Moved the code to enable processing of T_SHUTDONE messages until after reading
 * the config file.
 *
 * Revision 1.146  2005/02/21 01:16:16  alan
 * Changed the heartbeat code for shutting down clients.
 * We no longer remove them from the list, instead we maintain a pointer
 * to the last client not yet shut down, and update that pointer
 * without changing anything.
 *
 * Revision 1.145  2005/02/14 15:26:51  alan
 * Forgot to turn off DoManageResources(!).  I had thought that this was present
 * before.  Either I deleted it by mistake, or something...
 *
 * Revision 1.144  2005/02/12 10:53:39  alan
 * Put CCM back as an implicit apiauth because the tests require it,
 * and changing it is probably a bad idea :-).
 *
 * Revision 1.143  2005/02/12 10:38:51  alan
 * Added debugging to newly modified implicit CRM directives.
 *
 * Revision 1.142  2005/02/12 10:16:21  alan
 * Modified startup commands for 'crm' directive.
 *
 * Revision 1.141  2005/02/09 11:47:24  andrew
 * Adjust the order a little.  Give the CIB longer to start up.
 *
 * Revision 1.140  2005/02/08 07:24:26  alan
 * Added code to not start cibmon with the 'crm on' option.
 *
 * Revision 1.139  2005/02/04 20:52:12  alan
 * Put in a new directive to enable the CRM and all its paraphanelia
 * required for release 2.
 *
 * Revision 1.138  2005/01/31 17:56:19  alan
 * Made the comment on Deprecated legacy option not come out if DoManageResources
 * is turned off.
 *
 * Revision 1.137  2005/01/20 19:17:49  gshi
 * added flow control, if congestion happens, clients will be paused while heartbeat messages can still go through
 * congestion is denfined as (last_send_out_seq_number - last_ack_seq_number) is greater than half of message queue.
 *
 * Revision 1.136  2005/01/11 04:57:59  alan
 * Put back in the ability to configure from a string as an explicit operation,
 * and also the ability to configure from a file.  These are both
 * assuming old style info strings, and old-style files.
 * The file operations really ought to support a new format too...
 *
 * Revision 1.135  2005/01/03 18:12:10  alan
 * Stonith version 2.
 * Some compatibility with old versions is still missing.
 * Basically, you can't (yet) use files for config information.
 * But, we'll fix that :-).
 * Right now ssh, null and baytech all work.
 *
 * Revision 1.134  2004/11/23 16:26:38  gshi
 * rename directive conn_logd_intval to conn_logd_time
 *
 * Revision 1.133  2004/11/08 20:48:36  gshi
 * implemented logging daemon
 *
 * The logging daemon is to double-buffer log messages to protect us from blocking
 * writes to syslog / logfiles.
 *
 * Revision 1.132  2004/10/20 19:26:55  gshi
 * temporary fix for memory free problem in media failure
 *
 * Revision 1.131  2004/10/16 04:12:56  alan
 * Added core dump directories, and a bunch of code to cd into the
 * right core dump directory, and activated that code in several
 * different applications.  Note that I didn't do them all -- in particular
 * the SAF/AIS applications haven't been touched yet.
 *
 * Revision 1.130  2004/10/08 18:37:06  alan
 * Put in two things:
 * 	Got rid of old SUSEisms in the install process
 *
 * 	Added code to shut down respawn clients in reverse order.
 *
 * Revision 1.129  2004/09/18 22:43:08  alan
 * Merged in changes from 1.2.3
 *
 * Revision 1.128  2004/09/10 22:47:40  alan
 * BEAM FIXES:  various minor fixes related to running out of memory.
 *
 * Revision 1.127  2004/09/10 07:23:50  alan
 * BEAM fixes:  Fixed a couple of resource leaks, and a couple of use-after-free errors.
 *
 * Revision 1.126  2004/09/10 01:12:23  alan
 * BEAM CHANGES: Fixed a couple of very minor bugs, and cleaned up some BEAM warnings.
 *
 * Revision 1.125  2004/08/31 20:58:21  alan
 * Fixed the size of a memset call...
 *
 * Revision 1.124  2004/08/31 20:56:11  alan
 * Fixed the new bug that kept heartbeat from passing BasicSanityCheck
 *
 * Revision 1.123  2004/08/31 18:29:15  alan
 * added the code to config.c to suppress warnings about bad packets
 *
 * Revision 1.122  2004/08/31 13:47:31  alan
 * Put in a bug fix to check for MAXMEDIA in the configuration file.
 *
 * Revision 1.121  2004/07/07 19:07:14  gshi
 * implemented uuid as nodeid
 *
 * Revision 1.120  2004/05/24 09:20:08  sunjd
 * make heartbeat an apphbd client
 *
 * Revision 1.119  2004/05/17 15:12:07  lars
 * Reverting over-eager approach to disabling old resource manager code.
 *
 * Revision 1.118  2004/05/15 09:28:08  andrew
 * Disable ALL legacy resource management iff configured with --enable-crm
 * Possibly I have been a little over-zealous but likely the feature(s)
 *  would need to be re-written to use the new design anyway.
 *
 * Revision 1.117  2004/03/25 10:17:28  lars
 * Part I: Lower-case hostnames whereever they are coming in. STONITH
 * module audit to follow.
 *
 * Revision 1.116  2004/03/25 07:55:38  alan
 * Moved heartbeat libraries to the lib directory.
 *
 * Revision 1.115  2004/03/03 05:31:50  alan
 * Put in Gochun Shi's new netstrings on-the-wire data format code.
 * this allows sending binary data, among many other things!
 *
 * Revision 1.114  2004/02/17 22:11:57  lars
 * Pet peeve removal: _Id et al now gone, replaced with consistent Id header.
 *
 * Revision 1.113  2004/02/10 19:58:48  alan
 * Decreased startup debugging at lowest level.
 *
 * Revision 1.112  2004/02/10 05:29:26  alan
 * Provided more backwards compatibility for pre 1.0.x code than we had before.
 * We now permit nice_failback, even though we complain loudly about it.
 * We now also provide default authorization information for the ipfail
 * and ccm services.
 *
 * Revision 1.111  2004/02/06 07:18:15  horms
 * Fixed duplicated global definitions
 *
 * Revision 1.110  2004/01/30 15:11:27  lars
 * Fix shadow variable.
 *
 * Revision 1.109  2004/01/21 11:34:14  horms
 * - Replaced numerous malloc + strcpy/strncpy invocations with strdup
 *   * This usually makes the code a bit cleaner
 *   * Also is easier not to make code with potential buffer over-runs
 * - Added STRDUP to pils modules
 * - Removed some spurious MALLOC and FREE redefinitions
 *   _that could never be used_
 * - Make sure the return value of strdup is honoured in error conditions
 *
 * Revision 1.108  2004/01/21 00:54:29  horms
 * Added ha_strdup, so strdup allocations are audited
 *
 * Revision 1.107  2004/01/20 08:58:29  horms
 * Removed unused variables
 *
 * Revision 1.106  2004/01/18 21:15:14  alan
 * Put in various fixes associated with authentication bugs and also
 * with an infinite loop bug.
 *
 * Revision 1.105  2004/01/08 08:23:50  horms
 * typo
 *
 * Revision 1.104  2003/12/11 23:00:47  alan
 * Fixed a couple of signed/unsigned problems (warnings) in the code.
 *
 * Revision 1.103  2003/11/20 03:13:55  alan
 * Fixed a bug where we always waited forever for client messages once
 * we got the first one.
 *
 * Added real authentication code to the API infrastructure.
 *
 * Added lots of debugging messages.
 *
 * Changed the IPC code to authenticate based on int values, not on int *'s, since the
 * latter had no advantage and required malloc/freeing storage - which mostly wasn't
 * being done.
 *
 * Revision 1.102  2003/10/29 04:05:00  alan
 * Changed things so that the API uses IPC instead of FIFOs.
 * This isn't 100% done - python API code needs updating, and need to check authorization
 * for the ability to "sniff" other people's packets.
 *
 * Revision 1.101  2003/10/27 19:20:37  alan
 * Fixed a couple of minor but important bugs in the client authentication code.
 *
 * Revision 1.100  2003/10/27 15:47:10  alan
 * Added a new configuration directive for managing API authoriztion.
 *
 * Revision 1.99  2003/09/25 15:17:55  alan
 * Improved "no configuration file" messages for newbies.
 *
 * Revision 1.98  2003/09/23 06:40:55  alan
 * Put in extra explanatory text for explaining that nodes are named by uname -n.
 *
 * Revision 1.97  2003/09/19 19:08:05  alan
 * Fixed a bug where the -d level was always ignored.
 *
 * Revision 1.96  2003/08/14 06:24:13  horms
 * Don't override names modules give themselves
 *
 * Revision 1.95  2003/08/06 13:48:46  horms
 * Allow respawn programmes to have arguments. Diarmuid O'Neill + Horms
 *
 * Revision 1.94  2003/07/12 13:14:38  alan
 * Very minor change - either a minor bug fix or not needed.
 *
 * Revision 1.93  2003/07/03 23:27:19  alan
 * Moved #defines for parameter names to a public header file.
 * Added the ability to ask for the heartbeat version info through the API.
 *
 * Revision 1.92  2003/07/01 16:56:04  alan
 * Added code to allow us to specify whether to use normal or alternative
 * poll methods.
 *
 * Revision 1.91  2003/07/01 16:16:55  alan
 * Put in changes to set and record defaults so they can be
 * retrieved by applications, plugins, and the config dump code.
 *
 * Revision 1.90  2003/07/01 10:12:26  horms
 * Use defines for node types rather than arbitary strings
 *
 * Revision 1.89  2003/07/01 02:36:22  alan
 * Several somewhat-related things in this change set:
 * Added new API call to get general parameters.
 * Added new API code to test this new call.
 * Added new ability to name a node something other than the uname -n name.
 *
 * Revision 1.88  2003/06/28 04:47:51  alan
 * Fixed some terrible, horrible, no good very bad reload bugs -- especially
 * with nice_failback turned on.  Yuck!
 * Also fixed a STONITH bug.  The previous code wouldn't STONTIH a node
 * we hadn't heard from yet -- but we really need to.
 * Decreased debugging verbosity a bit...
 *
 * Revision 1.87  2003/06/19 04:04:00  alan
 * Improved the documentation for how many configuration parameters are
 * stored away by SetParameterValue() and then made available to the
 * plugins through the GetParameterValue() function.
 * Removed a now-superfluous global baudrate variable.
 * These changes inspired by Carson Gaspar <carson@taltos.org>
 *
 * Revision 1.86  2003/06/04 15:39:23  alan
 * Added some comments about some possible future work...
 *
 * Revision 1.85  2003/05/30 14:26:56  kevin
 * LOG_WARNING, not LOG_WARN.
 *
 * Revision 1.84  2003/05/30 14:10:21  alan
 * Include the POSIX realtime functions in appropriate #ifdefs to make
 * OpenBSD work right.
 *
 * Revision 1.83  2003/05/23 05:39:46  alan
 * Changed the options for auto_failback to be a ternary value,
 * and made nice_failback obsolete.
 *
 * Revision 1.82  2003/05/22 05:17:42  alan
 * Added the auto_failback option to heartbeat.
 *
 * Revision 1.81  2003/04/30 22:24:22  alan
 * Added the ability to have a ping node have a different timeout
 * interval than our usual one.
 *
 * Revision 1.80  2003/03/07 01:13:05  alan
 * Put in code for a time-based generation number option.
 *
 * Revision 1.79  2003/02/07 08:37:16  horms
 * Removed inclusion of portability.h from .h files
 * so that it does not need to be installed.
 *
 * Revision 1.78  2003/02/05 09:06:33  horms
 * Lars put a lot of work into making sure that portability.h
 * is included first, everywhere. However this broke a few
 * things when building against heartbeat headers that
 * have been installed (usually somewhere under /usr/include or
 * /usr/local/include).
 *
 * This patch should resolve this problem without undoing all of
 * Lars's hard work.
 *
 * As an asside: I think that portability.h is a virus that has
 * infected all of heartbeat's code and now must also infect all
 * code that builds against heartbeat. I wish that it didn't need
 * to be included all over the place. Especially in headers to
 * be installed on the system. However, I respect Lars's opinion
 * that this is the best way to resolve some weird build problems
 * in the current tree.
 *
 * Revision 1.77  2003/02/05 06:46:19  alan
 * Added the rtprio config option to the ha.cf file.
 *
 * Revision 1.76  2003/02/05 06:07:13  alan
 * Changed the default values for deadtime and heartbeat intervals.
 *
 * Revision 1.75  2003/01/31 10:02:09  lars
 * Various small code cleanups:
 * - Lots of "signed vs unsigned" comparison fixes
 * - time_t globally replaced with TIME_T
 * - All seqnos moved to "seqno_t", which defaults to unsigned long
 * - DIMOF() definition centralized to portability.h and typecast to int
 * - EOS define moved to portability.h
 * - dropped inclusion of signal.h from stonith.h, so that sigignore is
 *   properly defined
 *
 * Revision 1.74  2003/01/16 00:49:47  msoffen
 * Created static variable instead of "run time" allocation for config variable
 * becuase on Solaris the variable wasn't being created with proper memory
 * alignment.
 *
 * Revision 1.73  2003/01/08 21:17:39  msoffen
 * Made changes to allow compiling with -Wtraditional to work.
 *
 * Revision 1.72  2002/11/22 07:04:39  horms
 * make lots of symbols static
 *
 * Revision 1.71  2002/11/21 15:46:03  lars
 * Fix for ucast.c suggested by Sam O'Connor.
 *
 * Revision 1.70  2002/10/18 07:16:08  alan
 * Put in Horms big patch plus a patch for the apcmastersnmp code where
 * a macro named MIN returned the MAX instead.  The code actually wanted
 * the MAX, so when the #define for MIN was surrounded by a #ifndef, then
 * it no longer worked...  This fix courtesy of Martin Bene.
 * There was also a missing #include needed on older Linux systems.
 *
 * Revision 1.69  2002/09/20 02:09:50  alan
 * Switched heartbeat to do everything with longclock_t instead of clock_t.
 * Switched heartbeat to be configured fundamentally from millisecond times.
 * Changed heartbeat to not use alarms for much of anything.
 * These are relatively major changes, but the seem to work fine.
 *
 * Revision 1.68  2002/09/17 13:41:38  alan
 * Fixed a bug in PILS pointed out by lmb which kept it from working
 * 	when a user specified a STONITH directive in heartbeat.
 * 	This had to do with a static variable which I had to get rid of.
 * 	It was a bit painful.
 * Changed heartbeat main to exit with LSB-compliant exit codes.
 * Put in the fixes for debug signals interfering with other signals.
 * Put in code to make us not try and take over resources from ping
 * 	nodes when they go down (since they don't have any).
 * Put in a realtime fix for client API I/O (removed some test code).
 * Changed api_test to use the new cl_log facility.
 * Eliminated some unused code which was supposed to provide
 * 	application heartbeating.  It couldn't yet be used and was a bad idea.
 *
 * Enabled logging to stderr when heartbeat first starts.
 *
 * Revision 1.67  2002/09/10 21:50:06  alan
 * Added code, modified code to move to a common set of logging functions
 * - cl_log() and friends.
 *
 * Revision 1.66  2002/07/30 17:34:37  horms
 * Make sure that the initial dead time is not set to less than
 * 10 seconds if it has not been specified in the configuration file.
 *
 * Revision 1.65  2002/07/08 04:14:12  alan
 * Updated comments in the front of various files.
 * Removed Matt's Solaris fix (which seems to be illegal on Linux).
 *
 * Revision 1.64  2002/04/11 18:33:54  alan
 * Takeover/failover is much faster and a little safer than it was before...
 *
 * For the normal failback case
 * 	If the other machine is down, resources are taken immediately
 *
 * 	If the other machine is up, resources are requested and taken over
 * 		when they have been released.  If the other machine
 * 		never releases them, they are never taken over.
 * 		No background process is ever spawned to "eventually" take
 * 		them over.
 *
 * For the nice failback case
 * 	All resources are acquired *immediately* after the other machine is
 * 		declared dead.
 *
 * Changed the rules about initial deadtime:
 *
 * It now only insists the time be equal to deadtime.
 *
 * It gives a warning if its less than 10 seconds.
 *
 * If not specified, here is how it defaults...
 * 	If deadtime is less than or equal to 10 seconds, then it defaults it to be
 * 	twice the deadtime.
 *
 * 	If deadtime is greater than 10 seconds, then it defaults it to be
 * 	the same as deadtime.
 *
 * Revision 1.63  2002/04/10 07:41:14  alan
 * Enhanced the process tracking code, and used the enhancements ;-)
 * Made a number of minor fixes to make the tests come out better.
 * Put in a retry for writing to one of our FIFOs to allow for
 * an interrupted system call...
 * If a timeout came just as we started our system call, then
 * this could help.  Since it didn't go with a dead process, or
 * other symptoms this could be helpful.
 *
 * Revision 1.62  2002/04/09 06:37:26  alan
 * Fixed the STONITH code so it works again ;-)
 *
 * Also tested (and fixed) the case of graceful shutdown nodes not getting
 * STONITHed.  We also don't STONITH nodes which had no resources at
 * the time they left the cluster, at least when nice_failback is set.
 *
 * Revision 1.61  2002/03/28 02:42:58  alan
 * Fixed a bug where having an error on a media directive which was not on the
 * first occurance on the line made us unload the module prematurely, and then
 * we weren't able to proceed because we had mistakenly unloaded the module,
 * but thought it was still loaded.  Instant core dump.
 * Thanks to Sean Reifschneider for pointing this out.
 *
 * Revision 1.60  2002/03/25 23:54:52  alan
 * Put in fixes to two bugs noted by Gamid Isayev of netzilla networks.
 * One of the two fixes is also due to him.
 *
 * Revision 1.59  2002/03/25 23:19:36  horms
 * Patches from Gamid Isayev to
 *
 * 1) Avoid shadowing the udpport global, hence this
 *    configuration option should now work.
 *
 * 2) Call getservbyname with a vaild protocol (udp), this should
 *    also allow the udpport configuration option to work.
 *
 * --
 * Horms
 *
 * Revision 1.58  2002/03/19 13:43:43  alan
 * Just added a note about very long lines in haresources not being
 * handled correctly.
 *
 * Revision 1.57  2002/02/11 22:31:34  alan
 * Added a new option ('l') to make heartbeat run at low priority.
 * Added support for a new capability - to start and stop client
 * 	processes together with heartbeat itself.
 *
 * Revision 1.56  2002/02/10 23:09:25  alan
 * Added a little initial code to support starting client
 * programs when we start, and shutting them down when we stop.
 *
 * Revision 1.55  2002/02/06 21:22:40  alan
 * Fixed a bug concerning comparing a string to "udp".  Forgot the == 0.
 *
 * Revision 1.54  2002/01/25 05:32:01  alan
 * Put in a warning when 'udp' is encountered.
 *
 * Revision 1.53  2002/01/25 05:29:46  alan
 * Put in a dirty alias kludge to make the software recognize udp as well as bcast.
 *
 * Revision 1.52  2001/10/09 01:19:08  alan
 * Put in fix from Andreas Piesk for detecting / dealing with blank/comment
 * lines properly in config.c
 *
 * Revision 1.51  2001/10/04 21:14:30  alan
 * Patch from Reza Arbab <arbab@austin.ibm.com> to make it compile correctly
 * on AIX.
 *
 * Revision 1.50  2001/10/03 21:28:35  alan
 * Put in Andreas Piesk's fix to the funny question mark at the end of the line
 * on ps output when you have a multicast comm link.
 *
 * Revision 1.49  2001/10/03 05:28:01  alan
 * Fixed a kind of big oops regarding parsing directives...
 *
 * Revision 1.48  2001/10/03 05:22:19  alan
 * Added code to save configuration parameters so we can pass them to the various communication plugins...
 *
 * Revision 1.47  2001/09/29 19:08:24  alan
 * Wonderful security and error correction patch from Emily Ratliff
 * 	<ratliff@austin.ibm.com>
 * Fixes code to have strncpy() calls instead of strcpy calls.
 * Also fixes the number of arguments to several functions which were wrong.
 * Many thanks to Emily.
 *
 * Revision 1.46  2001/08/15 17:58:13  alan
 * Hopefully fixed the -v flag...
 * Soon to remove bag from head?
 *
 * Revision 1.45  2001/08/15 16:56:47  alan
 * Put in the code to allow serial port comm plugins to work...
 *
 * Revision 1.44  2001/08/10 17:35:37  alan
 * Removed some files for comm plugins
 * Moved the rest of the software over to use the new plugin system for comm
 * plugins.
 *
 * Revision 1.43  2001/07/18 21:13:43  alan
 * Put in Emily Ratliff's patch for checking for out of memory in config.c
 *
 */
