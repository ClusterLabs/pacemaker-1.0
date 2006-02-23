/*
 * heartbeat: Linux-HA uuid code
 *
 * Copyright (C) 2004 Guochun Shi <gshi@ncsa.uiuc.edu>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <portability.h>
#include <config.h>
#include <clplumbing/cl_uuid.h>
#include <heartbeat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <clplumbing/cl_misc.h>

extern int			DoManageResources;

#ifndef O_SYNC
#	define O_SYNC 0
#endif

static GHashTable*		name_table = NULL;
static GHashTable*		uuid_table = NULL;
static gboolean			nodecache_read_yet = FALSE;
static gboolean			delcache_read_yet = FALSE;
extern GList*			del_node_list;

static int			read_node_uuid_file(struct sys_config * cfg);
static int			read_delnode_file(struct sys_config* cfg);


static void  remove_all(void);

guint 
uuid_hash(gconstpointer key)
{
	const char *p = key;
	const char *pmax = p + sizeof(cl_uuid_t);
	guint h = *p;
	
	if (h){
		for (p += 1; p < pmax; p++){
			h = (h << 5) - h + *p;
		}
	}
	
	return h;
}

static int
string_hash(const char* key)
{
	const char *p = key;
	const char *pmax = p + strlen(key);
	guint h = *p;
	
	if (h){
		for (p += 1; p < pmax; p++){
			h = (h << 5) - h + *p;
		}
	}
	
	return h;
}

gint
uuid_equal(gconstpointer v, gconstpointer v2)
{
	return (cl_uuid_compare(v, v2) == 0 );
}

#if 0

static void
print_key_value(gpointer key, gpointer value, gpointer user_data)
{
	struct node_info* hip = (struct node_info*)value;
	
	cl_log(LOG_INFO, "key=%s, value=%s", (char*)key, 
	       uuid_is_null(&hip->uuid)?"null":"not null");
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


static void
uuidtable_entry_display( gpointer key, gpointer value, gpointer userdata)
{
	cl_uuid_t* uuid =(cl_uuid_t*) key;
	struct node_info* node= (struct node_info*)value;
	char	tmpstr[UU_UNPARSE_SIZEOF];
	
	memset(tmpstr , 0, UU_UNPARSE_SIZEOF);
	cl_uuid_unparse(uuid, tmpstr);

	cl_log(LOG_DEBUG, "uuid=%s, name=%s",
	       tmpstr, node->nodename);
	
}



static void
uuidtable_display(void)
{
	cl_log(LOG_DEBUG,"displaying uuid table");
	g_hash_table_foreach(uuid_table, uuidtable_entry_display,NULL);
	
	return;
}

static void
nametable_display(void)
{
	
	return;
}

static struct node_info*
lookup_nametable(const char* nodename)
{
	return (struct node_info*)g_hash_table_lookup(name_table, nodename);
}

static struct node_info*
lookup_uuidtable(cl_uuid_t* uuid)
{
	return (struct node_info*)g_hash_table_lookup(uuid_table, uuid);
}

struct node_info*
lookup_tables(const char* nodename, cl_uuid_t* uuid)
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

/*return value indicates whether tables are changed*/
gboolean
update_tables(const char* nodename, cl_uuid_t* uuid)
{

	struct node_info*  hip ;
	if (uuid == NULL){
		cl_log(LOG_ERR, "%s: NULL uuid pointer",
		       __FUNCTION__);
		return FALSE;
	}

	if(cl_uuid_is_null(uuid)){
		   return FALSE;
	}
	
	hip =  (struct node_info*) lookup_uuidtable(uuid);
	if (hip != NULL){
		if (strncmp(hip->nodename, 
			    nodename, sizeof(hip->nodename)) ==0){
			return FALSE;
		}

		cl_log(LOG_WARNING, "nodename %s uuid changed to %s"
		,	hip->nodename, nodename);	
		uuidtable_display();
		strncpy(hip->nodename, nodename, sizeof(hip->nodename));
		add_nametable(nodename, hip);
		return TRUE;
		
	} 

	hip = (struct node_info*) lookup_nametable(nodename);
	if(!hip){
		cl_log(LOG_WARNING,  "node %s not found in table", 
		       nodename);
		return FALSE;
	}
	
	if (cl_uuid_is_null(&hip->uuid)){
		cl_uuid_copy(&hip->uuid, uuid);
	}else if (cl_uuid_compare(&hip->uuid, uuid) != 0){
		cl_log(LOG_ERR, "node %s changed its uuid", nodename);	     
		nametable_display();
	}		
	add_uuidtable(uuid, hip);
	
	return TRUE;
}





int
tables_remove(const char* nodename, cl_uuid_t* uuid)
{
	int i;

	remove_all();

	for (i = 0; i< config->nodecount; i++){
		add_nametable(config->nodes[i].nodename, &config->nodes[i]);
		add_uuidtable(&config->nodes[i].uuid, &config->nodes[i]);
	}

	return HA_OK;
	
}

void
add_nametable(const char* nodename, struct node_info* value)
{
	char * ds = ha_strdup(nodename);
	g_hash_table_insert(name_table, ds, value);
}

void
add_uuidtable(cl_uuid_t* uuid, struct node_info* value)
{
	cl_uuid_t* du ;

	if (cl_uuid_is_null(uuid)){
		return;
	}

	du = (cl_uuid_t*)ha_malloc(sizeof(cl_uuid_t));
	cl_uuid_copy(du, uuid);
	
	g_hash_table_insert(uuid_table, du, value);
	
}

static void
free_data(gpointer data)
{
	if (data){
		g_free(data);
	}
	
}

int
inittable(void)

{
	if( uuid_table || name_table){
		cleanuptable();
	}

	uuid_table = g_hash_table_new_full(uuid_hash, uuid_equal, free_data, NULL);	
	if (!uuid_table){
		cl_log(LOG_ERR, "ghash table allocation error");
		return HA_FAIL;
	}
	
	name_table = g_hash_table_new_full(g_str_hash, g_str_equal, free_data, NULL);
	
	if (!name_table){
		cl_log(LOG_ERR, "ghash table allocation error");
		return HA_FAIL;
	}	

	return HA_OK;
}

static gboolean
always_true(gpointer key, gpointer value, gpointer userdata)
{
	return 1;
}

static void
remove_all(void)
{
	g_hash_table_foreach_remove(name_table, always_true, NULL);
	g_hash_table_foreach_remove(uuid_table, always_true, NULL);
}

void
cleanuptable(void){
	
	g_hash_table_destroy(name_table);
	name_table  = NULL;
	
	g_hash_table_destroy(uuid_table);
	uuid_table  = NULL;
}


const char* 
uuid2nodename(cl_uuid_t* uuid)
{
	struct node_info* hip;

	hip = g_hash_table_lookup(uuid_table, uuid);
	
	if (hip){
		return hip->nodename;
	} else{
		return NULL;
	}
}


int
nodename2uuid(const char* nodename, cl_uuid_t* id)
{
	struct node_info* hip; 
	
	if (nodename == NULL){
		cl_log(LOG_ERR, "nodename2uuid:"
		       "nodename is NULL ");
		return HA_FAIL;
	}
	cl_uuid_clear(id);
	hip = g_hash_table_lookup(name_table, nodename);
	
	if (!hip){		
		return HA_FAIL;
	}
	
	cl_uuid_copy(id, &hip->uuid);		
	
	return HA_OK;
}


static int
gen_uuid_from_name(const char* nodename, cl_uuid_t* uu)
{
	int		seed;
	int		value;
	int		loops[]={8,4,4, 4, 12};
	char		buf[UU_UNPARSE_SIZEOF];
	char		*p = buf;
	int		i;
	int		j;

	
	seed = string_hash(nodename);
	cl_log(LOG_INFO, "seed is %d", seed);
	srand(seed);
	
	for(i = 0; i < 5; i++){
		for (j = 0; j < loops[i]; j++){
			value = rand();
			p +=sprintf(p, "%01x", value%16);
		}
		if (i != 4){
			p += sprintf(p,"-");
		}
	}
	
	if (cl_uuid_parse(buf, uu) < 0){
		cl_log(LOG_INFO, "cl_uuid_parse failed");
		return HA_FAIL;
	}
	
	return HA_OK;
}


#ifndef HB_UUID_FILE
#define HB_UUID_FILE VAR_LIB_D "/hb_uuid"
#endif

int
GetUUID(struct sys_config* cfg, const char* nodename, cl_uuid_t* uuid)
{
	int		fd;
	int		flags = 0;
	int		uuid_len = sizeof(uuid->uuid);
	
	if (cfg->uuidfromname){
		return gen_uuid_from_name(nodename, uuid);
	}
	
	if ((fd = open(HB_UUID_FILE, O_RDONLY)) > 0
	    &&	read(fd, uuid->uuid, uuid_len) == uuid_len) {
		close(fd);
		return HA_OK;
	}
	
	cl_log(LOG_INFO, "No uuid found for current node"
	" - generating a new uuid.");
	flags = O_CREAT;
	
	if ((fd = open(HB_UUID_FILE, O_WRONLY|O_SYNC|flags, 0644)) < 0) {
		return HA_FAIL;
	}
	
	cl_uuid_generate(uuid);
	
	if (write(fd, uuid->uuid, uuid_len) != uuid_len) {
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


/*
 * Functions for writing out our current node/uuid configuration to a file
 * as nodes are added/deleted to the configuration and for reading it back
 * in at startup.
 */

static int
node_uuid_file_out(FILE *f, const char * nodename, const cl_uuid_t * uu)
{
	char	uuid_str[UU_UNPARSE_SIZEOF];
	cl_uuid_unparse(uu, uuid_str);
	if (fprintf(f, "%s\t%s\n", nodename, uuid_str) > sizeof(uuid_str)) {
		return HA_OK;
	}
	return HA_FAIL;
}

static int	/* Returns -, 0 + *; 0 = EOF, + = OK, - = ERROR */
node_uuid_file_in(FILE *f, char*  nodename, cl_uuid_t * uu)
{
	char	linebuf[MAXLINE];
	char *	tab;
	int	len;
	int	hlen;

	if (fgets(linebuf, MAXLINE, f) == NULL) {
		if (feof(f)) {
			return 0;
		}
		cl_perror("Cannot read line from node/uuid file");
		return -1;
	}
	len = strlen(linebuf);
	if (len < UU_UNPARSE_SIZEOF+2) {
		cl_log(LOG_ERR, "Malformed (short) node/uuid line [%s] (1)"
		,	linebuf);
		return -1;
	}
	len -=1;	/* fgets leaves '\n' on end of line */
	if (linebuf[len] != '\n') {
		cl_log(LOG_ERR, "Malformed (long) node/uuid line [%s] (2)"
		,	linebuf);
		return -1;
	}
	linebuf[len] = EOS;
	tab = strchr(linebuf, '\t');
	if (tab == NULL || (hlen=(tab - linebuf)) > (HOSTLENG-1) || hlen < 1){
		cl_log(LOG_ERR, "Malformed node/uuid line [%s] (3)", linebuf);
		return -1;
	}
	if ((len - hlen) != UU_UNPARSE_SIZEOF) {
		cl_log(LOG_ERR, "Malformed node/uuid line [%s] (4)", linebuf);
		return -1;
	}
	if (cl_uuid_parse(tab+1, uu) < 0) {
		cl_log(LOG_ERR, "Malformed uuid in line [%s] (5)", linebuf);
		return -1;
	}
	*tab = EOS;
	strncpy(nodename, linebuf, HOSTLENG);
	return 1;
}

static int
write_node_uuid_file(struct sys_config * cfg)
{
	int		j;
	const char *	tmpname =	HOSTUUIDCACHEFILETMP;
	const char *	finalname =	HOSTUUIDCACHEFILE;
	FILE *		f;

	if (!nodecache_read_yet) {
		read_node_uuid_file(cfg);
	}
	(void)unlink(tmpname);
	if ((f=fopen(tmpname, "w")) == NULL) {
		cl_perror("%s: Cannot fopen %s for writing"
		,	__FUNCTION__, tmpname);
		return HA_FAIL;
	}
	for (j=0; j < cfg->nodecount; ++j) {
		if (cfg->nodes[j].nodetype != NORMALNODE_I) {
			continue;
		}
		if (node_uuid_file_out(f, cfg->nodes[j].nodename
		,	&cfg->nodes[j].uuid) != HA_OK) {
			fclose(f);
			unlink(tmpname);
			return HA_FAIL;
		}
	}
	if (fflush(f) < 0) {
		cl_perror("fflush error on %s", tmpname);
		fclose(f);
		unlink(tmpname);
		return HA_FAIL;
	}
	if (fsync(fileno(f)) < 0) {
		cl_perror("fsync error on %s", tmpname);
		fclose(f);
		unlink(tmpname);
		return HA_FAIL;
	}
	if (fclose(f) < 0) {
		cl_perror("fclose error on %s", tmpname);
		unlink(tmpname);
		return HA_FAIL;
	}
	if (rename(tmpname, finalname) < 0) {
		cl_perror("Cannot rename %s to %s", tmpname, finalname);
		unlink(tmpname);
		return HA_FAIL;
	}
	sync();
	return HA_OK;
}

static int
read_node_uuid_file(struct sys_config * cfg)
{
	FILE *		f;
	char		host[HOSTLENG];
	cl_uuid_t	uu;
	int		rc;
	const char *	uuidcachename = HOSTUUIDCACHEFILE;
	gboolean	outofsync = FALSE;

	nodecache_read_yet = TRUE;
	if (!cl_file_exists(uuidcachename)){
		return HA_OK;
	}
	if ((f=fopen(uuidcachename, "r")) == NULL) {
		cl_perror("%s: Cannot fopen %s for reading"
		,	__FUNCTION__, uuidcachename);
		return HA_FAIL;
	}

	while ((rc=node_uuid_file_in(f, host, &uu)) > 0) {
		struct node_info *	thisnode = lookup_tables(host, &uu);
		cl_uuid_t		curuuid;
		if (thisnode == NULL) {
			/* auto-added node */
			add_node(host, NORMALNODE_I);
			update_tables(host, &uu);
			continue;
		}
		
		nodename2uuid(host, &curuuid);
		if (cl_uuid_compare(&uu, &curuuid) != 0) {
			if (!cl_uuid_is_null(&uu)) {
				update_tables(host, &uu);
				outofsync=TRUE;
			}
		}
	}
	fclose(f);
	/*
	 * If outofsync is TRUE, then we need to write out a new
	 * uuid cache file.
	 */
	if (outofsync) {
		write_node_uuid_file(cfg);
	}
	return rc < 0 ? HA_FAIL: HA_OK;
}


int
write_delnode_file(struct sys_config* cfg)
{
	const char *	tmpname =	DELHOSTCACHEFILETMP;
	const char *	finalname =	DELHOSTCACHEFILE;
	FILE *		f;
	GList*		list = NULL;
	const struct node_info*	hip;
	
	if (!delcache_read_yet) {
		read_delnode_file(cfg);
	}
	(void)unlink(tmpname);
	if ((f=fopen(tmpname, "w")) == NULL) {
		cl_perror("%s: Cannot fopen %s for writing", 
			  __FUNCTION__, tmpname);
		return HA_FAIL;
	}
	
	list = del_node_list;
	while(list != NULL){
		hip = (const struct node_info*) list->data;
		if (hip == NULL){
			break; /*list empty*/
		}
		
		if (node_uuid_file_out(f, hip->nodename,
				       &hip->uuid) != HA_OK) {
			fclose(f);
			unlink(tmpname);
			return HA_FAIL;
		}
		list = g_list_next(list);
	}
	
	if (fflush(f) < 0) {
		cl_perror("fflush error on %s", tmpname);
		fclose(f);
		unlink(tmpname);
		return HA_FAIL;
	}
	if (fsync(fileno(f)) < 0) {
		cl_perror("fsync error on %s", tmpname);
		fclose(f);
		unlink(tmpname);
		return HA_FAIL;
	}
	if (fclose(f) < 0) {
		cl_perror("fclose error on %s", tmpname);
		unlink(tmpname);
		return HA_FAIL;
	}
	if (rename(tmpname, finalname) < 0) {
		cl_perror("Cannot rename %s to %s", tmpname, finalname);
		unlink(tmpname);
		return HA_FAIL;
	}
	sync();
	return HA_OK;	
	
}

static int
read_delnode_file(struct sys_config* cfg)
{
	FILE *		f;
	char		host[HOSTLENG];
	cl_uuid_t	uu;
	int		rc;
	const char *	filename = DELHOSTCACHEFILE;
	struct node_info thisnode;

	delcache_read_yet = TRUE;
	if (!cl_file_exists(filename)){
		return HA_OK;
	}
	
	if ((f=fopen(filename, "r")) == NULL) {
		cl_perror("%s: Cannot fopen %s for reading"
		,	__FUNCTION__, filename);
		return HA_FAIL;
	}
	
	while ((rc=node_uuid_file_in(f, host, &uu)) > 0) {
		strncpy(thisnode.nodename, host, HOSTLENG);
		cl_uuid_copy(&thisnode.uuid, &uu);
		if (lookup_node(thisnode.nodename)){
                       delete_node(thisnode.nodename);
                }

	}
	fclose(f);

	return rc < 0 ? HA_FAIL: HA_OK;
	
}

int
write_cache_file(struct sys_config* cfg)
{
	if (DoManageResources){
		return HA_OK;
	}
	if (write_node_uuid_file(cfg) != HA_OK){
		return HA_FAIL;
	}
	
	return HA_OK;
}

int 
read_cache_file(struct sys_config* cfg)
{
	
	if (DoManageResources){
		return HA_OK;
	}
	if (read_node_uuid_file(cfg) != HA_OK){
		return HA_FAIL;
	}
	
	return read_delnode_file(cfg);	
	
}



