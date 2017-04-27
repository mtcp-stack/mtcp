/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
/**
 * @file
 *
 * HELPER debug
 */

#ifndef HELPER_DEBUG_H_
#define HELPER_DEBUG_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ODPH_DEBUG_PRINT
#define ODPH_DEBUG_PRINT 1
#endif

/**
 * log level.
 */
typedef enum HELPER_log_level {
	ODPH_LOG_DBG,
	ODPH_LOG_ERR,
	ODPH_LOG_ABORT
} HELPER_log_level_e;

/**
 * default LOG macro.
 */
#define ODPH_LOG(level, fmt, ...) \
do { \
	switch (level) { \
	case ODPH_LOG_ERR: \
		fprintf(stderr, "%s:%d:%s():" fmt, __FILE__, \
		__LINE__, __func__, ##__VA_ARGS__); \
		break; \
	case ODPH_LOG_DBG: \
		if (ODPH_DEBUG_PRINT == 1) \
			fprintf(stderr, "%s:%d:%s():" fmt, __FILE__, \
			__LINE__, __func__, ##__VA_ARGS__); \
		break; \
	case ODPH_LOG_ABORT: \
		fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
		__LINE__, __func__, ##__VA_ARGS__); \
		abort(); \
		break; \
	default: \
		fprintf(stderr, "Unknown LOG level"); \
		break;\
	} \
} while (0)

/**
 * Debug printing macro, which prints output when DEBUG flag is set.
 */
#define ODPH_DBG(fmt, ...) \
		ODPH_LOG(ODPH_LOG_DBG, fmt, ##__VA_ARGS__)

/**
 * Print output to stderr (file, line and function).
 */
#define ODPH_ERR(fmt, ...) \
		ODPH_LOG(ODPH_LOG_ERR, fmt, ##__VA_ARGS__)

/**
 * Print output to stderr (file, line and function),
 * then abort.
 */
#define ODPH_ABORT(fmt, ...) \
		ODPH_LOG(ODPH_LOG_ABORT, fmt, ##__VA_ARGS__)

/**
 * @}
 */

/**
 * Mark intentionally unused argument for functions
 */
#define ODPH_UNUSED     __attribute__((__unused__))

#ifdef __cplusplus
}
#endif

#endif
