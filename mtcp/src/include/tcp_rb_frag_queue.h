#ifndef __TCP_RB_FRAG_QUEUE_
#define __TCP_RB_FRAG_QUEUE_

#include "tcp_ring_buffer.h"

/*---------------------------------------------------------------------------*/
typedef struct rb_frag_queue* rb_frag_queue_t;
/*---------------------------------------------------------------------------*/
rb_frag_queue_t 
CreateRBFragQueue(int capacity);
/*---------------------------------------------------------------------------*/
void 
DestroyRBFragQueue(rb_frag_queue_t rb_fragq);
/*---------------------------------------------------------------------------*/
int 
RBFragEnqueue(rb_frag_queue_t rb_fragq, struct fragment_ctx *frag);
/*---------------------------------------------------------------------------*/
struct fragment_ctx *
RBFragDequeue(rb_frag_queue_t rb_fragq);
/*---------------------------------------------------------------------------*/

#endif /* __TCP_RB_FRAG_QUEUE_ */
