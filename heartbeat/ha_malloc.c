static const char * _ha_malloc_c_id = "$Id: ha_malloc.c,v 1.21 2004/01/21 00:54:29 horms Exp $";
#include <portability.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef BSD
#ifdef HAVE_MALLOC_H
#	include <malloc.h>
#endif
#endif
#include <heartbeat.h>
#include <hb_proc.h>

#include <ltdl.h>

/*
 * Compile time malloc debugging switches:
 *
 * MARK_PRISTINE - puts known byte pattern in freed memory
 *			Good at finding "use after free" cases
 *			Cheap in memory, but expensive in CPU
 *
 * MAKE_GUARD	 - puts a known pattern *after* allocated memory
 *			Good at finding overrun problems after the fact
 *			Cheap in CPU, adds a few bytes to each malloc item
 *
 */

#undef	MARK_PRISTINE		/* Expensive in CPU time */
#define	MAKE_GUARD	1	/* Adds 4 bytes memory - cheap in CPU*/


/*
 *
 *	Malloc wrapper functions
 *
 *	I wrote these so we can better track memory leaks, etc. and verify
 *	that the system is stable in terms of memory usage.
 *
 *	For our purposes, these functions are a somewhat faster than using
 *	malloc directly (although they use a bit more memory)
 *
 *	The general strategy is loosely related to the buddy system, 
 *	except very simple, well-suited to our continuous running
 *	nature, and the constancy of the requests and messages.
 *
 *	We keep an array of linked lists, each for a different size
 *	buffer.  If we need a buffer larger than the largest one provided
 *	by the list, we go directly to malloc.
 *
 *	Otherwise, we keep return them to the appropriate linked list
 *	when we're done with them, and reuse them from the list.
 *
 *	We never coalesce buffers on our lists, and we never free them.
 *
 *	It's very simple.  We get usage stats.  It makes me happy.
 *
 *
 * Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
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

#define	HA_MALLOC_MAGIC	0xFEEDBEEFUL
#define	HA_FREE_MAGIC	0xDEADBEEFUL


/*
 * We put a struct ha_mhdr in front of every malloc item.
 * This means each malloc item is 12 bytes bigger than it theoretically
 * needs to be.  But, it allows this code to be fast and recognize
 * multiple free attempts, and memory corruption *before* the object
 *
 * It's probably possible to combine these fields a bit,
 * since bucket and reqsize are only needed for allocated items,
 * both are bounded in value, and fairly strong integrity checks apply
 * to them.  But then we wouldn't be able to tell *quite* as reliably
 * if someone gave us an item to free that we didn't allocate...
 *
 * Could even make the bucket and reqsize objects into 16-bit ints...
 *
 * The idea of getting it all down into 32-bits of overhead is
 * an interesting thought...
 */

struct ha_mhdr {
#	ifdef HA_MALLOC_MAGIC
	unsigned long	magic;	/* Must match HA_*_MAGIC */
#endif
	size_t		reqsize;
	int		bucket;
};

struct ha_bucket {
	struct ha_mhdr		hdr;
	struct ha_bucket *	next;
};


#define	NUMBUCKS	8
#define	NOBUCKET	(NUMBUCKS)

static struct ha_bucket*	ha_malloc_buckets[NUMBUCKS];
static size_t	ha_bucket_sizes[NUMBUCKS];

static int ha_malloc_inityet = 0;
static size_t ha_malloc_hdr_offset = sizeof(struct ha_mhdr);

void*		ha_malloc(size_t size);
static void*	ha_new_mem(size_t size, int numbuck);
void*		ha_calloc(size_t nmemb, size_t size);
void		ha_free(void *ptr);
static void	ha_malloc_init(void);
static void	ha_dump_item(struct ha_bucket*b);

#ifdef MARK_PRISTINE
#	define	PRISTVALUE	0xff
	static int	ha_check_is_pristine(const void* v, unsigned size);
	static void	mark_pristine(void* v, unsigned size);
	static int	pristoff;
#endif

#define	BHDR(p)	 ((struct ha_bucket*)(void*)(((char*)p)-ha_malloc_hdr_offset))
#define	CBHDR(p) ((const struct ha_bucket*)(const void*)(((const char*)p)-ha_malloc_hdr_offset))
#define	MEMORYSIZE(p)(CBHDR(p)->hdr.reqsize)

#ifdef MAKE_GUARD
#	define GUARDLEN 2
	static const char ha_malloc_guard[] =
#if GUARDLEN == 1
	{0xA5};
#endif
#if GUARDLEN == 2
	{0x5A, 0xA5};
#endif
#if GUARDLEN == 4
	{0x5A, 0xA5, 0x5A, 0xA5};
#endif
#	define GUARDSIZE	sizeof(ha_malloc_guard)
#	define	ADD_GUARD(cp)	(memcpy((((char*)cp)+MEMORYSIZE(cp)), ha_malloc_guard, sizeof(ha_malloc_guard)))
#	define	GUARD_IS_OK(cp)	(memcmp((((char*)cp)+MEMORYSIZE(cp)), ha_malloc_guard, sizeof(ha_malloc_guard)) == 0)
#else
#	define GUARDSIZE	0
#	define ADD_GUARD(cp)	/* */
#	define GUARD_IS_OK(cp)	(1)
#endif



/*
 * ha_malloc: malloc clone
 */

void *
ha_malloc(size_t size)
{
	int			j;
	int			numbuck = NOBUCKET;
	struct ha_bucket*	buckptr = NULL;
	void*			ret;

	(void)_heartbeat_h_Id;
	(void)_ha_msg_h_Id;
	(void)_ha_malloc_c_id;

	if (!ha_malloc_inityet) {
		ha_malloc_init();
	}

	/*
	 * Find which bucket would have buffers of the requested size
	 */
	for (j=0; j < NUMBUCKS; ++j) {
		if (size <= ha_bucket_sizes[j]) {
			numbuck = j;
			buckptr = ha_malloc_buckets[numbuck];
			break;
		}
	}

	/*
	 * Pull it out of the linked list of free buffers if we can...
	 */

	if (buckptr == NULL) {
		ret = ha_new_mem(size, numbuck);
	}else{
		ha_malloc_buckets[numbuck] = buckptr->next;
		buckptr->hdr.reqsize = size;
		ret = (((char*)buckptr)+ha_malloc_hdr_offset);
		
#ifdef MARK_PRISTINE
		{
			int	bucksize = ha_bucket_sizes[numbuck];
			if (!ha_check_is_pristine(ret,	bucksize)) {
				ha_log(LOG_ERR
				,	"attempt to allocate memory"
				" which is not pristine.");
				ha_dump_item(buckptr);
			}
		}
#endif

#ifdef HA_MALLOC_MAGIC
		switch (buckptr->hdr.magic) {

			case HA_FREE_MAGIC:
				break;

			case HA_MALLOC_MAGIC:
				ha_log(LOG_ERR
				,	"attempt to allocate memory"
				" already allocated at 0x%lx"
				,	(unsigned long)ret);
				ha_dump_item(buckptr);
				ret=NULL;
				break;

			default:
				ha_log(LOG_ERR
				, "corrupt malloc buffer at 0x%lx"
				,	(unsigned long)ret);
				ha_dump_item(buckptr);
				ret=NULL;
				break;
		}
		buckptr->hdr.magic = HA_MALLOC_MAGIC;
#endif /* HA_MALLOC_MAGIC */
		if (curproc) {
			curproc->nbytes_req += size;
			curproc->nbytes_alloc+=ha_bucket_sizes[numbuck];
		}
		
	}

	if (ret && curproc) {
#ifdef HAVE_MALLINFO
		struct mallinfo	i = mallinfo();
		curproc->arena = i.arena;
#endif
		curproc->numalloc++;
	}
	if (ret) {
		ADD_GUARD(ret);
	}
	return(ret);
}

int
ha_is_allocated(const void *ptr)
{

#ifdef HA_MALLOC_MAGIC
	return (ptr && CBHDR(ptr)->hdr.magic == HA_MALLOC_MAGIC);
#else
	return (ptr != NULL);
#endif
}

/*
 * ha_free: "free" clone
 */

void
ha_free(void *ptr)
{
	int			bucket;
	struct ha_bucket*	bhdr;

	if (!ha_malloc_inityet) {
		ha_malloc_init();
	}

	if (ptr == NULL) {
		ha_log(LOG_ERR, "attempt to free NULL pointer in ha_free()");
		return;
	}

	/* Find the beginning of our "hidden" structure */

	bhdr = BHDR(ptr);

#ifdef HA_MALLOC_MAGIC
	switch (bhdr->hdr.magic) {
		case HA_MALLOC_MAGIC:
			break;

		case HA_FREE_MAGIC:
			ha_log(LOG_ERR
			,	"ha_free: attempt to free already-freed"
			" object at 0x%lx"
			,	(unsigned long)ptr);
			ha_dump_item(bhdr);
			return;
			break;
		default:
			ha_log(LOG_ERR, "ha_free: Bad magic number"
			" in object at 0x%lx"
			,	(unsigned long)ptr);
			ha_dump_item(bhdr);
			return;
			break;
	}
#endif
	if (!GUARD_IS_OK(ptr)) {
		ha_log(LOG_ERR
		,	"ha_free: attempt to free guard-corrupted"
		" object at 0x%lx", (unsigned long)ptr);
		ha_dump_item(bhdr);
		return;
	}
	bucket = bhdr->hdr.bucket;
#ifdef HA_MALLOC_MAGIC
	bhdr->hdr.magic = HA_FREE_MAGIC;
#endif

	/*
	 * Return it to the appropriate bucket (linked list), or just free
	 * it if it didn't come from one of our lists...
	 */

	if (bucket >= NUMBUCKS) {
		if (curproc) {
			if (curproc->nbytes_alloc >= bhdr->hdr.reqsize) {
				curproc->nbytes_req   -= bhdr->hdr.reqsize;
				curproc->nbytes_alloc -= bhdr->hdr.reqsize;
				curproc->mallocbytes  -= bhdr->hdr.reqsize;
			}
		}
		free(bhdr);
	}else{
		int	bucksize = ha_bucket_sizes[bucket];
		ASSERT(bhdr->hdr.reqsize <= ha_bucket_sizes[bucket]);
		if (curproc) {
			if (curproc->nbytes_alloc >= bhdr->hdr.reqsize) {
				curproc->nbytes_req  -= bhdr->hdr.reqsize;
				curproc->nbytes_alloc-= bucksize;
			}
		}
		bhdr->next = ha_malloc_buckets[bucket];
		ha_malloc_buckets[bucket] = bhdr;
#ifdef MARK_PRISTINE
		mark_pristine(ptr, bucksize);
#endif
	}
	if (curproc) {
		curproc->numfree++;
	}
}

/*
 * ha_new_mem:	use the real malloc to allocate some new memory
 */

static void*
ha_new_mem(size_t size, int numbuck)
{
	struct ha_bucket*	hdrret;
	size_t			allocsize;
	size_t			mallocsize;

	if (numbuck < NUMBUCKS) {
		allocsize = ha_bucket_sizes[numbuck];
	}else{
		allocsize = size;
	}

	mallocsize = allocsize + ha_malloc_hdr_offset + GUARDSIZE;

	if ((hdrret = malloc(mallocsize)) == NULL) {
		return(NULL);
	}

	hdrret->hdr.reqsize = size;
	hdrret->hdr.bucket = numbuck;
#ifdef HA_MALLOC_MAGIC
	hdrret->hdr.magic = HA_MALLOC_MAGIC;
#endif

	if (curproc) {
		curproc->nbytes_alloc += mallocsize;
		curproc->nbytes_req += size;
		curproc->mallocbytes += mallocsize;
	}
	return(((char*)hdrret)+ha_malloc_hdr_offset);
}


/*
 * ha_calloc: calloc clone
 */

void *
ha_calloc(size_t nmemb, size_t size)
{
	void *	ret = ha_malloc(nmemb*size);

	if (ret != NULL) {
		memset(ret, 0, nmemb*size);
	}
		
	return(ret);
}


/*
 * ha_strdup: strdup clone
 */

char *
ha_strdup(const char *s)
{
	void * ret = ha_malloc((strlen(s) + 1) * sizeof(char *));

	if (ret) {
		strcpy(ret, s);
	}
		
	return(ret);
}


/*
 * ha_malloc_init():	initialize our malloc wrapper things
 */

static void
ha_malloc_init()
{
	int	j;
	size_t	cursize = 32;

	ha_malloc_inityet = 1;
	if (ha_malloc_hdr_offset < sizeof(long long)) {
		ha_malloc_hdr_offset = sizeof(long long);
	}
	for (j=0; j < NUMBUCKS; ++j) {
		ha_malloc_buckets[j] = NULL;

		ha_bucket_sizes[j] = cursize;
		cursize <<= 1;
	}
#ifdef MARK_PRISTINE
	{
		struct ha_bucket	b;
		pristoff = (unsigned char*)&(b.next)-(unsigned char*)&b;
		pristoff += sizeof(b.next);
	}
#endif
}


static void
ha_dump_item(struct ha_bucket*b)
{
	unsigned char *	cbeg;
	unsigned char *	cend;
	unsigned char *	cp;
	ha_log(LOG_INFO, "Dumping ha_malloc item @ 0x%lx, bucket address: 0x%lx"
	,	((unsigned long)b)+ha_malloc_hdr_offset, (unsigned long)b);
#ifdef HA_MALLOC_MAGIC
	ha_log(LOG_INFO, "Magic number: 0x%lx reqsize=%ld"
	", bucket=%d, bucksize=%ld"
	,	b->hdr.magic
	,	(long)b->hdr.reqsize, b->hdr.bucket
	,	(long)(b->hdr.bucket >= NUMBUCKS ? 0 
	:	ha_bucket_sizes[b->hdr.bucket]));
#else
	ha_log(LOG_INFO, "reqsize=%ld"
	", bucket=%d, bucksize=%ld"
	,	(long)b->hdr.reqsize, b->hdr.bucket
	,	(long)(b->hdr.bucket >= NUMBUCKS ? 0 
	:	ha_bucket_sizes[b->hdr.bucket]));
#endif
	cbeg = ((char *)b)+ha_malloc_hdr_offset;
	cend = cbeg+b->hdr.reqsize+GUARDSIZE;

	for (cp=cbeg; cp < cend; cp+= sizeof(unsigned)) {
		ha_log(LOG_INFO, "%02x %02x %02x %02x \"%c%c%c%c\""
		,	(unsigned)cp[0], (unsigned)cp[1]
		,	(unsigned)cp[2], (unsigned)cp[3]
		,	cp[0], cp[1], cp[2], cp[3]);
	}
}
#ifdef MARK_PRISTINE
static int
ha_check_is_pristine(const void* v, unsigned size)
{
	const unsigned char *	cp;
	const unsigned char *	last;
	cp = v;
	last = cp + size;
	cp += pristoff;

	for (;cp < last; ++cp) {
		if (*cp != PRISTVALUE) {
			return FALSE;
		}
	}
	return TRUE;
}
static void
mark_pristine(void* v, unsigned size)
{
	unsigned char *	cp = v;
	memset(cp+pristoff, PRISTVALUE, size-pristoff);
}
#endif
