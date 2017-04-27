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

#ifndef _ODP_TIMER_H_
#define _ODP_TIMER_H_

/**
 * @file
   ODP Timer
 *
 * This library provides a timer service to ODP Data Plane execution
 * units that allows the execution of callback functions asynchronously.
 *
 * - Timers can be periodic or single (one-shot).
 * - The timers can be loaded from one core and executed on another. This has
 *   to be specified in the call to odp_hisi_timer_reset().
 * - High precision is possible. NOTE: this depends on the call frequency to
 *   odp_hisi_timer_manage() that check the timer expiration for the local core.
 * - If not used in an application, for improved performance, it can be
 *   disabled at compilation time by not calling the odp_hisi_timer_manage()
 *   to improve performance.
 *
 * The timer library uses the odp_get_hpet_cycles() function that
 * uses the HPET, when available, to provide a reliable time reference. [HPET
 * routines are provided by ODP, which falls back to using the chip TSC (time-
 * stamp counter) as fallback when HPET is not available]
 *
 * This library provides an interface to add, delete and restart a
 * timer. The API is based on the BSD callout(9) API with a few
 * differences.
 *
 * See the ODP architecture documentation for more information about the
 * design of this library.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ODP_HISI_TIMER_STOP    0        /**< State: timer is stopped. */
#define ODP_HISI_TIMER_PENDING 1        /**< State: timer is scheduled. */
#define ODP_HISI_TIMER_RUNNING 2        /**< State: timer function is running. */
#define ODP_HISI_TIMER_CONFIG  3        /**< State: timer is being configured. */

#define ODP_HISI_TIMER_NO_OWNER -2      /**< Timer has no owner. */

/**
 * Timer type: Periodic or single (one-shot).
 */
enum odp_hisi_timer_type {
	SINGLE,
	PERIODICAL
};

/**
 * Timer status: A union of the state (stopped, pending, running,
 * config) and an owner (the id of the core that owns the timer).
 */
union odp_hisi_timer_status {
	struct {
		uint16_t state; /**< Stop, pending, running, config. */
		int16_t	 owner; /**< The core that owns the timer. */
	};
	uint32_t u32;           /**< To atomic-set status + owner. */
};

struct odp_hisi_timer;

/**
 * Callback function type for timer expiry.
 */
typedef void (*odp_timer_cb_t)(struct odp_hisi_timer *, void *);

#define MAX_SKIPLIST_DEPTH 10

/**
 * A structure describing a timer in ODP.
 */
struct odp_hisi_timer {
	uint64_t		    expire; /**< Time when timer expire. */
	struct odp_hisi_timer	   *sl_next[MAX_SKIPLIST_DEPTH];
	union odp_hisi_timer_status status; /**< Status of timer. */
	uint64_t		    period; /**< Period of timer (0 if not periodic). */
	odp_timer_cb_t		    f;      /**< Callback function. */
	void			   *arg;    /**< Argument to callback function. */
};

#ifdef __cplusplus

/**
 * A C++ static initializer for a timer structure.
 */
#define ODP_HISI_TIMER_INITIALIZER {             \
		0,                                      \
		{NULL},                                 \
		{ {ODP_HISI_TIMER_STOP, ODP_HISI_TIMER_NO_OWNER} }, \
		0,                                      \
		NULL,                                   \
		NULL,                                   \
}
#else

/**
 * A static initializer for a timer structure.
 */
#define ODP_HISI_TIMER_INITIALIZER {                      \
		.status = { {                         \
				    .state = ODP_HISI_TIMER_STOP,     \
				    .owner = ODP_HISI_TIMER_NO_OWNER, \
			    } },                                  \
}
#endif

/**
 * Initialize the timer library.
 *
 * Initializes internal variables (list, locks and so on) for the ODP
 * timer library.
 */
void odp_hisi_timer_subsystem_init(void);

/**
 * Initialize a timer handle.
 *
 * The odp_hisi_timer_init() function initializes the timer handle *tim*
 * for use. No operations can be performed on a timer before it is
 * initialized.
 *
 * @param tim
 *   The timer to initialize.
 */
void odp_hisi_timer_init(struct odp_hisi_timer *tim);

/**
 * Reset and start the timer associated with the timer handle.
 *
 * The odp_hisi_timer_reset() function resets and starts the timer
 * associated with the timer handle *tim*. When the timer expires after
 * *ticks* HPET cycles, the function specified by *fct* will be called
 * with the argument *arg* on core *tim_core*.
 *
 * If the timer associated with the timer handle is already running
 * (in the RUNNING state), the function will fail. The user has to check
 * the return value of the function to see if there is a chance that the
 * timer is in the RUNNING state.
 *
 * If the timer is being configured on another core (the CONFIG state),
 * it will also fail.
 *
 * If the timer is pending or stopped, it will be rescheduled with the
 * new parameters.
 *
 * @param tim
 *   The timer handle.
 * @param ticks
 *   The number of cycles (see odp_get_hpet_hz()) before the callback
 *   function is called.
 * @param type
 *   The type can be either:
 *   - PERIODICAL: The timer is automatically reloaded after execution
 *     (returns to the PENDING state)
 *   - SINGLE: The timer is one-shot, that is, the timer goes to a
 *     STOPPED state after execution.
 * @param tim_core
 *   The ID of the core where the timer callback function has to be
 *   executed. If tim_core is CORE_ID_ANY, the timer library will
 *   launch it on a different core for each call (round-robin).
 * @param fct
 *   The callback function of the timer.
 * @param arg
 *   The user argument of the callback function.
 * @return
 *   - 0: Success; the timer is scheduled.
 *   - (-1): Timer is in the RUNNING or CONFIG state.
 */
int odp_hisi_timer_reset(struct odp_hisi_timer *tim, uint64_t ticks,
			 enum odp_hisi_timer_type type, unsigned tim_core,
			 odp_timer_cb_t fct, void *arg);

/**
 * Loop until odp_hisi_timer_reset() succeeds.
 *
 * Reset and start the timer associated with the timer handle. Always
 * succeed. See odp_hisi_timer_reset() for details.
 *
 * @param tim
 *   The timer handle.
 * @param ticks
 *   The number of cycles (see odp_get_hpet_hz()) before the callback
 *   function is called.
 * @param type
 *   The type can be either:
 *   - PERIODICAL: The timer is automatically reloaded after execution
 *     (returns to the PENDING state)
 *   - SINGLE: The timer is one-shot, that is, the timer goes to a
 *     STOPPED state after execution.
 * @param tim_core
 *   The ID of the core where the timer callback function has to be
 *   executed. If tim_core is CORE_ID_ANY, the timer library will
 *   launch it on a different core for each call (round-robin).
 * @param fct
 *   The callback function of the timer.
 * @param arg
 *   The user argument of the callback function.
 */
void
odp_hisi_timer_reset_sync(struct odp_hisi_timer *tim, uint64_t ticks,
			  enum odp_hisi_timer_type type, unsigned tim_core,
			  odp_timer_cb_t fct, void *arg);

/**
 * Stop a timer.
 *
 * The odp_hisi_timer_stop() function stops the timer associated with the
 * timer handle *tim*. It may fail if the timer is currently running or
 * being configured.
 *
 * If the timer is pending or stopped (for instance, already expired),
 * the function will succeed. The timer handle tim must have been
 * initialized using odp_hisi_timer_init(), otherwise, undefined behavior
 * will occur.
 *
 * This function can be called safely from a timer callback. If it
 * succeeds, the timer is not referenced anymore by the timer library
 * and the timer structure can be freed (even in the callback
 * function).
 *
 * @param tim
 *   The timer handle.
 * @return
 *   - 0: Success; the timer is stopped.
 *   - (-1): The timer is in the RUNNING or CONFIG state.
 */
int odp_hisi_timer_stop(struct odp_hisi_timer *tim);

/**
 * Loop until odp_hisi_timer_stop() succeeds.
 *
 * After a call to this function, the timer identified by *tim* is
 * stopped. See odp_hisi_timer_stop() for details.
 *
 * @param tim
 *   The timer handle.
 */
void odp_hisi_timer_stop_sync(struct odp_hisi_timer *tim);

/**
 * Test if a timer is pending.
 *
 * The odp_hisi_timer_pending() function tests the PENDING status
 * of the timer handle *tim*. A PENDING timer is one that has been
 * scheduled and whose function has not yet been called.
 *
 * @param tim
 *   The timer handle.
 * @return
 *   - 0: The timer is not pending.
 *   - 1: The timer is pending.
 */
int odp_hisi_timer_pending(struct odp_hisi_timer *tim);

/**
 * Manage the timer list and execute callback functions.
 *
 * This function must be called periodically from ODP cores
 * main_loop(). It browses the list of pending timers and runs all
 * timers that are expired.
 *
 * The precision of the timer depends on the call frequency of this
 * function. However, the more often the function is called, the more
 * CPU resources it will use.
 */
void odp_hisi_timer_manage(void);

/**
 * Dump statistics about timers.
 *
 * @param f
 *   A pointer to a file for output
 */
void odp_hisi_timer_dump_stats(FILE *f);

#ifdef __cplusplus
}
#endif
#endif /* _ODP_TIMER_H_ */
