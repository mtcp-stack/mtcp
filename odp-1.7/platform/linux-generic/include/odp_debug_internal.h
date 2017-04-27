/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
/**
 * @file
 *
 * ODP Debug internal
 * This file contains implementer support functions for Debug capabilities.
 *
 * @warning These definitions are not part of ODP API, they are for
 * internal use by implementers and should not be called from any other scope.
 */

#ifndef ODP_DEBUG_INTERNAL_H_
#define ODP_DEBUG_INTERNAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <odp/debug.h>
#include <odp_internal.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_ver_abt_log_dbg
 *  @{
 */

/**
 * Runtime assertion-macro - aborts if 'cond' is false.
 */
#ifdef ODP_DEBUG
#define ODP_ASSERT(cond) \
	do { if ((ODP_DEBUG == 1) && (!(cond))) { \
		ODP_ERR("%s\n", #cond); \
		odp_global_data.abort_fn(); } \
	} while (0)
#else
#define ODP_ASSERT(cond)
#endif

/**
 * This macro is used to indicate when a given function is not implemented
 */
#define ODP_UNIMPLEMENTED() \
		odp_global_data.log_fn(ODP_LOG_UNIMPLEMENTED, \
			"%s:%d:The function %s() is not implemented\n", \
			__FILE__, __LINE__, __func__)
/**
 * Log debug message if ODP_DEBUG_PRINT flag is set.
 */
/*#ifdef ODP_DEBUG_PRINT
#define ODP_DBG(fmt, ...) \
	do { \
		if (ODP_DEBUG_PRINT == 1) \
			ODP_LOG(ODP_LOG_DBG, fmt, ##__VA_ARGS__);\
	} while (0)
#else
#define ODP_DBG(fmt, ...)\
	do {\
	} while (0)
#endif*/

#define ODP_DBG(fmt, ...)	\
	printf("[Func: %s. Line: %d]" fmt, __func__, __LINE__, ## __VA_ARGS__)
/**
 * Log error message.
 */
/*#define ODP_ERR(fmt, ...) \
		ODP_LOG(ODP_LOG_ERR, fmt, ##__VA_ARGS__)*/

#define ODP_ERR(fmt, ...)	\
	printf("[Func: %s. Line: %d]" fmt, __func__, __LINE__, ## __VA_ARGS__)

/**
 * Log abort message and then stop execution (by default call abort()).
 * This function should not return.
 */
#define ODP_ABORT(fmt, ...) \
	do { \
		ODP_LOG(ODP_LOG_ABORT, fmt, ##__VA_ARGS__); \
		odp_global_data.abort_fn(); \
	} while (0)

/**
 * ODP LOG macro.
 */
#define ODP_LOG(level, fmt, ...) \
	odp_global_data.log_fn(level, "%s:%d:%s():" fmt, __FILE__, \
	__LINE__, __func__, ##__VA_ARGS__)

/**
 * Log print message when the application calls one of the ODP APIs
 * specifically for dumping internal data.
 */
/*#define ODP_PRINT(fmt, ...) \
	odp_global_data.log_fn(ODP_LOG_PRINT, " " fmt, ##__VA_ARGS__)*/

#define ODP_PRINT(fmt, ...)	\
	printf("[Func: %s. Line: %d]" fmt, __func__, __LINE__, ## __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
