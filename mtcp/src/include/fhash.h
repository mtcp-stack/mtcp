#ifndef FHASH_H
#define FHASH_H

#include <sys/queue.h>
#include "tcp_stream.h"

#define NUM_BINS_FLOWS 		(131072)     /* 132 K entries per thread*/
#define NUM_BINS_LISTENERS	(1024)	     /* assuming that chaining won't happen excessively */
#define TCP_AR_CNT 		(3)

typedef struct hash_bucket_head {
	tcp_stream *tqh_first;
	tcp_stream **tqh_last;
} hash_bucket_head;

typedef struct list_bucket_head {
	struct tcp_listener *tqh_first;
	struct tcp_listener **tqh_last;
} list_bucket_head;

/* hashtable structure */
struct hashtable {
	uint32_t bins;

	union {
		hash_bucket_head *ht_table;
		list_bucket_head *lt_table;
	};

	// functions
	unsigned int (*hashfn) (const void *);
	int (*eqfn) (const void *, const void *);
};

/*functions for hashtable*/
struct hashtable *CreateHashtable(unsigned int (*hashfn) (const void *), 
				  int (*eqfn) (const void *, 
					       const void *),
				  int bins);
void DestroyHashtable(struct hashtable *ht);


int StreamHTInsert(struct hashtable *ht, void *);
void* StreamHTRemove(struct hashtable *ht, void *);
void *StreamHTSearch(struct hashtable *ht, const void *);
unsigned int HashListener(const void *hbo_port_ptr);
int EqualListener(const void *hbo_port_ptr1, const void *hbo_port_ptr2);
int ListenerHTInsert(struct hashtable *ht, void *);
void *ListenerHTRemove(struct hashtable *ht, void *);
void *ListenerHTSearch(struct hashtable *ht, const void *);

#endif /* FHASH_H */
