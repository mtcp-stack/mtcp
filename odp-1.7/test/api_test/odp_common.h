/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP test application common headers
 */

#ifndef ODP_COMMON_H
#define ODP_COMMON_H

#define MAX_WORKERS 32 /**< Maximum number of work threads */

/** types of tests */
typedef enum {
	ODP_ATOMIC_TEST = 0,
	ODP_SHM_TEST,
	ODP_RING_TEST_BASIC,
	ODP_RING_TEST_STRESS,
	ODP_TIMER_PING_TEST,
	ODP_MAX_TEST
} odp_test_case_e;

/**
 * Thread argument
 */
typedef struct {
	int testcase; /**< specifies which set of API's to exercise */
	int numthrds; /**< no of pthreads to create */
} pthrd_arg;

extern void odp_print_system_info(void);
extern int odp_test_global_init(void);
/** create thread fro start_routine function */
extern int odp_test_thread_create(void *(*start_routine) (void *), pthrd_arg *);
extern int odp_test_thread_exit(pthrd_arg *);

#endif /* ODP_COMMON_H */
