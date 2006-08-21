/*
 * ha_tsa_common.c: common functions 
 *
 * Author: Jia Ming Pan <jmltc@cn.ibm.com>
 * Copyright (c) 2006 International Business Machines
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

#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_pidfile.h>
#include <clplumbing/cl_signal.h>
#include <clplumbing/coredumps.h>
#include <ha_msg.h>
#include "ha_tsa_common.h"


void 
init_logger(const char * entity)
{
        int debug_level = 2;
	cl_log_set_entity(entity);
	cl_log_enable_stderr(debug_level?TRUE:FALSE);
	cl_log_set_facility(LOG_DAEMON);
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
		token = cl_malloc(p-string+1);
		if ( token == NULL ) {
			return strings;
		}
		memcpy(token, string, p - string); 
		token[p-string] = EOS;

		strings = cl_realloc(strings, (*len+1) * sizeof(char *));
		if ( strings == NULL ) {
			return NULL;
		}
		strings[*len] = token;
		*len = *len + 1;
		string = p;
	}
	return strings;	
}

void
free_array(void** array, int len)
{
	int i;
	for (i=0; i<len; i++){
		if (array[i]) {
			cl_free(array[i]);
		}
	}
	cl_free(array);
}


char*
run_shell_cmnd(const char *cmnd, int *rc, int *len)
{
	char * buffer = NULL;
	FILE * fstream = NULL;
	int offset = 0;

	*len = 0;
	if ( (fstream = popen(cmnd, "r")) == NULL ){
		cl_log(LOG_ERR, "run_shell_cmnd: popen error: %s",
			strerror(errno));	
		return NULL;
	}

	offset = 0;
	while (!feof(fstream)) {
		int bytes = 0;
		if ( (buffer = cl_realloc(buffer, 1024) ) == NULL ) {
			cl_log(LOG_ERR, "run_shell_cmnd: malloc failed.");
			*len = 0;
			goto out;
		}

		*len = *len + 1024;

		while ( bytes < 1024 ) { 
			size_t rbytes =  fread(buffer + offset, 1, 1024 - bytes, fstream);
			if ( rbytes == 0 ) { goto out; }
			offset += rbytes;
		}
	}
out:	
	buffer[offset] = '\0';
	if ( (*rc = pclose(fstream)) == -1 ){
		/*** WARNING log ***/
                cl_log(LOG_WARNING, "failed to close pipe.");
	}
	return buffer;
}


