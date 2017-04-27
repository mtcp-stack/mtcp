/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP Classification Internal
 * Describes the classification internal Functions
 */

#ifndef __ODP_CLASSIFICATION_INTERNAL_H_
#define __ODP_CLASSIFICATION_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/classification.h>
#include <odp/queue.h>
#include <odp_packet_internal.h>
#include <odp/packet_io.h>
#include <odp_packet_io_internal.h>
#include <odp_classification_datamodel.h>

/** Classification Internal function **/

/**
@internal
Select a CoS for the given Packet based on pktio

This function will call all the PMRs associated with a pktio for
a given packet and will return the matched COS object.
This function will check PMR, L2 and L3 QoS COS object associated
with the PKTIO interface.

Returns the default cos if the packet does not match any PMR
Returns the error_cos if the packet has an error
**/
cos_t *pktio_select_cos(pktio_entry_t *pktio, int queue_id,
			const uint8_t *pkt_addr,
			odp_packet_hdr_t *pkt_hdr);

/**
@internal
match_qos_cos

Select a CoS for the given Packet based on QoS values
This function returns the COS object matching the L2 and L3 QoS
based on the l3_preference value of the pktio
**/
cos_t *match_qos_cos(pktio_entry_t *entry, const uint8_t *pkt_addr,
		     odp_packet_hdr_t *hdr);
/**
Packet Classifier

Start function for Packet Classifier
This function calls Classifier module internal functions for a given packet and
enqueues the packet to specific Queue based on PMR and CoS selected.
The packet is allocated from the pool associated with the CoS
**/
int packet_classifier(odp_pktio_t pktio, odp_packet_t pkt);

/**
@internal

Same as packet classifier uses linux-generic internal pktio struct
**/
int _odp_packet_classifier(pktio_entry_t *entry, int queue_id,
			   odp_packet_t pkt);

/**
Packet IO classifier init

This function does initialization of classifier object associated with pktio.
This function should be called during pktio initialization.
**/
int pktio_classifier_init(pktio_entry_t *pktio);

/**
@internal
match_pmr_cos

Match a PMR chain with a Packet and return matching CoS
This function gets called recursively to check the chained PMR Term value
with the packet.

**/
cos_t *match_pmr_cos(cos_t *cos, const uint8_t *pkt_addr, pmr_t *pmr,
		     odp_packet_hdr_t *hdr);
/**
@internal
CoS associated with L3 QoS value

This function returns the CoS associated with L3 QoS value
**/
cos_t *match_qos_l3_cos(pmr_l3_cos_t *l3_cos, const uint8_t *pkt_addr,
			odp_packet_hdr_t *hdr);

/**
@internal
CoS associated with L2 QoS value

This function returns the CoS associated with L2 QoS value
**/
cos_t *match_qos_l2_cos(pmr_l2_cos_t *l2_cos, const uint8_t *pkt_addr,
			odp_packet_hdr_t *hdr);
/**
@internal
Flow Signature Calculation

This function calculates the Flow Signature for a packet based on
CoS and updates in Packet Meta Data
**/
int update_flow_signature(uint8_t *pkt_addr, cos_t *cos);

/**
@internal
Allocate a odp_pmr_set_t Handle
*/
odp_pmr_set_t alloc_pmr_set(pmr_t **pmr);

/**
@internal
Allocate a odp_pmr_t Handle
*/
odp_pmr_t alloc_pmr(pmr_t **pmr);

/**
@internal
Pointer to pmr_set_t Handle
This function checks for validity of pmr_set_t Handle
*/
pmr_set_t *get_pmr_set_entry(odp_pmr_set_t pmr_set_id);

/**
@internal
Pointer to pmr_set_t Handle
*/
pmr_set_t *get_pmr_set_entry_internal(odp_pmr_set_t pmr_set_id);

/**
@internal
Pointer to pmr_set_t Handle
This function checks for validity of pmr_set_t Handle
*/
pmr_t *get_pmr_entry(odp_pmr_t pmr_id);

/**
@internal
Pointer to pmr_set_t Handle
*/
pmr_t *get_pmr_entry_internal(odp_pmr_t pmr_id);

/**
@internal
Pointer to odp_cos_t Handle
*/
cos_t *get_cos_entry(odp_cos_t cos_id);

/**
@internal
Pointer to odp_cos_t Handle
This function checks for validity of odp_cos_t Handle
*/
cos_t *get_cos_entry_internal(odp_cos_t cos_id);

/**
@internal
Verify PMR with a Packet

This function goes through each PMR_TERM value in pmr_t structure and
calls verification function for each term.Returns 1 if PMR matches or 0
Otherwise.
**/
int verify_pmr(pmr_t *pmr, const uint8_t *pkt_addr, odp_packet_hdr_t *pkt_hdr);

#ifdef __cplusplus
}
#endif
#endif
