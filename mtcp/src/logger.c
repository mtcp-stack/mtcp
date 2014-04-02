#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include "cpu.h"
#include "debug.h"
#include "logger.h"

/*----------------------------------------------------------------------------*/
static void
EnqueueFreeBuffer(log_thread_context *ctx, log_buff *free_bp) 
{
	pthread_mutex_lock(&ctx->free_mutex);
	TAILQ_INSERT_TAIL(&ctx->free_queue, free_bp, buff_link);
	ctx->free_buff_cnt++;

	assert(ctx->free_buff_cnt <= NUM_LOG_BUFF);
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
	pthread_mutex_unlock(&ctx->free_mutex);
}
/*----------------------------------------------------------------------------*/
log_buff*
DequeueFreeBuffer(log_thread_context *ctx)
{
	pthread_mutex_lock(&ctx->free_mutex);
	log_buff *free_bp = TAILQ_FIRST(&ctx->free_queue);
	if (free_bp) {
		TAILQ_REMOVE(&ctx->free_queue, free_bp, buff_link);
		ctx->free_buff_cnt--;
	}

	assert(ctx->free_buff_cnt >= 0);
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
	pthread_mutex_unlock(&ctx->free_mutex);
	return (free_bp);
}
/*----------------------------------------------------------------------------*/
void
EnqueueJobBuffer(log_thread_context *ctx, log_buff *working_bp)
{
	TAILQ_INSERT_TAIL(&ctx->working_queue, working_bp, buff_link);
	ctx->job_buff_cnt++;
	ctx->state = ACTIVE_LOGT;
	assert(ctx->job_buff_cnt <= NUM_LOG_BUFF);
	if (ctx->free_buff_cnt + ctx->job_buff_cnt > NUM_LOG_BUFF) {
		TRACE_ERROR("free_buff_cnt(%d) + job_buff_cnt(%d) > NUM_LOG_BUFF(%d)\n", 
				ctx->free_buff_cnt, ctx->job_buff_cnt, NUM_LOG_BUFF);
	}
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
}
/*----------------------------------------------------------------------------*/
static log_buff*
DequeueJobBuffer(log_thread_context *ctx) 
{
	pthread_mutex_lock(&ctx->mutex);
	log_buff *working_bp = TAILQ_FIRST(&ctx->working_queue);
	if (working_bp) {
		TAILQ_REMOVE(&ctx->working_queue, working_bp, buff_link);
		ctx->job_buff_cnt--;
	} else {
		ctx->state = IDLE_LOGT;
	}
	
	assert(ctx->job_buff_cnt >= 0);
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
	pthread_mutex_unlock(&ctx->mutex);
	return (working_bp);
}
/*----------------------------------------------------------------------------*/
void
InitLogThreadContext(struct log_thread_context *ctx, int cpu) 
{
	int i;
	int sv[2];

	/* initialize log_thread_context */
	memset(ctx, 0, sizeof(struct log_thread_context));
	ctx->cpu = cpu;
	ctx->state = IDLE_LOGT;
	ctx->done = 0;

	if (pipe(sv)) {
		fprintf(stderr, "pipe() failed, errno=%d, errstr=%s\n", 
				errno, strerror(errno));
		exit(1);
	}
	ctx->sp_fd = sv[0];
	ctx->pair_sp_fd = sv[1];

	pthread_mutex_init(&ctx->mutex, NULL);
	pthread_mutex_init(&ctx->free_mutex, NULL);

	TAILQ_INIT(&ctx->working_queue);
	TAILQ_INIT(&ctx->free_queue);

	/* initialize free log_buff */
	log_buff *w_buff = malloc(sizeof(log_buff) * NUM_LOG_BUFF);
	assert(w_buff);
	for (i = 0; i < NUM_LOG_BUFF; i++) {
		EnqueueFreeBuffer(ctx, &w_buff[i]);
	}
}
/*----------------------------------------------------------------------------*/
void *
ThreadLogMain(void* arg)
{
	size_t len;
	log_thread_context* ctx = (log_thread_context *) arg;
	log_buff* w_buff;
	int cnt;

	mtcp_core_affinitize(ctx->cpu);
	//fprintf(stderr, "[CPU %d] Log thread created. thread: %lu\n", 
	//		ctx->cpu, pthread_self());

	TRACE_LOG("Log thread %d is starting.\n", ctx->cpu);

	while (!ctx->done) {
		/* handle every jobs in job buffer*/
		cnt = 0;
		while ((w_buff = DequeueJobBuffer(ctx))){
			if (++cnt > NUM_LOG_BUFF) {
				TRACE_ERROR("CPU %d: Exceed NUM_LOG_BUFF %d.\n", 
						ctx->cpu, cnt);
				break;
			}
			len = fwrite(w_buff->buff, 1, w_buff->buff_len, w_buff->fid);
			if (len != w_buff->buff_len) {
				TRACE_ERROR("CPU %d: Tried to write %d, but only write %ld\n", 
						ctx->cpu, w_buff->buff_len, len);
			}
			//assert(len == w_buff->buff_len);
			EnqueueFreeBuffer(ctx, w_buff);
		}

		/* */
		while (ctx->state == IDLE_LOGT && !ctx->done) {
			char temp[1];
			int ret = read(ctx->sp_fd, temp, 1);
			if (ret)
				break;
		}
	}
	
	TRACE_LOG("Log thread %d out of first loop.\n", ctx->cpu);
	/* handle every jobs in job buffer*/
	cnt = 0;
	while ((w_buff = DequeueJobBuffer(ctx))){
		if (++cnt > NUM_LOG_BUFF) {
			TRACE_ERROR("CPU %d: "
					"Exceed NUM_LOG_BUFF %d in final loop.\n", ctx->cpu, cnt);
			break;
		}
		len = fwrite(w_buff->buff, 1, w_buff->buff_len, w_buff->fid);
		assert(len == w_buff->buff_len);
		EnqueueFreeBuffer(ctx, w_buff);
	}
	
	TRACE_LOG("Log thread %d finished.\n", ctx->cpu);
	pthread_exit(NULL);

	return NULL;
}
/*----------------------------------------------------------------------------*/
