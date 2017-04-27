/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP CPU masks and enumeration
 */

#ifndef ODP_CPUMASK_TYPES_H_
#define ODP_CPUMASK_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_cpumask
 *  @{
 */

#include <odp/std_types.h>

#define ODP_CPUMASK_SIZE 1024

#define ODP_CPUMASK_STR_SIZE ((ODP_CPUMASK_SIZE + 3) / 4 + 3)

/**
 * CPU mask
 *
 * Don't access directly, use access functions.
 */
typedef struct odp_cpumask_t {
	cpu_set_t set; /**< @private Mask*/
} odp_cpumask_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
