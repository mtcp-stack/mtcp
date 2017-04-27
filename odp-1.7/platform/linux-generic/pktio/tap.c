/* Copyright (c) 2015, Ilya Maximets <i.maximets@samsung.com>
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * TAP pktio type
 *
 * This file provides a pktio interface that allows for creating and
 * send/receive packets through TAP interface. It is intended for use
 * as a simple conventional communication method between applications
 * that use kernel network stack (ping, ssh, iperf, etc.) and ODP
 * applications for the purpose of functional testing.
 *
 * To use this interface the name passed to odp_pktio_open() must begin
 * with "tap:" and be in the format:
 *
 * tap:iface
 *
 *   iface   the name of TAP device to be created.
 *
 * TUN/TAP kernel module should be loaded to use this pktio.
 * There should be no device named 'iface' in the system.
 * The total length of the 'iface' is limited by IF_NAMESIZE.
 */

#include <odp_posix_extensions.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_tun.h>

#include <odp.h>
#include <odp_packet_socket.h>
#include <odp_packet_internal.h>
#include <odp_packet_io_internal.h>

#define BUF_SIZE 65536

static int gen_random_mac(unsigned char *mac)
{
	mac[0] = 0x7a; /* not multicast and local assignment bit is set */
	if (odp_random_data(mac + 1, 5, false) < 5) {
		ODP_ERR("odp_random_data failed.\n");
		return -1;
	}
	return 0;
}

static int tap_pktio_open(odp_pktio_t id ODP_UNUSED,
			  pktio_entry_t *pktio_entry,
			  const char *devname, odp_pool_t pool)
{
	int fd, skfd, flags, mtu;
	struct ifreq ifr;
	pkt_tap_t *tap = &pktio_entry->s.pkt_tap;

	if (strncmp(devname, "tap:", 4) != 0)
		return -1;

	/* Init pktio entry */
	memset(tap, 0, sizeof(*tap));
	tap->fd = -1;
	tap->skfd = -1;

	if (pool == ODP_POOL_INVALID)
		return -1;

	fd = open("/dev/net/tun", O_RDWR);
	if (fd < 0) {
		__odp_errno = errno;
		ODP_ERR("failed to open /dev/net/tun: %s\n", strerror(errno));
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	/* Flags: IFF_TUN   - TUN device (no Ethernet headers)
	 *        IFF_TAP   - TAP device
	 *
	 *        IFF_NO_PI - Do not provide packet information
	 */
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", devname + 4);

	if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
		__odp_errno = errno;
		ODP_ERR("%s: creating tap device failed: %s\n",
			ifr.ifr_name, strerror(errno));
		goto tap_err;
	}

	/* Set nonblocking mode on interface. */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		__odp_errno = errno;
		ODP_ERR("fcntl(F_GETFL) failed: %s\n", strerror(errno));
		goto tap_err;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		__odp_errno = errno;
		ODP_ERR("fcntl(F_SETFL) failed: %s\n", strerror(errno));
		goto tap_err;
	}

	if (gen_random_mac(tap->if_mac) < 0)
		goto tap_err;

	/* Create AF_INET socket for network interface related operations. */
	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (skfd < 0) {
		__odp_errno = errno;
		ODP_ERR("socket creation failed: %s\n", strerror(errno));
		goto tap_err;
	}

	mtu = mtu_get_fd(skfd, devname + 4);
	if (mtu < 0) {
		__odp_errno = errno;
		ODP_ERR("mtu_get_fd failed: %s\n", strerror(errno));
		goto sock_err;
	}

	/* Up interface by default. */
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) {
		__odp_errno = errno;
		ODP_ERR("ioctl(SIOCGIFFLAGS) failed: %s\n", strerror(errno));
		goto sock_err;
	}

	ifr.ifr_flags |= IFF_UP;
	ifr.ifr_flags |= IFF_RUNNING;

	if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0) {
		__odp_errno = errno;
		ODP_ERR("failed to come up: %s\n", strerror(errno));
		goto sock_err;
	}

	tap->fd = fd;
	tap->skfd = skfd;
	tap->mtu = mtu;
	tap->pool = pool;
	return 0;
sock_err:
	close(skfd);
tap_err:
	close(fd);
	ODP_ERR("Tap device alloc failed.\n");
	return -1;
}

static int tap_pktio_close(pktio_entry_t *pktio_entry)
{
	int ret = 0;
	pkt_tap_t *tap = &pktio_entry->s.pkt_tap;

	if (tap->fd != -1 && close(tap->fd) != 0) {
		__odp_errno = errno;
		ODP_ERR("close(tap->fd): %s\n", strerror(errno));
		ret = -1;
	}

	if (tap->skfd != -1 && close(tap->skfd) != 0) {
		__odp_errno = errno;
		ODP_ERR("close(tap->skfd): %s\n", strerror(errno));
		ret = -1;
	}

	return ret;
}

static odp_packet_t pack_odp_pkt(odp_pool_t pool,
				 const void *data,
				 unsigned int len)
{
	odp_packet_t pkt;

	pkt = packet_alloc(pool, len, 1);

	if (pkt == ODP_PACKET_INVALID)
		return pkt;

	if (odp_packet_copydata_in(pkt, 0, len, data) < 0) {
		ODP_ERR("failed to copy packet data\n");
		odp_packet_free(pkt);
		return ODP_PACKET_INVALID;
	}

	packet_parse_l2(odp_packet_hdr(pkt));

	return pkt;
}

static int tap_pktio_recv(pktio_entry_t *pktio_entry, odp_packet_t pkts[],
			  unsigned len)
{
	ssize_t retval;
	unsigned i;
	uint8_t buf[BUF_SIZE];
	pkt_tap_t *tap = &pktio_entry->s.pkt_tap;

	for (i = 0; i < len; i++) {
		do {
			retval = read(tap->fd, buf, BUF_SIZE);
		} while (retval < 0 && errno == EINTR);

		if (retval < 0) {
			__odp_errno = errno;
			break;
		}

		pkts[i] = pack_odp_pkt(tap->pool, buf, retval);
		if (pkts[i] == ODP_PACKET_INVALID)
			break;
	}

	return i;
}

static int tap_pktio_send(pktio_entry_t *pktio_entry, odp_packet_t pkts[],
			  unsigned len)
{
	ssize_t retval;
	unsigned i, n;
	uint32_t pkt_len;
	uint8_t buf[BUF_SIZE];
	pkt_tap_t *tap = &pktio_entry->s.pkt_tap;

	for (i = 0; i < len; i++) {
		pkt_len = odp_packet_len(pkts[i]);

		if (pkt_len > tap->mtu) {
			if (i == 0) {
				__odp_errno = EMSGSIZE;
				return -1;
			}
			break;
		}

		if (odp_packet_copydata_out(pkts[i], 0, pkt_len, buf) < 0) {
			ODP_ERR("failed to copy packet data\n");
			break;
		}

		do {
			retval = write(tap->fd, buf, pkt_len);
		} while (retval < 0 && errno == EINTR);

		if (retval < 0) {
			if (i == 0 && SOCK_ERR_REPORT(errno)) {
				__odp_errno = errno;
				ODP_ERR("write(): %s\n", strerror(errno));
				return -1;
			}
			break;
		} else if ((uint32_t)retval != pkt_len) {
			ODP_ERR("sent partial ethernet packet\n");
			if (i == 0) {
				__odp_errno = EMSGSIZE;
				return -1;
			}
			break;
		}
	}

	for (n = 0; n < i; n++)
		odp_packet_free(pkts[n]);

	return i;
}

static int tap_mtu_get(pktio_entry_t *pktio_entry)
{
	int ret;

	ret =  mtu_get_fd(pktio_entry->s.pkt_tap.skfd,
			  pktio_entry->s.name + 4);
	if (ret > 0)
		pktio_entry->s.pkt_tap.mtu = ret;

	return ret;
}

static int tap_promisc_mode_set(pktio_entry_t *pktio_entry,
				odp_bool_t enable)
{
	return promisc_mode_set_fd(pktio_entry->s.pkt_tap.skfd,
				   pktio_entry->s.name + 4, enable);
}

static int tap_promisc_mode_get(pktio_entry_t *pktio_entry)
{
	return promisc_mode_get_fd(pktio_entry->s.pkt_tap.skfd,
				   pktio_entry->s.name + 4);
}

static int tap_mac_addr_get(pktio_entry_t *pktio_entry, void *mac_addr)
{
	memcpy(mac_addr, pktio_entry->s.pkt_tap.if_mac, ETH_ALEN);
	return ETH_ALEN;
}

const pktio_if_ops_t tap_pktio_ops = {
	.init = NULL,
	.term = NULL,
	.open = tap_pktio_open,
	.close = tap_pktio_close,
	.start = NULL,
	.stop = NULL,
	.recv = tap_pktio_recv,
	.send = tap_pktio_send,
	.mtu_get = tap_mtu_get,
	.promisc_mode_set = tap_promisc_mode_set,
	.promisc_mode_get = tap_promisc_mode_get,
	.mac_get = tap_mac_addr_get
};
