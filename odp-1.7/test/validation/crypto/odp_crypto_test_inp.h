/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */
#ifndef ODP_CRYPTO_TEST_ASYNC_INP_
#define ODP_CRYPTO_TEST_ASYNC_INP_

#include <odp_cunit_common.h>

/* Suite names */
#define ODP_CRYPTO_ASYNC_INP	"odp_crypto_async_inp"
#define ODP_CRYPTO_SYNC_INP    "odp_crypto_sync_inp"

/* Suite test array */
extern odp_testinfo_t crypto_suite[];

int crypto_suite_sync_init(void);
int crypto_suite_async_init(void);

#endif
