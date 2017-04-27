/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP event
 */

#ifndef ODP_API_EVENT_H_
#define ODP_API_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif


/** @defgroup odp_event ODP EVENT
 *  Operations on an event.
 *  @{
 */


/**
 * @typedef odp_event_t
 * ODP event
 */

/**
 * @def ODP_EVENT_INVALID
 * Invalid event
 */

/**
 * @typedef odp_event_type_t
 * ODP event types:
 * ODP_EVENT_BUFFER, ODP_EVENT_PACKET, ODP_EVENT_TIMEOUT,
 * ODP_EVENT_CRYPTO_COMPL
 */

/**
 * Get event type
 *
 * @param event    Event handle
 *
 * @return Event type
 */
odp_event_type_t odp_event_type(odp_event_t event);

/**
 * Get printable value for an odp_event_t
 *
 * @param hdl  odp_event_t handle to be printed
 * @return     uint64_t value that can be used to print/display this
 *             handle
 *
 * @note This routine is intended to be used for diagnostic purposes
 * to enable applications to generate a printable value that represents
 * an odp_event_t handle.
 */
uint64_t odp_event_to_u64(odp_event_t hdl);

/**
 * Free event
 *
 * Frees the event based on its type. Results are undefined if event
 * type is unknown.
 *
 * @param event    Event handle
 *
 */
void odp_event_free(odp_event_t event);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
