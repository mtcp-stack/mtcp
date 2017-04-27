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
 * Derived from FreeBSD's bufring.c
 *
 **************************************************************************
 *
 * Copyright (c) 2007,2008 Kip Macy kmacy@freebsd.org
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <odp_common.h>
#include <odp/config.h>
#include <odp_memory.h>
#include <odp_mmdistrict.h>
#include <odp_base.h>
#include <odp_syslayout.h>
#include <odp/atomic.h>
#include <odp_core.h>
#include <odp/spinlock.h>
#include "odp_ring.h"
#include "odp_debug_internal.h"
TAILQ_HEAD(odp_ring_list, odp_tailq_entry);

static struct odp_tailq_elem odp_ring_tailq = {
	.name = ODP_TAILQ_RING_NAME,
};

ODP_REGISTER_TAILQ(odp_ring_tailq)

/* true if x is a power of 2 */
#define POWEROF2(x) ((((x) - 1) & (x)) == 0)

/* return the size of memory occupied by a ring */
ssize_t odp_ring_get_memsize(unsigned count)
{
	ssize_t sz;

	/* count must be a power of 2 */
	if ((!POWEROF2(count)) || (count > ODP_RING_SZ_MASK)) {
		ODP_PRINT("Requested size is invalid, must be power of 2, and "
			  "do not exceed the size limit %u\n",
			  ODP_RING_SZ_MASK);
		return -EINVAL;
	}

	sz = sizeof(struct odp_ring) + count * sizeof(void *);
	sz = ODP_ALIGN(sz, ODP_CACHE_LINE_SIZE);
	return sz;
}

int odp_ring_init(struct odp_ring *r, const char *name,
		  unsigned count, unsigned flags)
{
	/* compilation-time checks */
	ODP_BUILD_BUG_ON((sizeof(struct odp_ring) &
			  ODP_CACHE_LINE_MASK) != 0);
#ifdef ODP_RING_SPLIT_PROD_CONS
	ODP_BUILD_BUG_ON((offsetof(struct odp_ring, cons) &
			  ODP_CACHE_LINE_MASK) != 0);
#endif
	ODP_BUILD_BUG_ON((offsetof(struct odp_ring, prod) &
			  ODP_CACHE_LINE_MASK) != 0);
#ifdef ODP_LIBHODP_RING_DEBUG
	ODP_BUILD_BUG_ON((sizeof(struct odp_ring_debug_stats) &
			  ODP_CACHE_LINE_MASK) != 0);
	ODP_BUILD_BUG_ON((offsetof(struct odp_ring, stats) &
			  ODP_CACHE_LINE_MASK) != 0);
#endif

	/* init the ring structure */
	memset(r, 0, sizeof(*r));
	snprintf(r->name, sizeof(r->name), "%s", name);
	r->flags = flags;
	r->prod.watermark  = count;
	r->prod.sp_enqueue = !!(flags & RING_F_SP_ENQ);
	r->cons.sc_dequeue = !!(flags & RING_F_SC_DEQ);

	r->prod.size = count;
	r->prod.mask = count - 1;
	r->prod.head = 0;
	r->prod.tail = 0;

	r->cons.size = count;
	r->cons.mask = count - 1;
	r->cons.head = 0;
	r->cons.tail = 0;

	return 0;
}

/* create the ring */
struct odp_ring *odp_ring_create(const char *name, unsigned count,
				 int socket_id, unsigned flags)
{
	char mz_name[ODP_MEMZONE_NAMESIZE];
	struct odp_ring *r;
	struct odp_tailq_entry *te;
	const struct odp_mm_district *mz;
	ssize_t ring_size;
	int mz_flags = 0;
	struct odp_ring_list *ring_list = NULL;

	ring_list = ODP_TAILQ_CAST(odp_ring_tailq.head, odp_ring_list);

	ring_size = odp_ring_get_memsize(count);
	if (ring_size < 0) {
		odp_err = ring_size;
		return NULL;
	}

	te = malloc(sizeof(*te));
	if (te == NULL) {
		ODP_PRINT("Cannot reserve memory for tailq\n");
		odp_err = ENOMEM;
		return NULL;
	}

	snprintf(mz_name, sizeof(mz_name), "%s%s", ODP_RING_MZ_PREFIX, name);

	odp_rwlock_write_lock(ODP_TAILQ_RWLOCK);

	/* reserve a memory zone for this ring. If we can't get odp_config or
	 * we are secondary process, the mm_district_reserve function will set
	 * odp_err for us appropriately-hence no check in this this function */
	mz = odp_mm_district_reserve(mz_name, mz_name, ring_size,
				     socket_id, mz_flags);
	if (mz != NULL) {
		r = mz->addr;

		/* no need to check return value here, we already checked the
		 * arguments above */

		odp_ring_init(r, name, count, flags);

		te->data = (void *)r;

		TAILQ_INSERT_TAIL(ring_list, te, next);
	} else {
		r = NULL;
		ODP_PRINT("Cannot reserve memory\n");
		free(te);
	}

	odp_rwlock_write_unlock(ODP_TAILQ_RWLOCK);

	return r;
}

/*
 * change the high water mark. If *count* is 0, water marking is
 * disabled
 */
int odp_ring_set_water_mark(struct odp_ring *r, unsigned count)
{
	if (count >= r->prod.size)
		return -EINVAL;

	/* if count is 0, disable the watermarking */
	if (count == 0)
		count = r->prod.size;

	r->prod.watermark = count;
	return 0;
}

/* dump the status of the ring on the console */
void odp_ring_dump(FILE *f, const struct odp_ring *r)
{
#ifdef ODP_LIBHODP_RING_DEBUG
	struct odp_ring_debug_stats sum;
	unsigned core_id;
#endif

	fprintf(f, "ring <%s>@%p\n", r->name, r);
	fprintf(f, "  flags=%x\n", r->flags);
	fprintf(f, "  size=%d\n", r->prod.size);
	fprintf(f, "  ct=%d\n", r->cons.tail);
	fprintf(f, "  ch=%d\n", r->cons.head);
	fprintf(f, "  pt=%d\n", r->prod.tail);
	fprintf(f, "  ph=%d\n", r->prod.head);
	fprintf(f, "  used=%u\n", odp_ring_count(r));
	fprintf(f, "  avail=%u\n", odp_ring_free_count(r));
	if (r->prod.watermark == r->prod.size)
		fprintf(f, "  watermark=0\n");
	else
		fprintf(f, "  watermark=%d\n", r->prod.watermark);

	/* sum and dump statistics */
#ifdef ODP_LIBHODP_RING_DEBUG
	memset(&sum, 0, sizeof(sum));
	for (core_id = 0; core_id < ODP_MAX_CORE; core_id++) {
		sum.enq_success_bulk += r->stats[core_id].enq_success_bulk;
		sum.enq_success_objs += r->stats[core_id].enq_success_objs;
		sum.enq_quota_bulk += r->stats[core_id].enq_quota_bulk;
		sum.enq_quota_objs += r->stats[core_id].enq_quota_objs;
		sum.enq_fail_bulk  += r->stats[core_id].enq_fail_bulk;
		sum.enq_fail_objs  += r->stats[core_id].enq_fail_objs;
		sum.deq_success_bulk += r->stats[core_id].deq_success_bulk;
		sum.deq_success_objs += r->stats[core_id].deq_success_objs;
		sum.deq_fail_bulk += r->stats[core_id].deq_fail_bulk;
		sum.deq_fail_objs += r->stats[core_id].deq_fail_objs;
	}

	fprintf(f, "  size=%d\n", r->prod.size);
	fprintf(f, "  enq_success_bulk=%d\n", sum.enq_success_bulk);
	fprintf(f, "  enq_success_objs=%d\n", sum.enq_success_objs);
	fprintf(f, "  enq_quota_bulk=%d\n", sum.enq_quota_bulk);
	fprintf(f, "  enq_quota_objs=%d\n", sum.enq_quota_objs);
	fprintf(f, "  enq_fail_bulk=%d\n", sum.enq_fail_bulk);
	fprintf(f, "  enq_fail_objs=%d\n", sum.enq_fail_objs);
	fprintf(f, "  deq_success_bulk=%d\n", sum.deq_success_bulk);
	fprintf(f, "  deq_success_objs=%d\n", sum.deq_success_objs);
	fprintf(f, "  deq_fail_bulk=%d\n", sum.deq_fail_bulk);
	fprintf(f, "  deq_fail_objs=%d\n", sum.deq_fail_objs);
#else
	fprintf(f, "  no statistics available\n");
#endif
}

/* dump the status of all rings on the console */
void odp_ring_list_dump(FILE *f)
{
	const struct odp_tailq_entry *te;
	struct odp_ring_list *ring_list;

	ring_list = ODP_TAILQ_CAST(odp_ring_tailq.head, odp_ring_list);

	odp_rwlock_read_lock(ODP_TAILQ_RWLOCK);

	TAILQ_FOREACH(te, ring_list, next)
	{
		odp_ring_dump(f, (struct odp_ring *)te->data);
	}

	odp_rwlock_read_unlock(ODP_TAILQ_RWLOCK);
}

/* search a ring from its name */
struct odp_ring *odp_ring_lookup(const char *name)
{
	struct odp_tailq_entry *te;
	struct odp_ring *r = NULL;
	struct odp_ring_list   *ring_list;

	ring_list = ODP_TAILQ_CAST(odp_ring_tailq.head, odp_ring_list);

	odp_rwlock_read_lock(ODP_TAILQ_RWLOCK);

	TAILQ_FOREACH(te, ring_list, next)
	{
		r = (struct odp_ring *)te->data;
		if (strncmp(name, r->name, ODP_RING_NAMESIZE) == 0)
			break;
	}

	odp_rwlock_read_unlock(ODP_TAILQ_RWLOCK);

	if (te == NULL) {
		odp_err = ENOENT;
		return NULL;
	}

	return r;
}
