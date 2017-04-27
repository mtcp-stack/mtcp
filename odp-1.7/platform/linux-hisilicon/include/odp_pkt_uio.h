/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_PACKET_ODP_H
#define ODP_PACKET_ODP_H

#include <stdint.h>

/* #include <net/if.h> */

#include <odp/helper/eth.h>

/* #include <odp/helper/packet.h> */
#include <odp/align.h>
#include <odp/debug.h>
#include <odp/packet.h>
#include <odp_packet_internal.h>
#include <odp/pool.h>
#include <odp_pool_internal.h>
#include <odp_buffer_internal.h>
#include <odp/std_types.h>

#include <odp_config.h>
#include <odp_memory.h>
#include <odp_mmdistrict.h>

/* #include <odp_launch.h> */
#include <odp_tailq.h>
#include <odp_base.h>

/* #include <odp_per_core.h> */
#include <odp_core.h>

/* #include <odp_branch_prediction.h>
 #include <odp_prefetch.h> */
#include <odp_cycles.h>

/* #include <odp_err.h>
  #include <odp_debug.h>
#include <odp_log.h> */

/* #include <odp_byteorder.h> */
#include <odp_pci.h>

/* #include <odp_random.h> */
#include <odp_ether.h>
#include <odp_ethdev.h>

/* #include <odp_hash.h>
   #include <odp_jhash.h>
 #include <odp_hash_crc.h> */

#define ODP_ODP_MODE_HW 0
#define ODP_ODP_MODE_SW 1

#define ODP_BLOCKING_IO

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting ODP documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8    /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8    /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4    /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Huawei(R) 82599 10 GbE
 * Controller and the ODP ixgbe UMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36    /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0     /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0     /**< Default values of TX write-back threshold reg. */

#define BURST_TX_DRAIN_US	 100 /* TX drain every ~100us */
#define ODP_TEST_RX_DESC_DEFAULT 128
#define ODP_TEST_TX_DESC_DEFAULT 512

/** Packet socket using ODP mmaped rings for both Rx and Tx */
typedef struct {
	odp_pool_t pool;

	/********************************/
	char	 ifname[32];
	uint8_t	 portid;
	uint16_t queueid;
} pkt_odp_t;

/* int odp_init(void); */
#endif
