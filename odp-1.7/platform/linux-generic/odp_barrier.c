/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/barrier.h>
#include <odp/sync.h>
#include <odp/cpu.h>
#include <odp/atomic.h>

void odp_barrier_init(odp_barrier_t *barrier, int count)
{
	barrier->count = (uint32_t)count;
	odp_atomic_init_u32(&barrier->bar, 0);
}

/*
 * Efficient barrier_sync -
 *
 *   Barriers are initialized with a count of the number of callers
 *   that must sync on the barrier before any may proceed.
 *
 *   To avoid race conditions and to permit the barrier to be fully
 *   reusable, the barrier value cycles between 0..2*count-1. When
 *   synchronizing the wasless variable simply tracks which half of
 *   the cycle the barrier was in upon entry.  Exit is when the
 *   barrier crosses to the other half of the cycle.
 */
void odp_barrier_wait(odp_barrier_t *barrier)
{
	uint32_t count;
	int wasless;

	odp_mb_full();

	count   = odp_atomic_fetch_inc_u32(&barrier->bar);
	wasless = count < barrier->count;

	if (count == 2*barrier->count-1) {
		/* Wrap around *atomically* */
		odp_atomic_sub_u32(&barrier->bar, 2 * barrier->count);
	} else {
		while ((odp_atomic_load_u32(&barrier->bar) < barrier->count)
				== wasless)
			odp_cpu_pause();
	}

	odp_mb_full();
}
