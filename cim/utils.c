/*
 * utils.c: utilities
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

#include <crm_internal.h>
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

int debug_level = 10;

static char * 	   	pathname_encode(const char *);

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
	cl_log_set_facility(HA_LOG_FACILITY);
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

	for(nmatch=0; pm[nmatch].rm_so != -1 && nmatch < maxmatch; nmatch++);

	if ( (match = cim_malloc(nmatch*sizeof(char *))) == NULL ) {
		cl_log(LOG_ERR, "regex_search: alloc_failed.");
		regfree(&regexp);
		return NULL;
	}
	
	*len = nmatch;
	for(i = 0; i < maxmatch && i < nmatch; i++){
		int str_len = pm[i].rm_eo - pm[i].rm_so;
		if ((match[i] = cim_malloc(str_len + 1))) {
			strncpy( match[i], str + pm[i].rm_so, str_len);
			match[i][str_len] = EOS;
		}
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

int 
cim_msg2disk(const char *objpathname, struct ha_msg *msg)
{
	struct stat st;
	FILE *fobj; 
	char pathname[MAXLEN], *buf;
	char *msgstr = NULL;

	if ( stat(HA_CIM_VARDIR, &st) < 0 ) {
		if ( errno == ENOENT ) {	/* not exist, create */
			if (  mkdir(HA_CIM_VARDIR, 0660) < 0){
				cl_log(LOG_ERR,"msg2disk: mkdir failed.");		
				cl_log(LOG_ERR,"reason:%s",strerror(errno)); 
				return HA_FAIL;
			}

		} else {
			cl_log(LOG_ERR, "msg2disk: stat faild.");
			cl_log(LOG_ERR,"reason:%d(%s)",errno,strerror(errno)); 
			return HA_FAIL;
		}
	} 

	/* check stat */

	/* write msg*/
	if((buf = pathname_encode(objpathname))== NULL ) {
		return HA_FAIL;
	}
	snprintf(pathname, MAXLEN, "%s/%s", HA_CIM_VARDIR, buf);
	cim_free(buf);

        if ( ( fobj = fopen(pathname, "w") ) == NULL ) {
		cl_log(LOG_WARNING, "msg2disk: can't open file.");
		cl_log(LOG_WARNING, "reason:%d(%s)", errno, strerror(errno));
		return HA_FAIL;
	}
	if ( msg->nfields == 0 ) {
		fclose(fobj);
		return HA_OK;
	}

	if ((msgstr = msg2string(msg)) == NULL ) {
		cl_log(LOG_ERR, "cim_msg2disk: msgstr NULL.");
		return HA_FAIL;
	}

	fprintf(fobj, "%s", msgstr);
	fclose(fobj); 
	return HA_OK;
}

struct ha_msg*
cim_disk2msg(const char *objpathname)
{
	char pathname[MAXLEN], *buf;
	FILE *fobj = NULL;
	int ret;
	int bytes = 0;
	struct stat st;
	struct ha_msg *msg = NULL;

	if((buf = pathname_encode(objpathname))== NULL ) {
		return NULL;
	}
	snprintf(pathname, MAXLEN, "%s/%s", HA_CIM_VARDIR, buf);
	cim_free(buf);

	if ( ( ret = stat(pathname, &st)) < 0 ) {
		cl_log(LOG_WARNING, "disk2msg: stat faild for %s.", pathname);
		cl_log(LOG_WARNING,"reason:%d(%s)",errno,strerror(errno)); 
		return NULL;
	} 

	if (st.st_size == 0 ) {
		cl_log(LOG_WARNING, "disk2msg: size of %s is zero.", objpathname);
		return NULL;
	}

	if ((buf = cim_malloc(st.st_size)) == NULL ) {
		cl_log(LOG_ERR, "disk2msg: alloc msg failed for %s.", objpathname);
		return NULL;
	}
	
	if ( (fobj = fopen(pathname, "r")) == NULL ) {
		cl_log(LOG_WARNING, "msg2disk: can't open file %s.", pathname);
		cl_log(LOG_WARNING, "reason:%d(%s)", errno, strerror(errno));
		return NULL;
	}

	while ( (ret = fread(buf, st.st_size, 1, fobj))){
		bytes += ret*st.st_size;
	}
	
	if ( !feof(fobj) ) {
		cl_log(LOG_ERR, "msg2disk: read error for %s.", objpathname);
		cl_log(LOG_ERR, "reason: %d(%s).", errno, strerror(errno));		
		cim_free(msg);
		return NULL;
	}

	if ( bytes != st.st_size ) {
		cl_log(LOG_ERR, "msg2disk: incompete read:");
		cl_log(LOG_ERR, "read: %d vs size: %d.", bytes, (int)st.st_size);
		cim_free(msg);
			return NULL;
	}
	msg = string2msg(buf, bytes);
	cim_free(buf);
	fclose(fobj);
	return msg;
}


int
cim_disk_msg_del(const char *objpathname)
{
	char fullpathname[MAXLEN];
	char * pathname = pathname_encode(objpathname);
	snprintf(fullpathname, MAXLEN, "%s/%s", HA_CIM_VARDIR, pathname);
	cim_debug2(LOG_INFO, "%s: unlink %s", __FUNCTION__, fullpathname);
	unlink(fullpathname);
	cim_free(pathname);
	return HA_OK;
}

	
char*
cim_dbget(const char *pathname, const char*key)
{
	struct ha_msg *db = cim_disk2msg(pathname);
	const char * value;

	if ( db == NULL ) {
		return NULL;
	}
	
	value = cl_get_string(db, key);
	if ( value == NULL || strncmp(value, "null", MAXLEN) == 0) {
		return NULL;
	}
	return cim_strdup(value);
}

int
cim_dbput(const char *pathname, const char*key, const char*value)
{
	int ret;
	struct ha_msg* db = cim_disk2msg(pathname);
	if ( db == NULL ) {
		if ( (db = ha_msg_new(1)) == NULL ) {
			cl_log(LOG_ERR, "cim_dbput: alloc db failed.");
			return HA_FAIL;
		}
	}

	if ((cl_msg_modstring(db, key, value?value:"null")) != HA_OK ) {
		ha_msg_del(db);
		cl_log(LOG_ERR, "cim_dbput: put value failed.");
		return HA_FAIL;
	}

	ret = cim_msg2disk(pathname, db);
	ha_msg_del(db);
	return HA_OK;	
}

int
cim_dbdel(const char *pathname, const char*key)
{
	int ret;
	struct ha_msg* db = cim_disk2msg(pathname);
	if ( db == NULL ) {
		cl_log(LOG_ERR, "cim_dbdel: get db failed.");
		return HA_FAIL;
	}

	if ((cl_msg_remove(db, key)) != HA_OK ) {
		ha_msg_del(db);
		cl_log(LOG_ERR, "cim_dbdel: remove failed.");
		return HA_FAIL;
	}

	ret = cim_msg2disk(pathname, db);
	ha_msg_del(db);
	return HA_OK;	
}

struct ha_msg* 
cim_dbkeys(const char *pathname)
{
	struct ha_msg * db = cim_disk2msg(pathname);
	struct ha_msg * list;
	int i;

	if ( db == NULL ) {
		cl_log(LOG_ERR, "cim_dbkeys: get db failed.");
		return NULL;
	}
	
	if ( (list = ha_msg_new(1)) == NULL ) {
		ha_msg_del(db);
		cl_log(LOG_ERR, "cim_dbkeys: alloc list failed.");
		return NULL;
	}
	
	for (i = 0; i < db->nfields; i++) {
		cim_list_add(list, db->names[i]);
	}

	ha_msg_del(db);
	return list;
}

static char*   
pathname_encode(const char *pathname)
{
	char *new_pathname = NULL;
	char ch, *p;
	if ((new_pathname = cim_malloc(strlen(pathname)+1)) == NULL ) {
		cl_log(LOG_ERR, "pathname_enocde: alloc pathname failed.");
		return NULL;
	}
	p = new_pathname;
	while( (ch = *(pathname++)) ) {
		if (ch == '\\' || ch == '/')  {
			*(p++) = '_';
		} else {
			*(p++) = ch;
		}
	}
	*p = EOS;
	return new_pathname;
}


/****************************************************
 * msg
 ****************************************************/
int
cim_msg_children_count(struct ha_msg *parent)
{
	int i, count = 0;
	for (i = 0; i < parent->nfields; i++) {
		if ( parent->types[i] == FT_STRUCT ) {
			count++;
		}
	}
	return count;
}


const char *
cim_msg_child_name(struct ha_msg *parent, int index)
{	
	int i, current = 0;
	for (i = 0; i < parent->nfields; i++) {
		if ( parent->types[i] == FT_STRUCT ) {
			if ( index == current) {
				return parent->names[i];
			}
			current++;
		}
	}
	return NULL;
}


struct ha_msg * 
cim_msg_child_index(struct ha_msg *parent, int index)
{
	int i, current = 0;
	for (i = 0; i < parent->nfields; i++) {
		if ( parent->types[i] == FT_STRUCT ) {
			if ( index == current) {
				return parent->values[i];
			}
			current++;
		}
	}
	return NULL;
}


int
cim_list_find(struct ha_msg *list, const char *value)
{
	int len = cim_list_length(list);
	int i = 0;
	for (i = 0; i<len; i++) {
		char * v = cim_list_index(list, i);
		if ( v && (strncmp(v, value, MAXLEN) == 0)) {
			return TRUE;
		}
	}
	return FALSE;
}
