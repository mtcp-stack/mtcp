#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>

#define LOG_BUFF_SIZE (256*1024)
#define NUM_LOG_BUFF (100)

enum {
	IDLE_LOGT,
	ACTIVE_LOGT
} log_thread_state;

typedef struct log_buff
{
	int tid;
	FILE* fid;
	int buff_len;
	char buff[LOG_BUFF_SIZE];
	TAILQ_ENTRY(log_buff) buff_link;
} log_buff;

typedef struct log_thread_context {
	pthread_t thread;
	int cpu;
	int done;
	int sp_fd;
	int pair_sp_fd;
	int free_buff_cnt;
	int job_buff_cnt;

	uint8_t state;
	
	pthread_mutex_t mutex;
	pthread_mutex_t free_mutex;

	TAILQ_HEAD(, log_buff) working_queue;
	TAILQ_HEAD(, log_buff) free_queue;

} log_thread_context;

log_buff* DequeueFreeBuffer (log_thread_context *ctx);
void EnqueueJobBuffer(log_thread_context *ctx, log_buff* working_bp);
void InitLogThreadContext (log_thread_context *ctx, int cpu);
void *ThreadLogMain(void* arg);

#endif /* LOGGER_H */
