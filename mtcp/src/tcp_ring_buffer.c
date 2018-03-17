#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include "tcp_ring_buffer.h"
#include "tcp_rb_frag_queue.h"
#include "memory_mgt.h"
#include "debug.h"

#define MAX_RB_SIZE (16*1024*1024)
#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))
#ifdef ENABLELRO
#define __MEMCPY_DATA_2_BUFFER						\
	mtcp_manager_t mtcp = rbm->mtcp;				\
	if (mtcp->iom == &dpdk_module_func && len > TCP_DEFAULT_MSS)	\
		mtcp->iom->dev_ioctl(mtcp->ctx, 0, PKT_RX_TCP_LROSEG, buff->head + putx); \
	else								\
		memcpy(buff->head + putx, data, len);
#endif
/*----------------------------------------------------------------------------*/
struct rb_manager
{
	size_t chunk_size;
	uint32_t cur_num;
	uint32_t cnum;

	mem_pool_t mp;
	mem_pool_t frag_mp;

	rb_frag_queue_t free_fragq;		/* free fragment queue (for app thread) */
	rb_frag_queue_t free_fragq_int;	/* free fragment quuee (only for mtcp) */
#ifdef ENABLELRO
	mtcp_manager_t mtcp;
#endif
} rb_manager;
/*----------------------------------------------------------------------------*/
uint32_t
RBGetCurnum(rb_manager_t rbm)
{
	return rbm->cur_num;
}
/*-----------------------------------------------------------------------------*/
void 
RBPrintInfo(struct tcp_ring_buffer* buff)
{
	printf("buff_data %p, buff_size %d, buff_mlen %d, "
			"buff_clen %lu, buff_head %p (%d), buff_tail (%d)\n", 
			buff->data, buff->size, buff->merged_len, buff->cum_len, 
			buff->head, buff->head_offset, buff->tail_offset);
}
/*----------------------------------------------------------------------------*/
void 
RBPrintStr(struct tcp_ring_buffer* buff)
{
	RBPrintInfo(buff);
	printf("%s\n", buff->head);
}
/*----------------------------------------------------------------------------*/
void 
RBPrintHex(struct tcp_ring_buffer* buff)
{
	int i;

	RBPrintInfo(buff);

	for (i = 0; i < buff->merged_len; i++) {
		if (i != 0 && i % 16 == 0)
			printf("\n");
		printf("%0x ", *( (unsigned char*) buff->head + i));
	}
	printf("\n");
}
/*----------------------------------------------------------------------------*/
rb_manager_t
RBManagerCreate(mtcp_manager_t mtcp, size_t chunk_size, uint32_t cnum)
{
	rb_manager_t rbm = (rb_manager_t) calloc(1, sizeof(rb_manager));

	if (!rbm) {
		perror("rbm_create calloc");
		return NULL;
	}

	rbm->chunk_size = chunk_size;
	rbm->cnum = cnum;
#if ! defined(DISABLE_DPDK) && ! defined(ENABLE_ONVM)
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	sprintf(pool_name, "rbm_pool_%u", mtcp->ctx->cpu);
	rbm->mp = (mem_pool_t)MPCreate(pool_name, chunk_size, (uint64_t)chunk_size * cnum);	
#else
	rbm->mp = (mem_pool_t)MPCreate(chunk_size, (uint64_t)chunk_size * cnum);
#endif
	if (!rbm->mp) {
		TRACE_ERROR("Failed to allocate mp pool.\n");
		free(rbm);
		return NULL;
	}
#if ! defined(DISABLE_DPDK) && ! defined(ENABLE_ONVM)
	sprintf(pool_name, "frag_mp_%u", mtcp->ctx->cpu);
	rbm->frag_mp = (mem_pool_t)MPCreate(pool_name, sizeof(struct fragment_ctx), 
					    sizeof(struct fragment_ctx) * cnum);	
#else
	rbm->frag_mp = (mem_pool_t)MPCreate(sizeof(struct fragment_ctx), 
					    sizeof(struct fragment_ctx) * cnum);
#endif
	if (!rbm->frag_mp) {
		TRACE_ERROR("Failed to allocate frag_mp pool.\n");
		MPDestroy(rbm->mp);
		free(rbm);
		return NULL;
	}

	rbm->free_fragq = CreateRBFragQueue(cnum);
	if (!rbm->free_fragq) {
		TRACE_ERROR("Failed to create free fragment queue.\n");
		MPDestroy(rbm->mp);
		MPDestroy(rbm->frag_mp);
		free(rbm);
		return NULL;
	}
	rbm->free_fragq_int = CreateRBFragQueue(cnum);
	if (!rbm->free_fragq_int) {
		TRACE_ERROR("Failed to create internal free fragment queue.\n");
		MPDestroy(rbm->mp);
		MPDestroy(rbm->frag_mp);
		DestroyRBFragQueue(rbm->free_fragq);
		free(rbm);
		return NULL;
	}

#ifdef ENABLELRO
	rbm->mtcp = mtcp;
#endif
	return rbm;
}
/*----------------------------------------------------------------------------*/
static inline void
FreeFragmentContextSingle(rb_manager_t rbm, struct fragment_ctx* frag)
{
	if (frag->is_calloc)
		free(frag);
	else	
		MPFreeChunk(rbm->frag_mp, frag);
}
/*----------------------------------------------------------------------------*/
void
FreeFragmentContext(rb_manager_t rbm, struct fragment_ctx* fctx)
{
	struct fragment_ctx *remove;

	assert(fctx);
	if (fctx == NULL) 	
		return;

	while (fctx) {
		remove = fctx;
		fctx = fctx->next;
		FreeFragmentContextSingle(rbm, remove);
	}
}
/*----------------------------------------------------------------------------*/
static struct fragment_ctx *
AllocateFragmentContext(rb_manager_t rbm)
{
	/* this function should be called only in mtcp thread */
	struct fragment_ctx *frag;

	/* first try deqeue the fragment in free fragment queue */
	frag = RBFragDequeue(rbm->free_fragq);
	if (!frag) {
		frag = RBFragDequeue(rbm->free_fragq_int);
		if (!frag) {
			/* next fall back to fetching from mempool */
			frag = MPAllocateChunk(rbm->frag_mp);
			if (!frag) {
				TRACE_ERROR("fragments depleted, fall back to calloc\n");
				frag = calloc(1, sizeof(struct fragment_ctx));
				if (frag == NULL) {
					TRACE_ERROR("calloc failed\n");
					exit(-1);
				}
				frag->is_calloc = 1; /* mark it as allocated by calloc */
			}
		}
	}
	memset(frag, 0, sizeof(*frag));
	return frag;
}
/*----------------------------------------------------------------------------*/
struct tcp_ring_buffer* 
RBInit(rb_manager_t rbm, uint32_t init_seq)
{
	struct tcp_ring_buffer* buff = 
			(struct tcp_ring_buffer*)calloc(1, sizeof(struct tcp_ring_buffer));

	if (buff == NULL){
		perror("rb_init buff");
		return NULL;
	}

	buff->data = MPAllocateChunk(rbm->mp);
	if(!buff->data){
		perror("rb_init MPAllocateChunk");
		free(buff);
		return NULL;
	}

	//memset(buff->data, 0, rbm->chunk_size);

	buff->size = rbm->chunk_size;
	buff->head = buff->data;
	buff->head_seq = init_seq;
	buff->init_seq = init_seq;
	
	rbm->cur_num++;

	return buff;
}
/*----------------------------------------------------------------------------*/
void
RBFree(rb_manager_t rbm, struct tcp_ring_buffer* buff)
{
	assert(buff);
	if (buff->fctx) {
		FreeFragmentContext(rbm, buff->fctx);
		buff->fctx = NULL;
	}
	
	if (buff->data) {
		MPFreeChunk(rbm->mp, buff->data);
	}
	
	rbm->cur_num--;

	free(buff);
}
/*----------------------------------------------------------------------------*/
#define MAXSEQ               ((uint32_t)(0xFFFFFFFF))
/*----------------------------------------------------------------------------*/
static inline uint32_t
GetMinSeq(uint32_t a, uint32_t b)
{
	if (a == b) return a;
	if (a < b) 
		return ((b - a) <= MAXSEQ/2) ? a : b;
	/* b < a */
	return ((a - b) <= MAXSEQ/2) ? b : a;
}
/*----------------------------------------------------------------------------*/
static inline uint32_t
GetMaxSeq(uint32_t a, uint32_t b)
{
	if (a == b) return a;
	if (a < b) 
		return ((b - a) <= MAXSEQ/2) ? b : a;
	/* b < a */
	return ((a - b) <= MAXSEQ/2) ? a : b;
}
/*----------------------------------------------------------------------------*/
static inline int
CanMerge(const struct fragment_ctx *a, const struct fragment_ctx *b)
{
	uint32_t a_end = a->seq + a->len + 1;
	uint32_t b_end = b->seq + b->len + 1;

	if (GetMinSeq(a_end, b->seq) == a_end ||
		GetMinSeq(b_end, a->seq) == b_end)
		return 0;
	return (1);
}
/*----------------------------------------------------------------------------*/
static inline void
MergeFragments(struct fragment_ctx *a, struct fragment_ctx *b)
{
	/* merge a into b */
	uint32_t min_seq, max_seq;

	min_seq = GetMinSeq(a->seq, b->seq);
	max_seq = GetMaxSeq(a->seq + a->len, b->seq + b->len);
	b->seq  = min_seq;
	b->len  = max_seq - min_seq;
}
/*----------------------------------------------------------------------------*/
int
RBPut(rb_manager_t rbm, struct tcp_ring_buffer* buff, 
	   void* data, uint32_t len, uint32_t cur_seq)
{
	int putx, end_off;
	struct fragment_ctx *new_ctx;
	struct fragment_ctx* iter;
	struct fragment_ctx* prev, *pprev;
	int merged = 0;

	if (len <= 0)
		return 0;

	// if data offset is smaller than head sequence, then drop
	if (GetMinSeq(buff->head_seq, cur_seq) != buff->head_seq)
		return 0;

	putx = cur_seq - buff->head_seq;
	end_off = putx + len;
	if (buff->size < end_off) {
		return -2;
	}
	
	// if buffer is at tail, move the data to the first of head
	if (buff->size <= (buff->head_offset + end_off)) {
		memmove(buff->data, buff->head, buff->last_len);
		buff->tail_offset -= buff->head_offset;
		buff->head_offset = 0;
		buff->head = buff->data;
	}
#ifdef ENABLELRO
	// copy data to buffer
	__MEMCPY_DATA_2_BUFFER;
#else
	//copy data to buffer
	memcpy(buff->head + putx, data, len);
#endif
	if (buff->tail_offset < buff->head_offset + end_off) 
		buff->tail_offset = buff->head_offset + end_off;
	buff->last_len = buff->tail_offset - buff->head_offset;

	// create fragmentation context blocks
	new_ctx = AllocateFragmentContext(rbm);
	if (!new_ctx) {
		perror("allocating new_ctx failed");
		return 0;
	}
	new_ctx->seq  = cur_seq;
	new_ctx->len  = len;
	new_ctx->next = NULL;

	// traverse the fragment list, and merge the new fragment if possible
	for (iter = buff->fctx, prev = NULL, pprev = NULL; 
		iter != NULL;
		pprev = prev, prev = iter, iter = iter->next) {
		
		if (CanMerge(new_ctx, iter)) {
			/* merge the first fragment into the second fragment */
			MergeFragments(new_ctx, iter);

			/* remove the first fragment */
			if (prev == new_ctx) {
				if (pprev)
					pprev->next = iter;
				else
					buff->fctx = iter;
				prev = pprev;
			}	
			FreeFragmentContextSingle(rbm, new_ctx);
			new_ctx = iter;
			merged = 1;
		} 
		else if (merged || 
				 GetMaxSeq(cur_seq + len, iter->seq) == iter->seq) {
			/* merged at some point, but no more mergeable
			   then stop it now */
			break;
		} 
	}

	if (!merged) {
		if (buff->fctx == NULL) {
			buff->fctx = new_ctx;
		} else if (GetMinSeq(cur_seq, buff->fctx->seq) == cur_seq) {
			/* if the new packet's seqnum is before the existing fragments */
			new_ctx->next = buff->fctx;
			buff->fctx = new_ctx;
		} else {
			/* if the seqnum is in-between the fragments or
			   at the last */
			assert(GetMinSeq(cur_seq, prev->seq + prev->len) ==
				   prev->seq + prev->len);
			prev->next = new_ctx;
			new_ctx->next = iter;
		}
	}
	if (buff->head_seq == buff->fctx->seq) {
		buff->cum_len += buff->fctx->len - buff->merged_len;
		buff->merged_len = buff->fctx->len;
	}
	
	return len;
}
/*----------------------------------------------------------------------------*/
size_t
RBRemove(rb_manager_t rbm, struct tcp_ring_buffer* buff, size_t len, int option)
{
	/* this function should be called only in application thread */

	if (buff->merged_len < len) 
		len = buff->merged_len;
	
	if (len == 0) 
		return 0;

	buff->head_offset += len;
	buff->head = buff->data + buff->head_offset;
	buff->head_seq += len;

	buff->merged_len -= len;
	buff->last_len -= len;

	// modify fragementation chunks
	if (len == buff->fctx->len)	{
		struct fragment_ctx* remove = buff->fctx;
		buff->fctx = buff->fctx->next;
		if (option == AT_APP) {
			RBFragEnqueue(rbm->free_fragq, remove);
		} else if (option == AT_MTCP) {
			RBFragEnqueue(rbm->free_fragq_int, remove);
		}
	} 
	else if (len < buff->fctx->len) {
		buff->fctx->seq += len;
		buff->fctx->len -= len;
	} 
	else {
		assert(0);
	}

	return len;
}
/*----------------------------------------------------------------------------*/
