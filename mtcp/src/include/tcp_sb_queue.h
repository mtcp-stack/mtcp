#ifndef __TCP_SB_QUEUE_
#define __TCP_SB_QUEUE_

#include "tcp_send_buffer.h"

/*---------------------------------------------------------------------------*/
typedef struct sb_queue* sb_queue_t;
/*---------------------------------------------------------------------------*/
sb_queue_t 
CreateSBQueue(int capacity);
/*---------------------------------------------------------------------------*/
void 
DestroySBQueue(sb_queue_t sq);
/*---------------------------------------------------------------------------*/
int 
SBEnqueue(sb_queue_t sq, struct tcp_send_buffer *buf);
/*---------------------------------------------------------------------------*/
struct tcp_send_buffer *
SBDequeue(sb_queue_t sq);
/*---------------------------------------------------------------------------*/

#endif /* __TCP_SB_QUEUE_ */
