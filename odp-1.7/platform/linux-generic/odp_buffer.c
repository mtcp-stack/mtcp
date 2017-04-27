/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/buffer.h>
#include <odp_pool_internal.h>
#include <odp_buffer_internal.h>
#include <odp_buffer_inlines.h>
#include <odp_debug_internal.h>

#include <string.h>
#include <stdio.h>


odp_buffer_t odp_buffer_from_event(odp_event_t ev)
{
	return (odp_buffer_t)ev;
}

odp_event_t odp_buffer_to_event(odp_buffer_t buf)
{
	return (odp_event_t)buf;
}

void *odp_buffer_addr(odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr = odp_buf_to_hdr(buf);

	return hdr->addr[0];
}


uint32_t odp_buffer_size(odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr = odp_buf_to_hdr(buf);

	return hdr->size;
}

int odp_buffer_is_valid(odp_buffer_t buf)
{
	return validate_buf(buf) != NULL;
}


int odp_buffer_snprint(char *str, uint32_t n, odp_buffer_t buf)
{
	odp_buffer_hdr_t *hdr;
	int len = 0;

	if (!odp_buffer_is_valid(buf)) {
		ODP_PRINT("Buffer is not valid.\n");
		return len;
	}

	hdr = odp_buf_to_hdr(buf);

	len += snprintf(&str[len], n-len,
			"Buffer\n");
	len += snprintf(&str[len], n-len,
			"  pool         %" PRIu64 "\n",
			odp_pool_to_u64(hdr->pool_hdl));
	len += snprintf(&str[len], n-len,
			"  addr         %p\n",        hdr->addr);
	len += snprintf(&str[len], n-len,
			"  size         %" PRIu32 "\n",        hdr->size);
	len += snprintf(&str[len], n-len,
			"  ref_count    %" PRIu32 "\n",
			odp_atomic_load_u32(&hdr->ref_count));
	len += snprintf(&str[len], n-len,
			"  type         %i\n",        hdr->type);

	return len;
}


void odp_buffer_print(odp_buffer_t buf)
{
	int max_len = 512;
	char str[max_len];
	int len;

	len = odp_buffer_snprint(str, max_len-1, buf);
	str[len] = 0;

	ODP_PRINT("\n%s\n", str);
}
