/*
 * Heartbeat retransmission  mechanism
 *
 * Copyright (C) 2005 Guochun Shi <gshi@ncsa.uiuc.edu>
 *
 * This software licensed under the GNU LGPL.
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
#include <config.h>
#include <clplumbing/cl_uuid.h>
#include <heartbeat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <clplumbing/cl_misc.h>
#include <glib.h>
#include <clplumbing/Gmain_timeout.h>
#include <clplumbing/GSource.h>
#include <clplumbing/cl_random.h>


static void	schedule_rexmit_request(struct node_info* node, seqno_t seq, int delay);

static int		max_rexmit_delay = 250;
static GHashTable*	rexmit_hash_table = NULL;
void hb_set_max_rexmit_delay(int);


struct rexmit_info{
	seqno_t seq;
	struct node_info* node;
};

static gboolean rand_seed_set = FALSE;

void
hb_set_max_rexmit_delay(int value)
{
	if (value <= 0){
		cl_log(LOG_ERR, "%s: invalid value (%d)",
		       __FUNCTION__, value);
		return;
	}
	if (ANYDEBUG){
		cl_log(LOG_DEBUG, "Setting max_rexmit_delay to %d ms", 
		       value);
	}
	max_rexmit_delay =value;
	srand(cl_randseed());
	rand_seed_set = TRUE;
	
	return;
}
static guint
rexmit_hash_func(gconstpointer key)
{
	
	const struct rexmit_info* ri;
	guint hashvalue;

	ri = (const struct rexmit_info*) key;
	hashvalue=  ri->seq* g_str_hash(ri->node->nodename);
	
	return hashvalue;
}

static gboolean
rexmit_info_equal(gconstpointer a, gconstpointer b){
	
	const struct rexmit_info* ri1 ;
	const struct rexmit_info* ri2 ;
	
	ri1 = (const struct rexmit_info*) a;
	ri2 = (const struct rexmit_info*) b;
	
	if (ri1->seq == ri2->seq 
	    && strcmp(ri1->node->nodename, ri2->node->nodename)== 0){
		return TRUE;
	}
	
	return FALSE;
}

static void
free_data_func(gpointer data)
{
	if (data){
		ha_free(data);
		data = NULL;
	}
}


static void 
entry_display(gpointer key, gpointer value, gpointer user_data)
{
	struct rexmit_info* ri = (struct rexmit_info*)key;
	unsigned long  tag = (unsigned long) value;
	
	cl_log(LOG_INFO, "seq, node, nodename (%ld, %p, %s), tag = %ld",
	       ri->seq, ri->node, ri->node->nodename, tag);
}



static void
rexmit_hash_table_display(void)
{
	cl_log(LOG_INFO, "Dumping rexmit hash table:");
	if (rexmit_hash_table == NULL){
		cl_log(LOG_INFO, "rexmit_hash_table is NULL");
		return;
	}
	
	g_hash_table_foreach(rexmit_hash_table, entry_display, NULL);
	return;
	
}



int
init_rexmit_hash_table(void)
{
	rexmit_hash_table =  g_hash_table_new_full(rexmit_hash_func, 
						   rexmit_info_equal, 
						   free_data_func,
						   NULL);
	if (rexmit_hash_table == NULL){
		cl_log(LOG_ERR, "%s: creating rexmit hash_table failed",__FUNCTION__);
		return HA_FAIL;
	}
	
	return HA_OK;
}

int
destroy_rexmit_hash_table(void)
{
	if (rexmit_hash_table){
		g_hash_table_destroy(rexmit_hash_table);		
	}

	return HA_OK;
}


static gboolean
send_rexmit_request( gpointer data)
{
	struct rexmit_info* ri = (struct rexmit_info*) data;
	seqno_t seq = (seqno_t) ri->seq;
	struct node_info* node = ri->node;
	struct ha_msg*	hmsg;
	
	if ((hmsg = ha_msg_new(6)) == NULL) {
		cl_log(LOG_ERR, "%s: no memory for " T_REXMIT, 
		       __FUNCTION__);
		return FALSE;
	}
	
	
	if (ha_msg_add(hmsg, F_TYPE, T_REXMIT) != HA_OK
	    ||	ha_msg_add(hmsg, F_TO, node->nodename) !=HA_OK
	    ||	ha_msg_add_int(hmsg, F_FIRSTSEQ, seq) != HA_OK
	    ||	ha_msg_add_int(hmsg, F_LASTSEQ, seq) != HA_OK) {
		cl_log(LOG_ERR, "%s: adding fields to msg failed",
		       __FUNCTION__);
		ha_msg_del(hmsg);
		return FALSE;
	}
	
	if (send_cluster_msg(hmsg) != HA_OK) {
		cl_log(LOG_ERR, "%s: cannot send " T_REXMIT
		       " request to %s",__FUNCTION__,  node->nodename);
		ha_msg_del(hmsg);
		return FALSE;
	}
	
	node->track.last_rexmit_req = time_longclock();	
	
	if (!g_hash_table_remove(rexmit_hash_table, ri)){
		cl_log(LOG_ERR, "%s: entry not found in rexmit_hash_table"
		       "for seq/node(%ld %s)", 		       
		       __FUNCTION__, ri->seq, ri->node->nodename);
		return FALSE;
	}
	
	schedule_rexmit_request(node, seq, max_rexmit_delay);
	
	return FALSE;
}

#define	RANDROUND	(RAND_MAX/2)

static void
schedule_rexmit_request(struct node_info* node, seqno_t seq, int delay)    
{
	unsigned long		sourceid;
	struct rexmit_info*	ri;

	if (delay == 0){
		if (!rand_seed_set) {
			srand(cl_randseed());
			rand_seed_set = TRUE;
		}
		delay = ((rand()*max_rexmit_delay)+RANDROUND)/RAND_MAX;
	}
	
	ri = ha_malloc(sizeof(struct rexmit_info));
	if (ri == NULL){
		cl_log(LOG_ERR, "%s: memory allocation failed", __FUNCTION__);
		return;
	}
	
	ri->seq = seq;
	ri->node = node;
	
	sourceid = Gmain_timeout_add_full(G_PRIORITY_HIGH - 1, delay, 
					  send_rexmit_request, ri, NULL);
	G_main_setall_id(sourceid, "retransmit request", config->heartbeat_ms/2, 10);
	
	if (sourceid == 0){
		cl_log(LOG_ERR, "%s: scheduling a timeout event failed", 
		       __FUNCTION__);
		return;
	}

	if (rexmit_hash_table == NULL){
		init_rexmit_hash_table();
	}
	g_hash_table_insert(rexmit_hash_table, (gpointer)ri, (gpointer)sourceid);
	
	return ;
}

void
request_msg_rexmit(struct node_info *node, seqno_t lowseq,	seqno_t hiseq)
{
	
	int i;
	
	for (i = lowseq; i <= hiseq; i++){
		schedule_rexmit_request(node, i, 0);
	}

	return;

}

int
remove_msg_rexmit(struct node_info *node, seqno_t seq)
{
	struct rexmit_info ri;
	gpointer value;
	unsigned long sourceid;
			
	ri.seq = seq;
	ri.node =node;
	
	(void)rexmit_hash_table_display;
	value = g_hash_table_lookup(rexmit_hash_table, &ri);
	if (value == NULL){
		cl_log(LOG_ERR, "%s: no entry found in rexmit hash_table for the missing packet(%ld)",
		       __FUNCTION__, seq);
		return HA_FAIL;
	}else {
		sourceid = (unsigned long) value;
		Gmain_timeout_remove(sourceid);
		g_hash_table_remove(rexmit_hash_table, &ri);
	}
	
	return HA_OK;
}
