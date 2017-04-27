/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP packet IO - implementation internal
 */

#ifndef ODP_PACKET_IO_QUEUE_H_
#define ODP_PACKET_IO_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_queue_internal.h>
#include <odp_buffer_internal.h>

/** Max nbr of pkts to receive in one burst (keep same as QUEUE_MULTI_MAX) */
#define ODP_PKTIN_QUEUE_MAX_BURST 16
/* pktin_deq_multi() depends on the condition: */
_ODP_STATIC_ASSERT(ODP_PKTIN_QUEUE_MAX_BURST >= QUEUE_MULTI_MAX,
		   "ODP_PKTIN_DEQ_MULTI_MAX_ERROR");

int pktin_enqueue(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr, int sustain);
odp_buffer_hdr_t *pktin_dequeue(queue_entry_t *queue);

int pktin_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[], int num,
		    int sustain);
int pktin_deq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[], int num);


int pktout_enqueue(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr);
odp_buffer_hdr_t *pktout_dequeue(queue_entry_t *queue);

int pktout_enq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[],
		     int num);
int pktout_deq_multi(queue_entry_t *queue, odp_buffer_hdr_t *buf_hdr[],
		     int num);

#ifdef __cplusplus
}
#endif

#endif
