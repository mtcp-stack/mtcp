/* for I/O module def'ns */
#include "io_module.h"
/* for num_devices decl */
#include "config.h"
/* std lib funcs */
#include <stdlib.h>
/* std io funcs */
#include <stdio.h>
/* strcmp func etc. */
#include <string.h>
/* for ifreq struct */
#include <net/if.h>
/* for ioctl */
#include <sys/ioctl.h>
#ifndef DISABLE_DPDK
/* for dpdk ethernet functions (get mac addresses) */
#include <rte_ethdev.h>
#endif
/* for TRACE_* */
#include "debug.h"
/* for inet_* */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* for getopt() */
#include <unistd.h>
/* for getifaddrs */
#include <sys/types.h>
#include <ifaddrs.h>
/*----------------------------------------------------------------------------*/
io_module_func *current_iomodule_func = &dpdk_module_func;
#define ALL_STRING			"all"
#define MAX_PROCLINE_LEN		1024
#define MAX(a, b) 			((a)>(b)?(a):(b))
#define MIN(a, b) 			((a)<(b)?(a):(b))
/*----------------------------------------------------------------------------*/
#ifndef DISABLE_PSIO
static int
GetNumQueues()
{
	FILE *fp;
	char buf[MAX_PROCLINE_LEN];
	int queue_cnt;

	fp = fopen("/proc/interrupts", "r");
	if (!fp) {
		TRACE_CONFIG("Failed to read data from /proc/interrupts!\n");
		return -1;
	}

	/* count number of NIC queues from /proc/interrupts */
	queue_cnt = 0;
	while (!feof(fp)) {
		if (fgets(buf, MAX_PROCLINE_LEN, fp) == NULL)
			break;

		/* "xge0-rx" is the keyword for counting queues */
		if (strstr(buf, "xge0-rx")) {
			queue_cnt++;
		}
	}
	fclose(fp);

	return queue_cnt;
}
#endif /* !PSIO */
/*----------------------------------------------------------------------------*/
int
SetInterfaceInfo(char* dev_name_list)
{
	int eidx = 0;
	int i, j;

	int set_all_inf = (strncmp(dev_name_list, ALL_STRING, sizeof(ALL_STRING))==0);

	TRACE_CONFIG("Loading interface setting\n");

	CONFIG.eths = (struct eth_table *)
			calloc(MAX_DEVICES, sizeof(struct eth_table));
	if (!CONFIG.eths) {
		TRACE_ERROR("Can't allocate space for CONFIG.eths\n");
		exit(EXIT_FAILURE);
	}

	if (current_iomodule_func == &ps_module_func) {
#ifndef DISABLE_PSIO
		struct ifreq ifr;		
		/* calculate num_devices now! */
		num_devices = ps_list_devices(devices);
		if (num_devices == -1) {
			perror("ps_list_devices");
			exit(EXIT_FAILURE);
		}

		/* Create socket */
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (sock == -1) {
			TRACE_ERROR("socket");
			exit(EXIT_FAILURE);
		}

		/* To Do: Parse dev_name_list rather than use strstr */
		for (i = 0; i < num_devices; i++) {
			strcpy(ifr.ifr_name, devices[i].name);

			/* getting interface information */
			if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {

				if (!set_all_inf && strstr(dev_name_list, ifr.ifr_name) == NULL)
					continue;

				/* Setting informations */
				eidx = CONFIG.eths_num++;
				strcpy(CONFIG.eths[eidx].dev_name, ifr.ifr_name);
				CONFIG.eths[eidx].ifindex = devices[i].ifindex;

				/* getting address */
				if (ioctl(sock, SIOCGIFADDR, &ifr) == 0 ) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].ip_addr = *(uint32_t *)&sin;
				}

				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0 ) {
					for (j = 0; j < ETH_ALEN; j ++) {
						CONFIG.eths[eidx].haddr[j] = ifr.ifr_addr.sa_data[j];
					}
				}

				/* Net MASK */
				if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].netmask = *(uint32_t *)&sin;
				}

				/* add to attached devices */
				for (j = 0; j < num_devices_attached; j++) {
					if (devices_attached[j] == devices[i].ifindex) {
						break;
					}
				}
				devices_attached[num_devices_attached] = devices[i].ifindex;
				num_devices_attached++;

			} else {
				perror("SIOCGIFFLAGS");
			}
		}
		num_queues = GetNumQueues();
		if (num_queues <= 0) {
			TRACE_CONFIG("Failed to find NIC queues!\n");
			close(sock);
			return -1;
		}
		if (num_queues > num_cpus) {
			TRACE_CONFIG("Too many NIC queues available.\n");
			close(sock);
			return -1;
		}
		close(sock);
#endif /* !PSIO_MODULE */
	} else if (current_iomodule_func == &dpdk_module_func) {
#ifndef DISABLE_DPDK
		int cpu = CONFIG.num_cores;
		uint32_t cpumask = 0;
		char cpumaskbuf[10];
		char mem_channels[5];
		int ret;
		static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

		/* get the cpu mask */
		for (ret = 0; ret < cpu; ret++)
			cpumask = (cpumask | (1 << ret));
		sprintf(cpumaskbuf, "%X", cpumask);

		/* get the mem channels per socket */
		if (CONFIG.num_mem_ch == 0) {
			TRACE_ERROR("DPDK module requires # of memory channels "
				    "per socket parameter!\n");
			exit(EXIT_FAILURE);
		}
		sprintf(mem_channels, "%d", CONFIG.num_mem_ch);

		/* initialize the rte env first, what a waste of implementation effort!  */
		char *argv[] = {"",
				"-c",
				cpumaskbuf,
				"-n",
				mem_channels,
				"--proc-type=auto",
				""
		};
		const int argc = 6;

		/*
		 * re-set getopt extern variable optind.
		 * this issue was a bitch to debug
		 * rte_eal_init() internally uses getopt() syscall
		 * mtcp applications that also use an `external' getopt
		 * will cause a violent crash if optind is not reset to zero
		 * prior to calling the func below...
		 * see man getopt(3) for more details
		 */
		optind = 0;

		/* initialize the dpdk eal env */
		ret = rte_eal_init(argc, argv);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Invalid EAL args!\n");
		/* give me the count of 'detected' ethernet ports */
		num_devices = rte_eth_dev_count();
		if (num_devices == 0) {
			rte_exit(EXIT_FAILURE, "No Ethernet port!\n");
		}

		/* get mac addr entries of 'detected' dpdk ports */
		for (ret = 0; ret < num_devices; ret++)
			rte_eth_macaddr_get(ret, &ports_eth_addr[ret]);

		num_queues = MIN(CONFIG.num_cores, MAX_CPUS);

		struct ifaddrs *ifap;
		struct ifaddrs *iter_if;
		char *seek;

		if (getifaddrs(&ifap) != 0) {
			perror("getifaddrs: ");
			exit(EXIT_FAILURE);
		}

		iter_if = ifap;
		do {
			if (iter_if->ifa_addr->sa_family == AF_INET &&
			    !set_all_inf &&
			    (seek=strstr(dev_name_list, iter_if->ifa_name)) != NULL &&
			    /* check if the interface was not aliased */
			    *(seek + strlen(iter_if->ifa_name)) != ':') {
				struct ifreq ifr;

				/* Setting informations */
				eidx = CONFIG.eths_num++;
				strcpy(CONFIG.eths[eidx].dev_name, iter_if->ifa_name);
				strcpy(ifr.ifr_name, iter_if->ifa_name);

				/* Create socket */
				int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
				if (sock == -1) {
					perror("socket");
					exit(EXIT_FAILURE);
				}

				/* getting address */
				if (ioctl(sock, SIOCGIFADDR, &ifr) == 0 ) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].ip_addr = *(uint32_t *)&sin;
				}

				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0 ) {
					for (j = 0; j < ETH_ALEN; j ++) {
						CONFIG.eths[eidx].haddr[j] = ifr.ifr_addr.sa_data[j];
					}
				}

				/* Net MASK */
				if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].netmask = *(uint32_t *)&sin;
				}
				close(sock);

				for (j = 0; j < num_devices; j++) {
					if (!memcmp(&CONFIG.eths[eidx].haddr[0], &ports_eth_addr[j],
						    ETH_ALEN))
						CONFIG.eths[eidx].ifindex = j;
				}

				/* add to attached devices */
				for (j = 0; j < num_devices_attached; j++) {
					if (devices_attached[j] == CONFIG.eths[eidx].ifindex) {
						break;
					}
				}
				devices_attached[num_devices_attached] = CONFIG.eths[eidx].ifindex;
				num_devices_attached++;
				fprintf(stderr, "Total number of attached devices: %d\n",
					num_devices_attached);
				fprintf(stderr, "Interface name: %s\n",
					iter_if->ifa_name);
			}
			iter_if = iter_if->ifa_next;
		} while (iter_if != NULL);

		freeifaddrs(ifap);
#endif /* !DISABLE_DPDK */
	} else if (current_iomodule_func == &netmap_module_func) {
#ifndef DISABLE_NETMAP
		struct ifaddrs *ifap;
		struct ifaddrs *iter_if;
		char *seek;

		num_queues = MIN(CONFIG.num_cores, MAX_CPUS);

		if (getifaddrs(&ifap) != 0) {
			perror("getifaddrs: ");
			exit(EXIT_FAILURE);
		}

		iter_if = ifap;
		do {
			if (iter_if->ifa_addr->sa_family == AF_INET &&
			    !set_all_inf &&
			    (seek=strstr(dev_name_list, iter_if->ifa_name)) != NULL &&
			    /* check if the interface was not aliased */
			    *(seek + strlen(iter_if->ifa_name)) != ':') {
				struct ifreq ifr;

				/* Setting informations */
				eidx = CONFIG.eths_num++;
				strcpy(CONFIG.eths[eidx].dev_name, iter_if->ifa_name);
				strcpy(ifr.ifr_name, iter_if->ifa_name);

				/* Create socket */
				int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
				if (sock == -1) {
					perror("socket");
					exit(EXIT_FAILURE);
				}

				/* getting address */
				if (ioctl(sock, SIOCGIFADDR, &ifr) == 0 ) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].ip_addr = *(uint32_t *)&sin;
				}

				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0 ) {
					for (j = 0; j < ETH_ALEN; j ++) {
						CONFIG.eths[eidx].haddr[j] = ifr.ifr_addr.sa_data[j];
					}
				}

				/* Net MASK */
				if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].netmask = *(uint32_t *)&sin;
				}
				close(sock);
#if 0
				for (j = 0; j < num_devices; j++) {
					if (!memcmp(&CONFIG.eths[eidx].haddr[0], &ports_eth_addr[j],
						    ETH_ALEN))
						CONFIG.eths[eidx].ifindex = ifr.ifr_ifindex;
#endif
				CONFIG.eths[eidx].ifindex = eidx;//if_nametoindex(ifr.ifr_name);
				TRACE_INFO("Ifindex of interface %s is: %d\n",
					   ifr.ifr_name, CONFIG.eths[eidx].ifindex);
#if 0
				}
#endif

				/* add to attached devices */
				for (j = 0; j < num_devices_attached; j++) {
					if (devices_attached[j] == CONFIG.eths[eidx].ifindex) {
						break;
					}
				}
				devices_attached[num_devices_attached] = if_nametoindex(ifr.ifr_name);//CONFIG.eths[eidx].ifindex;
				num_devices_attached++;
				fprintf(stderr, "Total number of attached devices: %d\n",
					num_devices_attached);
				fprintf(stderr, "Interface name: %s\n",
					iter_if->ifa_name);
			}
			iter_if = iter_if->ifa_next;
		} while (iter_if != NULL);

		freeifaddrs(ifap);
#endif /* !DISABLE_NETMAP */
	}

	CONFIG.nif_to_eidx = (int*)calloc(MAX_DEVICES, sizeof(int));

	if (!CONFIG.nif_to_eidx) {
	        exit(EXIT_FAILURE);
	}

	for (i = 0; i < MAX_DEVICES; ++i) {
	        CONFIG.nif_to_eidx[i] = -1;
	}

	for (i = 0; i < CONFIG.eths_num; ++i) {

		j = CONFIG.eths[i].ifindex;
		if (j >= MAX_DEVICES) {
		        TRACE_ERROR("ifindex of eths_%d exceed the limit: %d\n", i, j);
		        exit(EXIT_FAILURE);
		}

		/* the physic port index of the i-th port listed in the config file is j*/
		CONFIG.nif_to_eidx[j] = i;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int
FetchEndianType()
{
#ifndef DISABLE_DPDK
	char *argv;
	char **argp = &argv;
	/* dpdk_module_func logic down below */
	dpdk_module_func.dev_ioctl(NULL, CONFIG.eths[0].ifindex, DRV_NAME, (void *)argp);
	if (!strcmp(*argp, "net_i40e"))
		return 1;

	return 0;
#else
	return 1;
#endif
}
/*----------------------------------------------------------------------------*/
