/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <string.h>
#include <stdlib.h>

#include <example_debug.h>

/* ODP main header */
#include <odp.h>

/* ODP helper for Linux apps */
#include <odp/helper/linux.h>

/* GNU lib C */
#include <getopt.h>


#define MAX_WORKERS           32            /**< Max worker threads */
#define NUM_TMOS              10000         /**< Number of timers */
#define WAIT_NUM	      10    /**< Max tries to rx last tmo per worker */


/** Test arguments */
typedef struct {
	int cpu_count;     /**< CPU count*/
	int resolution_us; /**< Timeout resolution in usec*/
	int min_us;        /**< Minimum timeout in usec*/
	int max_us;        /**< Maximum timeout in usec*/
	int period_us;     /**< Timeout period in usec*/
	int tmo_count;     /**< Timeout count*/
} test_args_t;

/** @private Helper struct for timers */
struct test_timer {
	odp_timer_t tim;
	odp_event_t ev;
};

/** Test global variables */
typedef struct {
	test_args_t args;		/**< Test argunments*/
	odp_barrier_t test_barrier;	/**< Barrier for test synchronisation*/
	odp_pool_t pool;		/**< pool handle*/
	odp_timer_pool_t tp;		/**< Timer pool handle*/
	odp_atomic_u32_t remain;	/**< Number of timeouts to receive*/
	struct test_timer tt[256];	/**< Array of all timer helper structs*/
	uint32_t num_workers;		/**< Number of threads */
} test_globals_t;

/** @private Timer set status ASCII strings */
static const char *timerset2str(odp_timer_set_t val)
{
	switch (val) {
	case ODP_TIMER_SUCCESS:
		return "success";
	case ODP_TIMER_TOOEARLY:
		return "too early";
	case ODP_TIMER_TOOLATE:
		return "too late";
	case ODP_TIMER_NOEVENT:
		return "no event";
	default:
		return "?";
	}
};

/** @private test timeout */
static void remove_prescheduled_events(void)
{
	odp_event_t ev;
	odp_queue_t queue;

	odp_schedule_pause();
	while ((ev = odp_schedule(&queue, ODP_SCHED_NO_WAIT)) !=
			ODP_EVENT_INVALID) {
		odp_event_free(ev);
	}
}

/** @private test timeout */
static void test_abs_timeouts(int thr, test_globals_t *gbls)
{
	uint64_t period;
	uint64_t period_ns;
	odp_queue_t queue;
	uint64_t tick;
	struct test_timer *ttp;
	odp_timeout_t tmo;
	uint32_t num_workers = gbls->num_workers;

	EXAMPLE_DBG("  [%i] test_timeouts\n", thr);

	queue = odp_queue_lookup("timer_queue");

	period_ns = gbls->args.period_us * ODP_TIME_USEC_IN_NS;
	period    = odp_timer_ns_to_tick(gbls->tp, period_ns);

	EXAMPLE_DBG("  [%i] period %"PRIu64" ticks,  %"PRIu64" ns\n", thr,
		    period, period_ns);

	EXAMPLE_DBG("  [%i] current tick %"PRIu64"\n", thr,
		    odp_timer_current_tick(gbls->tp));

	ttp = &gbls->tt[thr];
	ttp->tim = odp_timer_alloc(gbls->tp, queue, ttp);
	if (ttp->tim == ODP_TIMER_INVALID) {
		EXAMPLE_ERR("Failed to allocate timer\n");
		return;
	}
	tmo = odp_timeout_alloc(gbls->pool);
	if (tmo == ODP_TIMEOUT_INVALID) {
		EXAMPLE_ERR("Failed to allocate timeout\n");
		return;
	}
	ttp->ev = odp_timeout_to_event(tmo);
	tick = odp_timer_current_tick(gbls->tp);

	while (1) {
		int wait = 0;
		odp_event_t ev;
		odp_timer_set_t rc;

		if (ttp) {
			tick += period;
			rc = odp_timer_set_abs(ttp->tim, tick, &ttp->ev);
			if (odp_unlikely(rc != ODP_TIMER_SUCCESS)) {
				/* Too early or too late timeout requested */
				EXAMPLE_ABORT("odp_timer_set_abs() failed: %s\n",
					      timerset2str(rc));
			}
		}

		/* Get the next expired timeout.
		 * We invoke the scheduler in a loop with a timeout because
		 * we are not guaranteed to receive any more timeouts. The
		 * scheduler isn't guaranteeing fairness when scheduling
		 * buffers to threads.
		 * Use 1.5 second timeout for scheduler */
		uint64_t sched_tmo =
			odp_schedule_wait_time(1500000000ULL);
		do {
			ev = odp_schedule(&queue, sched_tmo);
			/* Check if odp_schedule() timed out, possibly there
			 * are no remaining timeouts to receive */
			if (++wait > WAIT_NUM &&
			    odp_atomic_load_u32(&gbls->remain) < num_workers)
				EXAMPLE_ABORT("At least one TMO was lost\n");
		} while (ev == ODP_EVENT_INVALID &&
			 (int)odp_atomic_load_u32(&gbls->remain) > 0);

		if (ev == ODP_EVENT_INVALID)
			break; /* No more timeouts */
		if (odp_event_type(ev) != ODP_EVENT_TIMEOUT) {
			/* Not a default timeout event */
			EXAMPLE_ABORT("Unexpected event type (%u) received\n",
				      odp_event_type(ev));
		}
		odp_timeout_t tmo = odp_timeout_from_event(ev);

		tick = odp_timeout_tick(tmo);
		ttp = odp_timeout_user_ptr(tmo);
		ttp->ev = ev;
		if (!odp_timeout_fresh(tmo)) {
			/* Not the expected expiration tick, timer has
			 * been reset or cancelled or freed */
			EXAMPLE_ABORT("Unexpected timeout received (timer %d, tick %lld)\n",
				      ttp->tim, tick);
		}
		EXAMPLE_DBG("  [%i] timeout, tick %"PRIu64"\n", thr, tick);

		uint32_t rx_num = odp_atomic_fetch_dec_u32(&gbls->remain);

		if (!rx_num)
			EXAMPLE_ABORT("Unexpected timeout received (timer %d, tick %lld)\n",
				      ttp->tim, tick);
		else if (rx_num > num_workers)
			continue;

		odp_event_free(ttp->ev);
		odp_timer_free(ttp->tim);
		ttp = NULL;
	}

	/* Remove any prescheduled events */
	remove_prescheduled_events();
}


/**
 * @internal Worker thread
 *
 * @param ptr  Pointer to test arguments
 *
 * @return Pointer to exit status
 */
static void *run_thread(void *ptr)
{
	int thr;
	odp_pool_t msg_pool;
	test_globals_t *gbls;

	gbls = ptr;
	thr  = odp_thread_id();

	printf("Thread %i starts on cpu %i\n", thr, odp_cpu_id());

	/*
	 * Find the pool
	 */
	msg_pool = odp_pool_lookup("msg_pool");

	if (msg_pool == ODP_POOL_INVALID) {
		EXAMPLE_ERR("  [%i] msg_pool not found\n", thr);
		return NULL;
	}

	odp_barrier_wait(&gbls->test_barrier);

	test_abs_timeouts(thr, gbls);


	printf("Thread %i exits\n", thr);
	fflush(NULL);
	return NULL;
}


/**
 * @internal Print help
 */
static void print_usage(void)
{
	printf("\n\nUsage: ./odp_example [options]\n");
	printf("Options:\n");
	printf("  -c, --count <number>    CPU count\n");
	printf("  -r, --resolution <us>   timeout resolution in usec\n");
	printf("  -m, --min <us>          minimum timeout in usec\n");
	printf("  -x, --max <us>          maximum timeout in usec\n");
	printf("  -p, --period <us>       timeout period in usec\n");
	printf("  -t, --timeouts <count>  timeout repeat count\n");
	printf("  -h, --help              this help\n");
	printf("\n\n");
}


/**
 * @internal Parse arguments
 *
 * @param argc  Argument count
 * @param argv  Argument vector
 * @param args  Test arguments
 */
static void parse_args(int argc, char *argv[], test_args_t *args)
{
	int opt;
	int long_index;

	static struct option longopts[] = {
		{"count",      required_argument, NULL, 'c'},
		{"resolution", required_argument, NULL, 'r'},
		{"min",        required_argument, NULL, 'm'},
		{"max",        required_argument, NULL, 'x'},
		{"period",     required_argument, NULL, 'p'},
		{"timeouts",   required_argument, NULL, 't'},
		{"help",       no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	/* defaults */
	args->cpu_count     = 0; /* all CPU's */
	args->resolution_us = 10000;
	args->min_us        = 0;
	args->max_us        = 10000000;
	args->period_us     = 1000000;
	args->tmo_count     = 30;

	while (1) {
		opt = getopt_long(argc, argv, "+c:r:m:x:p:t:h",
				  longopts, &long_index);

		if (opt == -1)
			break;	/* No more options */

		switch (opt) {
		case 'c':
			args->cpu_count = atoi(optarg);
			break;
		case 'r':
			args->resolution_us = atoi(optarg);
			break;
		case 'm':
			args->min_us = atoi(optarg);
			break;
		case 'x':
			args->max_us = atoi(optarg);
			break;
		case 'p':
			args->period_us = atoi(optarg);
			break;
		case 't':
			args->tmo_count = atoi(optarg);
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
			break;

		default:
			break;
		}
	}

	if (args->period_us < args->resolution_us)
		printf("\n\tWarn: timeout is set less then resolution\n");
}


/**
 * Test main function
 */
int main(int argc, char *argv[])
{
	odph_linux_pthread_t thread_tbl[MAX_WORKERS];
	int num_workers;
	odp_queue_t queue;
	uint64_t tick, ns;
	odp_queue_param_t param;
	odp_pool_param_t params;
	odp_timer_pool_param_t tparams;
	odp_timer_pool_info_t tpinfo;
	odp_cpumask_t cpumask;
	char cpumaskstr[ODP_CPUMASK_STR_SIZE];
	odp_shm_t shm = ODP_SHM_INVALID;
	test_globals_t *gbls = NULL;
	int err = 0;

	printf("\nODP timer example starts\n");

	if (odp_init_global(NULL, NULL)) {
		err = 1;
		printf("ODP global init failed.\n");
		goto err_global;
	}

	/* Init this thread. */
	if (odp_init_local(ODP_THREAD_CONTROL)) {
		err = 1;
		printf("ODP local init failed.\n");
		goto err_local;
	}

	printf("\n");
	printf("ODP system info\n");
	printf("---------------\n");
	printf("ODP API version: %s\n",        odp_version_api_str());
	printf("CPU model:       %s\n",        odp_cpu_model_str());
	printf("CPU freq (hz):   %"PRIu64"\n", odp_cpu_hz_max());
	printf("Cache line size: %i\n",        odp_sys_cache_line_size());
	printf("Max CPU count:   %i\n",        odp_cpu_count());

	printf("\n");

	/* Reserve memory for test_globals_t from shared mem */
	shm = odp_shm_reserve("shm_test_globals", sizeof(test_globals_t),
			      ODP_CACHE_LINE_SIZE, 0);
	if (ODP_SHM_INVALID == shm) {
		err = 1;
		EXAMPLE_ERR("Error: shared mem reserve failed.\n");
		goto err;
	}

	gbls = odp_shm_addr(shm);
	if (NULL == gbls) {
		err = 1;
		EXAMPLE_ERR("Error: shared mem alloc failed.\n");
		goto err;
	}
	memset(gbls, 0, sizeof(test_globals_t));
	gbls->pool = ODP_POOL_INVALID;
	gbls->tp = ODP_TIMER_POOL_INVALID;

	parse_args(argc, argv, &gbls->args);

	memset(thread_tbl, 0, sizeof(thread_tbl));

	/* Default to system CPU count unless user specified */
	num_workers = MAX_WORKERS;
	if (gbls->args.cpu_count)
		num_workers = gbls->args.cpu_count;

	/* Get default worker cpumask */
	num_workers = odp_cpumask_default_worker(&cpumask, num_workers);
	(void)odp_cpumask_to_str(&cpumask, cpumaskstr, sizeof(cpumaskstr));

	printf("num worker threads: %i\n", num_workers);
	printf("first CPU:          %i\n", odp_cpumask_first(&cpumask));
	printf("cpu mask:           %s\n", cpumaskstr);

	printf("resolution:         %i usec\n", gbls->args.resolution_us);
	printf("min timeout:        %i usec\n", gbls->args.min_us);
	printf("max timeout:        %i usec\n", gbls->args.max_us);
	printf("period:             %i usec\n", gbls->args.period_us);
	printf("timeouts:           %i\n", gbls->args.tmo_count);

	/*
	 * Create pool for timeouts
	 */
	odp_pool_param_init(&params);
	params.tmo.num   = NUM_TMOS;
	params.type      = ODP_POOL_TIMEOUT;

	gbls->pool = odp_pool_create("msg_pool", &params);

	if (gbls->pool == ODP_POOL_INVALID) {
		err = 1;
		EXAMPLE_ERR("Pool create failed.\n");
		goto err;
	}

	tparams.res_ns = gbls->args.resolution_us * ODP_TIME_USEC_IN_NS;
	tparams.min_tmo = gbls->args.min_us * ODP_TIME_USEC_IN_NS;
	tparams.max_tmo = gbls->args.max_us * ODP_TIME_USEC_IN_NS;
	tparams.num_timers = num_workers; /* One timer per worker */
	tparams.priv = 0; /* Shared */
	tparams.clk_src = ODP_CLOCK_CPU;
	gbls->tp = odp_timer_pool_create("timer_pool", &tparams);
	if (gbls->tp == ODP_TIMER_POOL_INVALID) {
		err = 1;
		EXAMPLE_ERR("Timer pool create failed.\n");
		goto err;
	}
	odp_timer_pool_start();

	odp_shm_print_all();
	(void)odp_timer_pool_info(gbls->tp, &tpinfo);
	printf("Timer pool\n");
	printf("----------\n");
	printf("  name: %s\n", tpinfo.name);
	printf("  resolution: %"PRIu64" ns\n", tpinfo.param.res_ns);
	printf("  min tmo: %"PRIu64" ticks\n", tpinfo.param.min_tmo);
	printf("  max tmo: %"PRIu64" ticks\n", tpinfo.param.max_tmo);
	printf("\n");

	/*
	 * Create a queue for timer test
	 */
	odp_queue_param_init(&param);
	param.type        = ODP_QUEUE_TYPE_SCHED;
	param.sched.prio  = ODP_SCHED_PRIO_DEFAULT;
	param.sched.sync  = ODP_SCHED_SYNC_PARALLEL;
	param.sched.group = ODP_SCHED_GROUP_ALL;

	queue = odp_queue_create("timer_queue", &param);

	if (queue == ODP_QUEUE_INVALID) {
		err = 1;
		EXAMPLE_ERR("Timer queue create failed.\n");
		goto err;
	}

	printf("CPU freq %"PRIu64" Hz\n", odp_cpu_hz_max());
	printf("Timer ticks vs nanoseconds:\n");
	ns = 0;
	tick = odp_timer_ns_to_tick(gbls->tp, ns);

	printf("  %12" PRIu64 " ns      ->  %12" PRIu64 " ticks\n", ns, tick);
	printf("  %12" PRIu64 " ticks   ->  %12" PRIu64 " ns\n", tick,
	       odp_timer_tick_to_ns(gbls->tp, tick));

	for (ns = 1; ns <= 100 * ODP_TIME_SEC_IN_NS; ns *= 10) {
		tick = odp_timer_ns_to_tick(gbls->tp, ns);

		printf("  %12" PRIu64 " ns      ->  %12" PRIu64 " ticks\n", ns,
		       tick);
		printf("  %12" PRIu64 " ticks   ->  %12" PRIu64 " ns\n", tick,
		       odp_timer_tick_to_ns(gbls->tp, tick));
	}

	printf("\n");

	gbls->num_workers = num_workers;

	/* Initialize number of timeouts to receive */
	odp_atomic_init_u32(&gbls->remain, gbls->args.tmo_count * num_workers);

	/* Barrier to sync test case execution */
	odp_barrier_init(&gbls->test_barrier, num_workers);

	/* Create and launch worker threads */
	odph_linux_pthread_create(thread_tbl, &cpumask,
				  run_thread, gbls, ODP_THREAD_WORKER);

	/* Wait for worker threads to exit */
	odph_linux_pthread_join(thread_tbl, num_workers);

	/* free resources */
	if (odp_queue_destroy(queue))
		err = 1;

err:

	if (gbls != NULL && gbls->tp != ODP_TIMER_POOL_INVALID)
		odp_timer_pool_destroy(gbls->tp);

	if (gbls != NULL && gbls->pool != ODP_TIMER_POOL_INVALID)
		if (odp_pool_destroy(gbls->pool))
			err = 1;

	if (shm != ODP_SHM_INVALID)
		if (odp_shm_free(shm))
			err = 1;

	if (odp_term_local())
		err = 1;
err_local:
	if (odp_term_global())
		err = 1;
err_global:
	if (err) {
		printf("Err: ODP timer test failed\n\n");
		return -1;
	}

	printf("ODP timer test complete\n\n");
	return 0;
}
