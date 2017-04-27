/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <string.h>
#include <sys/time.h>
#include <test_debug.h>

#include <odp.h>
#include <odp/helper/linux.h>

static void test_atomic_inc_dec_u32(void);
static void test_atomic_add_sub_u32(void);
static void test_atomic_inc_dec_64(void);
static void test_atomic_add_sub_64(void);
static void test_atomic_inc_u32(void);
static void test_atomic_dec_u32(void);
static void test_atomic_add_u32(void);
static void test_atomic_sub_u32(void);
static void test_atomic_inc_64(void);
static void test_atomic_dec_64(void);
static void test_atomic_add_64(void);
static void test_atomic_sub_64(void);
static void test_atomic_init(void);
static void test_atomic_basic(void);
static void test_atomic_store(void);
static int test_atomic_validate(void);
static int odp_test_global_init(void);
static void odp_print_system_info(void);

/**
 * Thread argument
 */
typedef struct {
	int testcase; /**< specifies which set of API's to exercise */
	int numthrds; /**< no of pthreads to create */
} pthrd_arg;

static int odp_test_thread_create(void *(*start_routine) (void *), pthrd_arg *);
static int odp_test_thread_exit(pthrd_arg *);

#define MAX_WORKERS           32            /**< Max worker threads */
/**
 * add_sub_cnt could be any valid value
 * so to exercise explicit atomic_add/sub
 * ops. For now using 5..
 */
#define ADD_SUB_CNT	5
#define	CNT 500000
#define	U32_INIT_VAL	(1UL << 10)
#define	U64_INIT_VAL	(1ULL << 33)

typedef enum {
	TEST_MIX = 1, /* Must be first test case num */
	TEST_INC_DEC_U32,
	TEST_ADD_SUB_U32,
	TEST_INC_DEC_64,
	TEST_ADD_SUB_64,
	TEST_MAX,
} odp_test_atomic_t;

static odp_atomic_u32_t a32u;
static odp_atomic_u64_t a64u;
static odp_barrier_t barrier;
static odph_linux_pthread_t thread_tbl[MAX_WORKERS]; /**< worker threads table*/
static int num_workers; /**< number of workers >----*/



static const char * const test_name[] = {
	"dummy",
	"test atomic all (add/sub/inc/dec) on 32- and 64-bit atomic ints",
	"test atomic inc/dec of 32-bit atomic int",
	"test atomic add/sub of 32-bit atomic int",
	"test atomic inc/dec of 64-bit atomic int",
	"test atomic add/sub of 64-bit atomic int"
};

static struct timeval tv0[MAX_WORKERS], tv1[MAX_WORKERS];

static void usage(void)
{
	printf("\n./odp_atomic -t <testcase> [-n <numthreads>]\n\n"
	       "\t<testcase> is\n"
	       "\t\t1 - Test all (inc/dec/add/sub on 32/64-bit atomic ints)\n"
	       "\t\t2 - Test inc/dec of 32-bit atomic int\n"
	       "\t\t3 - Test add/sub of 32-bit atomic int\n"
	       "\t\t4 - Test inc/dec of 64-bit atomic int\n"
	       "\t\t5 - Test add/sub of 64-bit atomic int\n"
	       "\t\t-n <1 - 31> - no of threads to start\n"
	       "\t\tif user doesn't specify this option, then\n"
	       "\t\tno of threads created is equivalent to no of CPU's\n"
	       "\t\tavailable in the system\n"
	       "\tExample usage:\n"
	       "\t\t./odp_atomic -t 2\n"
	       "\t\t./odp_atomic -t 3 -n 12\n");
}


void test_atomic_inc_u32(void)
{
	int i;

	for (i = 0; i < CNT; i++)
		odp_atomic_inc_u32(&a32u);
}

void test_atomic_inc_64(void)
{
	int i;

	for (i = 0; i < CNT; i++)
		odp_atomic_inc_u64(&a64u);
}

void test_atomic_dec_u32(void)
{
	int i;

	for (i = 0; i < CNT; i++)
		odp_atomic_dec_u32(&a32u);
}

void test_atomic_dec_64(void)
{
	int i;

	for (i = 0; i < CNT; i++)
		odp_atomic_dec_u64(&a64u);
}

void test_atomic_add_u32(void)
{
	int i;

	for (i = 0; i < (CNT / ADD_SUB_CNT); i++)
		odp_atomic_fetch_add_u32(&a32u, ADD_SUB_CNT);
}

void test_atomic_add_64(void)
{
	int i;

	for (i = 0; i < (CNT / ADD_SUB_CNT); i++)
		odp_atomic_fetch_add_u64(&a64u, ADD_SUB_CNT);
}

void test_atomic_sub_u32(void)
{
	int i;

	for (i = 0; i < (CNT / ADD_SUB_CNT); i++)
		odp_atomic_fetch_sub_u32(&a32u, ADD_SUB_CNT);
}

void test_atomic_sub_64(void)
{
	int i;

	for (i = 0; i < (CNT / ADD_SUB_CNT); i++)
		odp_atomic_fetch_sub_u64(&a64u, ADD_SUB_CNT);
}

void test_atomic_inc_dec_u32(void)
{
	test_atomic_inc_u32();
	test_atomic_dec_u32();
}

void test_atomic_add_sub_u32(void)
{
	test_atomic_add_u32();
	test_atomic_sub_u32();
}

void test_atomic_inc_dec_64(void)
{
	test_atomic_inc_64();
	test_atomic_dec_64();
}

void test_atomic_add_sub_64(void)
{
	test_atomic_add_64();
	test_atomic_sub_64();
}

/**
 * Test basic atomic operation like
 * add/sub/increment/decrement operation.
 */
void test_atomic_basic(void)
{
	test_atomic_inc_u32();
	test_atomic_dec_u32();
	test_atomic_add_u32();
	test_atomic_sub_u32();

	test_atomic_inc_64();
	test_atomic_dec_64();
	test_atomic_add_64();
	test_atomic_sub_64();
}

void test_atomic_init(void)
{
	odp_atomic_init_u32(&a32u, 0);
	odp_atomic_init_u64(&a64u, 0);
}

void test_atomic_store(void)
{
	odp_atomic_store_u32(&a32u, U32_INIT_VAL);
	odp_atomic_store_u64(&a64u, U64_INIT_VAL);
}

int test_atomic_validate(void)
{
	if (odp_atomic_load_u32(&a32u) != U32_INIT_VAL) {
		LOG_ERR("Atomic u32 usual functions failed\n");
		return -1;
	}

	if (odp_atomic_load_u64(&a64u) != U64_INIT_VAL) {
		LOG_ERR("Atomic u64 usual functions failed\n");
		return -1;
	}

	return 0;
}

static void *run_thread(void *arg)
{
	pthrd_arg *parg = (pthrd_arg *)arg;
	int thr;

	thr = odp_thread_id();

	LOG_DBG("Thread %i starts\n", thr);

	/* Wait here until all threads have arrived */
	/* Use multiple barriers to verify that it handles wrap around and
	 * has no race conditions which could be exposed when invoked back-
	 * to-back */
	odp_barrier_wait(&barrier);
	odp_barrier_wait(&barrier);
	odp_barrier_wait(&barrier);
	odp_barrier_wait(&barrier);

	gettimeofday(&tv0[thr], NULL);

	switch (parg->testcase) {
	case TEST_MIX:
		test_atomic_basic();
		break;
	case TEST_INC_DEC_U32:
		test_atomic_inc_dec_u32();
		break;
	case TEST_ADD_SUB_U32:
		test_atomic_add_sub_u32();
		break;
	case TEST_INC_DEC_64:
		test_atomic_inc_dec_64();
		break;
	case TEST_ADD_SUB_64:
		test_atomic_add_sub_64();
		break;
	}
	gettimeofday(&tv1[thr], NULL);
	fflush(NULL);

	printf("Time taken in thread %02d to complete op is %lld usec\n", thr,
	       (tv1[thr].tv_sec - tv0[thr].tv_sec) * 1000000ULL +
	       (tv1[thr].tv_usec - tv0[thr].tv_usec));

	return parg;
}

/** create test thread */
int odp_test_thread_create(void *func_ptr(void *), pthrd_arg *arg)
{
	odp_cpumask_t cpumask;

	/* Create and init additional threads */
	odp_cpumask_default_worker(&cpumask, arg->numthrds);
	odph_linux_pthread_create(thread_tbl, &cpumask, func_ptr,
				  (void *)arg, ODP_THREAD_WORKER);

	return 0;
}

/** exit from test thread */
int odp_test_thread_exit(pthrd_arg *arg)
{
	/* Wait for other threads to exit */
	odph_linux_pthread_join(thread_tbl, arg->numthrds);

	return 0;
}

/** test init globals and call odp_init_global() */
int odp_test_global_init(void)
{
	memset(thread_tbl, 0, sizeof(thread_tbl));

	if (odp_init_global(NULL, NULL)) {
		LOG_ERR("ODP global init failed.\n");
		return -1;
	}

	num_workers = odp_cpu_count();
	/* force to max CPU count */
	if (num_workers > MAX_WORKERS)
		num_workers = MAX_WORKERS;

	return 0;
}

/**
 * Print system information
 */
void odp_print_system_info(void)
{
	odp_cpumask_t cpumask;
	char str[ODP_CPUMASK_STR_SIZE];

	memset(str, 1, sizeof(str));

	odp_cpumask_zero(&cpumask);

	odp_cpumask_from_str(&cpumask, "0x1");
	(void)odp_cpumask_to_str(&cpumask, str, sizeof(str));

	printf("\n");
	printf("ODP system info\n");
	printf("---------------\n");
	printf("ODP API version: %s\n",        odp_version_api_str());
	printf("CPU model:       %s\n",        odp_cpu_model_str());
	printf("CPU freq (hz):   %"PRIu64"\n", odp_cpu_hz_max());
	printf("Cache line size: %i\n",        odp_sys_cache_line_size());
	printf("CPU count:       %i\n",        odp_cpu_count());
	printf("CPU mask:        %s\n",        str);

	printf("\n");
}


int main(int argc, char *argv[])
{
	pthrd_arg thrdarg;
	int test_type = 1, pthrdnum = 0, i = 0, cnt = argc - 1;
	char c;
	int result;

	if (argc == 0 || argc % 2 == 0) {
		usage();
		goto err_exit;
	}

	if (odp_test_global_init() != 0)
		goto err_exit;
	odp_print_system_info();

	while (cnt != 0) {
		sscanf(argv[++i], "-%c", &c);
		switch (c) {
		case 't':
			sscanf(argv[++i], "%d", &test_type);
			break;
		case 'n':
			sscanf(argv[++i], "%d", &pthrdnum);
			break;
		default:
			LOG_ERR("Invalid option %c\n", c);
			usage();
			goto err_exit;
		}
		if (test_type < TEST_MIX || test_type > TEST_MAX ||
		    pthrdnum > odp_cpu_count() || pthrdnum < 0) {
			usage();
			goto err_exit;
		}
		cnt -= 2;
	}

	if (pthrdnum == 0)
		pthrdnum = odp_cpu_count();

	test_atomic_init();
	test_atomic_store();

	memset(&thrdarg, 0, sizeof(pthrd_arg));
	thrdarg.testcase = test_type;
	thrdarg.numthrds = pthrdnum;

	if ((test_type > 0) && (test_type < TEST_MAX)) {
		printf("%s\n", test_name[test_type]);
	} else {
		LOG_ERR("Invalid test case [%d]\n", test_type);
		usage();
		goto err_exit;
	}
	odp_barrier_init(&barrier, pthrdnum);
	odp_test_thread_create(run_thread, &thrdarg);

	odp_test_thread_exit(&thrdarg);

	result = test_atomic_validate();

	if (result == 0) {
		printf("%s_%d_%d Result:pass\n",
		       test_name[test_type], test_type, pthrdnum);
	} else {
		printf("%s_%d_%d Result:fail\n",
		       test_name[test_type], test_type, pthrdnum);
	}
	return 0;

err_exit:
	return -1;
}


