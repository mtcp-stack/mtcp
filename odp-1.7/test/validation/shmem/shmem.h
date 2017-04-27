/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_SHMEM_H_
#define _ODP_TEST_SHMEM_H_

#include <odp_cunit_common.h>

/* test functions: */
void shmem_test_odp_shm_sunnyday(void);

/* test arrays: */
extern odp_testinfo_t shmem_suite[];

/* test registry: */
extern odp_suiteinfo_t shmem_suites[];

/* main test program: */
int shmem_main(void);

#endif
