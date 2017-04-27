/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP version
 */

#ifndef ODP_API_VERSION_H_
#define ODP_API_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup odp_version ODP VERSION
 * @details
 * <b> ODP API and implementation versions </b>
 *
 * ODP API version is identified by ODP_VERSION_API_XXX pre-processor macros.
 * In addition to these macros, API calls can be used to identify implementation
 * and API version information at run time.
 * @{
 */

/**
 * ODP API generation version
 *
 * Introduction of major new features or changes that make
 * very significant changes to the API. APIs with different
 * versions are likely not backward compatible.
 */
#define ODP_VERSION_API_GENERATION 1

/**
 * ODP API major version
 *
 * Introduction of major new features or changes. APIs with different major
 * versions are likely not backward compatible.
 */
#define ODP_VERSION_API_MAJOR 7

/**
 * ODP API minor version
 *
 * Minor version is incremented when introducing backward compatible changes
 * to the API. For an API with common generation and major version, but with
 * different minor numbers the two versions are backward compatible.
 */
#define ODP_VERSION_API_MINOR 0

/**
 * ODP API version string
 *
 * The API version string defines ODP API version in this format:
 * @verbatim <generation>.<major>.<minor> @endverbatim
 *
 * The string is null terminated.
 *
 * @return Pointer to API version string
 */
const char *odp_version_api_str(void);

/**
 * Implementation name string
 *
 * This is a free format string which identifies the implementation with a
 * unique name. The string should be compact and remain constant over multiple
 * API and implementation versions. Application can use this to identify the
 * underlying implementation at run time. The string is null terminated.
 *
 * @return Pointer to implementation name string
  */
const char *odp_version_impl_name(void);

/**
 * Implementation version string
 *
 * This is a free format string which identifies the implementation with
 * detailed version information. The string may include information about
 * implementation version, bug fix level, version control tags, build number,
 * etc details which exactly identify the implementation code base.
 * User may include this information e.g. to bug reports. The string is null
 * terminated.
 *
 * @see odp_version_api_str(), odp_version_impl_name()
 *
 * @return Pointer to implementation specific version identifier string
  */
const char *odp_version_impl_str(void);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
