#include "clock.h" 
/*----------------------------------------------------------------------------*/
uint64_t init_time_ns = 0;
uint32_t last_print = 0;
/*----------------------------------------------------------------------------*/
uint64_t
now_usecs()
{
	struct timespec now;
	uint64_t now_ns, now_us;

	clock_gettime(CLOCK_MONOTONIC, &now);

	now_ns = (1000000000L * now.tv_sec) + now.tv_nsec;
	if (init_time_ns == 0) {
		init_time_ns = now_ns;
	}

	now_us = ((now_ns - init_time_ns) / 1000) & 0xffffffff;
	return now_us;
}
/*----------------------------------------------------------------------------*/
uint64_t
time_since_usecs(uint64_t then) {
	return now_usecs() - then;
}
/*----------------------------------------------------------------------------*/
uint64_t
time_after_usecs(uint64_t usecs) {
	return now_usecs() + usecs;
}
/*----------------------------------------------------------------------------*/
#define SAMPLE_FREQ_US 10000

void
log_cwnd_rtt(void *vs) {
	tcp_stream *stream = (tcp_stream *)vs;
	unsigned long now = (unsigned long)(now_usecs());
	if (time_since_usecs(last_print) > SAMPLE_FREQ_US) {
		fprintf(stderr, "%lu %d %d/%d\n", 
			now / 1000, 
			stream->rcvvar->srtt * 125,
			stream->sndvar->cwnd / stream->sndvar->mss,
			stream->sndvar->peer_wnd / stream->sndvar->mss
			);
#if RATE_LIMIT_ENABLED
		PrintBucket(stream->bucket);
#endif
#if PACING_ENABLED
		PrintPacer(stream->pacer);
#endif
		last_print = now;
	}
}
/*----------------------------------------------------------------------------*/
