/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_packet_io_internal.h>

/* Ops for all implementation of pktio.
 * Order matters. The first implementation to setup successfully
 * will be picked.
 * Array must be NULL terminated */
const pktio_if_ops_t * const pktio_if_ops[]  = {
	&loopback_pktio_ops,
#ifdef ODP_NETMAP
	&netmap_pktio_ops,
#endif
#ifdef HAVE_PCAP
	&pcap_pktio_ops,
#endif
	/*&tap_pktio_ops,
	&sock_mmap_pktio_ops,
	&sock_mmsg_pktio_ops,*/
	&hns_eth_pktio_ops,
	/* &kni_pktio_ops, */
	NULL
};
