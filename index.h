/**
 * A Btree sorted by the hash value
 * The hash values inserted are aprox random so no balancing is needed
 */
#ifndef INDEX_H
#define INDEX_H

#include <pthread.h>
#include "types.h"
#include "mem.h"

typedef struct s_BNode BNode;

struct s_BNode {
    HashVal  hv[FANOUT]; // hash values
    IndexPtr ln[FANOUT];    // child links
}; // Target size is one cacheline

struct s_Index {
    pthread_rwlock_t locks[HASHTBLSIZE];  // one lock for every btree
    u32 version[HASHTBLSIZE];  // Each btree has a version that increments when it changes
    BlockMem nodes;  // type:BNode   NOTE: Cache-Align this
    BlockMem states; // type:StatePtr[FANOUT]
};

void index_init(Index *idx, Iint size);
void index_fini(Index *idx);
int index_ref(Index *idx, HashVal hv, StatePtr **sout, pthread_rwlock_t **lock);
int index_upgrade_rwlock(Index *idx, int wr, int hashidx, pthread_rwlock_t *lock);
float index_mem();
int index_test();


#endif

