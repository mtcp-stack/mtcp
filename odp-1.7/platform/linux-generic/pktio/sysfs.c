/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp.h>
#include <odp_packet_io_internal.h>
#include <errno.h>
#include <string.h>

static int sysfs_get_val(const char *fname, uint64_t *val)
{
	FILE  *file;
	char str[128];
	int ret = -1;

	file = fopen(fname, "rt");
	if (file == NULL) {
		__odp_errno = errno;
		/* do not print debug err if sysfs is not supported by
		 * kernel driver.
		 */
		if (errno != ENOENT)
			ODP_ERR("fopen %s: %s\n", fname, strerror(errno));
		return 0;
	}

	if (fgets(str, sizeof(str), file) != NULL)
		ret = sscanf(str, "%" SCNx64, val);

	(void)fclose(file);

	if (ret != 1) {
		ODP_ERR("read %s\n", fname);
		return -1;
	}

	return 0;
}

int sysfs_stats(pktio_entry_t *pktio_entry,
		odp_pktio_stats_t *stats)
{
	char fname[256];
	const char *dev = pktio_entry->s.name;
	int ret = 0;

	sprintf(fname, "/sys/class/net/%s/statistics/rx_bytes", dev);
	ret -= sysfs_get_val(fname, &stats->in_octets);

	sprintf(fname, "/sys/class/net/%s/statistics/rx_packets", dev);
	ret -= sysfs_get_val(fname, &stats->in_ucast_pkts);

	sprintf(fname, "/sys/class/net/%s/statistics/rx_droppped", dev);
	ret -= sysfs_get_val(fname, &stats->in_discards);

	sprintf(fname, "/sys/class/net/%s/statistics/rx_errors", dev);
	ret -= sysfs_get_val(fname, &stats->in_errors);

	/* stats->in_unknown_protos is not supported in sysfs */

	sprintf(fname, "/sys/class/net/%s/statistics/tx_bytes", dev);
	ret -= sysfs_get_val(fname, &stats->out_octets);

	sprintf(fname, "/sys/class/net/%s/statistics/tx_packets", dev);
	ret -= sysfs_get_val(fname, &stats->out_ucast_pkts);

	sprintf(fname, "/sys/class/net/%s/statistics/tx_dropped", dev);
	ret -= sysfs_get_val(fname, &stats->out_discards);

	sprintf(fname, "/sys/class/net/%s/statistics/tx_errors", dev);
	ret -= sysfs_get_val(fname, &stats->out_errors);

	return ret;
}
