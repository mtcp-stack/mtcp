/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_IPSEC_CACHE_H_
#define ODP_IPSEC_CACHE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp.h>
#include <odp/helper/ipsec.h>

#include <odp_ipsec_misc.h>
#include <odp_ipsec_sa_db.h>

/**
 * Mode specified on command line indicating how to exercise API
 */
typedef enum {
	CRYPTO_API_SYNC,              /**< Synchronous mode */
	CRYPTO_API_ASYNC_IN_PLACE,    /**< Asynchronous in place */
	CRYPTO_API_ASYNC_NEW_BUFFER   /**< Asynchronous new buffer */
} crypto_api_mode_e;

/**
 * IPsec cache data base entry
 */
typedef struct ipsec_cache_entry_s {
	struct ipsec_cache_entry_s  *next;        /**< Next entry on list */
	odp_bool_t                   in_place;    /**< Crypto API mode */
	uint32_t                     src_ip;      /**< Source v4 address */
	uint32_t                     dst_ip;      /**< Destination v4 address */
	sa_mode_t		     mode;        /**< SA mode - transport/tun */
	uint32_t                     tun_src_ip;  /**< Tunnel src IPv4 addr */
	uint32_t                     tun_dst_ip;  /**< Tunnel dst IPv4 addr */
	struct {
		odp_cipher_alg_t     alg;         /**< Cipher algorithm */
		uint32_t             spi;         /**< Cipher SPI */
		uint32_t             block_len;   /**< Cipher block length */
		uint32_t             iv_len;      /**< Cipher IV length */
		ipsec_key_t          key;         /**< Cipher key */
	} esp;
	struct {
		odp_auth_alg_t       alg;         /**< Auth algorithm */
		uint32_t             spi;         /**< Auth SPI */
		uint32_t             icv_len;     /**< Auth ICV length */
		ipsec_key_t          key;         /**< Auth key */
	} ah;

	/* Per SA state */
	struct {
		odp_crypto_session_t session;  /**< Crypto session handle */
		uint32_t      esp_seq;         /**< ESP TX sequence number */
		uint32_t      ah_seq;          /**< AH TX sequence number */
		uint8_t       iv[MAX_IV_LEN];  /**< ESP IV storage */
		odp_u16be_t    tun_hdr_id;     /**< Tunnel header IP ID */
	} state;
} ipsec_cache_entry_t;

/**
 * IPsec cache data base global structure
 */
typedef struct ipsec_cache_s {
	uint32_t             index;       /**< Index of next available entry */
	ipsec_cache_entry_t *in_list;     /**< List of active input entries */
	ipsec_cache_entry_t *out_list;    /**< List of active output entries */
	ipsec_cache_entry_t  array[MAX_DB]; /**< Entry storage */
} ipsec_cache_t;

/** Global pointer to ipsec_cache db */
extern ipsec_cache_t *ipsec_cache;

/** Initialize IPsec cache */
void init_ipsec_cache(void);

/**
 * Create an entry in the IPsec cache
 *
 * @param cipher_sa   Cipher SA DB entry pointer
 * @param auth_sa     Auth SA DB entry pointer
 * @param tun         Tunnel DB entry pointer
 * @param api_mode    Crypto API mode for testing
 * @param in          Direction (input versus output)
 * @param completionq Completion queue
 * @param out_pool    Output buffer pool
 *
 * @return 0 if successful else -1
 */
int create_ipsec_cache_entry(sa_db_entry_t *cipher_sa,
			     sa_db_entry_t *auth_sa,
			     tun_db_entry_t *tun,
			     crypto_api_mode_e api_mode,
			     odp_bool_t in,
			     odp_queue_t completionq,
			     odp_pool_t out_pool);

/**
 * Find a matching IPsec cache entry for input packet
 *
 * @param src_ip    Source IPv4 address
 * @param dst_ip    Destination IPv4 address
 * @param ah        Pointer to AH header in packet else NULL
 * @param esp       Pointer to ESP header in packet else NULL
 *
 * @return pointer to IPsec cache entry else NULL
 */
ipsec_cache_entry_t *find_ipsec_cache_entry_in(uint32_t src_ip,
					       uint32_t dst_ip,
					       odph_ahhdr_t *ah,
					       odph_esphdr_t *esp);

/**
 * Find a matching IPsec cache entry for output packet
 *
 * @param src_ip    Source IPv4 address
 * @param dst_ip    Destination IPv4 address
 * @param proto     IPv4 protocol (currently all protocols match)
 *
 * @return pointer to IPsec cache entry else NULL
 */
ipsec_cache_entry_t *find_ipsec_cache_entry_out(uint32_t src_ip,
						uint32_t dst_ip,
						uint8_t proto);

#ifdef __cplusplus
}
#endif

#endif
