/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_IPSEC_STREAM_H_
#define ODP_IPSEC_STREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp.h>
#include <odp_ipsec_misc.h>
#include <odp_ipsec_cache.h>

/**
 * Stream database entry structure
 */
typedef struct stream_db_entry_s {
	struct stream_db_entry_s *next; /**< Next entry on list */
	int              id;            /**< Stream ID */
	uint32_t         src_ip;        /**< Source IPv4 address */
	uint32_t         dst_ip;        /**< Destination IPv4 address */
	int              count;         /**< Packet count */
	uint32_t         length;        /**< Packet payload length */
	uint32_t         created;       /**< Number successfully created */
	uint32_t         verified;      /**< Number successfully verified */
	struct {
		int      loop;          /**< Input loop interface index */
		uint32_t ah_seq;        /**< AH sequence number if present */
		uint32_t esp_seq;       /**< ESP sequence number if present */
		ipsec_cache_entry_t *entry;  /**< IPsec to apply on input */
	} input;
	struct {
		int      loop;          /**< Output loop interface index */
		ipsec_cache_entry_t *entry;  /**t IPsec to verify on output */
	} output;
} stream_db_entry_t;

/**
 * Stream database
 */
typedef struct stream_db_s {
	uint32_t           index;          /**< Index of next available entry */
	stream_db_entry_t *list;           /**< List of active entries */
	stream_db_entry_t  array[MAX_DB];  /**< Entry storage */
} stream_db_t;

extern stream_db_t *stream_db;

/** Initialize stream database global control structure */
void init_stream_db(void);

/**
 * Create an stream DB entry
 *
 * String is of the format "SrcIP:DstIP:InInt:OutIntf:Count:Length"
 *
 * @param input  Pointer to string describing stream
 *
 * @return 0 if successful else -1
 */
int create_stream_db_entry(char *input);

/**
 * Resolve the stream DB against the IPsec input and output caches
 *
 * For each stream, look the source and destination IP address up in the
 * input and output IPsec caches.  If a hit is found, store the hit in
 * the stream DB to be used when creating packets.
 */
void resolve_stream_db(void);

/**
 * Create IPv4 packet for stream
 *
 * Create one ICMP test packet based on the stream structure.  If an input
 * IPsec cache entry is associated with the stream, build a packet that should
 * successfully match that entry and be correctly decoded by it.
 *
 * @param stream    Stream DB entry
 * @param dmac      Destination MAC address to use
 * @param pkt_pool  Packet buffer pool to allocate from
 *
 * @return packet else ODP_PACKET_INVALID
 */
odp_packet_t create_ipv4_packet(stream_db_entry_t *stream,
				uint8_t *dmac,
				odp_pool_t pkt_pool);

/**
 * Verify an IPv4 packet received on a loop output queue
 *
 * @todo Better error checking, add counters, add tracing,
 *       remove magic numbers, add order verification
 *       (see https://bugs.linaro.org/show_bug.cgi?id=620)
 *
 * @param stream  Stream to verify the packet against
 * @param pkt     Packet to verify
 *
 * @return TRUE if packet verifies else FALSE
 */
odp_bool_t verify_ipv4_packet(stream_db_entry_t *stream,
			      odp_packet_t pkt);

/**
 * Create input packets based on the stream DB
 *
 * Create input packets based on the configured streams and enqueue them
 * into loop interface input queues.  Once packet processing starts these
 * packets will be remomved and processed as if they had come from a normal
 * packet interface.
 *
 * @return number of streams successfully processed
 */
int create_stream_db_inputs(void);

/**
 * Verify stream DB outputs
 *
 * For each stream, poll the output loop interface queue and verify
 * any packets found on it
 *
 * @return TRUE if all packets on all streams verified else FALSE
 */
odp_bool_t verify_stream_db_outputs(void);

#ifdef __cplusplus
}
#endif

#endif
