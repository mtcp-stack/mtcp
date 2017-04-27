/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP Linux helper API
 *
 * This file is an optional helper to odp.h APIs. These functions are provided
 * to ease common setups in a Linux system. User is free to implement the same
 * setups in otherways (not via this API).
 */

#ifndef ODP_LINUX_H_
#define ODP_LINUX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp.h>

#include <pthread.h>
#include <sys/types.h>

/** The thread starting arguments */
typedef struct {
	void *(*start_routine)(void *); /**< The function to run */
	void *arg; /**< The functions arguemnts */
	odp_thread_type_t thr_type; /**< The thread type */
} odp_start_args_t;

/** Linux pthread state information */
typedef struct {
	pthread_t      thread; /**< Pthread ID */
	pthread_attr_t attr;   /**< Pthread attributes */
	int            cpu;    /**< CPU ID */
	/** Saved starting args for join to later free */
	odp_start_args_t *start_args;
} odph_linux_pthread_t;


/** Linux process state information */
typedef struct {
	pid_t pid;      /**< Process ID */
	int   cpu;      /**< CPU ID */
	int   status;   /**< Process state change status */
} odph_linux_process_t;

/**
 * Creates and launches pthreads
 *
 * Creates, pins and launches threads to separate CPU's based on the cpumask.
 *
 * @param thread_tbl    Thread table
 * @param mask          CPU mask
 * @param start_routine Thread start function
 * @param arg           Thread argument
 * @param thr_type      Thread type
 *
 * @return Number of threads created
 */
int odph_linux_pthread_create(odph_linux_pthread_t *thread_tbl,
			       const odp_cpumask_t *mask,
			       void *(*start_routine)(void *), void *arg,
			       odp_thread_type_t thr_type);

/**
 * Waits pthreads to exit
 *
 * Returns when all threads have been exit.
 *
 * @param thread_tbl    Thread table
 * @param num           Number of threads to create
 *
 */
void odph_linux_pthread_join(odph_linux_pthread_t *thread_tbl, int num);


/**
 * Fork a process
 *
 * Forks and sets CPU affinity for the child process
 *
 * @param proc          Pointer to process state info (for output)
 * @param cpu           Destination CPU for the child process
 *
 * @return On success: 1 for the parent, 0 for the child
 *         On failure: -1 for the parent, -2 for the child
 */
int odph_linux_process_fork(odph_linux_process_t *proc, int cpu);


/**
 * Fork a number of processes
 *
 * Forks and sets CPU affinity for child processes
 *
 * @param proc_tbl      Process state info table (for output)
 * @param mask          CPU mask of processes to create
 *
 * @return On success: 1 for the parent, 0 for the child
 *         On failure: -1 for the parent, -2 for the child
 */
int odph_linux_process_fork_n(odph_linux_process_t *proc_tbl,
			      const odp_cpumask_t *mask);


/**
 * Wait for a number of processes
 *
 * Waits for a number of child processes to terminate. Records process state
 * change status into the process state info structure.
 *
 * @param proc_tbl      Process state info table (previously filled by fork)
 * @param num           Number of processes to wait
 *
 * @return 0 on success, -1 on failure
 */
int odph_linux_process_wait_n(odph_linux_process_t *proc_tbl, int num);


#ifdef __cplusplus
}
#endif

#endif
