/* Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef ODP_POSIX_EXTENSIONS_H
#define ODP_POSIX_EXTENSIONS_H

/*
 * This should be the only file to define POSIX extension levels. When
 * extensions are needed it should be included first in each C source file.
 * Header files should not include it.
 */

/*
 * Enable POSIX and GNU extensions
 *
 * This macro defines:
 *   o  _BSD_SOURCE, _SVID_SOURCE, _ATFILE_SOURCE, _LARGE‐FILE64_SOURCE,
 *      _ISOC99_SOURCE, _XOPEN_SOURCE_EXTENDED, _POSIX_SOURCE
 *   o  _POSIX_C_SOURCE with the value:
 *        * 200809L since  glibc v2.10 (== POSIX.1-2008 base specification)
 *        * 200112L before glibc v2.10 (== POSIX.1-2001 base specification)
 *        * 199506L before glibc v2.5
 *        * 199309L before glibc v2.1
 *   o  _XOPEN_SOURCE with the value:
 *        * 700 since  glibc v2.10
 *        * 600 before glibc v2.10
 *        * 500 before glibc v2.2
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#endif
