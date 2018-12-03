#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <numa.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <assert.h>
#include "mtcp_api.h"
#ifndef DISABLE_DPDK
#include <rte_per_lcore.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <gmp.h>
#include <mtcp.h>
#endif

#define MAX_FILE_NAME 1024

/*----------------------------------------------------------------------------*/
inline int 
GetNumCPUs() 
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}
/*----------------------------------------------------------------------------*/
pid_t 
Gettid()
{
	return syscall(__NR_gettid);
}
/*----------------------------------------------------------------------------*/
inline int
whichCoreID(int thread_no)
{
#ifndef DISABLE_DPDK
	int i, cpu_id;
	if (mpz_get_ui(CONFIG._cpumask) == 0)
		return thread_no;
	else {
		int limit =  mpz_popcount(CONFIG._cpumask);
		
		for (cpu_id = 0, i = 0; i < limit; cpu_id++)
			if (mpz_tstbit(CONFIG._cpumask, cpu_id)) {
				if (thread_no == i)
					return cpu_id;
				i++;
			}
	}
#endif
	return thread_no;
}
/*----------------------------------------------------------------------------*/
int 
mtcp_core_affinitize(int cpu)
{
	cpu_set_t cpus;
	size_t n;
	int ret;

	n = GetNumCPUs();

	cpu = whichCoreID(cpu);
	
	if (cpu < 0 || cpu >= (int) n) {
		errno = -EINVAL;
		return -1;
	}

	CPU_ZERO(&cpus);
	CPU_SET((unsigned)cpu, &cpus);

#ifndef DISABLE_DPDK
	return rte_thread_set_affinity(&cpus);
#else
	struct bitmask *bmask;
	FILE *fp;
	char sysfname[MAX_FILE_NAME];
	int phy_id;
	
	ret = sched_setaffinity(Gettid(), sizeof(cpus), &cpus);

	if (numa_max_node() == 0)
		return ret;

	bmask = numa_bitmask_alloc(numa_max_node() + 1);
	assert(bmask);

	/* read physical id of the core from sys information */
	snprintf(sysfname, MAX_FILE_NAME - 1, 
			"/sys/devices/system/cpu/cpu%d/topology/physical_package_id", cpu);
	fp = fopen(sysfname, "r");
	if (!fp) {
		perror(sysfname);
		errno = EFAULT;
		return -1;
	}
	ret = fscanf(fp, "%d", &phy_id);
	if (ret != 1) {
		fclose(fp);
		perror("Fail to read core id");
		errno = EFAULT;
		return -1;
	}

	numa_bitmask_setbit(bmask, phy_id);
	numa_set_membind(bmask);
	numa_bitmask_free(bmask);

	fclose(fp);
#endif
	return ret;
}
