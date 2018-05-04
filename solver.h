/**

List of states (huge)  N

rbtree (index pk:hash)  N



*/
#ifndef SOLVER_H
#define SOLVER_H

#include <pthread.h>
#include "types.h"
#include "list.h"
#include "index.h"
#include "mem.h"
#include "queue.h"
#include "board.h"
#include "state.h"

typedef struct {
    u16 piece;
    u16 dir;
} Move;

typedef struct {
    pthread_t thread;
    int i; // thread number
    int num, oops, dup, dpth; //number of states analyzed
    float dist;
    pthread_mutex_t lock; // for communicating with parent
    Solver *ks;  // the shared solver state
} ThreadState;

struct s_Solver {
    int active_threads;   // number of threads processing states
    pthread_mutex_t alock; // small lock for atomic operations.
    int early_abort;      // everyone stop and exit
    Board bd;             // the starting board
    StatePtr root;        // starting state
    StatePtr solution;    // the end state
    Queue pq;             // priority queue
    StateSet states;      // the states living on this node
}; 


void solver_init(Solver *sv, Board bd, Iint nstates);
void solver_fini(Solver *sv);
void solver_solve(Solver *ks, int nthreads);
void solver_make_sequence(Solver *ks, StatePtr sp, List *seq);

#endif

