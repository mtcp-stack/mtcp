/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/spinlock_recursive.h>
#include <odp/thread.h>

#define NO_OWNER (-1)

void odp_spinlock_recursive_init(odp_spinlock_recursive_t *rlock)
{
	odp_spinlock_init(&rlock->lock);
	rlock->owner = NO_OWNER;
	rlock->cnt   = 0;
}

void odp_spinlock_recursive_lock(odp_spinlock_recursive_t *rlock)
{
	int thr = odp_thread_id();

	if (rlock->owner == thr) {
		rlock->cnt++;
		return;
	}

	odp_spinlock_lock(&rlock->lock);
	rlock->owner = thr;
	rlock->cnt   = 1;
}

int odp_spinlock_recursive_trylock(odp_spinlock_recursive_t *rlock)
{
	int thr = odp_thread_id();

	if (rlock->owner == thr) {
		rlock->cnt++;
		return 1;
	}

	if (odp_spinlock_trylock(&rlock->lock)) {
		rlock->owner = thr;
		rlock->cnt   = 1;
		return 1;
	} else {
		return 0;
	}
}

void odp_spinlock_recursive_unlock(odp_spinlock_recursive_t *rlock)
{
	rlock->cnt--;

	if (rlock->cnt > 0)
		return;

	rlock->owner = NO_OWNER;
	odp_spinlock_unlock(&rlock->lock);
}

int odp_spinlock_recursive_is_locked(odp_spinlock_recursive_t *rlock)
{
	int thr = odp_thread_id();

	if (rlock->owner == thr)
		return 1;

	return odp_spinlock_is_locked(&rlock->lock);
}
