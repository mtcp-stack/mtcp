
/* 
 * 2010.12.10 Shinae Woo
 * Ring buffer structure for managing dynamically allocating ring buffer
 * 
 * put data to the tail
 * get/pop/remove data from the head
 * 
 * always garantee physically continuous ready in-memory data from data_offset to the data_offset+len
 * automatically increase total buffer size when buffer is full
 * for efficiently managing packet payload and chunking
 *
 */

#ifndef NRE_RING_BUFFER
#define NRE_RING_BUFFER

#include <stdint.h>
#include <sys/types.h>

/*----------------------------------------------------------------------------*/
enum rb_caller
{
	AT_APP, 
	AT_MTCP
};
/*----------------------------------------------------------------------------*/
typedef struct mtcp_manager* mtcp_manager_t;
typedef struct rb_manager* rb_manager_t;
/*----------------------------------------------------------------------------*/
struct fragment_ctx
{
	uint32_t seq;
	uint32_t len : 31;
	uint32_t is_calloc : 1;
	struct fragment_ctx *next;
};
/*----------------------------------------------------------------------------*/
struct tcp_ring_buffer
{
	u_char* data;			/* buffered data */
	u_char* head;			/* pointer to the head */

	uint32_t head_offset;	/* offset for the head (head - data) */
	uint32_t tail_offset;	/* offset fot the last byte (null byte) */

	int merged_len;			/* contiguously merged length */
	uint64_t cum_len;		/* cummulatively merged length */
	int last_len;			/* currently saved data length */
	int size;				/* total ring buffer size */
	
	/* TCP payload features */
	uint32_t head_seq;
	uint32_t init_seq;

	struct fragment_ctx* fctx;
};
/*----------------------------------------------------------------------------*/
uint32_t RBGetCurnum(rb_manager_t rbm);
void RBPrintInfo(struct tcp_ring_buffer* buff);
void RBPrintStr(struct tcp_ring_buffer* buff);
void RBPrintHex(struct tcp_ring_buffer* buff);
/*----------------------------------------------------------------------------*/
rb_manager_t RBManagerCreate(mtcp_manager_t mtcp, size_t chunk_size, uint32_t cnum);
/*----------------------------------------------------------------------------*/
struct tcp_ring_buffer* RBInit(rb_manager_t rbm,  uint32_t init_seq);
void RBFree(rb_manager_t rbm, struct tcp_ring_buffer* buff);
uint32_t RBIsDanger(rb_manager_t rbm);
/*----------------------------------------------------------------------------*/
/* data manupulation functions */
int RBPut(rb_manager_t rbm, struct tcp_ring_buffer* buff, 
					void* data, uint32_t len , uint32_t seq);
size_t RBGet(rb_manager_t rbm, struct tcp_ring_buffer* buff, size_t len);
size_t RBRemove(rb_manager_t rbm, struct tcp_ring_buffer* buff, 
					size_t len, int option);
/*----------------------------------------------------------------------------*/

#endif
