/*
 * Linux HA management common macros and functions
 *
 * Author: Huang Zhen <zhenhltc@cn.ibm.com>
 * Copyright (C) 2005 International Business Machines
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <portability.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mgmt/mgmt_common.h>

malloc_t 	malloc_f = NULL;
realloc_t 	realloc_f = NULL;
free_t 		free_f = NULL;

char*
mgmt_new_msg(const char* type, ...)
{
	va_list ap;
	int len;
	char* buf;
	
	/* count the total len of fields */	
	len = strnlen(type, MAX_STRLEN)+1;
	va_start(ap,type);
	while(1) {
		char* arg = va_arg(ap, char*);
		if (arg == NULL) {
			break;
		}
		len += strnlen(arg, MAX_STRLEN)+1;
	}
	va_end(ap);
	
	/* alloc memory */
	buf = (char*)mgmt_malloc(len+1);
	if (buf == NULL) {
		return NULL;
	}

	/* assign the first field */
	snprintf(buf,len,"%s", type);
	
	/* then the others */
	va_start(ap, type);
	while(1) {
		char* arg = va_arg(ap, char*);
		if (arg == NULL) {
			break;
		}
		strncat(buf, "\n", len);
		strncat(buf, arg, len);
	}
	va_end(ap);
	
	return buf;
}
char*
mgmt_msg_append(char* msg, const char* append)
{
	int msg_len;
	int append_len;
	int len;
	
	msg_len = strnlen(msg, MAX_MSGLEN);
	if (append != NULL) {
		append_len = strnlen(append, MAX_STRLEN);
		/* +2: one is the '\n', other is the end 0*/
		len = msg_len+append_len+2;
		msg = (char*)mgmt_realloc(msg, len);
		strncat(msg, "\n", len);
		strncat(msg, append, len);
	}
	else {
		/* +2: one is the '\n', other is the end 0*/
		len = msg_len+2;
		msg = (char*)mgmt_realloc(msg, len);
		strncat(msg, "\n", len);
	}
	return msg;
}
int
mgmt_result_ok(char* msg)
{
	int ret, num;
	char** args = mgmt_msg_args(msg, &num);
	if (args == NULL || num ==0) {
		ret = 0;
	}
	else if (STRNCMP_CONST(args[0], MSG_OK)!=0) {
		ret = 0;
	}
	else {
		ret = 1;
	}
	mgmt_del_args(args);
	return ret;
}

char**
mgmt_msg_args(const char* msg, int* num)
{
	char* p;
	char* buf;
	char** ret = NULL;
	int i,n;
	int len;
	
	if (msg == NULL) {
		return NULL;
	}
	
	/* alloc memory */
	len = strnlen(msg, MAX_MSGLEN);
	buf = (char*)mgmt_malloc(len+1);
	if (buf == NULL) {
		return NULL;
	}
	
	strncpy(buf, msg, len);
	buf[len] = 0;
	
	/* find out how many fields first */
	p = buf;
	n = 1;
	while(1) {
		p=strchr(p,'\n');
		if (p != NULL) {
			p++;
			n++;
		}
		else {
			break;
		}
	}

	/* malloc the array for args */
	ret = (char**)mgmt_malloc(sizeof(char*)*n);
	if (ret == NULL) {
		mgmt_free(p);
		return NULL;
	}

	/* splite the string to fields */
	ret[0] = buf;
	for (i = 1; i < n; i++) {
		ret[i] = strchr(ret[i-1],'\n');
		*ret[i] = 0;
		ret[i]++;
	}
	if (num != NULL) {
		*num = n;
	}
	return ret;
}

void
mgmt_del_msg(char* msg)
{
	if (msg != NULL) {
		mgmt_free(msg);
	}
}
void
mgmt_del_args(char** args)
{
	if (args != NULL) {
		if (args[0] != NULL) {
			mgmt_free(args[0]);
		}
		mgmt_free(args);
	}
}

void
mgmt_set_mem_funcs(malloc_t m, realloc_t r, free_t f)
{
	malloc_f = m;
	realloc_f = r;
	free_f = f;
}

void*
mgmt_malloc(size_t size)
{
	if (malloc_f == NULL) {
		return malloc(size);
	}
	return (*malloc_f)(size);
}

void*
mgmt_realloc(void* oldval, size_t newsize)
{
	if (realloc_f == NULL) {
		return realloc(oldval, newsize);
	}
	return (*realloc_f)(oldval, newsize);
}

void
mgmt_free(void *ptr)
{
	if (free_f == NULL) {
		free(ptr);
		return;
	}
	(*free_f)(ptr);
}
