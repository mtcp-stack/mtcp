/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP Packet IO
 */

#ifndef ODP_API_PACKET_IO_STATS_H_
#define ODP_API_PACKET_IO_STATS_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_packet_io
 *  @{
 */

/**
 * Packet IO statistics
 *
 * Packet IO statictics counters follow RFCs for Management Information Base
 * (MIB)for use with network management protocols in the Internet community:
 * https://tools.ietf.org/html/rfc3635
 * https://tools.ietf.org/html/rfc2863
 * https://tools.ietf.org/html/rfc2819
 */
typedef struct odp_pktio_stats_t {
	/**
	 * The number of octets in valid MAC frames received on this interface,
	 * including the MAC header and FCS. See ifHCInOctets counter
	 * description in RFC 3635 for details.
	 */
	uint64_t in_octets;

	/**
	 * The number of packets, delivered by this sub-layer to a higher
	 * (sub-)layer, which were not addressed to a multicast or broadcast
	 * address at this sub-layer. See ifHCInUcastPkts in RFC 2863, RFC 3635.
	 */
	uint64_t in_ucast_pkts;

	/**
	 * The number of inbound packets which were chosen to be discarded
	 * even though no errors had been detected to preven their being
	 * deliverable to a higher-layer protocol.  One possible reason for
	 * discarding such a packet could be to free up buffer space.
	 * See ifInDiscards in RFC 2863.
	 */
	uint64_t in_discards;

	/**
	 * The sum for this interface of AlignmentErrors, FCSErrors, FrameTooLongs,
	 * InternalMacReceiveErrors. See ifInErrors in RFC 3635.
	 */
	uint64_t in_errors;

	/**
	 * For packet-oriented interfaces, the number of packets received via
	 * the interface which were discarded because of an unknown or
	 * unsupported protocol.  For character-oriented or fixed-length
	 * interfaces that support protocol multiplexing the number of
	 * transmission units received via the interface which were discarded
	 * because of an unknown or unsupported protocol.  For any interface
	 * that does not support protocol multiplexing, this counter will always
	 * be 0. See ifInUnknownProtos in RFC 2863, RFC 3635.
	 */
	uint64_t in_unknown_protos;

	/**
	 * The number of octets transmitted in valid MAC frames on this
	 * interface, including the MAC header and FCS.  This does include
	 * the number of octets in valid MAC Control frames transmitted on
	 * this interface. See ifHCOutOctets in RFC 3635.
	 */
	uint64_t out_octets;

	/**
	 * The total number of packets that higher-level protocols requested
	 * be transmitted, and which were not addressed to a multicast or
	 * broadcast address at this sub-layer, including those that were
	 * discarded or not sent. does not include MAC Control frames.
	 * See ifHCOutUcastPkts RFC 2863, 3635.
	 */
	uint64_t out_ucast_pkts;

	/**
	 * The number of outbound packets which were chosen to be discarded
	 * even though no errors had been detected to prevent their being
	 * transmitted.  One possible reason for discarding such a packet could
	 * be to free up buffer space.  See  OutDiscards in  RFC 2863.
	 */
	uint64_t out_discards;

	/**
	 * The sum for this interface of SQETestErrors, LateCollisions,
	 * ExcessiveCollisions, InternalMacTransmitErrors and
	 * CarrierSenseErrors. See ifOutErrors in RFC 3635.
	 */
	uint64_t out_errors;
} odp_pktio_stats_t;

/**
 * Get statistics for pktio handle
 *
 * @param	pktio	 Packet IO handle
 * @param[out]	stats	 Output buffer for counters
 * @retval  0 on success
 * @retval <0 on failure
 *
 * @note: If counter is not supported by platform it has
 *	  to be set to 0.
 */
int odp_pktio_stats(odp_pktio_t pktio,
		    odp_pktio_stats_t *stats);

/**
 * Reset statistics for pktio handle
 *
 * Reset all pktio counters to 0.
 * @param	pktio	 Packet IO handle
 * @retval  0 on success
 * @retval <0 on failure
 *
 */
int odp_pktio_stats_reset(odp_pktio_t pktio);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
