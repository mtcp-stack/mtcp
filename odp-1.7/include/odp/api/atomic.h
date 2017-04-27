/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP atomic operations
 */

#ifndef ODP_API_ATOMIC_H_
#define ODP_API_ATOMIC_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup odp_atomic ODP ATOMIC
 * @details
 * <b> Atomic integers using relaxed memory ordering </b>
 *
 * Atomic integer types (odp_atomic_u32_t and odp_atomic_u64_t) can be used to
 * implement e.g. shared counters. If not otherwise documented, operations in
 * this API are implemented using <b> RELAXED memory ordering </b> (see memory
 * order descriptions in the C11 specification). Relaxed operations do not
 * provide synchronization or ordering for other memory accesses (initiated
 * before or after the operation), only atomicity of the operation itself is
 * guaranteed.
 *
 * <b> Operations with non-relaxed memory ordering </b>
 *
 * <b> An operation with RELEASE </b> memory ordering (odp_atomic_xxx_rel_xxx())
 * ensures that other threads loading the same atomic variable with ACQUIRE
 * memory ordering see all stores (from the calling thread) that happened before
 * this releasing store.
 *
 * <b> An operation with ACQUIRE </b> memory ordering (odp_atomic_xxx_acq_xxx())
 * ensures that the calling thread sees all stores (done by the releasing
 * thread) that happened before a RELEASE memory ordered store to the same
 * atomic variable.
 *
 * <b> An operation with ACQUIRE-and-RELEASE </b> memory ordering
 * (odp_atomic_xxx_acq_rel_xxx()) combines the effects of ACQUIRE and RELEASE
 * memory orders. A single operation acts as both an acquiring load and
 * a releasing store.
 *
 * @{
 */

/**
 * @typedef odp_atomic_u64_t
 * Atomic 64-bit unsigned integer
 *
 * @typedef odp_atomic_u32_t
 * Atomic 32-bit unsigned integer
 */

/*
 * 32-bit operations in RELAXED memory ordering
 * --------------------------------------------
 */

/**
 * Initialize atomic uint32 variable
 *
 * Initializes the atomic variable with 'val'. This operation is not atomic.
 * Application must ensure that there's no race condition while initializing
 * the variable.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to initialize the variable with
 */
void odp_atomic_init_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Load value of atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable
 */
uint32_t odp_atomic_load_u32(odp_atomic_u32_t *atom);

/**
 * Store value to atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to store in the variable
 */
void odp_atomic_store_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Fetch and add to atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be added to the variable
 *
 * @return Value of the variable before the addition
 */
uint32_t odp_atomic_fetch_add_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Add to atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be added to the variable
 */
void odp_atomic_add_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Fetch and subtract from atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be subracted from the variable
 *
 * @return Value of the variable before the subtraction
 */
uint32_t odp_atomic_fetch_sub_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Subtract from atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be subtracted from the variable
 */
void odp_atomic_sub_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Fetch and increment atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable before the increment
 */
uint32_t odp_atomic_fetch_inc_u32(odp_atomic_u32_t *atom);

/**
 * Increment atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 */
void odp_atomic_inc_u32(odp_atomic_u32_t *atom);

/**
 * Fetch and decrement atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable before the subtraction
 */
uint32_t odp_atomic_fetch_dec_u32(odp_atomic_u32_t *atom);

/**
 * Decrement atomic uint32 variable
 *
 * @param atom    Pointer to atomic variable
 */
void odp_atomic_dec_u32(odp_atomic_u32_t *atom);

/**
 * Update maximum value of atomic uint32 variable
 *
 * Compares value of atomic variable to the new maximum value. If the new value
 * is greater than the current value, writes the new value into the variable.
 *
 * @param atom    Pointer to atomic variable
 * @param new_max New maximum value to be written into the atomic variable
 */
void odp_atomic_max_u32(odp_atomic_u32_t *atom, uint32_t new_max);

/**
 * Update minimum value of atomic uint32 variable
 *
 * Compares value of atomic variable to the new minimum value. If the new value
 * is less than the current value, writes the new value into the variable.
 *
 * @param atom    Pointer to atomic variable
 * @param new_min New minimum value to be written into the atomic variable
 */
void odp_atomic_min_u32(odp_atomic_u32_t *atom, uint32_t new_min);

/**
 * Compare and swap atomic uint32 variable
 *
 * Compares value of atomic variable to the value pointed by 'old_val'.
 * If values are equal, the operation writes 'new_val' into the atomic variable
 * and returns success. If they are not equal, the operation writes current
 * value of atomic variable into 'old_val' and returns failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 *
 */
int odp_atomic_cas_u32(odp_atomic_u32_t *atom, uint32_t *old_val,
		       uint32_t new_val);

/**
 * Exchange value of atomic uint32 variable
 *
 * Atomically replaces the value of atomic variable with the new value. Returns
 * the old value.
 *
 * @param atom    Pointer to atomic variable
 * @param new_val New value of the atomic variable
 *
 * @return Value of the variable before the operation
 */
uint32_t odp_atomic_xchg_u32(odp_atomic_u32_t *atom, uint32_t new_val);

/*
 * 64-bit operations in RELAXED memory ordering
 * --------------------------------------------
 */

/**
 * Initialize atomic uint64 variable
 *
 * Initializes the atomic variable with 'val'. This operation is not atomic.
 * Application must ensure that there's no race condition while initializing
 * the variable.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to initialize the variable with
 */
void odp_atomic_init_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Load value of atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable
 */
uint64_t odp_atomic_load_u64(odp_atomic_u64_t *atom);

/**
 * Store value to atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to store in the variable
 */
void odp_atomic_store_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Fetch and add to atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be added to the variable
 *
 * @return Value of the variable before the addition
 */
uint64_t odp_atomic_fetch_add_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Add to atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be added to the variable
 */
void odp_atomic_add_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Fetch and subtract from atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be subtracted from the variable
 *
 * @return Value of the variable before the subtraction
 */
uint64_t odp_atomic_fetch_sub_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Subtract from atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be subtracted from the variable
 */
void odp_atomic_sub_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Fetch and increment atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable before the increment
 */
uint64_t odp_atomic_fetch_inc_u64(odp_atomic_u64_t *atom);

/**
 * Increment atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 */
void odp_atomic_inc_u64(odp_atomic_u64_t *atom);

/**
 * Fetch and decrement atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable before the decrement
 */
uint64_t odp_atomic_fetch_dec_u64(odp_atomic_u64_t *atom);

/**
 * Decrement atomic uint64 variable
 *
 * @param atom    Pointer to atomic variable
 */
void odp_atomic_dec_u64(odp_atomic_u64_t *atom);

/**
 * Update maximum value of atomic uint64 variable
 *
 * Compares value of atomic variable to the new maximum value. If the new value
 * is greater than the current value, writes the new value into the variable.
 *
 * @param atom    Pointer to atomic variable
 * @param new_max New maximum value to be written into the atomic variable
 */
void odp_atomic_max_u64(odp_atomic_u64_t *atom, uint64_t new_max);

/**
 * Update minimum value of atomic uint64 variable
 *
 * Compares value of atomic variable to the new minimum value. If the new value
 * is less than the current value, writes the new value into the variable.
 *
 * @param atom    Pointer to atomic variable
 * @param new_min New minimum value to be written into the atomic variable
 */
void odp_atomic_min_u64(odp_atomic_u64_t *atom, uint64_t new_min);

/**
 * Compare and swap atomic uint64 variable
 *
 * Compares value of atomic variable to the value pointed by 'old_val'.
 * If values are equal, the operation writes 'new_val' into the atomic variable
 * and returns success. If they are not equal, the operation writes current
 * value of atomic variable into 'old_val' and returns failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 */
int odp_atomic_cas_u64(odp_atomic_u64_t *atom, uint64_t *old_val,
		       uint64_t new_val);

/**
 * Exchange value of atomic uint64 variable
 *
 * Atomically replaces the value of atomic variable with the new value. Returns
 * the old value.
 *
 * @param atom    Pointer to atomic variable
 * @param new_val New value of the atomic variable
 *
 * @return Value of the variable before the operation
 */
uint64_t odp_atomic_xchg_u64(odp_atomic_u64_t *atom, uint64_t new_val);

/*
 * 32-bit operations in non-RELAXED memory ordering
 * ------------------------------------------------
 */

/**
 * Load value of atomic uint32 variable using ACQUIRE memory ordering
 *
 * Otherwise identical to odp_atomic_load_u32() but ensures ACQUIRE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable
 */
uint32_t odp_atomic_load_acq_u32(odp_atomic_u32_t *atom);

/**
 * Store value to atomic uint32 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_store_u32() but ensures RELEASE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to store in the variable
 */
void odp_atomic_store_rel_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Add to atomic uint32 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_add_u32() but ensures RELEASE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be added to the variable
 */
void odp_atomic_add_rel_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Subtract from atomic uint32 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_sub_u32() but ensures RELEASE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be subtracted from the variable
 */
void odp_atomic_sub_rel_u32(odp_atomic_u32_t *atom, uint32_t val);

/**
 * Compare and swap atomic uint32 variable using ACQUIRE memory ordering
 *
 * Otherwise identical to odp_atomic_cas_u32() but ensures ACQUIRE memory
 * ordering on success. Memory ordering is RELAXED on failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 */
int odp_atomic_cas_acq_u32(odp_atomic_u32_t *atom, uint32_t *old_val,
			   uint32_t new_val);

/**
 * Compare and swap atomic uint32 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_cas_u32() but ensures RELEASE memory
 * ordering on success. Memory ordering is RELAXED on failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 */
int odp_atomic_cas_rel_u32(odp_atomic_u32_t *atom, uint32_t *old_val,
			   uint32_t new_val);

/**
 * Compare and swap atomic uint32 variable using ACQUIRE-and-RELEASE memory
 * ordering
 *
 * Otherwise identical to odp_atomic_cas_u32() but ensures ACQUIRE-and-RELEASE
 * memory ordering on success. Memory ordering is RELAXED on failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 */
int odp_atomic_cas_acq_rel_u32(odp_atomic_u32_t *atom, uint32_t *old_val,
			       uint32_t new_val);

/*
 * 64-bit operations in non-RELAXED memory ordering
 * ------------------------------------------------
 */

/**
 * Load value of atomic uint64 variable using ACQUIRE memory ordering
 *
 * Otherwise identical to odp_atomic_load_u64() but ensures ACQUIRE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 *
 * @return Value of the variable
 */
uint64_t odp_atomic_load_acq_u64(odp_atomic_u64_t *atom);

/**
 * Store value to atomic uint64 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_store_u64() but ensures RELEASE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to store in the variable
 */
void odp_atomic_store_rel_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Add to atomic uint64 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_add_u64() but ensures RELEASE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be added to the variable
 */
void odp_atomic_add_rel_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Subtract from atomic uint64 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_sub_u64() but ensures RELEASE memory
 * ordering.
 *
 * @param atom    Pointer to atomic variable
 * @param val     Value to be subtracted from the variable
 */
void odp_atomic_sub_rel_u64(odp_atomic_u64_t *atom, uint64_t val);

/**
 * Compare and swap atomic uint64 variable using ACQUIRE memory ordering
 *
 * Otherwise identical to odp_atomic_cas_u64() but ensures ACQUIRE memory
 * ordering on success. Memory ordering is RELAXED on failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 */
int odp_atomic_cas_acq_u64(odp_atomic_u64_t *atom, uint64_t *old_val,
			   uint64_t new_val);

/**
 * Compare and swap atomic uint64 variable using RELEASE memory ordering
 *
 * Otherwise identical to odp_atomic_cas_u64() but ensures RELEASE memory
 * ordering on success. Memory ordering is RELAXED on failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 */
int odp_atomic_cas_rel_u64(odp_atomic_u64_t *atom, uint64_t *old_val,
			   uint64_t new_val);

/**
 * Compare and swap atomic uint64 variable using ACQUIRE-and-RELEASE memory
 * ordering
 *
 * Otherwise identical to odp_atomic_cas_u64() but ensures ACQUIRE-and-RELEASE
 * memory ordering on success. Memory ordering is RELAXED on failure.
 *
 * @param         atom      Pointer to atomic variable
 * @param[in,out] old_val   Pointer to the old value of the atomic variable.
 *                          Operation updates this value on failure.
 * @param         new_val   New value to be written into the atomic variable
 *
 * @return 0 on failure, !0 on success
 */
int odp_atomic_cas_acq_rel_u64(odp_atomic_u64_t *atom, uint64_t *old_val,
			       uint64_t new_val);

/**
 * Atomic operations
 *
 * Atomic operations listed in a bit field structure.
 */
typedef union odp_atomic_op_t {
	/** Operation flags */
	struct {
		uint32_t init      : 1;  /**< Init atomic variable */
		uint32_t load      : 1;  /**< Atomic load */
		uint32_t store     : 1;  /**< Atomic store */
		uint32_t fetch_add : 1;  /**< Atomic fetch and add */
		uint32_t add       : 1;  /**< Atomic add */
		uint32_t fetch_sub : 1;  /**< Atomic fetch and subtract */
		uint32_t sub       : 1;  /**< Atomic subtract */
		uint32_t fetch_inc : 1;  /**< Atomic fetch and increment */
		uint32_t inc       : 1;  /**< Atomic increment */
		uint32_t fetch_dec : 1;  /**< Atomic fetch and decrement */
		uint32_t dec       : 1;  /**< Atomic decrement */
		uint32_t min       : 1;  /**< Atomic minimum */
		uint32_t max       : 1;  /**< Atomic maximum */
		uint32_t cas       : 1;  /**< Atomic compare and swap */
		uint32_t xchg      : 1;  /**< Atomic exchange */
	} op;

	/** All bits of the bit field structure.
	  * Operation flag mapping is architecture specific. This field can be
	  * used to set/clear all flags, or bitwise operations over the entire
	  * structure. */
	uint32_t all_bits;
} odp_atomic_op_t;

/**
 * Query which atomic uint64 operations are lock-free
 *
 * Lock-free implementations have higher performance and scale better than
 * implementations using locks. User can decide to use e.g. uint32 atomic
 * variables instead of uint64 to optimize performance on platforms that
 * implement a performance critical operation using locks.
 *
 * Init operations (e.g. odp_atomic_init_64()) are not atomic. This function
 * clears the op.init bit but will never set it to one.
 *
 * @param atomic_op  Pointer to atomic operation structure for storing
 *                   operation flags. All bits are initialized to zero during
 *                   the operation. The parameter is ignored when NULL.
 * @retval 0 None of the operations are lock-free
 * @retval 1 Some of the operations are lock-free
 * @retval 2 All operations are lock-free
 */
int odp_atomic_lock_free_u64(odp_atomic_op_t *atomic_op);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
