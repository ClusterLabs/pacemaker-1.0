/* File: stonithd_msg.c
 * Description: Common HA message handling library to STONITHD subsytem.
 *
 * Author: Sun Jiang Dong <sunjd@cn.ibm.com>
 * Copyright (c) 2004 International Business Machines
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <crm_internal.h>
#include <glib.h>
#include <ha_msg.h>
#include <clplumbing/cl_log.h>
#include <fencing/stonithd_msg.h>

static const gboolean DEBUG_MODE = FALSE;
/* Internally used function to free the string item of a hash table */
static void stdmsg_log(int priority, const char * fmt, ...)G_GNUC_PRINTF(2,3);
static void free_str_key(gpointer data);
static void free_str_val(gpointer data);

/* Temporarily just handle string hashtable correctly  */
int
ha_msg_addhash(struct ha_msg * msg, const char * name, GHashTable * htable)
{
	struct ha_msg * msg_tmp = NULL;

	if (msg == NULL || htable == NULL ) {
		stdmsg_log(LOG_ERR, "ha_msg_addhash: NULL parameter pointers.");
		return HA_FAIL;
	}

	if ((msg_tmp = hashtable_to_hamsg(htable)) == NULL ) {
		stdmsg_log(LOG_ERR, "hashtable_to_hamsg failed.");
		return HA_FAIL;
	}
	
	if ( ha_msg_addstruct(msg, name, msg_tmp) != HA_OK ) {
		stdmsg_log(LOG_ERR, "ha_msg_addhash: ha_msg_addstruct failed.");
		ZAPMSG(msg_tmp);
		return HA_FAIL;
	}
	
	ZAPMSG(msg_tmp);
	return HA_OK;
}

/* Temporarily just handle string hashtable correctly  */
struct ha_msg *
hashtable_to_hamsg(GHashTable * htable)
{
	struct ha_msg * msg_tmp = NULL;

	if (htable == NULL) {
		return NULL;
	}

	msg_tmp = ha_msg_new(0);
	g_hash_table_foreach(htable, insert_data_pairs, msg_tmp);
	return msg_tmp;
}

void
insert_data_pairs(gpointer key, gpointer value, gpointer user_data)
{
	struct ha_msg * msg_tmp = (struct ha_msg *) user_data;
	
	if ( ha_msg_add(msg_tmp, (const char *)key, (const char *)value) 
		!= HA_OK ) {
		stdmsg_log(LOG_ERR, "insert_data_pairs: ha_msg_add failed.");
	}
}
/* Now just handle string hash table correctly */
GHashTable *
cl_get_hashtable(struct ha_msg * msg, const char * name)
{
	struct ha_msg * tmp_msg = NULL;
	GHashTable * htable = NULL;
	int i;

	if (msg==NULL || name==NULL) {
		stdmsg_log(LOG_ERR, "cl_get_hashtable: parameter error.");
		return NULL;
	}

	if ((tmp_msg = cl_get_struct(msg, name)) == NULL) {
		stdmsg_log(LOG_ERR, "cl_get_hashtable: get NULL field.");
		return NULL;
	}

	htable = g_hash_table_new_full(g_str_hash, g_str_equal,
					free_str_key, free_str_val);

	for (i = 0; i < tmp_msg->nfields; i++) {
		if( FT_STRING != tmp_msg->types[i] ) {
			stdmsg_log(LOG_ERR, "cl_get_hashtable: "
					"field data type error.");
			continue;
		}
		g_hash_table_insert(htable,
			    g_strndup(tmp_msg->names[i], tmp_msg->nlens[i]),
			    g_strndup(tmp_msg->values[i], tmp_msg->vlens[i]));
		stdmsg_log(LOG_DEBUG, "cl_get_hashtable: field[%d]: "
			"name=%s, value=%s", i, tmp_msg->names[i],
			(const char *)tmp_msg->values[i]);
	}

	stdmsg_log(LOG_DEBUG, "cl_get_hashtable: table's address=%p", htable);
	return htable;
}

static void
free_str_key(gpointer data)
{
	g_free((gchar *)data);
}

static void
free_str_val(gpointer data)
{
	g_free((gchar *)data);
}

void
print_str_hashtable(GHashTable * htable)
{
	if (htable == NULL) {
		stdmsg_log(LOG_DEBUG, "printf_str_hashtable: htable==NULL");
		return;
	}

	g_hash_table_foreach(htable, print_str_item, NULL);
}

void
print_str_item(gpointer key, gpointer value, gpointer user_data)
{
	stdmsg_log(LOG_DEBUG, "key=%s, value=%s", 
		     (const char *)key,(const char *)value);
}
/* copied from stdmsg_log.c, need to be the same */
#ifndef MAXLINE
#	define MAXLINE	512
#endif
static void
stdmsg_log(int priority, const char * fmt, ...)
{
	va_list		ap;
	char		buf[MAXLINE];

	if ( DEBUG_MODE == FALSE && priority == LOG_DEBUG ) {
		return;
	}
	
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf)-1, fmt, ap);
	va_end(ap);
	cl_log(priority, "%s", buf);
}
