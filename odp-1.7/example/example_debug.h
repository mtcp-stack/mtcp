/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
/**
 * @file
 *
 * example debug
 */

#ifndef EXAMPLE_DEBUG_H_
#define EXAMPLE_DEBUG_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EXAMPLE_DEBUG_PRINT
#define EXAMPLE_DEBUG_PRINT 1
#endif

/**
 * log level.
 */
typedef enum example_log_level {
	EXAMPLE_LOG_DBG,
	EXAMPLE_LOG_ERR,
	EXAMPLE_LOG_ABORT
} example_log_level_e;

/**
 * default LOG macro.
 */
#define EXAMPLE_LOG(level, fmt, ...) \
do { \
	switch (level) { \
	case EXAMPLE_LOG_ERR: \
		fprintf(stderr, "%s:%d:%s():" fmt, __FILE__, \
		__LINE__, __func__, ##__VA_ARGS__); \
		break; \
	case EXAMPLE_LOG_DBG: \
		if (EXAMPLE_DEBUG_PRINT == 1) \
			fprintf(stderr, "%s:%d:%s():" fmt, __FILE__, \
			__LINE__, __func__, ##__VA_ARGS__); \
		break; \
	case EXAMPLE_LOG_ABORT: \
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
#define EXAMPLE_DBG(fmt, ...) \
		EXAMPLE_LOG(EXAMPLE_LOG_DBG, fmt, ##__VA_ARGS__)

/**
 * Print output to stderr (file, line and function).
 */
#define EXAMPLE_ERR(fmt, ...) \
		EXAMPLE_LOG(EXAMPLE_LOG_ERR, fmt, ##__VA_ARGS__)

/**
 * Print output to stderr (file, line and function),
 * then abort.
 */
#define EXAMPLE_ABORT(fmt, ...) \
		EXAMPLE_LOG(EXAMPLE_LOG_ABORT, fmt, ##__VA_ARGS__)

/**
 * Intentionally unused variables to functions
 */
#define EXAMPLE_UNUSED     __attribute__((__unused__))

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
