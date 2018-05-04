/**
  cost(FullState from, FullState to);
  can_move
  next_states

goal states:
 - piece type in exact place (the goal of the game)
 - ability to move certain piece in a direction
 - board zen
   - clustered blanks
 - piece flow  

  Finds a set of moves to get from state1 to state2.
get_path(state1, state2)
{
    find a set of moves to get from state one to state two
    if it seems simple enough just do astar
    otherwise find waypoints and recurse
}
*/
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include "list.h"
#include "solver.h"
#include "state.h"


#define TYPE(t) list_el(PieceType, ks->bd.types, t)

/** This calculates a huristic value.
 */
static float state_huristic(Solver *ks, u16 *pcs, u8 *grid)
{
    int i, d, lsp, sp=0, t;
    int dy = (pcs[0] / ks->bd.w) - (ks->bd.end / ks->bd.w);
    int dx = (pcs[0] % ks->bd.w) - (ks->bd.end % ks->bd.w);
    // distance to finish
    float dist = fabs(dx) + fabs(dy);

    // we would like to favor clumped spaces
    // calculate variance of spaces
    for(i=0, t=0; t < ks->bd.nsp; i++) {
	if(grid[i] & 0x7F)
	    continue; // only looking for spaces
	lsp = 0; // number of spaces adjacent to this space
	for(d=0; d < 4; d++) {
	    if(grid[i + ks->bd.dir[d]] == 0)
		lsp ++;
	}
	sp += (1<<lsp); // favor adjacent spaces exponentally
	t++; // counting found spaces
    }
    // nsp <= sp < nsp*16
    float spscore = (float)(sp - ks->bd.nsp) / (ks->bd.nsp*15); // 0.0 - 1.0
    return dist + (1.0 - spscore); // lower is better
}


/** This calculates all adjacent states to @s and puts them in @a adj
 */
void state_adj(Solver *ks, List *adjs, u16 *pcs, u8 *grid, u8 *pmov)
{
    int i, d, t,j;
    StateFull *fs;
    memset(pmov, 0, ks->bd.npcs);

    list_clear(adjs);

    // Find all the spaces and mark adjacent pieces
    for(i=0, t=0; t < ks->bd.nsp; i++) {
	if(grid[i]&0x7F)
	    continue; // we are only looking for spaces
	for(d=0; d<4; d++) { // all 4 directions
	    j = grid[i + ks->bd.dir[d^2]]; // opposite direction
	    if(j!=0 && !(j&0x80)) // not wall or space
		pmov[j-1] |= (1<<d); // mark it for later
	}
	t++; // count found spaces
    }

    // try to move marked pieces
    for(i=0, t=0; i < ks->bd.npcs; t += (i == TYPE(t).last), i++) {
	if(!pmov[i]) // quick abort
	    continue;
	for(d=0; d<4; d++) { // All 4 directions
	    if((pmov[i] & (1<<d)) &&
		    board_can_move(&ks->bd, grid, t, pcs[i], d)) {
		
		// add to the list of adj states
		fs = &listv_push(StateFull, adjs);
		memcpy(fs->pcs, pcs, 2*ks->bd.npcs);
		fs->semi.ipcs = i;
		fs->semi.dir = d;
		board_apply_move(&ks->bd, fs->pcs, i, d);
		board_assert_sorted(&ks->bd, fs->pcs);
	    }
	}
    }
}



StatePtr stall(Solver *ks)
{
    StatePtr sp = 0;
    
    while(sp == 0) {
	pthread_mutex_lock(&ks->alock);
	ks->active_threads--; // take ourselves out of the game
	pthread_mutex_unlock(&ks->alock);
	usleep(1000); // wait on other threads for a bit
	// is the game over?
	pthread_mutex_lock(&ks->alock);
	if(!ks->active_threads) {
	    pthread_mutex_unlock(&ks->alock); 
	    return 0; // all the other threads are done. so are we.
	}
	ks->active_threads ++; // lets keep trying
	pthread_mutex_unlock(&ks->alock);
	// try to pop a state
	sp = queue_pop(&ks->pq);
    }
    // Were back in the game
    return sp;
}

/** Solve via a bfs search
 */
static void *solver_thread(ThreadState *tstate)
{
    int i, ret;
    Solver *ks = tstate->ks;
    StatePtr sp, adjp;
    u8 *grid, *pmov;
    List adjs; // type StateFull
    StateFull *nfs, *cfs = alloca(ks->states.sizeof_full);

    grid = safe_malloc(ks->bd.w * ks->bd.h);
    pmov = safe_malloc(ks->bd.npcs);

    // initilize adjacent states
    list_init(&adjs, ks->states.sizeof_full, 4*ks->bd.nsp);

    // proccess states from the top of the priority queue
    while(1) {
	if(ks->solution)
	    break; // I guess someone else found a solution
	sp = queue_pop(&ks->pq);
	if(!sp) { // others might still be processing so just wait
	    sp = stall(ks);
	    if(!sp) // everyone is done
		break;
	}
	// We got a state so go ahead
	state_ref(&ks->states, &ks->bd, sp, cfs);
	tstate->dpth = cfs->depth;
	// create an intermediate grid for other algorithms to use
	board_fill(&ks->bd, cfs->pcs, grid);
	//get adjacent states
	state_adj(ks, &adjs, cfs->pcs, grid, pmov);	
	// process each adjacent state
	for(i=0; i < adjs.length; i++) {
	    tstate->num++;
	    nfs = &listv_el(StateFull, &adjs, i);
	    nfs->semi.idx_next = 0;
	    nfs->semi.node = ks->states.node;
	    nfs->semi.parent = sp;
	    nfs->depth = cfs->depth+1;
	    while((ret = state_insert(&ks->states, &ks->bd, nfs, &adjp)) < 0)
		tstate->oops++; // just keep trying
	    if(ret > 0) { // dup or sent to different node
		tstate->dup++;
		continue;
	    }
	    // is this a solution?
	    if(nfs->pcs[0] == ks->bd.end) {
		ks->solution = adjp;
	    }
	    // this is a unique state add it to the queue for later processing
	    float dist = state_huristic(ks, nfs->pcs, grid);
	    tstate->dist = dist;
	    queue_push(&ks->pq, adjp, nfs->depth + dist);
	}
    }
    
    list_fini(&adjs);
    free(grid);
    free(pmov);
    
    return NULL;
}

/** Solves the puzzle using nthreads
 */
void solver_solve(Solver *ks, int nthreads)
{
    int i, last=0;
    ThreadState *threads;
    threads = safe_malloc(sizeof(ThreadState) * nthreads);
    ks->active_threads = nthreads;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Start the threads
    for(i=0; i < nthreads; i++) {
	threads[i].i = i;
	threads[i].ks = ks;
	threads[i].num = threads[i].dup = threads[i].oops = 0;
	pthread_mutex_init(&threads[i].lock, NULL);
	pthread_create(&threads[i].thread, &attr, (ThreadMain)solver_thread, (void*)&threads[i]);
    }
    pthread_attr_destroy(&attr);
    printf("depth: states examined / unique states (states, index, queue)\n");
    // wait for the end and print stats
    while(1) {
	pthread_mutex_lock(&ks->alock);
	i = ks->active_threads;
	pthread_mutex_unlock(&ks->alock);
	if(!i || ks->solution) // game is over
	    break;
	// still going
	int num = 0, used = state_used(&ks->states);
	for(i=0; i < nthreads; i++)
	    num += threads[i].num;
	
	printf(" %3d / %0.2f : %0.2f / %0.2f (%.1f, %.1f, %.1f) rate:%0.2f \r",
		threads[0].dpth, threads[0].dist, num/1.0e6, used/1.0e6,
		100.0*used / num,
		100.0*ks->states.idx.nodes.used*FANOUT / num,
		100.0*ks->pq.num / num,	(num-last)*10.0 / 1000.0
		);
	last = num;
	fflush(stdout);
	usleep(100000); // dont print stats too fast
    }
    printf("\n");

    // join back with all the threads
    for(i=0; i < nthreads; i++) {
	void *ret;
	pthread_join(threads[i].thread, &ret);
	pthread_mutex_destroy(&threads[i].lock);
    }

    free(threads);
}

/**
 * Finds \a p in \a pcs.  Returns -1 if not found
 */
int piece_find(u16 p, u16 *pcs, int len)
{
    int i;
    for(i = 0; i < len; i++)
	if(pcs[i] == p)
	    return i;
    return -1;
}

/** Resuffle perm the way state p1 went to p2
 * return the piece that moved | the direction it moved
 */
Move state_diff_move(Board *bd, u16 *p1, u16 *p2, u16 *perm)
{
    int i,j;
    Move mv = {0};
    u16 *tmp = safe_malloc(2*bd->npcs);

    for(i = 0; i < bd->npcs; i++) {
	j = piece_find(p1[i], p2, bd->npcs);
	if(j < 0) {
	    mv.piece = perm[i];
	    // perm[i] is the piece that moved. where to?
	    for(j=0; j < bd->npcs; j++)
		if(piece_find(p2[j], p1, bd->npcs) < 0)
		    break;
	    // which direction did it move?
	    int diff = p2[j] - p1[i];
	    if(diff == 1)           mv.dir = DIR_E;
	    else if(diff == -1)     mv.dir = DIR_W;
	    else if(diff == bd->w)  mv.dir = DIR_S;
	    else if(diff == -bd->w) mv.dir = DIR_N;
	}
	// perm i -> j
	tmp[j] = perm[i];
    }

    // tmp is the new perm
    for(i = 0; i < bd->npcs; i++)
	perm[i] = tmp[i];
    
    free(tmp);

    return mv;
}


static void _sol_seq(Solver *ks, u16 *perm, StatePtr sp, StatePtr psp, List *seq)
{
    if(!sp) {
	// init to straight permutation
	int i;
	for(i=0; i < ks->bd.npcs ; i++)
	    perm[i] = i;
    } else {
	StateFull *fs = alloca(ks->states.sizeof_full);
	StateFull *ps = alloca(ks->states.sizeof_full);
	state_ref(&ks->states, &ks->bd, sp, fs);
	state_ref(&ks->states, &ks->bd, psp, ps);
	_sol_seq(ks, perm, fs->semi.parent, sp, seq);
	listp_push(Move, seq) = state_diff_move(&ks->bd, fs->pcs, ps->pcs, perm);
    }
}

/**
 * Pass a state to backtrace and an empty initilized List of type Move
 */
void solver_make_sequence(Solver *ks, StatePtr sp, List *seq)
{
    StateFull *fs = alloca(ks->states.sizeof_full);
    u16 *perm = alloca(2*ks->bd.npcs);
    state_ref(&ks->states, &ks->bd, sp, fs);
    _sol_seq(ks, perm, fs->semi.parent, sp, seq);
}

void solver_init(Solver *ks, Board bd, Iint nstates)
{
    memset(ks, 0, sizeof(Solver));

    // set up data structures
    ks->bd = bd;
    // init states
    state_init(&ks->states, nstates, 8, bd.npcs, 1);
    // init priority queue
    queue_init(&ks->pq, QFANOUT*(nstates*0.5/QFANOUT));
    pthread_mutex_init(&ks->alock, NULL);

    // add the initial state
    StateFull *fs = alloca(ks->states.sizeof_full);
    memset(fs, 0, ks->states.sizeof_full);
    memcpy(fs->pcs, bd.pcs, 2*bd.npcs);
    state_insert(&ks->states, &ks->bd, fs, &ks->root);
    ks->solution = 0;
    queue_push(&ks->pq, ks->root, 0);
}

void solver_fini(Solver *ks)
{
    // sv->bd is freed by parent
    state_fini(&ks->states);
    queue_fini(&ks->pq);
    pthread_mutex_destroy(&ks->alock);
}

#undef TYPE



