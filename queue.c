#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "base.h"
#include "queue.h"


void queue_init(Queue *q, Iint size)
{
    if(size % QFANOUT)
	DIE("Size must be a multiple of %lu", QFANOUT);

    memset(q, 0, sizeof(Queue));
    q->pqraw = malloc(sizeof(PQNode) * (size+CACHE_LINE));
    unsigned long ptr = (unsigned long)q->pqraw;
    q->pq = (void*)((ptr + CACHE_LINE) & ~(CACHE_LINE-1)) ;
    q->len = size;
    printf("Queue Size = %u\n", size);
    pthread_mutex_init(&q->lock, NULL);
}

void queue_fini(Queue *q)
{
    free(q->pqraw);
    pthread_mutex_destroy(&q->lock);
}

void queue_push(Queue *q, StatePtr sp, float pri)
{
    int i;
    pthread_mutex_lock(&q->lock);

    if(q->num == q->brk) {
        // this is virgin memory that has not been initlized
        if(q->num == q->len)
            DIE("Queue FULL");
        for(i=0; i< QFANOUT; i++)
	    q->pq[q->brk+i].pri = PRIORITY_MAX;
        q->brk += QFANOUT;
    }

    // Walk it up the tree
    PQNode *node = &q->pq[q->num];
    Iptr p = q->num / QFANOUT;
    while(p && pri < q->pq[--p].pri) {
        *node = q->pq[p];
        node = &q->pq[p];
        p /= QFANOUT;
    }
    node->pri = pri;
    node->sp = sp;
    q->num ++;
    pthread_mutex_unlock(&q->lock);
}

/** Loop unrolling with accumulators
 */
static inline int get_min2(PQNode *nset)
{
    int i, min1 = 0, min2 = QFANOUT/2;
    for(i = 0; i < QFANOUT/2; i++) {
	if(nset[i].pri < nset[min1].pri)   min1 = i;
	if(nset[i+QFANOUT/2].pri < nset[min2].pri) min2 = i+QFANOUT/2;
    }
    return nset[min1].pri < nset[min2].pri ? min1 : min2;
}

/** implement this in ASM
 */
static inline int get_min4(PQNode *nset)
{
    //ASSERT(QFANOUT == 8, "FANOUT == %d", QFANOUT);
    int m[4];
    m[0] = 0 + (nset[1].pri < nset[0].pri);
    m[1] = 2 + (nset[3].pri < nset[2].pri);
    m[2] = m[0 + (nset[m[1]].pri < nset[m[0]].pri)];
    
    m[0] = 4 + (nset[5].pri < nset[4].pri);
    m[1] = 6 + (nset[7].pri < nset[6].pri);
    m[3] = m[0 + (nset[m[1]].pri < nset[m[0]].pri)];
    
    return m[2 + (nset[m[3]].pri < nset[m[2]].pri)]; 
}

/** Simple linear
 */
static inline int get_min(PQNode *nset)
{
    int i, min=0;
    for(i=1; i < QFANOUT; i++) {
        if(nset[i].pri < nset[min].pri)
            min = i;
    }
    return min;
}

/** returns the StatePtr with the minimal priority
 */
StatePtr queue_pop(Queue *q)
{   
    pthread_mutex_lock(&q->lock);

    if(q->num == 0) {
	pthread_mutex_unlock(&q->lock);
        return 0;
    }

    // Remove the last one
    PQNode tail = q->pq[--q->num];
    q->pq[q->num].pri = PRIORITY_MAX;

    // Insert tail into the queue from the top
    PQNode min_node, *parent = &min_node;
    Iint set = 0, min, i, nsets = q->num / QFANOUT + !!(q->num % QFANOUT);
    while(set < nsets) {
        // find min of the current set
        min = get_min2(q->pq + QFANOUT * set);
        i = QFANOUT * set + min;
        if(tail.pri <= q->pq[i].pri)
            break;
        // next level down
        *parent = q->pq[i];
        parent = &q->pq[i];
        set = i + 1;
    }
    *parent = tail;
    
    pthread_mutex_unlock(&q->lock);

    return min_node.sp;
}

int queue_test(void)
{
    int i, e = 0, size=1<<15;
    u32 z,a,b=0;

    Queue q;
    queue_init(&q, size);

    for(i=0; i<size; i++) {
        z = rand()%100;
        queue_push(&q, z,z);
    }
    for(i=0; i<size; i++) {
        a = queue_pop(&q);
        if(a < b)
            e |= 0x1;
        b=a;
    }

    if(q.num != 0) 
        e |= 0x2;
    
    queue_pop(&q); // test for zero pop

    queue_fini(&q);
    return e;
}



