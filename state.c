/** \file state.c
 *
 * 2.8Gb  2m40.2s
 *
 */
#include <string.h>
#include "base.h"
#include "state.h"
#include "mem.h"


unsigned int APHash(char* str, unsigned int len)
{
   unsigned int hash = 0;
   unsigned int i    = 0;

   for(i = 0; i < len; str++, i++)
   {
      hash ^= ((i & 1) == 0) ? (  (hash <<  7) ^ (*str) ^ (hash >> 3)) :
                               (~((hash << 11) ^ (*str) ^ (hash >> 5)));
   }

   return hash;
}


/** Create a 32 bit hash from a state
 */
HashVal state_hash(u16 *pcs, int len)
{
    HashVal h = APHash((char*)pcs, len*2);
    if(h == 0 || h == ~0)
	return 1;
    return h;
}

/** Compare two states for equality
 */
int state_eq(u16 *s1, u16 *s2, int len)
{
    int i;
    for(i=0; i < len; i++)
        if(s1[i] != s2[i])
            return 0;
    return 1;
}

StatePtr new_full(StateSet *ss, StateFull *state)
{
    StatePtr fsp;
    StateFull *sf;
    fsp = bm_alloc(&ss->full);
    if(fsp == 0)
	DIE("Out of full node space");
    sf = (StateFull*)bm_ref(&ss->full, fsp);
    memcpy(sf, state, ss->sizeof_full);
    return fsp * ss->fmod;
}

StatePtr new_semi(StateSet *ss, StateFull *state)
{
    StatePtr sp;
    StateSemi *s;
    sp = bm_alloc(&ss->semi);
    if(sp == 0)
	DIE("Out of semi-node space");
    s = (StateSemi*)bm_ref(&ss->semi, sp);
    memcpy(s, &state->semi, sizeof(StateSemi));
    return (StatePtr)(((unsigned long long)(sp-1) * ss->fmod) / (ss->fmod-1)) + 1;
}

StateSemi *state_ref_semi(StateSet *ss, StatePtr sp)
{
    if(sp % ss->fmod) {
	return (StateSemi*)bm_ref(&ss->semi, sp - sp / ss->fmod);
    } else {
	return (StateSemi*)bm_ref(&ss->full, sp / ss->fmod);
    }
}

/**
 * Inserts \a state into the pool of states.
 * All fields of \a state should be set.  Set idx_next to zero.
 * This function is tied closely with the index.  (it passes locks back and forth)
 *
 * RETURN:
 *  -1: Error.  Re-run
 *   0: State unique and inserted
 *   1: State is a duplicate
 *   2: Sent state to another node
 */
int state_insert(StateSet *ss, Board *bd, StateFull *state, StatePtr *spret)
{
    HashVal hv = state_hash(state->pcs, ss->npcs); // the all-important hash of this state
    int wr; // is our lock RW or RO?
    pthread_rwlock_t *lock; // the index lock
    u16 *tpcs, *spcs;
    StatePtr *sp;
    StateSemi *semi;
    StateFull *fs = alloca(ss->sizeof_full);
    // we need some temp piece mem for calculating pcs from semi-states
    tpcs = alloca(2*ss->npcs);

    // Does this state belong on this node?
    if(hv % ss->nnodes != ss->node) {
	// Send this state to another node and tell it where its from (us)
	DIE("Nodes not implemented");
	return 2;
    }

    // Use our index to find a small chain of possibly equal states
    wr = index_ref(&ss->idx, hv, &sp, &lock);
    if(wr < 0) // failed to aquire rw lock.  Abort
	return -1;

    // follow the chain of states that all hash to hv
    while(*sp) {
	state_ref(ss, bd, *sp, fs);
	semi = state_ref_semi(ss, *sp);

	// we now have spcs
	if(state_eq(state->pcs, fs->pcs, ss->npcs)) {
	    // A full read-only search :-)
	    if(state->depth < fs->depth) {
		//DIE("depth short circuit!");
		ss->shorterr++;
		// this state is better. (dosen't happen often)
		/*pthread_mutex_lock(&ks->alock);
		s->depth = depth;
		s->parent = parent;
		pthread_mutex_unlock(&ks->alock);
		*/
	    }
	    pthread_rwlock_unlock(lock);
	    return 1;
	}
	sp = &semi->idx_next;
    }
    // this is a unique state so add it (need RW to modify index)
    if(!index_upgrade_rwlock(&ss->idx, wr, hv%HASHTBLSIZE, lock))
	return -1; // rw lock contention
    // We need to decide if we are creating a full-state or semi-state
    // Assume a semi-state and upgrade to full-state if nessicary
    
    if(state->semi.parent == 0 || !(hv%ss->fmod)) {
	*sp = new_full(ss, state);
    } else {
	*sp = new_semi(ss, state);
    }
       
    *spret = *sp;
    // unlock our lock and return
    pthread_rwlock_unlock(lock);
    return 0;
}

void state_ref(StateSet *ss, Board *bd, StatePtr sp, StateFull *fs)
{
    StateSemi *s = state_ref_semi(ss, sp);

    if(sp%ss->fmod) {
	state_ref(ss, bd, s->parent, fs);
	fs->depth++;
	memcpy(&fs->semi, s, sizeof(StateSemi));
	// now step the pieces forward
	board_apply_move(bd, fs->pcs, s->ipcs, s->dir);
    } else {
	memcpy(fs, (StateFull*)s, ss->sizeof_full);
    }
}

int state_used(StateSet *ss)
{
    return ss->full.used + ss->semi.used;
}

void state_init(StateSet *ss, Iint num, int full_fraction, int npcs, int nnodes)
{
    if(num%full_fraction!=0)
	DIE("num must be multiple of full_fraction");
    
    ss->npcs = npcs;
    ss->sizeof_full = sizeof(StateFull) + 2*npcs;
    ss->shorterr = 0;

    // initilize index
    index_init(&ss->idx, FANOUT*(num*2.0/FANOUT));
    
    // initilize MPI (not implemented yet)
    ss->nnodes = nnodes;
    ss->node = 0;

    // initilize state mem
    ss->fmod = full_fraction;
    bm_init(&ss->full, ss->sizeof_full, num / full_fraction);
    bm_init(&ss->semi, sizeof(StateSemi), num - ss->full.num);
}

void state_fini(StateSet *ss)
{
    bm_fini(&ss->full);
    bm_fini(&ss->semi);
    index_fini(&ss->idx);
}


