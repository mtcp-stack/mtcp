#ifndef STAT_H
#define STAT_H

struct run_stat
{
	uint64_t rounds;
	uint64_t rounds_rx;
	uint64_t rounds_rx_try;
	uint64_t rounds_tx;
	uint64_t rounds_tx_try;
	uint64_t rounds_select;
	uint64_t rounds_select_rx;
	uint64_t rounds_select_tx;
	uint64_t rounds_select_intr;

	uint64_t rounds_accept;
	uint64_t rounds_read;
	uint64_t rounds_write;
	uint64_t rounds_epoll;
	uint64_t rounds_wndadv;

	uint64_t rounds_rtocheck;
	uint64_t rounds_twcheck;
	uint64_t rounds_tocheck;
};

struct stat_counter
{
	uint64_t cnt;
	uint64_t sum;
	uint64_t max;
	uint64_t min;
};

struct time_stat
{
	struct stat_counter round;
	struct stat_counter processing;
	struct stat_counter tcheck;
	struct stat_counter epoll;
	struct stat_counter handle;
	struct stat_counter xmit;
	struct stat_counter select;
};

struct net_stat
{
	uint64_t tx_packets[MAX_DEVICES];
	uint64_t tx_bytes[MAX_DEVICES];
	uint64_t tx_drops[MAX_DEVICES];
	uint64_t rx_packets[MAX_DEVICES];
	uint64_t rx_bytes[MAX_DEVICES];
	uint64_t rx_errors[MAX_DEVICES];
#ifdef ENABLELRO
	uint64_t tx_gdptbytes;
	uint64_t rx_gdptbytes;
#endif
};

struct bcast_stat
{
	uint64_t cycles;
	uint64_t write;
	uint64_t read;
	uint64_t epoll;
	uint64_t wnd_adv;
	uint64_t ack;
};

struct timeout_stat
{
	uint64_t cycles;
	uint64_t rto_try;
	uint64_t rto;
	uint64_t timewait_try;
	uint64_t timewait;
};

#ifdef NETSTAT
#define STAT_COUNT(stat) stat++
#else
#define STAT_COUNT(stat)
#endif

#endif /* STAT_H */
