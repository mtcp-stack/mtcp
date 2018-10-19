#define _GNU_SOURCE		1
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <dirent.h>
#include <rte_version.h>
#include <rte_ethdev.h>
#include "dpdk_iface_common.h"
/*--------------------------------------------------------------------------*/
//#define DEBUG				1
#define SYSFS_PCI_DRIVER_PATH		"/sys/bus/pci/drivers/"
#define SYSFS_PCI_IGB_UIO		SYSFS_PCI_DRIVER_PATH"igb_uio"
#define SYSFS_PCI_VFIO_PCI		SYSFS_PCI_DRIVER_PATH"vfio-pci"
#define SYSFS_PCI_UIOPCIGEN		SYSFS_PCI_DRIVER_PATH"uio_pci_generic"
#define RTE_ARGC_MAX			(RTE_MAX_ETHPORTS << 1) + 7
/*--------------------------------------------------------------------------*/
typedef struct {
	PciDevice pd;
	struct rte_eth_dev_info dev_details;
	struct ether_addr ports_eth_addr;
} DevInfo;

static DevInfo di[RTE_MAX_ETHPORTS];	
/*--------------------------------------------------------------------------*/
/**
 * Really crappy version for detecting pci entries..
 * but it should work.
 */
int
IsPciEnt(const struct dirent *entry)
{
	if (entry->d_type == DT_LNK &&
	    strstr(entry->d_name, ":") != NULL)
		return 1;

	return 0;
}
/*--------------------------------------------------------------------------*/
/**
 * Similar to strverscmp(), but sorts in hexadecimal context
 */
int
localversionsort(const void *elem1, const void *elem2)
{
	uint16_t domain1, domain2;
	uint8_t bus1, bus2, device1, device2, function1, function2;
	DevInfo *d1 = (DevInfo *)elem1;
	DevInfo *d2 = (DevInfo *)elem2;

	domain1 = d1->pd.pa.domain;
	domain2 = d2->pd.pa.domain;
	bus1 = d1->pd.pa.bus;
	bus2 = d2->pd.pa.bus;
	device1 = d1->pd.pa.device;
	device2 = d2->pd.pa.device;
	function1 = d1->pd.pa.function;
	function2 = d2->pd.pa.function;

	if (domain1 < domain2) return -1;
	if (domain2 < domain1) return 1;

	if (bus1 < bus2) return -1;
	if (bus2 < bus1) return 1;

	if (device1 < device2) return -1;
	if (device2 < device1) return 1;

	if (function1 < function2)
		return -1;
	if (function2 < function1)
		return 1;

	return 0;
}
/*--------------------------------------------------------------------------*/
int
probe_all_rte_devices(char **argv, int *argc)
{
	struct dirent **dirlist;
	int pci_index, total_files, i, j;

	/* reset pci_index */
	pci_index = 0;

	for (j = 0; j < 3; j++) {
		switch (j) {
		case 0:
			/* scan igb_uio first */
			total_files = scandir(SYSFS_PCI_IGB_UIO, &dirlist,
					      IsPciEnt, versionsort);
			break;
		case 1:
			/* scan vfio_pci next */
			total_files = scandir(SYSFS_PCI_VFIO_PCI, &dirlist,
					      IsPciEnt, versionsort);
			break;
		case 2:
			/* finally scan uio_pci_generic */
			total_files = scandir(SYSFS_PCI_UIOPCIGEN, &dirlist,
					      IsPciEnt, versionsort);
			break;
		default:
			fprintf(stderr, "Control can never come here!\n");
			goto panic_err;
		}

		for (i = 0; i < total_files; i++, pci_index++) {
			argv[*argc] = strdup("-w");
			argv[*argc + 1] = strdup(dirlist[i]->d_name);
			if (argv[*argc] == NULL ||
			    argv[*argc + 1] == NULL)
				goto alloc_err;
			*argc += 2;
			if (sscanf(dirlist[i]->d_name, PCI_DOM":"PCI_BUS":"
				   PCI_DEVICE"."PCI_FUNC,
				   &di[pci_index].pd.pa.domain,
				   &di[pci_index].pd.pa.bus,
				   &di[pci_index].pd.pa.device,
				   &di[pci_index].pd.pa.function) != 4)
				goto sscanf_err;
			free(dirlist[i]);
		}
		
		//free(dirlist);
	}

	/* now sort all recorded entries */
	qsort(di, pci_index, sizeof(DevInfo), localversionsort);
	return pci_index;
 sscanf_err:
	fprintf(stderr, "Unable to retrieve pci address!\n");
	exit(EXIT_FAILURE);
 alloc_err:
	fprintf(stderr, "Can't allocate memory for argv items!\n");
	exit(EXIT_FAILURE);
 panic_err:
	fprintf(stderr, "Could not open the directory!\n");
	exit(EXIT_FAILURE);
}
/*--------------------------------------------------------------------------*/
int
fetch_major_no()
{
	FILE *f;
	int major_no;
	char *line;
	size_t len;
	char dummy[512];

	major_no = -1;
	len = 0;
	line = NULL;
	
	f = fopen(DEV_PROC_PATH, "r");
	if (f == NULL) {
		fprintf(stderr, "Can't open %s file\n", DEV_PROC_PATH);
		return -1;
	}

	while (getline(&line, &len, f) != -1) {
		if (strstr(line, DEV_NAME) != NULL) {
			if (sscanf(line, "%d %s", &major_no, dummy) == 2) {
				free(line);
				break;
			}
		}
		free(line);
		line = NULL;
		len = 0;
	}
	
	/* close the file descriptor */
	fclose(f);

	return major_no;
}
/*--------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	int ret, fd, num_devices, i;
	dev_t dev;
	char *cpumaskbuf = "0x1";
	char *mem_channels = "4";
	char *rte_argv[RTE_ARGC_MAX] = {"",
					"-c",
					cpumaskbuf,
					"-n",
					mem_channels,
					"--proc-type=auto"
	};
	int rte_argc = 6;

	ret = probe_all_rte_devices(rte_argv, &rte_argc);

#if DEBUG
	for (i = 0; i < ret; i++) {
		fprintf(stderr, "Pci Address: %04hX:%02hhX:%02hhX.%01hhX\n",
			di[i].pd.pa.domain,
			di[i].pd.pa.bus,
			di[i].pd.pa.device,
			di[i].pd.pa.function);
	}
#endif
	
	if (geteuid()) {
		fprintf(stderr, "[CAUTION] Run the app as root!\n");
		exit(EXIT_FAILURE);
	}

	/* remove previously created dpdk-iface device node file */
	fprintf(stderr, "Removing existing device node entry...");
	ret = remove(DEV_PATH);
	fprintf(stderr, (ret == 0) ? "\033[32m done. \033[0m \n" :
		"\033[32m not present. \033[0m \n");

	/* create dpdk-iface device node entry */
#if 0
	dev = makedev(MAJOR_NO, 0);
#else
	dev = makedev(fetch_major_no(), 0);
#endif
	ret = mknod(DEV_PATH, S_IFCHR | O_RDWR, dev);
	if (ret == 0)
		fprintf(stderr, "Creating device node entry...");
	else {
		fprintf(stderr, "Failed to create device node entry\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr, "\033[32m done. \033[0m \n");
	
	/* setting permissions on the device node entry */
	ret = chmod(DEV_PATH,
		    S_IRGRP | S_IROTH | S_IRUSR |
		    S_IWGRP | S_IWOTH | S_IWUSR);

	if (ret == 0)
		fprintf(stderr, "Setting permissions on the device node entry...");
	else {
		fprintf(stderr, "Failed to set permissions on the device node entry\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr, "\033[32m done. \033[0m \n");
	
#if RTE_VERSION < RTE_VERSION_NUM(17, 05, 0, 16)
	rte_set_log_level(RTE_LOG_EMERG);
#else
	rte_log_set_global_level(RTE_LOG_EMERG);
#endif

	fprintf(stderr, "Scanning the system for dpdk-compatible devices...");
	/* initialize the rte env first */
	ret = rte_eal_init(rte_argc, rte_argv);

	/* get total count of detected ethernet ports */
	num_devices = rte_eth_dev_count();
	if (num_devices == 0) {
		fprintf(stderr, "No Ethernet port detected!\n");
		exit(EXIT_FAILURE);
	}

	for (ret = 0; ret < num_devices; ret++) {
		di[ret].pd.ports_eth_addr = &di[ret].ports_eth_addr.addr_bytes[0];
		/* get mac addr entries of detected dpdk ports */
		rte_eth_macaddr_get(ret, &di[ret].ports_eth_addr);
		/* check port capabailties/info */
		rte_eth_dev_info_get(ret, &di[ret].dev_details);
		/* get numa socket location for future socket-mem field */
		if ((di[ret].pd.numa_socket=rte_eth_dev_socket_id(ret)) == -1) {
			fprintf(stderr, "Can't determine socket ID!\n");
			exit(EXIT_FAILURE);
		}
	}

	fprintf(stderr, "\033[32m done. \033[0m \n");
	
	/* open the device node first */
	fd = open(DEV_PATH, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Failed to open %s for port detection!\n",
			DEV_PATH);
		exit(EXIT_FAILURE);
	}

	/* clear all previous entries */
	fprintf(stderr, "Clearing previous entries\n");
	ret = ioctl(fd, CLEAR_IFACE, di[0].ports_eth_addr.addr_bytes);
	if (ret == -1) {
		fprintf(stderr, "ioctl call failed!\n");
		return EXIT_FAILURE;
	}
	
	/* register the newly detected dpdk ports */
	for (ret = 0; ret < num_devices; ret++) {
		if (strcmp(di[ret].dev_details.driver_name, "net_mlx4") &&
		    strcmp(di[ret].dev_details.driver_name, "net_mlx5")) {
			fprintf(stderr, "Registering port %d (%02X:%02X:%02X:%02X:%02X:%02X) to mTCP stack",
				ret,
				di[ret].ports_eth_addr.addr_bytes[0],
				di[ret].ports_eth_addr.addr_bytes[1],
				di[ret].ports_eth_addr.addr_bytes[2],
				di[ret].ports_eth_addr.addr_bytes[3],
				di[ret].ports_eth_addr.addr_bytes[4],
				di[ret].ports_eth_addr.addr_bytes[5]);
			di[ret].pd.ports_eth_addr = di[ret].ports_eth_addr.addr_bytes;

			if (ioctl(fd, CREATE_IFACE, &di[ret].pd) == -1) {
				fprintf(stderr, "ioctl call failed!\n");
			}
			fprintf(stderr, " (%s).\n",
				di[ret].pd.ifname);
		}
	}

	/* close the fd */
	close(fd);

#if 0
	/*
	 * XXX: It seems that there is a bug in the RTE SDK.
	 * The dynamically allocated rte_argv params are left 
	 * as dangling pointers. Freeing them causes program
	 * to crash.
	 */
	
	/* free up all resources */
	for (; rte_argc >= 6; rte_argc--) {
		if (rte_argv[rte_argc] != NULL) {
			fprintf(stderr, "Cleaning up rte_argv[%d]: %s (%p)\n",
				rte_argc, rte_argv[rte_argc], rte_argv[rte_argc]);
			free(rte_argv[rte_argc]);
			rte_argv[rte_argc] = NULL;
		}
	}
#endif	
	return EXIT_SUCCESS;
}
/*--------------------------------------------------------------------------*/
