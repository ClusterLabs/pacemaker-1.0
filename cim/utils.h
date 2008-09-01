/*
 * utils.h: utilities header
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


#ifndef _UTILS_H
#define _UTILS_H
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <hb_api.h>
#include <clplumbing/cl_uuid.h>
#include <clplumbing/cl_log.h>

#define MAXLEN	1024

/* memory routines */
#define cim_malloc   malloc
#define cim_free     free
#define cim_strdup   strdup
#define cim_realloc  realloc

typedef     	void (* cim_free_t)(void *);

/* for debuging */
#define DEBUG_ENTER() cl_log(LOG_INFO, "%s: --- ENTER ---", __FUNCTION__)
#define DEBUG_LEAVE() cl_log(LOG_INFO, "%s: --- LEAVE ---", __FUNCTION__)

#ifndef ASSERT
#ifdef HAVE_STRINGIZE
#       define  ASSERT(X)    {if(!(X)) cim_assert(#X, __LINE__, __FILE__);}
#else
#       define  ASSERT(X)    {if(!(X)) cim_assert("X", __LINE__, __FILE__);}
#endif
#endif

extern int debug_level;
#define cim_debug2(prio, fmt...)	\
	do {					\
		if(debug_level > 2){		\
			cl_log(prio, ##fmt);	\
		}				\
	}while(0)

#define PROVIDER_INIT_LOGGER()  cim_init_logger(PROVIDER_ID)
#define cim_debug_msg(msg, fmt...) 		\
	do {					\
		cl_log(LOG_INFO, ##fmt);	\
		cl_log(LOG_INFO, "%s", msg2string(msg));\
	}while (0)


int	cim_init_logger(const char* entity);
void	cim_assert(const char* assertion, int line, const char* file);
int	run_shell_cmnd(const char* cmnd,int* ret,char*** out,char***);
char**	regex_search(const char * reg, const char * str, int * len);
void	free_2d_array(void *array, int len, cim_free_t free);
void	free_2d_zarray(void *zarray, cim_free_t free);
char* 	uuid_to_str(const cl_uuid_t * uuid);
char**	split_string(const char *string, int *len, const char *delim);


#define cim_dbget_msg(pathname, key) ({			\
	struct ha_msg *msg = NULL;			\
	char *value = cim_dbget(pathname, key);		\
	if (value) {					\
		msg = string2msg(value, strlen(value));	\
		cim_free(value);			\
	}						\
	msg;						\
})

#define cim_dbput_msg(pathname,key,msg) \
	cim_dbput(pathname, key, msg?msg2string(msg):NULL)

struct ha_msg* 	cim_disk2msg(const char *objpathname);
int		cim_msg2disk(const char *objpathname, struct ha_msg *);
int		cim_disk_msg_del(const char *objpathname);

char*	cim_dbget(const char *pathname, const char*key);
int	cim_dbput(const char *pathname, const char*key, const char*value);
int	cim_dbdel(const char *pathname, const char*key);
struct ha_msg* cim_dbkeys(const char *pathname);	


int		cim_list_find(struct ha_msg *list, const char *value);
#define cim_list_length(msg) 		cl_msg_list_length(msg, CIM_MSG_LIST)
#define cim_list_index(msg,index) 					\
	((char *)cl_msg_list_nth_data(msg, CIM_MSG_LIST, index))

#define cim_list_add(msg, value)					\
	cl_msg_list_add_string(msg, CIM_MSG_LIST, value)		

#define cim_msg_add_child(parent,id, child)			\
		ha_msg_addstruct(parent, id, child)
#define cim_msg_find_child(parent, id)		cl_get_struct(parent,id)
#define cim_msg_remove_child(parent, id)	cl_msg_remove(parent,id)

int		cim_msg_children_count(struct ha_msg *parent);
const char *	cim_msg_child_name(struct ha_msg * parent, int index);
struct ha_msg * cim_msg_child_index(struct ha_msg *parent, int index);


#endif
