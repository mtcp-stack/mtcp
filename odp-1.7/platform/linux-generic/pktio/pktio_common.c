/* Copyright (c) 2013, Linaro Limited
 * Copyright (c) 2013, Nokia Solutions and Networks
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp_packet_io_internal.h>
#include <odp_classification_internal.h>

int _odp_packet_cls_enq(pktio_entry_t *pktio_entry,
			const uint8_t *base, uint16_t buf_len,
			odp_packet_t *pkt_ret)
{
	cos_t *cos;
	odp_packet_t pkt;
	odp_packet_hdr_t pkt_hdr;
	int ret;
	odp_pool_t pool;
	int id = 0;

	packet_parse_reset(&pkt_hdr);

	_odp_cls_parse(&pkt_hdr, base);
	cos = pktio_select_cos(pktio_entry, id, base, &pkt_hdr);

	/* if No CoS found then drop the packet */
	if (cos == NULL || cos->s.queue == NULL || cos->s.pool == NULL)
		return 0;

	pool = cos->s.pool->s.pool_hdl;

	pkt = odp_packet_alloc(pool, buf_len);
	if (odp_unlikely(pkt == ODP_PACKET_INVALID))
		return 0;

	copy_packet_parser_metadata(&pkt_hdr, odp_packet_hdr(pkt));
	odp_packet_hdr(pkt)->input = pktio_entry->s.id;

	if (odp_packet_copydata_in(pkt, 0, buf_len, base) != 0) {
		odp_packet_free(pkt);
		return 0;
	}

	/* Parse and set packet header data */
	odp_packet_pull_tail(pkt, odp_packet_len(pkt) - buf_len);
	ret = queue_enq(cos->s.queue, odp_buf_to_hdr((odp_buffer_t)pkt), 0);
	if (ret < 0) {
		*pkt_ret = pkt;
		return 1;
	}

	return 0;
}

int sock_stats_reset_fd(pktio_entry_t *pktio_entry, int fd)
{
	int err = 0;
	odp_pktio_stats_t cur_stats;

	if (pktio_entry->s.stats_type == STATS_UNSUPPORTED) {
		memset(&pktio_entry->s.stats, 0,
		       sizeof(odp_pktio_stats_t));
		return 0;
	}

	memset(&cur_stats, 0, sizeof(odp_pktio_stats_t));

	if (pktio_entry->s.stats_type == STATS_ETHTOOL) {
		(void)ethtool_stats_get_fd(fd,
					   pktio_entry->s.name,
					   &cur_stats);
	} else if (pktio_entry->s.stats_type == STATS_SYSFS) {
		err = sysfs_stats(pktio_entry, &cur_stats);
		if (err != 0)
			ODP_ERR("stats error\n");
	}

	if (err == 0)
		memcpy(&pktio_entry->s.stats, &cur_stats,
		       sizeof(odp_pktio_stats_t));

	return err;
}

int sock_stats_fd(pktio_entry_t *pktio_entry,
		  odp_pktio_stats_t *stats,
		  int fd)
{
	odp_pktio_stats_t cur_stats;
	int ret = 0;

	if (pktio_entry->s.stats_type == STATS_UNSUPPORTED)
		return 0;

	memset(&cur_stats, 0, sizeof(odp_pktio_stats_t));
	if (pktio_entry->s.stats_type == STATS_ETHTOOL) {
		(void)ethtool_stats_get_fd(fd,
					   pktio_entry->s.name,
					   &cur_stats);
	} else if (pktio_entry->s.stats_type == STATS_SYSFS) {
		sysfs_stats(pktio_entry, &cur_stats);
	}

	stats->in_octets = cur_stats.in_octets -
				pktio_entry->s.stats.in_octets;
	stats->in_ucast_pkts = cur_stats.in_ucast_pkts -
				pktio_entry->s.stats.in_ucast_pkts;
	stats->in_discards = cur_stats.in_discards -
				pktio_entry->s.stats.in_discards;
	stats->in_errors = cur_stats.in_errors -
				pktio_entry->s.stats.in_errors;
	stats->in_unknown_protos = cur_stats.in_unknown_protos -
				pktio_entry->s.stats.in_unknown_protos;

	stats->out_octets = cur_stats.out_octets -
				pktio_entry->s.stats.out_octets;
	stats->out_ucast_pkts = cur_stats.out_ucast_pkts -
				pktio_entry->s.stats.out_ucast_pkts;
	stats->out_discards = cur_stats.out_discards -
				pktio_entry->s.stats.out_discards;
	stats->out_errors = cur_stats.out_errors -
				pktio_entry->s.stats.out_errors;

	return ret;
}
