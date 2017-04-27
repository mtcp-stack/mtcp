/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Huawei Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Huawei Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Derived from FreeBSD's bufring.h
 *
 **************************************************************************
 *
 * Copyright (c) 2007-2009 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. The name of Kip Macy nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

#ifndef _ODP_RING_H_
#define _ODP_RING_H_

/**
 * @file
 * ODP Ring
 *
 * The Ring Manager is a fixed-size queue, implemented as a table of
 * pointers. Head and tail pointers are modified atomically, allowing
 * concurrent access to it. It has the following features:
 *
 * - FIFO (First In First Out)
 * - Maximum size is fixed; the pointers are stored in a table.
 * - Lockless implementation.
 * - Multi- or single-consumer dequeue.
 * - Multi- or single-producer enqueue.
 * - Bulk dequeue.
 * - Bulk enqueue.
 *
 * Note: the ring implementation is not preemptable. A core must not
 * be interrupted by another task that uses the same ring.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>
#include <errno.h>
#include <odp_common.h>
#include <odp_base.h>
#include <odp_memory.h>
#include <odp_core.h>
#include <odp/atomic.h>
#include <odp/hints.h>
#include <odp_hisi_atomic.h>

#define ODP_TAILQ_RING_NAME "ODP_RING"

#define odp_mem_barrier() /* added by huwei only for finishiing compiling */

enum odp_ring_queue_behavior {
	/* Enq/Deq a fixed number of items from a ring */
	ODP_RING_QUEUE_FIXED = 0,

	/* Enq/Deq as many items a possible from ring */
	ODP_RING_QUEUE_VARIABLE
};

#ifdef ODP_LIBHODP_RING_DEBUG

/**
 * A structure that stores the ring statistics (per-core).
 */
struct odp_ring_debug_stats {
	uint64_t enq_success_bulk; /**< Successful enqueues number. */
	uint64_t enq_success_objs; /**< Objects successfully enqueued. */
	uint64_t enq_quota_bulk;   /**< Successful enqueues above watermark. */
	uint64_t enq_quota_objs;   /**< Objects enqueued above watermark. */
	uint64_t enq_fail_bulk;    /**< Failed enqueues number. */
	uint64_t enq_fail_objs;    /**< Objects that failed to be enqueued. */
	uint64_t deq_success_bulk; /**< Successful dequeues number. */
	uint64_t deq_success_objs; /**< Objects successfully dequeued. */
	uint64_t deq_fail_bulk;    /**< Failed dequeues number. */
	uint64_t deq_fail_objs;    /**< Objects that failed to be dequeued. */
} __odp_cache_aligned;
#endif

#define ODP_RING_NAMESIZE  32     /**< The maximum length of a ring name. */
#define ODP_RING_MZ_PREFIX "RG_"

#ifndef ODP_RING_PAUSE_REP_COUNT

/**< Yield after pause num of times, no yield
 *   if HODP_RING_PAUSE_REP not defined. */
#define ODP_RING_PAUSE_REP_COUNT 0
#endif

/**
 * An ODP ring structure.
 *
 * The producer and the consumer have a head and a tail index. The particularity
 * of these index is that they are not between 0 and size(ring). These indexes
 * are between 0 and 2^32, and we mask their value when we access the ring[]
 * field. Thanks to this assumption, we can do subtractions between 2 index
 * values in a modulo-32bit base: that's why the overflow of the indexes is not
 * a problem.
 */
struct odp_ring {
	char name[ODP_RING_NAMESIZE];  /**< Name of the ring. */
	int  flags;                    /**< Flags supplied at creation. */

	/** Ring producer status. */
	struct prod {
		uint32_t watermark;  /**< Maximum items before EDQUOT. */
		uint32_t sp_enqueue; /**< True, if single producer. */
		uint32_t size;       /**< Size of ring. */
		uint32_t mask;       /**< Mask (size-1) of ring. */
		uint32_t head;       /**< Producer head. */
		uint32_t tail;       /**< Producer tail. */
	} prod __odp_cache_aligned;

	/** Ring consumer status. */
	struct cons {
		uint32_t sc_dequeue; /**< True, if single consumer. */
		uint32_t size;       /**< Size of the ring. */
		uint32_t mask;       /**< Mask (size-1) of ring. */
		uint32_t head;       /**< Consumer head. */
		uint32_t tail;       /**< Consumer tail. */

#ifdef ODP_RING_SPLIT_PROD_CONS
	} cons __odp_cache_aligned;
#else
	} cons;
#endif

#ifdef ODP_LIBHODP_RING_DEBUG
	struct odp_ring_debug_stats stats[ODP_MAX_CORE];
#endif

	/* Memory space of ring starts here.
	 * not volatile so need to be careful
	 * about compiler re-ordering */
	void *ring[0] __odp_cache_aligned;
};

#ifndef BIT_ULL
#define BIT_ULL(nr) (1ULL << (nr))
#endif

/**< The default enqueue is "single-producer". */
#define RING_F_SP_ENQ 0x0001

/**< The default dequeue is "single-consumer". */
#define RING_F_SC_DEQ 0x0002

/**< Quota exceed for burst ops */
#define ODP_RING_QUOT_EXCEED BIT_ULL(31)
#define ODP_RING_SZ_MASK     (unsigned)(0x0fffffff) /**< Ring size mask */

/**
 * @internal When debug is enabled, store ring statistics.
 * @param r
 *   A pointer to the ring.
 * @param name
 *   The name of the statistics field to increment in the ring.
 * @param n
 *   The number to add to the object-oriented statistics.
 */
#ifdef ODP_LIBHODP_RING_DEBUG
#define __RING_STAT_ADD(r, name, n) do {                        \
		unsigned __core_id = odp_core_id();           \
		if (__core_id < ODP_MAX_CORE) {               \
			r->stats[__core_id].name ## _objs += n;  \
			r->stats[__core_id].name ## _bulk += 1;  \
		}                                               \
} while (0)
#else
#define __RING_STAT_ADD(r, name, n) do {} while (0)
#endif

/**
 * Calculate the memory size needed for a ring
 *
 * This function returns the number of bytes needed for a ring, given
 * the number of elements in it. This value is the sum of the size of
 * the structure odp_ring and the size of the memory needed by the
 * objects pointers. The value is aligned to a cache line size.
 *
 * @param count
 *   The number of elements in the ring (must be a power of 2).
 * @return
 *   - The memory size needed for the ring on success.
 *   - -EINVAL if count is not a power of 2.
 */
ssize_t odp_ring_get_memsize(unsigned count);

/**
 * Initialize a ring structure.
 *
 * Initialize a ring structure in memory pointed by "r". The size of the
 * memory area must be large enough to store the ring structure and the
 * object table. It is advised to use odp_ring_get_memsize() to get the
 * appropriate size.
 *
 * The ring size is set to *count*, which must be a power of two. Water
 * marking is disabled by default. The real usable ring size is
 * *count-1* instead of *count* to differentiate a free ring from an
 * empty ring.
 *
 * The ring is not added in HODP_TAILQ_RING global list. Indeed, the
 * memory given by the caller may not be shareable among dpdk
 * processes.
 *
 * @param r
 *   The pointer to the ring structure followed by the objects table.
 * @param name
 *   The name of the ring.
 * @param count
 *   The number of elements in the ring (must be a power of 2).
 * @param flags
 *   An OR of the following:
 *    - RING_F_SP_ENQ: If this flag is set, the default behavior when
 *      using ``odp_ring_enqueue()`` or ``odp_ring_enqueue_bulk()``
 *      is "single-producer". Otherwise, it is "multi-producers".
 *    - RING_F_SC_DEQ: If this flag is set, the default behavior when
 *      using ``odp_ring_dequeue()`` or ``odp_ring_dequeue_bulk()``
 *      is "single-consumer". Otherwise, it is "multi-consumers".
 * @return
 *   0 on success, or a negative value on error.
 */
int odp_ring_init(struct odp_ring *r, const char *name, unsigned count,
		  unsigned flags);

/**
 * Create a new ring named *name* in memory.
 *
 * This function uses ``mm_district_reserve()`` to allocate memory. Then it
 * calls odp_ring_init() to initialize an empty ring.
 *
 * The new ring size is set to *count*, which must be a power of
 * two. Water marking is disabled by default. The real usable ring size
 * is *count-1* instead of *count* to differentiate a free ring from an
 * empty ring.
 *
 * The ring is added in HODP_TAILQ_RING list.
 *
 * @param name
 *   The name of the ring.
 * @param count
 *   The size of the ring (must be a power of 2).
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   An OR of the following:
 *    - RING_F_SP_ENQ: If this flag is set, the default behavior when
 *      using ``odp_ring_enqueue()`` or ``odp_ring_enqueue_bulk()``
 *      is "single-producer". Otherwise, it is "multi-producers".
 *    - RING_F_SC_DEQ: If this flag is set, the default behavior when
 *      using ``odp_ring_dequeue()`` or ``odp_ring_dequeue_bulk()``
 *      is "single-consumer". Otherwise, it is "multi-consumers".
 * @return
 *   On success, the pointer to the new allocated ring. NULL on error with
 *    odp_err set appropriately. Possible errno values include:
 *    - E_ODP_NO_CONFIG - function could not get pointer to config structure
 *    - E_ODP_SECONDARY - function was called from secondary process instance
 *    - EINVAL - count provided is not a power of 2
 *    - ENOSPC - the maximum number of mm_districts has already been allocated
 *    - EEXIST - a mm_district with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to createdistrict
 */
struct odp_ring *odp_ring_create(const char *name, unsigned count,
				 int socket_id, unsigned flags);

/**
 * Change the high water mark.
 *
 * If *count* is 0, water marking is disabled. Otherwise, it is set to the
 * *count* value. The *count* value must be greater than 0 and less
 * than the ring size.
 *
 * This function can be called at any time (not necessarily at
 * initialization).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param count
 *   The new water mark value.
 * @return
 *   - 0: Success; water mark changed.
 *   - -EINVAL: Invalid water mark value.
 */
int odp_ring_set_water_mark(struct odp_ring *r, unsigned count);

/**
 * Dump the status of the ring to the console.
 *
 * @param f
 *   A pointer to a file for output
 * @param r
 *   A pointer to the ring structure.
 */
void odp_ring_dump(FILE *f, const struct odp_ring *r);

/* the actual enqueue of pointers on the ring.
 * Placed here since identical code needed in both
 * single and multi producer enqueue functions */
#define ENQUEUE_PTRS() do { \
		const uint32_t size = r->prod.size; \
		uint32_t idx = prod_head & mask; \
		if (odp_likely(idx + n < size)) { \
			for (i = 0; i < (n & ((~(unsigned)0x3))); \
			     i += 4, idx += 4) {    \
				r->ring[idx] = obj_table[i]; \
				r->ring[idx + 1] = obj_table[i + 1]; \
				r->ring[idx + 2] = obj_table[i + 2]; \
				r->ring[idx + 3] = obj_table[i + 3]; \
			} \
			switch (n & 0x3) { \
			case 3: \
				r->ring[idx++] = obj_table[i++]; \
			case 2: \
				r->ring[idx++] = obj_table[i++]; \
			case 1: \
				r->ring[idx++] = obj_table[i++]; \
			} \
		} else { \
			for (i = 0; idx < size; i++, idx++) \
				r->ring[idx] = obj_table[i]; \
			for (idx = 0; i < n; i++, idx++) \
				r->ring[idx] = obj_table[i]; \
		} \
} while (0)

/* the actual copy of pointers on the ring to obj_table.
 * Placed here since identical code needed in both
 * single and multi consumer dequeue functions */
#define DEQUEUE_PTRS() do { \
		uint32_t idx = cons_head & mask; \
		const uint32_t size = r->cons.size; \
		if (odp_likely(idx + n < size)) { \
			for (i = 0; i < (n & (~(unsigned)0x3)); \
			     i += 4, idx += 4) {    \
				obj_table[i] = r->ring[idx]; \
				obj_table[i + 1] = r->ring[idx + 1]; \
				obj_table[i + 2] = r->ring[idx + 2]; \
				obj_table[i + 3] = r->ring[idx + 3]; \
			} \
			switch (n & 0x3) { \
			case 3: \
				obj_table[i++] = r->ring[idx++]; \
			case 2: \
				obj_table[i++] = r->ring[idx++]; \
			case 1: \
				obj_table[i++] = r->ring[idx++]; \
			} \
		} else { \
			for (i = 0; idx < size; i++, idx++) \
				obj_table[i] = r->ring[idx]; \
			for (idx = 0; i < n; i++, idx++) \
				obj_table[i] = r->ring[idx]; \
		} \
} while (0)

/**
 * @internal Enqueue several objects on the ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param behavior
 *   ODP_RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   ODP_RING_QUEUE_VARIABLE: Enqueue as many items a possible from ring
 * @return
 *   Depend on the behavior value
 *   if behavior = ODP_RING_QUEUE_FIXED
 *   - 0: Success; objects enqueue.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue, no object is enqueued.
 *   if behavior = ODP_RING_QUEUE_VARIABLE
 *   - n: Actual number of objects enqueued.
 */
static inline int __attribute__((always_inline)) __odp_ring_mp_do_enqueue(
	struct odp_ring *r, void *const *obj_table,
	unsigned n,
	enum odp_ring_queue_behavior behavior)
{
	uint32_t prod_head, prod_next;

	uint32_t cons_tail, free_entries;
	const unsigned max = n;
	int success;
	unsigned i, rep = 0;
	uint32_t mask = r->prod.mask;
	int ret;

	/* move prod.head atomically */
	do {
		/* Reset n to the initial burst count */
		n = max;

		prod_head = r->prod.head;
		cons_tail = r->cons.tail;

		/* The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * prod_head > cons_tail). So 'free_entries' is always between 0
		 * and size(ring)-1. */
		free_entries = (mask + cons_tail - prod_head);

		/* check that we have enough room in ring */
		if (odp_unlikely(n > free_entries)) {
			if (behavior == ODP_RING_QUEUE_FIXED) {
				__RING_STAT_ADD(r, enq_fail, n);
				return -ENOBUFS;
			}

			/* No free entry available */
			if (odp_unlikely(free_entries == 0)) {
				__RING_STAT_ADD(r, enq_fail, n);
				return 0;
			}

			n = free_entries;
		}

		prod_next = prod_head + n;
		success = odp_atomic_cmpset_u32_a64(
			(odp_atomic_u32_t *)&r->prod.head,
			prod_head,
			prod_next);
	} while (odp_unlikely(success == 0));

	/* write entries in ring */
	ENQUEUE_PTRS();
	odp_mem_barrier();

	/* if we exceed the watermark */
	if (odp_unlikely(((mask + 1) - free_entries + n) >
			 r->prod.watermark)) {
		ret = (behavior == ODP_RING_QUEUE_FIXED) ? -EDQUOT :
		      (int)(n | ODP_RING_QUOT_EXCEED);
		__RING_STAT_ADD(r, enq_quota, n);
	} else {
		ret = (behavior == ODP_RING_QUEUE_FIXED) ? 0 : n;
		__RING_STAT_ADD(r, enq_success, n);
	}

	/*
	 * If there are other enqueues in progress that preceded us,
	 * we need to wait for them to complete
	 */
	while (odp_unlikely(r->prod.tail != prod_head)) {
		odp_pause();

		/* Set ODP_RING_PAUSE_REP_COUNT to avoid spin too long waiting
		 * for other thread finish. It gives pre-empted thread a chance
		 * to proceed and finish with ring dequeue operation. */
		if (ODP_RING_PAUSE_REP_COUNT &&
		    (++rep == ODP_RING_PAUSE_REP_COUNT)) {
			rep = 0;
			sched_yield();
		}
	}

	r->prod.tail = prod_next;
	return ret;
}

/**
 * @internal Enqueue several objects on a ring (NOT multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param behavior
 *   ODP_RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   ODP_RING_QUEUE_VARIABLE: Enqueue as many items a possible from ring
 * @return
 *   Depend on the behavior value
 *   if behavior = ODP_RING_QUEUE_FIXED
 *   - 0: Success; objects enqueue.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue, no object is enqueued.
 *   if behavior = ODP_RING_QUEUE_VARIABLE
 *   - n: Actual number of objects enqueued.
 */
static inline int __attribute__((always_inline)) __odp_ring_sp_do_enqueue(
	struct odp_ring *r, void *const *obj_table,
	unsigned n,
	enum odp_ring_queue_behavior behavior)
{
	uint32_t prod_head, cons_tail;
	uint32_t prod_next, free_entries;
	unsigned i;
	uint32_t mask = r->prod.mask;
	int ret;

	prod_head = r->prod.head;
	cons_tail = r->cons.tail;

	/* The subtraction is done between two unsigned 32bits value
	 * (the result is always modulo 32 bits even if we have
	 * prod_head > cons_tail). So 'free_entries' is always between 0
	 * and size(ring)-1. */
	free_entries = mask + cons_tail - prod_head;

	/* check that we have enough room in ring */
	if (odp_unlikely(n > free_entries)) {
		if (behavior == ODP_RING_QUEUE_FIXED) {
			__RING_STAT_ADD(r, enq_fail, n);
			return -ENOBUFS;
		}

		/* No free entry available */
		if (odp_unlikely(free_entries == 0)) {
			__RING_STAT_ADD(r, enq_fail, n);
			return 0;
		}

		n = free_entries;
	}

	prod_next = prod_head + n;
	r->prod.head = prod_next;

	/* write entries in ring */
	ENQUEUE_PTRS();
	odp_mem_barrier();

	/* if we exceed the watermark */
	if (odp_unlikely(((mask + 1) - free_entries + n) >
			 r->prod.watermark)) {
		ret = (behavior == ODP_RING_QUEUE_FIXED) ? -EDQUOT :
		      (int)(n | ODP_RING_QUOT_EXCEED);
		__RING_STAT_ADD(r, enq_quota, n);
	} else {
		ret = (behavior == ODP_RING_QUEUE_FIXED) ? 0 : n;
		__RING_STAT_ADD(r, enq_success, n);
	}

	r->prod.tail = prod_next;
	return ret;
}

/**
 * @internal Dequeue several objects from a ring (multi-consumers safe). When
 * the request objects are more than the available objects, only dequeue the
 * actual number of objects
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param behavior
 *   ODP_RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   ODP_RING_QUEUE_VARIABLE: Dequeue as many items a possible from ring
 * @return
 *   Depend on the behavior value
 *   if behavior = ODP_RING_QUEUE_FIXED
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue; no object is
 *     dequeued.
 *   if behavior = ODP_RING_QUEUE_VARIABLE
 *   - n: Actual number of objects dequeued.
 */

static inline int __attribute__((always_inline)) __odp_ring_mc_do_dequeue(
	struct odp_ring *r, void **obj_table,
	unsigned n,
	enum odp_ring_queue_behavior behavior)
{
	uint32_t cons_head, prod_tail;
	uint32_t cons_next, entries;
	const unsigned max = n;
	int success;
	unsigned i, rep = 0;
	uint32_t mask = r->prod.mask;

	/* move cons.head atomically */
	do {
		/* Restore n as it may change every loop */
		n = max;

		cons_head = r->cons.head;
		prod_tail = r->prod.tail;

		/* The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * cons_head > prod_tail). So 'entries' is always between 0
		 * and size(ring)-1. */
		entries = (prod_tail - cons_head);

		/* Set the actual entries for dequeue */
		if (n > entries) {
			if (behavior == ODP_RING_QUEUE_FIXED) {
				__RING_STAT_ADD(r, deq_fail, n);
				return -ENOENT;
			}

			if (odp_unlikely(entries == 0)) {
				__RING_STAT_ADD(r, deq_fail, n);
				return 0;
			}

			n = entries;
		}

		cons_next = cons_head + n;
		success = odp_atomic_cmpset_u32_a64(
			(odp_atomic_u32_t *)&r->cons.head, cons_head,
			cons_next);
	} while (odp_unlikely(success == 0));

	/* copy in table */
	DEQUEUE_PTRS();
	odp_mem_barrier();

	/*
	 * If there are other dequeues in progress that preceded us,
	 * we need to wait for them to complete
	 */
	while (odp_unlikely(r->cons.tail != cons_head)) {
		odp_pause();

		/* Set ODP_RING_PAUSE_REP_COUNT to avoid spin too long waiting
		 * for other thread finish. It gives pre-empted thread a chance
		 * to proceed and finish with ring dequeue operation. */
		if (ODP_RING_PAUSE_REP_COUNT &&
		    (++rep == ODP_RING_PAUSE_REP_COUNT)) {
			rep = 0;
			sched_yield();
		}
	}

	__RING_STAT_ADD(r, deq_success, n);
	r->cons.tail = cons_next;

	return behavior == ODP_RING_QUEUE_FIXED ? 0 : n;
}

/**
 * @internal Dequeue several objects from a ring (NOT multi-consumers safe).
 * When the request objects are more than the available objects, only dequeue
 * the actual number of objects
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @param behavior
 *   ODP_RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   ODP_RING_QUEUE_VARIABLE: Dequeue as many items a possible from ring
 * @return
 *   Depend on the behavior value
 *   if behavior = ODP_RING_QUEUE_FIXED
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue; no object is
 *     dequeued.
 *   if behavior = ODP_RING_QUEUE_VARIABLE
 *   - n: Actual number of objects dequeued.
 */
static inline int __attribute__((always_inline)) __odp_ring_sc_do_dequeue(
	struct odp_ring *r, void **obj_table,
	unsigned n,
	enum odp_ring_queue_behavior behavior)
{
	uint32_t cons_head, prod_tail;
	uint32_t cons_next, entries;
	unsigned i;
	uint32_t mask = r->prod.mask;

	cons_head = r->cons.head;
	prod_tail = r->prod.tail;

	/* The subtraction is done between two unsigned 32bits value
	 * (the result is always modulo 32 bits even if we have
	 * cons_head > prod_tail). So 'entries' is always between 0
	 * and size(ring)-1. */
	entries = prod_tail - cons_head;

	if (n > entries) {
		if (behavior == ODP_RING_QUEUE_FIXED) {
			__RING_STAT_ADD(r, deq_fail, n);
			return -ENOENT;
		}

		if (odp_unlikely(entries == 0)) {
			__RING_STAT_ADD(r, deq_fail, n);
			return 0;
		}

		n = entries;
	}

	cons_next = cons_head + n;
	r->cons.head = cons_next;

	/* copy in table */
	DEQUEUE_PTRS();
	odp_mem_barrier();

	__RING_STAT_ADD(r, deq_success, n);
	r->cons.tail = cons_next;
	return behavior == ODP_RING_QUEUE_FIXED ? 0 : n;
}

/**
 * Enqueue several objects on the ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @return
 *   - 0: Success; objects enqueue.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue, no object is enqueued.
 */
static inline int __attribute__((always_inline)) odp_ring_mp_enqueue_bulk(
	struct odp_ring *r, void *const *obj_table,
	unsigned n)
{
	return __odp_ring_mp_do_enqueue(r, obj_table, n, ODP_RING_QUEUE_FIXED);
}

/**
 * Enqueue several objects on a ring (NOT multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static inline int __attribute__((always_inline)) odp_ring_sp_enqueue_bulk(
	struct odp_ring *r, void *const *obj_table,
	unsigned n)
{
	return __odp_ring_sp_do_enqueue(r, obj_table, n, ODP_RING_QUEUE_FIXED);
}

/**
 * Enqueue several objects on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static inline int __attribute__((always_inline)) odp_ring_enqueue_bulk(
	struct odp_ring *r, void *const *obj_table,
	unsigned n)
{
	if (r->prod.sp_enqueue)
		return odp_ring_sp_enqueue_bulk(r, obj_table, n);
	else
		return odp_ring_mp_enqueue_bulk(r, obj_table, n);
}

/**
 * Enqueue one object on a ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static inline int __attribute__((always_inline)) odp_ring_mp_enqueue(
	struct odp_ring *r, void *obj)
{
	return odp_ring_mp_enqueue_bulk(r, &obj, 1);
}

/**
 * Enqueue one object on a ring (NOT multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static inline int __attribute__((always_inline)) odp_ring_sp_enqueue(
	struct odp_ring *r, void *obj)
{
	return odp_ring_sp_enqueue_bulk(r, &obj, 1);
}

/**
 * Enqueue one object on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj
 *   A pointer to the object to be added.
 * @return
 *   - 0: Success; objects enqueued.
 *   - -EDQUOT: Quota exceeded. The objects have been enqueued, but the
 *     high water mark is exceeded.
 *   - -ENOBUFS: Not enough room in the ring to enqueue; no object is enqueued.
 */
static inline int __attribute__((always_inline)) odp_ring_enqueue(
	struct odp_ring *r, void *obj)
{
	if (r->prod.sp_enqueue)
		return odp_ring_sp_enqueue(r, obj);
	else
		return odp_ring_mp_enqueue(r, obj);
}

/**
 * Dequeue several objects from a ring (multi-consumers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue; no object is
 *     dequeued.
 */
static inline int __attribute__((always_inline)) odp_ring_mc_dequeue_bulk(
	struct odp_ring *r, void **obj_table,
	unsigned n)
{
	return __odp_ring_mc_do_dequeue(r, obj_table, n, ODP_RING_QUEUE_FIXED);
}

/**
 * Dequeue several objects from a ring (NOT multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table,
 *   must be strictly positive.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue; no object is
 *     dequeued.
 */
static inline int __attribute__((always_inline)) odp_ring_sc_dequeue_bulk(
	struct odp_ring *r, void **obj_table,
	unsigned n)
{
	return __odp_ring_sc_do_dequeue(r, obj_table, n, ODP_RING_QUEUE_FIXED);
}

/**
 * Dequeue several objects from a ring.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue, no object is
 *     dequeued.
 */
static inline int __attribute__((always_inline)) odp_ring_dequeue_bulk(
	struct odp_ring *r, void **obj_table,
	unsigned n)
{
	if (r->cons.sc_dequeue)
		return odp_ring_sc_dequeue_bulk(r, obj_table, n);
	else
		return odp_ring_mc_dequeue_bulk(r, obj_table, n);
}

/**
 * Dequeue one object from a ring (multi-consumers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue; no object is
 *     dequeued.
 */
static inline int __attribute__((always_inline)) odp_ring_mc_dequeue(
	struct odp_ring *r, void **obj_p)
{
	return odp_ring_mc_dequeue_bulk(r, obj_p, 1);
}

/**
 * Dequeue one object from a ring (NOT multi-consumers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success; objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue, no object is
 *     dequeued.
 */
static inline int __attribute__((always_inline)) odp_ring_sc_dequeue(
	struct odp_ring *r, void **obj_p)
{
	return odp_ring_sc_dequeue_bulk(r, obj_p, 1);
}

/**
 * Dequeue one object from a ring.
 *
 * This function calls the multi-consumers or the single-consumer
 * version depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_p
 *   A pointer to a void * pointer (object) that will be filled.
 * @return
 *   - 0: Success, objects dequeued.
 *   - -ENOENT: Not enough entries in the ring to dequeue, no object is
 *     dequeued.
 */
static inline int __attribute__((always_inline)) odp_ring_dequeue(
	struct odp_ring *r, void **obj_p)
{
	if (r->cons.sc_dequeue)
		return odp_ring_sc_dequeue(r, obj_p);
	else
		return odp_ring_mc_dequeue(r, obj_p);
}

/**
 * Test if a ring is full.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   - 1: The ring is full.
 *   - 0: The ring is not full.
 */
static inline int odp_ring_full(const struct odp_ring *r)
{
	uint32_t prod_tail = r->prod.tail;
	uint32_t cons_tail = r->cons.tail;

	return (((cons_tail - prod_tail - 1) & r->prod.mask) == 0);
}

/**
 * Test if a ring is empty.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   - 1: The ring is empty.
 *   - 0: The ring is not empty.
 */
static inline int odp_ring_empty(const struct odp_ring *r)
{
	uint32_t prod_tail = r->prod.tail;
	uint32_t cons_tail = r->cons.tail;

	return !!(cons_tail == prod_tail);
}

/**
 * Return the number of entries in a ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The number of entries in the ring.
 */
static inline unsigned odp_ring_count(const struct odp_ring *r)
{
	uint32_t prod_tail = r->prod.tail;
	uint32_t cons_tail = r->cons.tail;

	return ((prod_tail - cons_tail) & r->prod.mask);
}

/**
 * Return the number of free entries in a ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The number of free entries in the ring.
 */
static inline unsigned odp_ring_free_count(const struct odp_ring *r)
{
	uint32_t prod_tail = r->prod.tail;
	uint32_t cons_tail = r->cons.tail;

	return ((cons_tail - prod_tail - 1) & r->prod.mask);
}

/**
 * Dump the status of all rings on the console
 *
 * @param f
 *   A pointer to a file for output
 */
void odp_ring_list_dump(FILE *f);

/**
 * Search a ring from its name
 *
 * @param name
 *   The name of the ring.
 * @return
 *   The pointer to the ring matching the name, or NULL if not found,
 *   with odp_err set appropriately. Possible odp_err values include:
 *    - ENOENT - required entry not available to return.
 */
struct odp_ring *odp_ring_lookup(const char *name);

/**
 * Enqueue several objects on the ring (multi-producers safe).
 *
 * This function uses a "compare and set" instruction to move the
 * producer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static inline unsigned __attribute__((always_inline)) odp_ring_mp_enqueue_burst(
	struct odp_ring *r,
	void *const	*obj_table,
	unsigned	 n)
{
	return __odp_ring_mp_do_enqueue(r,
					obj_table, n, ODP_RING_QUEUE_VARIABLE);
}

/**
 * Enqueue several objects on a ring (NOT multi-producers safe).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static inline unsigned __attribute__((always_inline)) odp_ring_sp_enqueue_burst(
	struct odp_ring *r,
	void *const	*obj_table,
	unsigned	 n)
{
	return __odp_ring_sp_do_enqueue(r, obj_table,
					n, ODP_RING_QUEUE_VARIABLE);
}

/**
 * Enqueue several objects on a ring.
 *
 * This function calls the multi-producer or the single-producer
 * version depending on the default behavior that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects).
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @return
 *   - n: Actual number of objects enqueued.
 */
static inline unsigned __attribute__((always_inline)) odp_ring_enqueue_burst(
	struct odp_ring *r,
	void *const	*obj_table,
	unsigned	 n)
{
	if (r->prod.sp_enqueue)
		return odp_ring_sp_enqueue_burst(r, obj_table, n);
	else
		return odp_ring_mp_enqueue_burst(r, obj_table, n);
}

/**
 * Dequeue several objects from a ring (multi-consumers safe). When the request
 * objects are more than the available objects, only dequeue the actual number
 * of objects
 *
 * This function uses a "compare and set" instruction to move the
 * consumer index atomically.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @return
 *   - n: Actual number of objects dequeued, 0 if ring is empty
 */
static inline unsigned __attribute__((always_inline)) odp_ring_mc_dequeue_burst(
	struct odp_ring *r, void **obj_table,
	unsigned n)
{
	return __odp_ring_mc_do_dequeue(r, obj_table, n,
					ODP_RING_QUEUE_VARIABLE);
}

/**
 * Dequeue several objects from a ring (NOT multi-consumers safe).When the
 * request objects are more than the available objects, only dequeue the
 * actual number of objects
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @return
 *   - n: Actual number of objects dequeued, 0 if ring is empty
 */
static inline unsigned __attribute__((always_inline)) odp_ring_sc_dequeue_burst(
	struct odp_ring *r, void **obj_table,
	unsigned n)
{
	return __odp_ring_sc_do_dequeue(r, obj_table, n,
					ODP_RING_QUEUE_VARIABLE);
}

/**
 * Dequeue multiple objects from a ring up to a maximum number.
 *
 * This function calls the multi-consumers or the single-consumer
 * version, depending on the default behaviour that was specified at
 * ring creation time (see flags).
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of void * pointers (objects) that will be filled.
 * @param n
 *   The number of objects to dequeue from the ring to the obj_table.
 * @return
 *   - Number of objects dequeued
 */
static inline unsigned __attribute__((always_inline)) odp_ring_dequeue_burst(
	struct odp_ring *r, void **obj_table,
	unsigned n)
{
	if (r->cons.sc_dequeue)
		return odp_ring_sc_dequeue_burst(r, obj_table, n);
	else
		return odp_ring_mc_dequeue_burst(r, obj_table, n);
}

#ifdef __cplusplus
}
#endif
#endif /* _ODP_RING_H_ */
