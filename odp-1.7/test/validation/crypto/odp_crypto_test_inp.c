/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <odp.h>
#include <CUnit/Basic.h>
#include <odp_cunit_common.h>
#include "test_vectors.h"
#include "odp_crypto_test_inp.h"
#include "crypto.h"

struct suite_context_s {
	odp_crypto_op_mode_t pref_mode;
	odp_pool_t pool;
	odp_queue_t queue;
};

static struct suite_context_s suite_context;

/* Basic algorithm run function for async inplace mode.
 * Creates a session from input parameters and runs one operation
 * on input_vec. Checks the output of the crypto operation against
 * output_vec. Operation completion event is dequeued polling the
 * session output queue. Completion context pointer is retrieved
 * and checked against the one set before the operation.
 * Completion event can be a separate buffer or the input packet
 * buffer can be used.
 * */
static void alg_test(odp_crypto_op_t op,
		     odp_cipher_alg_t cipher_alg,
		     odp_crypto_iv_t ses_iv,
		     uint8_t *op_iv_ptr,
		     odp_crypto_key_t cipher_key,
		     odp_auth_alg_t auth_alg,
		     odp_crypto_key_t auth_key,
		     odp_crypto_data_range_t *cipher_range,
		     odp_crypto_data_range_t *auth_range,
		     const uint8_t *plaintext,
		     unsigned int plaintext_len,
		     const uint8_t *ciphertext,
		     unsigned int ciphertext_len,
		     const uint8_t *digest,
		     unsigned int digest_len
		     )
{
	odp_crypto_session_t session;
	int rc;
	odp_crypto_ses_create_err_t status;
	odp_bool_t posted;
	odp_event_t event;
	odp_crypto_compl_t compl_event;
	odp_crypto_op_result_t result;

	/* Create a crypto session */
	odp_crypto_session_params_t ses_params;
	memset(&ses_params, 0, sizeof(ses_params));
	ses_params.op = op;
	ses_params.auth_cipher_text = false;
	ses_params.pref_mode = suite_context.pref_mode;
	ses_params.cipher_alg = cipher_alg;
	ses_params.auth_alg = auth_alg;
	ses_params.compl_queue = suite_context.queue;
	ses_params.output_pool = suite_context.pool;
	ses_params.cipher_key = cipher_key;
	ses_params.iv = ses_iv;
	ses_params.auth_key = auth_key;

	rc = odp_crypto_session_create(&ses_params, &session, &status);
	CU_ASSERT_FATAL(!rc);
	CU_ASSERT(status == ODP_CRYPTO_SES_CREATE_ERR_NONE);
	CU_ASSERT(odp_crypto_session_to_u64(session) !=
		  odp_crypto_session_to_u64(ODP_CRYPTO_SESSION_INVALID));

	/* Prepare input data */
	odp_packet_t pkt = odp_packet_alloc(suite_context.pool,
					    plaintext_len + digest_len);
	CU_ASSERT(pkt != ODP_PACKET_INVALID);
	uint8_t *data_addr = odp_packet_data(pkt);
	memcpy(data_addr, plaintext, plaintext_len);
	int data_off = 0;

	/* Prepare input/output params */
	odp_crypto_op_params_t op_params;
	memset(&op_params, 0, sizeof(op_params));
	op_params.session = session;
	op_params.pkt = pkt;
	op_params.out_pkt = pkt;
	op_params.ctx = (void *)0xdeadbeef;

	if (cipher_range) {
		op_params.cipher_range = *cipher_range;
		data_off = cipher_range->offset;
	} else {
		op_params.cipher_range.offset = data_off;
		op_params.cipher_range.length = plaintext_len;
	}
	if (auth_range) {
		op_params.auth_range = *auth_range;
	} else {
		op_params.auth_range.offset = data_off;
		op_params.auth_range.length = plaintext_len;
	}
	if (op_iv_ptr)
		op_params.override_iv_ptr = op_iv_ptr;

	op_params.hash_result_offset = plaintext_len;

	rc = odp_crypto_operation(&op_params, &posted, &result);
	if (rc < 0) {
		CU_FAIL("Failed odp_crypto_operation()");
		goto cleanup;
	}

	if (posted) {
		/* Poll completion queue for results */
		do {
			event = odp_queue_deq(suite_context.queue);
		} while (event == ODP_EVENT_INVALID);

		compl_event = odp_crypto_compl_from_event(event);
		CU_ASSERT(odp_crypto_compl_to_u64(compl_event) ==
			  odp_crypto_compl_to_u64(odp_crypto_compl_from_event(event)));
		odp_crypto_compl_result(compl_event, &result);
		odp_crypto_compl_free(compl_event);
	}

	CU_ASSERT(result.ok);
	CU_ASSERT(result.pkt == pkt);

	if (cipher_alg != ODP_CIPHER_ALG_NULL)
		CU_ASSERT(!memcmp(data_addr, ciphertext, ciphertext_len));

	if (op == ODP_CRYPTO_OP_ENCODE && auth_alg != ODP_AUTH_ALG_NULL)
		CU_ASSERT(!memcmp(data_addr + op_params.hash_result_offset,
				  digest, digest_len));

	CU_ASSERT(result.ctx == (void *)0xdeadbeef);
cleanup:
	rc = odp_crypto_session_destroy(session);
	CU_ASSERT(!rc);

	odp_packet_free(pkt);
}

/* This test verifies the correctness of encode (plaintext -> ciphertext)
 * operation for 3DES_CBC algorithm. IV for the operation is the session IV.
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.*/
void crypto_test_enc_alg_3des_cbc(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv;
	unsigned int test_vec_num = (sizeof(tdes_cbc_reference_length)/
				     sizeof(tdes_cbc_reference_length[0]));

	unsigned int i;
	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = tdes_cbc_reference_key[i];
		cipher_key.length = sizeof(tdes_cbc_reference_key[i]);
		iv.data = tdes_cbc_reference_iv[i];
		iv.length = sizeof(tdes_cbc_reference_iv[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_3DES_CBC,
			 iv,
			 NULL,
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 tdes_cbc_reference_plaintext[i],
			 tdes_cbc_reference_length[i],
			 tdes_cbc_reference_ciphertext[i],
			 tdes_cbc_reference_length[i], NULL, 0);
	}
}

/* This test verifies the correctness of encode (plaintext -> ciphertext)
 * operation for 3DES_CBC algorithm. IV for the operation is the operation IV.
 * */
void crypto_test_enc_alg_3des_cbc_ovr_iv(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = TDES_CBC_IV_LEN };
	unsigned int test_vec_num = (sizeof(tdes_cbc_reference_length)/
				     sizeof(tdes_cbc_reference_length[0]));

	unsigned int i;
	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = tdes_cbc_reference_key[i];
		cipher_key.length = sizeof(tdes_cbc_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_3DES_CBC,
			 iv,
			 tdes_cbc_reference_iv[i],
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 tdes_cbc_reference_plaintext[i],
			 tdes_cbc_reference_length[i],
			 tdes_cbc_reference_ciphertext[i],
			 tdes_cbc_reference_length[i], NULL, 0);
	}
}


/* This test verifies the correctness of decode (ciphertext -> plaintext)
 * operation for 3DES_CBC algorithm. IV for the operation is the session IV
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_dec_alg_3des_cbc(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = 0 };
	unsigned int test_vec_num = (sizeof(tdes_cbc_reference_length)/
				     sizeof(tdes_cbc_reference_length[0]));

	unsigned int i;
	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = tdes_cbc_reference_key[i];
		cipher_key.length = sizeof(tdes_cbc_reference_key[i]);
		iv.data = tdes_cbc_reference_iv[i];
		iv.length = sizeof(tdes_cbc_reference_iv[i]);

		alg_test(ODP_CRYPTO_OP_DECODE,
			 ODP_CIPHER_ALG_3DES_CBC,
			 iv,
			 NULL,
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 tdes_cbc_reference_ciphertext[i],
			 tdes_cbc_reference_length[i],
			 tdes_cbc_reference_plaintext[i],
			 tdes_cbc_reference_length[i], NULL, 0);
	}
}

/* This test verifies the correctness of decode (ciphertext -> plaintext)
 * operation for 3DES_CBC algorithm. IV for the operation is the session IV
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_dec_alg_3des_cbc_ovr_iv(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = TDES_CBC_IV_LEN };
	unsigned int test_vec_num = (sizeof(tdes_cbc_reference_length)/
				     sizeof(tdes_cbc_reference_length[0]));

	unsigned int i;
	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = tdes_cbc_reference_key[i];
		cipher_key.length = sizeof(tdes_cbc_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_DECODE,
			 ODP_CIPHER_ALG_3DES_CBC,
			 iv,
			 tdes_cbc_reference_iv[i],
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 tdes_cbc_reference_ciphertext[i],
			 tdes_cbc_reference_length[i],
			 tdes_cbc_reference_plaintext[i],
			 tdes_cbc_reference_length[i], NULL, 0);
	}
}

/* This test verifies the correctness of encode (plaintext -> ciphertext)
 * operation for AES128_GCM algorithm. IV for the operation is the session IV.
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.*/
void crypto_test_enc_alg_aes128_gcm(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = AES128_GCM_IV_LEN };
	unsigned int test_vec_num = (sizeof(aes128_gcm_reference_length) /
				     sizeof(aes128_gcm_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_gcm_reference_key[i];
		cipher_key.length = sizeof(aes128_gcm_reference_key[i]);
		iv.data = aes128_gcm_reference_iv[i];
		iv.length = sizeof(aes128_gcm_reference_iv[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_AES128_GCM,
			 iv,
			 NULL,
			 cipher_key,
			 ODP_AUTH_ALG_AES128_GCM,
			 auth_key,
			 &aes128_gcm_cipher_range[i],
			 &aes128_gcm_auth_range[i],
			 aes128_gcm_reference_plaintext[i],
			 aes128_gcm_reference_length[i],
			 aes128_gcm_reference_ciphertext[i],
			 aes128_gcm_reference_length[i],
			 aes128_gcm_reference_ciphertext[i] +
			 aes128_gcm_reference_length[i],
			 AES128_GCM_CHECK_LEN);
	}
}

/* This test verifies the correctness of encode (plaintext -> ciphertext)
 * operation for AES128_GCM algorithm. IV for the operation is the session IV.
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.*/
void crypto_test_enc_alg_aes128_gcm_ovr_iv(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = AES128_GCM_IV_LEN };
	unsigned int test_vec_num = (sizeof(aes128_gcm_reference_length) /
				     sizeof(aes128_gcm_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_gcm_reference_key[i];
		cipher_key.length = sizeof(aes128_gcm_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_AES128_GCM,
			 iv,
			 aes128_gcm_reference_iv[i],
			 cipher_key,
			 ODP_AUTH_ALG_AES128_GCM,
			 auth_key,
			 &aes128_gcm_cipher_range[i],
			 &aes128_gcm_auth_range[i],
			 aes128_gcm_reference_plaintext[i],
			 aes128_gcm_reference_length[i],
			 aes128_gcm_reference_ciphertext[i],
			 aes128_gcm_reference_length[i],
			 aes128_gcm_reference_ciphertext[i] +
			 aes128_gcm_reference_length[i],
			 AES128_GCM_CHECK_LEN);
	}
}

/* This test verifies the correctness of decode (ciphertext -> plaintext)
 * operation for 3DES_CBC algorithm. IV for the operation is the session IV
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_dec_alg_aes128_gcm(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = AES128_GCM_IV_LEN };
	unsigned int test_vec_num = (sizeof(aes128_gcm_reference_length) /
				     sizeof(aes128_gcm_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_gcm_reference_key[i];
		cipher_key.length = sizeof(aes128_gcm_reference_key[i]);
		iv.data = aes128_gcm_reference_iv[i];
		iv.length = sizeof(aes128_gcm_reference_iv[i]);

		alg_test(ODP_CRYPTO_OP_DECODE,
			 ODP_CIPHER_ALG_AES128_GCM,
			 iv,
			 NULL,
			 cipher_key,
			 ODP_AUTH_ALG_AES128_GCM,
			 auth_key,
			 &aes128_gcm_cipher_range[i],
			 &aes128_gcm_auth_range[i],
			 aes128_gcm_reference_ciphertext[i],
			 aes128_gcm_reference_length[i] + AES128_GCM_CHECK_LEN,
			 aes128_gcm_reference_plaintext[i],
			 aes128_gcm_reference_length[i],
			 aes128_gcm_reference_ciphertext[i] +
			 aes128_gcm_reference_length[i],
			 AES128_GCM_CHECK_LEN);
	}
}

/* This test verifies the correctness of decode (ciphertext -> plaintext)
 * operation for 3DES_CBC algorithm. IV for the operation is the session IV
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_dec_alg_aes128_gcm_ovr_iv(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = AES128_GCM_IV_LEN };
	unsigned int test_vec_num = (sizeof(aes128_gcm_reference_length) /
				     sizeof(aes128_gcm_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_gcm_reference_key[i];
		cipher_key.length = sizeof(aes128_gcm_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_DECODE,
			 ODP_CIPHER_ALG_AES128_GCM,
			 iv,
			 aes128_gcm_reference_iv[i],
			 cipher_key,
			 ODP_AUTH_ALG_AES128_GCM,
			 auth_key,
			 &aes128_gcm_cipher_range[i],
			 &aes128_gcm_auth_range[i],
			 aes128_gcm_reference_ciphertext[i],
			 aes128_gcm_reference_length[i] + AES128_GCM_CHECK_LEN,
			 aes128_gcm_reference_plaintext[i],
			 aes128_gcm_reference_length[i],
			 aes128_gcm_reference_ciphertext[i] +
			 aes128_gcm_reference_length[i],
			 AES128_GCM_CHECK_LEN);
	}
}

/* This test verifies the correctness of encode (plaintext -> ciphertext)
 * operation for AES128_CBC algorithm. IV for the operation is the session IV.
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.*/
void crypto_test_enc_alg_aes128_cbc(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv;
	unsigned int test_vec_num = (sizeof(aes128_cbc_reference_length) /
				     sizeof(aes128_cbc_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_cbc_reference_key[i];
		cipher_key.length = sizeof(aes128_cbc_reference_key[i]);
		iv.data = aes128_cbc_reference_iv[i];
		iv.length = sizeof(aes128_cbc_reference_iv[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_AES128_CBC,
			 iv,
			 NULL,
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 aes128_cbc_reference_plaintext[i],
			 aes128_cbc_reference_length[i],
			 aes128_cbc_reference_ciphertext[i],
			 aes128_cbc_reference_length[i], NULL, 0);
	}
}

/* This test verifies the correctness of encode (plaintext -> ciphertext)
 * operation for AES128_CBC algorithm. IV for the operation is the operation IV.
 * */
void crypto_test_enc_alg_aes128_cbc_ovr_iv(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = AES128_CBC_IV_LEN };
	unsigned int test_vec_num = (sizeof(aes128_cbc_reference_length) /
				     sizeof(aes128_cbc_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_cbc_reference_key[i];
		cipher_key.length = sizeof(aes128_cbc_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_AES128_CBC,
			 iv,
			 aes128_cbc_reference_iv[i],
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 aes128_cbc_reference_plaintext[i],
			 aes128_cbc_reference_length[i],
			 aes128_cbc_reference_ciphertext[i],
			 aes128_cbc_reference_length[i], NULL, 0);
	}
}

/* This test verifies the correctness of decode (ciphertext -> plaintext)
 * operation for AES128_CBC algorithm. IV for the operation is the session IV
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_dec_alg_aes128_cbc(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = 0 };
	unsigned int test_vec_num = (sizeof(aes128_cbc_reference_length) /
				     sizeof(aes128_cbc_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_cbc_reference_key[i];
		cipher_key.length = sizeof(aes128_cbc_reference_key[i]);
		iv.data = aes128_cbc_reference_iv[i];
		iv.length = sizeof(aes128_cbc_reference_iv[i]);

		alg_test(ODP_CRYPTO_OP_DECODE,
			 ODP_CIPHER_ALG_AES128_CBC,
			 iv,
			 NULL,
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 aes128_cbc_reference_ciphertext[i],
			 aes128_cbc_reference_length[i],
			 aes128_cbc_reference_plaintext[i],
			 aes128_cbc_reference_length[i], NULL, 0);
	}
}

/* This test verifies the correctness of decode (ciphertext -> plaintext)
 * operation for AES128_CBC algorithm. IV for the operation is the session IV
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_dec_alg_aes128_cbc_ovr_iv(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = AES128_CBC_IV_LEN };
	unsigned int test_vec_num = (sizeof(aes128_cbc_reference_length) /
				     sizeof(aes128_cbc_reference_length[0]));
	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		cipher_key.data = aes128_cbc_reference_key[i];
		cipher_key.length = sizeof(aes128_cbc_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_DECODE,
			 ODP_CIPHER_ALG_AES128_CBC,
			 iv,
			 aes128_cbc_reference_iv[i],
			 cipher_key,
			 ODP_AUTH_ALG_NULL,
			 auth_key,
			 NULL, NULL,
			 aes128_cbc_reference_ciphertext[i],
			 aes128_cbc_reference_length[i],
			 aes128_cbc_reference_plaintext[i],
			 aes128_cbc_reference_length[i], NULL, 0);
	}
}


/* This test verifies the correctness of HMAC_MD5 digest operation.
 * The output check length is truncated to 12 bytes (96 bits) as
 * returned by the crypto operation API call.
 * Note that hash digest is a one-way operation.
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_alg_hmac_md5(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = 0 };

	unsigned int test_vec_num = (sizeof(hmac_md5_reference_length)/
				     sizeof(hmac_md5_reference_length[0]));

	unsigned int i;
	for (i = 0; i < test_vec_num; i++) {
		auth_key.data = hmac_md5_reference_key[i];
		auth_key.length = sizeof(hmac_md5_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_NULL,
			 iv,
			 iv.data,
			 cipher_key,
			 ODP_AUTH_ALG_MD5_96,
			 auth_key,
			 NULL, NULL,
			 hmac_md5_reference_plaintext[i],
			 hmac_md5_reference_length[i],
			 NULL, 0,
			 hmac_md5_reference_digest[i],
			 HMAC_MD5_96_CHECK_LEN);
	}
}

/* This test verifies the correctness of HMAC_MD5 digest operation.
 * The output check length is truncated to 12 bytes (96 bits) as
 * returned by the crypto operation API call.
 * Note that hash digest is a one-way operation.
 * In addition the test verifies if the implementation can use the
 * packet buffer as completion event buffer.
 * */
void crypto_test_alg_hmac_sha256(void)
{
	odp_crypto_key_t cipher_key = { .data = NULL, .length = 0 },
			 auth_key   = { .data = NULL, .length = 0 };
	odp_crypto_iv_t iv = { .data = NULL, .length = 0 };

	unsigned int test_vec_num = (sizeof(hmac_sha256_reference_length) /
				     sizeof(hmac_sha256_reference_length[0]));

	unsigned int i;

	for (i = 0; i < test_vec_num; i++) {
		auth_key.data = hmac_sha256_reference_key[i];
		auth_key.length = sizeof(hmac_sha256_reference_key[i]);

		alg_test(ODP_CRYPTO_OP_ENCODE,
			 ODP_CIPHER_ALG_NULL,
			 iv,
			 iv.data,
			 cipher_key,
			 ODP_AUTH_ALG_SHA256_128,
			 auth_key,
			 NULL, NULL,
			 hmac_sha256_reference_plaintext[i],
			 hmac_sha256_reference_length[i],
			 NULL, 0,
			 hmac_sha256_reference_digest[i],
			 HMAC_SHA256_128_CHECK_LEN);
	}
}

int crypto_suite_sync_init(void)
{
	suite_context.pool = odp_pool_lookup("packet_pool");
	if (suite_context.pool == ODP_POOL_INVALID)
		return -1;

	suite_context.queue = ODP_QUEUE_INVALID;
	suite_context.pref_mode = ODP_CRYPTO_SYNC;
	return 0;
}

int crypto_suite_async_init(void)
{
	suite_context.pool = odp_pool_lookup("packet_pool");
	if (suite_context.pool == ODP_POOL_INVALID)
		return -1;
	suite_context.queue = odp_queue_lookup("crypto-out");
	if (suite_context.queue == ODP_QUEUE_INVALID)
		return -1;

	suite_context.pref_mode = ODP_CRYPTO_ASYNC;
	return 0;
}

odp_testinfo_t crypto_suite[] = {
	ODP_TEST_INFO(crypto_test_enc_alg_3des_cbc),
	ODP_TEST_INFO(crypto_test_dec_alg_3des_cbc),
	ODP_TEST_INFO(crypto_test_enc_alg_3des_cbc_ovr_iv),
	ODP_TEST_INFO(crypto_test_dec_alg_3des_cbc_ovr_iv),
	ODP_TEST_INFO(crypto_test_enc_alg_aes128_cbc),
	ODP_TEST_INFO(crypto_test_dec_alg_aes128_cbc),
	ODP_TEST_INFO(crypto_test_enc_alg_aes128_cbc_ovr_iv),
	ODP_TEST_INFO(crypto_test_dec_alg_aes128_cbc_ovr_iv),
	ODP_TEST_INFO(crypto_test_enc_alg_aes128_gcm),
	ODP_TEST_INFO(crypto_test_enc_alg_aes128_gcm_ovr_iv),
	ODP_TEST_INFO(crypto_test_dec_alg_aes128_gcm),
	ODP_TEST_INFO(crypto_test_dec_alg_aes128_gcm_ovr_iv),
	ODP_TEST_INFO(crypto_test_alg_hmac_md5),
	ODP_TEST_INFO(crypto_test_alg_hmac_sha256),
	ODP_TEST_INFO_NULL,
};
