#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>
#include <hb_api.h>
#include <errno.h>
#include <heartbeat.h>
#include "cluster_info.h"
#include "utils.h"

int debug_class = 0;

int 
cim_init_logger(const char * entity)
{
        char * inherit_debuglevel;
        int debug_level = 2;
 
	inherit_debuglevel = getenv(HADEBUGVAL);
	if (inherit_debuglevel != NULL) {
		debug_level = atoi(inherit_debuglevel);
		if (debug_level > 2) {
			debug_level = 2;
		}
	}

	cl_log_set_entity(entity);
	cl_log_enable_stderr(debug_level?TRUE:FALSE);
	cl_log_set_facility(LOG_DAEMON);
        return HA_OK;
}

int 
run_shell_cmnd(const char *cmnd, int *ret, char ***out, char ***err)
				/* err not used currently */
{
	FILE * fstream = NULL;
	char buffer [4096];
	int cmnd_rc, rc, i;

	DEBUG_ENTER();
	if ( (fstream = popen(cmnd, "r")) == NULL ){
		cl_log(LOG_ERR, "run_shell_cmnd: popen error: %s",
			strerror(errno));	
		return HA_FAIL;
	}

	if ( (*out = cim_malloc(sizeof(char*)) ) == NULL ) {
		cl_log(LOG_ERR, "run_shell_cmnd: failed malloc.");
                return HA_FAIL;
        }

	(*out)[0] = NULL;

	i = 0;
	while (!feof(fstream)) {
		if ( fgets(buffer, 4096, fstream) != NULL ){
			/** add buffer to out **/
			*out = cim_realloc(*out, (i+2) * sizeof(char*));	
                        if ( *out == NULL ) {
                                rc = HA_FAIL;
                                goto exit;
                        }
                        (*out)[i] = cim_strdup(buffer);
			(*out)[i+1] = NULL;		
		}else{
			continue;
		}
		i++;
	}
	
	rc = HA_OK;
exit:
	if ( (cmnd_rc = pclose(fstream)) == -1 ){
		/*** WARNING log ***/
                cl_log(LOG_WARNING, "failed to close pipe.");
	}
	*ret = cmnd_rc;

	DEBUG_LEAVE();
	return rc;
}

char **
regex_search(const char * reg, const char * str, int * len)
{
	regex_t regexp;
	const int maxmatch = 16;
	regmatch_t pm[16];
	int i, ret, nmatch = 0;
	char **match = NULL;

	DEBUG_ENTER();

	*len = 0;
	ret = regcomp(&regexp, reg, REG_EXTENDED);
	if ( ret != 0) {
		cl_log(LOG_ERR, "regex_search: error regcomp regex %s.", reg);
		return HA_FAIL;
	}

	ret = regexec(&regexp, str, nmatch, pm, 0);
	if ( ret == REG_NOMATCH ){
		regfree(&regexp);
		cl_log(LOG_ERR, "regex_search: no match.");
		return NULL;
	}else if (ret != 0){
        	cl_log(LOG_ERR, "regex_search: error regexec.\n");
		regfree(&regexp);
		return NULL;
	}

	for(nmatch=0; pm[nmatch++].rm_so != -1 && nmatch <= maxmatch; );
	cl_log(LOG_INFO, "%d, matched ", nmatch);

	if ( (match = cim_malloc(nmatch*sizeof(char *))) == NULL ) {
		cl_log(LOG_ERR, "regex_search: alloc_failed.");
		regfree(&regexp);
		return NULL;
	}
	
	*len = nmatch;
	for(i = 0; i < maxmatch && i < nmatch; i++){
		int str_len = pm[i].rm_eo - pm[i].rm_so;
		match[i] = cim_malloc(str_len + 1);
		strncpy( match[i], str + pm[i].rm_so, str_len);
		match[i][str_len] = EOS;
	}
	regfree(&regexp);
	DEBUG_LEAVE();
	return match;
} 

void
free_2d_zarray(void *zarray, cim_free_t free)
{
	void ** z = (void **)zarray;
	int i = 0;
	void * p;

	while ((p=z[i++])) {
		free(p);
	}
	cim_free(z);
}

void
free_2d_array(void * a, int len, cim_free_t free)
{
	int 	i;
	void ** array = (void **)a;

	for (i=0; i<len; i++){
		if (array[i]) {
			free(array[i]);
		}
	}
	cim_free(array);
}

char * 
uuid_to_str(const cl_uuid_t * uuid){
        int i, len = 0;
        char * str = cim_malloc(256);

        if ( str == NULL ) {
                return NULL;
        }

        memset(str, 0, 256);

        for ( i = 0; i < sizeof(cl_uuid_t); i++){
                len += snprintf(str + len, 2, "%.2X", uuid->uuid[i]);
        }
        return str;
}

void
cim_assert(const char * assertion, int line, const char * file)
{
        cl_log(LOG_ERR, "Assertion \"%s\" failed on line %d in file \"%s\""
        ,       assertion, line, file);
        exit(1);
}

static void
dump_foreach(void * key, void * value, void * user)
{
	cimdata_t * d = (cimdata_t *) value;
	if ( d == NULL ) {
		return;
	}
	if ( d->type == TYPEString) {
		cl_log(LOG_INFO, "DUMP: %s: %s", (char *)key, d->v.str);
	} else if (d->type == TYPETable) {
		cl_log(LOG_INFO, "DUMP: %s: <table>", (char *)key);
	} else if ( d->type == TYPEArray ) {
		cl_log(LOG_INFO, "DUMP: %s: <array>", (char *)key);
	}
}

void
dump_cim_table(CIMTable * table, const char * id)
{
	cl_log(LOG_INFO, "--- Begin dump %s ---", id? id : "<NULL>");
	cim_table_foreach(table, dump_foreach, NULL);
	cl_log(LOG_INFO, "--- End dump %s ---", id? id : "<NULL>");

}

char **
split_string(const char* string, int *len, const char *delim)
{
	char **strings = NULL;
	const char *p;

	*len = 0;
	while(*string){
		char * token = NULL;
		/* eat up delim chars */
		while (*string && strchr(delim, *string)) {
			string++;
			continue;
		}	
		if(!*string) {
			break;
		}
		
		/* reach a delim char */
		p = string;
		while ( *p && !strchr(delim, *p) ) {
			p ++;
			continue;
		}
		
		/* copy string~(p-1) to token */
		token = cim_malloc(p-string+1);
		if ( token == NULL ) {
			return strings;
		}
		memcpy(token, string, p - string); 
		token[p-string] = EOS;

		strings = cim_realloc(strings, (*len+1) * sizeof(char *));
		if ( strings == NULL ) {
			return NULL;
		}
		strings[*len] = token;
		*len = *len + 1;
		string = p;
	}
	return strings;	
}

