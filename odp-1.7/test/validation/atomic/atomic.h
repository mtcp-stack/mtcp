/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_ATOMIC_H_
#define _ODP_TEST_ATOMIC_H_

#include <odp_cunit_common.h>

/* test functions: */
void atomic_test_atomic_inc_dec(void);
void atomic_test_atomic_add_sub(void);
void atomic_test_atomic_fetch_inc_dec(void);
void atomic_test_atomic_fetch_add_sub(void);
void atomic_test_atomic_max_min(void);
void atomic_test_atomic_cas_inc_dec(void);
void atomic_test_atomic_xchg(void);
void atomic_test_atomic_non_relaxed(void);
void atomic_test_atomic_op_lock_free(void);

/* test arrays: */
extern odp_testinfo_t atomic_suite_atomic[];

/* test array init/term functions: */
int atomic_suite_init(void);

/* test registry: */
extern odp_suiteinfo_t atomic_suites[];

/* executable init/term functions: */
int atomic_init(void);

/* main test program: */
int atomic_main(void);

#endif
