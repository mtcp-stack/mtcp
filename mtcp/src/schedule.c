#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>

#include "schedule.h"
#include "lthread_api.h"
#include "debug.h"

#ifdef ENABLE_APP_INTERRUPT
static inline int
UnregisterTimerInterrupt(timer_t timerid)
{
	int ret;
	struct itimerspec its = {
		.it_value = {
			.tv_sec = 0,
			.tv_nsec = 0,
		},
		.it_interval = {
			.tv_sec = 0,
			.tv_nsec = 0,
		},
	};

	ret = timer_settime(timerid, 0, &its, NULL);
	if (ret < 0)
		TRACE_ERROR("Failed to unregister timer interrupt: (reason: %d)\n", errno);

	return ret;
}

static inline int
RegisterTimerInterrupt(timer_t timerid, uint32_t timeout)
{
	int ret;
	struct itimerspec its = {
		.it_value = {
			.tv_sec = 0,
			.tv_nsec = timeout,
		},
		.it_interval = {
			.tv_sec = 0,
			.tv_nsec = 0,
		},
	};

	ret = timer_settime(timerid, 0, &its, NULL);
	if (ret < 0)
		TRACE_ERROR("Failed to register timer interrupt: (reason: %d)\n", errno);

	return ret;
}

void
HandleTimerInterrupt(int sig, siginfo_t *si, void *uc)
{
    int nif, pending_nic;

	if (si->si_code != SI_TIMER) {
		fprintf(stderr, "No\n");
	}

	mtcp_thread_context_t ctx;

	ctx = (mtcp_thread_context_t) si->si_value.sival_ptr;
	if (ctx != NULL) {
		// TODO update stat schedule
	}

    /* check if there's any pending RX packet in NIC */
    pending_nic = 0;
    for (nif = 0; nif < CONFIG.eths_num; nif++)
        pending_nic += ctx->mtcp_manager->iom->dev_ioctl(ctx, nif, DRV_PENDING_RX_PKT, NULL);

    if (!pending_nic) {
        RegisterTimerInterrupt(ctx->timerid, ctx->tick);

        return;
    }

	/* yield to stack if app is not in mTCP API */
	ctx->int_app = TRUE;
	YieldToStack(ctx, YIELD_REASON_TIMER);
}

int
InitTimer(mtcp_thread_context_t ctx)
{
	int ret;
	struct sigevent sev;

	memset(&sev, -1, sizeof(struct sigevent));
	sev.sigev_signo = TIMER_SIGNAL;
	sev.sigev_notify = SIGEV_THREAD_ID;
	sev.sigev_value.sival_ptr = ctx;
	sev._sigev_un._tid = syscall(SYS_gettid);

	ret = timer_create(CLOCK_REALTIME, &sev, &ctx->timerid);
	if (ret < 0)
		perror("timer_create");

    /* FIXME currently using static 50us */
	ctx->tick = 50000;
	ctx->int_app = FALSE;
	ctx->in_api = FALSE;

	UnregisterTimerInterrupt(ctx->timerid);

	return ret;
}

int
DeleteTimer(mtcp_thread_context_t ctx)
{
	int ret;

	ret = timer_delete(ctx->timerid);
	if (ret < 0)
		perror("timer_delete");

	return ret;
}

#endif

inline void
YieldToStack(mtcp_thread_context_t ctx, int reason)
{
	assert(mtcp);

#ifdef ENABLE_APP_INTERRUPT
	if (reason == YIELD_REASON_TIMER) {
		if (ctx->int_app)
			UnregisterTimerInterrupt(ctx->timerid);
		else
			return;
	}
#endif

	lthread_yield();
}

inline void
YieldToApp(mtcp_thread_context_t ctx, int interrupt)
{
	assert(mtcp);

#ifdef ENABLE_APP_INTERRUPT
	if (interrupt)
		RegisterTimerInterrupt(ctx->timerid, ctx->tick);
#endif

	lthread_yield();
}
