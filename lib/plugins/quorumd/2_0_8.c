/* majority.c: quorum module
 * policy ---  if it has more than half of total number of nodes, you have the quorum
 *		if you have exactly half othe total number of nodes, you don't have the quorum
 *		otherwise you have a tie
 *
 * Copyright (C) 2005 Guochun Shi <gshi@ncsa.uiuc.edu>
 *
 * SECURITY NOTE:  It would be very easy for someone to masquerade as the
 * device that you're pinging.  If they don't know the password, all they can
 * do is echo back the packets that you're sending out, or send out old ones.
 * This does mean that if you're using such an approach, that someone could
 * make you think you have quorum when you don't during a cluster partition.
 * The danger in that seems small, but you never know ;-)
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

#include <string.h>
#include <unistd.h>
#include <gnutls/gnutls.h>

#include <heartbeat.h>
#include <pils/plugin.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_log.h>
#include <clplumbing/cl_quorumd.h>


#define PIL_PLUGINTYPE          HB_QUORUMD_TYPE
#define PIL_PLUGINTYPE_S        HB_QUORUMD_TYPE_S
#define PIL_PLUGIN              2_0_8
#define PIL_PLUGIN_S            "2_0_8"
#define PIL_PLUGINLICENSE	LICENSE_LGPL
#define PIL_PLUGINLICENSEURL	URL_LGPL

static struct hb_quorumd_fns Ops;

PIL_PLUGIN_BOILERPLATE2("1.0", Debug)
     
static const PILPluginImports*  PluginImports;
static PILPlugin*               OurPlugin;
static PILInterface*		OurInterface;
static struct hb_media_imports*	OurImports;
static void*			interfprivate = NULL;

#define LOG	PluginImports->log
#define MALLOC	PluginImports->alloc
#define STRDUP  PluginImports->mstrdup
#define FREE	PluginImports->mfree

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports);

PIL_rc
PIL_PLUGIN_INIT(PILPlugin*us, const PILPluginImports* imports)
{
	/* Force the compiler to do a little type checking */
	(void)(PILPluginInitFun)PIL_PLUGIN_INIT;

	PluginImports = imports;
	OurPlugin = us;

	/* Register ourself as a plugin */
	imports->register_plugin(us, &OurPIExports);  

	/*  Register our interface implementation */
 	return imports->register_interface(us, PIL_PLUGINTYPE_S,
					   PIL_PLUGIN_S,
					   &Ops,
					   NULL,
					   &OurInterface,
					   (void*)&OurImports,
					   interfprivate); 
}

static int
test(void)
{
	cl_log(LOG_DEBUG, "quorumd plugin 2.0.8, test()");
 	return 123; 
}


#define DEFAULT_TIMEOUT	5000
#define MAX_NAME_LEN	255
#define MAX_DATA	1024

#define QUORUM_YES	0
#define QUORUM_NO	1
#define QUORUM_TIE	2

#define	T_INIT		"init"
#define	T_QUORUM	"quorum"
#define	T_BRB		"brb"
#define	T_ACK		"ack"

typedef struct 
{
	char 	name[MAXLINE];
	int 	t_timeout;
	int	t_interval;
	int	t_takeover;
	int	t_giveup;
	int 	cur_quorum;
	int 	waiting;
	guint 	waiting_src;
	GList* 	clients;
	int 	nodenum;
	int 	weight;
}qs_cluster_t;

typedef struct
{
	char	CN[MAX_DN_LEN];
	int 	id;
	guint	ch_src;
	guint	timeout_src;
	int 	nodenum;
	int	weight;
	GIOChannel*	ch;
	qs_cluster_t*	cluster;		
	gnutls_session session;
}qs_client_t;

static void del_cluster(gpointer data);
static gboolean del_client(gpointer data);
static int load_config_file(void);
static gboolean _remove_cluster(gpointer key, gpointer value, gpointer user_data);

static int dump_data(int priority);
static void dump_cluster(int priority, qs_cluster_t* cluster);
static void _dump_cluster(gpointer key, gpointer value, gpointer user_data);
static void dump_client(int priority,qs_client_t* client);

static int on_connect(int sock, gnutls_session session, const char* CN);
static void on_disconnect(gpointer data);

static gboolean on_msg_arrived(GIOChannel *ch
,			       GIOCondition condition
,			       gpointer data);
static struct ha_msg* on_init_msg(struct ha_msg* msg, qs_client_t* client);
static struct ha_msg* on_quorum_msg(struct ha_msg* msg, qs_client_t* client);
static gboolean on_cluster_finish_waiting(gpointer data);
static int calculate_quorum(qs_client_t* client);

static GHashTable* clusters = NULL;

static int
init(void)
{
	cl_log(LOG_DEBUG, "quorumd plugin 2.0.8, init()");
	clusters = g_hash_table_new_full(g_str_hash, g_str_equal, cl_free, del_cluster);
	load_config_file();
	
 	return 0; 
}
#define	WHITESPACE	" \t\n\r\f"
#define	COMMENTCHAR	'#'
#define	CRLF		"\r\n"
static int
load_config_file(void)
{
	FILE* f;
	qs_cluster_t* cluster = NULL;
	GList* list = NULL;
	int skip;
	char buf[MAXLINE];
	char key[MAXLINE];
	char* p;
	char* cp;
	int value;
	quorum_log(LOG_INFO, "load config file %s", CONFIGFILE);
	/* read the config file*/
	f = fopen(CONFIGFILE, "r");
	if (f == NULL) {
		quorum_log(LOG_ERR, "can't open file %s", CONFIGFILE);
		return -1;
	}
	while (fgets(buf, MAXLINE, f) != NULL) {
		p = buf;
		p += strspn(p, WHITESPACE);
		if ((cp = strchr(p, COMMENTCHAR)) != NULL)  {
			*cp = EOS;
		}
		if ((cp = strpbrk(p, CRLF)) != NULL) {
			*cp = EOS;
		}
		if (*p == EOS) {
			continue;
		}
		sscanf(p, "%s", key);
		if (STRNCMP_CONST(key,"cluster")==0) {
			if(cluster != NULL) {
				if(!skip) {
					list = g_list_append(list, cluster);
				}
				else {
					cl_free(cluster);
				}
			}
			cluster = (qs_cluster_t*)cl_malloc(sizeof(qs_cluster_t));
			memset(cluster->name, 0, MAXLINE);
			sscanf(p, "%s %s", key, cluster->name);
			cluster->t_timeout = 0;
			cluster->t_interval = 0;
			cluster->t_giveup = 0;
			cluster->t_takeover = 0;
			cluster->clients = NULL;
			cluster->cur_quorum = -1;
			cluster->waiting = FALSE;
			cluster->nodenum = 0;
			cluster->weight = 0;
			skip = 0;
		}
		else
		{
			if(cluster == NULL) {
				fclose(f);
				quorum_log(LOG_ERR, "wrong format in file %s"
						, CONFIGFILE);
				return -1;
			}
			if (STRNCMP_CONST(key,"version")==0) {
				sscanf(p, "%s %s", key, buf);
				if(STRNCMP_CONST(buf,"2_0_8")!=0) {
					skip = 1;
				}
			}
			else
			if (!skip && STRNCMP_CONST(key,"timeout")==0) {
				sscanf(p, "%s %d", key, &value);
				cluster->t_timeout = value;
			}
			else 
			if (!skip && STRNCMP_CONST(key,"interval")==0) {
				sscanf(p, "%s %d", key, &value);
				cluster->t_interval = value;
			}
			else 
			if (!skip && STRNCMP_CONST(key,"giveup")==0) {
				sscanf(p, "%s %d", key, &value);
				cluster->t_giveup = value;
			}
			else 
			if (!skip && STRNCMP_CONST(key,"takeover")==0) {
				sscanf(p, "%s %d", key, &value);
				cluster->t_takeover = value;
			}
			else 
			if (!skip && STRNCMP_CONST(key,"nodenum")==0) {
				sscanf(p, "%s %d", key, &value);
				cluster->nodenum = value;
			}
			else 
			if (!skip && STRNCMP_CONST(key,"weight")==0) {
				sscanf(p, "%s %d", key, &value);
				cluster->weight = value;
			}
			else
			if (!skip) {
				quorum_log(LOG_ERR, "unknown key %s in file %s"
						, key, CONFIGFILE);
			}
		}
	}
	if(cluster != NULL) {
		if(!skip) {
			list = g_list_append(list, cluster);
		}
		else {
			cl_free(cluster);
		}
	}
	
	fclose(f);
	
	/* remove the cluster which is not in new configuration*/
	g_hash_table_foreach_remove(clusters, _remove_cluster, list);
	
	/* insert or update the clusters */
	while (list != NULL) {
		qs_cluster_t* old = NULL;
		qs_cluster_t* new = (qs_cluster_t*)list->data;
		list = g_list_remove(list, new);
		old = (qs_cluster_t*)g_hash_table_lookup(clusters, new->name);
		if (old == NULL) {
			g_hash_table_insert(clusters, cl_strdup(new->name), new);
		}
		else {
			old->t_timeout = new->t_timeout;
			old->nodenum = new->nodenum;
			old->weight = new->weight;
			del_cluster(new);
		}
	}
	
	dump_data(LOG_INFO);
	return 0;
}
gboolean
_remove_cluster(gpointer key, gpointer value, gpointer user_data)
{
	const char* name = (const char*) key;
	GList* list = (GList*)user_data;
	while (list != NULL) {
		qs_cluster_t* cluster = (qs_cluster_t*) list->data;
		if(strncmp(name, cluster->name, MAXLINE) == 0) {
			return FALSE;
		}
		list = g_list_next(list);
	}
	return TRUE;
}

int
on_connect(int sock, gnutls_session session, const char* CN)
{
	static int id = 1;
	qs_client_t* client = cl_malloc(sizeof(qs_client_t));
	if (client == NULL) {
		quorum_log(LOG_ERR, "cl_malloc failed for new client");
		return -1;
	}
	strncpy(client->CN, CN, MAX_DN_LEN);
	client->CN[MAX_DN_LEN-1] = '\0';
	client->id = id;
	client->cluster = NULL;
	client->ch = g_io_channel_unix_new(sock);
	g_io_channel_set_close_on_unref(client->ch,TRUE);
	client->ch_src = g_io_add_watch_full(client->ch,G_PRIORITY_DEFAULT
	,	G_IO_IN|G_IO_ERR|G_IO_HUP,on_msg_arrived, client, on_disconnect);
	client->timeout_src = -1;
	client->nodenum = 0;
	client->weight = 0;
	client->session = session;
	quorum_log(LOG_DEBUG, "create new client %d", id);
	id++;
	return 0;
}
void
on_disconnect(gpointer data)
{
	qs_client_t* client = (qs_client_t*)data;
	
	quorum_log(LOG_DEBUG, "client %d disconnected", client->id);
	if (client->timeout_src != -1) {
		g_source_remove(client->timeout_src);
	}
	client->timeout_src = g_timeout_add(0,del_client,client);
}

void
del_cluster(gpointer data)
{
	qs_client_t* client;
	qs_cluster_t* cluster = (qs_cluster_t*)data;
	while(cluster->clients != NULL) {
		client = (qs_client_t*)cluster->clients->data;
		cluster->clients = g_list_remove(cluster->clients,client);
		del_client(client);
	}
	if (cluster->waiting) {
		g_source_remove(cluster->waiting_src);
	}
	quorum_log(LOG_DEBUG, "delete cluster %s", cluster->name);
	cl_free(cluster);
	return;
}

gboolean
del_client(gpointer data)
{
	qs_client_t* client = (qs_client_t*)data;
	if (client == NULL) {
		return FALSE;
	}

	if (client->session != NULL) {
		gnutls_bye (client->session, GNUTLS_SHUT_WR);
		gnutls_deinit (client->session);
	}
	
	if (client->ch_src != 0) {
		g_source_remove(client->ch_src);
		client->ch_src = -1;
	}
	if (client->ch != NULL) {
		g_io_channel_unref(client->ch);
		client->ch = NULL;
	}
	if (client->timeout_src != 0) {
		g_source_remove(client->timeout_src);
		client->timeout_src = -1;
	}
	
	if (client->cluster != NULL) {
		client->cluster->clients = 
			g_list_remove(client->cluster->clients, client);
		if (client->cluster->cur_quorum == client->id) {
			client->cluster->waiting_src = g_timeout_add(
			client->cluster->t_takeover
			,	on_cluster_finish_waiting, client->cluster);
			client->cluster->waiting = TRUE;
			client->cluster->cur_quorum = -1;
		}
	}
	quorum_log(LOG_DEBUG, "delete client %d", client->id);
	cl_free(client);
	return FALSE;
}

gboolean
on_msg_arrived(GIOChannel *ch, GIOCondition condition, gpointer data)
{
	qs_client_t* client;
	int sock;
	char buf[MAXMSG];
	size_t len;
	
	client = (qs_client_t*) data;
	if (condition & G_IO_IN) {
		struct ha_msg* msg;
		sock = g_io_channel_unix_get_fd(ch);
		len = gnutls_record_recv(client->session, buf, MAXMSG);
		if ((ssize_t)len <= 0) {
			quorum_log(LOG_DEBUG
			, "receive 0 byte or error from client %d", client->id);
			return FALSE;
		}
		msg = wirefmt2msg(buf, len, FALSE);
		
		if (msg != NULL) {
			struct ha_msg* ret = NULL;
			char* str;
			const char* type;
			quorum_debug(LOG_DEBUG, "receive from client %d:", client->id);
			type = ha_msg_value(msg, F_TYPE);
			if (STRNCMP_CONST(type,T_INIT)==0) {
				ret = on_init_msg(msg, client);
			}
			else if (STRNCMP_CONST(type,T_QUORUM)==0) {
				ret = on_quorum_msg(msg, client);
			}
			else {
				ret = ha_msg_new(1);
				ha_msg_add(ret, F_TYPE, T_ACK);
				ha_msg_add(ret, "reason", "unknown msg type");
				ha_msg_add(ret, "result", "fail");
				quorum_log(LOG_ERR, "UNKOWN msg %s ", type);
			}
			if (ret != NULL) {			
				str  = msg2wirefmt(ret, &len);
				gnutls_record_send(client->session, str, len);
				quorum_debug(LOG_DEBUG, "send to client %d:", client->id);
				cl_free(str);
				ha_msg_del(ret);
			}
			ha_msg_del(msg);
		}
	}
	return TRUE;
}
struct ha_msg* 
on_init_msg(struct ha_msg* msg, qs_client_t* client)
{
	struct ha_msg* ret;
	const char* cl_name;
	qs_cluster_t* cluster;
	ret = ha_msg_new(1);
	ha_msg_add(ret, F_TYPE, T_ACK);
	
	if((cl_name = ha_msg_value(msg, "cl_name")) == NULL
	|| strncmp(cl_name, client->CN, MAX_DN_LEN) != 0
	|| (cluster = g_hash_table_lookup(clusters, cl_name)) == NULL) {
		quorum_log(LOG_DEBUG, "cl_name:%s, CN:%s",cl_name, client->CN);
		ha_msg_add(ret, "result", "fail");
		return ret;
	}
	client->cluster = cluster;
	cluster->clients = g_list_append(cluster->clients, client);
	client->timeout_src = g_timeout_add(cluster->t_timeout,del_client,client);
	
	ha_msg_add_int(ret, "timeout", cluster->t_timeout);
	ha_msg_add_int(ret, "interval", cluster->t_interval);
	ha_msg_add_int(ret, "giveup", cluster->t_giveup);
	ha_msg_add_int(ret, "takeover", cluster->t_takeover);
	ha_msg_add(ret, "result", "ok");
	return ret;
	
}
struct ha_msg* 
on_quorum_msg(struct ha_msg* msg, qs_client_t* client)
{
	struct ha_msg* ret = ha_msg_new(1);
	
	ha_msg_add(ret, F_TYPE, T_ACK);
	
	if (client->timeout_src != -1) {
		g_source_remove(client->timeout_src);
	}
	client->timeout_src = g_timeout_add(client->cluster->t_timeout
	,			del_client,client);
	
	if (ha_msg_value_int(msg, "nodenum", &client->nodenum) != HA_OK
	|| ha_msg_value_int(msg, "weight", &client->weight) != HA_OK) {
		ha_msg_add_int(ret, "quorum", 0);
		ha_msg_add(ret, "reason", "can't find nodenum or weight");
		ha_msg_add(ret, "result", "fail");
		return ret;
	}	
	
	ha_msg_add(ret, F_TYPE, T_ACK);
	ha_msg_add_int(ret, "quorum", calculate_quorum(client));
	ha_msg_add(ret, "result", "ok");
	return ret;
}
int
calculate_quorum(qs_client_t* client)
{
	qs_cluster_t* cluster = client->cluster;
	qs_client_t* cur_owner = NULL;
	qs_client_t* new_owner = NULL;
	GList* cur;
	int max_weight = 0;
	
	if (cluster->waiting) {
		return 0;
	}
	cur = cluster->clients;
	while (cur != NULL) {
		qs_client_t* cur_client = (qs_client_t*)cur->data;
		if (cur_client->id == cluster->cur_quorum) {
			cur_owner = cur_client;
		}
		if (cur_client->weight > max_weight) {
			max_weight = cur_client->weight;
			new_owner = cur_client;
		}
		cur = g_list_next(cur);
	}
	if (cur_owner == NULL) {
		cluster->cur_quorum = new_owner->id;
	}
	else if (new_owner != cur_owner) {
		cluster->waiting_src = g_timeout_add(
		cluster->t_timeout + cluster->t_giveup
		,	on_cluster_finish_waiting, cluster);
		cluster->waiting = TRUE;
		cluster->cur_quorum = -1;
		return 0;
	}
	if (client->id == cluster->cur_quorum) {
		return 1;
	}
	return 0;
}
gboolean
on_cluster_finish_waiting(gpointer data)
{
	GList* cur;
	int max_weight = 0; 
	qs_cluster_t* cluster = (qs_cluster_t*) data;
	
	cur = cluster->clients;
	while (cur != NULL) {
		qs_client_t* client = (qs_client_t*) cur->data;
		if (client->weight > max_weight) {
			cluster->cur_quorum = client-> id;
			max_weight = client->weight;
		}
		cur = g_list_next(cur);
	}
	cluster->waiting = FALSE;
	
	return FALSE;
}

void
dump_client(int priority, qs_client_t* client)
{
/*
typedef struct
{
	char	CN[MAX_DN_LEN];
	int 	id;
	guint	ch_src;
	guint	timeout_src;
	int 	nodenum;
	int	weight;
	GIOChannel*	ch;
	qs_cluster_t*	cluster;		
	gnutls_session session;
}qs_client_t;
*/
	quorum_log(priority, "\t\tclient %p", client);
	quorum_log(priority, "\t\tCN=%s", client->CN);
	quorum_log(priority, "\t\tid=%d", client->id);
	quorum_log(priority, "\t\tch_src=%d", client->ch_src);
	quorum_log(priority, "\t\ttimeout_src=%d", client->timeout_src);
	quorum_log(priority, "\t\tnodenum=%d", client->nodenum);
	quorum_log(priority, "\t\tweight=%d", client->weight);
	quorum_log(priority, "\t\tch=%p", client->ch);
	quorum_log(priority, "\t\tcluster=%p", client->cluster);
	quorum_log(priority, "\t\tsession=%p", client->session);
}
void
dump_cluster(int priority, qs_cluster_t* cluster)
{
/*
typedef struct 
{
	char 	name[MAXLINE];
	int 	t_timeout;
	int	t_interval;
	int	t_takeover;
	int	t_giveup;
	int 	cur_quorum;
	int 	waiting;
	guint 	waiting_src;
	GList* 	clients;
	int 	nodenum;
	int 	weight;
}qs_cluster_t;
*/
	GList* cur;
	quorum_log(priority, "cluster %p", cluster);
	quorum_log(priority, "\tname=%s", cluster->name);
	quorum_log(priority, "\tt_timeout=%d", cluster->t_timeout);
	quorum_log(priority, "\tt_interval=%d", cluster->t_interval);
	quorum_log(priority, "\tt_takeover=%d", cluster->t_takeover);
	quorum_log(priority, "\tt_giveup=%d", cluster->t_giveup);
	quorum_log(priority, "\tcur_quorum=%d", cluster->cur_quorum);
	quorum_log(priority, "\twaiting=%d", cluster->waiting);
	quorum_log(priority, "\twaiting_src=%d", cluster->waiting_src);
	quorum_log(priority, "\tnodenum=%d", cluster->nodenum);
	quorum_log(priority, "\tweight=%d", cluster->weight);
	quorum_log(priority, "\tclients=%p(%d)", cluster->clients
			,	g_list_length(cluster->clients));
	cur = cluster->clients;
	while (cur != NULL) {
		qs_client_t* client = (qs_client_t*)cur->data;
		dump_client(priority, client);
		cur = g_list_next(cur);
	}
}
void 
_dump_cluster(gpointer key, gpointer value, gpointer user_data)
{
	qs_cluster_t* cluster = (qs_cluster_t*)value;
	int priority = GPOINTER_TO_INT(user_data);
	dump_cluster(priority, cluster);
}
int
dump_data(int priority)
{
	quorum_log(priority, "dump data of quorum server (2_0_8):");
	g_hash_table_foreach(clusters, _dump_cluster, GINT_TO_POINTER(priority));
	return 0;
}

static struct hb_quorumd_fns Ops ={
	test,
	init,
	load_config_file,
	dump_data,
	on_connect
};

