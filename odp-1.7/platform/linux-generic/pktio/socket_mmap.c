/* Copyright (c) 2013, Linaro Limited
 * Copyright (c) 2013, Nokia Solutions and Networks
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_posix_extensions.h>

#include <odp_packet_io_internal.h>

#include <sys/socket.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bits/wordsize.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <odp.h>
#include <odp_packet_socket.h>
#include <odp_packet_internal.h>
#include <odp_packet_io_internal.h>
#include <odp_debug_internal.h>
#include <odp_classification_datamodel.h>
#include <odp_classification_inlines.h>
#include <odp_classification_internal.h>
#include <odp/hints.h>

#include <odp/helper/eth.h>
#include <odp/helper/ip.h>

static int set_pkt_sock_fanout_mmap(pkt_sock_mmap_t *const pkt_sock,
				    int sock_group_idx)
{
	int sockfd = pkt_sock->sockfd;
	int val;
	int err;
	uint16_t fanout_group;

	fanout_group = (uint16_t)(sock_group_idx & 0xffff);
	val = (PACKET_FANOUT_HASH << 16) | fanout_group;

	err = setsockopt(sockfd, SOL_PACKET, PACKET_FANOUT, &val, sizeof(val));
	if (err != 0) {
		__odp_errno = errno;
		ODP_ERR("setsockopt(PACKET_FANOUT): %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

union frame_map {
	struct {
		struct tpacket2_hdr tp_h ODP_ALIGNED(TPACKET_ALIGNMENT);
		struct sockaddr_ll s_ll
		ODP_ALIGNED(TPACKET_ALIGN(sizeof(struct tpacket2_hdr)));
	} *v2;

	void *raw;
};

static int mmap_pkt_socket(void)
{
	int ver = TPACKET_V2;

	int ret, sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

	if (sock == -1) {
		__odp_errno = errno;
		ODP_ERR("socket(SOCK_RAW): %s\n", strerror(errno));
		return -1;
	}

	ret = setsockopt(sock, SOL_PACKET, PACKET_VERSION, &ver, sizeof(ver));
	if (ret == -1) {
		__odp_errno = errno;
		ODP_ERR("setsockopt(PACKET_VERSION): %s\n", strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

static inline int mmap_rx_kernel_ready(struct tpacket2_hdr *hdr)
{
	return ((hdr->tp_status & TP_STATUS_USER) == TP_STATUS_USER);
}

static inline void mmap_rx_user_ready(struct tpacket2_hdr *hdr)
{
	hdr->tp_status = TP_STATUS_KERNEL;
	__sync_synchronize();
}

static inline int mmap_tx_kernel_ready(struct tpacket2_hdr *hdr)
{
	return !(hdr->tp_status & (TP_STATUS_SEND_REQUEST | TP_STATUS_SENDING));
}

static inline void mmap_tx_user_ready(struct tpacket2_hdr *hdr)
{
	hdr->tp_status = TP_STATUS_SEND_REQUEST;
	__sync_synchronize();
}

static inline unsigned pkt_mmap_v2_rx(pktio_entry_t *pktio_entry,
				      pkt_sock_mmap_t *pkt_sock,
				      odp_packet_t pkt_table[], unsigned len,
				      unsigned char if_mac[])
{
	union frame_map ppd;
	unsigned frame_num, next_frame_num;
	uint8_t *pkt_buf;
	int pkt_len;
	struct ethhdr *eth_hdr;
	unsigned i = 0;
	uint8_t nb_rx = 0;
	int id = 0;
	struct ring *ring;
	int ret;

	ring  = &pkt_sock->rx_ring;
	frame_num = ring->frame_num;

	while (i < len) {
		if (!mmap_rx_kernel_ready(ring->rd[frame_num].iov_base))
			break;

		ppd.raw = ring->rd[frame_num].iov_base;
		next_frame_num = (frame_num + 1) % ring->rd_num;

		pkt_buf = (uint8_t *)ppd.raw + ppd.v2->tp_h.tp_mac;
		pkt_len = ppd.v2->tp_h.tp_snaplen;

		/* Don't receive packets sent by ourselves */
		eth_hdr = (struct ethhdr *)pkt_buf;
		if (odp_unlikely(ethaddrs_equal(if_mac,
						eth_hdr->h_source))) {
			mmap_rx_user_ready(ppd.raw); /* drop */
			frame_num = next_frame_num;
			continue;
		}

		if (pktio_cls_enabled(pktio_entry, id)) {
			ret = _odp_packet_cls_enq(pktio_entry, pkt_buf,
						  pkt_len, &pkt_table[nb_rx]);
			if (ret)
				nb_rx++;
		} else {
			odp_packet_hdr_t *hdr;

			pkt_table[i] = packet_alloc(pkt_sock->pool, pkt_len, 1);
			if (odp_unlikely(pkt_table[i] == ODP_PACKET_INVALID)) {
				mmap_rx_user_ready(ppd.raw); /* drop */
				frame_num = next_frame_num;
				continue;
			}
			hdr = odp_packet_hdr(pkt_table[i]);
			ret = odp_packet_copydata_in(pkt_table[i], 0,
						     pkt_len, pkt_buf);
			if (ret != 0) {
				odp_packet_free(pkt_table[i]);
				mmap_rx_user_ready(ppd.raw); /* drop */
				frame_num = next_frame_num;
				continue;
			}

			packet_parse_l2(hdr);
			nb_rx++;
		}

		mmap_rx_user_ready(ppd.raw);
		frame_num = next_frame_num;
		i++;
	}

	ring->frame_num = frame_num;
	return nb_rx;
}

static inline unsigned pkt_mmap_v2_tx(int sock, struct ring *ring,
				      odp_packet_t pkt_table[], unsigned len)
{
	union frame_map ppd;
	uint32_t pkt_len;
	unsigned first_frame_num, frame_num, frame_count;
	int ret;
	uint8_t *buf;
	unsigned n, i = 0;
	unsigned nb_tx = 0;
	int send_errno;
	int total_len = 0;

	first_frame_num = ring->frame_num;
	frame_num = first_frame_num;
	frame_count = ring->rd_num;

	while (i < len) {
		ppd.raw = ring->rd[frame_num].iov_base;
		if (!odp_unlikely(mmap_tx_kernel_ready(ppd.raw)))
			break;

		pkt_len = odp_packet_len(pkt_table[i]);
		ppd.v2->tp_h.tp_snaplen = pkt_len;
		ppd.v2->tp_h.tp_len = pkt_len;
		total_len += pkt_len;

		buf = (uint8_t *)ppd.raw + TPACKET2_HDRLEN -
		       sizeof(struct sockaddr_ll);
		odp_packet_copydata_out(pkt_table[i], 0, pkt_len, buf);

		mmap_tx_user_ready(ppd.raw);

		if (++frame_num >= frame_count)
			frame_num = 0;

		i++;
	}

	ret = sendto(sock, NULL, 0, MSG_DONTWAIT, NULL, 0);
	send_errno = errno;

	/* On success, the return value indicates the number of bytes sent. On
	 * failure a value of -1 is returned, even if the failure occurred
	 * after some of the packets in the ring have already been sent, so we
	 * need to inspect the packet status to determine which were sent. */
	if (odp_likely(ret == total_len)) {
		nb_tx = i;
		ring->frame_num = frame_num;
	} else if (ret == -1) {
		for (frame_num = first_frame_num, n = 0; n < i; ++n) {
			struct tpacket2_hdr *hdr = ring->rd[frame_num].iov_base;

			if (odp_likely(hdr->tp_status == TP_STATUS_AVAILABLE ||
				       hdr->tp_status == TP_STATUS_SENDING)) {
				nb_tx++;
			} else {
				/* The remaining frames weren't sent, clear
				 * their status to indicate we're not waiting
				 * for the kernel to process them. */
				hdr->tp_status = TP_STATUS_AVAILABLE;
			}

			if (++frame_num >= frame_count)
				frame_num = 0;
		}

		ring->frame_num = (first_frame_num + nb_tx) % frame_count;

		if (nb_tx == 0 && SOCK_ERR_REPORT(send_errno)) {
			__odp_errno = send_errno;
			/* ENOBUFS indicates that the transmit queue is full,
			 * which will happen regularly when overloaded so don't
			 * print it */
			if (errno != ENOBUFS)
				ODP_ERR("sendto(pkt mmap): %s\n",
					strerror(send_errno));
			return -1;
		}
	} else {
		/* Short send, return value is number of bytes sent so use this
		 * to determine number of complete frames sent. */
		for (n = 0; n < i && ret > 0; ++n) {
			ret -= odp_packet_len(pkt_table[n]);
			if (ret >= 0)
				nb_tx++;
		}

		ring->frame_num = (first_frame_num + nb_tx) % frame_count;
	}

	for (i = 0; i < nb_tx; ++i)
		odp_packet_free(pkt_table[i]);

	return nb_tx;
}

static void mmap_fill_ring(struct ring *ring, odp_pool_t pool_hdl, int fanout)
{
	/*@todo add Huge Pages support*/
	int pz = getpagesize();
	uint32_t pool_id;
	pool_entry_t *pool_entry;

	if (pool_hdl == ODP_POOL_INVALID)
		ODP_ABORT("Invalid pool handle\n");

	pool_id = pool_handle_to_index(pool_hdl);
	pool_entry = get_pool_entry(pool_id);

	/* Frame has to capture full packet which can fit to the pool block.*/
	ring->req.tp_frame_size = (pool_entry->s.blk_size +
				   TPACKET_HDRLEN + TPACKET_ALIGNMENT +
				   + (pz - 1)) & (-pz);

	/* Calculate how many pages do we need to hold all pool packets
	*  and align size to page boundary.
	*/
	ring->req.tp_block_size = (ring->req.tp_frame_size *
				   pool_entry->s.buf_num + (pz - 1)) & (-pz);

	if (!fanout) {
		/* Single socket is in use. Use 1 block with buf_num frames. */
		ring->req.tp_block_nr = 1;
	} else {
		/* Fanout is in use, more likely taffic split accodring to
		 * number of cpu threads. Use cpu blocks and buf_num frames. */
		ring->req.tp_block_nr = odp_cpu_count();
	}

	ring->req.tp_frame_nr = ring->req.tp_block_size /
				ring->req.tp_frame_size * ring->req.tp_block_nr;

	ring->mm_len = ring->req.tp_block_size * ring->req.tp_block_nr;
	ring->rd_num = ring->req.tp_frame_nr;
	ring->flen = ring->req.tp_frame_size;
}

static int mmap_setup_ring(int sock, struct ring *ring, int type,
			   odp_pool_t pool_hdl, int fanout)
{
	int ret = 0;

	ring->sock = sock;
	ring->type = type;
	ring->version = TPACKET_V2;

	mmap_fill_ring(ring, pool_hdl, fanout);

	ret = setsockopt(sock, SOL_PACKET, type, &ring->req, sizeof(ring->req));
	if (ret == -1) {
		__odp_errno = errno;
		ODP_ERR("setsockopt(pkt mmap): %s\n", strerror(errno));
		return -1;
	}

	ring->rd_len = ring->rd_num * sizeof(*ring->rd);
	ring->rd = malloc(ring->rd_len);
	if (!ring->rd) {
		__odp_errno = errno;
		ODP_ERR("malloc(): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int mmap_sock(pkt_sock_mmap_t *pkt_sock)
{
	int i;
	int sock = pkt_sock->sockfd;

	/* map rx + tx buffer to userspace : they are in this order */
	pkt_sock->mmap_len =
		pkt_sock->rx_ring.req.tp_block_size *
		pkt_sock->rx_ring.req.tp_block_nr +
		pkt_sock->tx_ring.req.tp_block_size *
		pkt_sock->tx_ring.req.tp_block_nr;

	pkt_sock->mmap_base =
		mmap(NULL, pkt_sock->mmap_len, PROT_READ | PROT_WRITE,
		     MAP_SHARED | MAP_LOCKED | MAP_POPULATE, sock, 0);

	if (pkt_sock->mmap_base == MAP_FAILED) {
		__odp_errno = errno;
		ODP_ERR("mmap rx&tx buffer failed: %s\n", strerror(errno));
		return -1;
	}

	pkt_sock->rx_ring.mm_space = pkt_sock->mmap_base;
	memset(pkt_sock->rx_ring.rd, 0, pkt_sock->rx_ring.rd_len);
	for (i = 0; i < pkt_sock->rx_ring.rd_num; ++i) {
		pkt_sock->rx_ring.rd[i].iov_base =
			pkt_sock->rx_ring.mm_space
			+ (i * pkt_sock->rx_ring.flen);
		pkt_sock->rx_ring.rd[i].iov_len = pkt_sock->rx_ring.flen;
	}

	pkt_sock->tx_ring.mm_space =
		pkt_sock->mmap_base + pkt_sock->rx_ring.mm_len;
	memset(pkt_sock->tx_ring.rd, 0, pkt_sock->tx_ring.rd_len);
	for (i = 0; i < pkt_sock->tx_ring.rd_num; ++i) {
		pkt_sock->tx_ring.rd[i].iov_base =
			pkt_sock->tx_ring.mm_space
			+ (i * pkt_sock->tx_ring.flen);
		pkt_sock->tx_ring.rd[i].iov_len = pkt_sock->tx_ring.flen;
	}

	return 0;
}

static void mmap_unmap_sock(pkt_sock_mmap_t *pkt_sock)
{
	munmap(pkt_sock->mmap_base, pkt_sock->mmap_len);
	free(pkt_sock->rx_ring.rd);
	free(pkt_sock->tx_ring.rd);
}

static int mmap_bind_sock(pkt_sock_mmap_t *pkt_sock, const char *netdev)
{
	int ret;

	pkt_sock->ll.sll_family = PF_PACKET;
	pkt_sock->ll.sll_protocol = htons(ETH_P_ALL);
	pkt_sock->ll.sll_ifindex = if_nametoindex(netdev);
	pkt_sock->ll.sll_hatype = 0;
	pkt_sock->ll.sll_pkttype = 0;
	pkt_sock->ll.sll_halen = 0;

	ret = bind(pkt_sock->sockfd, (struct sockaddr *)&pkt_sock->ll,
		   sizeof(pkt_sock->ll));
	if (ret == -1) {
		__odp_errno = errno;
		ODP_ERR("bind(to IF): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int sock_mmap_close(pktio_entry_t *entry)
{
	pkt_sock_mmap_t *const pkt_sock = &entry->s.pkt_sock_mmap;

	mmap_unmap_sock(pkt_sock);
	if (pkt_sock->sockfd != -1 && close(pkt_sock->sockfd) != 0) {
		__odp_errno = errno;
		ODP_ERR("close(sockfd): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int sock_mmap_open(odp_pktio_t id ODP_UNUSED,
			  pktio_entry_t *pktio_entry,
			  const char *netdev, odp_pool_t pool)
{
	int if_idx;
	int ret = 0;
	odp_pktio_stats_t cur_stats;

	if (getenv("ODP_PKTIO_DISABLE_SOCKET_MMAP"))
		return -1;

	pkt_sock_mmap_t *const pkt_sock = &pktio_entry->s.pkt_sock_mmap;
	int fanout = 1;

	/* Init pktio entry */
	memset(pkt_sock, 0, sizeof(*pkt_sock));
	/* set sockfd to -1, because a valid socked might be initialized to 0 */
	pkt_sock->sockfd = -1;

	if (pool == ODP_POOL_INVALID)
		return -1;

	/* Store eth buffer offset for pkt buffers from this pool */
	pkt_sock->frame_offset = 0;

	pkt_sock->pool = pool;
	pkt_sock->sockfd = mmap_pkt_socket();
	if (pkt_sock->sockfd == -1)
		goto error;

	ret = mmap_bind_sock(pkt_sock, netdev);
	if (ret != 0)
		goto error;

	ret = mmap_setup_ring(pkt_sock->sockfd, &pkt_sock->tx_ring,
			      PACKET_TX_RING, pool, fanout);
	if (ret != 0)
		goto error;

	ret = mmap_setup_ring(pkt_sock->sockfd, &pkt_sock->rx_ring,
			      PACKET_RX_RING, pool, fanout);
	if (ret != 0)
		goto error;

	ret = mmap_sock(pkt_sock);
	if (ret != 0)
		goto error;

	ret = mac_addr_get_fd(pkt_sock->sockfd, netdev, pkt_sock->if_mac);
	if (ret != 0)
		goto error;

	if_idx = if_nametoindex(netdev);
	if (if_idx == 0) {
		__odp_errno = errno;
		ODP_ERR("if_nametoindex(): %s\n", strerror(errno));
		goto error;
	}

	pkt_sock->fanout = fanout;
	if (fanout) {
		ret = set_pkt_sock_fanout_mmap(pkt_sock, if_idx);
		if (ret != 0)
			goto error;
	}

	ret = ethtool_stats_get_fd(pktio_entry->s.pkt_sock_mmap.sockfd,
				   pktio_entry->s.name,
				   &cur_stats);
	if (ret != 0) {
		ret = sysfs_stats(pktio_entry, &cur_stats);
		if (ret != 0) {
			pktio_entry->s.stats_type = STATS_UNSUPPORTED;
			ODP_DBG("pktio: %s unsupported stats\n",
				pktio_entry->s.name);
		} else {
			pktio_entry->s.stats_type = STATS_SYSFS;
		}
	} else {
		pktio_entry->s.stats_type = STATS_ETHTOOL;
	}

	ret = sock_stats_reset_fd(pktio_entry,
				  pktio_entry->s.pkt_sock_mmap.sockfd);
	if (ret != 0)
		goto error;

	return 0;

error:
	sock_mmap_close(pktio_entry);
	return -1;
}

static int sock_mmap_recv(pktio_entry_t *pktio_entry,
			  odp_packet_t pkt_table[], unsigned len)
{
	pkt_sock_mmap_t *const pkt_sock = &pktio_entry->s.pkt_sock_mmap;

	return pkt_mmap_v2_rx(pktio_entry, pkt_sock,
			      pkt_table, len, pkt_sock->if_mac);
}

static int sock_mmap_send(pktio_entry_t *pktio_entry,
			  odp_packet_t pkt_table[], unsigned len)
{
	pkt_sock_mmap_t *const pkt_sock = &pktio_entry->s.pkt_sock_mmap;

	return pkt_mmap_v2_tx(pkt_sock->tx_ring.sock, &pkt_sock->tx_ring,
			      pkt_table, len);
}

static int sock_mmap_mtu_get(pktio_entry_t *pktio_entry)
{
	return mtu_get_fd(pktio_entry->s.pkt_sock_mmap.sockfd,
			  pktio_entry->s.name);
}

static int sock_mmap_mac_addr_get(pktio_entry_t *pktio_entry, void *mac_addr)
{
	memcpy(mac_addr, pktio_entry->s.pkt_sock_mmap.if_mac, ETH_ALEN);
	return ETH_ALEN;
}

static int sock_mmap_promisc_mode_set(pktio_entry_t *pktio_entry,
				      odp_bool_t enable)
{
	return promisc_mode_set_fd(pktio_entry->s.pkt_sock_mmap.sockfd,
				   pktio_entry->s.name, enable);
}

static int sock_mmap_promisc_mode_get(pktio_entry_t *pktio_entry)
{
	return promisc_mode_get_fd(pktio_entry->s.pkt_sock_mmap.sockfd,
				   pktio_entry->s.name);
}

static int sock_mmap_link_status(pktio_entry_t *pktio_entry)
{
	return link_status_fd(pktio_entry->s.pkt_sock_mmap.sockfd,
			      pktio_entry->s.name);
}

static int sock_mmap_stats(pktio_entry_t *pktio_entry,
			   odp_pktio_stats_t *stats)
{
	if (pktio_entry->s.stats_type == STATS_UNSUPPORTED) {
		memset(stats, 0, sizeof(*stats));
		return 0;
	}

	return sock_stats_fd(pktio_entry,
			     stats,
			     pktio_entry->s.pkt_sock_mmap.sockfd);
}

static int sock_mmap_stats_reset(pktio_entry_t *pktio_entry)
{
	if (pktio_entry->s.stats_type == STATS_UNSUPPORTED) {
		memset(&pktio_entry->s.stats, 0,
		       sizeof(odp_pktio_stats_t));
		return 0;
	}

	return sock_stats_reset_fd(pktio_entry,
				   pktio_entry->s.pkt_sock_mmap.sockfd);
}

const pktio_if_ops_t sock_mmap_pktio_ops = {
	.name = "socket_mmap",
	.init = NULL,
	.term = NULL,
	.open = sock_mmap_open,
	.close = sock_mmap_close,
	.start = NULL,
	.stop = NULL,
	.stats = sock_mmap_stats,
	.stats_reset = sock_mmap_stats_reset,
	.recv = sock_mmap_recv,
	.send = sock_mmap_send,
	.mtu_get = sock_mmap_mtu_get,
	.promisc_mode_set = sock_mmap_promisc_mode_set,
	.promisc_mode_get = sock_mmap_promisc_mode_get,
	.mac_get = sock_mmap_mac_addr_get,
	.link_status = sock_mmap_link_status,
	.capability = NULL,
	.input_queues_config = NULL,
	.output_queues_config = NULL,
	.in_queues = NULL,
	.pktin_queues = NULL,
	.pktout_queues = NULL,
	.recv_queue = NULL,
	.send_queue = NULL
};
