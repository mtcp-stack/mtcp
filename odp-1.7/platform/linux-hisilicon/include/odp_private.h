/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Huawei Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Huawei Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ODP_PRIVATE_H_
#define _ODP_PRIVATE_H_

#include <stdio.h>

/**
 * Initialize the mm_district subsystem (private to odp).
 *
 * @return
 *   - 0 on success
 *   - Negative on error
 */
int odp_mm_district_init(void);

/**
 * Common log initialization function (private to odp).
 *
 * Called by environment-specific log initialization function to initialize
 * log history.
 *
 * @param default_log
 *   The default log stream to be used.
 * @return
 *   - 0 on success
 *   - Negative on error
 */
int odp_common_log_init(FILE *default_log);

/**
 * Fill configuration with number of physical and logical processors
 *
 * This function is private to HODP.
 *
 * Parse /proc/cpuinfo to get the number of physical and logical
 * processors on the machine.
 *
 * @return
 *   0 on success, negative on error
 */
int odp_cpu_init(void);

/**
 * Map memory
 *
 * This function is private to HODP.
 *
 * Fill configuration structure with these infos, and return 0 on success.
 *
 * @return
 *   0 on success, negative on error
 */
int odp_memory_init(void);

/**
 * Configure timers
 *
 * This function is private to HODP.
 *
 * Mmap memory areas used by HPET (high precision event timer) that will
 * provide our time reference, and configure the TSC frequency also for it
 * to be used as a reference.
 *
 * @return
 *   0 on success, negative on error
 */
int odp_hisi_timer_init(void);

/**
 * Init early logs
 *
 * This function is private to HODP.
 *
 * @return
 *   0 on success, negative on error
 */
int odp_log_early_init(void);

/**
 * Init the default log stream
 *
 * This function is private to HODP.
 *
 * @return
 *   0 on success, negative on error
 */
int odp_log_init(const char *id, int facility);

/**
 * Init the default log stream
 *
 * This function is private to ODP.
 *
 * @return
 *   0 on success, negative on error
 */
int odp_pci_info_init(void);

struct odp_pci_driver;
struct odp_pci_device;

/**
 * Mmap memory for single PCI device
 *
 * This function is private to HODP.
 *
 * @return
 *   0 on success, negative on error
 */
int odp_pci_probe_one_driver(struct odp_pci_driver *dr,
			     struct odp_pci_device *dev);

/**
 * Munmap memory for single PCI device
 *
 * This function is private to HODP.
 *
 * @param	dr
 *  The pointer to the pci driver structure
 * @param	dev
 *  The pointer to the pci device structure
 * @return
 *   0 on success, negative on error
 */
int odp_pci_close_one_driver(struct odp_pci_driver *dr,
			     struct odp_pci_device *dev);

/**
 * Init tail queues for non-HODP library structures. This is to allow
 * the rings, mempools, etc. lists to be shared among multiple processes
 *
 * This function is private to HODP
 *
 * @return
 *    0 on success, negative on error
 */
int odp_tailqs_init(void);

/**
 * Init interrupt handling.
 *
 * This function is private to HODP.
 *
 * @return
 *  0 on success, negative on error
 */
int odp_intr_init(void);

/**
 * Init alarm mechanism. This is to allow a callback be called after
 * specific time.
 *
 * This function is private to HODP.
 *
 * @return
 *  0 on success, negative on error
 */
int odp_alarm_init(void);

/**
 * This function initialises any virtual devices
 *
 * This function is private to the HODP.
 */
int odp_dev_init(void);

/**
 * Function is to check if the kernel module(like, vfio, vfio_iommu_type1,
 * etc.) loaded.
 *
 * @param module_name
 *	The module's name which need to be checked
 *
 * @return
 *	-1 means some error happens(NULL pointer or open failure)
 *	0  means the module not loaded
 *	1  means the module loaded
 */
int odp_check_module(const char *module_name);
#endif /* _ODP_PRIVATE_H_ */
