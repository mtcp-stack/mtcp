#ifndef DEBUG_H
#define DEBUG_H

#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include "mtcp.h"
#include "tcp_in.h"

#ifdef DBGTEMP

#define TRACE_TEMP(f, m...) { \
	fprintf(stderr, "[CPU %d][%10s:%4d] " f, mtcp->ctx->cpu, 		\
			__FUNCTION__, __LINE__, ##m);	\
	}

#else

#define TRACE_TEMP(f, m...)	(void)0

#endif /* DBGTEMP*/

#ifdef DBGERR

#define TRACE_ERROR(f, m...) { \
	fprintf(stderr, "[%10s:%4d] " f, __FUNCTION__, __LINE__, ##m);	\
	}

#else

#define TRACE_ERROR(f, m...)	(void)0

#endif /* DBGERR */

#ifdef DBGCERR

#define CTRACE_ERROR(f, m...) { \
	fprintf(stderr, "[CPU %d][%10s:%4d] " f, mtcp->ctx->cpu, __FUNCTION__, __LINE__, ##m);	\
	}

#else

#define CTRACE_ERROR(f, m...)	(void)0

#endif /* DBGERR */

#ifdef DBGMSG

#define TRACE_DBG(f, m...) {\
	thread_printf(mtcp, mtcp->log_fp, "[%10s:%4d] " \
			f, __FUNCTION__, __LINE__, ##m);   \
	}

#else

#define TRACE_DBG(f, m...)   (void)0

#endif /* DBGMSG */

#ifdef INFO                       

#define TRACE_INFO(f, m...) {                                         \
	fprintf(stderr, "[%10s:%4d] " f,__FUNCTION__, __LINE__, ##m);    \
    }

#else

#define TRACE_INFO(f, m...) (void)0

#endif /* INFO */

#define TRACE_CONFIG(f, m...) fprintf(stderr, f, ##m)

#ifdef DBGLOG
#define TRACE_LOG(f, m...) TRACE_INFO(f, ##m)
#else
#define TRACE_LOG(f, m...) (void)0
#endif

#ifdef STREAM
#define TRACE_STREAM(f, m...) TRACE_FUNC("STREAM", f, ##m)
#else
#define TRACE_STREAM(f, m...)   (void)0
#endif

#ifdef STATE
#define TRACE_STATE(f, m...) TRACE_FUNC("STATE", f, ##m)
#else
#define TRACE_STATE(f, m...)   (void)0
#endif

#ifdef SNDBUF
#define TRACE_SNDBUF(f, m...) TRACE_FUNC("SNDBUF", f, ##m)
#else
#define TRACE_SNDBUF(f, m...)   (void)0
#endif

#ifdef RCVBUF
#define TRACE_RCVBUF(f, m...) TRACE_FUNC("RCVBUF", f, ##m)
#else
#define TRACE_RCVBUF(f, m...)   (void)0
#endif

#ifdef CLWND
#define TRACE_CLWND(f, m...) TRACE_FUNC("CLWND", f, ##m)
#else
#define TRACE_CLWND(f, m...)   (void)0
#endif

#ifdef LOSS
#define TRACE_LOSS(f, m...) TRACE_FUNC("LOSS", f, ##m)
#else
#define TRACE_LOSS(f, m...)   (void)0
#endif

#ifdef SACK
#define TRACE_SACK(f, m...) TRACE_FUNC("SACK", f, ##m)
#else
#define TRACE_SACK(f, m...)   (void)0
#endif

#ifdef TSTAMP
#define TRACE_TSTAMP(f, m...) TRACE_FUNC("TSTAMP", f, ##m)
#else
#define TRACE_TSTAMP(f, m...)   (void)0
#endif

#ifdef RTT
#define TRACE_RTT(f, m...) TRACE_FUNC("RTT", f, ##m)
#else
#define TRACE_RTT(f, m...)   (void)0
#endif

#ifdef RTO
#define TRACE_RTO(f, m...) TRACE_FUNC("RTO", f, ##m)
#else
#define TRACE_RTO(f, m...)   (void)0
#endif

#ifdef CONG
#define TRACE_CONG(f, m...) TRACE_FUNC("CONG", f, ##m)
#else
#define TRACE_CONG(f, m...)   (void)0
#endif

#ifdef EPOLL
#define TRACE_EPOLL(f, m...) TRACE_FUNC("EPOLL", f, ##m)
#else
#define TRACE_EPOLL(f, m...)   (void)0
#endif

#ifdef FSTAT
#define TRACE_FSTAT(f, m...) TRACE_FUNC("FSTAT", f, ##m)
#else
#define TRACE_FSTAT(f, m...)   (void)0
#endif

#ifdef APP
#define TRACE_APP(f, m...) TRACE_FUNC("APP", f, ##m)
#else
#define TRACE_APP(f, m...) (void)0
#endif

#ifdef DBGFIN
#define TRACE_FIN(f, m...) TRACE_FUNC("FIN", f, ##m)
#else
#define TRACE_FIN(f, m...) (void)0
#endif

#ifdef TSTAT
#define TRACE_TSTAT(f, m...) TRACE_FUNC("TSTAT", f, ##m)
#else
#define TRACE_TSTAT(f, m...) (void)0
#endif

#ifdef LOOP
#define TRACE_LOOP(f, m...) TRACE_FUNC("LOOP", "ts: %u, "f, cur_ts, ##m)
#else
#define TRACE_LOOP(f, m...) (void)0
#endif

#ifdef ROUND
#define TRACE_ROUND(f, m...) TRACE_FUNC("ROUND", f, ##m)
#else
#define TRACE_ROUND(f, m...) (void)0
#endif

#ifdef SELECT
#define TRACE_SELECT(f, m...) TRACE_FUNC("SELECT", f, ##m)
#else
#define TRACE_SELECT(f, m...) (void)0
#endif

#ifdef API
#define TRACE_API(f, m...) TRACE_FUNC("API", f, ##m)
#else
#define TRACE_API(f, m...) (void)0
#endif

#ifdef DBGFUNC

#define TRACE_FUNC(n, f, m...) {                                         \
	thread_printf(mtcp, mtcp->log_fp, "[%6s: %10s:%4d] " \
			f, n, __FUNCTION__, __LINE__, ##m);    \
	}

#else

#define TRACE_FUNC(f, m...) (void)0

#endif /* DBGFUNC */

void 
DumpPacket(mtcp_manager_t mtcp, char *buf, int len, char *step, int ifindex);

void
DumpIPPacket(mtcp_manager_t mtcp, const struct iphdr *iph, int len);

void
DumpIPPacketToFile(FILE *fout, const struct iphdr *iph, int len);

void
flush_log_data(mtcp_manager_t mtcp);

void
thread_printf(mtcp_manager_t mtcp, FILE* f_idx, const char* _Format, ...);

#endif /* DEBUG_H */
