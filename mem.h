
#ifndef MEM_H
#define MEM_H

#include <pthread.h>
#include "types.h"

struct s_BlockMem {
    Iint bsize;  // size of block;
    Iint num;   // number of possible slots
    Iint used;  // number of used slots
    Iint brk;   // first of virgin slots
    Iptr free;  // first of free chain
    pthread_mutex_t lock;
    void *mem;
};

void bm_init(BlockMem *bm, Iint bsize, Iint max);
void bm_fini(BlockMem *bm);
void *bm_ref(BlockMem *bm, Iptr el);
Iptr bm_alloc(BlockMem *bm);
void bm_free(BlockMem *bm, Iptr el);


#endif


