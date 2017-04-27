/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP Implementation information
 */

#ifndef ODP_IMPL_H_
#define ODP_IMPL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/version.h>

#define ODP_VERSION_IMPL 0
#define ODP_VERSION_IMPL_STR \
	ODP_VERSION_TO_STR(PLATFORM) " " \
	ODP_VERSION_TO_STR(ODP_VERSION_API_GENERATION) "." \
	ODP_VERSION_TO_STR(ODP_VERSION_API_MAJOR) "." \
	ODP_VERSION_TO_STR(ODP_VERSION_API_MINOR) "-" \
	ODP_VERSION_TO_STR(ODP_VERSION_IMPL) " (v" \
	ODP_VERSION_TO_STR(ODP_VERSION_API_GENERATION) "." \
	ODP_VERSION_TO_STR(ODP_VERSION_API_MAJOR) "." \
	ODP_VERSION_TO_STR(ODP_VERSION_API_MINOR) ") " \
	__DATE__ " " __TIME__ " " \
	ODP_VERSION_TO_STR(GIT_HASH)

const char *odp_version_impl_str(void)
{
	return ODP_VERSION_IMPL_STR;
}

#ifdef __cplusplus
}
#endif
#endif
