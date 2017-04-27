/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP IPSec headers
 */

#ifndef ODPH_IPSEC_H_
#define ODPH_IPSEC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/byteorder.h>
#include <odp/align.h>
#include <odp/debug.h>

/** @addtogroup odph_header ODPH HEADER
 *  @{
 */

#define ODPH_ESPHDR_LEN      8    /**< IPSec ESP header length */
#define ODPH_ESPTRL_LEN      2    /**< IPSec ESP trailer length */
#define ODPH_AHHDR_LEN      12    /**< IPSec AH header length */

/**
 * IPSec ESP header
 */
typedef struct ODP_PACKED {
	odp_u32be_t spi;     /**< Security Parameter Index */
	odp_u32be_t seq_no;  /**< Sequence Number */
	uint8_t    iv[0];    /**< Initialization vector */
} odph_esphdr_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_esphdr_t) == ODPH_ESPHDR_LEN, "ODPH_ESPHDR_T__SIZE_ERROR");

/**
 * IPSec ESP trailer
 */
typedef struct ODP_PACKED {
	uint8_t pad_len;      /**< Padding length (0-255) */
	uint8_t next_header;  /**< Next header protocol */
	uint8_t icv[0];       /**< Integrity Check Value (optional) */
} odph_esptrl_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_esptrl_t) == ODPH_ESPTRL_LEN, "ODPH_ESPTRL_T__SIZE_ERROR");

/**
 * IPSec AH header
 */
typedef struct ODP_PACKED {
	uint8_t    next_header;  /**< Next header protocol */
	uint8_t    ah_len;       /**< AH header length */
	odp_u16be_t pad;         /**< Padding (must be 0) */
	odp_u32be_t spi;         /**< Security Parameter Index */
	odp_u32be_t seq_no;      /**< Sequence Number */
	uint8_t    icv[0];       /**< Integrity Check Value */
} odph_ahhdr_t;

/** @internal Compile time assert */
_ODP_STATIC_ASSERT(sizeof(odph_ahhdr_t) == ODPH_AHHDR_LEN, "ODPH_AHHDR_T__SIZE_ERROR");

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
