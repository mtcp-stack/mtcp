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
// 在ctx的free_queue中插入free_bp 缓冲区
static void
EnqueueFreeBuffer(log_thread_context *ctx, log_buff *free_bp) 
{
	pthread_mutex_lock(&ctx->free_mutex);
	//尾插法入队（空闲队列）
	TAILQ_INSERT_TAIL(&ctx->free_queue, free_bp, buff_link);
	ctx->free_buff_cnt++;//增加空闲缓冲区标记

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
	//获取工作队列的头部元素
	log_buff *working_bp = TAILQ_FIRST(&ctx->working_queue);
	if (working_bp) {
		//从工作队列中溢出
		TAILQ_REMOVE(&ctx->working_queue, working_bp, buff_link);
		//修改计数
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
//初始化logger线程上下文
void
InitLogThreadContext(struct log_thread_context *ctx, int cpu) 
{
	int i;
	int sv[2];

	/* initialize log_thread_context */
	memset(ctx, 0, sizeof(struct log_thread_context));
	ctx->cpu = cpu;	//核心ID
	ctx->state = IDLE_LOGT; // 状态为IDLE_LOGT（空闲）
	ctx->done = 0; //终止标志为0（未终止）

	//绑定管道
	if (pipe(sv)) {
		fprintf(stderr, "pipe() failed, errno=%d, errstr=%s\n", 
				errno, strerror(errno));
		exit(1);
	}
	ctx->sp_fd = sv[0]; //读取管道
	ctx->pair_sp_fd = sv[1];	//写入管道

	//初始化互斥锁
	pthread_mutex_init(&ctx->mutex, NULL); //工作队列
	pthread_mutex_init(&ctx->free_mutex, NULL);	//空闲队列

	//初始化队列
	TAILQ_INIT(&ctx->working_queue);
	TAILQ_INIT(&ctx->free_queue);

	/* initialize free log_buff */
	// 向空闲队列插入NUM_LOG_BUFF个缓冲区
	log_buff *w_buff = malloc(sizeof(log_buff) * NUM_LOG_BUFF);
	assert(w_buff);
	for (i = 0; i < NUM_LOG_BUFF; i++) {
		EnqueueFreeBuffer(ctx, &w_buff[i]);
	}
}
/*----------------------------------------------------------------------------*/
//日志线程
void *
ThreadLogMain(void* arg)
{
	size_t len;
	log_thread_context* ctx = (log_thread_context *) arg;
	log_buff* w_buff;
	int cnt;
	//绑定核心
	mtcp_core_affinitize(ctx->cpu);
	//fprintf(stderr, "[CPU %d] Log thread created. thread: %lu\n", 
	//		ctx->cpu, pthread_self());

	TRACE_LOG("Log thread %d is starting.\n", ctx->cpu);

	while (!ctx->done) {
		/* handle every jobs in job buffer*/
		cnt = 0;
		// 获取工作队列元素，写入后归还
		while ((w_buff = DequeueJobBuffer(ctx))){
			if (++cnt > NUM_LOG_BUFF) {
				TRACE_ERROR("CPU %d: Exceed NUM_LOG_BUFF %d.\n", 
						ctx->cpu, cnt);
				break;
			}
			// 写入日志到w_buff->fid
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
			//阻塞读取管道（读端）
			int ret = read(ctx->sp_fd, temp, 1);
			if (ret)
				break;
		}
	}
	
	TRACE_LOG("Log thread %d out of first loop.\n", ctx->cpu);
	/* handle every jobs in job buffer*/
	//同上文循环
	//TODO 为什么此处需要额外循环，其可以保证多线程中新加入的日志不会丢失？
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
