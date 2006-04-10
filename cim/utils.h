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
#include <portability.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#include <glib.h>
#include <hb_api.h>
#include <clplumbing/cl_malloc.h>
#include <clplumbing/cl_uuid.h>
#include <clplumbing/cl_log.h>

#define MAXLEN	1024

/* memory routines */
#define cim_malloc   cl_malloc
#define cim_free     cl_free
#define cim_strdup   cl_strdup
#define cim_realloc  cl_realloc

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

/* container */
enum ContainerType { T_ARRAY, T_TABLE };
/* container and element type */
typedef struct {
		GHashTable * table;	/* must be the 1st */
		int type;
	} CIMTable;

typedef struct {
		GPtrArray * array;	/* must be the 1st */
		int type;
	} CIMArray;

enum DataType {	TYPEString, TYPEUint32, TYPEUint16, TYPEArray, TYPETable };

typedef struct cimdata_t_s {
	union {
		char * str;
		uint32_t uint32;
		CIMArray * array;
		CIMTable * table;
	} v;	/* this must be the first */
	int	type;
} cimdata_t;

#define makeStrData(s) ({						\
	cimdata_t * data = (cimdata_t*)cim_malloc(sizeof(cimdata_t));	\
	if(data) {							\
		data->type = TYPEString;				\
		data->v.str = cim_strdup(s);				\
	};								\
	data;								\
})

#define makeUint32Data(i) ({						\
	cimdata_t * data = (cimdata_t*)cim_malloc(sizeof(cimdata_t));	\
	if(data) {							\
		data->type = TYPEUint32;				\
		data->v.uint32 = i;					\
	};								\
	data;								\
})

#define makeArrayData(x) ({						\
	cimdata_t * data = (cimdata_t*)cim_malloc(sizeof(cimdata_t));	\
	if(data) {							\
		data->type = TYPEArray;					\
		data->v.array = x;					\
	};								\
	data;								\
})


#define makeTableData(x) ({						\
	cimdata_t * data = (cimdata_t*)cim_malloc(sizeof(cimdata_t));	\
	if(data) {							\
		data->type = TYPETable;					\
		data->v.table = x;					\
	};								\
	data;								\
})


/* table */
#define cim_table_new() ({						\
	CIMTable * t = (CIMTable*)cim_malloc(sizeof(CIMTable));		\
	if (t) {							\
		t->table = g_hash_table_new_full(g_str_hash, g_str_equal, \
					cim_free, cimdata_free);	\
		t->type = T_TABLE;					\
	}								\
	t;								\
})



#define cim_table_lookup(t,k)	((cimdata_t*)g_hash_table_lookup(t->table,k))
#define cim_table_lookup_v(t,k)	({					\
	cimdata_t zero_data = { {NULL}, 0 };				\
	cimdata_t * d = (cimdata_t*)g_hash_table_lookup(t->table,k);	\
	(d? (*d) : zero_data);						\
})

#define cim_table_foreach(x,a,b) g_hash_table_foreach(x->table, a,b)
#define cim_table_replace(t,k,v) g_hash_table_replace(t->table,k,v)
#define cim_table_insert(t,k,v)	 g_hash_table_insert(t->table,k,v)

/* array */
#define cim_array_new()		({				\
	CIMArray * a = (CIMArray*)cim_malloc(sizeof(CIMArray));	\
	if (a) {						\
		a->array = g_ptr_array_new();			\
		a->type = T_ARRAY;				\
	};							\
	a;							\
})

#define cim_array_append(a,p)	g_ptr_array_add(a->array,p)
#define cim_array_index(a,i)	g_ptr_array_index(a->array,i)
#define cim_array_index_v(a,i) ({				\
	cimdata_t zero_data = {{NULL}, 0};			\
	cimdata_t * d = (cimdata_t*)cim_array_index(a,i);	\
	(d?(*d) : zero_data );					\
})

#define cim_array_len(a) ((a && a->array)?a->array->len:0)

void	cim_table_free(void * table);
void	cim_array_free(void*);
void 	cimdata_free(void * data);
int	cim_table_strdup_replace(CIMTable*table, const char*key, const char*val);
void	dump_cim_table(CIMTable *table, const char *id);
int	cim_init_logger(const char* entity);
void	cim_assert(const char* assertion, int line, const char* file);
int	run_shell_cmnd(const char* cmnd,int* ret,char*** out,char***);
char**	regex_search(const char * reg, const char * str, int * len);
void	free_2d_array(void *array, int len, cim_free_t free);
void	free_2d_zarray(void *zarray, cim_free_t free);
char* 	uuid_to_str(const cl_uuid_t * uuid);
char**	split_string(const char *string, int *len, const char *delim);

#endif
