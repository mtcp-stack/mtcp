/* 
 * TCP stream queue - tcp_stream_queue.c/h
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

#include <stdio.h>
#include <stdlib.h>

#include "tcp_stream_queue.h"
#include "debug.h"

#ifndef _INDEX_TYPE_
#define _INDEX_TYPE_
typedef uint32_t index_type;
typedef int32_t signed_index_type;
#endif
/*---------------------------------------------------------------------------*/
struct stream_queue
{
	index_type _capacity;
	volatile index_type _head;
	volatile index_type _tail;

	struct tcp_stream * volatile * _q;
};
/*----------------------------------------------------------------------------*/
stream_queue_int * 
CreateInternalStreamQueue(int size)
{
	stream_queue_int *sq;

	sq = (stream_queue_int *)calloc(1, sizeof(stream_queue_int));
	if (!sq) {
		return NULL;
	}

	sq->array = (tcp_stream **)calloc(size, sizeof(tcp_stream *));
	if (!sq->array) {
		free(sq);
		return NULL;
	}

	sq->size = size;
	sq->first = sq->last = 0;
	sq->count = 0;

	return sq;
}
/*----------------------------------------------------------------------------*/
void 
DestroyInternalStreamQueue(stream_queue_int *sq)
{
	if (!sq)
		return;
	
	if (sq->array) {
		free(sq->array);
		sq->array = NULL;
	}

	free(sq);
}
/*----------------------------------------------------------------------------*/
int 
StreamInternalEnqueue(stream_queue_int *sq, struct tcp_stream *stream)
{
	if (sq->count >= sq->size) {
		/* queue is full */
		TRACE_INFO("[WARNING] Queue overflow. Set larger queue size! "
				"count: %d, size: %d\n", sq->count, sq->size);
		return -1;
	}

	sq->array[sq->last++] = stream;
	sq->count++;
	if (sq->last >= sq->size) {
		sq->last = 0;
	}
	assert (sq->count <= sq->size);

	return 0;
}
/*----------------------------------------------------------------------------*/
struct tcp_stream *
StreamInternalDequeue(stream_queue_int *sq)
{
	struct tcp_stream *stream = NULL;

	if (sq->count <= 0) {
		return NULL;
	}

	stream = sq->array[sq->first++];
	assert(stream != NULL);
	if (sq->first >= sq->size) {
		sq->first = 0;
	}
	sq->count--;
	assert(sq->count >= 0);

	return stream;
}
/*---------------------------------------------------------------------------*/
static inline index_type 
NextIndex(stream_queue_t sq, index_type i)
{
	return (i != sq->_capacity ? i + 1: 0);
}
/*---------------------------------------------------------------------------*/
static inline index_type 
PrevIndex(stream_queue_t sq, index_type i)
{
	return (i != 0 ? i - 1: sq->_capacity);
}
/*---------------------------------------------------------------------------*/
int 
StreamQueueIsEmpty(stream_queue_t sq)
{
	return (sq->_head == sq->_tail);
}
/*---------------------------------------------------------------------------*/
static inline void 
StreamMemoryBarrier(tcp_stream * volatile stream, volatile index_type index)
{
	__asm__ volatile("" : : "m" (stream), "m" (index));
}
/*---------------------------------------------------------------------------*/
stream_queue_t 
CreateStreamQueue(int capacity)
{
	stream_queue_t sq;

	sq = (stream_queue_t)calloc(1, sizeof(struct stream_queue));
	if (!sq)
		return NULL;

	sq->_q = (tcp_stream **)calloc(capacity + 1, sizeof(tcp_stream *));
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
DestroyStreamQueue(stream_queue_t sq)
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
StreamEnqueue(stream_queue_t sq, tcp_stream *stream)
{
	index_type h = sq->_head;
	index_type t = sq->_tail;
	index_type nt = NextIndex(sq, t);

	if (nt != h) {
		sq->_q[t] = stream;
		StreamMemoryBarrier(sq->_q[t], sq->_tail);
		sq->_tail = nt;
		return 0;
	}

	TRACE_ERROR("Exceed capacity of stream queue!\n");
	return -1;
}
/*---------------------------------------------------------------------------*/
tcp_stream *
StreamDequeue(stream_queue_t sq)
{
	index_type h = sq->_head;
	index_type t = sq->_tail;

	if (h != t) {
		tcp_stream *stream = sq->_q[h];
		StreamMemoryBarrier(sq->_q[h], sq->_head);
		sq->_head = NextIndex(sq, h);
		assert(stream);
		return stream;
	}

	return NULL;
}
/*---------------------------------------------------------------------------*/
