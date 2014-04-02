/* 
 * TCP free fragment queue for ring buffer - tcp_rb_frag_queue.c/h
 *
 * EunYoung Jeong
 *
 * Part of this code borrows Click's simple queue implementation
 *
 * ============================== Click License =============================
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include "tcp_rb_frag_queue.h"
#include "debug.h"

/*----------------------------------------------------------------------------*/
#ifndef _INDEX_TYPE_
#define _INDEX_TYPE_
typedef uint32_t index_type;
typedef int32_t signed_index_type;
#endif
/*---------------------------------------------------------------------------*/
struct rb_frag_queue
{
	index_type _capacity;
	volatile index_type _head;
	volatile index_type _tail;

	struct fragment_ctx * volatile * _q;
};
/*----------------------------------------------------------------------------*/
static inline index_type 
NextIndex(rb_frag_queue_t rb_fragq, index_type i)
{
	return (i != rb_fragq->_capacity ? i + 1: 0);
}
/*---------------------------------------------------------------------------*/
static inline index_type 
PrevIndex(rb_frag_queue_t rb_fragq, index_type i)
{
	return (i != 0 ? i - 1: rb_fragq->_capacity);
}
/*---------------------------------------------------------------------------*/
static inline void 
RBFragMemoryBarrier(struct fragment_ctx * volatile frag, volatile index_type index)
{
	__asm__ volatile("" : : "m" (frag), "m" (index));
}
/*---------------------------------------------------------------------------*/
rb_frag_queue_t 
CreateRBFragQueue(int capacity)
{
	rb_frag_queue_t rb_fragq;

	rb_fragq = (rb_frag_queue_t)calloc(1, sizeof(struct rb_frag_queue));
	if (!rb_fragq)
		return NULL;

	rb_fragq->_q = (struct fragment_ctx **)
			calloc(capacity + 1, sizeof(struct fragment_ctx *));
	if (!rb_fragq->_q) {
		free(rb_fragq);
		return NULL;
	}

	rb_fragq->_capacity = capacity;
	rb_fragq->_head = rb_fragq->_tail = 0;

	return rb_fragq;
}
/*---------------------------------------------------------------------------*/
void 
DestroyRBFragQueue(rb_frag_queue_t rb_fragq)
{
	if (!rb_fragq)
		return;

	if (rb_fragq->_q) {
		free((void *)rb_fragq->_q);
		rb_fragq->_q = NULL;
	}

	free(rb_fragq);
}
/*---------------------------------------------------------------------------*/
int 
RBFragEnqueue(rb_frag_queue_t rb_fragq, struct fragment_ctx *frag)
{
	index_type h = rb_fragq->_head;
	index_type t = rb_fragq->_tail;
	index_type nt = NextIndex(rb_fragq, t);

	if (nt != h) {
		rb_fragq->_q[t] = frag;
		RBFragMemoryBarrier(rb_fragq->_q[t], rb_fragq->_tail);
		rb_fragq->_tail = nt;
		return 0;
	}

	TRACE_ERROR("Exceed capacity of frag queue!\n");
	return -1;
}
/*---------------------------------------------------------------------------*/
struct fragment_ctx *
RBFragDequeue(rb_frag_queue_t rb_fragq)
{
	index_type h = rb_fragq->_head;
	index_type t = rb_fragq->_tail;

	if (h != t) {
		struct fragment_ctx *frag = rb_fragq->_q[h];
		RBFragMemoryBarrier(rb_fragq->_q[h], rb_fragq->_head);
		rb_fragq->_head = NextIndex(rb_fragq, h);
		assert(frag);

		return frag;
	}

	return NULL;
}
/*---------------------------------------------------------------------------*/
