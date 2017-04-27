/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>

#include <example_debug.h>

#include <odp.h>

#include <odp/helper/ipsec.h>

#include <odp_ipsec_cache.h>

/** Global pointer to ipsec_cache db */
ipsec_cache_t *ipsec_cache;

void init_ipsec_cache(void)
{
	odp_shm_t shm;

	shm = odp_shm_reserve("shm_ipsec_cache",
			      sizeof(ipsec_cache_t),
			      ODP_CACHE_LINE_SIZE,
			      0);

	ipsec_cache = odp_shm_addr(shm);

	if (ipsec_cache == NULL) {
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		exit(EXIT_FAILURE);
	}
	memset(ipsec_cache, 0, sizeof(*ipsec_cache));
}

int create_ipsec_cache_entry(sa_db_entry_t *cipher_sa,
			     sa_db_entry_t *auth_sa,
			     tun_db_entry_t *tun,
			     crypto_api_mode_e api_mode,
			     odp_bool_t in,
			     odp_queue_t completionq,
			     odp_pool_t out_pool)
{
	odp_crypto_session_params_t params;
	ipsec_cache_entry_t *entry;
	odp_crypto_ses_create_err_t ses_create_rc;
	odp_crypto_session_t session;
	sa_mode_t mode = IPSEC_SA_MODE_TRANSPORT;

	/* Verify we have a good entry */
	entry = &ipsec_cache->array[ipsec_cache->index];
	if (MAX_DB <= ipsec_cache->index)
		return -1;

	/* Verify SA mode match in case of cipher&auth */
	if (cipher_sa && auth_sa &&
	    (cipher_sa->mode != auth_sa->mode))
		return -1;

	/* Setup parameters and call crypto library to create session */
	params.op = (in) ? ODP_CRYPTO_OP_DECODE : ODP_CRYPTO_OP_ENCODE;
	params.auth_cipher_text = TRUE;
	if (CRYPTO_API_SYNC == api_mode) {
		params.pref_mode   = ODP_CRYPTO_SYNC;
		params.compl_queue = ODP_QUEUE_INVALID;
		params.output_pool = ODP_POOL_INVALID;
	} else {
		params.pref_mode   = ODP_CRYPTO_ASYNC;
		params.compl_queue = completionq;
		params.output_pool = out_pool;
	}

	if (CRYPTO_API_ASYNC_NEW_BUFFER == api_mode)
		entry->in_place = FALSE;
	else
		entry->in_place = TRUE;

	/* Cipher */
	if (cipher_sa) {
		params.cipher_alg  = cipher_sa->alg.u.cipher;
		params.cipher_key.data  = cipher_sa->key.data;
		params.cipher_key.length  = cipher_sa->key.length;
		params.iv.data = entry->state.iv;
		params.iv.length = cipher_sa->iv_len;
		mode = cipher_sa->mode;
	} else {
		params.cipher_alg = ODP_CIPHER_ALG_NULL;
		params.iv.data = NULL;
		params.iv.length = 0;
	}

	/* Auth */
	if (auth_sa) {
		params.auth_alg = auth_sa->alg.u.auth;
		params.auth_key.data = auth_sa->key.data;
		params.auth_key.length = auth_sa->key.length;
		mode = auth_sa->mode;
	} else {
		params.auth_alg = ODP_AUTH_ALG_NULL;
	}

	/* Generate an IV */
	if (params.iv.length) {
		int32_t size = params.iv.length;

		int32_t ret = odp_random_data(params.iv.data, size, 1);

		if (ret != size)
			return -1;
	}

	/* Synchronous session create for now */
	if (odp_crypto_session_create(&params, &session, &ses_create_rc))
		return -1;
	if (ODP_CRYPTO_SES_CREATE_ERR_NONE != ses_create_rc)
		return -1;

	/* Copy remainder */
	if (cipher_sa) {
		entry->src_ip = cipher_sa->src_ip;
		entry->dst_ip = cipher_sa->dst_ip;
		entry->esp.alg = cipher_sa->alg.u.cipher;
		entry->esp.spi = cipher_sa->spi;
		entry->esp.block_len = cipher_sa->block_len;
		entry->esp.iv_len = cipher_sa->iv_len;
		memcpy(&entry->esp.key, &cipher_sa->key, sizeof(ipsec_key_t));
	}
	if (auth_sa) {
		entry->src_ip = auth_sa->src_ip;
		entry->dst_ip = auth_sa->dst_ip;
		entry->ah.alg = auth_sa->alg.u.auth;
		entry->ah.spi = auth_sa->spi;
		entry->ah.icv_len = auth_sa->icv_len;
		memcpy(&entry->ah.key, &auth_sa->key, sizeof(ipsec_key_t));
	}

	if (tun) {
		entry->tun_src_ip = tun->tun_src_ip;
		entry->tun_dst_ip = tun->tun_dst_ip;
		mode = IPSEC_SA_MODE_TUNNEL;

		int ret;

		if (!in) {
			/* init tun hdr id */
			ret = odp_random_data((uint8_t *)
					      &entry->state.tun_hdr_id,
					      sizeof(entry->state.tun_hdr_id),
					      1);
			if (ret != sizeof(entry->state.tun_hdr_id))
				return -1;
		}
	}
	entry->mode = mode;

	/* Initialize state */
	entry->state.esp_seq = 0;
	entry->state.ah_seq = 0;
	entry->state.session = session;

	/* Add entry to the appropriate list */
	ipsec_cache->index++;
	if (in) {
		entry->next = ipsec_cache->in_list;
		ipsec_cache->in_list = entry;
	} else {
		entry->next = ipsec_cache->out_list;
		ipsec_cache->out_list = entry;
	}

	return 0;
}

ipsec_cache_entry_t *find_ipsec_cache_entry_in(uint32_t src_ip,
					       uint32_t dst_ip,
					       odph_ahhdr_t *ah,
					       odph_esphdr_t *esp)
{
	ipsec_cache_entry_t *entry = ipsec_cache->in_list;

	/* Look for a hit */
	for (; NULL != entry; entry = entry->next) {
		if ((entry->src_ip != src_ip) || (entry->dst_ip != dst_ip))
			if ((entry->tun_src_ip != src_ip) ||
			    (entry->tun_dst_ip != dst_ip))
				continue;
		if (ah &&
		    ((!entry->ah.alg) ||
		     (entry->ah.spi != odp_be_to_cpu_32(ah->spi))))
			continue;
		if (esp &&
		    ((!entry->esp.alg) ||
		     (entry->esp.spi != odp_be_to_cpu_32(esp->spi))))
			continue;
		break;
	}

	return entry;
}

ipsec_cache_entry_t *find_ipsec_cache_entry_out(uint32_t src_ip,
						uint32_t dst_ip,
						uint8_t proto EXAMPLE_UNUSED)
{
	ipsec_cache_entry_t *entry = ipsec_cache->out_list;

	/* Look for a hit */
	for (; NULL != entry; entry = entry->next) {
		if ((entry->src_ip == src_ip) && (entry->dst_ip == dst_ip))
			break;
	}
	return entry;
}
