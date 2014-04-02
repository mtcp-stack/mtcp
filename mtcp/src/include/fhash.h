#ifndef __FHASH_H_
#define __FHASH_H_

#include <sys/queue.h>
#include "tcp_stream.h"

#define NUM_BINS (131072)     /* 132 K entries per thread*/
#define TCP_AR_CNT (3)

#define STATIC_TABLE FALSE

typedef struct hash_bucket_head {
	tcp_stream *tqh_first;
	tcp_stream **tqh_last;
} hash_bucket_head;

/* hashtable structure */
struct hashtable {
	uint8_t ht_count ;                    // count for # entry

#if STATIC_TABLE
	tcp_stream* ht_array[NUM_BINS][TCP_AR_CNT];
#endif
	hash_bucket_head ht_table[NUM_BINS];

	// functions
	unsigned int (*hashfn) (const tcp_stream *);
	int (*eqfn) (const tcp_stream *, const tcp_stream *);
};

/*functions for hashtable*/
struct hashtable *CreateHashtable(unsigned int (*hashfn) (const tcp_stream*), 
								   int (*eqfn) (const tcp_stream*, 
												const tcp_stream *));
void DestroyHashtable(struct hashtable *ht);


int HTInsert(struct hashtable *ht, tcp_stream *);
void* HTRemove(struct hashtable *ht, tcp_stream *);
tcp_stream* HTSearch(struct hashtable *ht, const tcp_stream *);

#endif /* __FHASH_H_ */
