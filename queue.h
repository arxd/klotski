#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "types.h"

#define QFANOUT (CACHE_LINE / sizeof(PQNode))
#define PRIORITY_MAX 1e100

typedef struct s_PQNode PQNode;

struct s_PQNode {
    float pri; // priority in queue
    StatePtr sp; // pointer to this state
};

/** Priority queue
 */
struct s_PQueue {
    Iint len;   // sizeof pq
    Iint brk;   // first virgin mem
    Iint num;   // number of items in the queue
    pthread_mutex_t lock;
    PQNode *pqraw,*pq; // fanout of QFANOUT
};

void queue_init(Queue *q, Iint size);
void queue_fini(Queue *q);
void queue_push(Queue *q, StatePtr sp, float pri);
StatePtr queue_pop(Queue *q);
int queue_mem1k(void);
int queue_test(void);

#endif

