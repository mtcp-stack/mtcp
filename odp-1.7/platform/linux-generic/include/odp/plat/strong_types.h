/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP Strong Types. Common macros for implementing strong typing
 * for ODP abstract data types
 */

#ifndef STRONG_TYPES_H_
#define STRONG_TYPES_H_

/** Use strong typing for ODP types */
#ifdef __cplusplus
#define ODP_HANDLE_T(type) struct _##type { uint8_t unused_dummy_var; } *type
#else
#define odp_handle_t struct { uint8_t unused_dummy_var; } *
/** C/C++ helper macro for strong typing */
#define ODP_HANDLE_T(type) odp_handle_t type
#endif

/** Internal macro to get value of an ODP handle */
#define _odp_typeval(handle) ((uint32_t)(uintptr_t)(handle))

/** Internal macro to get printable value of an ODP handle */
#define _odp_pri(handle) ((uint64_t)_odp_typeval(handle))

/** Internal macro to convert a scalar to a typed handle */
#define _odp_cast_scalar(type, val) ((type)(uintptr_t)(val))

#endif
