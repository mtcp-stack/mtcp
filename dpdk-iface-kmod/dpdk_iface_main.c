#include <rte_ethdev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <rte_version.h>
#include "dpdk_iface_common.h"
/*--------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	int ret, fd, num_devices;
	dev_t dev;
	char *cpumaskbuf = "0x1";
	char *mem_channels = "4";
	char *rte_argv[] = {"",
			    "-c",
			    cpumaskbuf,
			    "-n",
			    mem_channels,
			    "--proc-type=auto",
			    ""
	};
	const int rte_argc = 6;
	typedef struct {
		struct ether_addr ports_eth_addr;
		struct rte_eth_dev_info dev_details;
	} dev_info;
	dev_info di[RTE_MAX_ETHPORTS];

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
	dev = makedev(MAJOR_NO, 0);
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
		/* get mac addr entries of detected dpdk ports */
		rte_eth_macaddr_get(ret, &di[ret].ports_eth_addr);
		/* check port capabailties/info */
		rte_eth_dev_info_get(ret, &di[ret].dev_details);
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
	ioctl(fd, CLEAR_IFACE, di[0].ports_eth_addr.addr_bytes);
	
	/* register the newly detected dpdk ports */
	for (ret = 0; ret < num_devices; ret++) {
		if (strcmp(di[ret].dev_details.driver_name, "net_mlx4") &&
		    strcmp(di[ret].dev_details.driver_name, "net_mlx5")) {
			fprintf(stderr, "Registering port %d (%02X:%02X:%02X:%02X:%02X:%02X) to mTCP stack\n",
				ret,
				di[ret].ports_eth_addr.addr_bytes[0], di[ret].ports_eth_addr.addr_bytes[1],
				di[ret].ports_eth_addr.addr_bytes[2], di[ret].ports_eth_addr.addr_bytes[3],
				di[ret].ports_eth_addr.addr_bytes[4], di[ret].ports_eth_addr.addr_bytes[5]);
			ioctl(fd, 1, di[ret].ports_eth_addr.addr_bytes);
		}
	}

	/* close the fd */
	close(fd);
	return EXIT_SUCCESS;
}
/*--------------------------------------------------------------------------*/
