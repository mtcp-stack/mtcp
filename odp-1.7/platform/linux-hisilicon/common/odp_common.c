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
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in
 *	 the documentation and/or other materials provided with the
 *	 distribution.
 *     * Neither the name of Huawei Corporation nor the names of its
 *	 contributors may be used to endorse or promote products derived
 *	 from this software without specific prior written permission.
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <getopt.h>
#include <sys/file.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/queue.h>

#include <odp/cpu.h>
#if defined(ODP_ARCH_X86_64) || defined(ODP_ARCH_I686)
#include <sys/io.h>
#endif

#include <odp_common.h>
#include <odp/config.h>
#include <odp_memory.h>
#include <odp_mmdistrict.h>
#include <odp_hugepage.h>
#include <odp_base.h>
#include <odp_syslayout.h>
#include <odp_core.h>
#include <odp_cycles.h>
#include <odp_pci.h>
#include <odp_devargs.h>
#include <odp/atomic.h>
#include "odp_private.h"
#include "odp_local_cfg.h"
#include "odp_filesystem.h"
#include "odp_options.h"
#include "odp_debug_internal.h"
#include "odp_uio_internal.h"

#define MEMSIZE_IF_NO_HUGE_PAGE (64ULL * 1024ULL * 1024ULL)

#define SOCKET_MEM_STRLEN (ODP_MAX_NUMA_NODES * 10)
#define LINE_LEN	  128
#define BITS_PER_HEX	  4

/* Allow the application to print its usage message too if set */
static odp_usage_hook_t odp_application_usage_hook;

TAILQ_HEAD(shared_driver_list, shared_driver);

/* Definition for shared object drivers. */
struct shared_driver {
	TAILQ_ENTRY(shared_driver) next;

	char  name[ODP_PATH_MAX];
	void *lib_handle;
	char  desc[ODP_LIB_DESC_LEN];
};

/* early configuration structure, when memory config is not mmapped */
static struct odp_sys_layout early_sys_layout;

/* define fd variable here, because file needs to be kept open for the
 * duration of the program, as we hold a write lock on it in the primary proc */
static int mem_cfg_fd = -1;

static struct flock wr_lock = {
	.l_type	  = F_WRLCK,
	.l_whence = SEEK_SET,
	.l_start  = offsetof(struct odp_sys_layout, mmfrag),
	.l_len	  = sizeof(early_sys_layout.mmfrag),
};

/* Address of global and public configuration */
static struct odp_config godp_config = {
	.sys_layout = &early_sys_layout,
};

/* internal configuration (per-core) */
struct core_config gcore_config[ODP_MAX_CORE];

/* internal configuration */
struct odp_local_config local_config;

/* Per core error number. */
ODP_DEFINE_PER_CORE(int, _odp_errno);

/* Return a pointer to the configuration structure */
struct odp_config *odp_get_configuration(void)
{
	return &godp_config;
}

int get_mplock_value_debug(void)
{
	return godp_config.sys_layout->mplock.cnt.v;
}

/* parse a sysfs (or other) file containing one integer value */
unsigned long odp_parse_sysfs_value(const char *filename)
{
	FILE *f;
	char  buf[ODP_BUFF_SIZE];
	char *end = NULL;
	unsigned long value = -1;

	f = fopen(filename, "r");
	if (!f) {
		/*ODP_ERR("cannot open sysfs value %s\n", filename);*/
		return -1;
	}

	if (!fgets(buf, sizeof(buf), f)) {
		ODP_ERR("cannot read sysfs value %s\n", filename);
		fclose(f);
		return -1;
	}

	value = strtoul(buf, &end, 0);
	if ((buf[0] == '\0') || (!end) || (*end != '\n')) {
		ODP_ERR("cannot parse sysfs value %s\n", filename);
		fclose(f);
		return -1;
	}

	fclose(f);
	return value;
}

/* create memory configuration in shared/mmap memory. Take out
 * a write lock on the mmfrags, so we can auto-detect primary/secondary.
 * This means we never close the file while running (auto-close on exit).
 * We also don't lock the whole file, so that in future we can use read-locks
 * on other parts, e.g. memzones, to detect if there are running secondary
 * processes. */
static void odp_config_create(void)
{
	void *odp_mem_cfg_addr;
	int   retval;

	const char *pathname = odp_runtime_config_path();

	if (local_config.no_shconf)
		return;

	/* map the config before hugepage address so that we don't
	 * waste a page */
	if (local_config.base_virtaddr != 0)
		odp_mem_cfg_addr =
			(void *)ODP_ALIGN_FLOOR(local_config.base_virtaddr -
						sizeof(struct odp_sys_layout),
						sysconf(_SC_PAGE_SIZE));
	else
		odp_mem_cfg_addr = NULL;

	if (mem_cfg_fd < 0) {
		mem_cfg_fd = open(pathname, O_RDWR | O_CREAT, 0660);
		if (mem_cfg_fd < 0)
			ODP_ERR("Cannot open '%s' for odp_sys_layout\n",
				pathname);
	}

	retval = ftruncate(mem_cfg_fd, sizeof(*godp_config.sys_layout));
	if (retval < 0) {
		close(mem_cfg_fd);
		ODP_ERR("Cannot resize '%s' for odp_sys_layout\n", pathname);
	}

	retval = fcntl(mem_cfg_fd, F_SETLK, &wr_lock);
	if (retval < 0) {
		close(mem_cfg_fd);
		ODP_ERR(
			"Cannot create lock on '%s'. Is another primary process running?\n",
			pathname);
	}

	odp_mem_cfg_addr = mmap(odp_mem_cfg_addr,
				sizeof(*godp_config.sys_layout),
				PROT_READ | PROT_WRITE, MAP_SHARED, mem_cfg_fd,
				0);

	if (odp_mem_cfg_addr == MAP_FAILED)
		ODP_ERR("Cannot mmap memory for godp_config\n");

	memcpy(odp_mem_cfg_addr, &early_sys_layout, sizeof(early_sys_layout));
	godp_config.sys_layout = (struct odp_sys_layout *)odp_mem_cfg_addr;

	/* store address of the config in the config itself so that secondary
	 * processes could later map the config into this exact location */
	godp_config.sys_layout->mem_cfg_addr = (uintptr_t)odp_mem_cfg_addr;
}

/* attach to an existing shared memory config */
static void odp_config_attach(void)
{
	struct odp_sys_layout *sys_layout;

	const char *pathname = odp_runtime_config_path();

	if (local_config.no_shconf)
		return;

	if (mem_cfg_fd < 0) {
		mem_cfg_fd = open(pathname, O_RDWR);
		if (mem_cfg_fd < 0)
			ODP_PRINT("Cannot open '%s' for odp_sys_layout\n",
				  pathname);
	}

	/* map it as read-only first */
	sys_layout = (struct odp_sys_layout *)mmap(NULL, sizeof(*sys_layout),
						   PROT_READ, MAP_SHARED,
						   mem_cfg_fd, 0);
	if (sys_layout == MAP_FAILED)
		ODP_ERR("Cannot mmap memory for godp_config\n");

	godp_config.sys_layout = sys_layout;
}

/* reattach the shared config at exact memory location primary process has it */
static void odp_config_reattach(void)
{
	struct odp_sys_layout *sys_layout;
	void *odp_mem_cfg_addr;

	if (local_config.no_shconf)
		return;

	/* save the address primary process has mapped shared config to */
	odp_mem_cfg_addr =
		(void *)(uintptr_t)godp_config.sys_layout->mem_cfg_addr;

	/* unmap original config */
	munmap(godp_config.sys_layout, sizeof(struct odp_sys_layout));

	/* remap the config at proper address */
	sys_layout = (struct odp_sys_layout *)mmap(odp_mem_cfg_addr,
						   sizeof(*sys_layout),
						   PROT_READ | PROT_WRITE,
						   MAP_SHARED,
						   mem_cfg_fd, 0);
	close(mem_cfg_fd);
	if ((sys_layout == MAP_FAILED) || (sys_layout != odp_mem_cfg_addr))
		ODP_ERR("Cannot mmap memory for godp_config\n");

	godp_config.sys_layout = sys_layout;
}

/* Detect if we are a primary or a secondary process */
enum odp_proc_type_t odp_proc_type_detect(void)
{
	enum odp_proc_type_t ptype = ODP_PROC_PRIMARY;
	const char *pathname = odp_runtime_config_path();

	/* if we can open the file but not get a write-lock we are a secondary
	 * process. NOTE: if we get a file handle back, we keep that open
	 * and don't close it to prevent a race condition between multiple
	 * opens */
	mem_cfg_fd = open(pathname, O_RDWR);
	if ((mem_cfg_fd >= 0) &&
	    (fcntl(mem_cfg_fd, F_SETLK, &wr_lock) < 0))
		ptype = ODP_PROC_SECONDARY;

	ODP_PRINT("Auto-detected process type: %s\n",
		  ptype == ODP_PROC_PRIMARY ? "PRIMARY" : "SECONDARY");

	return ptype;
}

/* Sets up godp_config structure with the pointer to shared memory config.*/
static void odp_global_config_init(void)
{
	godp_config.process_type = local_config.process_type;
	switch (godp_config.process_type) {
	case ODP_PROC_PRIMARY:
		odp_config_create();
		break;
	case ODP_PROC_SECONDARY:
		odp_config_attach();
		odp_mcfg_wait_complete(godp_config.sys_layout);
		odp_config_reattach();
		break;
	case ODP_PROC_AUTO:
	case ODP_PROC_INVALID:
		ODP_ERR("Invalid process type\n");
	}
}

/* Unlocks hugepage directories that were locked by odp_hugepage_info_init */
static void odp_hugedirs_unlock(void)
{
	int i;

	for (i = 0; i < MAX_HUGEPAGE_SIZES; i++) {
		/* skip uninitialized */
		if (local_config.odp_hugepage_type[i].lock_descriptor < 0)
			continue;

		/* unlock hugepage file */

		flock(local_config.odp_hugepage_type[i].lock_descriptor,
		      LOCK_UN);
		close(local_config.odp_hugepage_type[i].lock_descriptor);

		/* reset the field */
		local_config.odp_hugepage_type[i].lock_descriptor = -1;
	}
}

/* Set a per-application usage message */
odp_usage_hook_t odp_set_application_usage_hook(odp_usage_hook_t usage_func)
{
	odp_usage_hook_t old_func;

	/* Will be NULL on the first call to denote the last usage routine. */
	old_func = odp_application_usage_hook;
	odp_application_usage_hook = usage_func;

	return old_func;
}

static inline size_t odp_get_hugepage_mem_size(void)
{
	uint64_t size = 0;
	unsigned i, j;

	for (i = 0; i < local_config.num_hugepage_types; i++) {
		struct odp_hugepage_type *hpi =
			&local_config.odp_hugepage_type[i];

		if (hpi->hugedir)
			for (j = 0; j < ODP_MAX_NUMA_NODES; j++)
				size += hpi->hugepage_sz * hpi->num_pages[j];
	}

	return (size < MEM_SIZE_MAX) ? (size_t)(size) : MEM_SIZE_MAX;
}

static inline void odp_mcfg_complete(void)
{
	/* ALL shared sys_layout related INIT DONE */
	if (godp_config.process_type == ODP_PROC_PRIMARY)
		godp_config.sys_layout->magic = ODP_MAGIC;
}

/*
 * Request iopl privilege for all RPL, returns 0 on success
 * iopl() call is mostly for the i386 architecture. For other architectures,
 * return -1 to indicate IO privilege can't be changed in this way.
 */
int odp_iopl_init(void)
{
#if defined(ODP_ARCH_X86_64) || defined(ODP_ARCH_I686)
	if (iopl(3) != 0)
		return -1;

	return 0;
#else
	return -1;
#endif
}

static int xdigit2val(unsigned char c)
{
	int val;

	if (isdigit(c))
		val = c - '0';
	else if (isupper(c))
		val = c - 'A' + 10;
	else
		val = c - 'a' + 10;

	return val;
}

int odp_set_coremask(char *coremask)
{
	struct odp_config *cfg = odp_get_configuration();
	int i, j, idx = 0;
	unsigned count = 0;
	char c;
	int  val;

	if (!coremask)
		return -1;

	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*coremask))
		coremask++;

	if ((coremask[0] == '0') && ((coremask[1] == 'x') ||
				     (coremask[1] == 'X')))
		coremask += 2;

	i = strlen(coremask);
	while ((i > 0) && isblank(coremask[i - 1]))
		i--;

	if (i == 0)
		return -1;

	for (i = i - 1; i >= 0 && idx < ODP_MAX_CORE; i--) {
		c = coremask[i];
		if (isxdigit(c) == 0)
			/* invalid characters */
			return -1;

		val = xdigit2val(c);
		for (j = 0; j < BITS_PER_HEX && idx < ODP_MAX_CORE;
		     j++, idx++) {
			if ((1 << j) & val) {
				if (!gcore_config[idx].detected) {
					ODP_ERR("core %u unavailable\n", idx);
					return -1;
				}

				cfg->core_role[idx] = ROLE_ODP;
				gcore_config[idx].core_index = count;
				count++;
			} else {
				cfg->core_role[idx] = ROLE_OFF;
				gcore_config[idx].core_index = -1;
			}
		}
	}

	for (; i >= 0; i--)
		if (coremask[i] != '0')
			return -1;

	for (; idx < ODP_MAX_CORE; idx++) {
		cfg->core_role[idx] = ROLE_OFF;
		gcore_config[idx].core_index = -1;
	}

	if (count == 0)
		return -1;

	/* Update the count of enabled logical cores of
	 * the ODP configuration */
	cfg->core_num = count;

	return 0;
}

void odp_init_coremask(void)
{
	int ret = -1;
	int core_count, i, num_cores = 0;
	char core_mask[8];

	core_count = odp_cpu_count();
	for (i = 0; i < core_count; i++)
		num_cores += (0x1 << i);

	sprintf(core_mask, "%x", num_cores);
	ret = odp_set_coremask(core_mask);
	if (ret) {
		ODP_ERR("odp_set_coremask fail!!");
		return;
	}
}

void odp_init_local_config(struct odp_local_config *local_cfg)
{
	int i;

	if (local_cfg == NULL) {
		ODP_ERR("odp_init_local_config input param error!!");
		return;
	}

	local_cfg->memory = 0;
	local_cfg->force_rank_num = 0;
	local_cfg->force_channel_num = 3;
	local_cfg->force_sockets = 0;

	/* zero out the NUMA config */
	for (i = 0; i < ODP_MAX_NUMA_NODES; i++)
		local_cfg->socket_mem[i] = 0;

	/* zero out hugedir descriptors */
	for (i = 0; i < MAX_HUGEPAGE_SIZES; i++)
		local_cfg->odp_hugepage_type[i].lock_descriptor = -1;

	local_cfg->base_virtaddr = 0;

	local_cfg->syslog_facility = LOG_DAEMON;

	/* if set to NONE, interrupt mode is determined automatically */
	local_cfg->vfio_intr_mode = ODP_INTR_MODE_NONE;

	local_cfg->process_type = ODP_PROC_AUTO;
}

/*
 * Parse the coremask given as argument (hexadecimal string) and fill
 * the global configuration (core role and core count) with the parsed
 * value.
 */
int odp_adjust_local_config(struct odp_local_config *local_cfg)
{
	int i;

	if (local_config.process_type == ODP_PROC_AUTO)
		local_config.process_type = odp_proc_type_detect();

	/* if no memory amounts were requested, this will result in 0 and
	 * will be overridden later, right after odp_hugepage_info_init() */
	for (i = 0; i < ODP_MAX_NUMA_NODES; i++)
		local_cfg->memory += local_cfg->socket_mem[i];

	return 0;
}

/*****************************************************************************
   Function     : odp_pre_init
   Description  : odp mem and cpu info init
   Input        : in put
   Output       : None
   Return       : 0,succesed;other,failed
   Create By    : x00180405
   Modification :
   1.created: 2015/7/2
   Restriction  :
*****************************************************************************/
int odp_arch_pv660_init(int argc, char **argv)
{
	int ret;
	static int run_once;

	(void)argc;
	(void)argv;
	if (run_once) {
		ODP_ERR("odp_init has been called!!");
		return -1;
	}

	ret = odp_cpu_init();
	if (ret < 0) {
		ODP_ERR("odp_cpu_init fail ret = %d!!", ret);
		return -1;
	}

	odp_init_local_config(&local_config);

	odp_init_coremask();
	ret = odp_adjust_local_config(&local_config);
	if (ret != 0) {
		ODP_ERR("odp_adjust_config fail ret = %d!!", ret);
		return -1;
	}

	if ((local_config.no_hugetlbfs == 0) &&
	    (local_config.process_type != ODP_PROC_SECONDARY) &&
	    (odp_hugepage_info_init() < 0)) {
		ODP_ERR("Cannot get hugepage information\n");
		return -1;
	}

	if ((local_config.memory == 0) && (local_config.force_sockets == 0)) {
		if (local_config.no_hugetlbfs)
			local_config.memory = MEMSIZE_IF_NO_HUGE_PAGE;
		else
			local_config.memory = odp_get_hugepage_mem_size();
	}

	ODP_PRINT(
		"system memory=%lu (k), current system pagesize=%lu (k), dirent=%s, number=%d\n",
		local_config.memory / 1024,
		local_config.odp_hugepage_type[0].hugepage_sz / 1024,
		local_config.odp_hugepage_type[0].hugedir,
		local_config.odp_hugepage_type[0].num_pages[0]);

	odp_global_config_init();
	if (odp_pci_info_init() < 0) {
		ODP_ERR("Cannot init PCI\n");
		return -1;
	}

	if (odp_memory_init() < 0) {
		ODP_ERR("Cannot init memory\n");
		return -1;
	}

	/* the directories are locked during odp_hugepage_info_init */
	odp_hugedirs_unlock();

	if (odp_mm_district_init() < 0) {
		ODP_ERR("Cannot init mm_district\n");
		return -1;
	}

	if (odp_tailqs_init() < 0) {
		ODP_ERR("Cannot init tail queues for objects\n");
		return -1;
	}

	/* odp_check_mem_on_local_socket(); */
	odp_mcfg_complete();

	ret = odp_uio_drv_load_pro();
	if (ret) {
		ODP_ERR("call odp_uio_drv_load_pro failed!\n");
		return ret;
	}

	if (odp_dev_init() < 0) {
		ODP_ERR("Cannot init umd devices\n");
		return -1;
	}

	/* Probe & Initialize PCI devices */
	if (odp_pci_probe()) {
		ODP_ERR("Cannot probe PCI\n");
		return -1;
	}

	run_once = 1;

	return 0;
}

/* get core role */
enum odp_core_role_t odp_core_role(unsigned core_id)
{
	return godp_config.core_role[core_id];
}

enum odp_proc_type_t odp_process_type(void)
{
	return godp_config.process_type;
}

int odp_has_hugepages(void)
{
	return !local_config.no_hugetlbfs;
}

int odp_check_module(const char *module_name)
{
	char mod_name[30]; /* Any module names can be longer than 30 bytes? */
	int  ret = 0;
	int  n;

	if (NULL == module_name)
		return -1;

	FILE *fd = fopen("/proc/modules", "r");

	if (NULL == fd) {
		ODP_ERR("Open /proc/modules failed! error %i (%s)\n",
			errno, strerror(errno));
		return -1;
	}

	while (!feof(fd)) {
		n = fscanf(fd, "%29s %*[^\n]", mod_name);
		if ((n == 1) && !strcmp(mod_name, module_name)) {
			ret = 1;
			break;
		}
	}

	fclose(fd);

	return ret;
}

void odp_hexdump(FILE *f, const char *title, const void *buf, unsigned int len)
{
	unsigned int i, out, ofs;
	const unsigned char *data = buf;
	char line[LINE_LEN]; /* space needed 8+16*3+3+16 == 75 */

	fprintf(f, "%s at [%p], len=%u\n", (title) ? title  : "  Dump data",
		data, len);
	ofs = 0;
	while (ofs < len) {
		/* format the line in the buffer, then use PRINT
		 * to output to screen */
		out = snprintf(line, LINE_LEN, "%08X:", ofs);
		for (i = 0; ((ofs + i) < len) && (i < 16); i++)
			out += snprintf(line + out, LINE_LEN - out,
					" %02X", (data[ofs + i] & 0xff));

		for (; i <= 16; i++)
			out += snprintf(line + out, LINE_LEN - out, " | ");

		for (i = 0; (ofs < len) && (i < 16); i++, ofs++) {
			unsigned char c = data[ofs];

			if ((c < ' ') || (c > '~'))
				c = '.';

			out += snprintf(line + out, LINE_LEN - out, "%c", c);
		}

		fprintf(f, "%s\n", line);
	}

	fflush(f);
}

enum odp_proc_type_t odp_get_process_type(void)
{
	return local_config.process_type;
}

int odp_init_hisilicon(void)
{
	if (odp_arch_pv660_init(0, (char **)NULL) < 0) {
		ODP_ERR("odp_arch_pv660_init fail!");
		return -1;
	}

	return 0;
}

int odp_hisi_term_global(void)
{
	return 0;
}
