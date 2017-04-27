/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP classification descriptor
 */

#ifndef ODP_CLASSIFY_TYPES_H_
#define ODP_CLASSIFY_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/plat/strong_types.h>

/** @addtogroup odp_classification
 *  @{
 */

typedef ODP_HANDLE_T(odp_cos_t);
typedef ODP_HANDLE_T(odp_flowsig_t);

#define ODP_COS_INVALID  _odp_cast_scalar(odp_cos_t, ~0)
#define ODP_COS_NAME_LEN 32

typedef uint16_t odp_cos_flow_set_t;

typedef ODP_HANDLE_T(odp_pmr_t);
#define ODP_PMR_INVAL _odp_cast_scalar(odp_pmr_t, ~0)

typedef ODP_HANDLE_T(odp_pmr_set_t);
#define ODP_PMR_SET_INVAL _odp_cast_scalar(odp_pmr_set_t, ~0)

/** Get printable format of odp_cos_t */
static inline uint64_t odp_cos_to_u64(odp_cos_t hdl)
{
	return _odp_pri(hdl);
}

/** Get printable format of odp_pmr_t */
static inline uint64_t odp_pmr_to_u64(odp_pmr_t hdl)
{
	return _odp_pri(hdl);
}

/** Get printable format of odp_pmr_set_t */
static inline uint64_t odp_pmr_set_to_u64(odp_pmr_set_t hdl)
{
	return _odp_pri(hdl);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
