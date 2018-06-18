#include "pacing.h"
#include "clock.h"

#if RATE_LIMIT_ENABLED
token_bucket *NewTokenBucket() {
    token_bucket *bucket;
    bucket = malloc(sizeof(token_bucket));
    if (bucket) {
        fprintf(stderr, "created bucket!\n");
    }
    bucket->rate = 0;
    bucket->burst = 14480;
    bucket->tokens = bucket->burst;
    bucket->last_fill_t = now_usecs();
    return bucket;
}

void _refill_bucket(token_bucket *bucket) {
    uint32_t elapsed = time_since_usecs(bucket->last_fill_t);
    double new_tokens = (bucket->rate / 1000000.0) * elapsed;
    double prev_tokens = bucket->tokens;
    bucket->tokens = MIN(bucket->burst, bucket->tokens + new_tokens);
    if (bucket->tokens > prev_tokens) {
        bucket->last_fill_t = now_usecs();
    } else {
        //fprintf(stderr, "elapsed=%lu new=%f\n", time_since_usecs(bucket->last_fill_t), new_tokens);
    }
}

int SufficientTokens(token_bucket *bucket, uint64_t new_bits) {
    double new_bytes = (new_bits / 8.0);

    //fprintf(stderr, "checking for %ld tokens\n", new_bits);

    _refill_bucket(bucket);

    if (bucket->tokens >= new_bytes) {
        bucket->tokens -= new_bytes;
        return 0;
    }

    return -1;
}

void PrintBucket(token_bucket *bucket) {
    fprintf(stderr, "[rate=%.3f tokens=%f last=%u]\n",
            bucket->rate / 1000000.0,
            bucket->tokens,
            bucket->last_fill_t);
}
#endif

#if PACING_ENABLED
packet_pacer *NewPacketPacer() {
    packet_pacer *pacer;
    pacer = malloc(sizeof(packet_pacer));
    pacer->rate_bps = 0;
    pacer->extra_packets = 1;
    pacer->next_send_time = 0; 
    return pacer;
}

#define PACKET_SIZE 1500
int CanSendNow(packet_pacer *pacer) {
    if (pacer->rate_bps == 0) {
        return TRUE;
    }

    uint32_t now = now_usecs();
    if (now >= pacer->next_send_time) {
        pacer->next_send_time = now + (int)(PACKET_SIZE / (pacer->rate_bps / 8000000.0));
        pacer->extra_packets = 1;
        //fprintf(stderr, "now=%u, next=%u\n", now, pacer->next_send_time);

        return TRUE;
    } else if (pacer->extra_packets) {
        pacer->extra_packets--;
        return TRUE;
    } else {
        return FALSE;
    }
}

void PrintPacer(packet_pacer *pacer) {
    //fprintf(stderr, "[rate=%u next_time=%u]\n", pacer->rate_bps, pacer->next_send_time);
}

#endif
