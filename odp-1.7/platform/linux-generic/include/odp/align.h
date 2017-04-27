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

#ifndef ODP_PLAT_ALIGN_H_
#define ODP_PLAT_ALIGN_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup odp_compiler_optim
 *  @{
 */

#ifdef __GNUC__

#define ODP_ALIGNED(x) __attribute__((aligned(x)))

#define ODP_PACKED __attribute__((__packed__))

#define ODP_OFFSETOF(type, member) __builtin_offsetof(type, member)

#define ODP_FIELD_SIZEOF(type, member) sizeof(((type *)0)->member)

#if defined __x86_64__ || defined __i386__

#define ODP_CACHE_LINE_SIZE 64

#elif defined __arm__ || defined __aarch64__

#define ODP_CACHE_LINE_SIZE 64

#elif defined __OCTEON__

#define ODP_CACHE_LINE_SIZE 128

#elif defined __powerpc__

#define ODP_CACHE_LINE_SIZE 64

#else
#error GCC target not found
#endif

#else
#error Non-gcc compatible compiler
#endif

#define ODP_PAGE_SIZE       4096

#define ODP_ALIGNED_CACHE   ODP_ALIGNED(ODP_CACHE_LINE_SIZE)

#define ODP_ALIGNED_PAGE    ODP_ALIGNED(ODP_PAGE_SIZE)

/**
 * @}
 */

#include <odp/api/align.h>

#ifdef __cplusplus
}
#endif

#endif
