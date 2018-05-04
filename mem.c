#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include "base.h"
#include "mem.h"

void bm_init(BlockMem *bm, Iptr bsize, Iptr num)
{
    // allocate with stdlib or kernel memmap?
    unsigned long totsize = (unsigned long)bsize*num;
    memset(bm, 0, sizeof(BlockMem));
    bm->mem = malloc(totsize);
    if(!bm->mem)
	DIE("No mem");
    /*printf("Zeroing %dx%d: %p - %p\n", bsize, num,
	bm->mem, bm->mem+totsize-1);
    unsigned long i;
    for(i=0; i < totsize; i++)
	((u8*)bm->mem)[i] = 0;
    */
    bm->num = num;
    bm->bsize = bsize;
    pthread_mutex_init(&bm->lock, NULL);
}

void bm_fini(BlockMem *bm)
{
    free(bm->mem);
    bm->mem = 0;
    pthread_mutex_destroy(&bm->lock);
}

inline void *bm_ref(BlockMem *bm, Iptr el)
{
    ASSERT(el, "Null Iptr deref");
    ASSERT(el <= bm->num, "Iptr out of range %u", el);
    unsigned long off = (unsigned long)(el-1) * bm->bsize;
    return (void*)bm->mem + off;
}

Iptr bm_alloc(BlockMem *bm)
{
    pthread_mutex_lock(&bm->lock);
    Iptr blk = 0;
    bm->used ++;
    if(bm->free) { // we have a chain of free blks
	blk = bm->free;
	bm->free = *(Iptr*)bm_ref(bm, bm->free);
    } else if(bm->brk < bm->num) {
	bm->brk++;
	blk = bm->brk;
    } else {
	bm->used --;
    }
    pthread_mutex_unlock(&bm->lock);
    return blk;
}

void bm_free(BlockMem *bm, Iptr el)
{
    Iptr *i = (Iptr*)bm_ref(bm, el);
    *i = bm->free;
    bm->free = el;
}


