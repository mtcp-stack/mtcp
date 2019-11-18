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
#define RTE_ARGC_MAX		(RTE_MAX_ETHPORTS << 1) + 9
/* for dpdk ethernet functions (get mac addresses) */
#include <rte_ethdev.h>
#include <dpdk_iface_common.h>
/* for ceil func */
#include <math.h>
/* for retrieving rte version(s) */
#include <rte_version.h>
#endif /* DISABLE_DPDK */
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
#ifdef ENABLE_ONVM
/* for onvm */
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
/* for dpdk/onvm big ints */
#include <gmp.h>
#endif
/* for file opening */
#include <sys/stat.h>
#include <fcntl.h>
/* for netmap macros */
#include "netmap_user.h"
/*----------------------------------------------------------------------------*/
io_module_func *current_iomodule_func = &dpdk_module_func;
#ifndef DISABLE_DPDK
enum rte_proc_type_t eal_proc_type_detect(void);
/**
 * DPDK's RTE consumes some huge pages for internal bookkeeping.
 * Therefore, it is not always safe to reserve the exact amount
 * of pages for our stack (e.g. dividing requested mem, in MB, by
 * (1<<20) would be insufficient). Hence, the following value.
 */
#define RTE_SOCKET_MEM_SHIFT		((1<<19)|(1<<18))
#endif
/*----------------------------------------------------------------------------*/
#define ALL_STRING			"all"
#define MAX_PROCLINE_LEN		1024
#define MAX(a, b) 			((a)>(b)?(a):(b))
#define MIN(a, b) 			((a)<(b)?(a):(b))
/*----------------------------------------------------------------------------*/

/* onvm struct for port info lookup */
extern struct port_info *ports;

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
#ifndef DISABLE_DPDK
/**
 * returns max numa ID while probing for rte devices
 */
static int
probe_all_rte_devices(char **argv, int *argc, char *dev_name_list)
{
	PciDevice pd;
	int fd, numa_id = -1;
	static char end[] = "";
	static const char delim[] = " \t";
	static char *dev_tokenizer;
	char *dev_token, *saveptr;

	dev_tokenizer = strdup(dev_name_list);
	if (dev_tokenizer == NULL) {
		TRACE_ERROR("Can't allocate memory for dev_tokenizer!\n");
		exit(EXIT_FAILURE);
	}
	fd = open(DEV_PATH, O_RDONLY);
	if (fd != -1) {
		dev_token = strtok_r(dev_tokenizer, delim, &saveptr);
		while (dev_token != NULL) {
			strcpy(pd.ifname, dev_token);
			if (ioctl(fd, FETCH_PCI_ADDRESS, &pd) == -1) {
				TRACE_DBG("Could not find pci info on dpdk "
					  "device: %s. Is it a dpdk-attached "
					  "interface?\n", dev_token);
				goto loop_over;
			}
			argv[*argc] = strdup("-w");
			argv[*argc + 1] = calloc(PCI_LENGTH, 1);
			if (argv[*argc] == NULL ||
			    argv[*argc + 1] == NULL) {
				TRACE_ERROR("Memory allocation error!\n");
				exit(EXIT_FAILURE);
			}
			sprintf(argv[*argc + 1], PCI_DOM":"PCI_BUS":"
				PCI_DEVICE"."PCI_FUNC,
				pd.pa.domain, pd.pa.bus, pd.pa.device,
				pd.pa.function);
			*argc += 2;
			if (pd.numa_socket > numa_id) numa_id = pd.numa_socket;
		loop_over:
			dev_token = strtok_r(NULL, delim, &saveptr);
		}
		close(fd);
		free(dev_tokenizer);
	} else {
		TRACE_ERROR("Error opening dpdk-face!\n");
		exit(EXIT_FAILURE);
	}

	/* add the terminating "" sequence */
	argv[*argc] = end;

	return numa_id;
}
#endif /* !DISABLE_DPDK */
/*----------------------------------------------------------------------------*/
int
SetNetEnv(char *dev_name_list, char *port_stat_list)
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
		mpz_t _cpumask;
		char cpumaskbuf[32] = "";
		char mem_channels[8] = "";
		char socket_mem_str[32] = "";
		// int i;
		int ret, socket_mem;
#if RTE_VERSION < RTE_VERSION_NUM(19, 8, 0, 0)
		static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
#else
		static struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS]; 
#endif

		/* STEP 1: first determine CPU mask */
		mpz_init(_cpumask);

		if (!mpz_cmp(_cpumask, CONFIG._cpumask)) {
			/* get the cpu mask */
			for (ret = 0; ret < cpu; ret++)
				mpz_setbit(_cpumask, ret);
			
			gmp_sprintf(cpumaskbuf, "%ZX", _cpumask);
		} else
			gmp_sprintf(cpumaskbuf, "%ZX", CONFIG._cpumask);
		
		mpz_clear(_cpumask);

		/* STEP 2: determine memory channels per socket */
		/* get the mem channels per socket */
		if (CONFIG.num_mem_ch == 0) {
			TRACE_ERROR("DPDK module requires # of memory channels "
				    "per socket parameter!\n");
			exit(EXIT_FAILURE);
		}
		sprintf(mem_channels, "%d", CONFIG.num_mem_ch);

		/* STEP 3: determine socket memory */
		/* get socket memory threshold (in MB) */
		socket_mem = 
			RTE_ALIGN_CEIL((unsigned long)ceil((CONFIG.num_cores *
							    (CONFIG.rcvbuf_size +
							     CONFIG.sndbuf_size +
							     sizeof(struct tcp_stream) +
							     sizeof(struct tcp_recv_vars) +
							     sizeof(struct tcp_send_vars) +
							     sizeof(struct fragment_ctx)) *
							    CONFIG.max_concurrency)/RTE_SOCKET_MEM_SHIFT),
				       RTE_CACHE_LINE_SIZE);
		
		/* initialize the rte env, what a waste of implementation effort! */
		int argc = 6;//8;
		char *argv[RTE_ARGC_MAX] = {"",
					    "-c",
					    cpumaskbuf,
					    "-n",
					    mem_channels,
#if 0
					    "--socket-mem",
					    socket_mem_str,
#endif
					    "--proc-type=auto"
		};
		ret = probe_all_rte_devices(argv, &argc, dev_name_list);


		/* STEP 4: build up socket mem parameter */
		sprintf(socket_mem_str, "%d", socket_mem);
#if 0
		char *smsptr = socket_mem_str + strlen(socket_mem_str);
		for (i = 1; i < ret + 1; i++) {
			sprintf(smsptr, ",%d", socket_mem);
			smsptr += strlen(smsptr);
		}
		TRACE_DBG("socket_mem: %s\n", socket_mem_str);
#endif
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

#ifdef DEBUG
		/* print argv's */
		for (i = 0; i < argc; i++)
			TRACE_INFO("argv[%d]: %s\n", i, argv[i]);
#endif
		/* initialize the dpdk eal env */
		ret = rte_eal_init(argc, argv);
		if (ret < 0) {
			TRACE_ERROR("Invalid EAL args!\n");
			exit(EXIT_FAILURE);
		}
		/* give me the count of 'detected' ethernet ports */
#if RTE_VERSION < RTE_VERSION_NUM(18, 5, 0, 0)
		num_devices = rte_eth_dev_count();
#else
		num_devices = rte_eth_dev_count_avail();
#endif
		if (num_devices == 0) {
			TRACE_ERROR("No Ethernet port!\n");
			exit(EXIT_FAILURE);
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
			if (iter_if->ifa_addr && iter_if->ifa_addr->sa_family == AF_INET &&
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
#if 0
		/*
		 * XXX: It seems that there is a bug in the RTE SDK.
		 * The dynamically allocated rte_argv params are left 
		 * as dangling pointers. Freeing them causes program
		 * to crash.
		 */
		
		/* free up all resources */
		for (; rte_argc >= 9; rte_argc--) {
			if (rte_argv[rte_argc] != NULL) {
				fprintf(stderr, "Cleaning up rte_argv[%d]: %s (%p)\n",
					rte_argc, rte_argv[rte_argc], rte_argv[rte_argc]);
				free(rte_argv[rte_argc]);
				rte_argv[rte_argc] = NULL;
			}
		}
#endif
		/* check if process is primary or secondary */
		CONFIG.multi_process_is_master = (eal_proc_type_detect() == RTE_PROC_PRIMARY) ?
			1 : 0;
		
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
			if (iter_if->ifa_addr && iter_if->ifa_addr->sa_family == AF_INET &&
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
				CONFIG.eths[eidx].ifindex = eidx;
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
				devices_attached[num_devices_attached] = if_nametoindex(ifr.ifr_name);
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
	} else if (current_iomodule_func == &onvm_module_func) {
#ifdef ENABLE_ONVM
		int cpu = CONFIG.num_cores;
		mpz_t cpumask;
		char cpumaskbuf[32];
		char mem_channels[8];
		char service[6];
		char instance[6];
		int ret;

		mpz_init(cpumask);
		
		/* get the cpu mask */
		for (ret = 0; ret < cpu; ret++)
			mpz_setbit(cpumask, ret);
		gmp_sprintf(cpumaskbuf, "%ZX", cpumask);

		mpz_clear(cpumask);
				
		/* get the mem channels per socket */
		if (CONFIG.num_mem_ch == 0) {
			TRACE_ERROR("DPDK module requires # of memory channels "
				    "per socket parameter!\n");
			exit(EXIT_FAILURE);
		}
		sprintf(mem_channels, "%d", CONFIG.num_mem_ch);
		sprintf(service, "%d", CONFIG.onvm_serv);
		sprintf(instance, "%d", CONFIG.onvm_inst);

		/* initialize the rte env first, what a waste of implementation effort!  */
		char *argv[] = {"", 
				"-c", 
				cpumaskbuf,
				"-n", 
				mem_channels,
				"--proc-type=secondary",
				"--",
				"-r",
				service,
				instance,
				""
		};
		
		const int argc = 10; 

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

		/* Initialize onvm */
		CONFIG.nf_local_ctx = onvm_nflib_init_nf_local_ctx();
		ret = onvm_nflib_init(argc, argv, "mtcp_nf", CONFIG.nf_local_ctx, NULL);
		if (ret < 0) {
			TRACE_ERROR("Invalid EAL args!\n");
			exit(EXIT_FAILURE);
		}
		/* give me the count of 'detected' ethernet ports */
		num_devices = ports->num_ports;
		if (num_devices == 0) {
			TRACE_ERROR("No Ethernet port!\n");
			exit(EXIT_FAILURE);
		}

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
			if (iter_if->ifa_addr && iter_if->ifa_addr->sa_family == AF_INET &&
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
				}			
				
				/* getting address */
				if (ioctl(sock, SIOCGIFADDR, &ifr) == 0 ) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].ip_addr = *(uint32_t *)&sin;
				}

				for (j = 0; j < ETH_ALEN; j ++) {
					CONFIG.eths[eidx].haddr[j] = ports->mac[eidx].addr_bytes[j];
					};

				/* Net MASK */
				if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0) {
					struct in_addr sin = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
					CONFIG.eths[eidx].netmask = *(uint32_t *)&sin;
				}
				close(sock);
				
				CONFIG.eths[eidx].ifindex = ports->id[eidx];		
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
#endif /* ENABLE_ONVM */
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

		/* finally set the port stats option `on' */
		if (strstr(port_stat_list, CONFIG.eths[i].dev_name) != 0)
			CONFIG.eths[i].stat_print = TRUE;
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
	/* dpdk_module_func/onvm_module_func logic down below */
	if (current_iomodule_func == &dpdk_module_func) {
		(*current_iomodule_func).dev_ioctl(NULL, CONFIG.eths[0].ifindex, DRV_NAME, (void *)argp);
		if (!strcmp(*argp, "net_i40e"))
			return 1;
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
int
CheckIOModuleAccessPermissions()
{
	int fd;
	/* check if netmap module can access I/O with sudo privileges */
	if (current_iomodule_func == &netmap_module_func) {
		fd = open(NETMAP_DEVICE_NAME, O_RDONLY);
		if (fd != -1)
			close(fd);
		return fd;
	}

	/* sudo privileges are definitely needed otherwise */
	if (geteuid())
		return -1;

	return 0;
}
/*----------------------------------------------------------------------------*/
