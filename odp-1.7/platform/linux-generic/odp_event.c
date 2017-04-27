/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/event.h>
#include <odp/buffer.h>
#include <odp/crypto.h>
#include <odp/packet.h>
#include <odp/timer.h>
#include <odp/pool.h>
#include <odp_buffer_internal.h>
#include <odp_buffer_inlines.h>
#include <odp_debug_internal.h>

odp_event_type_t odp_event_type(odp_event_t event)
{
	return _odp_buffer_event_type(odp_buffer_from_event(event));
}

void odp_event_free(odp_event_t event)
{
	switch (odp_event_type(event)) {
	case ODP_EVENT_BUFFER:
		odp_buffer_free(odp_buffer_from_event(event));
		break;
	case ODP_EVENT_PACKET:
		odp_packet_free(odp_packet_from_event(event));
		break;
	case ODP_EVENT_TIMEOUT:
		odp_timeout_free(odp_timeout_from_event(event));
		break;
	case ODP_EVENT_CRYPTO_COMPL:
		odp_crypto_compl_free(odp_crypto_compl_from_event(event));
		break;
	default:
		ODP_ABORT("Invalid event type: %d\n", odp_event_type(event));
	}
}
