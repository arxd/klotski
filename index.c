/**
 * Threading notes
 *   1) the init/fini functions are not thread-safe
 *   2) 
 *
 */
#include <string.h>
#include <pthread.h>
#include "base.h"
#include "index.h"

static inline IndexPtr alloc_BNode(Index *idx)
{
    IndexPtr iptr = bm_alloc(&idx->states);
    if(!iptr)
	DIE("Out of node space\n");
    bm_alloc(&idx->nodes);
    memset( ((BNode*)bm_ref(&idx->nodes, iptr))->hv, 0xFF, sizeof(HashVal)*FANOUT);
    return iptr;
}

void index_init(Index *idx, Iint size)
{
    int i;
    if(size%FANOUT)
	DIE("size must be a multiple of %d", FANOUT);
	
    printf("Index size = %u\n", size);
    bm_init(&idx->nodes, sizeof(BNode), size/FANOUT);
    bm_init(&idx->states, sizeof(StatePtr)*FANOUT, size/FANOUT);
    /* initilize each btree */
    for(i=0; i < HASHTBLSIZE; i++) { 
	pthread_rwlock_init(&idx->locks[i], NULL);
	idx->version[i] = 0;
	alloc_BNode(idx);
    }
}

void index_fini(Index *idx)
{
    int i;
    for(i=0; i < HASHTBLSIZE; i++)
	pthread_rwlock_destroy(&idx->locks[i]);
    bm_fini(&idx->nodes);
    bm_fini(&idx->states);
}

/** allocates a new BNode and returns an Iptr to it
 */
float index_mem(void)
{
    return sizeof(StatePtr) + sizeof(BNode) / (float)FANOUT;
}

/** This tries to upgrade \a lock from RO to RW
 * If idx was modified in the meantime it returns 0 and the lock is unlocked
 */
int index_upgrade_rwlock(Index *idx, int wr, int hashidx, pthread_rwlock_t *lock)
{
    if(wr) // we are already WR
	return 1;
    
    u32 pversion = idx->version[hashidx]; // remember previous version
    pthread_rwlock_unlock(lock); // release our RO lock
    // version might change right now... :-/
    pthread_rwlock_wrlock(lock); // aquire WR lock
    if(idx->version[hashidx] != pversion) {
	// version changed.  abort
	pthread_rwlock_unlock(lock);
	return 0;
    }
    // mark idx as modified
    idx->version[hashidx] ++;
    return 1;
}

/**
 * Search the index for hv.  If an existing entry exists return a pointer to it.
 * Otherwise, create a new entry and return a pointer to it. (*ptr will == 0)
 *   hv: hash value, should not be 0 or ~0
 *   sout: returns a pointer to the StatePtr in this index
 *   lock: this function returns a rwlock through lock
 *         the state of this lock is returned as the return value
 *  
 * Returns:
 *   -1 : Aborted insert (lock is free).  Re-run
 *   0 : lock is RO
 *   1 : lock is RW
 *
 * The first HASHTBLSIZE nodes are the hash table from which btrees spring
 */
int index_ref(Index *idx, HashVal hv, StatePtr **sout, pthread_rwlock_t **lock)
{
    BNode *n;
    u32 ht = hv % HASHTBLSIZE;
    IndexPtr maxip=0;
    IndexPtr ip = ht + 1; // this is our starting node index
    int i;

    // Get a read-only lock to the b-tree we are working in
    *lock = &idx->locks[hv%HASHTBLSIZE];
    pthread_rwlock_rdlock(*lock);
    int wr = 0; // we are RO starting off

    // Walk down the b tree starting at the hashtbl location
    while(1) {
	n = (BNode*)bm_ref(&idx->nodes, ip);

	// Look for correct slot in this node
	for(i=0; i < FANOUT && hv >= n->hv[i]; i++) {
	    if(hv == n->hv[i]) {
		// exiting state found. all RO ;-)
		*sout = (StatePtr*)bm_ref(&idx->states, ip) + i;
		return 0;
	    }
	}

	if(i == FANOUT) {
            // need to modify idx
	    if(!(wr = index_upgrade_rwlock(idx, wr, ht, *lock)))		
		return -1; // we are lockless. just abort
	    // This is the biggest hashval
	    // Swap last with the current hv and pretend FANOUT-1 is the correct slot
	    i--;
	    IndexPtr tmp = hv;
	    hv = n->hv[i];
	    n->hv[i] = tmp;
	    maxip = maxip ?: ip; // remember the first max node
	}

	if(n->hv[FANOUT-1] == ~0) {
	    // need to modify idx
	    if(!(wr = index_upgrade_rwlock(idx, wr, ht, *lock)))
		return -1; // failed to get rw lock. abort
	    // This is a leaf node so put it here and were done
	    int j = FANOUT;
	    StatePtr *sptr = (StatePtr*)bm_ref(&idx->states, ip);
	    while(--j > i) { // must be inserted in sorted order
		n->hv[j] = n->hv[j-1];
		n->ln[j] = n->ln[j-1];
		sptr[j] = sptr[j-1];
	    }
	    // initilize the new node
	    n->hv[j] = hv;
	    n->ln[j] = 0;
	    sptr[j] = 0;
	    break;
	} 
	// The awnser lies somewhere under n->ln[i]
	if(!n->ln[i]) { // We need to create a new node
	    // need to modify idx
	    if(!(wr = index_upgrade_rwlock(idx, wr, ht, *lock))) 		
		return -1; // failed to get lock. abort
	    n->ln[i] = alloc_BNode(idx);
	}

	ip = n->ln[i];
    }
    *sout = (StatePtr*)bm_ref(&idx->states, maxip?:ip) + (maxip?(FANOUT-1):i);

    // as we exit we are locked either RO or RW
    return wr;
}

void bnode_debug(Index *idx, Iptr ni, int depth)
{
    int i;
    BNode *bn = (BNode*)bm_ref(&idx->nodes, ni);
    for(i=0; i < depth; i++)
	printf("\t");
    for(i=0; i < FANOUT; i++) {
	printf("%2d, ",bn->hv[i]+1 ? bn->hv[i] : 0 );
	if(bn->hv[i] != ~0 && bn->ln[i]) {
	    printf("\n");
	    bnode_debug(idx, bn->ln[i], depth+1);
	}
    }
    printf("\n");
}


void index_debug(Index *idx)
{
    printf("Index[%d] used %d / brk %d\n", FANOUT, idx->states.used, idx->states.brk);
    bnode_debug(idx, 1, 0);
}

int traverse(Index *idx, Iptr ni, int prev)
{
    int i;
    BNode *bn = (BNode*)bm_ref(&idx->nodes, ni);
    for(i=0; i < FANOUT && bn->hv[i] != ~0; i++) {
	if(bn->ln[i])
	    prev = traverse(idx, bn->ln[i], i?bn->hv[i-1]:prev);
	
	StatePtr *ptr = (StatePtr*)bm_ref(&idx->states, ni) + i;
	printf("%2d ", *ptr);
    }
    return bn->hv[i-1];
}

int  index_test()
{
/*    int i, z;
    StatePtr *ptr;
    Index idx;
    index_init(&idx, 1024);

    for(i=0; i < 1000; i++) {
	z = rand()%98+1;
	ptr = index_ref(&idx, z);
	*ptr = z;
    }
    printf("\nsorted\n");
    traverse(&idx, 1, 0);
    printf("\n");
    
    
   // index_debug(&idx);


    index_fini(&idx);
  */  return 0;
}


