#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include <signal.h>
#include "mtcp.h"

/* global constants */
#define TIMER_SIGNAL           SIGRTMIN
#define YIELD_REASON_TIMER          0x1
#define YIELD_REASON_EPOLL          0x2
#define YIELD_REASON_BLOCK          0x4

#ifdef ENABLE_APP_INTERRUPT

/* initialize and delete per-thread timer */
int
InitTimer(mtcp_thread_context_t mtcp);

int
DeleteTimer(mtcp_thread_context_t mtcp);

/* setup timer interrupt */
int
SetupTimerInterrupt(mtcp_thread_context_t mtcp);

void
HandleTimerInterrupt(int sig, siginfo_t *si, void *uc);

/* enable and disable timer interrupt */
int
EnableTimerInterrupt(mtcp_thread_context_t mtcp);

int
DisableTimerInterrupt(mtcp_thread_context_t mtcp);

#endif /* ENABLE_APP_INTERRUPT */

/* yield between stack context and app context */
inline void
YieldToStack(mtcp_thread_context_t mtcp, int reason);

inline void
YieldToApp(mtcp_thread_context_t mtcp, int interrupt);

#endif /* _SCHEDULE_H_ */
