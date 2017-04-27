/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP configuration
 */

#ifndef ODP_PLAT_CONFIG_H_
#define ODP_PLAT_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @ingroup odp_config ODP CONFIG
 * Platform-specific configuration limits
 * @{
 */

/**
 * Maximum number of pools
 */
#define ODP_CONFIG_POOLS 16
static inline int odp_config_pools(void)
{
	return ODP_CONFIG_POOLS;
}

/**
 * Maximum number of queues
 */
#define ODP_CONFIG_QUEUES 1024
static inline int odp_config_queues(void)
{
	return ODP_CONFIG_QUEUES;
}

/**
 * Number of ordered locks per queue
 */
#define ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE 2
static inline int odp_config_max_ordered_locks_per_queue(void)
{
	return ODP_CONFIG_MAX_ORDERED_LOCKS_PER_QUEUE;
}

/**
 * Number of scheduling priorities
 */
#define ODP_CONFIG_SCHED_PRIOS 8
static inline int odp_config_sched_prios(void)
{
	return ODP_CONFIG_SCHED_PRIOS;
}

/**
 * Number of scheduling groups
 */
#define ODP_CONFIG_SCHED_GRPS 256
static inline int odp_config_sched_grps(void)
{
	return ODP_CONFIG_SCHED_GRPS;
}

/**
 * Maximum number of packet IO resources
 */
#define ODP_CONFIG_PKTIO_ENTRIES 64
static inline int odp_config_pktio_entries(void)
{
	return ODP_CONFIG_PKTIO_ENTRIES;
}

/**
 * Minimum buffer alignment
 *
 * This defines the minimum supported buffer alignment. Requests for values
 * below this will be rounded up to this value.
 */
#define ODP_CONFIG_BUFFER_ALIGN_MIN 16
static inline int odp_config_buffer_align_min(void)
{
	return ODP_CONFIG_BUFFER_ALIGN_MIN;
}

/**
 * Maximum buffer alignment
 *
 * This defines the maximum supported buffer alignment. Requests for values
 * above this will fail.
 */
#define ODP_CONFIG_BUFFER_ALIGN_MAX (4 * 1024)
static inline int odp_config_buffer_align_max(void)
{
	return ODP_CONFIG_BUFFER_ALIGN_MAX;
}

/**
 * Default packet headroom
 *
 * This defines the minimum number of headroom bytes that newly created packets
 * have by default. The default apply to both ODP packet input and user
 * allocated packets. Implementations may reserve a larger than minimum headroom
 * size e.g. due to HW or a protocol specific alignment requirement.
 *
 * @internal In linux-generic implementation:
 * The default value (66) allows a 1500-byte packet to be received into a single
 * segment with Ethernet offset alignment and room for some header expansion.
 */
#define ODP_CONFIG_PACKET_HEADROOM 128
static inline int odp_config_packet_headroom(void)
{
	return ODP_CONFIG_PACKET_HEADROOM;
}

/**
 * Default packet tailroom
 *
 * This defines the minimum number of tailroom bytes that newly created packets
 * have by default. The default apply to both ODP packet input and user
 * allocated packets. Implementations are free to add to this as desired
 * without restriction. Note that most implementations will automatically
 * consider any unused portion of the last segment of a packet as tailroom
 */
#define ODP_CONFIG_PACKET_TAILROOM 64
static inline int odp_config_packet_tailroom(void)
{
	return ODP_CONFIG_PACKET_TAILROOM;
}

/**
 * Minimum packet segment length
 *
 * This defines the minimum packet segment buffer length in bytes. The user
 * defined segment length (seg_len in odp_pool_param_t) will be rounded up into
 * this value.
 */
#define ODP_CONFIG_PACKET_SEG_LEN_MIN 1598
static inline int odp_config_packet_seg_len_min(void)
{
	return ODP_CONFIG_PACKET_SEG_LEN_MIN;
}

/**
 * Maximum packet segment length
 *
 * This defines the maximum packet segment buffer length in bytes. The user
 * defined segment length (seg_len in odp_pool_param_t) must not be larger than
 * this.
 */
#define ODP_CONFIG_PACKET_SEG_LEN_MAX (64 * 1024)
static inline int odp_config_packet_seg_len_max(void)
{
	return ODP_CONFIG_PACKET_SEG_LEN_MAX;
}

/**
 * Maximum packet buffer length
 *
 * This defines the maximum number of bytes that can be stored into a packet
 * (maximum return value of odp_packet_buf_len(void)). Attempts to allocate
 * (including default head- and tailrooms) or extend packets to sizes larger
 * than this limit will fail.
 *
 * @internal In linux-generic implementation:
 * - The value MUST be an integral number of segments
 * - The value SHOULD be large enough to accommodate jumbo packets (9K)
 */
#define ODP_CONFIG_PACKET_BUF_LEN_MAX (ODP_CONFIG_PACKET_SEG_LEN_MIN * 6)
static inline int odp_config_packet_buf_len_max(void)
{
	return ODP_CONFIG_PACKET_BUF_LEN_MAX;
}

/** Maximum number of shared memory blocks.
 *
 * This the the number of separate SHM areas that can be reserved concurrently
 */
#define ODP_CONFIG_SHM_BLOCKS (ODP_CONFIG_POOLS + 48)
static inline int odp_config_shm_blocks(void)
{
	return ODP_CONFIG_SHM_BLOCKS;
}

#include <odp/api/config.h>

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
