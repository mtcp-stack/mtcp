#ifndef __CLOCK__H_
#define __CLOCK__H_

#include <time.h>
#include <stdint.h>
#include "tcp_stream.h"

uint64_t now_usecs();
uint64_t time_since_usecs();
uint64_t time_after_usecs();
void     log_cwnd_rtt(void *stream);

#endif
