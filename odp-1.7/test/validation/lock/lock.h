/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_LOCK_H_
#define _ODP_TEST_LOCK_H_

#include <odp_cunit_common.h>

/* test functions: */
void lock_test_no_lock_functional(void);
void lock_test_spinlock_api(void);
void lock_test_spinlock_functional(void);
void lock_test_spinlock_recursive_api(void);
void lock_test_spinlock_recursive_functional(void);
void lock_test_ticketlock_api(void);
void lock_test_ticketlock_functional(void);
void lock_test_rwlock_api(void);
void lock_test_rwlock_functional(void);
void lock_test_rwlock_recursive_api(void);
void lock_test_rwlock_recursive_functional(void);

/* test arrays: */
extern odp_testinfo_t lock_suite_no_locking[];
extern odp_testinfo_t lock_suite_spinlock[];
extern odp_testinfo_t lock_suite_spinlock_recursive[];
extern odp_testinfo_t lock_suite_ticketlock[];
extern odp_testinfo_t lock_suite_rwlock[];
extern odp_testinfo_t lock_suite_rwlock_recursive[];

/* test array init/term functions: */
int lock_suite_init(void);

/* test registry: */
extern odp_suiteinfo_t lock_suites[];

/* executable init/term functions: */
int lock_init(void);

/* main test program: */
int lock_main(void);

#endif
