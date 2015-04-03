#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>

#include "debug.h"
#include "fhash.h"

/*----------------------------------------------------------------------------*/
struct hashtable * 
CreateHashtable(unsigned int (*hashfn) (const tcp_stream *), // key function
				 int (*eqfn) (const tcp_stream*,
							  const tcp_stream *))            // equality
{
	int i;
	struct hashtable* ht = calloc(1, sizeof(struct hashtable));
	if (!ht){
		TRACE_ERROR("calloc: CreateHashtable");
		return 0;
	}

	ht->hashfn = hashfn;
	ht->eqfn = eqfn;

	/* init the tables */
	for (i = 0; i < NUM_BINS; i++)
		TAILQ_INIT(&ht->ht_table[i]);
	return ht;
}
/*----------------------------------------------------------------------------*/
void
DestroyHashtable(struct hashtable *ht)
{
	free(ht);	
}
/*----------------------------------------------------------------------------*/
int 
HTInsert(struct hashtable *ht, tcp_stream *item)
{
	/* create an entry*/ 
	int idx;

	assert(ht);
	assert(ht->ht_count <= 65535); // uint16_t ht_count 

	idx = ht->hashfn(item);
	assert(idx >=0 && idx < NUM_BINS);

#if STATIC_TABLE
	for (i = 0; i < TCP_AR_CNT; i++) {
		// insert into empty array slot
		if (!ht->ht_array[idx][i]) {
			ht->ht_array[idx][i] = item;
			item->ht_idx = i;
			ht->ht_count++;
			return 0;
		}
	}
	
	TRACE_INFO("[WARNING] HTSearch() cnt: %d!!\n", TCP_AR_CNT);	
#endif

	TAILQ_INSERT_TAIL(&ht->ht_table[idx], item, rcvvar->he_link);
	item->ht_idx = TCP_AR_CNT;
	ht->ht_count++;
	
	return 0;
}
/*----------------------------------------------------------------------------*/
void* 
HTRemove(struct hashtable *ht, tcp_stream *item)
{
	hash_bucket_head *head;
	int idx = ht->hashfn(item);

#if STATIC_TABLE
	if (item->ht_idx < TCP_AR_CNT) {
		assert(ht_array[idx][item->ht_idx]);
		ht->ht_array[idx][item->ht_idx] = NULL;
	} else {
#endif
		head = &ht->ht_table[idx];
		TAILQ_REMOVE(head, item, rcvvar->he_link);	
#if STATIC_TABLE
	}
#endif

	ht->ht_count--;
	return (item);
}	
/*----------------------------------------------------------------------------*/
tcp_stream* 
HTSearch(struct hashtable *ht, const tcp_stream *item)
{
	int idx;
	tcp_stream *walk;
	hash_bucket_head *head;

	idx = ht->hashfn(item);

#if STATIC_TABLE
	for (i = 0; i < TCP_AR_CNT; i++) {
		if (ht->ht_array[idx][i]) {
			if (ht->eqfn(ht->ht_array[idx][i], item)) 
				return ht->ht_array[idx][i];
		}
	}
#endif

	head = &ht->ht_table[ht->hashfn(item)];
	TAILQ_FOREACH(walk, head, rcvvar->he_link) {
		if (ht->eqfn(walk, item)) 
			return walk;
	}

	UNUSED(idx);
	return NULL;
}
/*----------------------------------------------------------------------------*/
