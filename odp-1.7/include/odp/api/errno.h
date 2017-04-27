/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

/**
 * @file
 *
 * ODP errno API
 */

#ifndef ODP_ERRNO_H_
#define ODP_ERRNO_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup odp_errno ODP ERRNO
 * @details
 * <b> ODP errno </b>
 *
 * ODP errno (error number) is a thread local variable that any ODP function may
 * set on a failure. It expresses additional information about the cause of
 * the latest failure. A successful function call never sets errno. Application
 * may initialize errno to zero at anytime by calling odp_errno_zero(). Other
 * ODP functions never set errno to zero. Valid errno values are non-zero
 * and implementation specific. It's also implementation specific which
 * functions set errno in addition to those explicitly specified by
 * the API spec. ODP errno is initially zero.
 *
 *  @{
 */

/**
* Latest ODP errno
*
* Returns the current ODP errno value on the calling thread. A non-zero value
* indicates cause of the latest errno setting failure.
*
* @return Latest ODP errno value
* @retval 0    Errno has not been set since the last initialization to zero
*/
int odp_errno(void);

/**
* Set ODP errno to zero
*
* Sets errno value to zero on the calling thread.
*/
void odp_errno_zero(void);

/**
* Print ODP errno
*
* Interprets the value of ODP errno as an error message, and prints it,
* optionally preceding it with the custom message specified in str.
*
* @param str   Pointer to the string to be appended, or NULL
*/
void odp_errno_print(const char *str);

/**
* Error message string
*
* Interprets the value of ODP errno, generating a string with a message that
* describes the error. Errno values and messages are implementation specific.
*
* @param errnum	 ODP errno value
*
* @retval Pointer to the error message string
*/
const char *odp_errno_str(int errnum);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
