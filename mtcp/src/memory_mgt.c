#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef HUGETABLE
#include <hugetlbfs.h>
#endif
#include "debug.h"
#include "memory_mgt.h"

/*----------------------------------------------------------------------------*/
typedef struct tag_mem_chunk
{
	int mc_free_chunks;
	struct tag_mem_chunk *mc_next;
} mem_chunk;
/*----------------------------------------------------------------------------*/
typedef mem_chunk *mem_chunk_t;
#ifdef HUGETABLE
typedef enum { MEM_NORMAL, MEM_HUGEPAGE};
#endif
/*----------------------------------------------------------------------------*/
typedef struct mem_pool
{
	u_char *mp_startptr;      /* start pointer */
	mem_chunk_t mp_freeptr;   /* pointer to the start memory chunk */
	int mp_free_chunks;       /* number of total free chunks */
	int mp_total_chunks;       /* number of total free chunks */
	int mp_chunk_size;        /* chunk size in bytes */
	int mp_type;

} mem_pool;
/*----------------------------------------------------------------------------*/
mem_pool * 
MPCreate(int chunk_size, size_t total_size, int is_hugepage)
{
	int res;
	mem_pool_t mp;

	if (chunk_size < sizeof(mem_chunk)) {
		TRACE_ERROR("The chunk size should be larger than %lu. current: %d\n", 
				sizeof(mem_chunk), chunk_size);
		return NULL;
	}
	if (chunk_size % 4 != 0) {
		TRACE_ERROR("The chunk size should be multiply of 4!\n");
		return NULL;
	}

	//assert(chunk_size <= 2*1024*1024);

	if ((mp = calloc(1, sizeof(mem_pool))) == NULL) {
		perror("calloc failed");
		exit(0);
	}
	mp->mp_type = is_hugepage;
	mp->mp_chunk_size = chunk_size;
	mp->mp_free_chunks = ((total_size + (chunk_size -1))/chunk_size);
	mp->mp_total_chunks = mp->mp_free_chunks;
	total_size = chunk_size * ((size_t)mp->mp_free_chunks);


	/* allocate the big memory chunk */
#ifdef HUGETABLE
	if (is_hugepage == MEM_HUGEPAGE) {
		mp->mp_startptr = get_huge_pages(total_size, NULL);
		if (!mp->mp_startptr) {
			TRACE_ERROR("posix_memalign failed, size=%ld\n", total_size);
			assert(0);
			if (mp) free(mp);
			return (NULL);
		}
	} else {
#endif
		res = posix_memalign((void **)&mp->mp_startptr, getpagesize(), total_size);
		if (res != 0) {
			TRACE_ERROR("posix_memalign failed, size=%ld\n", total_size);
			assert(0);
			if (mp) free(mp);
			return (NULL);
		}
#ifdef HUGETABLE
	}
#endif

	/* try mlock only for superuser */
	if (geteuid() == 0) {
		if (mlock(mp->mp_startptr, total_size) < 0) 
			TRACE_ERROR("m_lock failed, size=%ld\n", total_size);
	}

	mp->mp_freeptr = (mem_chunk_t)mp->mp_startptr;
	mp->mp_freeptr->mc_free_chunks = mp->mp_free_chunks;
	mp->mp_freeptr->mc_next = NULL;

	return mp;
}
/*----------------------------------------------------------------------------*/
void *
MPAllocateChunk(mem_pool_t mp)
{
	mem_chunk_t p = mp->mp_freeptr;
	
	if (mp->mp_free_chunks == 0) 
		return (NULL);
	assert(p->mc_free_chunks > 0 && p->mc_free_chunks <= p->mc_free_chunks);
	
	p->mc_free_chunks--;
	mp->mp_free_chunks--;
	if (p->mc_free_chunks) {
		/* move right by one chunk */
		mp->mp_freeptr = (mem_chunk_t)((u_char *)p + mp->mp_chunk_size);
		mp->mp_freeptr->mc_free_chunks = p->mc_free_chunks;
		mp->mp_freeptr->mc_next = p->mc_next;
	}
	else {
		mp->mp_freeptr = p->mc_next;
	}

	return p;
}
/*----------------------------------------------------------------------------*/
void
MPFreeChunk(mem_pool_t mp, void *p)
{
	mem_chunk_t mcp = (mem_chunk_t)p;

	//	assert((u_char*)p >= mp->mp_startptr && 
	//		   (u_char *)p < mp->mp_startptr + mp->mp_total_size);
	assert(((u_char *)p - mp->mp_startptr) % mp->mp_chunk_size == 0);
	//	assert(*((u_char *)p + (mp->mp_chunk_size-1)) == 'a');
	//	*((u_char *)p + (mp->mp_chunk_size-1)) = 'f';

	mcp->mc_free_chunks = 1;
	mcp->mc_next = mp->mp_freeptr;
	mp->mp_freeptr = mcp;
	mp->mp_free_chunks++;
}
/*----------------------------------------------------------------------------*/
void
MPDestroy(mem_pool_t mp)
{
#ifdef HUGETABLE
	if(mp->mp_type == MEM_HUGEPAGE) {
		free_huge_pages(mp->mp_startptr);
	} else {
#endif
		free(mp->mp_startptr);
#ifdef HUGETABLE
	}
#endif
	free(mp);
}
/*----------------------------------------------------------------------------*/
int
MPGetFreeChunks(mem_pool_t mp)
{
	return mp->mp_free_chunks;
}
/*----------------------------------------------------------------------------*/
uint32_t 
MPIsDanger(mem_pool_t mp)
{
#define DANGER_THREASHOLD 0.95
#define SAFE_THREASHOLD 0.90
    uint32_t danger_num = mp->mp_total_chunks * DANGER_THREASHOLD;
    uint32_t safe_num = mp->mp_total_chunks * SAFE_THREASHOLD;
    if (danger_num < mp->mp_total_chunks - mp->mp_free_chunks) {
        return mp->mp_total_chunks - mp->mp_free_chunks - safe_num;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
uint32_t
MPIsOverSafeline(mem_pool_t mp)
{
#define SAFELINE 0.90
    uint32_t safe_num = mp->mp_total_chunks * SAFELINE;
    if (safe_num < mp->mp_total_chunks - mp->mp_free_chunks) {
        return 1;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
