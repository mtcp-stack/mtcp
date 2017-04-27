/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_CRYPTO_INTERNAL_H_
#define ODP_CRYPTO_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/des.h>
#include <openssl/aes.h>

#define OP_RESULT_MAGIC 0x91919191

/** Forward declaration of session structure */
typedef struct odp_crypto_generic_session odp_crypto_generic_session_t;

/**
 * Algorithm handler function prototype
 */
typedef
odp_crypto_alg_err_t (*crypto_func_t)(odp_crypto_op_params_t *params,
				      odp_crypto_generic_session_t *session);

/**
 * Per crypto session data structure
 */
struct odp_crypto_generic_session {
	struct odp_crypto_generic_session *next;
	odp_crypto_op_t op;
	odp_bool_t do_cipher_first;
	odp_queue_t compl_queue;
	odp_pool_t output_pool;
	struct {
		odp_cipher_alg_t   alg;
		struct {
			uint8_t *data;
			size_t   len;
		} iv;
		union {
			struct {
				DES_key_schedule ks1;
				DES_key_schedule ks2;
				DES_key_schedule ks3;
			} des;
			struct {
				AES_KEY key;
			} aes;
			struct {
				EVP_CIPHER_CTX *ctx;
			} aes_gcm;
		} data;
		crypto_func_t func;
	} cipher;
	struct {
		odp_auth_alg_t  alg;
		union {
			struct {
				uint8_t  key[16];
				uint32_t bytes;
			} md5;
			struct {
				uint8_t  key[32];
				uint32_t bytes;
			} sha256;
		} data;
		crypto_func_t func;
	} auth;
};

/**
 * Per packet operation result
 */
typedef struct odp_crypto_generic_op_result {
	uint32_t magic;
	odp_crypto_op_result_t result;
} odp_crypto_generic_op_result_t;

/**
 * Per session creation operation result
 */
typedef struct odp_crypto_generic_session_result {
	odp_crypto_ses_create_err_t    rc;
	odp_crypto_session_t           session;
} odp_crypto_generic_session_result_t;

#ifdef __cplusplus
}
#endif

#endif
