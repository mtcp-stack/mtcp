/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP buffer descriptor
 */

#ifndef ODP_BUFFER_TYPES_H_
#define ODP_BUFFER_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/plat/strong_types.h>

/** ODP buffer */
typedef ODP_HANDLE_T(odp_buffer_t);


/** Invalid buffer */
#define ODP_BUFFER_INVALID _odp_cast_scalar(odp_buffer_t, 0x0)



/** ODP buffer segment */
typedef ODP_HANDLE_T(odp_buffer_seg_t);

/** Invalid segment */
#define ODP_SEGMENT_INVALID ((odp_buffer_seg_t)ODP_BUFFER_INVALID)

/** Get printable format of odp_buffer_t */
static inline uint64_t odp_buffer_to_u64(odp_buffer_t hdl)
{
	return _odp_pri(hdl);
}

#ifdef __cplusplus
}
#endif

#endif
