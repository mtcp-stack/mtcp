/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_VERSION_TYPESH_
#define ODP_VERSION_TYPESH_

#ifdef __cplusplus
extern "C" {
#endif

/** @internal Version string expand */
#define ODP_VERSION_STR_EXPAND(x)  #x

/** @internal Version to string */
#define ODP_VERSION_TO_STR(x)      ODP_VERSION_STR_EXPAND(x)

/** @internal API version string */
#define ODP_VERSION_API_STR \
ODP_VERSION_TO_STR(ODP_VERSION_API_GENERATION) "." \
ODP_VERSION_TO_STR(ODP_VERSION_API_MAJOR) "." \
ODP_VERSION_TO_STR(ODP_VERSION_API_MINOR)

#ifdef __cplusplus
}
#endif

#endif
