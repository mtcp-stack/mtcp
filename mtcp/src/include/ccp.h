#ifndef __CCP_H_
#define __CCP_H_

#include <sys/un.h>

#include "tcp_stream.h"
#include "tcp_in.h"
#include "debug.h"

// CCP currently only supports a single global datapath and CCP instance, but
// this ID exists in case there is a need for supporting multiple
// If this change is made in the future CCP_UNIX_BASE_ID will need to be
// generated dynamically based on the CCP/datapath ID. For now, we always use 0.
#define CCP_UNIX_BASE    "/tmp/ccp/"
#define CCP_ID           "0/"
#define FROM_CCP         "out"
#define TO_CCP           "in"
#define FROM_CCP_PATH    CCP_UNIX_BASE CCP_ID FROM_CCP
#define TO_CCP_PATH      CCP_UNIX_BASE CCP_ID TO_CCP
#define CCP_MAX_MSG_SIZE 32678

#define MIN(a, b) ((a)<(b)?(a):(b))
#define MAX(a, b) ((a)>(b)?(a):(b))

#define EVENT_DUPACK     1
#define EVENT_TRI_DUPACK 2
#define EVENT_TIMEOUT    3
#define EVENT_ECN        4

void setup_ccp_connection(mtcp_manager_t mtcp);
void setup_ccp_send_socket(mtcp_manager_t mtcp);
void destroy_ccp_connection(mtcp_manager_t mtcp);
void ccp_create(mtcp_manager_t mtcp, tcp_stream *stream);
void ccp_cong_control(mtcp_manager_t mtcp, tcp_stream *stream, uint32_t ack, uint64_t bytes_delivered, uint64_t packets_delivered);
void ccp_record_event(mtcp_manager_t mtcp, tcp_stream *stream, uint8_t event_type, uint32_t val);

#endif
