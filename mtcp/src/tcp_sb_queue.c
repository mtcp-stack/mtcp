/* 
 * TCP free send buffer queue - tcp_sb_queue.c/h
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

#include "tcp_sb_queue.h"
#include "debug.h"

/*----------------------------------------------------------------------------*/
#ifndef _INDEX_TYPE_
#define _INDEX_TYPE_
typedef uint32_t index_type;
typedef int32_t signed_index_type;
#endif
/*---------------------------------------------------------------------------*/
struct sb_queue
{
	index_type _capacity;
	volatile index_type _head;
	volatile index_type _tail;

	struct tcp_send_buffer * volatile * _q;
};
/*----------------------------------------------------------------------------*/
static inline index_type 
NextIndex(sb_queue_t sq, index_type i)
{
	return (i != sq->_capacity ? i + 1: 0);
}
/*---------------------------------------------------------------------------*/
static inline index_type 
PrevIndex(sb_queue_t sq, index_type i)
{
	return (i != 0 ? i - 1: sq->_capacity);
}
/*---------------------------------------------------------------------------*/
static inline void 
SBMemoryBarrier(struct tcp_send_buffer * volatile buf, volatile index_type index)
{
	__asm__ volatile("" : : "m" (buf), "m" (index));
}
/*---------------------------------------------------------------------------*/
sb_queue_t 
CreateSBQueue(int capacity)
{
	sb_queue_t sq;

	sq = (sb_queue_t)calloc(1, sizeof(struct sb_queue));
	if (!sq)
		return NULL;

	sq->_q = (struct tcp_send_buffer **)
			calloc(capacity + 1, sizeof(struct tcp_send_buffer *));
	if (!sq->_q) {
		free(sq);
		return NULL;
	}

	sq->_capacity = capacity;
	sq->_head = sq->_tail = 0;

	return sq;
}
/*---------------------------------------------------------------------------*/
void 
DestroySBQueue(sb_queue_t sq)
{
	if (!sq)
		return;

	if (sq->_q) {
		free((void *)sq->_q);
		sq->_q = NULL;
	}

	free(sq);
}
/*---------------------------------------------------------------------------*/
int 
SBEnqueue(sb_queue_t sq, struct tcp_send_buffer *buf)
{
	index_type h = sq->_head;
	index_type t = sq->_tail;
	index_type nt = NextIndex(sq, t);

	if (nt != h) {
		sq->_q[t] = buf;
		SBMemoryBarrier(sq->_q[t], sq->_tail);
		sq->_tail = nt;
		return 0;
	}

	TRACE_ERROR("Exceed capacity of buf queue!\n");
	return -1;
}
/*---------------------------------------------------------------------------*/
struct tcp_send_buffer *
SBDequeue(sb_queue_t sq)
{
	index_type h = sq->_head;
	index_type t = sq->_tail;

	if (h != t) {
		struct tcp_send_buffer *buf = sq->_q[h];
		SBMemoryBarrier(sq->_q[h], sq->_head);
		sq->_head = NextIndex(sq, h);
		assert(buf);

		return buf;
	}

	return NULL;
}
/*---------------------------------------------------------------------------*/
