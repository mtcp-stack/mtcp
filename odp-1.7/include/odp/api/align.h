/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */


/**
 * @file
 *
 * ODP alignments
 */

#ifndef ODP_API_ALIGN_H_
#define ODP_API_ALIGN_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_compiler_optim
 *  Macros that allow cache line size configuration, check that
 *  alignment is a power of two etc.
 *  @{
 */

/* Checkpatch complains, but cannot use __aligned(size) for this purpose. */

/**
 * @def ODP_ALIGNED
 * Defines type/struct/variable alignment in bytes
 */

/**
 * @def ODP_PACKED
 * Defines type/struct to be packed
 */

/**
 * @def ODP_OFFSETOF
 * Returns offset of member in type
 */

/**
 * @def ODP_FIELD_SIZEOF
 * Returns sizeof member
 */

/**
 * @def ODP_CACHE_LINE_SIZE
 * Cache line size
 */

/**
 * @def ODP_PAGE_SIZE
 * Page size
 */

/**
 * @def ODP_ALIGNED_CACHE
 * Defines type/struct/variable to be cache line size aligned
 */

/**
 * @def ODP_ALIGNED_PAGE
 * Defines type/struct/variable to be page size aligned
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
