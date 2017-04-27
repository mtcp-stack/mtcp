/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/ticketlock.h>
#include <odp/atomic.h>
#include <odp/sync.h>
#include <odp/cpu.h>

void odp_ticketlock_init(odp_ticketlock_t *ticketlock)
{
	odp_atomic_init_u32(&ticketlock->next_ticket, 0);
	odp_atomic_init_u32(&ticketlock->cur_ticket, 0);
}

void odp_ticketlock_lock(odp_ticketlock_t *ticketlock)
{
	uint32_t ticket;

	/* Take a ticket using an atomic increment of 'next_ticket'.
	 * This can be a relaxed operation but it cannot have the
	 * acquire semantics since we haven't acquired the lock yet */
	ticket = odp_atomic_fetch_inc_u32(&ticketlock->next_ticket);

	/* Spin waiting for our turn. Use load-acquire so that we acquire
	 * all stores from the previous lock owner */
	while (ticket != odp_atomic_load_acq_u32(&ticketlock->cur_ticket)) {
		__asm__ __volatile__ ("nop");
		__asm__ __volatile__ ("nop");
		__asm__ __volatile__ ("nop");
		__asm__ __volatile__ ("nop");
	}
}

int odp_ticketlock_trylock(odp_ticketlock_t *tklock)
{
	/* We read 'next_ticket' and 'cur_ticket' non-atomically which should
	 * not be a problem as they are not independent of each other.
	 * 'cur_ticket' is always <= to 'next_ticket' and if we see an
	 * older value of 'cur_ticket', this only means the lock will
	 * look busy and trylock will fail. */
	uint32_t next = odp_atomic_load_u32(&tklock->next_ticket);
	uint32_t cur = odp_atomic_load_u32(&tklock->cur_ticket);
	/* First check that lock is available and possible to take without
	 * spinning. */
	if (next == cur) {
		/* Then try to take the lock by incrementing 'next_ticket'
		 * but only if it still has the original value which is
		 * equal to 'cur_ticket'.
		 * We don't have to include 'cur_ticket' in the comparison
		 * because it cannot be larger than 'next_ticket' (only
		 * smaller if the lock is busy).
		 * If CAS fails, it means some other thread intercepted and
		 * took a ticket which means the lock is not available
		 * anymore */
		if (odp_atomic_cas_acq_u32(&tklock->next_ticket,
					   &next, next + 1))
			return 1;
	}
	return 0;
}

void odp_ticketlock_unlock(odp_ticketlock_t *ticketlock)
{
	/* Release the lock by incrementing 'cur_ticket'. As we are the
	 * lock owner and thus the only thread that is allowed to write
	 * 'cur_ticket', we don't need to do this with an (expensive)
	 * atomic RMW operation. Instead load-relaxed the current value
	 * and a store-release of the incremented value */
	uint32_t cur = odp_atomic_load_u32(&ticketlock->cur_ticket);

	odp_atomic_store_rel_u32(&ticketlock->cur_ticket, cur + 1);

#if defined __OCTEON__
	odp_sync_stores(); /* SYNCW to flush write buffer */
#endif
}

int odp_ticketlock_is_locked(odp_ticketlock_t *ticketlock)
{
	/* Compare 'cur_ticket' with 'next_ticket'. Ideally we should read
	 * both variables atomically but the information can become stale
	 * immediately anyway so the function can only be used reliably in
	 * a quiescent system where non-atomic loads should not pose a
	 * problem */
	return odp_atomic_load_u32(&ticketlock->cur_ticket) !=
		odp_atomic_load_u32(&ticketlock->next_ticket);
}
