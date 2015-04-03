#define _GNU_SOURCE

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <numa.h>

#include "../../include/ps.h"

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>

#define MAX_CPUS	32

#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))


#define IPPROTO_UDP		17
#define HTONS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#define NTOHS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))

#define HTONL(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
	((((unsigned long)(n) & 0xFF00)) << 8) | \
	((((unsigned long)(n) & 0xFF0000)) >> 8) | \
		  ((((unsigned long)(n) & 0xFF000000)) >> 24))

#define NTOHL(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
	((((unsigned long)(n) & 0xFF00)) << 8) | \
	((((unsigned long)(n) & 0xFF0000)) >> 8) | \
		  ((((unsigned long)(n) & 0xFF000000)) >> 24))


uint32_t magic_number;
int payload_offset;

int num_cpus;
int my_cpu;
int num_packets;
int ip_version;
int time_limit;

int num_devices;
struct ps_device devices[MAX_DEVICES];

int num_devices_registered;
int devices_registered[MAX_DEVICES];

struct ps_handle handles[MAX_CPUS];

void done()
{
	struct ps_handle *handle = &handles[my_cpu];

	uint64_t total_tx_packets = 0;

	int i;
	int ifindex;

	usleep(10000 * (my_cpu + 1));

	for (i = 0; i < num_devices_registered; i++) {
		ifindex = devices_registered[i];
		total_tx_packets += handle->tx_packets[ifindex];
	}

	printf("----------\n");
	printf("CPU %d: total %ld packets transmitted\n", 
			my_cpu, total_tx_packets);
	
	for (i = 0; i < num_devices_registered; i++) {
		char *dev = devices[devices_registered[i]].name;
		ifindex = devices_registered[i];

		if (handle->tx_packets[ifindex] == 0)
			continue;

		printf("  %s: %ld packets "
				"(%ld chunks, %.2f packets per chunk)\n", 
				dev, 
				handle->tx_packets[ifindex],
				handle->tx_chunks[ifindex],
				handle->tx_packets[ifindex] / 
				  (double)handle->tx_chunks[ifindex]);
	}

	exit(0);
}

void handle_signal(int signal)
{
	done();
}

int get_num_cpus()
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int bind_cpu(int cpu)
{
        cpu_set_t *cmask;
	struct bitmask *bmask;
	size_t n;
	int ret;

	n = get_num_cpus();

        if (cpu < 0 || cpu >= (int)n) {
		errno = -EINVAL;
		return -1;
	}

	cmask = CPU_ALLOC(n);
	if (cmask == NULL)
		return -1;

        CPU_ZERO_S(n, cmask);
        CPU_SET_S(cpu, n, cmask);

        ret = sched_setaffinity(0, n, cmask);

	CPU_FREE(cmask);

	/* skip NUMA stuff for UMA systems */
	if (numa_max_node() == 0)
		return ret;

	bmask = numa_bitmask_alloc(16);
	assert(bmask);

	numa_bitmask_setbit(bmask, cpu % 2);
	numa_set_membind(bmask);
	numa_bitmask_free(bmask);

	return ret;
}

void update_stats(struct ps_handle *handle)
{
	static int total_sec = 0;
	static long counter;
	static time_t last_sec = 0;
	static int first = 1;

	static long last_total_tx_packets = 0;
	static long last_total_tx_bytes= 0;
	static long last_device_tx_packets[MAX_DEVICES];
	static long last_device_tx_bytes[MAX_DEVICES];

	long total_tx_packets = 0;
	long total_tx_bytes = 0;

	struct timeval tv;
	int sec_diff;

	int i;

	if (++counter % 100 != 0) 
		return;

	assert(gettimeofday(&tv, NULL) == 0);

	if (tv.tv_sec <= last_sec)
		return;

	sec_diff = tv.tv_sec - last_sec;

	for (i = 0; i < num_devices_registered; i++) {
		int ifindex = devices_registered[i];
		total_tx_packets += handle->tx_packets[ifindex];
		total_tx_bytes += handle->tx_bytes[ifindex];
	}

	if (!first) {
		long pps = total_tx_packets - last_total_tx_packets;
		long bps = (total_tx_bytes - last_total_tx_bytes) * 8;

		pps /= sec_diff;
		bps /= sec_diff;

		printf("CPU %d: %8ld pps, %6.3f Gbps "
				"(%.2f packets per chunk)",
				my_cpu, 
				pps,
				(bps + (pps * 24) * 8) / 1000000000.0,
				total_tx_packets / (double)counter);

		for (i = 0; i < num_devices_registered; i++) {
			char *dev;
			int ifindex;

			dev = devices[devices_registered[i]].name;
			ifindex = devices_registered[i];

			pps = handle->tx_packets[ifindex] -
					last_device_tx_packets[ifindex];
			bps = (handle->tx_bytes[ifindex] -
					last_device_tx_bytes[ifindex]) * 8;

			printf("  %s:%8ld pps,%6.3f Gbps", 
					dev,
					pps,
					(bps + (pps * 24) * 8) / 1000000000.0);
		}

		printf("\n");
		fflush(stdout);

		total_sec++;
		if (total_sec == time_limit)
			done();
	}

	if (sec_diff == 1)
		first = 0;

	last_sec = tv.tv_sec;
	last_total_tx_packets = total_tx_packets;
	last_total_tx_bytes = total_tx_bytes;

	for (i = 0; i < MAX_DEVICES; i++) {
		last_device_tx_packets[i] = handle->tx_packets[i];
		last_device_tx_bytes[i] = handle->tx_bytes[i];
	}
}

static inline uint32_t myrand(uint64_t *seed) 
{
	*seed = *seed * 1103515245 + 12345;
	return (uint32_t)(*seed >> 32);
}

void build_packet(char *buf, int size, uint64_t *seed)
{
	struct ethhdr *eth;
	struct iphdr *ip;
	struct udphdr *udp;

	uint32_t rand_val;

	//memset(buf, 0, size);

	/* build an ethernet header */
	eth = (struct ethhdr *)buf;

	eth->h_dest[0] = 0x00;
	eth->h_dest[1] = 0x00;
	eth->h_dest[2] = 0x00;
	eth->h_dest[3] = 0x00;
	eth->h_dest[4] = 0x00;
	eth->h_dest[5] = 0x02;

	eth->h_source[0] = 0x00;
	eth->h_source[1] = 0x00;
	eth->h_source[2] = 0x00;
	eth->h_source[3] = 0x00;
	eth->h_source[4] = 0x00;
	eth->h_source[5] = 0x01;

	eth->h_proto = HTONS(0x0800);

	/* build an IP header */
	ip = (struct iphdr *)(buf + sizeof(*eth));

	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = HTONS(size - sizeof(*eth));
	ip->id = 0;
	ip->frag_off = 0;
	ip->ttl = 32;
	ip->protocol = IPPROTO_UDP;
	ip->saddr = HTONL(0x0A000001);
	ip->daddr = HTONL(myrand(seed));
	ip->check = 0;
	ip->check = ip_fast_csum(ip, ip->ihl);

	udp = (struct udphdr *)((char *)ip + sizeof(*ip));

	rand_val = myrand(seed);
	udp->source = HTONS(rand_val & 0xFFFF);
	udp->dest = HTONS((rand_val >> 16) & 0xFFFF);

	udp->len = HTONS(size - sizeof(*eth) - sizeof(*ip));
	udp->check = 0;
}

void build_packet_v6(char *buf, int size, uint64_t *seed)
{
	struct ethhdr *eth;
	struct ipv6hdr *ip;
	struct udphdr *udp;

	uint32_t rand_val;

	//memset(buf, 0, size);

	/* build an ethernet header */
	eth = (struct ethhdr *)buf;

	eth->h_dest[0] = 0x00;
	eth->h_dest[1] = 0x00;
	eth->h_dest[2] = 0x00;
	eth->h_dest[3] = 0x00;
	eth->h_dest[4] = 0x00;
	eth->h_dest[5] = 0x02;

	eth->h_source[0] = 0x00;
	eth->h_source[1] = 0x00;
	eth->h_source[2] = 0x00;
	eth->h_source[3] = 0x00;
	eth->h_source[4] = 0x00;
	eth->h_source[5] = 0x01;

	eth->h_proto = HTONS(0x86DD);

	/* build an IP header */
	ip = (struct ipv6hdr *)(buf + sizeof(*eth));

	ip->version = 6;
	ip->payload_len = HTONS(size - sizeof(*eth) - sizeof(*ip));
	ip->hop_limit = 32;
	ip->nexthdr = IPPROTO_UDP;
	ip->saddr.s6_addr32[0] = HTONL(0x0A000001);
	ip->saddr.s6_addr32[1] = HTONL(0x00000000);
	ip->saddr.s6_addr32[2] = HTONL(0x00000000);
	ip->saddr.s6_addr32[3] = HTONL(0x00000000);
	ip->daddr.s6_addr32[0] = HTONL(myrand(seed));
	ip->daddr.s6_addr32[1] = HTONL(myrand(seed));
	ip->daddr.s6_addr32[2] = HTONL(myrand(seed));
	ip->daddr.s6_addr32[3] = HTONL(myrand(seed));


	udp = (struct udphdr *)((char *)ip + sizeof(*ip));

	rand_val = myrand(seed);
	udp->source = HTONS(rand_val & 0xFFFF);
	udp->dest = HTONS((rand_val >> 16) & 0xFFFF);

	udp->len = HTONS(size - sizeof(*eth) - sizeof(*ip));
	udp->check = 0;
}

#define MAX_FLOWS 1024

void send_packets(long packets, 
		int chunk_size,
		int packet_size,
		int num_flows)
{
	struct ps_handle *handle = &handles[my_cpu];
	struct ps_chunk chunk;
	char packet[MAX_FLOWS][MAX_PACKET_SIZE];
	int ret;

	int i, j;
	unsigned int next_flow[MAX_DEVICES];

	long sent = 0;
	uint64_t seed = 0;

	if (num_flows == 0)
		seed = time(NULL) + my_cpu;

	for (i = 0; i < num_flows; i++) {
		if (ip_version == 4) 
			build_packet(packet[i], packet_size, &seed);
		else if (ip_version == 6)
			build_packet_v6(packet[i], packet_size, &seed);
	}

	assert(ps_init_handle(handle) == 0);

	for (i = 0; i < num_devices_registered; i++)
		next_flow[i] = 0;

	assert(ps_alloc_chunk(handle, &chunk) == 0);
	chunk.queue.qidx = my_cpu; /* CPU_i holds queue_i */

	assert(chunk.info);

	while (1) {
		int working = 0;

		for (i = 0; i < num_devices_registered; i++) {
			chunk.queue.ifindex = devices_registered[i];
			working = 1;

			for (j = 0; j < chunk_size; j++) {
				chunk.info[j].len = packet_size;
				chunk.info[j].offset = j * ALIGN(packet_size, 64);

				if (num_flows == 0){
					if (ip_version == 4){
						build_packet(chunk.buf + chunk.info[j].offset,
							     packet_size, &seed);
					} else {
						build_packet_v6(chunk.buf + chunk.info[j].offset,
							packet_size, &seed);
					}

				}
				else
					memcpy_aligned(chunk.buf + chunk.info[j].offset, 
						packet[(next_flow[i] + j) % num_flows], 
						packet_size);
			}

			if (packets - sent < chunk_size)
				chunk.cnt = packets - sent;
			else
				chunk.cnt = chunk_size;

			ret = ps_send_chunk(handle, &chunk);
			assert(ret >= 0);
			usleep(25);

			update_stats(handle);
			sent += ret;

			if (packets <= sent)
				done();

			if (num_flows)
				next_flow[i] = (next_flow[i] + ret) % num_flows;
		}

		if (!working)
			break;
	}

	ps_close_handle(handle);
}

void print_usage(char *program)
{
	fprintf(stderr, "usage: %s "
			"[-n <num_packets>] "
			"[-s <chunk_size>] "
			"[-p <packet_size>] "
			"[-f <num_flows>] "
			"[-v <ip version>] "
			"[-c <loop count>] "
			"[-t <seconds>] "
			"-i all|dev1 [-i dev2] ...\n",
			program);

	fprintf(stderr, "  default <num_packets> is 0 (infinite)\n");
	fprintf(stderr, "    (note: <num_packets> is a per-cpu value)\n");
	fprintf(stderr, "  default <chunk_size> is 64 packets per chunk\n");
	fprintf(stderr, "  default <packet_size> is 60 (w/o 4-byte CRC)\n");
	fprintf(stderr, "  default <num_flows> is 0 (0 = infinite)\n");
	fprintf(stderr, "  default <ip version> is 4 (6 = ipv6)\n");
	fprintf(stderr, "  default <loop count> is 1 (only valid for latency mesaurement)\n");
	fprintf(stderr, "  default <seconds> is 0 (0 = infinite)\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int num_packets = 0;
	int chunk_size = 64;
	int packet_size = 60;
	int num_flows = 0;
	
	int i;

	ip_version = 4;

	struct timeval begin, end;

	num_cpus = get_num_cpus();
	assert(num_cpus >= 1);

	num_devices = ps_list_devices(devices);
	assert(num_devices != -1);
	assert(num_devices > 0);

	for (i = 1; i < argc; i += 2) {
		if (i == argc - 1)
			print_usage(argv[0]);

		if (!strcmp(argv[i], "-n")) {
			num_packets = atoi(argv[i + 1]);
			assert(num_packets >= 0);
		} else if (!strcmp(argv[i], "-s")) {
			chunk_size = atoi(argv[i + 1]);
			assert(chunk_size >= 1 && chunk_size <= MAX_CHUNK_SIZE);
		} else if (!strcmp(argv[i], "-p")) {
			packet_size = atoi(argv[i + 1]);
			assert(packet_size >= 60 && packet_size <= 1514);
		} else if (!strcmp(argv[i], "-f")) {
			num_flows = atoi(argv[i + 1]);
			assert(num_flows >= 0 && num_flows <= MAX_FLOWS);
		} else if (!strcmp(argv[i], "-v")) {
			ip_version = atoi(argv[i + 1]);
			assert(ip_version == 4 || ip_version == 6);
		} else if (!strcmp(argv[i], "-i")) {
			int ifindex = -1;
			int j;

			if (!strcmp(argv[i + 1], "all")) {
				for (j = 0; j < num_devices; j++)
					devices_registered[j] = j;
				num_devices_registered = num_devices;
				continue;
			}

			for (j = 0; j < num_devices; j++)
				if (!strcmp(argv[i + 1], devices[j].name))
					ifindex = j;

			if (ifindex == -1) {
				fprintf(stderr, "device %s does not exist!\n", 
						argv[i + 1]);
				exit(1);
			}

			for (j = 0; j < num_devices_registered; j++)
				if (devices_registered[j] == ifindex) {
					fprintf(stderr, "device %s is registered more than once!\n",
							argv[i + 1]);
					exit(1);
				}

			devices_registered[num_devices_registered] = ifindex;
			num_devices_registered++;
		} else if (!strcmp(argv[i], "-t")) {
			time_limit = atoi(argv[i + 1]);
			assert(time_limit >= 0);
		} else
			print_usage(argv[0]);
	}

	if (num_devices_registered == 0)
		print_usage(argv[0]);

	printf("# of CPUs = %d\n", num_cpus);
	printf("# of packets to transmit = %d\n", num_packets);
	printf("chunk size = %d\n", chunk_size);
	printf("packet size = %d bytes\n", packet_size);
	printf("# of flows = %d\n", num_flows);
	printf("ip version = %d\n", ip_version);
	printf("time limit = %d seconds\n", time_limit);

	printf("interfaces: ");
	for (i = 0; i < num_devices_registered; i++) {
		if (i > 0)
			printf(", ");
		printf("%s", devices[devices_registered[i]].name);
	}
	printf("\n");
	
	printf("----------\n");
		
	if (num_flows > 0)
		srand(time(NULL));

	assert(gettimeofday(&begin, NULL) == 0);

	for (my_cpu = 0; my_cpu < num_cpus; my_cpu++) {
		int ret = fork();
		assert(ret >= 0);

		if (ret == 0) {
			bind_cpu(my_cpu);
			signal(SIGINT, handle_signal);

			send_packets(num_packets ? : LONG_MAX, chunk_size,
					packet_size,
					num_flows);
			return 0;
		}
	}

	signal(SIGINT, SIG_IGN);

	while (1) {
		int ret = wait(NULL);
		if (ret == -1 && errno == ECHILD)
			break;
	}

	assert(gettimeofday(&end, NULL) == 0);

	printf("----------\n");
	printf("%.2f seconds elapsed\n", 
			((end.tv_sec - begin.tv_sec) * 1000000 +
			 (end.tv_usec - begin.tv_usec))
			/ 1000000.0);

	return 0;
}
