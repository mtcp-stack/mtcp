/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP atomic operations
 */

#ifndef ODP_ATOMIC_TYPES_H_
#define ODP_ATOMIC_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/align.h>

/**
 * @internal
 * Atomic 64-bit unsigned integer
 */
struct odp_atomic_u64_s {
	uint64_t v; /**< Actual storage for the atomic variable */
#if __GCC_ATOMIC_LLONG_LOCK_FREE < 2
	/* Some architectures do not support lock-free operations on 64-bit
	 * data types. We use a spin lock to ensure atomicity. */
	char lock; /**< Spin lock (if needed) used to ensure atomic access */
#endif
} ODP_ALIGNED(sizeof(uint64_t)); /* Enforce alignement! */;

/**
 * @internal
 * Atomic 32-bit unsigned integer
 */
struct odp_atomic_u32_s {
	uint32_t v; /**< Actual storage for the atomic variable */
} ODP_ALIGNED(sizeof(uint32_t)); /* Enforce alignement! */;

#if __GCC_ATOMIC_LLONG_LOCK_FREE < 2

/**
 * @internal
 * CAS operation expression for the ATOMIC_OP macro
 */
#define ATOMIC_CAS_OP(ret_ptr, old_val, new_val) \
({ \
	if (atom->v == (old_val)) { \
		atom->v = (new_val); \
		*(ret_ptr) = 1; \
	} else { \
		*(ret_ptr) = 0; \
	} \
})

/**
 * @internal
 * Helper macro for lock-based atomic operations on 64-bit integers
 * @param[in,out] atom Pointer to the 64-bit atomic variable
 * @param expr Expression used update the variable.
 * @return The old value of the variable.
 */
#define ATOMIC_OP(atom, expr) \
({ \
	uint64_t _old_val; \
	/* Loop while lock is already taken, stop when lock becomes clear */ \
	while (__atomic_test_and_set(&(atom)->lock, __ATOMIC_ACQUIRE)) \
		(void)0; \
	_old_val = (atom)->v; \
	(expr); /* Perform whatever update is desired */ \
	__atomic_clear(&(atom)->lock, __ATOMIC_RELEASE); \
	_old_val; /* Return old value */ \
})
#endif

typedef struct odp_atomic_u64_s odp_atomic_u64_t;

typedef struct odp_atomic_u32_s odp_atomic_u32_t;

#ifdef __cplusplus
}
#endif

#endif
