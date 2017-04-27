/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_CPUMASK_H_
#define _ODP_TEST_CPUMASK_H_

#include <odp.h>
#include <odp_cunit_common.h>

/* test functions: */
#include "mask_common.h"
void cpumask_test_odp_cpumask_def_control(void);
void cpumask_test_odp_cpumask_def_worker(void);
void cpumask_test_odp_cpumask_def(void);

/* test arrays: */
extern odp_testinfo_t cpumask_suite[];

/* test registry: */
extern odp_suiteinfo_t cpumask_suites[];

/* main test program: */
int cpumask_main(void);

#endif
