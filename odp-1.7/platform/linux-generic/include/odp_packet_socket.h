/* Copyright (c) 2013, Linaro Limited
 * Copyright (c) 2013, Nokia Solutions and Networks
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_PACKET_SOCKET_H
#define ODP_PACKET_SOCKET_H

#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <string.h>

#include <odp/align.h>
#include <odp/buffer.h>
#include <odp/debug.h>
#include <odp/pool.h>
#include <odp/packet.h>
#include <odp/packet_io.h>

#include <linux/version.h>

/*
 * Packet socket config:
 */

/** Max receive (Rx) burst size*/
#define ODP_PACKET_SOCKET_MAX_BURST_RX 32
/** Max transmit (Tx) burst size*/
#define ODP_PACKET_SOCKET_MAX_BURST_TX 32

/*
 * This makes sure that building for kernels older than 3.1 works
 * and a fanout requests fails (for invalid packet socket option)
 * in runtime if requested
 */
#ifndef PACKET_FANOUT
#define PACKET_FANOUT		18
#define PACKET_FANOUT_HASH	0
#endif /* PACKET_FANOUT */

typedef struct {
	int sockfd; /**< socket descriptor */
	odp_pool_t pool; /**< pool to alloc packets from */
	unsigned char if_mac[ETH_ALEN];	/**< IF eth mac addr */
	uint8_t *cache_ptr[ODP_PACKET_SOCKET_MAX_BURST_RX];
	odp_shm_t shm;
} pkt_sock_t;

/** packet mmap ring */
struct ring {
	struct iovec *rd;
	unsigned frame_num;
	int rd_num;

	int sock;
	int type;
	int version;
	uint8_t *mm_space;
	size_t mm_len;
	size_t rd_len;
	int flen;

	struct tpacket_req req;
};
_ODP_STATIC_ASSERT(offsetof(struct ring, mm_space) <= ODP_CACHE_LINE_SIZE,
		   "ERR_STRUCT_RING");

/** Packet socket using mmap rings for both Rx and Tx */
typedef struct {
	/** Packet mmap ring for Rx */
	struct ring rx_ring ODP_ALIGNED_CACHE;
	/** Packet mmap ring for Tx */
	struct ring tx_ring ODP_ALIGNED_CACHE;

	int sockfd ODP_ALIGNED_CACHE;
	odp_pool_t pool;
	size_t frame_offset; /**< frame start offset from start of pkt buf */
	uint8_t *mmap_base;
	unsigned mmap_len;
	unsigned char if_mac[ETH_ALEN];
	struct sockaddr_ll ll;
	int fanout;
} pkt_sock_mmap_t;

static inline void
ethaddr_copy(unsigned char mac_dst[], unsigned char mac_src[])
{
	memcpy(mac_dst, mac_src, ETH_ALEN);
}

static inline int
ethaddrs_equal(unsigned char mac_a[], unsigned char mac_b[])
{
	return !memcmp(mac_a, mac_b, ETH_ALEN);
}

/**
 * Read the MAC address from a packet socket
 */
int mac_addr_get_fd(int fd, const char *name, unsigned char mac_dst[]);

/**
 * Read the MTU from a packet socket
 */
int mtu_get_fd(int fd, const char *name);

/**
 * Enable/Disable promisc mode for a packet socket
 */
int promisc_mode_set_fd(int fd, const char *name, int enable);

/**
 * Return promisc mode of a packet socket
 */
int promisc_mode_get_fd(int fd, const char *name);

/**
 * Return link status of a packet socket (up/down)
 */
int link_status_fd(int fd, const char *name);

/**
 * Get enabled RSS hash protocols of a packet socket
 *
 * @param fd              Socket file descriptor
 * @param name            Interface name
 * @param hash_proto[out] Hash protocols
 *
 * @returns Number enabled hash protocols
 */
int rss_conf_get_fd(int fd, const char *name,
		    odp_pktin_hash_proto_t *hash_proto);

/**
 * Get supported RSS hash protocols of a packet socket
 *
 * Can be both read and modified.
 *
 * @param fd              Socket file descriptor
 * @param name            Interface name
 * @param hash_proto[out] Hash protocols
 *
 * @returns Number of supported hash protocols
 */
int rss_conf_get_supported_fd(int fd, const char *name,
			      odp_pktin_hash_proto_t *hash_proto);

/**
 * Set RSS hash protocols of a packet socket
 *
 * @param fd              Socket file descriptor
 * @param name            Interface name
 * @param hash_proto      Hash protocols
 *
 * @retval 0 on success
 * @retval <0 on failure
 */
int rss_conf_set_fd(int fd, const char *name,
		    const odp_pktin_hash_proto_t *proto);

/**
 * Print enabled RSS hash protocols
 *
 * @param hash_proto      Hash protocols
 */
void rss_conf_print(const odp_pktin_hash_proto_t *hash_proto);

/**
 * Get ethtool statistics of a packet socket
 */
int ethtool_stats_get_fd(int fd, const char *name, odp_pktio_stats_t *stats);

#endif
