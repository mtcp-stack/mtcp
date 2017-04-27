/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <string.h>
#include <odp.h>
#include <odp_cunit_common.h>
#include <odp/helper/linux.h>
/* Globals */
static odph_linux_pthread_t thread_tbl[MAX_WORKERS];

/*
 * global init/term functions which may be registered
 * defaults to functions performing odp init/term.
 */
static int tests_global_init(void);
static int tests_global_term(void);
static struct {
	int (*global_init_ptr)(void);
	int (*global_term_ptr)(void);
} global_init_term = {tests_global_init, tests_global_term};

static odp_suiteinfo_t *global_testsuites;

/** create test thread */
int odp_cunit_thread_create(void *func_ptr(void *), pthrd_arg *arg)
{
	odp_cpumask_t cpumask;

	/* Create and init additional threads */
	odp_cpumask_default_worker(&cpumask, arg->numthrds);

	return odph_linux_pthread_create(thread_tbl, &cpumask, func_ptr,
					 (void *)arg, ODP_THREAD_WORKER);
}

/** exit from test thread */
int odp_cunit_thread_exit(pthrd_arg *arg)
{
	/* Wait for other threads to exit */
	odph_linux_pthread_join(thread_tbl, arg->numthrds);

	return 0;
}

static int tests_global_init(void)
{
	if (0 != odp_init_global(NULL, NULL)) {
		fprintf(stderr, "error: odp_init_global() failed.\n");
		return -1;
	}
	if (0 != odp_init_local(ODP_THREAD_CONTROL)) {
		fprintf(stderr, "error: odp_init_local() failed.\n");
		return -1;
	}

	return 0;
}

static int tests_global_term(void)
{
	if (0 != odp_term_local()) {
		fprintf(stderr, "error: odp_term_local() failed.\n");
		return -1;
	}

	if (0 != odp_term_global()) {
		fprintf(stderr, "error: odp_term_global() failed.\n");
		return -1;
	}

	return 0;
}

/*
 * register tests_global_init and tests_global_term functions.
 * If some of these functions are not registered, the defaults functions
 * (tests_global_init() and tests_global_term()) defined above are used.
 * One should use these register functions when defining these hooks.
 * Note that passing NULL as function pointer is valid and will simply
 * prevent the default (odp init/term) to be done.
 */
void odp_cunit_register_global_init(int (*func_init_ptr)(void))
{
	global_init_term.global_init_ptr = func_init_ptr;
}

void odp_cunit_register_global_term(int (*func_term_ptr)(void))
{
	global_init_term.global_term_ptr = func_term_ptr;
}

static odp_suiteinfo_t *cunit_get_suite_info(const char *suite_name)
{
	odp_suiteinfo_t *sinfo;

	for (sinfo = global_testsuites; sinfo->pName; sinfo++)
		if (strcmp(sinfo->pName, suite_name) == 0)
			return sinfo;

	return NULL;
}

static odp_testinfo_t *cunit_get_test_info(odp_suiteinfo_t *sinfo,
					   const char *test_name)
{
	odp_testinfo_t *tinfo;

	for (tinfo = sinfo->pTests; tinfo->pName; tinfo++)
		if (strcmp(tinfo->pName, test_name) == 0)
				return tinfo;

	return NULL;
}

/* A wrapper for the suite's init function. This is done to allow for a
 * potential runtime check to determine whether each test in the suite
 * is active (enabled by using ODP_TEST_INFO_CONDITIONAL()). If present,
 * the conditional check is run after the suite's init function.
 */
static int _cunit_suite_init(void)
{
	int ret = 0;
	CU_pSuite cur_suite = CU_get_current_suite();
	odp_suiteinfo_t *sinfo;
	odp_testinfo_t *tinfo;

	/* find the suite currently being run */
	cur_suite = CU_get_current_suite();
	if (!cur_suite)
		return -1;

	sinfo = cunit_get_suite_info(cur_suite->pName);
	if (!sinfo)
		return -1;

	/* execute its init function */
	if (sinfo->pInitFunc) {
		ret = sinfo->pInitFunc();
		if (ret)
			return ret;
	}

	/* run any configured conditional checks and mark inactive tests */
	for (tinfo = sinfo->pTests; tinfo->pName; tinfo++) {
		CU_pTest ptest;
		CU_ErrorCode err;

		if (!tinfo->check_active || tinfo->check_active())
			continue;

		/* test is inactive, mark it as such */
		ptest = CU_get_test_by_name(tinfo->pName, cur_suite);
		if (ptest)
			err = CU_set_test_active(ptest, CU_FALSE);
		else
			err = CUE_NOTEST;

		if (err != CUE_SUCCESS) {
			fprintf(stderr, "%s: failed to set test %s inactive\n",
				__func__, tinfo->pName);
			return -1;
		}
	}

	return ret;
}

/*
 * Register suites and tests with CUnit.
 *
 * Similar to CU_register_suites() but using locally defined wrapper
 * types.
 */
static int cunit_register_suites(odp_suiteinfo_t testsuites[])
{
	odp_suiteinfo_t *sinfo;
	odp_testinfo_t *tinfo;
	CU_pSuite suite;
	CU_pTest test;

	for (sinfo = testsuites; sinfo->pName; sinfo++) {
		suite = CU_add_suite(sinfo->pName,
				     _cunit_suite_init, sinfo->pCleanupFunc);
		if (!suite)
			return CU_get_error();

		for (tinfo = sinfo->pTests; tinfo->pName; tinfo++) {
			test = CU_add_test(suite, tinfo->pName,
					   tinfo->pTestFunc);
			if (!test)
				return CU_get_error();
		}
	}

	return 0;
}

static int cunit_update_test(CU_pSuite suite,
			     odp_suiteinfo_t *sinfo,
			     odp_testinfo_t *updated_tinfo)
{
	CU_pTest test = NULL;
	CU_ErrorCode err;
	odp_testinfo_t *tinfo;
	const char *test_name = updated_tinfo->pName;

	tinfo = cunit_get_test_info(sinfo, test_name);
	if (tinfo)
		test = CU_get_test(suite, test_name);

	if (!tinfo || !test) {
		fprintf(stderr, "%s: unable to find existing test named %s\n",
			__func__, test_name);
		return -1;
	}

	err = CU_set_test_func(test, updated_tinfo->pTestFunc);
	if (err != CUE_SUCCESS) {
		fprintf(stderr, "%s: failed to update test func for %s\n",
			__func__, test_name);
		return -1;
	}

	tinfo->check_active = updated_tinfo->check_active;

	return 0;
}

static int cunit_update_suite(odp_suiteinfo_t *updated_sinfo)
{
	CU_pSuite suite = NULL;
	CU_ErrorCode err;
	odp_suiteinfo_t *sinfo;
	odp_testinfo_t *tinfo;

	/* find previously registered suite with matching name */
	sinfo = cunit_get_suite_info(updated_sinfo->pName);

	if (sinfo) {
		/* lookup the associated CUnit suite */
		suite = CU_get_suite_by_name(updated_sinfo->pName,
					     CU_get_registry());
	}

	if (!sinfo || !suite) {
		fprintf(stderr, "%s: unable to find existing suite named %s\n",
			__func__, updated_sinfo->pName);
		return -1;
	}

	sinfo->pInitFunc = updated_sinfo->pInitFunc;
	sinfo->pCleanupFunc = updated_sinfo->pCleanupFunc;

	err = CU_set_suite_cleanupfunc(suite, updated_sinfo->pCleanupFunc);
	if (err != CUE_SUCCESS) {
		fprintf(stderr, "%s: failed to update cleanup func for %s\n",
			__func__, updated_sinfo->pName);
		return -1;
	}

	for (tinfo = updated_sinfo->pTests; tinfo->pName; tinfo++) {
		int ret;

		ret = cunit_update_test(suite, sinfo, tinfo);
		if (ret != 0)
			return ret;
	}

	return 0;
}

/*
 * Run tests previously registered via odp_cunit_register()
 */
int odp_cunit_run(void)
{
	int ret;

	printf("\tODP API version: %s\n", odp_version_api_str());
	printf("\tODP implementation version: %s\n", odp_version_impl_str());

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();

	ret = CU_get_number_of_failure_records();

	CU_cleanup_registry();

	/* call test executable terminason hook, if any */
	if (global_init_term.global_term_ptr &&
	    ((*global_init_term.global_term_ptr)() != 0))
		return -1;

	return (ret) ? -1 : 0;
}

/*
 * Update suites/tests previously registered via odp_cunit_register().
 *
 * Note that this is intended for modifying the properties of already
 * registered suites/tests. New suites/tests can only be registered via
 * odp_cunit_register().
 */
int odp_cunit_update(odp_suiteinfo_t testsuites[])
{
	int ret = 0;
	odp_suiteinfo_t *sinfo;

	for (sinfo = testsuites; sinfo->pName && ret == 0; sinfo++)
		ret = cunit_update_suite(sinfo);

	return ret;
}

/*
 * Register test suites to be run via odp_cunit_run()
 */
int odp_cunit_register(odp_suiteinfo_t testsuites[])
{
	/* call test executable init hook, if any */
	if (global_init_term.global_init_ptr &&
	    ((*global_init_term.global_init_ptr)() != 0))
		return -1;

	CU_set_error_action(CUEA_ABORT);

	CU_initialize_registry();
	global_testsuites = testsuites;
	cunit_register_suites(testsuites);
	CU_set_fail_on_inactive(CU_FALSE);

	return 0;
}
