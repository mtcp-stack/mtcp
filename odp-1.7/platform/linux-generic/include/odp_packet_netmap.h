/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_PACKET_NETMAP_H
#define ODP_PACKET_NETMAP_H

#include <odp/align.h>
#include <odp/debug.h>
#include <odp/packet_io.h>
#include <odp/pool.h>
#include <odp/ticketlock.h>
#include <odp_align_internal.h>

#include <linux/if_ether.h>
#include <net/if.h>

#define NM_MAX_DESC 32

/** Ring for mapping pktin/pktout queues to netmap descriptors */
struct netmap_ring_t {
	unsigned first; /**< Index of first netmap descriptor */
	unsigned last;  /**< Index of last netmap descriptor */
	unsigned num;   /**< Number of netmap descriptors */
	/** Netmap metadata for the device */
	struct nm_desc *desc[NM_MAX_DESC];
	unsigned cur;		/**< Index of current netmap descriptor */
	odp_ticketlock_t lock;  /**< Queue lock */
};

typedef union {
	struct netmap_ring_t s;
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct netmap_ring_t))];
} netmap_ring_t ODP_ALIGNED_CACHE;

/** Packet socket using netmap mmaped rings for both Rx and Tx */
typedef struct {
	odp_pool_t pool;		/**< pool to alloc packets from */
	size_t max_frame_len;		/**< buf_size - sizeof(pkt_hdr) */
	uint32_t if_flags;		/**< interface flags */
	uint32_t mtu;			/**< maximum transmission unit */
	int sockfd;			/**< control socket */
	unsigned char if_mac[ETH_ALEN]; /**< eth mac address */
	char nm_name[IF_NAMESIZE + 7];  /**< netmap:<ifname> */
	odp_pktio_capability_t	capa;	/**< interface capabilities */
	unsigned cur_rx_queue;		/**< current pktin queue */
	uint32_t num_rx_rings;		/**< number of nm rx rings */
	uint32_t num_tx_rings;		/**< number of nm tx rings */
	unsigned num_rx_desc_rings;	/**< number of rx descriptor rings */
	unsigned num_tx_desc_rings;	/**< number of tx descriptor rings */
	odp_bool_t lockless_rx;		/**< no locking for rx */
	odp_bool_t lockless_tx;		/**< no locking for tx */
	/** mapping of pktin queues to netmap rx descriptors */
	netmap_ring_t rx_desc_ring[PKTIO_MAX_QUEUES];
	/** mapping of pktout queues to netmap tx descriptors */
	netmap_ring_t tx_desc_ring[PKTIO_MAX_QUEUES];
} pkt_netmap_t;

#endif
