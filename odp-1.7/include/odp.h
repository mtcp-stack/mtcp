/* Copyright (c) 2013, Linaro Limited
 * All rights reserved
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * The OpenDataPlane API
 *
 */

#ifndef ODP_H_
#define ODP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/config.h>

#include <odp/version.h>
#include <odp/std_types.h>
#include <odp/compiler.h>
#include <odp/align.h>
#include <odp/hash.h>
#include <odp/hints.h>
#include <odp/debug.h>
#include <odp/byteorder.h>
#include <odp/cpu.h>
#include <odp/cpumask.h>
#include <odp/barrier.h>
#include <odp/spinlock.h>
#include <odp/atomic.h>
#include <odp/init.h>
#include <odp/system_info.h>
#include <odp/thread.h>
#include <odp/shared_memory.h>
#include <odp/buffer.h>
#include <odp/pool.h>
#include <odp/queue.h>
#include <odp/ticketlock.h>
#include <odp/time.h>
#include <odp/timer.h>
#include <odp/schedule.h>
#include <odp/sync.h>
#include <odp/packet.h>
#include <odp/packet_flags.h>
#include <odp/packet_io.h>
#include <odp/crypto.h>
#include <odp/classification.h>
#include <odp/rwlock.h>
#include <odp/event.h>
#include <odp/random.h>
#include <odp/errno.h>
#include <odp/thrmask.h>
#include <odp/spinlock_recursive.h>
#include <odp/rwlock_recursive.h>
#include <odp/std_clib.h>

#ifdef __cplusplus
}
#endif
#endif
