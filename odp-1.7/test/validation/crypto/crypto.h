/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _ODP_TEST_CRYPTO_H_
#define _ODP_TEST_CRYPTO_H_

#include "odp_cunit_common.h"

/* test functions: */
void crypto_test_enc_alg_3des_cbc(void);
void crypto_test_enc_alg_3des_cbc_ovr_iv(void);
void crypto_test_dec_alg_3des_cbc(void);
void crypto_test_dec_alg_3des_cbc_ovr_iv(void);
void crypto_test_enc_alg_aes128_cbc(void);
void crypto_test_enc_alg_aes128_cbc_ovr_iv(void);
void crypto_test_dec_alg_aes128_cbc(void);
void crypto_test_dec_alg_aes128_cbc_ovr_iv(void);
void crypto_test_enc_alg_aes128_gcm(void);
void crypto_test_enc_alg_aes128_gcm_ovr_iv(void);
void crypto_test_dec_alg_aes128_gcm(void);
void crypto_test_dec_alg_aes128_gcm_ovr_iv(void);
void crypto_test_alg_hmac_md5(void);
void crypto_test_alg_hmac_sha256(void);

/* test arrays: */
extern odp_testinfo_t crypto_suite[];

/* test array init/term functions: */
int crypto_suite_sync_init(void);
int crypto_suite_async_init(void);

/* test registry: */
extern odp_suiteinfo_t crypto_suites[];

/* executable init/term functions: */
int crypto_init(void);
int crypto_term(void);

/* main test program: */
int crypto_main(void);

#endif
