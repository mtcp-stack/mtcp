/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP CPU masks and enumeration
 */

#ifndef ODP_API_CPUMASK_H_
#define ODP_API_CPUMASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/config.h>

/** @defgroup odp_cpumask ODP CPUMASK
 *  CPU mask operations.
 *  @{
 */

/**
 * @def ODP_CPUMASK_SIZE
 * Maximum cpumask size, this definition limits the number of individual CPUs
 * that can be accessed in this system.
 */

/**
 * @def ODP_CPUMASK_STR_SIZE
 * The maximum number of characters needed to record any CPU mask as
 * a string (output of odp_cpumask_to_str()).
 */

/**
 * Add CPU mask bits from a string
 *
 * Each bit set in the string represents a CPU to be added into the mask.
 * The string is null terminated and consists of hexadecimal digits. It may be
 * prepended with '0x' and may contain leading zeros (e.g. 0x0001, 0x1 or 1).
 * CPU #0 is located at the least significant bit (0x1).
 *
 * @param mask   CPU mask to modify
 * @param str    String of hexadecimal digits
 */
void odp_cpumask_from_str(odp_cpumask_t *mask, const char *str);

/**
 * Format a string from CPU mask
 *
 * Output string format is defined in odp_cpumask_from_str() documentation,
 * except that the string is always prepended with '0x' and does not have any
 * leading zeros (e.g. outputs always 0x1 instead of 0x0001 or 1).
 *
 * @param      mask  CPU mask
 * @param[out] str   String pointer for output
 * @param      size  Size of output buffer. Buffer size ODP_CPUMASK_STR_SIZE
 *                   or larger will have enough space for any CPU mask.
 *
 * @return Number of characters written (including terminating null char)
 * @retval <0 on failure (e.g. buffer too small)
 */
int32_t odp_cpumask_to_str(const odp_cpumask_t *mask, char *str, int32_t size);

/**
 * Clear entire CPU mask
 * @param mask CPU mask to clear
 */
void odp_cpumask_zero(odp_cpumask_t *mask);

/**
 * Add CPU to mask
 * @param mask  CPU mask to update
 * @param cpu   CPU number
 */
void odp_cpumask_set(odp_cpumask_t *mask, int cpu);

/**
 * Set all CPUs in mask
 *
 * Set all possible CPUs in the mask. All CPUs from 0 to odp_cpumask_count()
 * minus one are set, regardless of which CPUs are actually available to
 * the application.
 *
 * @param mask  CPU mask to set
 */
void odp_cpumask_setall(odp_cpumask_t *mask);

/**
 * Remove CPU from mask
 * @param mask  CPU mask to update
 * @param cpu   CPU number
 */
void odp_cpumask_clr(odp_cpumask_t *mask, int cpu);

/**
 * Test if CPU is a member of mask
 *
 * @param mask  CPU mask to test
 * @param cpu   CPU number
 * @return      non-zero if set
 * @retval      0 if not set
 */
int odp_cpumask_isset(const odp_cpumask_t *mask, int cpu);

/**
 * Count number of CPUs set in mask
 *
 * @param mask  CPU mask
 * @return population count
 */
int odp_cpumask_count(const odp_cpumask_t *mask);

/**
 * Member-wise AND over two CPU masks
 *
 * @param dest    Destination CPU mask (may be one of the source masks)
 * @param src1    Source CPU mask 1
 * @param src2    Source CPU mask 2
 */
void odp_cpumask_and(odp_cpumask_t *dest, const odp_cpumask_t *src1,
		     const odp_cpumask_t *src2);

/**
 * Member-wise OR over two CPU masks
 *
 * @param dest    Destination CPU mask (may be one of the source masks)
 * @param src1    Source CPU mask 1
 * @param src2    Source CPU mask 2
 */
void odp_cpumask_or(odp_cpumask_t *dest, const odp_cpumask_t *src1,
		    const odp_cpumask_t *src2);

/**
 * Member-wise XOR over two CPU masks
 *
 * @param dest    Destination CPU mask (may be one of the source masks)
 * @param src1    Source CPU mask 1
 * @param src2    Source CPU mask 2
 */
void odp_cpumask_xor(odp_cpumask_t *dest, const odp_cpumask_t *src1,
		     const odp_cpumask_t *src2);

/**
 * Test if two CPU masks contain the same CPUs
 *
 * @param mask1    CPU mask 1
 * @param mask2    CPU mask 2
 *
 * @retval non-zero if CPU masks equal
 * @retval 0 if CPU masks not equal
 */
int odp_cpumask_equal(const odp_cpumask_t *mask1,
		      const odp_cpumask_t *mask2);

/**
 * Copy a CPU mask
 *
 * @param dest    Destination CPU mask
 * @param src     Source CPU mask
 */
void odp_cpumask_copy(odp_cpumask_t *dest, const odp_cpumask_t *src);

/**
 * Find first set CPU in mask
 *
 * @param mask    CPU mask
 *
 * @return cpu number
 * @retval <0 if no CPU found
 */
int odp_cpumask_first(const odp_cpumask_t *mask);

/**
 * Find last set CPU in mask
 *
 * @param mask    CPU mask
 *
 * @return cpu number
 * @retval <0 if no CPU found
 */
int odp_cpumask_last(const odp_cpumask_t *mask);

/**
 * Find next set CPU in mask
 *
 * Finds the next CPU in the CPU mask, starting at the CPU passed.
 * Use with odp_cpumask_first to traverse a CPU mask, e.g.
 *
 * int cpu = odp_cpumask_first(&mask);
 * while (0 <= cpu) {
 *     ...
 *     ...
 *     cpu = odp_cpumask_next(&mask, cpu);
 * }
 *
 * @param mask        CPU mask
 * @param cpu         CPU to start from
 *
 * @return cpu number
 * @retval <0 if no CPU found
 *
 * @see odp_cpumask_first()
 */
int odp_cpumask_next(const odp_cpumask_t *mask, int cpu);

/**
 * Default cpumask for worker threads
 *
 * Initializes cpumask with CPUs available for worker threads. Sets up to 'num'
 * CPUs and returns the count actually set. Use zero for all available CPUs.
 *
 * @param[out] mask      CPU mask to initialize
 * @param      num       Number of worker threads, zero for all available CPUs
 * @return Actual number of CPUs used to create the mask
 */
int odp_cpumask_default_worker(odp_cpumask_t *mask, int num);

/**
 * Default cpumask for control threads
 *
 * Initializes cpumask with CPUs available for control threads. Sets up to 'num'
 * CPUs and returns the count actually set. Use zero for all available CPUs.
 *
 * @param[out] mask      CPU mask to initialize
 * @param      num       Number of control threads, zero for all available CPUs
 * @return Actual number of CPUs used to create the mask
 */
int odp_cpumask_default_control(odp_cpumask_t *mask, int num);

/**
 * Report all the available CPUs
 *
 * All the available CPUs include both worker CPUs and control CPUs
 *
 * @param[out] mask    CPU mask to hold all available CPUs
 * @return cpu number of all available CPUs
 */
int odp_cpumask_all_available(odp_cpumask_t *mask);


/**
 * unbind the process or thread to the cpu
 *
 * 
 *
 * @param[in] cpu    cpu id
 * @return 0 sucess or fail
 */
int odp_cpumask_unbind_cpu(int cpu);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
