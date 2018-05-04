#include <stdio.h>
#include <time.h>
#include "solver.h"
#include "queue.h"
#include "board.h"
#include "list.h"

#define Mb (1024*1024L)
#define Gb (1024*Mb)

void run_tests(void)
{
    clock_t t1,t2;
    int e;
    LOG_INFO("Testing queue...");
    t1 = clock();
    e = queue_test();
    t2 = clock();
    if(e) LOG_INFO("%d\n", e); else LOG_INFO("passed in %es\n", (t2-t1)/(double)CLOCKS_PER_SEC);

    index_test();
}

void write_json(Solver *ks, List *seq, FILE *stream)
{
    int r,c;
    u8 *grid = safe_malloc(ks->bd.w * ks->bd.h);

    board_fill(&ks->bd, ks->bd.pcs, grid);

    fprintf(stream, "{\"name\":\"%s\",\"grid\":[", ks->bd.name);
    for(r=0; r < ks->bd.h; r++) {
	fprintf(stream, "%s[", r?",":"");
	for(c=0; c < ks->bd.w; c++) 
	    fprintf(stream, "%s%d", c?",":"", grid[r*ks->bd.w+c]);
	fprintf(stream, "]");
    }
    
    fprintf(stream,"],\"end\":%d,\"solution\":[", ks->bd.end);
    if(seq) {
	for(r = 0; r < seq->length; r++) {
	    fprintf(stream, "%s[%d,%d]", r?",":"", listp_el(Move,seq,r).piece+1, listp_el(Move,seq,r).dir);
	}
    }
    fprintf(stream,"]}"); 
    free(grid);
}


int main(int argc, char *argv[])
{
    Solver ks;
    Board bd;
    char filename[256];
    long nstates, nthreads;
    FILE *file;

    set_log_level(LOG_LEVEL);
    //LOG_INFO("TESTING:\n");
    //run_tests();

    if(argc < 4)
	DIE("Usage:\n\t- the name of a puzzle\n\t- the number of states\n\t- the number of threads");

    nstates = strtol(argv[2], 0, 10) * 1024*1024;
    nthreads = strtol(argv[3], 0, 10);
    snprintf(filename, 256, "boards/%s.k", argv[1]);
    if(!(file = fopen(filename, "r")))
	DIE("Can't open file \'%s\'\n", filename);
    board_init(&bd, file);
    printf("%d pieces %d types %d spaces\n", bd.npcs, bd.types.length, bd.nsp); 
   
    // Calculate  nstates = mem / (index_mem + state_mem + ...)
    solver_init(&ks, bd, nstates);
    
    write_json(&ks, NULL, stdout);
    printf("\n\n");

    
    solver_solve(&ks, nthreads);
    
    if(ks.solution) {
	// solution found 
	List seq;
	list_init(&seq, sizeof(Move), 10);
	solver_make_sequence(&ks, ks.solution, &seq);
	write_json(&ks, &seq, stdout);
	list_fini(&seq);
    } else {
	// no solution found
	printf("No solution found in %d states\n", ks.states.full.used);
    }
    printf("\n");
    solver_fini(&ks);
    board_fini(&bd);
    return 0;
}

