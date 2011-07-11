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

#include <crm_internal.h>

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#include <crm/crm.h>
#include <crm/msg_xml.h>
#include <crm/common/xml.h>
#include <crm/common/util.h>
#include <crm/common/ipc.h>
#include <crm/common/iso8601.h>


#if HAVE_HB_CONFIG_H
#include <heartbeat/hb_config.h> /* for HB_COREDIR */
#endif

#if HAVE_GLUE_CONFIG_H
#include <glue_config.h> /* for HB_COREDIR */
#endif

#ifndef MAXLINE
#    define MAXLINE 512
#endif

#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#endif

static uint ref_counter = 0;
unsigned int crm_log_level = LOG_INFO;
gboolean crm_config_error = FALSE;
gboolean crm_config_warning = FALSE;
const char *crm_system_name = "unknown";

int node_score_red = 0;
int node_score_green = 0;
int node_score_yellow = 0;
int node_score_infinity = INFINITY;

void crm_set_env_options(void);

gboolean
check_time(const char *value) 
{
	if(crm_get_msec(value) < 5000) {
		return FALSE;
	}
	return TRUE;
}

gboolean
check_timer(const char *value) 
{
	if(crm_get_msec(value) < 0) {
		return FALSE;
	}
	return TRUE;
}

gboolean
check_boolean(const char *value) 
{
	int tmp = FALSE;
	if(crm_str_to_boolean(value, &tmp) != 1) {
		return FALSE;
	}
	return TRUE;
}

gboolean
check_number(const char *value) 
{
	errno = 0;
	if(value == NULL) {
		return FALSE;
		
	} else if(safe_str_eq(value, MINUS_INFINITY_S)) {
		
	} else if(safe_str_eq(value, INFINITY_S)) {

	} else {
		crm_int_helper(value, NULL);
	}

	if(errno != 0) {
		return FALSE;
	}
	return TRUE;
}

int
char2score(const char *score) 
{
	int score_f = 0;
	
	if(score == NULL) {
		
	} else if(safe_str_eq(score, MINUS_INFINITY_S)) {
		score_f = -node_score_infinity;
		
	} else if(safe_str_eq(score, INFINITY_S)) {
		score_f = node_score_infinity;
		
	} else if(safe_str_eq(score, "+"INFINITY_S)) {
		score_f = node_score_infinity;
		
	} else if(safe_str_eq(score, "red")) {
		score_f = node_score_red;

	} else if(safe_str_eq(score, "yellow")) {
		score_f = node_score_yellow;

	} else if(safe_str_eq(score, "green")) {
		score_f = node_score_green;

	} else {
		score_f = crm_parse_int(score, NULL);
		if(score_f > 0 && score_f > node_score_infinity) {
			score_f = node_score_infinity;
			
		} else if(score_f < 0 && score_f < -node_score_infinity) {
			score_f = -node_score_infinity;
		}
	}
	
	return score_f;
}

char *
score2char(int score) 
{
	if(score >= node_score_infinity) {
		return crm_strdup(INFINITY_S);

	} else if(score <= -node_score_infinity) {
		return crm_strdup("-"INFINITY_S);
	} 
	return crm_itoa(score);
}


const char *
cluster_option(GHashTable* options, gboolean(*validate)(const char*),
	       const char *name, const char *old_name, const char *def_value)
{
	const char *value = NULL;
	CRM_ASSERT(name != NULL);

	if(options != NULL) {
		value = g_hash_table_lookup(options, name);
	}

	if(value == NULL && old_name && options != NULL) {
		value = g_hash_table_lookup(options, old_name);
		if(value != NULL) {
			crm_config_warn("Using deprecated name '%s' for"
				       " cluster option '%s'", old_name, name);
			g_hash_table_insert(
				options, crm_strdup(name), crm_strdup(value));
			value = g_hash_table_lookup(options, old_name);
		}
	}

	if(value == NULL) {
		crm_debug("Using default value '%s' for cluster option '%s'",
			  def_value, name);

		if(options == NULL) {
			return def_value;
		}
		
		g_hash_table_insert(
			options, crm_strdup(name), crm_strdup(def_value));
		value = g_hash_table_lookup(options, name);
	}
	
	if(validate && validate(value) == FALSE) {
		crm_config_err("Value '%s' for cluster option '%s' is invalid."
			      "  Defaulting to %s", value, name, def_value);
		g_hash_table_replace(options, crm_strdup(name),
				     crm_strdup(def_value));
		value = g_hash_table_lookup(options, name);
	}
	
	return value;
}


const char *
get_cluster_pref(GHashTable *options, pe_cluster_option *option_list, int len, const char *name)
{
	int lpc = 0;
	const char *value = NULL;
	gboolean found = FALSE;
	for(lpc = 0; lpc < len; lpc++) {
		if(safe_str_eq(name, option_list[lpc].name)) {
			found = TRUE;
			value = cluster_option(options, 
					       option_list[lpc].is_valid,
					       option_list[lpc].name,
					       option_list[lpc].alt_name,
					       option_list[lpc].default_value);
		}
	}
	CRM_CHECK(found, crm_err("No option named: %s", name));
	CRM_ASSERT(value != NULL);
	return value;
}

void
config_metadata(const char *name, const char *version, const char *desc_short, const char *desc_long,
		pe_cluster_option *option_list, int len)
{
	int lpc = 0;

	fprintf(stdout, "<?xml version=\"1.0\"?>"
		"<!DOCTYPE resource-agent SYSTEM \"ra-api-1.dtd\">\n"
		"<resource-agent name=\"%s\">\n"
		"  <version>%s</version>\n"
		"  <longdesc lang=\"en\">%s</longdesc>\n"
		"  <shortdesc lang=\"en\">%s</shortdesc>\n"
		"  <parameters>\n", name, version, desc_long, desc_short);
	
	for(lpc = 0; lpc < len; lpc++) {
		if(option_list[lpc].description_long == NULL
			&& option_list[lpc].description_short == NULL) {
			continue;
		}
		fprintf(stdout, "    <parameter name=\"%s\" unique=\"0\">\n"
			"      <shortdesc lang=\"en\">%s</shortdesc>\n"
			"      <content type=\"%s\" default=\"%s\"/>\n"
			"      <longdesc lang=\"en\">%s%s%s</longdesc>\n"
			"    </parameter>\n",
			option_list[lpc].name,
			option_list[lpc].description_short,
			option_list[lpc].type,
			option_list[lpc].default_value,
			option_list[lpc].description_long?option_list[lpc].description_long:option_list[lpc].description_short,
			option_list[lpc].values?"  Allowed values: ":"",
			option_list[lpc].values?option_list[lpc].values:"");
	}
	fprintf(stdout, "  </parameters>\n</resource-agent>\n");
}

void
verify_all_options(GHashTable *options, pe_cluster_option *option_list, int len)
{
	int lpc = 0;
	for(lpc = 0; lpc < len; lpc++) {
		cluster_option(options, 
			       option_list[lpc].is_valid,
			       option_list[lpc].name,
			       option_list[lpc].alt_name,
			       option_list[lpc].default_value);
	}
}

char *
generateReference(const char *custom1, const char *custom2)
{

	const char *local_cust1 = custom1;
	const char *local_cust2 = custom2;
	int reference_len = 4;
	char *since_epoch = NULL;

	reference_len += 20; /* too big */
	reference_len += 40; /* too big */
	
	if(local_cust1 == NULL) { local_cust1 = "_empty_"; }
	reference_len += strlen(local_cust1);
	
	if(local_cust2 == NULL) { local_cust2 = "_empty_"; }
	reference_len += strlen(local_cust2);
	
	crm_malloc0(since_epoch, reference_len);

	if(since_epoch != NULL) {
		sprintf(since_epoch, "%s-%s-%ld-%u",
			local_cust1, local_cust2,
			(unsigned long)time(NULL), ref_counter++);
	}

	return since_epoch;
}

gboolean
decodeNVpair(const char *srcstring, char separator, char **name, char **value)
{
	int lpc = 0;
	int len = 0;
	const char *temp = NULL;

	CRM_ASSERT(name != NULL && value != NULL);
	*name = NULL;
	*value = NULL;

	crm_debug_4("Attempting to decode: [%s]", srcstring);
	if (srcstring != NULL) {
		len = strlen(srcstring);
		while(lpc <= len) {
			if (srcstring[lpc] == separator) {
				crm_malloc0(*name, lpc+1);
				if(*name == NULL) {
					break; /* and return FALSE */
				}
				strncpy(*name, srcstring, lpc);
				(*name)[lpc] = '\0';

/* this sucks but as the strtok manpage says..
 * it *is* a bug
 */
				len = len-lpc; len--;
				if(len <= 0) {
					*value = NULL;
				} else {

					crm_malloc0(*value, len+1);
					if(*value == NULL) {
						crm_free(*name);
						break; /* and return FALSE */
					}
					temp = srcstring+lpc+1;
					strncpy(*value, temp, len);
					(*value)[len] = '\0';
				}
				return TRUE;
			}
			lpc++;
		}
	}

	if(*name != NULL) {
		crm_free(*name);
	}
	*name = NULL;
	*value = NULL;
    
	return FALSE;
}

char *
crm_concat(const char *prefix, const char *suffix, char join) 
{
	int len = 0;
	char *new_str = NULL;
	CRM_ASSERT(prefix != NULL);
	CRM_ASSERT(suffix != NULL);
	len = strlen(prefix) + strlen(suffix) + 2;

	crm_malloc0(new_str, (len));
	sprintf(new_str, "%s%c%s", prefix, join, suffix);
	new_str[len-1] = 0;
	return new_str;
}


char *
generate_hash_key(const char *crm_msg_reference, const char *sys)
{
	char *hash_key = crm_concat(sys?sys:"none", crm_msg_reference, '_');
	crm_debug_3("created hash key: (%s)", hash_key);
	return hash_key;
}

char *
generate_hash_value(const char *src_node, const char *src_subsys)
{
	char *hash_value = NULL;
	
	if (src_node == NULL || src_subsys == NULL) {
		return NULL;
	}
    
	if (strcasecmp(CRM_SYSTEM_DC, src_subsys) == 0) {
		hash_value = crm_strdup(src_subsys);
		CRM_ASSERT(hash_value);
		return hash_value;
	}

	hash_value = crm_concat(src_node, src_subsys, '_');
	crm_info("created hash value: (%s)", hash_value);
	return hash_value;
}

char *
crm_itoa(int an_int)
{
	int len = 32;
	char *buffer = NULL;
	
	crm_malloc0(buffer, (len+1));
	if(buffer != NULL) {
		snprintf(buffer, len, "%d", an_int);
	}
	
	return buffer;
}

extern int LogToLoggingDaemon(int priority, const char * buf, int bstrlen, gboolean use_pri_str);

#ifdef HAVE_G_LOG_SET_DEFAULT_HANDLER
GLogFunc glib_log_default;

static void
crm_glib_handler(const gchar *log_domain, GLogLevelFlags flags, const gchar *message, gpointer user_data)
{
	int log_level = LOG_WARNING;
	GLogLevelFlags msg_level = (flags & G_LOG_LEVEL_MASK);

	switch(msg_level) {
	    case G_LOG_LEVEL_CRITICAL:
		/* log and record how we got here */
		crm_abort(__FILE__,__PRETTY_FUNCTION__,__LINE__, message, TRUE, TRUE);
		return;

	    case G_LOG_LEVEL_ERROR:	log_level = LOG_ERR;    break;
	    case G_LOG_LEVEL_MESSAGE:	log_level = LOG_NOTICE; break;
	    case G_LOG_LEVEL_INFO:	log_level = LOG_INFO;   break;
	    case G_LOG_LEVEL_DEBUG:	log_level = LOG_DEBUG;  break;
		
	    case G_LOG_LEVEL_WARNING:
	    case G_LOG_FLAG_RECURSION:
	    case G_LOG_FLAG_FATAL:
	    case G_LOG_LEVEL_MASK:
		log_level = LOG_WARNING;
		break;
	}

	do_crm_log(log_level, "%s: %s", log_domain, message);
}
#endif

void crm_log_deinit(void) {
#ifdef HAVE_G_LOG_SET_DEFAULT_HANDLER
    g_log_set_default_handler(glib_log_default, NULL);
#endif
}

gboolean
crm_log_init(
    const char *entity, int level, gboolean coredir, gboolean to_stderr,
    int argc, char **argv)
{
	/* Redirect messages from glib functions to our handler */
/*  	cl_malloc_forced_for_glib(); */
#ifdef HAVE_G_LOG_SET_DEFAULT_HANDLER
	glib_log_default = g_log_set_default_handler(crm_glib_handler, NULL);
#endif
	
	/* and for good measure... - this enum is a bit field (!) */
	g_log_set_always_fatal((GLogLevelFlags)0); /*value out of range*/
	
	crm_system_name = entity;
	setenv("PCMK_service", crm_system_name, 1);
	cl_log_set_entity(entity);
	if(argc == 0) {
	    /* Nuke any syslog activity */
	    unsetenv("HA_logfacility");

	} else if(getenv("HA_logfacility") == NULL) {
	    /* Set a default */
	    cl_log_set_facility(HA_LOG_FACILITY);
	} /* else: picked up by crm_set_env_options() */
	
	if(coredir) {
	    int user = getuid();
	    struct passwd *pwent = NULL;
	    const char *base = HA_COREDIR;
	    
	    pwent = getpwuid(user);

	    if (chdir(base) < 0) {
		crm_perror(LOG_ERR, "Cannot change active directory to %s", base);

	    } else if (pwent == NULL) {
		crm_perror(LOG_ERR, "Cannot get name for uid: %d", user);

	    } else if (chdir(pwent->pw_name) < 0) {
		crm_perror(LOG_ERR, "Cannot change active directory to %s/%s", base, pwent->pw_name);

	    } else {
		crm_info("Changed active directory to %s/%s", base, pwent->pw_name);
	    }
	}
	
	set_crm_log_level(level);
	crm_set_env_options();

	cl_log_args(argc, argv);
	cl_log_enable_stderr(to_stderr);

	crm_signal(DEBUG_INC, alter_debug);
	crm_signal(DEBUG_DEC, alter_debug);

	return TRUE;
}

/* returns the old value */
unsigned int
set_crm_log_level(unsigned int level)
{
	unsigned int old = crm_log_level;

	while(crm_log_level < 100 && crm_log_level < level) {
		alter_debug(DEBUG_INC);
	}
	while(crm_log_level > 0 && crm_log_level > level) {
		alter_debug(DEBUG_DEC);
	}
	
	return old;
}

unsigned int
get_crm_log_level(void)
{
	return crm_log_level;
}

static int
crm_version_helper(const char *text, char **end_text)
{
	int atoi_result = -1;
	CRM_ASSERT(end_text != NULL);

	errno = 0;
	
	if(text != NULL && text[0] != 0) {
	    atoi_result = (int)strtol(text, end_text, 10);
		
	    if(errno == EINVAL) {
		crm_err("Conversion of '%s' %c failed", text, text[0]);
		atoi_result = -1;
	    }
	}
	return atoi_result;
}


/*
 * version1 < version2 : -1
 * version1 = version2 :  0
 * version1 > version2 :  1
 */
int
compare_version(const char *version1, const char *version2)
{
	int rc = 0;
	int lpc = 0;
	char *ver1_copy = NULL, *ver2_copy = NULL;
	char *rest1 = NULL, *rest2 = NULL;

	if(version1 == NULL && version2 == NULL) {
		return 0;
	} else if(version1 == NULL) {
		return -1;
	} else if(version2 == NULL) {
		return 1;
	}
	
	ver1_copy = crm_strdup(version1);
	ver2_copy = crm_strdup(version2);
	rest1 = ver1_copy;
	rest2 = ver2_copy;

	while(1) {
		int digit1 = 0;
		int digit2 = 0;

		lpc++;

		if(rest1 == rest2) {
		    break;
		}
		
		if(rest1 != NULL) {
		    digit1 = crm_version_helper(rest1, &rest1);
		}

		if(rest2 != NULL) {
		    digit2 = crm_version_helper(rest2, &rest2);
		}

		if(digit1 < digit2){
			rc = -1;
			crm_debug_5("%d < %d", digit1, digit2);
			break;
			
		} else if (digit1 > digit2){
			rc = 1;
			crm_debug_5("%d > %d", digit1, digit2);
			break;
		}

		if(rest1 != NULL && rest1[0] == '.') {
		    rest1++;
		}
		if(rest1 != NULL && rest1[0] == 0) {
		    rest1 = NULL;
		}

		if(rest2 != NULL && rest2[0] == '.') {
		    rest2++;
		}
		if(rest2 != NULL && rest2[0] == 0) {
		    rest2 = NULL;
		}
	}
	
	crm_free(ver1_copy);
	crm_free(ver2_copy);

	if(rc == 0) {
	    crm_debug_3("%s == %s (%d)", version1, version2, lpc);
	} else if(rc < 0) {
	    crm_debug_3("%s < %s (%d)", version1, version2, lpc);
	} else if(rc > 0) {
	    crm_debug_3("%s > %s (%d)", version1, version2, lpc);
	}

	return rc;
}

gboolean do_stderr = FALSE;

void
alter_debug(int nsig) 
{
	crm_signal(DEBUG_INC, alter_debug);
	crm_signal(DEBUG_DEC, alter_debug);
	
	switch(nsig) {
		case DEBUG_INC:
			if (crm_log_level < 100) {
				crm_log_level++;
			}
			break;

		case DEBUG_DEC:
			if (crm_log_level > 0) {
				crm_log_level--;
			}
			break;	

		default:
			fprintf(stderr, "Unknown signal %d\n", nsig);
			cl_log(LOG_ERR, "Unknown signal %d", nsig);
			break;	
	}
}


void g_hash_destroy_str(gpointer data)
{
	crm_free(data);
}

#include <sys/types.h>
/* #include <stdlib.h> */
/* #include <limits.h> */

long long
crm_int_helper(const char *text, char **end_text)
{
    long long result = -1;
    char *local_end_text = NULL;
    
    errno = 0;
    
    if(text != NULL) {
#ifdef ANSI_ONLY
	if(end_text != NULL) {
	    result = strtol(text, end_text, 10);
	} else {
	    result = strtol(text, &local_end_text, 10);
	}
#else
	if(end_text != NULL) {
	    result = strtoll(text, end_text, 10);
	} else {
	    result = strtoll(text, &local_end_text, 10);
	}
#endif
/* 		CRM_CHECK(errno != EINVAL); */
	if(errno == EINVAL) {
	    crm_err("Conversion of %s failed", text);
	    result = -1;
	    
	} else if(errno == ERANGE) {
	    crm_err("Conversion of %s was clipped: %lld", text, result);

	} else if(errno != 0) {
	    crm_perror(LOG_ERR,"Conversion of %s failed:", text);
	}
			
	if(local_end_text != NULL && local_end_text[0] != '\0') {
	    crm_err("Characters left over after parsing '%s': '%s'", text, local_end_text);
	}
    }
    return result;
}

int
crm_parse_int(const char *text, const char *default_text)
{
	int atoi_result = -1;
	if(text != NULL) {
		atoi_result = crm_int_helper(text, NULL);
		if(errno == 0) {
			return atoi_result;
		}
	}
	
	if(default_text != NULL) {
		atoi_result = crm_int_helper(default_text, NULL);
		if(errno == 0) {
			return atoi_result;
		}

	} else {
		crm_err("No default conversion value supplied");
	}

	return -1;
}

gboolean
safe_str_neq(const char *a, const char *b)
{
	if(a == b) {
		return FALSE;

	} else if(a==NULL || b==NULL) {
		return TRUE;

	} else if(strcasecmp(a, b) == 0) {
		return FALSE;
	}
	return TRUE;
}

char *
crm_strdup_fn(const char *src, const char *file, const char *fn, int line)
{
	char *dup = NULL;
	CRM_CHECK(src != NULL, return NULL);
	crm_malloc0(dup, strlen(src) + 1);
	return strcpy(dup, src);
}



#define ENV_PREFIX "HA_"
void
crm_set_env_options(void) 
{
	cl_inherit_logging_environment(500);
	cl_log_set_logd_channel_source(NULL, NULL);

	if(debug_level > 0 && (debug_level+LOG_INFO) > (int)crm_log_level) {
	    set_crm_log_level(LOG_INFO + debug_level);
	}
}

gboolean
crm_is_true(const char * s)
{
	gboolean ret = FALSE;
	if(s != NULL) {
		crm_str_to_boolean(s, &ret);
	}
	return ret;
}

int
crm_str_to_boolean(const char * s, int * ret)
{
	if(s == NULL) {
		return -1;

	} else if (strcasecmp(s, "true") == 0
		   ||	strcasecmp(s, "on") == 0
		   ||	strcasecmp(s, "yes") == 0
		   ||	strcasecmp(s, "y") == 0
		   ||	strcasecmp(s, "1") == 0){
		*ret = TRUE;
		return 1;

	} else if (strcasecmp(s, "false") == 0
		   ||	strcasecmp(s, "off") == 0
		   ||	strcasecmp(s, "no") == 0
		   ||	strcasecmp(s, "n") == 0
		   ||	strcasecmp(s, "0") == 0){
		*ret = FALSE;
		return 1;
	}
	return -1;
}

#ifndef NUMCHARS
#    define	NUMCHARS	"0123456789."
#endif

#ifndef WHITESPACE
#    define	WHITESPACE	" \t\n\r\f"
#endif

unsigned long long
crm_get_interval(const char * input)
{
    ha_time_t *interval = NULL;
    char *input_copy = crm_strdup(input);
    char *input_copy_mutable = input_copy;
    unsigned long long msec = 0;
    
    if(input == NULL) {
	return 0;

    } else if(input[0] != 'P') {
	crm_free(input_copy);
	return crm_get_msec(input);
    }
    
    interval = parse_time_duration(&input_copy_mutable);
    msec = date_in_seconds(interval);
    free_ha_date(interval);
    crm_free(input_copy);
    return msec * 1000;
}

long long
crm_get_msec(const char * input)
{
	const char *cp = input;
	const char *units;
	long long multiplier = 1000;
	long long divisor = 1;
	long long msec = -1;
	char *end_text = NULL;
	/* double dret; */

	if(input == NULL) {
		return msec;
	}
	
	cp += strspn(cp, WHITESPACE);
	units = cp + strspn(cp, NUMCHARS);
	units += strspn(units, WHITESPACE);

	if (strchr(NUMCHARS, *cp) == NULL) {
		return msec;
	}

	if (strncasecmp(units, "ms", 2) == 0
	    || strncasecmp(units, "msec", 4) == 0) {
		multiplier = 1;
		divisor = 1;
	} else if (strncasecmp(units, "us", 2) == 0
		  || strncasecmp(units, "usec", 4) == 0) {
		multiplier = 1;
		divisor = 1000;
	} else if (strncasecmp(units, "s", 1) == 0
		  || strncasecmp(units, "sec", 3) == 0) {
		multiplier = 1000;
		divisor = 1;
	} else if (strncasecmp(units, "m", 1) == 0
		  || strncasecmp(units, "min", 3) == 0) {
		multiplier = 60*1000;
		divisor = 1;
	} else if (strncasecmp(units, "h", 1) == 0
		  || strncasecmp(units, "hr", 2) == 0) {
		multiplier = 60*60*1000;
		divisor = 1;
	} else if (*units != EOS && *units != '\n' && *units != '\r') {
		return msec;
	}
	
	msec = crm_int_helper(cp, &end_text);
	msec *= multiplier;
	msec /= divisor;
	/* dret += 0.5; */
	/* msec = (long long)dret; */
	return msec;
}

const char *
op_status2text(op_status_t status)
{
	switch(status) {
		case LRM_OP_PENDING:
			return "pending";
			break;
		case LRM_OP_DONE:
			return "complete";
			break;
		case LRM_OP_ERROR:
			return "Error";
			break;
		case LRM_OP_TIMEOUT:
			return "Timed Out";
			break;
		case LRM_OP_NOTSUPPORTED:
			return "NOT SUPPORTED";
			break;
		case LRM_OP_CANCELLED:
			return "Cancelled";
			break;
	}
	crm_err("Unknown status: %d", status);
	return "UNKNOWN!";
}

char *
generate_op_key(const char *rsc_id, const char *op_type, int interval)
{
	int len = 35;
	char *op_id = NULL;

	CRM_CHECK(rsc_id  != NULL, return NULL);
	CRM_CHECK(op_type != NULL, return NULL);
	
	len += strlen(op_type);
	len += strlen(rsc_id);
	crm_malloc0(op_id, len);
	CRM_CHECK(op_id != NULL, return NULL);
	sprintf(op_id, "%s_%s_%d", rsc_id, op_type, interval);
	return op_id;
}

gboolean
parse_op_key(const char *key, char **rsc_id, char **op_type, int *interval)
{
	char *notify = NULL;
	char *mutable_key = NULL;
	char *mutable_key_ptr = NULL;
	int len = 0, offset = 0, ch = 0;

	CRM_CHECK(key != NULL, return FALSE);
	
	*interval = 0;
	len = strlen(key);
	offset = len-1;

	crm_debug_3("Source: %s", key);
	
	while(offset > 0 && isdigit(key[offset])) {
		int digits = len-offset;
		ch = key[offset] - '0';
		CRM_CHECK(ch < 10, return FALSE);
		CRM_CHECK(ch >= 0, return FALSE);
		while(digits > 1) {
			digits--;
			ch = ch * 10;
		}
		*interval +=  ch;
		offset--;
	}

	crm_debug_3("  Interval: %d", *interval);
	CRM_CHECK(key[offset] == '_', return FALSE);

	mutable_key = crm_strdup(key);
	mutable_key_ptr = mutable_key_ptr;
	mutable_key[offset] = 0;
	offset--;

	while(offset > 0 && key[offset] != '_') {
		offset--;
	}

	CRM_CHECK(key[offset] == '_',
		  crm_free(mutable_key); return FALSE);

	mutable_key_ptr = mutable_key+offset+1;

	crm_debug_3("  Action: %s", mutable_key_ptr);
	
	*op_type = crm_strdup(mutable_key_ptr);

	mutable_key[offset] = 0;
	offset--;

	CRM_CHECK(mutable_key != mutable_key_ptr,
		  crm_free(mutable_key); return FALSE);

	notify = strstr(mutable_key, "_post_notify");
	if(safe_str_eq(notify, "_post_notify")) {
	    notify[0] = 0;
	}

	notify = strstr(mutable_key, "_pre_notify");
	if(safe_str_eq(notify, "_pre_notify")) {
	    notify[0] = 0;
	}

	crm_debug_3("  Resource: %s", mutable_key);
	*rsc_id = mutable_key;

	return TRUE;
}

char *
generate_notify_key(const char *rsc_id, const char *notify_type, const char *op_type)
{
	int len = 12;
	char *op_id = NULL;

	CRM_CHECK(rsc_id  != NULL, return NULL);
	CRM_CHECK(op_type != NULL, return NULL);
	CRM_CHECK(notify_type != NULL, return NULL);
	
	len += strlen(op_type);
	len += strlen(rsc_id);
	len += strlen(notify_type);
	crm_malloc0(op_id, len);
	if(op_id != NULL) {
		sprintf(op_id, "%s_%s_notify_%s_0", rsc_id, notify_type, op_type);
	}
	return op_id;
}

char *
generate_transition_magic_v202(const char *transition_key, int op_status)
{
	int len = 80;
	char *fail_state = NULL;

	CRM_CHECK(transition_key != NULL, return NULL);
	
	len += strlen(transition_key);
	
	crm_malloc0(fail_state, len);
	if(fail_state != NULL) {
		snprintf(fail_state, len, "%d:%s", op_status,transition_key);
	}
	return fail_state;
}

char *
generate_transition_magic(const char *transition_key, int op_status, int op_rc)
{
	int len = 80;
	char *fail_state = NULL;

	CRM_CHECK(transition_key != NULL, return NULL);
	
	len += strlen(transition_key);
	
	crm_malloc0(fail_state, len);
	if(fail_state != NULL) {
		snprintf(fail_state, len, "%d:%d;%s",
			 op_status, op_rc, transition_key);
	}
	return fail_state;
}

gboolean
decode_transition_magic(
	const char *magic, char **uuid, int *transition_id, int *action_id,
	int *op_status, int *op_rc, int *target_rc)
{
    int res = 0;
    char *key = NULL;
    gboolean result = TRUE;

    CRM_CHECK(magic != NULL, return FALSE);
    CRM_CHECK(op_rc != NULL, return FALSE);
    CRM_CHECK(op_status != NULL, return FALSE);
    
    crm_malloc0(key, strlen(magic));
    res = sscanf(magic, "%d:%d;%s", op_status, op_rc, key);
    if(res != 3) {
	crm_crit("Only found %d items in: %s", res, magic);
	result = FALSE;
	goto bail;
    }
    
    CRM_CHECK(decode_transition_key(key, uuid, transition_id, action_id, target_rc),
	      result = FALSE;
	      goto bail;
	);
    
  bail:
    crm_free(key);
    return result;
}

char *
generate_transition_key(int transition_id, int action_id, int target_rc, const char *node)
{
	int len = 40;
	char *fail_state = NULL;

	CRM_CHECK(node != NULL, return NULL);
	
	len += strlen(node);
	
	crm_malloc0(fail_state, len);
	if(fail_state != NULL) {
		snprintf(fail_state, len, "%d:%d:%d:%s",
			 action_id, transition_id, target_rc, node);
	}
	return fail_state;
}


gboolean
decode_transition_key(
	const char *key, char **uuid, int *transition_id, int *action_id, int *target_rc)
{
	int res = 0;
	gboolean done = FALSE;

	CRM_CHECK(uuid != NULL, return FALSE);
	CRM_CHECK(target_rc != NULL, return FALSE);
	CRM_CHECK(action_id != NULL, return FALSE);
	CRM_CHECK(transition_id != NULL, return FALSE);
	
	crm_malloc0(*uuid, strlen(key));
	res = sscanf(key, "%d:%d:%d:%s", action_id, transition_id, target_rc, *uuid);
	switch(res) {
	    case 4:
		/* Post Pacemaker 0.6 */
		done = TRUE;
		break;
	    case 3:
	    case 2:
		/* this can be tricky - the UUID might start with an integer */

		/* Until Pacemaker 0.6 */
		done = TRUE;
		*target_rc = -1;
		res = sscanf(key, "%d:%d:%s", action_id, transition_id, *uuid);
		if(res == 2) {
		    *action_id = -1;
		    res = sscanf(key, "%d:%s", transition_id, *uuid);
		    CRM_CHECK(res == 2, done = FALSE);

		} else if(res != 3) {
		    CRM_CHECK(res == 3, done = FALSE);
		}
		break;
		
	    case 1:
		/* Prior to Heartbeat 2.0.8 */
		done = TRUE;
		*action_id = -1;
		*target_rc = -1;
		res = sscanf(key, "%d:%s", transition_id, *uuid);
		CRM_CHECK(res == 2, done = FALSE);
		break;
	    default:
		crm_crit("Unhandled sscanf result (%d) for %s", res, key);
		
	}

	if(strlen(*uuid) != 36) {
	    crm_warn("Bad UUID (%s) in sscanf result (%d) for %s", *uuid, res, key);		    
	}
	
	if(done == FALSE) {
	    crm_err("Cannot decode '%s' rc=%d", key, res);
	    
	    crm_free(*uuid);
	    *uuid = NULL;
	    *target_rc = -1;
	    *action_id = -1;
	    *transition_id = -1;
	}
	
	return done;
}

void
filter_action_parameters(xmlNode *param_set, const char *version) 
{
	char *key = NULL;
	char *timeout = NULL;
	char *interval = NULL;
#if CRM_DEPRECATED_SINCE_2_0_5
	const char *filter_205[] = {
		XML_ATTR_TE_TARGET_RC,
		XML_ATTR_LRM_PROBE,
		XML_RSC_ATTR_START,
		XML_RSC_ATTR_NOTIFY,
		XML_RSC_ATTR_UNIQUE,
		XML_RSC_ATTR_MANAGED,
		XML_RSC_ATTR_PRIORITY,
		XML_RSC_ATTR_MULTIPLE,
		XML_RSC_ATTR_STICKINESS,
		XML_RSC_ATTR_FAIL_STICKINESS,
		XML_RSC_ATTR_TARGET_ROLE,

/* ignore clone fields */
		XML_RSC_ATTR_INCARNATION, 
		XML_RSC_ATTR_INCARNATION_MAX, 
		XML_RSC_ATTR_INCARNATION_NODEMAX,
		XML_RSC_ATTR_MASTER_MAX,
		XML_RSC_ATTR_MASTER_NODEMAX,
		
/* old field names */
		"role",
		"crm_role",
		"te-target-rc",
		
/* ignore notify fields */
 		"notify_stop_resource",
 		"notify_stop_uname",
 		"notify_start_resource",
 		"notify_start_uname",
 		"notify_active_resource",
 		"notify_active_uname",
 		"notify_inactive_resource",
 		"notify_inactive_uname",
 		"notify_promote_resource",
 		"notify_promote_uname",
 		"notify_demote_resource",
 		"notify_demote_uname",
 		"notify_master_resource",
 		"notify_master_uname",
 		"notify_slave_resource",
 		"notify_slave_uname"		
	};
#endif
	
	const char *attr_filter[] = {
		XML_ATTR_ID,
		XML_ATTR_CRM_VERSION,
		XML_LRM_ATTR_OP_DIGEST,
	};

	gboolean do_delete = FALSE;
	int lpc = 0;
	static int meta_len = 0;
	if(meta_len == 0) {
		meta_len  = strlen(CRM_META);
	}	
	
	if(param_set == NULL) {
		return;
	}

#if CRM_DEPRECATED_SINCE_2_0_5
 	if(version == NULL || compare_version("1.0.5", version) > 0) {
		for(lpc = 0; lpc < DIMOF(filter_205); lpc++) {
			xml_remove_prop(param_set, filter_205[lpc]); 
		}
	}
#endif

	for(lpc = 0; lpc < DIMOF(attr_filter); lpc++) {
		xml_remove_prop(param_set, attr_filter[lpc]); 
	}

	key = crm_meta_name(XML_LRM_ATTR_INTERVAL);
	interval = crm_element_value_copy(param_set, key);
	crm_free(key);

	key = crm_meta_name(XML_ATTR_TIMEOUT);
	timeout = crm_element_value_copy(param_set, key);
	
	xml_prop_iter(param_set, prop_name, prop_value,      
		      do_delete = FALSE;
		      if(strncasecmp(prop_name, CRM_META, meta_len) == 0) {
			      do_delete = TRUE;
		      }

		      if(do_delete) {
			      xml_remove_prop(param_set, prop_name);
		      }
		);

	if(crm_get_msec(interval) > 0 && compare_version(version, "1.0.8") > 0) {
		/* Re-instate the operation's timeout value */
		if(timeout != NULL) {
			crm_xml_add(param_set, key, timeout);
		}
	}

	crm_free(interval);
	crm_free(timeout);
	crm_free(key);
}

void
filter_reload_parameters(xmlNode *param_set, const char *restart_string) 
{
	int len = 0;
	char *name = NULL;
	char *match = NULL;
	
	if(param_set == NULL) {
		return;
	}

	xml_prop_iter(param_set, prop_name, prop_value,      
		      name = NULL;
		      len = strlen(prop_name) + 3;

		      crm_malloc0(name, len);
		      sprintf(name, " %s ", prop_name);
		      name[len-1] = 0;
		      
		      match = strstr(restart_string, name);
		      if(match == NULL) {
			      crm_debug_3("%s not found in %s",
					  prop_name, restart_string);
			      xml_remove_prop(param_set, prop_name);
		      }
		      crm_free(name);
		);
}

void
crm_abort(const char *file, const char *function, int line,
	  const char *assert_condition, gboolean do_core, gboolean do_fork)
{
	int rc = 0;
	int pid = 0;
	int status = 0;

	if(do_core == FALSE) {
	    do_crm_log(LOG_ERR, "%s: Triggered assert at %s:%d : %s",
		       function, file, line, assert_condition);
	    return;

	} else if(do_fork) {
	    pid=fork();

	} else {
	    do_crm_log(LOG_ERR, "%s: Triggered fatal assert at %s:%d : %s",
		       function, file, line, assert_condition);
	}
	
	switch(pid) {
		case -1:
			do_crm_log(LOG_CRIT, "%s: Cannot create core for non-fatal assert at %s:%d : %s",
				   function, file, line, assert_condition);
			return;

		default:	/* Parent */
			do_crm_log(LOG_ERR, 
				   "%s: Forked child %d to record non-fatal assert at %s:%d : %s",
				   function, pid, file, line, assert_condition);
			do {
			    rc = waitpid(pid, &status, 0);
			    if(rc < 0 && errno != EINTR) {
				crm_perror(LOG_ERR,"%s: Cannot wait on forked child %d", function, pid);
			    }
			    
			} while(rc < 0 && errno == EINTR);
			    
			return;

		case 0:	/* Child */
			abort();
			break;
	}
}

char *
generate_series_filename(
	const char *directory, const char *series, int sequence, gboolean bzip)
{
	int len = 40;
	char *filename = NULL;
	const char *ext = "raw";

	CRM_CHECK(directory  != NULL, return NULL);
	CRM_CHECK(series != NULL, return NULL);
	
	len += strlen(directory);
	len += strlen(series);
	crm_malloc0(filename, len);
	CRM_CHECK(filename != NULL, return NULL);

	if(bzip) {
		ext = "bz2";
	}
	sprintf(filename, "%s/%s-%d.%s", directory, series, sequence, ext);
	
	return filename;
}

int
get_last_sequence(const char *directory, const char *series)
{
	FILE *file_strm = NULL;
	int start = 0, length = 0, read_len = 0;
	char *series_file = NULL;
	char *buffer = NULL;
	int seq = 0;
	int len = 36;

	CRM_CHECK(directory  != NULL, return 0);
	CRM_CHECK(series != NULL, return 0);
	
	len += strlen(directory);
	len += strlen(series);
	crm_malloc0(series_file, len);
	CRM_CHECK(series_file != NULL, return 0);
	sprintf(series_file, "%s/%s.last", directory, series);
	
	file_strm = fopen(series_file, "r");
	if(file_strm == NULL) {
		crm_debug("Series file %s does not exist", series_file);
		crm_free(series_file);
		return 0;
	}
	
	/* see how big the file is */
	start  = ftell(file_strm);
	fseek(file_strm, 0L, SEEK_END);
	length = ftell(file_strm);
	fseek(file_strm, 0L, start);
	
	CRM_ASSERT(start == ftell(file_strm));

	crm_debug_3("Reading %d bytes from file", length);
	crm_malloc0(buffer, (length+1));
	read_len = fread(buffer, 1, length, file_strm);

	if(read_len != length) {
		crm_err("Calculated and read bytes differ: %d vs. %d",
			length, read_len);
		crm_free(buffer);
		buffer = NULL;
		
	} else  if(length <= 0) {
		crm_info("%s was not valid", series_file);
		crm_free(buffer);
		buffer = NULL;
	}
	
	crm_free(series_file);
	seq = crm_parse_int(buffer, "0");
	crm_free(buffer);
	fclose(file_strm);
	return seq;
}

void
write_last_sequence(
	const char *directory, const char *series, int sequence, int max)
{
	int rc = 0;
	int len = 36;
	FILE *file_strm = NULL;
	char *series_file = NULL;

	CRM_CHECK(directory  != NULL, return);
	CRM_CHECK(series != NULL, return);

	if(max == 0) {
		return;
	}
	while(max > 0 && sequence > max) {
		sequence -= max;
	}
	
	len += strlen(directory);
	len += strlen(series);
	crm_malloc0(series_file, len);
	sprintf(series_file, "%s/%s.last", directory, series);
	
	file_strm = fopen(series_file, "w");
	if(file_strm == NULL) {
		crm_err("Cannout open series file %s for writing", series_file);
		goto bail;
	}

	rc = fprintf(file_strm, "%d", sequence);
	if(rc < 0) {
		crm_perror(LOG_ERR,"Cannot write to series file %s", series_file);
	}

  bail:
	if(file_strm != NULL) {
		fflush(file_strm);
		fclose(file_strm);
	}
	
	crm_free(series_file);
}

#define	LOCKSTRLEN	11

int crm_pid_active(long pid)
{
    if(pid <= 0) {
	return -1;

    } else if (kill(pid, 0) < 0 && errno == ESRCH) {
	return 0;
    }

#ifndef HAVE_PROC_PID
    return 1;
#else
    {
    int rc = 0;
    int running = 0;
    char proc_path[PATH_MAX], exe_path[PATH_MAX], myexe_path[PATH_MAX];
	
    /* check to make sure pid hasn't been reused by another process */
    snprintf(proc_path, sizeof(proc_path), "/proc/%lu/exe", pid);
	
    rc = readlink(proc_path, exe_path, PATH_MAX-1);
    if(rc < 0) {
	crm_perror(LOG_ERR, "Could not read from %s", proc_path);
	goto bail;
    }

    exe_path[rc] = 0;
    snprintf(proc_path, sizeof(proc_path), "/proc/%lu/exe", (long unsigned int)getpid());
    rc = readlink(proc_path, myexe_path, PATH_MAX-1);
    if(rc < 0) {
	crm_perror(LOG_ERR, "Could not read from %s", proc_path);
	goto bail;
    }
    
    myexe_path[rc] = 0;
    if(strcmp(exe_path, myexe_path) == 0) {
	running = 1;
    }
    }
    
  bail:
    return running;
#endif
}


int
crm_read_pidfile(const char *filename)
{
    int fd;
    long pid = -1;
    char  buf[LOCKSTRLEN+1];
    if ((fd = open(filename, O_RDONLY)) < 0) {
	goto bail;
    }
    
    if (read(fd, buf, sizeof(buf)) < 1) {
	goto bail;
    } 
    
    if (sscanf(buf, "%lu", &pid) > 0) {
	if (pid <= 0){
	    pid = -LSB_STATUS_STOPPED;
	}
    }
    
  bail:
    close(fd);
    return pid;
}

int
crm_lock_pidfile(const char *filename)
{
    struct stat sbuf;
    int fd = 0, rc = 0;
    long pid = 0, mypid = 0;
    char lf_name[256], tf_name[256], buf[LOCKSTRLEN+1];

    mypid = (unsigned long) getpid();
    snprintf(lf_name, sizeof(lf_name), "%s",filename);
    snprintf(tf_name, sizeof(tf_name), "%s.%lu", filename, mypid);
	
    if ((fd = open(lf_name, O_RDONLY)) >= 0) {
	if (fstat(fd, &sbuf) >= 0 && sbuf.st_size < LOCKSTRLEN) {
	    sleep(1); /* if someone was about to create one,
		       * give'm a sec to do so
		       * Though if they follow our protocol,
		       * this won't happen.  They should really
		       * put the pid in, then link, not the
		       * other way around.
		       */
	}
	if (read(fd, buf, sizeof(buf)) > 0) {
	    if (sscanf(buf, "%lu", &pid) > 0) {
		if (pid > 1 && pid != getpid() && crm_pid_active(pid)) {
		    /* locked by existing process - give up */
		    close(fd);
		    return -1;
		}
	    }
	}
	unlink(lf_name);
	close(fd);
    }
    
    if ((fd = open(tf_name, O_CREAT | O_WRONLY | O_EXCL, 0644)) < 0) {
	/* Hmmh, why did we fail? Anyway, nothing we can do about it */
	return -3;
    }

    /* Slight overkill with the %*d format ;-) */
    snprintf(buf, sizeof(buf), "%*lu\n", LOCKSTRLEN-1, mypid);

    if (write(fd, buf, LOCKSTRLEN) != LOCKSTRLEN) {
	/* Again, nothing we can do about this */
	rc = -3;
	close(fd);
	goto out;
    }
    close(fd);

    switch (link(tf_name, lf_name)) {
	case 0:
	    if (stat(tf_name, &sbuf) < 0) {
		/* something weird happened */
		rc = -3;

	    } else if (sbuf.st_nlink < 2) {
		/* somehow, it didn't get through - NFS trouble? */
		rc = -2;

	    } else {
		rc = 0;
	    }
	    break;

	case EEXIST:
	    rc = -1;
	    break;

	default:
	    rc = -3;
    }
  out:
    unlink(tf_name);
    return rc;
}

void
crm_make_daemon(const char *name, gboolean daemonize, const char *pidfile)
{
    long pid;
    const char *devnull = "/dev/null";

    if(daemonize == FALSE) {
	return;
    }
	
    pid = fork();
    if (pid < 0) {
	fprintf(stderr, "%s: could not start daemon\n", name);
	crm_perror(LOG_ERR,"fork");
	exit(LSB_EXIT_GENERIC);

    } else if (pid > 0) {
	exit(LSB_EXIT_OK);
    }

    if (crm_lock_pidfile(pidfile) < 0 ) {
	pid = crm_read_pidfile(pidfile);
	if(crm_pid_active(pid) > 0) {
	    crm_warn("%s: already running [pid %ld] (%s).\n", name, pid, pidfile);
	    exit(LSB_EXIT_OK);
	}
    }
	
    umask(022);
    close(STDIN_FILENO);
    (void)open(devnull, O_RDONLY);		/* Stdin:  fd 0 */
    close(STDOUT_FILENO);
    (void)open(devnull, O_WRONLY);		/* Stdout: fd 1 */
    close(STDERR_FILENO);
    (void)open(devnull, O_WRONLY);		/* Stderr: fd 2 */
}

gboolean
crm_is_writable(const char *dir, const char *file,
		const char *user, const char *group, gboolean need_both)
{
	int s_res = -1;
	struct stat buf;
	char *full_file = NULL;
	const char *target = NULL;
	
	gboolean pass = TRUE;
	gboolean readwritable = FALSE;

	CRM_ASSERT(dir != NULL);
	if(file != NULL) {
		full_file = crm_concat(dir, file, '/');
		target = full_file;
		s_res = stat(full_file, &buf);
		if( s_res == 0 && S_ISREG(buf.st_mode) == FALSE ) {
			crm_err("%s must be a regular file", target);
			pass = FALSE;
			goto out;
		}
	}
	
	if (s_res != 0) {
		target = dir;
		s_res = stat(dir, &buf);
		if(s_res != 0) {
			crm_err("%s must exist and be a directory", dir);
			pass = FALSE;
			goto out;

		} else if( S_ISDIR(buf.st_mode) == FALSE ) {
			crm_err("%s must be a directory", dir);
			pass = FALSE;
		}
	}

	if(user) {
		struct passwd *sys_user = NULL;
		sys_user = getpwnam(user);
		readwritable = (sys_user != NULL
				&& buf.st_uid == sys_user->pw_uid
				&& (buf.st_mode & (S_IRUSR|S_IWUSR)));
		if(readwritable == FALSE) {
			crm_err("%s must be owned and r/w by user %s",
				target, user);
			if(need_both) {
				pass = FALSE;
			}
		}
	}	

	if(group) {
		struct group *sys_grp = getgrnam(group);
		readwritable = (
			sys_grp != NULL
			&& buf.st_gid == sys_grp->gr_gid
			&& (buf.st_mode & (S_IRGRP|S_IWGRP)));		
		if(readwritable == FALSE) {
			if(need_both || user == NULL) {
				pass = FALSE;
				crm_err("%s must be owned and r/w by group %s",
					target, group);
			} else {
				crm_warn("%s should be owned and r/w by group %s",
					 target, group);
			}
		}
	}

  out:
	crm_free(full_file);
	return pass;
}

static unsigned long long crm_bit_filter = 0; /* 0x00000002ULL; */
static unsigned int bit_log_level = LOG_DEBUG_5;

long long
crm_clear_bit(const char *function, long long word, long long bit)
{
	unsigned int level = bit_log_level;
	if(bit & crm_bit_filter) {
	    level = LOG_ERR;
	}

	do_crm_log_unlikely(level, "Bit 0x%.16llx cleared by %s", bit, function);
	word &= ~bit;

	return word;
}

long long
crm_set_bit(const char *function, long long word, long long bit)
{
	unsigned int level = bit_log_level;
	if(bit & crm_bit_filter) {
	    level = LOG_ERR;
	}

	do_crm_log_unlikely(level, "Bit 0x%.16llx set by %s", bit, function);
	word |= bit;
	return word;
}

static const char *cluster_type = NULL;

gboolean is_openais_cluster(void)
{
    if(cluster_type == NULL) {
	cluster_type = getenv("HA_cluster_type");
	if(cluster_type == NULL) {
	    cluster_type = "Heartbeat";
	}
    }
    
    if(safe_str_eq("openais", cluster_type)) {
#if SUPPORT_AIS
	return TRUE;
#else
	crm_crit("The installation of Pacemaker only supports Heartbeat"
		 " but you're trying to run it on %s.  Terminating.",
		 cluster_type);
	exit(100);
#endif
    }
    return FALSE;
}

gboolean is_heartbeat_cluster(void)
{
#if SUPPORT_HEARTBEAT
    return !is_openais_cluster();
#else
    if(is_openais_cluster() == FALSE) {
	crm_crit("The installation of Pacemaker only supports OpenAIS"
		 " but you're trying to run it on %s.  Terminating.",
		 cluster_type);
	exit(100);
    }
    return FALSE;
#endif
}

gboolean crm_str_eq(const char *a, const char *b, gboolean use_case) 
{
    if(a == b) {
	return TRUE;
	
    } else if(a == NULL || b == NULL) {
	/* shouldn't be comparing NULLs */
	return FALSE;
	    
    } else if(use_case && a[0] != b[0]) {
	return FALSE;		
	
    } else if(strcasecmp(a, b) == 0) {
	return TRUE;
    }
    return FALSE;
}

char *crm_meta_name(const char *field) 
{
    int lpc = 0;
    int max = 0;
    char *crm_name = NULL;

    CRM_CHECK(field != NULL, return NULL);
    crm_name = crm_concat(CRM_META, field, '_');

    /* Massage the names so they can be used as shell variables */ 
    max = strlen(crm_name);
    for(; lpc < max; lpc++) {
	switch(crm_name[lpc]) {
	    case '-':
		crm_name[lpc] = '_';
		break;
	}
    }
    return crm_name;
}

const char *crm_meta_value(GHashTable *hash, const char *field) 
{
    char *key = NULL;
    const char *value = NULL;

    key = crm_meta_name(field);
    if(key) {
	value = g_hash_table_lookup(hash, key);
	crm_free(key);
    }
    
    return value;
}

static struct crm_option *crm_long_options = NULL;
static const char *crm_app_description = NULL;
static const char *crm_short_options = NULL;
static const char *crm_app_usage = NULL;

static struct option *crm_create_long_opts(struct crm_option *long_options) 
{
    struct option *long_opts = NULL;

#ifdef HAVE_GETOPT_H
    int index = 0, lpc = 0;

    /*
     * A previous, possibly poor, choice of '?' as the short form of --help
     * means that getopt_long() returns '?' for both --help and for "unknown option"
     *
     * This dummy entry allows us to differentiate between the two in crm_get_option()
     * and exit with the correct error code
     */
    crm_realloc(long_opts, (index+1) * sizeof(struct option));
    long_opts[index].name = "__dummmy__";
    long_opts[index].has_arg = 0;
    long_opts[index].flag = 0;
    long_opts[index].val = '_';
    index++;
    
    for(lpc = 0; long_options[lpc].name != NULL; lpc++) {
	if(long_options[lpc].name[0] == '-') {
	    continue;
	}
	
	crm_realloc(long_opts, (index+1) * sizeof(struct option));
	/*fprintf(stderr, "Creating %d %s = %c\n", index,
	 * long_options[lpc].name, long_options[lpc].val);	*/
	long_opts[index].name = long_options[lpc].name;
	long_opts[index].has_arg = long_options[lpc].has_arg;
	long_opts[index].flag = long_options[lpc].flag;
	long_opts[index].val = long_options[lpc].val;
	index++;
    }

    /* Now create the list terminator */
    crm_realloc(long_opts, (index+1) * sizeof(struct option));
    long_opts[index].name = NULL;
    long_opts[index].has_arg = 0;
    long_opts[index].flag = 0;
    long_opts[index].val = 0;
#endif
    
    return long_opts;
}

void crm_set_options(const char *short_options, const char *app_usage, struct crm_option *long_options, const char *app_desc) 
{
    if(short_options) {
	crm_short_options = short_options;
    }
    if(long_options) {
	crm_long_options = long_options;
    }
    if(app_desc) {
	crm_app_description = app_desc;
    }
    if(app_usage) {
	crm_app_usage = app_usage;
    }
}

int crm_get_option(int argc, char **argv, int *index) 
{
#ifdef HAVE_GETOPT_H
    static struct option *long_opts = NULL;
    if(long_opts == NULL && crm_long_options) {
	long_opts = crm_create_long_opts(crm_long_options);
    }
    
    if(long_opts) {
	int flag = getopt_long(argc, argv, crm_short_options, long_opts, index);
	switch(flag) {
	    case 0: return long_opts[*index].val;
	    case -1:  /* End of option processing */ break;
	    case ':': crm_debug_2("Missing argument"); crm_help('?', 1); break;
	    case '?': crm_help('?', *index?0:1); break;
	}
	return flag;
    }
#endif

    if(crm_short_options) {
	return getopt(argc, argv, crm_short_options);
    }
    
    return -1;
}

void crm_help(char cmd, int exit_code) 
{
    int i = 0;
    FILE *stream = (exit_code ? stderr : stdout);

    if(cmd == 'v' || cmd == '$') {
	fprintf(stream, "%s %s for %s (Build: %s)\n",
		crm_system_name, VERSION,
		#if !SUPPORT_HEARTBEAT
		"OpenAIS",
		#elif !SUPPORT_AIS
		"Heartbeat",
		#else
		"OpenAIS and Heartbeat",
		#endif
		BUILD_VERSION);
	fprintf(stream, "\nWritten by Andrew Beekhof\n");
	goto out;
    }
    
    fprintf(stream, "%s - %s\n", crm_system_name, crm_app_description);

    if(crm_app_usage) {
	fprintf(stream, "Usage: %s %s\n", crm_system_name, crm_app_usage);
    }
	
    if(crm_long_options) {
	fprintf(stream, "Options:\n");
	for(i = 0; crm_long_options[i].name != NULL; i++) {
	    if(crm_long_options[i].flags & pcmk_option_hidden) {

	    } else if(crm_long_options[i].flags & pcmk_option_paragraph) {
		fprintf(stream, "%s\n\n", crm_long_options[i].desc);

	    } else if(crm_long_options[i].flags & pcmk_option_example) {
		fprintf(stream, "\t#%s\n\n", crm_long_options[i].desc);

	    } else if(crm_long_options[i].val == '-' && crm_long_options[i].desc) {
		fprintf(stream, "%s\n", crm_long_options[i].desc);
		
	    } else {
                /* is val printable as char ? */
                if(crm_long_options[i].val <= UCHAR_MAX) {
                    fprintf(stream, " -%c,", crm_long_options[i].val);
                } else {
                    fputs("    ", stream);
                }
		fprintf(stream, " --%s%c%s\t%s\n", crm_long_options[i].name,
			crm_long_options[i].has_arg?'=':' ',crm_long_options[i].has_arg?"value":"",
			crm_long_options[i].desc?crm_long_options[i].desc:"");
	    }
	}
	    
    } else if(crm_short_options) {
	fprintf(stream, "Usage: %s - %s\n", crm_system_name, crm_app_description);
	for(i = 0; crm_short_options[i] != 0; i++) {
	    int has_arg = FALSE;
		
	    if(crm_short_options[i+1] == ':') {
		has_arg = TRUE;
	    }
		
	    fprintf(stream, " -%c %s\n", crm_short_options[i], has_arg?"{value}":"");
	    if(has_arg) {
		i++;
	    }
	}
    }

    fprintf(stream, "\nReport bugs to %s\n", PACKAGE_BUGREPORT);

  out:
    if(exit_code >= 0) {
	exit(exit_code);
    }
}

#include <../../tools/attrd.h>
gboolean attrd_update(IPC_Channel *cluster, char command, const char *host, const char *name, const char *value, const char *section, const char *set, const char *dampen) 
{
    gboolean success = FALSE;
    const char *reason = "Cluster connection failed";

    /* remap common aliases */
    if(safe_str_eq(section, "reboot")) {
	section = XML_CIB_TAG_STATUS;
	
    } else if(safe_str_eq(section, "forever")) {
	section = XML_CIB_TAG_NODES;
    }
    
    if(cluster == NULL) {
	reason = "No connection to the cluster";

    } else {
	xmlNode *update = create_xml_node(NULL, __FUNCTION__);
	crm_xml_add(update, F_TYPE, T_ATTRD);
	crm_xml_add(update, F_ORIG, crm_system_name);

	if(name == NULL && command == 'U') {
	    command = 'R';
	}
	
	switch(command) {
	    case 'D':
	    case 'U':
	    case 'v':
		crm_xml_add(update, F_ATTRD_TASK, "update");
		crm_xml_add(update, F_ATTRD_ATTRIBUTE, name);
		break;
	    case 'R':
		crm_xml_add(update, F_ATTRD_TASK, "refresh");
		break;
	    case 'q':
		crm_xml_add(update, F_ATTRD_TASK, "query");
		break;
	}
	
	crm_xml_add(update, F_ATTRD_VALUE, value);
	crm_xml_add(update, F_ATTRD_DAMPEN, dampen);
	crm_xml_add(update, F_ATTRD_SECTION, section);
	crm_xml_add(update, F_ATTRD_HOST, host);
	crm_xml_add(update, F_ATTRD_SET, set);

	success = send_ipc_message(cluster, update);
	free_xml(update);
    }
 
    if(success) {
	crm_debug("Sent update: %s=%s for %s", name, value, host?host:"localhost");
	return TRUE;
    }

    crm_info("Could not send update: %s=%s for %s", name, value, host?host:"localhost");
    return FALSE;
}

gboolean attrd_lazy_update(char command, const char *host, const char *name, const char *value, const char *section, const char *set, const char *dampen) 
{
    int max = 5;
    gboolean updated = FALSE;
    static IPC_Channel *cluster = NULL;

    while(updated == 0 && max > 0) {
	if(cluster == NULL) {
	    crm_info("Connecting to cluster... %d retries remaining", max);
	    cluster = init_client_ipc_comms_nodispatch(T_ATTRD);
	}

	if(cluster != NULL) {
	    updated = attrd_update(cluster, command, host, name, value, section, set, dampen);
	}
	
	if(updated == 0) {
	    cluster = NULL;
	    sleep(2);
	    max--;
	}
    }

    return updated;
}

gboolean attrd_update_no_mainloop(int *connection, char command, const char *host, const char *name, const char *value, const char *section, const char *set, const char *dampen) 
{
    int max = 5;
    gboolean updated = FALSE;
    static IPC_Channel *cluster = NULL;

    if(connection && *connection == 0 && cluster) {
	crm_info("Forcing a new connection to the cluster");
	cluster = NULL;
    }
    
    while(updated == 0 && max > 0) {
	if(cluster == NULL) {
	    crm_info("Connecting to cluster... %d retries remaining", max);
	    cluster = init_client_ipc_comms_nodispatch(T_ATTRD);
	}

	if(connection) {
	    if(cluster != NULL) {
		*connection = cluster->ops->get_recv_select_fd(cluster);
	    } else {
		*connection = 0;
	    }
	}
	
	if(cluster != NULL) {
	    updated = attrd_update(cluster, command, host, name, value, section, set, dampen);
	}
	
	if(updated == 0) {
	    cluster = NULL;
	    sleep(2);
	    max--;
	}
    }
    return updated;
}
