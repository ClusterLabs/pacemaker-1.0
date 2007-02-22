/* 
 * ccmgraph.c: Keeps track of the connectivity within the cluster members
 *		to derive the largest totally connected subgraph.
 *
 * Copyright (c) International Business Machines  Corp., 2002
 * Author: Ram Pai (linuxram@us.ibm.com)
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
 *
 */
#include <lha_internal.h>
#include <ccm.h>

/* ASSUMPTIONS IN THIS FILE. 
 * we assume that there can be at most MAXNODE number of nodes in the graph.
 * we assume that we are working with only one graph at a time, from the call
 * to graph_init() to graph_delete().  If Multiple graph_init() are done 
 * simultaneously then results may be unpredictable.
 */

static vertex_t  graph[MAXNODE]; /* allocate memory statically */
static char 	vyesorno='n';
#define GRAPH_TIMEOUT  15
#define GRAPH_TIMEOUT_TOO_LONG  25




static void
bitmap_display(char* bitmap)
{
	ccm_debug2(LOG_DEBUG, "bitmap_display:%x", (unsigned int) bitmap[0]);	
}

static void 
graph_display(graph_t* gr)
{
	int i;

	if (gr == NULL){
		ccm_log(LOG_ERR, "graph_display:graph is NULL");
		return;
	}
	
	for ( i = 0 ; i < gr->graph_nodes; i++ ) {
		char* bitmap = gr->graph_node[i]->bitmap;
		int	index = gr->graph_node[i]->uuid;
		
		ccm_debug2(LOG_DEBUG, "graph_display:node[%d]'s bitmap is:", index);
		if(bitmap != NULL) {
			bitmap_display(bitmap);
		}
	}
}



/* */
/* clean up the unneccessary bits in the graph and check for */
/* inconsistency. */
/* */
static void
graph_sanitize(graph_t *gr)
{
	char *bitmap;
	int i,j, uuid_i, uuid_j;
	vertex_t **graph_node;

	(void)graph_display;

	graph_node = gr->graph_node;

	for ( i=0; i < gr->graph_nodes; i++ ) {
		uuid_i = graph_node[i]->uuid;

		assert(uuid_i >= 0 && uuid_i < MAXNODE);
		if(graph_node[i]->bitmap == NULL) {
			bitmap_create(&bitmap, MAXNODE);
			graph_node[i]->bitmap = bitmap;
			graph_node[i]->count = 0;
		}
		/* 
		 * Loop through each uuid from 0 to MAXNODE.
		 * If there is no vertex with the corresponding uuid,
		 * reset the bit corresponding to this uuid.
		 */
		for(uuid_j=0; uuid_j < MAXNODE; uuid_j++) {
			for (j=0; j<gr->graph_nodes; j++) {
				if(uuid_j == graph_node[j]->uuid) {
					break;
				}
			}
			if(j == gr->graph_nodes) {
				/* node uuid_j is not in the graph, so clear its
				 * bits.
				 */
				bitmap_clear(uuid_j, graph_node[i]->bitmap, 
					     MAXNODE);
			} else {
				if(uuid_i == uuid_j) {
					continue;
				}
				
				if(graph_node[j]->bitmap == NULL) {
					bitmap_create(&bitmap, MAXNODE);
					graph_node[j]->bitmap = bitmap;
					graph_node[j]->count = 0;
				}

				if(!bitmap_test(uuid_j,graph_node[i]->bitmap,
						MAXNODE) ||
				   !bitmap_test(uuid_i,graph_node[j]->bitmap,
						MAXNODE)){
					bitmap_clear(uuid_j,
						graph_node[i]->bitmap,MAXNODE);
					bitmap_clear(uuid_i,
						graph_node[j]->bitmap,MAXNODE);
				}
			}
		}
		graph_node[i]->count = 
			 bitmap_count(graph_node[i]->bitmap,MAXNODE);
	}

	return;
}
		

/* */
/* print the vertices that belong the largest totally connected subgraph. */
/* */
static void
print_vertex(vertex_t **vertex, int maxnode)
{
	int i,j;

	for ( i = 0 ; i < maxnode ; i++) {
		printf("%d:\t",i);
		for ( j = 0 ; j < maxnode ; j++) {
			if(bitmap_test(j,  vertex[i]->bitmap, maxnode)) {
				printf(" 1 ");
			}else{
				printf(" 0 ");
			}
		}
		printf("uuid=%d, count=%d\n",vertex[i]->uuid,vertex[i]->count);
		printf("\n");
	}
	printf("----------------------------------------\n");
}


/* */
/* BEGIN OF FUNCTIONS THAT FORM THE CORE OF THE ALGORITHM */
/* */

/* */
/* the function that orders the vertices in the graph while sorting. */
/* */
static int
compare(const void *value1, const void *value2)
{
        const vertex_t *t1 = *(const vertex_t * const *)value1;
        const vertex_t *t2 = *(const vertex_t * const *)value2;

        return(t2->count - t1->count);
}


static void
relocate(vertex_t **vertex, 
		int indx, 
		int size, 
		int *indxtab, 
		int maxnode)
{
	vertex_t *tmp_vertex;
	int i;

	tmp_vertex = vertex[indx];
	for  ( i = indx+1; i < size; i++ ) {
		if(tmp_vertex->count >= vertex[i]->count) {
			break;
		}
		vertex[i-1] = vertex[i];
		indxtab[vertex[i-1]->uuid] = i-1;
	}
	vertex[i-1] = tmp_vertex;
	indxtab[vertex[i-1]->uuid] = i-1;
}

static void
decrement_count(vertex_t **vertex, int indx, 
			int size, 
			int *indxtab, 
			int maxnode)
{
	vertex_t *tmp_vertex;

	tmp_vertex = vertex[indx];
	tmp_vertex->count--;
	relocate(vertex, indx, size, indxtab, maxnode);
}


static int
find_best_candidate(vertex_t **vertex, int startindx, 
				int size, 
				int *indxtab, 
				int maxnode) 
{
	int min_indx, min_count;
	int i, uuid;
	int count, indx;


	min_indx = startindx;
	min_count = INT_MAX;
	
	for ( i = size-1; i >= startindx; i-- ) {
		count = 0;
		for (uuid = 0; uuid < maxnode; uuid++) {
			if(bitmap_test(uuid, vertex[i]->bitmap, maxnode)){
				indx = indxtab[uuid];
				if(indx == -1 || indx >= size) {
					continue;
				}

				count += vertex[indx]->count;
			}
		}
		if(count == min_count) {
			if (vyesorno == 'y') {
				ccm_debug2(LOG_DEBUG
				,	"find_best_candidate:probably 1 more group exists");
			}
		}
		if(count < min_count) {
			min_count = count;
			min_indx = i;
		} 
	}			
	return min_indx;
}

static int
find_size(vertex_t **vertex, int maxnode)
{
	int size, i;
	assert(vertex[0]->count == 0);
	size=1;
	for ( i= 1 ; i < maxnode; i++ ) {
		if(vertex[i]->count == size) {
			size++;
		} else {
			break;
		}
	}
	return size;
}

static int
delete_entry(vertex_t **vertex, int indx, int size, 
			int *indxtab, int maxnode)
{
	vertex_t *tmp_vertex;
	int  uuid;
	int  loc;

	/* move this entry to the end of the table and shuffle the other
	 * entries up
	 */
	if (vyesorno == 'y') {
		ccm_debug2(LOG_DEBUG, "delete_entry:k=%d is being removed",indx);
	}
	tmp_vertex = vertex[indx];
	tmp_vertex->count--;
	relocate(vertex, indx, size, indxtab, maxnode);

	for ( uuid = 0 ; uuid < maxnode ; uuid ++ ){
		if(bitmap_test(uuid, tmp_vertex->bitmap, maxnode)) {
			loc = indxtab[uuid];
			if(loc == -1 || loc >= size-1) {
				continue;
			}
			decrement_count(vertex, loc, size-1, indxtab, maxnode);
		}
	}
	if (vyesorno == 'y') {
		print_vertex(vertex, maxnode);
	}
	if (tmp_vertex->count == 0) {
		return find_size(vertex+size-1, maxnode-indx);
	}
	return -1;
}


#ifdef NEED_PRINT_MEMBERS
static void
print_members(vertex_t **vertex, int maxmem)
{
	int i;

	printf("the members are \n");
	for ( i = 0 ; i < maxmem ; i++) {
		printf("%d ", vertex[i]->uuid);
	}
	printf("\n");
}
#endif

static int
get_max_clique(vertex_t **vertex,  int maxnode, int *loc)
{
	int i,j,k,num;
	int maxconnect, tmp_maxconnect;
	int size;
	int *indxtab;

	/* sort the scratchpad entries with respect to their 
	 * connectivity value 
	 */
	qsort(vertex, maxnode, sizeof(vertex_t *), compare);

	/* indx the uuid into the indx table */
	indxtab = g_malloc(MAXNODE*sizeof(int));
	
	/*
	 * TOBEDONE: we really do not need to allocate MAXNODE size array
	 * What is required is: Find the max uuid in the vertex[] array
	 * and allocate a indxtab table of that size. Postponing
	 * the implementation currently.
	 */
	for ( i = 0 ; i < MAXNODE ; i++ ) {
		*(indxtab+i) = -1;
	}
	for ( i = 0 ; i < maxnode ; i++ ) {
		if(vertex[i]->uuid != -1) {
			indxtab[vertex[i]->uuid] = i;
		}
	}
	
	maxconnect = 1;
	for ( j=i-1 ; j>=0;  j-- ) {
		if (vyesorno == 'y') {
			print_vertex(vertex, maxnode);
		}

		if((j+1)<maxconnect){
			break; /* done */
		}
		
		if(vertex[j]->count >= j+1){
			break; /* done */
		}

		/* find number of entries with the same connectivity value */
		num=1;
		for(k=j-1; k>=0; k--) {
			if(vertex[j]->count == vertex[k]->count){
				num++;
			}
			else {
				break;
			}
		}
		
		/* find the best candidate to be considered for removal */
		k = find_best_candidate(vertex, j-num+1, j+1, indxtab, 
					maxnode); 
		if (vyesorno == 'y') {
			ccm_debug2(LOG_DEBUG
			,	"get_max_clique:k=%d is the best candidate for removal",k);
		}

		/* delete the candidate */
		tmp_maxconnect = delete_entry(vertex, k, j+1, indxtab, 
					maxnode);
		if(tmp_maxconnect>maxconnect) {
				*loc = j;
				maxconnect=tmp_maxconnect;
		}
	}
	
	if ((j+1) < maxconnect) {
		size = maxconnect;
	} else {
		*loc = 0; size = j+1; 
	}
	
	g_free(indxtab);


	return size;
}
/* */
/* END OF FUNCTIONS THAT FORM THE CORE OF THE ALGORITHM */
/* */

/* */
/* initialize the graph. */
/* */
graph_t *
graph_init()
{
	int i;
	graph_t *gr;

	if((gr = (graph_t *)g_malloc(sizeof(graph_t))) == NULL){
		return NULL;
	}
	
	memset(gr, 0, sizeof(graph_t));
	memset(graph, 0, sizeof(graph));
	for ( i = 0 ; i < MAXNODE ; i++ ) {
		gr->graph_node[i] = &graph[i];
	}

	return gr;
}


/* */
/* free all the datastructures  */
/* */
void
graph_free(graph_t *gr)
{
	int i;
	if(!gr) {
		return;
	}

	for ( i = 0 ; i < gr->graph_nodes; i++ ) {
		if(gr->graph_node[i]->bitmap != NULL) {
			bitmap_delete(gr->graph_node[i]->bitmap);
		}
	}
	g_free(gr);
	return;
}


/* */
/* add a new member to the graph, whose id is 'uuid' */
/* */
void
graph_add_uuid(graph_t *gr, int uuid)
{
	int i;
	for ( i = 0 ; i < gr->graph_nodes; i++ ) {
		if(gr->graph_node[i]->uuid == uuid) {
			return;
		}
	}

	gr->graph_node[gr->graph_nodes++]->uuid = uuid;
}

/* */
/* add the member whose id is 'dst_uuid' to the connectivity list */
/* of the member with id 'src_uuid' */
/* */
void
graph_add_to_membership(graph_t *gr, int src_uuid, int dst_uuid)
{
	int i;
	for ( i = 0 ; i < gr->graph_nodes; i++ ) {
		if(gr->graph_node[i]->uuid == src_uuid) {
			assert(gr->graph_node[i]->bitmap);
			bitmap_mark(dst_uuid, gr->graph_node[i]->bitmap,
						MAXNODE);
			return;
		}
	}
	assert(0);
}



/* */
/* update the connectivity information of the member whose id is 'uuid'. */
/* */
void
graph_update_membership(graph_t *gr, 
			int uuid, 
			char *bitlist)
{
	int i;
	for ( i = 0 ; i < gr->graph_nodes; i++ ) {
		if(gr->graph_node[i]->uuid == uuid) {
			/* assert that this is not a duplicate message */
			if(gr->graph_node[i]->bitmap != NULL) {
				bitmap_delete(gr->graph_node[i]->bitmap);
				gr->graph_rcvd--;
			}
			gr->graph_node[i]->bitmap = bitlist;
			/* postpone the calculation of count, because
			 * we have to sanitize this graph after
			 * reciving all the bitmaps 
			 */
			gr->graph_rcvd++;
			break;
		}
	}

	/* make sure we have not received message from unknown node */
	assert(i < gr->graph_nodes);

	return;
}

/* */
/* return TRUE, if all the members of the graph have their */
/* connectivity information updated. */
/* */
int
graph_filled_all(graph_t *gr)
{
	return (gr->graph_rcvd == gr->graph_nodes);
}

/* */
/* return the largest fully connected subgraph. */
/* */
int
graph_get_maxclique(graph_t *gr, char **bitmap)
{
	int loc = 0;
	int i, size, numBytes;
	
	graph_sanitize(gr);
	size = get_max_clique(gr->graph_node, gr->graph_nodes, 
				&loc);
	numBytes = bitmap_create(bitmap, MAXNODE);
	for ( i = loc ; i < size ; i++ ) {
		bitmap_mark(gr->graph_node[i]->uuid, *bitmap, MAXNODE);
	}
	return size;
}
