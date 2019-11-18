#ifndef __PACING_H_
#define __PACING_H_

#include "tcp_stream.h"
#include "clock.h"

#if RATE_LIMIT_ENABLED
typedef struct token_bucket {
    double tokens;
    uint32_t rate;
    uint32_t burst;
    uint32_t last_fill_t;
} token_bucket;

token_bucket* NewTokenBucket();
int           SufficientTokens(token_bucket *bucket, uint64_t new_bits);
void          PrintBucket(token_bucket *bucket);
#endif

#if PACING_ENABLED
typedef struct packet_pacer {
    uint32_t rate_bps;
    uint32_t extra_packets;
    uint32_t next_send_time;
} packet_pacer;

packet_pacer* NewPacketPacer();
int           CanSendNow(packet_pacer *pacer);
void          PrintPacer(packet_pacer *pacer);
#endif

#endif
