/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
/**
 * @file
 *
 * ODP debug
 */

#ifndef ODP_API_DEBUG_H_
#define ODP_API_DEBUG_H_


#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) && !defined(__clang__)


#if __GNUC__ < 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ < 6))

/**
 * @internal _Static_assert was only added in GCC 4.6. Provide a weak replacement
 * for previous versions.
 */
#define _Static_assert(e, s) (extern int (*static_assert_checker(void)) \
	[sizeof(struct { unsigned int error_if_negative:(e) ? 1 : -1; })])

#endif



#endif


/**
 * @internal Compile time assertion-macro - fail compilation if cond is false.
 * This macro has zero runtime overhead
 */
#define _ODP_STATIC_ASSERT(cond, msg)  _Static_assert(cond, msg)


#ifdef __cplusplus
}
#endif

#endif
