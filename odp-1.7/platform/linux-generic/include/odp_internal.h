/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP HW system information
 */

#ifndef ODP_INTERNAL_H_
#define ODP_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/init.h>
#include <odp/thread.h>
#include <stdio.h>

extern __thread int __odp_errno;

#define MAX_CPU_NUMBER 128

typedef struct {
	uint64_t cpu_hz_max[MAX_CPU_NUMBER];
	uint64_t huge_page_size;
	uint64_t page_size;
	int      cache_line_size;
	int      cpu_count;
	char     cpu_arch_str[128];
	char     model_str[MAX_CPU_NUMBER][128];
} odp_system_info_t;

struct odp_global_data_s {
	odp_log_func_t log_fn;
	odp_abort_func_t abort_fn;
	odp_system_info_t system_info;
};

enum init_stage {
	NO_INIT = 0,    /* No init stages completed */
	TIME_INIT = 1,
	SYSINFO_INIT = 2,
	HISI_INIT = 3,
	SHM_INIT = 4,
	THREAD_INIT = 5,
	POOL_INIT = 6,
	QUEUE_INIT = 7,
	SCHED_INIT = 8,
	PKTIO_INIT = 9,
	TIMER_INIT = 10,
	CRYPTO_INIT = 11,
	CLASSIFICATION_INIT = 12,
	UIO_INIT = 13,
	ALL_INIT = 14  /* All init stages completed */
};

extern struct odp_global_data_s odp_global_data;

int _odp_term_global(enum init_stage stage);
int _odp_term_local(enum init_stage stage);

int odp_system_info_init(void);
int odp_system_info_term(void);

int odp_thread_init_global(void);
int odp_thread_init_local(odp_thread_type_t type);
int odp_thread_term_local(void);
int odp_thread_term_global(void);

int odp_shm_init_global(void);
int odp_shm_term_global(void);
int odp_shm_init_local(void);

int odp_pool_init_global(void);
int odp_pool_init_local(void);
int odp_pool_term_global(void);
int odp_pool_term_local(void);

int odp_pktio_init_global(void);
int odp_pktio_term_global(void);
int odp_pktio_init_local(void);

int odp_classification_init_global(void);
int odp_classification_term_global(void);

int odp_queue_init_global(void);
int odp_queue_term_global(void);

int odp_crypto_init_global(void);
int odp_crypto_term_global(void);

int odp_schedule_init_global(void);
int odp_schedule_term_global(void);
int odp_schedule_init_local(void);
int odp_schedule_term_local(void);

int odp_timer_init_global(void);
int odp_timer_term_global(void);
int odp_timer_disarm_all(void);

int odp_time_init_global(void);
int odp_time_term_global(void);
int odp_uio_init_global(void);
void _odp_flush_caches(void);

int odp_cpuinfo_parser(FILE *file, odp_system_info_t *sysinfo);
uint64_t odp_cpu_hz_current(int id);
int odp_cpu_detected(unsigned core_id);
void *odp_get_global_data(void);
int odp_uio_term_global(void);

#ifdef __cplusplus
}
#endif

#endif
