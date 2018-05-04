/** \file state.h
 */
#ifndef STATE_H
#define STATE_H

#include "types.h"
#include "mem.h"
#include "index.h"
#include "board.h"

typedef struct s_StateSet StateSet;
typedef struct s_StateSemi StateSemi;
typedef struct s_StateFull StateFull;


struct s_StateSemi {
    u16 dir:2;   // direction piece moved
    u16 ipcs:8; // piece that moved from parent
    u16 node;    // node on which the parent is hosted
    StatePtr idx_next;
    StatePtr parent;
};

struct s_StateFull {
    struct s_StateSemi semi;
    u16 depth;
    u16 pcs[];
};

struct s_StateSet {
    int npcs;  // number of pieces in the game
    int sizeof_full;  // since StateFull is a [] this is it's size
    int shorterr; // number of short-path errors

    // for multi node processing
    int nnodes; // total number of nodes
    int node;   // the index of our node

    // for indexing our states
    Index idx;                            // index of states based on hash
    
    // We split the states into semi-states and full states
    // Every !(StatePtr%fmod) is a full state  
    int fmod;    // (1/fmod) * totalstates == fullstates
    BlockMem semi;   // semi states
    BlockMem full;   // full states
}; 

void state_ref(StateSet *ss, Board *bd, StatePtr sp, StateFull *fs);
int state_insert(StateSet *ss, Board *bd, StateFull *fs, StatePtr *sp);
int state_used(StateSet *ss);
void state_init(StateSet *ss, StatePtr num, int fullmod, int npcs, int nnodes);
void state_fini(StateSet *ss);



#endif /* STATE_H */

