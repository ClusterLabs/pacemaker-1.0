/*
 * heartbeat: Linux-HA uuid code
 *
 * Copyright (C) 2004 Guochun Shi <gshi@ncsa.uiuc.edu>
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

#include <config.h>
#include <uuid/uuid.h>
#include <heartbeat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_SYNC
#	define O_SYNC 0
#endif

GHashTable*			name_table = NULL;
GHashTable*			uuid_table = NULL;

static guint 
uuid_hash(gconstpointer key)
{
	const char *p = key;
	const char *pmax = p + sizeof(uuid_t);
	guint h = *p;
	
	if (h){
		for (p += 1; p < pmax; p++){
			h = (h << 5) - h + *p;
		}
	}
	
	return h;
}

static gint
uuid_equal(gconstpointer v, gconstpointer v2)
{
	return (uuid_compare(v, v2) == 0 );
}

#if 0

static void
print_key_value(gpointer key, gpointer value, gpointer user_data)
{
	struct node_info* hip = (struct node_info*)value;
	
	cl_log(LOG_INFO, "key=%s, value=%s", (char*)key, 
	       uuid_is_null(hip->uuid)?"null":"not null");
}

static void
printout(void){
	cl_log(LOG_INFO, " printing out name table:");
	g_hash_table_foreach(name_table, 
			     print_key_value, NULL);	
		cl_log(LOG_INFO, " printing out uuidname table:");
		g_hash_table_foreach(uuid_table, 
				     print_key_value, NULL);	
	
}

#endif

static struct node_info*
lookup_nametable(const char* nodename)
{
	return (struct node_info*)g_hash_table_lookup(name_table, nodename);
}

static struct node_info*
lookup_uuidtable(const char* uuid)
{
	return (struct node_info*)g_hash_table_lookup(uuid_table, uuid);
}

struct node_info*
lookup_tables(const char* nodename, const char* uuid)
{
	
	struct node_info* hip = NULL;
	
	if(!nodename){
		cl_log(LOG_ERR,"lookup_tables: bad parameters");
	}
	
	/*printout();*/

	if(uuid){
		hip = lookup_uuidtable(uuid);
	}
	
	if(!hip){
		hip = lookup_nametable(nodename);
	}
	
	return hip;
	
}

void
update_tables(const char* nodename, const char* uuid)
{

	struct node_info*  hip ;

	if(!uuid || uuid_is_null(uuid)){
		cl_log(LOG_ERR,"update_tables: bad parameters");	
		return;
	}
	
	hip =  (struct node_info*) lookup_uuidtable(uuid);
	if (hip != NULL){
		if (strncmp(hip->nodename, nodename, sizeof(hip->nodename))){
			cl_log(LOG_WARNING, "nodename %s"
			       " changed to %s?", hip->nodename, nodename);	
			strncpy(hip->nodename, nodename, sizeof(hip->nodename));
			add_nametable(nodename, (char*)hip);
		}
		
	} else {
		hip = (struct node_info*) lookup_nametable(nodename);
		if(!hip){
			cl_log(LOG_WARNING,  "node %s not found in table", 
			       nodename);
			return;
		}
		
		if (uuid_is_null(hip->uuid) || !uuid_compare(uuid, hip->uuid)){
			uuid_copy(hip->uuid, uuid);
		}
		
		add_uuidtable(uuid, (char*)hip);
		
	}
	
	
	


}



void
add_nametable(const char* nodename, char* value)
{
	char * ds = ha_strdup(nodename);
	g_hash_table_insert(name_table, ds, value);
}

void
add_uuidtable(const char* uuid, char* value)
{
	char* du = ha_malloc(sizeof(uuid_t));
	uuid_copy(du, uuid);
	
	g_hash_table_insert(uuid_table, du, value);
}

int
inittable(void)

{
	if( uuid_table || name_table){
		cleanuptable();
	}

	uuid_table = g_hash_table_new(uuid_hash, uuid_equal);	
	if (!uuid_table){
		cl_log(LOG_ERR, "ghash table allocation error");
		return HA_FAIL;
	}
	
	name_table = g_hash_table_new(g_str_hash, g_str_equal);
	
	if (!name_table){
		cl_log(LOG_ERR, "ghash table allocation error");
		return HA_FAIL;
	}	

	return HA_OK;
}


static void
free_key(gpointer key, gpointer value, gpointer userdata)
{
	
	if(key){
		g_free(key);
	}
}


void
cleanuptable(void){
	
	g_hash_table_foreach(name_table, free_key, NULL);
	g_hash_table_destroy(name_table);
	name_table  = NULL;
	
	g_hash_table_foreach(uuid_table, free_key, NULL);
	g_hash_table_destroy(uuid_table);
	uuid_table  = NULL;
}


const char* 
uuid2nodename(const uuid_t uuid)
{
	struct node_info* hip;

	hip = g_hash_table_lookup(uuid_table, uuid);
	
	if (hip){
		return hip->nodename;
	} else{
		return NULL;
	}
}


const char *
nodename2uuid(const char* nodename)
{
	struct node_info* hip; 

	hip = g_hash_table_lookup(name_table, nodename);
	
	if (hip){
		if( uuid_is_null(hip->uuid)){
			return NULL;
		}

		return hip->uuid;
		
	} else {
		return NULL;
	}
}


#ifndef HB_UUID_FILE
#define HB_UUID_FILE VAR_LIB_D "/hb_uuid"
#endif

int
GetUUID(uuid_t uuid)
{
	int		fd;
	int		flags = 0;
	int		uuid_len = sizeof(uuid_t);

	if ((fd = open(HB_UUID_FILE, O_RDONLY)) > 0
	    &&	read(fd, uuid, uuid_len) == uuid_len) {
		close(fd);
		return HA_OK;
	}
	
	cl_log(LOG_INFO, "No uuid found - generating an uuid");
	flags = O_CREAT;
	
	if ((fd = open(HB_UUID_FILE, O_WRONLY|O_SYNC|flags, 0644)) < 0) {
		return HA_FAIL;
	}
	
	uuid_generate(uuid);
	
	if (write(fd, uuid, uuid_len) != uuid_len) {
		close(fd);
		return HA_FAIL;
	}
	
	/*
	 * Some UNIXes don't implement O_SYNC.
	 * So we do an fsync here for good measure.  It can't hurt ;-)
	 */
	
	if (fsync(fd) < 0) {
		cl_perror("fsync failure on " HB_UUID_FILE);
		return HA_FAIL;
	}
	if (close(fd) < 0) {
		cl_perror("close failure on " HB_UUID_FILE);
		return HA_FAIL;
	}

	return HA_OK;
}

