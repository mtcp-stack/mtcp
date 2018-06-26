#ifndef MEMORY_MGT_H
#define MEMORY_MGT_H
/*----------------------------------------------------------------------------*/
#if ! defined(DISABLE_DPDK) && !defined(ENABLE_ONVM)
#include <rte_common.h>
#include <rte_mempool.h>
/*----------------------------------------------------------------------------*/
typedef struct rte_mempool mem_pool;
typedef struct rte_mempool* mem_pool_t;
/* create a memory pool with a chunk size and total size
   an return the pointer to the memory pool */
mem_pool_t
MPCreate(char *name, int chunk_size, size_t total_size);
/*----------------------------------------------------------------------------*/
#else
struct mem_pool;
typedef struct mem_pool* mem_pool_t;

/* create a memory pool with a chunk size and total size
   an return the pointer to the memory pool */
mem_pool_t MPCreate(int chunk_size, size_t total_size);
#endif /* DISABLE_DPDK */
/*----------------------------------------------------------------------------*/
/* allocate one chunk */
void *
MPAllocateChunk(mem_pool_t mp);

/* free one chunk */
void
MPFreeChunk(mem_pool_t mp, void *p);

/* destroy the memory pool */
void
MPDestroy(mem_pool_t mp);

/* retrun the number of free chunks */
int
MPGetFreeChunks(mem_pool_t mp);
/*----------------------------------------------------------------------------*/
#endif /* MEMORY_MGT_H */
