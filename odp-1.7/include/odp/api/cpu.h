/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP CPU API
 */

#ifndef ODP_CPU_H_
#define ODP_CPU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>

/** @defgroup odp_cpu ODP CPU
 *  @{
 */


/**
 * CPU identifier
 *
 * Determine CPU identifier on which the calling is running. CPU numbering is
 * system specific.
 *
 * @return CPU identifier
 */
int odp_cpu_id(void);

/**
 * CPU count
 *
 * Report the number of CPU's available to this ODP program.
 * This may be smaller than the number of (online) CPU's in the system.
 *
 * @return Number of available CPU's
 */
int odp_cpu_count(void);

/**
 * CPU model name of this CPU
 *
 * Returns the CPU model name of this CPU.
 *
 * @return Pointer to CPU model name string
 */
const char *odp_cpu_model_str(void);

/**
 * CPU model name of a CPU
 *
 * Return CPU model name of the specified CPU.
 *
 * @param id    CPU ID
 *
 * @return Pointer to CPU model name string
 */
const char *odp_cpu_model_str_id(int id);

/**
 * Current CPU frequency in Hz
 *
 * Returns current frequency of this CPU
 *
 * @return CPU frequency in Hz
 * @retval 0 on failure
 */
uint64_t odp_cpu_hz(void);

/**
 * Current CPU frequency of a CPU (in Hz)
 *
 * Returns current frequency of specified CPU
 *
 * @param id    CPU ID
 *
 * @return CPU frequency in Hz
 * @retval 0 on failure
 */
uint64_t odp_cpu_hz_id(int id);

/**
 * Maximum CPU frequency in Hz
 *
 * Returns maximum frequency of this CPU
 *
 * @return CPU frequency in Hz
 * @retval 0 on failure
 */
uint64_t odp_cpu_hz_max(void);

/**
 * Maximum CPU frequency of a CPU (in Hz)
 *
 * Returns maximum frequency of specified CPU
 *
 * @param id    CPU ID
 *
 * @return CPU frequency in Hz
 * @retval 0 on failure
 */
uint64_t odp_cpu_hz_max_id(int id);

/**
 * Current CPU cycle count
 *
 * Return current CPU cycle count. Cycle count may not be reset at ODP init
 * and thus may wrap back to zero between two calls. Use odp_cpu_cycles_max()
 * to read the maximum count value after which it wraps. Cycle count frequency
 * follows the CPU frequency and thus may change at any time. The count may
 * advance in steps larger than one. Use odp_cpu_cycles_resolution() to read
 * the step size.
 *
 * @note Do not use CPU count for time measurements since the frequency may
 * vary.
 *
 * @return Current CPU cycle count
 */
uint64_t odp_cpu_cycles(void);

/**
 * CPU cycle count difference
 *
 * Calculate difference between cycle counts c1 and c2. Parameter c1 must be the
 * first cycle count sample and c2 the second. The function handles correctly
 * single cycle count wrap between c1 and c2.
 *
 * @param c2    Second cycle count
 * @param c1    First cycle count
 *
 * @return CPU cycles from c1 to c2
 */
uint64_t odp_cpu_cycles_diff(uint64_t c2, uint64_t c1);

/**
 * Maximum CPU cycle count
 *
 * Maximum CPU cycle count value before it wraps back to zero.
 *
 * @return Maximum CPU cycle count value
 */
uint64_t odp_cpu_cycles_max(void);

/**
 * Resolution of CPU cycle count
 *
 * CPU cycle count may advance in steps larger than one. This function returns
 * resolution of odp_cpu_cycles() in CPU cycles.
 *
 * @return CPU cycle count resolution in CPU cycles
 */
uint64_t odp_cpu_cycles_resolution(void);

/**
 * Pause CPU execution for a short while
 *
 * This call is intended for tight loops which poll a shared resource. A short
 * pause within the loop may save energy and improve system performance as
 * CPU polling frequency is reduced.
 */
void odp_cpu_pause(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
