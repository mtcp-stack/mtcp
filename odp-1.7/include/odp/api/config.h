/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP configuration
 */

#ifndef ODP_API_CONFIG_H_
#define ODP_API_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HNS_PRINT
#define HNS_PRINT(fmt, ...) printf("¡¾Func: %s. Line: %d¡¿" fmt, __func__, __LINE__, \
			       ## __VA_ARGS__)
#endif

/** @defgroup odp_config ODP CONFIG
 *  Platform-specific configuration limits.
 *
 * @note The API calls defined for ODP configuration limits are the
 * normative means of accessing platform-specific configuration limits.
 * Platforms MAY in addition include \#defines for these limits for
 * internal use in dimensioning arrays, however there is no guarantee
 * that applications using such \#defines will be portable across all
 * ODP implementations. Applications SHOULD expect that over time such
 * \#defines will be deprecated and removed.
 *  @{
 */

/**
 * Maximum number of pools
 * @return The maximum number of pools supported by this platform
 */
int odp_config_pools(void);

/**
 * Maximum number of queues
 * @return The maximum number of queues supported by this platform
 */
int odp_config_queues(void);

/**
 * Maximum number of ordered locks per queue
 * @return The maximum number of ordered locks per queue supported by
 * this platform.
 */
int odp_config_max_ordered_locks_per_queue(void);

/**
 * Number of scheduling priorities
 * @return The number of scheduling priorities supported by this platform
 */
int odp_config_sched_prios(void);

/**
 * Number of scheduling groups
 * @return Number of scheduling groups supported by this platofmr
 */
int odp_config_sched_grps(void);

/**
 * Maximum number of packet IO resources
 * @return The maximum number of packet I/O resources supported by this
 * platform
 */
int odp_config_pktio_entries(void);

/**
 * Minimum buffer alignment
 *
 * @return The minimum buffer alignment supported by this platform
 * @note Requests for values below this will be rounded up to this value.
 */
int odp_config_buffer_align_min(void);

/**
 * Maximum buffer alignment
 *
 * This defines the maximum supported buffer alignment. Requests for values
 * above this will fail.
 *
 * @return The maximum buffer alignment supported by this platform.
 */
int odp_config_buffer_align_max(void);

/**
 * Default packet headroom
 *
 * This defines the minimum number of headroom bytes that newly created packets
 * have by default. The default apply to both ODP packet input and user
 * allocated packets. Implementations may reserve a larger than minimum headroom
 * size e.g. due to HW or a protocol specific alignment requirement.
 *
 * @return Default packet headroom in bytes
 */
int odp_config_packet_headroom(void);

/**
 * Default packet tailroom
 *
 * This defines the minimum number of tailroom bytes that newly created packets
 * have by default. The default apply to both ODP packet input and user
 * allocated packets. Implementations are free to add to this as desired
 * without restriction.
 *
 * @return The default packet tailroom in bytes
 */
int odp_config_packet_tailroom(void);

/**
 * Minimum packet segment length
 *
 * This defines the minimum packet segment buffer length in bytes. The user
 * defined segment length (seg_len in odp_pool_param_t) will be rounded up into
 * this value.
 *
 * @return The minimum packet seg_len supported by this platform
 */
int odp_config_packet_seg_len_min(void);

/**
 * Maximum packet segment length
 *
 * This defines the maximum packet segment buffer length in bytes. The user
 * defined segment length (seg_len in odp_pool_param_t) must not be larger than
 * this.
 *
 * @return The maximum packet seg_len supported by this platform
 */
int odp_config_packet_seg_len_max(void);

/**
 * Maximum packet buffer length
 *
 * This defines the maximum number of bytes that can be stored into a packet
 * (maximum return value of odp_packet_buf_len()). Attempts to allocate
 * (including default head- and tailrooms) or extend packets to sizes larger
 * than this limit will fail.
 *
 * @return The maximum packet buffer length supported by this platform
 */
int odp_config_packet_buf_len_max(void);

/** Maximum number of shared memory blocks.
 *
 * This the the number of separate SHM areas that can be reserved concurrently
 *
 * @return The maximum number of shm areas supported by this platform
 */
int odp_config_shm_blocks(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif
