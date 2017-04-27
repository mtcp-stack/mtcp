/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:	BSD-3-Clause
 */

#include <odp.h>
#include <odp_cunit_common.h>
#include "odp_classification_testsuites.h"
#include "classification.h"

odp_suiteinfo_t classification_suites[] = {
	{ .pName = "classification basic",
			.pTests = classification_suite_basic,
	},
	{ .pName = "classification pmr tests",
			.pTests = classification_suite_pmr,
			.pInitFunc = classification_suite_pmr_init,
			.pCleanupFunc = classification_suite_pmr_term,
	},
	{ .pName = "classification tests",
			.pTests = classification_suite,
			.pInitFunc = classification_suite_init,
			.pCleanupFunc = classification_suite_term,
	},
	ODP_SUITE_INFO_NULL,
};

int classification_main(void)
{
	int ret = odp_cunit_register(classification_suites);

	if (ret == 0)
		ret = odp_cunit_run();

	return ret;
}
