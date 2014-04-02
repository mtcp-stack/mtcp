#ifndef __MEMORY_MGT_H_
#define __MEMORY_MGT_H_

struct mem_pool;
typedef struct mem_pool* mem_pool_t;

/* create a memory pool with a chunk size and total size
   an return the pointer to the memory pool */
mem_pool_t MPCreate(int chunk_size, size_t total_size, int is_hugepage);

/* allocate one chunk */
void *MPAllocateChunk(mem_pool_t mp);

/* free one chunk */
void MPFreeChunk(mem_pool_t mp, void *p);

/* destroy the memory pool */
void MPDestroy(mem_pool_t mp);

/* return the number of free chunks */
int MPGetFreeChunks(mem_pool_t mp);

#endif /* __MEMORY_MGT_H_ */
