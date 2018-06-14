#ifndef __CCP_H_
#define __CCP_H_

#include <sys/un.h>

#include "tcp_stream.h"
#include "tcp_in.h"
#include "debug.h"

#define FROM_CCP_PREFIX "/tmp/ccp/0/out"
#define TO_CCP_PREFIX "/tmp/ccp/0/in"
#define CCP_MAX_MSG_SIZE 32678

#define MIN(a, b) ((a)<(b)?(a):(b))
#define MAX(a, b) ((a)>(b)?(a):(b))

#define EVENT_DUPACK     1
#define EVENT_TRI_DUPACK 2
#define EVENT_TIMEOUT    3
#define EVENT_ECN        4

void setup_ccp_connection(mtcp_manager_t mtcp);
void destroy_ccp_connection(mtcp_manager_t mtcp);
void ccp_create(mtcp_manager_t mtcp, tcp_stream *stream);
void ccp_cong_control(mtcp_manager_t mtcp, tcp_stream *stream, uint32_t ack, uint64_t bytes_delivered, uint64_t packets_delivered);
void ccp_record_event(mtcp_manager_t mtcp, tcp_stream *stream, uint8_t event_type, uint32_t val);

#endif
