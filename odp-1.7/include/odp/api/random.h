/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP random number API
 */

#ifndef ODP_API_RANDOM_H_
#define ODP_API_RANDOM_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup odp_random ODP RANDOM
 *  @{
 */


/**
 * Generate random byte data
 *
 * @param[out]    buf   Output buffer
 * @param         size  Size of output buffer
 * @param use_entropy   Use entropy
 *
 * @todo Define the implication of the use_entropy parameter
 *
 * @return Number of bytes written
 * @retval <0 on failure
 */
int32_t
odp_random_data(uint8_t *buf, int32_t size, odp_bool_t use_entropy);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
