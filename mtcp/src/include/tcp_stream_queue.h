#ifndef __TCP_STREAM_QUEUE_
#define __TCP_STREAM_QUEUE_

#include <stdint.h>

/* Lock definitions for stream queue */
#if LOCK_STREAM_QUEUE

#if USE_SPIN_LOCK
#define SQ_LOCK_INIT(lock, errmsg, action);		\
	if (pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)) {		\
		perror("pthread_spin_init" errmsg);		\
		action;									\
	}
#define SQ_LOCK_DESTROY(lock)	pthread_spin_destroy(lock)
#define SQ_LOCK(lock)			pthread_spin_lock(lock)
#define SQ_UNLOCK(lock)			pthread_spin_unlock(lock)
#else
#define SQ_LOCK_INIT(lock, errmsg, action);		\
	if (pthread_mutex_init(lock, NULL)) {		\
		perror("pthread_mutex_init" errmsg);	\
		action;									\
	}
#define SQ_LOCK_DESTROY(lock)	pthread_mutex_destroy(lock)
#define SQ_LOCK(lock)			pthread_mutex_lock(lock)
#define SQ_UNLOCK(lock)			pthread_mutex_unlock(lock)
#endif /* USE_SPIN_LOCK */

#else /* LOCK_STREAM_QUEUE */
#define SQ_LOCK_INIT(lock, errmsg, action)	(void) 0
#define SQ_LOCK_DESTROY(lock)	(void) 0
#define SQ_LOCK(lock)			(void) 0
#define SQ_UNLOCK(lock)			(void) 0
#endif /* LOCK_STREAM_QUEUE */

/*---------------------------------------------------------------------------*/
typedef struct stream_queue* stream_queue_t;
/*---------------------------------------------------------------------------*/
typedef struct stream_queue_int
{
	struct tcp_stream **array;
	int size;

	int first;
	int last;
	int count;

} stream_queue_int;
/*---------------------------------------------------------------------------*/
stream_queue_int * 
CreateInternalStreamQueue(int size);
/*---------------------------------------------------------------------------*/
void 
DestroyInternalStreamQueue(stream_queue_int *sq);
/*---------------------------------------------------------------------------*/
int 
StreamInternalEnqueue(stream_queue_int *sq, struct tcp_stream *stream);
/*---------------------------------------------------------------------------*/
struct tcp_stream *
StreamInternalDequeue(stream_queue_int *sq);
/*---------------------------------------------------------------------------*/
stream_queue_t 
CreateStreamQueue(int size);
/*---------------------------------------------------------------------------*/
void 
DestroyStreamQueue(stream_queue_t sq);
/*---------------------------------------------------------------------------*/
int 
StreamEnqueue(stream_queue_t sq, struct tcp_stream *stream);
/*---------------------------------------------------------------------------*/
struct tcp_stream *
StreamDequeue(stream_queue_t sq);
/*---------------------------------------------------------------------------*/
int 
StreamQueueIsEmpty(stream_queue_t sq);
/*---------------------------------------------------------------------------*/

#endif /* __TCP_STREAM_QUEUE_ */
