#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <sys/wait.h>
#include <numa.h>

#include "../../include/ps.h"

#define MAX_CPUS 32
#define ALARM_TIMER 1
#define TIMEVAL_TO_TS(t)	(uint32_t)((t)->tv_sec * HZ + \
							((t)->tv_usec / TIME_TICK))


int num_devices;

int num_devices_attached;
int devices_attached[MAX_DEVICES];

static int num_cpus;
struct ps_device devices[MAX_DEVICES];
struct ps_handle handles[MAX_CPUS];

int my_cpu;
int sink;
uint64_t variance;

int too_much, too_less;

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

	numa_bitmask_setbit(bmask, 0);
	numa_set_membind(bmask);
	numa_bitmask_free(bmask);

	return ret;
}

void print_usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [-s] <interface to echo> <...>\n",
			argv0);
	fprintf(stderr, "  -s option makes this program work as a sink\n");

	exit(2);
}

void parse_opt(int argc, char **argv)
{
	int i, j;

	if (argc < 2)
		print_usage(argv[0]);

	if (strcmp(argv[1], "-s") == 0) {
		sink = 1;
		printf("just dropping incoming packets...\n");
	}

	for (i = 1 + sink; i < argc; i++) {
		int ifindex = -1;

		for (j = 0; j < num_devices; j++) {
			if (strcmp(argv[i], devices[j].name) != 0)
				continue;

			ifindex = devices[j].ifindex;
			break;
		}

		if (ifindex == -1) {
			fprintf(stderr, "Interface %s does not exist!\n", argv[i]);
			exit(4);
		}

		for (j = 0; j < num_devices_attached; j++) {
			if (devices_attached[j] == ifindex)
				goto already_attached;
		}

		devices_attached[num_devices_attached] = ifindex;
		num_devices_attached++;

already_attached:
		;
	}

	assert(num_devices_attached > 0);
}

void handle_signal(int signal)
{
	struct ps_handle *handle = &handles[my_cpu];

	uint64_t total_rx_packets = 0;
	uint64_t total_tx_packets = 0;

	int i;
	int ifindex;

	usleep(10000 * (my_cpu + 1));

	for (i = 0; i < num_devices_attached; i++) {
		ifindex = devices_attached[i];
		total_tx_packets += handle->tx_packets[ifindex];
		total_rx_packets += handle->rx_packets[ifindex];
	}

	printf("----------\n");
	printf("CPU %d: %ld packets received, %ld packets transmitted\n", 
			my_cpu, total_rx_packets, total_tx_packets);
	
	for (i = 0; i < num_devices_attached; i++) {
		char *dev = devices[devices_attached[i]].name;
		ifindex = devices_attached[i];

		if (handle->tx_packets[ifindex] == 0 && handle->rx_packets[ifindex] == 0)
			continue;

		printf("  %s: ", dev);
		
		printf("RX %ld packets "
				"(%ld chunks, %.2f packets per chunk) v %.2f ", 
				handle->rx_packets[ifindex],
				handle->rx_chunks[ifindex],
				handle->rx_packets[ifindex] / 
				  (double)handle->rx_chunks[ifindex],
				variance / (double)handle->rx_chunks[ifindex]);

		printf("TX %ld packets "
				"(%ld chunks, %.2f packets per chunk)\n", 
				handle->tx_packets[ifindex],
				handle->tx_chunks[ifindex],
				handle->tx_packets[ifindex] / 
				  (double)handle->tx_chunks[ifindex]);
		printf("too_much %d\n, too_less %d\n", too_much, too_less);
	}

	exit(0);
}
void handle_alarm(int signal) 
{
	int i;

	static uint64_t total_rx_bytes_until_last[MAX_DEVICES] = {0};
	static uint64_t total_rx_packets_until_last[MAX_DEVICES] = {0};
	static uint64_t total_tx_bytes_until_last[MAX_DEVICES] = {0};
	static uint64_t total_tx_packets_until_last[MAX_DEVICES] = {0};

	uint64_t rx_chunks[MAX_DEVICES] = {0};
	uint64_t tx_chunks[MAX_DEVICES] = {0};
			
	struct ps_handle *handle = &handles[my_cpu];

	for (i = 0; i < num_devices_attached; i++) {
		int ifindex = devices_attached[i];
		
		rx_chunks[ifindex] = handle->rx_chunks[ifindex];
		tx_chunks[ifindex] = handle->tx_chunks[ifindex];

		if (handle->rx_packets[ifindex] == total_rx_packets_until_last[ifindex] &&
				 handle->tx_packets[ifindex] == total_tx_packets_until_last[ifindex])
			break;


		double rx_Gbps = ((handle->rx_bytes[ifindex] - total_rx_bytes_until_last[ifindex]) +
							(handle->rx_packets[ifindex] - total_rx_packets_until_last[ifindex]) * 24)
							/ (double) ALARM_TIMER 
							/ (1000 * 1000 * 1000) * 8;
		
		double tx_Gbps = ((handle->tx_bytes[ifindex] - total_tx_bytes_until_last[ifindex]) +
							(handle->tx_packets[ifindex] - total_tx_packets_until_last[ifindex]) * 24)
							/ (double) ALARM_TIMER 
							/ (1000 * 1000 * 1000) * 8;

		printf("%s:%d RX %ld packets (%ld chunks, %.2f batch), %.2f Gbps\t", 
				devices[ifindex].name, my_cpu,
				handle->rx_packets[ifindex] - total_rx_packets_until_last[ifindex],
				rx_chunks[ifindex], 
				rx_chunks[ifindex] ?
				handle->rx_packets[ifindex] / (double) rx_chunks[ifindex] : 0, 
				rx_Gbps);
		printf("TX %ld packets (%ld chunks, %.2f batch), %.2f Gbps\n", 
				handle->tx_packets[ifindex] - total_tx_packets_until_last[ifindex],
				tx_chunks[ifindex],
				tx_chunks[ifindex]?
				handle->tx_packets[ifindex] / (double) tx_chunks[ifindex] : 0, 
				tx_Gbps);

		total_rx_bytes_until_last[ifindex] = handle->rx_bytes[ifindex];
		total_rx_packets_until_last[ifindex] = handle->rx_packets[ifindex];
		total_tx_bytes_until_last[ifindex] = handle->tx_bytes[ifindex];
		total_tx_packets_until_last[ifindex] = handle->tx_packets[ifindex];
	}
	
	alarm(ALARM_TIMER);
		
}
void echo()
{
	struct ps_handle *handle = &handles[my_cpu];
	struct ps_chunk chunk;

	int i;
	int working = 0;
	int ret = 0;

	assert(ps_init_handle(handle) == 0);

	for (i = 0; i < num_devices_attached; i++) {
		struct ps_queue queue;
		if (devices[devices_attached[i]].num_rx_queues <= my_cpu)
			continue;

		if (devices[devices_attached[i]].num_tx_queues <= my_cpu) {
			printf("WARNING: xge%d has not enough TX queues!\n",
					devices_attached[i]);
			continue;
		}

		working = 1;
		queue.ifindex = devices_attached[i];
		queue.qidx = my_cpu;

		printf("attaching RX queue xge%d:%d to CPU%d\n", queue.ifindex, queue.qidx, my_cpu);
		assert(ps_attach_rx_device(handle, &queue) == 0);
	}

	if (!working)
		goto done;

	assert(ps_alloc_chunk(handle, &chunk) == 0);

	chunk.recv_blocking = 1;

	for (;;) {
	
		chunk.cnt = 64;
		ret = ps_recv_chunk(handle, &chunk);
		if (!sink && ret > 0) {
			chunk.cnt = ret;
			ret = ps_send_chunk(handle, &chunk);
		}
	}

done:
	
	ps_close_handle(handle);
}

int main(int argc, char **argv)
{
	int i ;

	num_cpus = get_num_cpus();
	assert(num_cpus >= 1);

	num_devices = ps_list_devices(devices);
	if (num_devices == -1) {
		perror("ps_list_devices");
		exit(1);
	}

	parse_opt(argc, argv);

	for (i = 0; i < num_cpus; i++) {
		int ret = fork();
		assert(ret >= 0);

		my_cpu = i;

		if (ret == 0) {
			bind_cpu(i);
			signal(SIGINT, handle_signal);
			signal(SIGALRM, handle_alarm);
			alarm(ALARM_TIMER);

			echo();
			return 0;
		}
	}

	signal(SIGINT, SIG_IGN);

	while (1) {
		int ret = wait(NULL);
		if (ret == -1 && errno == ECHILD)
			break;
	}

	return 0;
}
