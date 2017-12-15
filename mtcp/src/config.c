#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

#include "mtcp.h"
#include "config.h"
#include "tcp_in.h"
#include "arp.h"
#include "debug.h"
/* for setting up io modules */
#include "io_module.h"
/* for if_nametoindex */
#include <net/if.h>

#define MAX_ROUTE_ENTRY 64
#define MAX_OPTLINE_LEN 1024
#define ALL_STRING "all"

static const char *route_file = "config/route.conf";
static const char *arp_file = "config/arp.conf";
struct mtcp_manager *g_mtcp[MAX_CPUS] = {NULL};
struct mtcp_config CONFIG = {0};
addr_pool_t ap[ETH_NUM] = {NULL};
/* total cpus detected in the mTCP stack*/
int num_cpus;
/* this should be equal to num_cpus */
int num_queues;
int num_devices;

int num_devices_attached;
int devices_attached[MAX_DEVICES];
/*----------------------------------------------------------------------------*/
static inline int
mystrtol(const char *nptr, int base)
{
	int rval;
	char *endptr;

	errno = 0;
	rval = strtol(nptr, &endptr, 10);
	/* check for strtol errors */
	if ((errno == ERANGE && (rval == LONG_MAX ||
				 rval == LONG_MIN))
	    || (errno != 0 && rval == 0)) {
		perror("strtol");
		exit(EXIT_FAILURE);
	}
	if (endptr == nptr) {
		TRACE_CONFIG("Parsing strtol error!\n");
		exit(EXIT_FAILURE);
	}

	return rval;
}
/*----------------------------------------------------------------------------*/
static int 
GetIntValue(char* value)
{
	int ret = 0;
	ret = strtol(value, (char**)NULL, 10);
	if (errno == EINVAL || errno == ERANGE)
		return -1;
	return ret;
}
/*----------------------------------------------------------------------------*/
inline uint32_t 
MaskFromPrefix(int prefix)
{
	uint32_t mask = 0;
	uint8_t *mask_t = (uint8_t *)&mask;
	int i, j;

	for (i = 0; i <= prefix / 8 && i < 4; i++) {
		for (j = 0; j < (prefix - i * 8) && j < 8; j++) {
			mask_t[i] |= (1 << (7 - j));
		}
	}

	return mask;
}
/*----------------------------------------------------------------------------*/
static void
EnrollRouteTableEntry(char *optstr)
{
	char *daddr_s;
	char *prefix;
#ifdef DISABLE_NETMAP 
	char *dev;
	int i;
#endif
	int ifidx;
	int ridx;
	char *saveptr;
 
	saveptr = NULL;
	daddr_s = strtok_r(optstr, "/", &saveptr);
	prefix = strtok_r(NULL, " ", &saveptr);
#ifdef DISABLE_NETMAP
	dev = strtok_r(NULL, "\n", &saveptr);
#endif
	assert(daddr_s != NULL);
	assert(prefix != NULL);
#ifdef DISABLE_NETMAP	
	assert(dev != NULL);
#endif

	ifidx = -1;
	if (current_iomodule_func == &ps_module_func) {
#ifndef DISABLE_PSIO		
		for (i = 0; i < num_devices; i++) {
			if (strcmp(dev, devices[i].name) != 0)
				continue;
			
			ifidx = devices[i].ifindex;
			break;
		}
		if (ifidx == -1) {
			TRACE_CONFIG("Interface %s does not exist!\n", dev);
			exit(4);
		}
#endif
	} else if (current_iomodule_func == &dpdk_module_func) {
#ifndef DISABLE_DPDK
		for (i = 0; i < num_devices; i++) {
			if (strcmp(CONFIG.eths[i].dev_name, dev))
				continue;
			ifidx = CONFIG.eths[i].ifindex;
			break;
		}
#endif
	}

	ridx = CONFIG.routes++;
	if (ridx == MAX_ROUTE_ENTRY) {
		TRACE_CONFIG("Maximum routing entry limit (%d) has been reached."
		             "Consider increasing MAX_ROUTE_ENTRY.\n", MAX_ROUTE_ENTRY);
		exit(4);
	}

	CONFIG.rtable[ridx].daddr = inet_addr(daddr_s);
	CONFIG.rtable[ridx].prefix = mystrtol(prefix, 10);
	if (CONFIG.rtable[ridx].prefix > 32 || CONFIG.rtable[ridx].prefix < 0) {
		TRACE_CONFIG("Prefix length should be between 0 - 32.\n");
		exit(4);
	}
	
	CONFIG.rtable[ridx].mask = MaskFromPrefix(CONFIG.rtable[ridx].prefix);
	CONFIG.rtable[ridx].masked = 
			CONFIG.rtable[ridx].daddr & CONFIG.rtable[ridx].mask;
	CONFIG.rtable[ridx].nif = ifidx;

	if (CONFIG.rtable[ridx].mask == 0) {
		TRACE_CONFIG("Default Route GW set!\n");
		CONFIG.gateway = &CONFIG.rtable[ridx];
	}	
}
/*----------------------------------------------------------------------------*/
int 
SetRoutingTableFromFile() 
{
#define ROUTES "ROUTES"

	FILE *fc;
	char optstr[MAX_OPTLINE_LEN];
	int i;

	TRACE_CONFIG("Loading routing configurations from : %s\n", route_file);

	fc = fopen(route_file, "r");
	if (fc == NULL) {
		perror("fopen");
		TRACE_CONFIG("Skip loading static routing table\n");
		return -1;
	}

	while (1) {
		char *iscomment;
		int num;
  
		if (fgets(optstr, MAX_OPTLINE_LEN, fc) == NULL)
			break;

		//skip comment
		iscomment = strchr(optstr, '#');
		if (iscomment == optstr)
			continue;
		if (iscomment != NULL)
			*iscomment = 0;

		if (!strncmp(optstr, ROUTES, sizeof(ROUTES) - 1)) {
			num = GetIntValue(optstr + sizeof(ROUTES));
			if (num <= 0)
				break;

			for (i = 0; i < num; i++) {
				if (fgets(optstr, MAX_OPTLINE_LEN, fc) == NULL)
					break;

				if (*optstr == '#') {
					i -= 1;
					continue;
				}
				if (!CONFIG.gateway)
					EnrollRouteTableEntry(optstr);
				else {
					TRACE_ERROR("Default gateway settings in %s should "
						    "always come as last entry!\n",
						    route_file);
					exit(EXIT_FAILURE);
				}	
			}
		}
	}

	fclose(fc);
	return 0;
}
/*----------------------------------------------------------------------------*/
void
PrintRoutingTable()
{
	int i;
	uint8_t *da;
	uint8_t *m;
	uint8_t *md;

	/* print out process start information */
	TRACE_CONFIG("Routes:\n");
	for (i = 0; i < CONFIG.routes; i++) {
		da = (uint8_t *)&CONFIG.rtable[i].daddr;
		m = (uint8_t *)&CONFIG.rtable[i].mask;
		md = (uint8_t *)&CONFIG.rtable[i].masked;
		TRACE_CONFIG("Destination: %u.%u.%u.%u/%d, Mask: %u.%u.%u.%u, "
				"Masked: %u.%u.%u.%u, Route: ifdx-%d\n", 
				da[0], da[1], da[2], da[3], CONFIG.rtable[i].prefix, 
				m[0], m[1], m[2], m[3], md[0], md[1], md[2], md[3], 
				CONFIG.rtable[i].nif);
	}
	if (CONFIG.routes == 0)
		TRACE_CONFIG("(blank)\n");

	TRACE_CONFIG("----------------------------------------------------------"
			"-----------------------\n");
}
/*----------------------------------------------------------------------------*/
void
ParseMACAddress(unsigned char *haddr, char *haddr_str)
{
	int i;
	char *str;
	unsigned int temp;
	char *saveptr = NULL;

	saveptr = NULL;
	str = strtok_r(haddr_str, ":", &saveptr);
	i = 0;
	while (str != NULL) {
		if (i >= ETH_ALEN) {
			TRACE_CONFIG("MAC address length exceeds %d!\n", ETH_ALEN);
			exit(4);
		}
		if (sscanf(str, "%x", &temp) < 1) {
			TRACE_CONFIG("sscanf failed!\n");
			exit(4);
		}
		haddr[i++] = temp;
		str = strtok_r(NULL, ":", &saveptr);
	}
	if (i < ETH_ALEN) {
		TRACE_CONFIG("MAC address length is less than %d!\n", ETH_ALEN);
		exit(4);
	}
}
/*----------------------------------------------------------------------------*/
int 
ParseIPAddress(uint32_t *ip_addr, char *ip_str)
{
	if (ip_str == NULL) {
		*ip_addr = 0;
		return -1;
	}

	*ip_addr = inet_addr(ip_str);
	if (*ip_addr == INADDR_NONE) {
		TRACE_CONFIG("IP address is not valid %s\n", ip_str);
		*ip_addr = 0;
		return -1;
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
int
SetRoutingTable() 
{
	int i, ridx;
	unsigned int c;

	CONFIG.routes = 0;
	CONFIG.rtable = (struct route_table *)
			calloc(MAX_ROUTE_ENTRY, sizeof(struct route_table));
	if (!CONFIG.rtable) 
		exit(EXIT_FAILURE);

	/* set default routing table */
	for (i = 0; i < CONFIG.eths_num; i ++) {
		
		ridx = CONFIG.routes++;
		CONFIG.rtable[ridx].daddr = CONFIG.eths[i].ip_addr & CONFIG.eths[i].netmask;
		
		CONFIG.rtable[ridx].prefix = 0;
		c = CONFIG.eths[i].netmask;
		while ((c = (c >> 1))){
			CONFIG.rtable[ridx].prefix++;
		}
		CONFIG.rtable[ridx].prefix++;
		
		CONFIG.rtable[ridx].mask = CONFIG.eths[i].netmask;
		CONFIG.rtable[ridx].masked = CONFIG.rtable[ridx].daddr;
		CONFIG.rtable[ridx].nif = CONFIG.eths[ridx].ifindex;
	}

	/* set additional routing table */
	SetRoutingTableFromFile();

	return 0;
}
/*----------------------------------------------------------------------------*/
void
PrintInterfaceInfo() 
{
	int i;
		
	/* print out process start information */
	TRACE_CONFIG("Interfaces:\n");
	for (i = 0; i < CONFIG.eths_num; i++) {
			
		uint8_t *da = (uint8_t *)&CONFIG.eths[i].ip_addr;
		uint8_t *nm = (uint8_t *)&CONFIG.eths[i].netmask;

		TRACE_CONFIG("name: %s, ifindex: %d, "
				"hwaddr: %02X:%02X:%02X:%02X:%02X:%02X, "
				"ipaddr: %u.%u.%u.%u, "
				"netmask: %u.%u.%u.%u\n",
				CONFIG.eths[i].dev_name, 
				CONFIG.eths[i].ifindex, 
				CONFIG.eths[i].haddr[0],
				CONFIG.eths[i].haddr[1],
				CONFIG.eths[i].haddr[2],
				CONFIG.eths[i].haddr[3],
				CONFIG.eths[i].haddr[4],
				CONFIG.eths[i].haddr[5],
				da[0], da[1], da[2], da[3],
				nm[0], nm[1], nm[2], nm[3]);
	}
	TRACE_CONFIG("Number of NIC queues: %d\n", num_queues);
	TRACE_CONFIG("----------------------------------------------------------"
			"-----------------------\n");
}
/*----------------------------------------------------------------------------*/
static void
EnrollARPTableEntry(char *optstr)
{
	char *dip_s;		/* destination IP string */
	char *prefix_s;		/* IP prefix string */
	char *daddr_s;		/* destination MAC string */

	int prefix;
	uint32_t dip_mask;
	int idx;

	char *saveptr;
	
	saveptr = NULL;
	dip_s = strtok_r(optstr, "/", &saveptr);
	prefix_s = strtok_r(NULL, " ", &saveptr);
	daddr_s = strtok_r(NULL, "\n", &saveptr);

	assert(dip_s != NULL);
	assert(prefix_s != NULL);
	assert(daddr_s != NULL);

	if (prefix_s == NULL)
		prefix = 32;
	else
		prefix = mystrtol(prefix_s, 10);

	if (prefix > 32 || prefix < 0) {
		TRACE_CONFIG("Prefix length should be between 0 - 32.\n");
		return;
	}

	idx = CONFIG.arp.entries++;

	CONFIG.arp.entry[idx].prefix = prefix;
	ParseIPAddress(&CONFIG.arp.entry[idx].ip, dip_s);
	ParseMACAddress(CONFIG.arp.entry[idx].haddr, daddr_s);
	
	dip_mask = MaskFromPrefix(prefix);
	CONFIG.arp.entry[idx].ip_mask = dip_mask;
	CONFIG.arp.entry[idx].ip_masked = CONFIG.arp.entry[idx].ip & dip_mask;
	if (CONFIG.gateway && ((CONFIG.gateway)->daddr &
			       CONFIG.arp.entry[idx].ip_mask) ==
	    CONFIG.arp.entry[idx].ip_masked) {
		CONFIG.arp.gateway = &CONFIG.arp.entry[idx];
		TRACE_CONFIG("ARP Gateway SET!\n");
	}

/*
	int i, cnt;
	cnt = 1;
	cnt = cnt << (32 - prefix);

	for (i = 0; i < cnt; i++) {
		idx = CONFIG.arp.entries++;
		CONFIG.arp.entry[idx].ip = htonl(ntohl(ip) + i);
		memcpy(CONFIG.arp.entry[idx].haddr, haddr, ETH_ALEN);
	}
*/
}
/*----------------------------------------------------------------------------*/
int 
LoadARPTable()
{
#define ARP_ENTRY "ARP_ENTRY"

	FILE *fc;
	char optstr[MAX_OPTLINE_LEN];
	int numEntry = 0;
	int hasNumEntry = 0;

	TRACE_CONFIG("Loading ARP table from : %s\n", arp_file);

	InitARPTable();

	fc = fopen(arp_file, "r");
	if (fc == NULL) {
		perror("fopen");
		TRACE_CONFIG("Skip loading static ARP table\n");
		return -1;
	}

	while (1) {
		char *p;
		char *temp;

		if (fgets(optstr, MAX_OPTLINE_LEN, fc) == NULL)
			break;

		p = optstr;

		// skip comment
		if ((temp = strchr(p, '#')) != NULL)
			*temp = 0;
		// remove front and tailing spaces
		while (*p && isspace((int)*p))
			p++;
		temp = p + strlen(p) - 1;
		while (temp >= p && isspace((int)*temp))
			   *temp = 0;
		if (*p == 0) /* nothing more to process? */
			continue;

		if (!hasNumEntry && strncmp(p, ARP_ENTRY, sizeof(ARP_ENTRY)-1) == 0) {
			numEntry = GetIntValue(p + sizeof(ARP_ENTRY));
			if (numEntry <= 0) {
				fprintf(stderr, "Wrong entry in arp.conf: %s\n", p);
				exit(-1);
			}
#if 0
			CONFIG.arp.entry = (struct arp_entry *)
				calloc(numEntry + MAX_ARPENTRY, sizeof(struct arp_entry));
			if (CONFIG.arp.entry == NULL) {
				fprintf(stderr, "Wrong entry in arp.conf: %s\n", p);
				exit(-1);
			}
#endif
			hasNumEntry = 1;
		} else {
			if (numEntry <= 0) {
				fprintf(stderr, 
						"Error in arp.conf: more entries than "
						"are specifed, entry=%s\n", p);
				exit(-1);
			}
			EnrollARPTableEntry(p);
			numEntry--;
		}
	}

	fclose(fc);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int
SetMultiProcessSupport(char *multiprocess_details)
{
	char *token = " =";
	char *sample;
	char *saveptr;

	TRACE_CONFIG("Loading multi-process configuration\n");

	saveptr = NULL;
	sample = strtok_r(multiprocess_details, token, &saveptr);
	if (sample == NULL) {
		TRACE_CONFIG("No option for multi-process support given!\n");
		return -1;
	}
	CONFIG.multi_process_curr_core = mystrtol(sample, 10);
	
	sample = strtok_r(NULL, token, &saveptr);
	if (sample != NULL && !strcmp(sample, "master"))
		CONFIG.multi_process_is_master = 1;
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int 
ParseConfiguration(char *line)
{
	char optstr[MAX_OPTLINE_LEN];
	char *p, *q;

	char *saveptr;

	strncpy(optstr, line, MAX_OPTLINE_LEN - 1);
	saveptr = NULL;

	p = strtok_r(optstr, " \t=", &saveptr);
	if (p == NULL) {
		TRACE_CONFIG("No option name found for the line: %s\n", line);
		return -1;
	}

	q = strtok_r(NULL, " \t=", &saveptr);
	if (q == NULL) {
		TRACE_CONFIG("No option value found for the line: %s\n", line);
		return -1;
	}

	if (strcmp(p, "num_cores") == 0) {
		CONFIG.num_cores = mystrtol(q, 10);
		if (CONFIG.num_cores <= 0) {
			TRACE_CONFIG("Number of cores should be larger than 0.\n");
			return -1;
		}
		if (CONFIG.num_cores > num_cpus) {
			TRACE_CONFIG("Number of cores should be smaller than "
					"# physical CPU cores.\n");
			return -1;
		}
		num_cpus = CONFIG.num_cores;
	} else if (strcmp(p, "max_concurrency") == 0) {
		CONFIG.max_concurrency = mystrtol(q, 10);
		if (CONFIG.max_concurrency < 0) {
			TRACE_CONFIG("The maximum concurrency should be larger than 0.\n");
			return -1;
		}
	} else if (strcmp(p, "max_num_buffers") == 0) {
		CONFIG.max_num_buffers = mystrtol(q, 10);
		if (CONFIG.max_num_buffers < 0) {
			TRACE_CONFIG("The maximum # buffers should be larger than 0.\n");
			return -1;
		}
	} else if (strcmp(p, "rcvbuf") == 0) {
		CONFIG.rcvbuf_size = mystrtol(q, 10);
		if (CONFIG.rcvbuf_size < 64) {
			TRACE_CONFIG("Receive buffer size should be larger than 64.\n");
			return -1;
		}
	} else if (strcmp(p, "sndbuf") == 0) {
		CONFIG.sndbuf_size = mystrtol(q, 10);
		if (CONFIG.sndbuf_size < 64) {
			TRACE_CONFIG("Send buffer size should be larger than 64.\n");
			return -1;
		}
	} else if (strcmp(p, "tcp_timeout") == 0) {
		CONFIG.tcp_timeout = mystrtol(q, 10);
		if (CONFIG.tcp_timeout > 0) {
			CONFIG.tcp_timeout = SEC_TO_USEC(CONFIG.tcp_timeout) / TIME_TICK;
		}
	} else if (strcmp(p, "tcp_timewait") == 0) {
		CONFIG.tcp_timewait = mystrtol(q, 10);
		if (CONFIG.tcp_timewait > 0) {
			CONFIG.tcp_timewait = SEC_TO_USEC(CONFIG.tcp_timewait) / TIME_TICK;
		}
	} else if (strcmp(p, "stat_print") == 0) {
		int i;

		for (i = 0; i < CONFIG.eths_num; i++) {
			if (strcmp(CONFIG.eths[i].dev_name, q) == 0) {
				CONFIG.eths[i].stat_print = TRUE;
			}
		}
	} else if (strcmp(p, "port") == 0) {
		if(strncmp(q, ALL_STRING, sizeof(ALL_STRING)) == 0) {
			SetInterfaceInfo(q);
		} else {
			SetInterfaceInfo(line + strlen(p) + 1);
		}
	} else if (strcmp(p, "io") == 0) {
		AssignIOModule(q);
	} else if (strcmp(p, "num_mem_ch") == 0) {
		CONFIG.num_mem_ch = mystrtol(q, 10);
	} else if (strcmp(p, "multiprocess") == 0) {
		CONFIG.multi_process = 1;
		SetMultiProcessSupport(line + strlen(p) + 1);
	} else {
		TRACE_CONFIG("Unknown option type: %s\n", line);
		return -1;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
int 
LoadConfiguration(const char *fname)
{
	FILE *fp;
	char optstr[MAX_OPTLINE_LEN];

	TRACE_CONFIG("----------------------------------------------------------"
			"-----------------------\n");
	TRACE_CONFIG("Loading mtcp configuration from : %s\n", fname);

	fp = fopen(fname, "r");
	if (fp == NULL) {
		perror("fopen");
		TRACE_CONFIG("Failed to load configuration file: %s\n", fname);
		return -1;
	}

	/* set default configuration */
	CONFIG.num_cores = num_cpus;
	CONFIG.max_concurrency = 100000;
	CONFIG.max_num_buffers = 100000;
	CONFIG.rcvbuf_size = 8192;
	CONFIG.sndbuf_size = 8192;
	CONFIG.tcp_timeout = TCP_TIMEOUT;
	CONFIG.tcp_timewait = TCP_TIMEWAIT;
	CONFIG.num_mem_ch = 0;

	while (1) {
		char *p;
		char *temp;

		if (fgets(optstr, MAX_OPTLINE_LEN, fp) == NULL)
			break;

		p = optstr;

		// skip comment
		if ((temp = strchr(p, '#')) != NULL)
			*temp = 0;
		// remove front and tailing spaces
		while (*p && isspace((int)*p))
			p++;
		temp = p + strlen(p) - 1;
		while (temp >= p && isspace((int)*temp))
			   *temp = 0;
		if (*p == 0) /* nothing more to process? */
			continue;

		if (ParseConfiguration(p) < 0) {
			fclose(fp);
			return -1;
		}
	}

	fclose(fp);

	return 0;
}
/*----------------------------------------------------------------------------*/
void 
PrintConfiguration()
{
	int i;

	TRACE_CONFIG("Configurations:\n");
	TRACE_CONFIG("Number of CPU cores available: %d\n", num_cpus);
	TRACE_CONFIG("Number of CPU cores to use: %d\n", CONFIG.num_cores);
	TRACE_CONFIG("Maximum number of concurrency per core: %d\n", 
			CONFIG.max_concurrency);
	if (CONFIG.multi_process == 1) {
		TRACE_CONFIG("Multi-process support is enabled and current core is: %d\n",
			     CONFIG.multi_process_curr_core);
		if (CONFIG.multi_process_is_master == 1)
			TRACE_CONFIG("Current core is master (for multi-process)\n");
		else
			TRACE_CONFIG("Current core is not master (for multi-process)\n");
	}
	TRACE_CONFIG("Maximum number of preallocated buffers per core: %d\n", 
			CONFIG.max_num_buffers);
	TRACE_CONFIG("Receive buffer size: %d\n", CONFIG.rcvbuf_size);
	TRACE_CONFIG("Send buffer size: %d\n", CONFIG.sndbuf_size);

	if (CONFIG.tcp_timeout > 0) {
		TRACE_CONFIG("TCP timeout seconds: %d\n", 
				USEC_TO_SEC(CONFIG.tcp_timeout * TIME_TICK));
	} else {
		TRACE_CONFIG("TCP timeout check disabled.\n");
	}
	TRACE_CONFIG("TCP timewait seconds: %d\n", 
			USEC_TO_SEC(CONFIG.tcp_timewait * TIME_TICK));
	TRACE_CONFIG("NICs to print statistics:");
	for (i = 0; i < CONFIG.eths_num; i++) {
		if (CONFIG.eths[i].stat_print) {
			TRACE_CONFIG(" %s", CONFIG.eths[i].dev_name);
		}
	}
	TRACE_CONFIG("\n");
	TRACE_CONFIG("----------------------------------------------------------"
			"-----------------------\n");
}
/*----------------------------------------------------------------------------*/
