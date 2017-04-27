/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP schedule
 */

#ifndef ODP_SCHEDULE_TYPES_H_
#define ODP_SCHEDULE_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_scheduler
 *  @{
 */

#define ODP_SCHED_WAIT     UINT64_MAX
#define ODP_SCHED_NO_WAIT  0

typedef int odp_schedule_prio_t;

#define ODP_SCHED_PRIO_HIGHEST  0

#define ODP_SCHED_PRIO_NORMAL   (ODP_CONFIG_SCHED_PRIOS / 2)

#define ODP_SCHED_PRIO_LOWEST   (ODP_CONFIG_SCHED_PRIOS - 1)

#define ODP_SCHED_PRIO_DEFAULT  ODP_SCHED_PRIO_NORMAL

typedef int odp_schedule_sync_t;

#define ODP_SCHED_SYNC_PARALLEL 0
#define ODP_SCHED_SYNC_ATOMIC   1
#define ODP_SCHED_SYNC_ORDERED  2

typedef int odp_schedule_group_t;

/* These must be kept in sync with thread_globals_t in odp_thread.c */
#define ODP_SCHED_GROUP_INVALID -1
#define ODP_SCHED_GROUP_ALL     0
#define ODP_SCHED_GROUP_WORKER  1
#define ODP_SCHED_GROUP_CONTROL 2

#define ODP_SCHED_GROUP_NAME_LEN 32

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
