/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_internal.h>
#include <odp/debug.h>
#include <odp_debug_internal.h>
#include <odp/hints.h>

#include <stdarg.h>

ODP_WEAK_SYMBOL ODP_PRINTF_FORMAT(2, 3)
int odp_override_log(odp_log_level_t level, const char *fmt, ...)
{
	va_list args;
	int r;
	FILE *logfd;

	switch (level) {
	case ODP_LOG_ERR:
	case ODP_LOG_UNIMPLEMENTED:
	case ODP_LOG_ABORT:
		logfd = stderr;
		break;
	default:
		logfd = stdout;
	}

	va_start(args, fmt);
	r = vfprintf(logfd, fmt, args);
	va_end(args);

	return r;
}

ODP_WEAK_SYMBOL void odp_override_abort(void)
{
	abort();
}
