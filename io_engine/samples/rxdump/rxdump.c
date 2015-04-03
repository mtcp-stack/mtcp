#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <netinet/in.h>

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>

#include "../../include/ps.h"

void dump_packet(char *buf, int len)
{
	struct ethhdr *ethh;
	struct iphdr *iph;
	struct udphdr *udph;
	struct tcphdr *tcph;
	uint8_t *t;

	ethh = (struct ethhdr *)buf;
	printf("%02X:%02X:%02X:%02X:%02X:%02X -> %02X:%02X:%02X:%02X:%02X:%02X ",
			ethh->h_source[0],
			ethh->h_source[1],
			ethh->h_source[2],
			ethh->h_source[3],
			ethh->h_source[4],
			ethh->h_source[5],
			ethh->h_dest[0],
			ethh->h_dest[1],
			ethh->h_dest[2],
			ethh->h_dest[3],
			ethh->h_dest[4],
			ethh->h_dest[5]);

	if (ntohs(ethh->h_proto) != ETH_P_IP) {
		printf("protocol %04hx  ", ntohs(ethh->h_proto));
		goto done;
	}

	printf(" ");

	iph = (struct iphdr *)(ethh + 1);
	udph = (struct udphdr *)((uint32_t *)iph + iph->ihl);
	tcph = (struct tcphdr *)((uint32_t *)iph + iph->ihl);

	t = (uint8_t *)&iph->saddr;
	printf("%u.%u.%u.%u", t[0], t[1], t[2], t[3]);
	if (iph->protocol == IPPROTO_TCP || iph->protocol == IPPROTO_UDP)
		printf("(%d)", ntohs(udph->source));

	printf(" -> ");

	t = (uint8_t *)&iph->daddr;
	printf("%u.%u.%u.%u", t[0], t[1], t[2], t[3]);
	if (iph->protocol == IPPROTO_TCP || iph->protocol == IPPROTO_UDP)
		printf("(%d)", ntohs(udph->dest));

	printf(" TTL=%d ", iph->ttl);

	if (ip_fast_csum(iph, iph->ihl)) {
		__sum16 org_csum, correct_csum;
		
		org_csum = iph->check;
		iph->check = 0;
		correct_csum = ip_fast_csum(iph, iph->ihl);
		printf("(bad checksum %04x should be %04x) ",
				ntohs(org_csum), ntohs(correct_csum));
		iph->check = org_csum;
	}

	switch (iph->protocol) {
	case IPPROTO_TCP:
		printf("TCP ");
		if (tcph->syn)
			printf("S ");
		if (tcph->fin)
			printf("F ");
		if (tcph->ack)
			printf("A ");
		if (tcph->rst)
			printf("R ");

		printf("seq %u ", ntohl(tcph->seq));
		if (tcph->ack)
			printf("ack %u ", ntohl(tcph->ack_seq));
		break;
	case IPPROTO_UDP:
		printf("UDP ");
		break;
	default:
		printf("protocol %d ", iph->protocol);
		goto done;
	}

done:
	printf("len=%d\n", len);
}

int num_devices;
struct ps_device devices[MAX_DEVICES];

struct ps_handle handle;
int num_devices_attached;
int devices_attached[MAX_DEVICES];

void print_usage(char *argv0)
{
	fprintf(stderr, "Usage: %s <interface to sniff> <...>",
			argv0);

	exit(2);
}

void parse_opt(int argc, char **argv)
{
	int i, j;

	if (argc < 2)
		print_usage(argv[0]);

	for (i = 1; i < argc; i++) {
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

void attach()
{
	int ret;
	int i, j;

	ret = ps_init_handle(&handle);
	if (ret != 0) {
		perror("ps_init_handle");
		exit(1);
	}

	for (i = 0; i < num_devices_attached; i++) {
		struct ps_queue queue;

		queue.ifindex = devices_attached[i];

		for (j = 0; j < devices[devices_attached[i]].num_rx_queues; j++) {
			queue.qidx = j;

			ret = ps_attach_rx_device(&handle, &queue);
			if (ret != 0) {
				perror("ps_attach_rx_device");
				exit(1);
			}
		}
	}
}

void dump()
{
	int ret;
	struct ps_chunk chunk;

	ret = ps_alloc_chunk(&handle, &chunk);
	if (ret != 0) {
		perror("ps_alloc_chunk");
		exit(1);
	}

	chunk.cnt = 1; /* no batching */
	chunk.recv_blocking = 1;

	for (;;) {
		int ret = ps_recv_chunk(&handle, &chunk);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			if (!chunk.recv_blocking && errno == EWOULDBLOCK)
				break;

			assert(0);
		}

		if (ret > 0) {
			struct ps_packet packet;

			printf("%s:%d ", 
					devices[chunk.queue.ifindex].name,
					chunk.queue.qidx);
			dump_packet(chunk.buf + chunk.info[0].offset, chunk.info[0].len);

			packet.ifindex = chunk.queue.ifindex;
			packet.len = chunk.info[0].len;
			packet.buf = chunk.buf + chunk.info[0].offset;

			assert(ps_slowpath_packet(&handle, &packet) == 0);
		}

		chunk.cnt = 1;
	}
}

int main(int argc, char **argv)
{
	num_devices = ps_list_devices(devices);
	if (num_devices == -1) {
		perror("ps_list_devices");
		exit(1);
	}

	parse_opt(argc, argv);
	attach();
	dump();

	return 0;
}
