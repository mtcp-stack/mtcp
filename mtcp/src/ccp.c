#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>

#include "mtcp.h"
#include "tcp_in.h"
#include "tcp_stream.h"
#include "debug.h"
#include "clock.h"
#if USE_CCP
#include "ccp.h"
#include "libccp/ccp.h"
/*----------------------------------------------------------------------------*/
static inline void
get_stream_from_ccp(tcp_stream **stream, struct ccp_connection *conn)
{
	*stream = (tcp_stream *) ccp_get_impl(conn);
}
/*----------------------------------------------------------------------------*/
static inline void
get_mtcp_from_ccp(mtcp_manager_t *mtcp)
{
	*mtcp = (mtcp_manager_t) ccp_get_global_impl();
}
/*----------------------------------------------------------------------------*/
/* Function handlers passed to libccp */ 
/*----------------------------------------------------------------------------*/
static void
_dp_set_cwnd(struct ccp_datapath *dp, struct ccp_connection *conn, uint32_t cwnd)
{
	tcp_stream *stream;
	get_stream_from_ccp(&stream, conn);
	uint32_t new_cwnd = MAX(cwnd, TCP_INIT_CWND * stream->sndvar->mss);

	// (time_ms) (rtt) (curr_cwnd_pkts) (new_cwnd_pkts) (ssthresh)
	if (cwnd != stream->sndvar->cwnd) {
		CCP_PROBE("%lu %d %d->%d (ss=%d)\n", 
            USECS_TO_MS(now_usecs()),
            UNSHIFT_SRTT(stream->rcvvar->srtt)
            stream->sndvar->cwnd / stream->sndvar->mss,
            new_cwnd / stream->sndvar->mss,
            stream->sndvar->ssthresh / stream->sndvar->mss
        );
	}
	stream->sndvar->cwnd = new_cwnd;
}
/*----------------------------------------------------------------------------*/
static void
_dp_set_rate_abs(struct ccp_datapath *dp, struct ccp_connection *conn, uint32_t rate)
{
	tcp_stream *stream;
	get_stream_from_ccp(&stream, conn);
#if PACING_ENABLED || RATE_LIMIT_ENABLED
#if RATE_LIMIT_ENABLED
        stream->bucket->rate = rate;
#endif
#if PACING_ENABLED
        stream->pacer->rate_bps = rate;
#endif
#else
	TRACE_ERROR("unable to set rate, both PACING and RATE_LIMIT are disabled."
		    " Enable one to use rates.\n");
#endif
}
/*----------------------------------------------------------------------------*/
static void
_dp_set_rate_rel(struct ccp_datapath *dp, struct ccp_connection *conn, uint32_t factor)
{
	tcp_stream *stream;
	get_stream_from_ccp(&stream, conn);
#if PACING_ENABLED || RATE_LIMIT_ENABLED
#if RATE_LIMIT_ENABLED
        stream->bucket->rate *= (factor / 100);
#endif
#if PACING_ENABLED
        stream->pacer->rate_bps *= (factor / 100);
#endif
#else
	TRACE_ERROR("unable to set rate, both PACING and RATE_LIMIT are disabled."
		    " Enable one to use rates.\n");
#endif
}
/*----------------------------------------------------------------------------*/
int
_dp_send_msg(struct ccp_datapath *dp, struct ccp_connection *conn, char *msg, int msg_size)
{
	mtcp_manager_t mtcp;
	get_mtcp_from_ccp(&mtcp);

	int ret = send(mtcp->to_ccp, msg, msg_size, 0);
	if (ret < 0) {
		TRACE_ERROR("failed to send msg to ccp: %s\n", strerror(errno));
	}
	return ret;
}
/*----------------------------------------------------------------------------*/

/* Connect to CCP process via unix sockets */
/*----------------------------------------------------------------------------*/
void
setup_ccp_connection(mtcp_manager_t mtcp)
{
	mtcp_thread_context_t ctx = mtcp->ctx;
	// TODO do we need a socket per core?
	int cpu = ctx->cpu;
	//char cpu_str[2] = "";
	int recv_sock;
	int path_len;
	int ret;
	struct sockaddr_un local;

	// Make sure unix socket path exists
	ret = mkdir(CCP_UNIX_BASE, 0755);
	if (ret < 0 && errno != EEXIST) {
		TRACE_ERROR("Failed to create path for ccp unix socket (%d): %s\n",
			    ret, strerror(errno));
	}
	ret = mkdir(CCP_UNIX_BASE CCP_ID, 0755);
	if (ret < 0 && errno != EEXIST) {
		TRACE_ERROR("Failed to create path for ccp unix socket (%d): %s\n",
			    ret, strerror(errno));
	}
	if ((recv_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		TRACE_ERROR("Failed to create unix recv socket for ccp comm\n");
		exit(EXIT_FAILURE);
	}
	local.sun_family = AF_UNIX;
	strcpy(local.sun_path, FROM_CCP_PATH);
	unlink(local.sun_path);
	path_len = strlen(local.sun_path) + sizeof(local.sun_family);
	if (bind(recv_sock, (struct sockaddr *)&local, path_len) == -1) {
		TRACE_ERROR("(Cpu %d) failed to bind to unix://%s because %s\n",
			    cpu, FROM_CCP_PATH, strerror(errno));
		exit(EXIT_FAILURE);
	}
	mtcp->from_ccp = recv_sock;


	struct ccp_datapath dp = {
				  .set_cwnd = &_dp_set_cwnd,
				  .set_rate_abs = &_dp_set_rate_abs, 
				  .set_rate_rel = &_dp_set_rate_rel,
				  .send_msg = &_dp_send_msg,
				  .now = &now_usecs,
				  .since_usecs = &time_since_usecs,
				  .after_usecs = &time_after_usecs,
				  .impl = mtcp
	};

	if (ccp_init(&dp) < 0) {
		TRACE_ERROR("Failed to initialize ccp connection map\n");
		exit(EXIT_FAILURE);
	}
}
/*----------------------------------------------------------------------------*/
void
setup_ccp_send_socket(mtcp_manager_t mtcp)
{
	int send_sock;
	int path_len;
	struct sockaddr_un remote;
	if ((send_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		TRACE_ERROR("failed to create unix send socket for ccp comm\n");
		exit(EXIT_FAILURE);
	}
	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, TO_CCP_PATH);//TODO:CCP
	path_len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	if (connect(send_sock, (struct sockaddr *)&remote, path_len) == -1) {
		TRACE_ERROR("failed to connect to unix://%s because %s\n", TO_CCP_PATH, strerror(errno));
		exit(EXIT_FAILURE);
	}
	mtcp->to_ccp = send_sock;
}
/*----------------------------------------------------------------------------*/
void
destroy_ccp_connection(mtcp_manager_t mtcp)
{
	ccp_free();
	close(mtcp->from_ccp);
	close(mtcp->to_ccp);
}
/*----------------------------------------------------------------------------*/

/* Should be called when a new connection is created */
/*----------------------------------------------------------------------------*/
void
ccp_create(mtcp_manager_t mtcp, tcp_stream *stream)
{
	struct ccp_datapath_info info = {
					 .init_cwnd = TCP_INIT_CWND, // TODO maybe multiply by mss?
					 .mss = stream->sndvar->mss,
					 .src_ip = stream->saddr,
					 .src_port = stream->sport,
					 .dst_ip = stream->daddr,
					 .dst_port = stream->dport,
					 .congAlg = "reno"
	};

	stream->ccp_conn = ccp_connection_start((void *) stream, &info);
	if (stream->ccp_conn == NULL) {
		TRACE_ERROR("failed to initialize ccp_connection")
			} else {
		TRACE_CCP("ccp.create(%d)\n", dp->index);
	}
}

/* Should be called on each ACK */
/*----------------------------------------------------------------------------*/
uint32_t last_drop_t = 0;
void
ccp_cong_control(mtcp_manager_t mtcp, tcp_stream *stream, 
		 uint32_t ack, uint64_t bytes_delivered, uint64_t packets_delivered)
{
	uint64_t rin  = bytes_delivered, //* S_TO_US,  // TODO:CCP divide by snd_int_us
		rout = bytes_delivered; // * S_TO_US;  // TODO:CCP divide by rcv_int_us
	struct ccp_connection *conn = stream->ccp_conn;
	struct ccp_primitives *mmt = &conn->prims;

	//log_cwnd_rtt(stream);

	mmt->bytes_acked       = bytes_delivered;
	mmt->packets_acked     = packets_delivered;
	mmt->snd_cwnd          = stream->sndvar->cwnd; 
	mmt->rtt_sample_us     = UNSHIFT_SRTT(stream->rcvvar->srtt); 
	mmt->bytes_in_flight   = 0; // TODO
	mmt->packets_in_flight = 0; // TODO
	mmt->rate_outgoing     = rin;
	mmt->rate_incoming     = rout;
#if TCP_OPT_SACK_ENABLED
	mmt->bytes_misordered   = stream->rcvvar->sacked_pkts * MSS;
	mmt->packets_misordered = stream->rcvvar->sacked_pkts;
#endif

	/*
	  if (last_drop_t == 0 || _dp_since_usecs(last_drop_t) > 25000) {
	  mmt->lost_pkts_sample = 0;
	  last_drop_t = now_usecs();
	  }
	*/

	//fprintf(stderr, "mmt: %u %u\n", conn->prims.packets_misordered, conn->prims.lost_pkts_sample);

	if (conn != NULL) {
		//fprintf(stderr, " lost_pkts=%u\n", mmt->lost_pkts_sample);
		ccp_invoke(conn);
		conn->prims.was_timeout        = false;
		conn->prims.bytes_misordered   = 0;
		conn->prims.packets_misordered = 0;
		conn->prims.lost_pkts_sample   = 0;
#if TCP_OPT_SACK_ENABLED
		stream->rcvvar->sacked_pkts    = 0;
#endif
	} else {
		TRACE_ERROR("ccp_connection not initialized\n");	
	}
}

#if TCP_OPT_SACK_ENABLED
uint32_t window_edge_at_last_loss = 0;
uint32_t last_loss = 0;
#endif
uint32_t last_tri_dupack_seq = 0;

/* Should be called for any other connection event other than ACK */ 
/*----------------------------------------------------------------------------*/
void
ccp_record_event(mtcp_manager_t mtcp, tcp_stream *stream, uint8_t event_type, uint32_t val)
{
#ifdef DBGCCP
	unsigned long now = (unsigned long)(now_usecs());
#endif
	int i;

	switch(event_type) {
        case EVENT_DUPACK:
#if TCP_OPT_SACK_ENABLED
#else
		// use num dupacks as a proxy for sacked
		stream->ccp_conn->prims.bytes_misordered += val;
		stream->ccp_conn->prims.packets_misordered++;
#endif
		break;
        case EVENT_TRI_DUPACK:
#if TCP_OPT_SACK_ENABLED
		if (val > window_edge_at_last_loss) {
			TRACE_CCP("%lu tridup ack=%u\n", 
				  now / 1000,
				  val - stream->sndvar->iss
				  );
			for (i = 0; i < MAX_SACK_ENTRY; i++) {
				window_edge_at_last_loss = MAX(
							       window_edge_at_last_loss,
							       stream->rcvvar->sack_table[i].right_edge
							       );
			}
			last_tri_dupack_seq = val;
			last_loss = now_usecs();
			stream->ccp_conn->prims.lost_pkts_sample++;
		}
#else
		// only count as a loss if we haven't already seen 3 dupacks for
		// this seq number
		if (last_tri_dupack_seq != val) {
			TRACE_CCP("%lu tridup ack=%d\n", 
				  now / 1000,
				  val// - stream->sndvar->iss
				  );
			stream->ccp_conn->prims.lost_pkts_sample++;
			last_tri_dupack_seq = val;
		}
#endif
		break;
        case EVENT_TIMEOUT:
		//stream->ccp_conn->prims.was_timeout = true;
		break;
        case EVENT_ECN:
		TRACE_ERROR("ecn is not currently supported!\n");
		break;
        default:
		TRACE_ERROR("unknown record event type %d!\n", event_type);
		break;
	}
}
/*----------------------------------------------------------------------------*/
#endif
