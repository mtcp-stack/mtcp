/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

/**
 * @file
 *
 * ODP internal alignments
 */

#ifndef ODP_ALIGN_INTERNAL_H_
#define ODP_ALIGN_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/align.h>

/** @addtogroup odp_compiler_optim
 *  @{
 */

/*
 * Round up
 */

/**
 * @internal
 * Round up pointer 'x' to alignment 'align'
 */
#define ODP_ALIGN_ROUNDUP_PTR(x, align)\
	((void *)ODP_ALIGN_ROUNDUP((uintptr_t)(x), (uintptr_t)(align)))

/**
 * @internal
 * Round up pointer 'x' to cache line size alignment
 */
#define ODP_CACHE_LINE_SIZE_ROUNDUP_PTR(x)\
	((void *)ODP_CACHE_LINE_SIZE_ROUNDUP((uintptr_t)(x)))

/**
 * @internal
 * Round up 'x' to alignment 'align'
 */
#define ODP_ALIGN_ROUNDUP(x, align)\
	((align) * (((x) + align - 1) / (align)))

/**
 * @internal
 * Round up pointer 'x' to alignment 'align'
 */
#define ODP_ALIGN_ROUNDUP_PTR(x, align)\
	((void *)ODP_ALIGN_ROUNDUP((uintptr_t)(x), (uintptr_t)(align)))

/**
 * @internal
 * Round up 'x' to cache line size alignment
 */
#define ODP_CACHE_LINE_SIZE_ROUNDUP(x)\
	ODP_ALIGN_ROUNDUP(x, ODP_CACHE_LINE_SIZE)

/**
 * @internal
 * Round up pointer 'x' to cache line size alignment
 */
#define ODP_CACHE_LINE_SIZE_ROUNDUP_PTR(x)\
	((void *)ODP_CACHE_LINE_SIZE_ROUNDUP((uintptr_t)(x)))

/**
 * @internal
 * Round up 'x' to page size alignment
 */
#define ODP_PAGE_SIZE_ROUNDUP(x)\
	ODP_ALIGN_ROUNDUP(x, ODP_PAGE_SIZE)

/*
 * Round down
 */

/**
 * @internal
 * Round down pointer 'x' to 'align' alignment, which is a power of two
 */
#define ODP_ALIGN_ROUNDDOWN_PTR_POWER_2(x, align)\
((void *)ODP_ALIGN_ROUNDDOWN_POWER_2((uintptr_t)(x), (uintptr_t)(align)))

/**
 * @internal
 * Round down pointer 'x' to cache line size alignment
 */
#define ODP_CACHE_LINE_SIZE_ROUNDDOWN_PTR(x)\
	((void *)ODP_CACHE_LINE_SIZE_ROUNDDOWN((uintptr_t)(x)))

/**
 * @internal
 * Round down 'x' to 'align' alignment, which is a power of two
 */
#define ODP_ALIGN_ROUNDDOWN_POWER_2(x, align)\
	((x) & (~((align) - 1)))
/**
 * @internal
 * Round down 'x' to cache line size alignment
 */
#define ODP_CACHE_LINE_SIZE_ROUNDDOWN(x)\
	ODP_ALIGN_ROUNDDOWN_POWER_2(x, ODP_CACHE_LINE_SIZE)

/**
 * @internal
 * Round down pointer 'x' to 'align' alignment, which is a power of two
 */
#define ODP_ALIGN_ROUNDDOWN_PTR_POWER_2(x, align)\
((void *)ODP_ALIGN_ROUNDDOWN_POWER_2((uintptr_t)(x), (uintptr_t)(align)))

/**
 * @internal
 * Round down pointer 'x' to cache line size alignment
 */
#define ODP_CACHE_LINE_SIZE_ROUNDDOWN_PTR(x)\
	((void *)ODP_CACHE_LINE_SIZE_ROUNDDOWN((uintptr_t)(x)))

/*
 * Check align
 */

/**
 * @internal
 * Check if pointer 'x' is aligned to 'align', which is a power of two
 */
#define ODP_ALIGNED_CHECK_POWER_2(x, align)\
	((((uintptr_t)(x)) & (((uintptr_t)(align))-1)) == 0)

/**
 * @internal
 * Check if value is a power of two
 */
#define ODP_VAL_IS_POWER_2(x) ((((x)-1) & (x)) == 0)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
