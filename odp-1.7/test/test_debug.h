/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
/**
 * @file
 *
 * test debug
 */

#ifndef TEST_DEBUG_H_
#define TEST_DEBUG_H_

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TEST_DEBUG_PRINT
#define TEST_DEBUG_PRINT 1
#endif

/**
 * log level.
 */
typedef enum test_log_level {
	TEST_LOG_DBG,
	TEST_LOG_ERR,
	TEST_LOG_ABORT
} test_log_level_e;

/**
 * default LOG macro.
 */
#define TEST_LOG(level, fmt, ...) \
do { \
	switch (level) { \
	case TEST_LOG_ERR: \
		fprintf(stderr, "%s:%d:%s():" fmt, __FILE__, \
		__LINE__, __func__, ##__VA_ARGS__); \
		break; \
	case TEST_LOG_DBG: \
		if (TEST_DEBUG_PRINT == 1) \
			fprintf(stderr, "%s:%d:%s():" fmt, __FILE__, \
			__LINE__, __func__, ##__VA_ARGS__); \
		break; \
	case TEST_LOG_ABORT: \
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
#define LOG_DBG(fmt, ...) \
		TEST_LOG(TEST_LOG_DBG, fmt, ##__VA_ARGS__)

/**
 * Print output to stderr (file, line and function).
 */
#define LOG_ERR(fmt, ...) \
		TEST_LOG(TEST_LOG_ERR, fmt, ##__VA_ARGS__)

/**
 * Print output to stderr (file, line and function),
 * then abort.
 */
#define LOG_ABORT(fmt, ...) \
		TEST_LOG(TEST_LOG_ABORT, fmt, ##__VA_ARGS__)

/**
 * @}
 */

/**
 * Mark intentionally unused argument for functions
 */
#define TEST_UNUSED     __attribute__((__unused__))

#ifdef __cplusplus
}
#endif

#endif
