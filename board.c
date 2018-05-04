/**
 * Requirements of board.k
 *  - First line contains 3 numbers (width height endpos)
 *    endpos is the linear index of the end position
 *
 *  Threading notes
 *    1) the init/fini functions are not thread-safe
 *    2) can_move and fill are safe
 */
#include <stdio.h>
#include <string.h>
#include "list.h"
#include "board.h"

#define BUFSIZE 260
#define TYPE(t) list_el(PieceType, bd->types, t)
#define FRAG(t, f) list_el(u16, TYPE(t).frag, f)

/** Strip trailing whitespace
 */
void strip(char *str)
{
    char *ws=0, pw = 0;
    do {
	if(*str > ' ') {
	    pw = 0;
	} else if(!pw) {
	    ws = str;
	    pw = 1;
	}
    } while(*str++);

    *ws = '\0';
}

/** Find the piece with the smallest @a loc
 * If there are no pieces it returns zero
 */
static int find_piece(Board *bd, u16 *loc)
{
    // find first piece
    for(*loc=0; *loc < bd->w*bd->h; (*loc)++) {
	if((bd->grid[*loc] != 0) && !(bd->grid[*loc]&0x80))
	    break;
    }
    return (*loc != bd->w*bd->h);
}

/** Extract the piece starting at loc and create a new PieceType off it
 */
static void make_type(Board *bd, u16 loc, PieceType *pt)
{
    int i,p = bd->grid[loc], d;
    list_init(&pt->frag, sizeof(u16), 1);
    for(d=0; d<4; d++)
	list_init(&pt->edge[d], sizeof(s16), 1);
    // find all the fragments of p
    for(i=loc; i <= bd->w*bd->h; i++) {
	if(bd->grid[i] == p) {
	    list_push(u16, pt->frag) = i - loc;
	    // create edges of movement
	    for(d=0; d<4; d++) {
		if(bd->grid[i + bd->dir[d]] != p) // can move in direction d
		    list_push(s16, pt->edge[d]) = (i+bd->dir[d]) - loc;
	    }
	}
    }
}

/** Removes the piece that starts at @a loc.
 */
static void remove_piece(Board *bd, u16 loc)
{
    int p = bd->grid[loc];
    do {
	if(bd->grid[loc] == p)
	    bd->grid[loc] = 0;
    } while(++loc < bd->w*bd->h);
}

/** Compare two PieceTypes for equality
 */
int type_eq(PieceType *t1, PieceType *t2)
{
    int i;
    if(t1->frag.length != t2->frag.length)
	return 0;
    for(i=0; i < t1->frag.length; i++)
	if(list_el(u16, t1->frag, i) != list_el(u16, t2->frag, i))
	    return 0;
    return 1;
}

static void free_PieceType(PieceType *pt)
{
    int d;
    list_fini(&pt->frag);
    for(d=0; d < 4; d++)
	list_fini(&pt->edge[d]);
}



/** Make sure bd is freed before it is inited
 * File format is
 * 1: <width> <height> <endpos>
 * 2: FFFFFFFF
 * 3: FF0102FF
 * 4: etc...
 * 5: <blank line>
 */
void board_init(Board *bd, FILE *stream)
{
    char buf[BUFSIZE];
    PieceType pt;
    int i,j;
    u16 loc;
    struct s_p {PieceType type; u16 loc;};
    List pcs; // type:struct s_p
    #define PCS(i) (list_el(struct s_p, pcs, i))

    fgets(buf, BUFSIZE, stream), strip(buf);
    bd->name = strdup(buf);
    fgets(buf, BUFSIZE, stream), strip(buf);
    sscanf(buf, "%d %d %hu", &bd->w, &bd->h, &bd->end);
    
    // initilize direction offsets
    bd->dir[DIR_N] = -bd->w;
    bd->dir[DIR_S] = bd->w;
    bd->dir[DIR_W] = -1;
    bd->dir[DIR_E] = 1;

    // Read grid data
    bd->nsp = 0;
    bd->grid = safe_malloc(bd->w * bd->h);
    for(i=0; i < bd->h; i++) {
	fgets(buf, BUFSIZE, stream),strip(buf);
	for(j=0; j < bd->w; j++) {
	    switch(buf[j]) {
		case '#': bd->grid[i*bd->w+j] = 0xFF; break;
		case '-': bd->nsp++; bd->grid[i*bd->w+j] = 0x80; break;
		case ' ': bd->nsp++; bd->grid[i*bd->w+j] = 0x00; break;
		default:  bd->grid[i*bd->w+j] = buf[j]; break;
	    }
	}
    }

    // turn all the pieces into a class
    list_init(&pcs, sizeof(struct s_p), 1); 
    while(find_piece(bd, &loc)) {
	make_type(bd, loc, &pt);
	struct s_p sp = {pt, loc};
	if(bd->grid[loc] == '*') {
	    // the main piece must be first
	    list_insert(&pcs, 1, 0);
	    list_head(struct s_p, pcs) = sp;
	} else {
	    list_push(struct s_p, pcs) = sp;
	}
	remove_piece(bd, loc);
    }
 
    // the pieces are already sorted by their loc
    // but they need to be clumped by type
    for(i=1; i < pcs.length; i++) {
	// remove the tail piece
	struct s_p sp = PCS(pcs.length-1);
	list_remove(&pcs, 1, pcs.length-1);
	// search through inserted elements for one with the same type
	j = 1;
	while(j < i && !(type_eq(&PCS(j).type, &sp.type)))
	    j++;
	// insert sp at j
	list_insert(&pcs, 1, j);
	PCS(j) = sp;
    }
 
    // set up piece classes
    list_init(&bd->types, sizeof(PieceType), 1);
    list_push(PieceType, bd->types) = PCS(0).type; // always unique
    for(i=1; i < pcs.length; i++) {
	if(i==1 || !type_eq(&PCS(i).type, &list_tail(PieceType, bd->types))) {
	    // this is a unique type
	    list_tail(PieceType, bd->types).last = i-1; 
	    list_push(PieceType, bd->types) = PCS(i).type;
	} else { // not unique
	    free_PieceType(&PCS(i).type);
	}
    }
    list_tail(PieceType, bd->types).last = i-1;

    // initilize pcs
    bd->npcs = pcs.length;
    bd->pcs = safe_malloc(sizeof(u16) * bd->npcs);
    for(i=0; i < bd->npcs; i++)
	bd->pcs[i] = PCS(i).loc;

    // cleanup
    #undef PCS
    list_fini(&pcs);
    fclose(stream);
}

/** Don't call on an uninitilized board
 */
void board_fini(Board *bd)
{
    int i;
    safe_free(bd->grid);
    safe_free(bd->pcs);
    safe_free(bd->name);
    for(i=0; i < bd->types.length; i++)
	free_PieceType(&TYPE(i));
    list_fini(&bd->types);
}

/** Check to see if a piece of @a type at @a loc on @a grid
 *  can move in some @a dir
 */
int board_can_move(Board *bd, u8 *grid, int type, u16 loc, int dir)
{
    int i, delta;
    for(i=0; i < TYPE(type).edge[dir].length; i++) {
	delta = list_el(s16, TYPE(type).edge[dir], i);
	if(grid[loc + delta] && !(type==0 && grid[loc+delta]==0x80))
	    return 0;
    }
    return 1;
}

void board_assert_sorted(Board *bd, u16 *pcs)
{
    int i,  t;
    for(i=0, t=0; i < bd->npcs; t += (i==TYPE(t).last), i++) {
	if(i != TYPE(t).last && pcs[i] > pcs[i+1])
	    DIE("not sorted");
    }


}

/**
 * apply the move (fs->ipcd, fs->dir) to fs->pcs
 */
void board_apply_move(Board *bd, u16 *pcs, int ipcs, int dir)
{
    int j, i, t;
    // find type
    for(t=0; t < bd->types.length; t++)
	if(ipcs <= TYPE(t).last)
	    break;

    j = i = ipcs;
    u16 tmp = pcs[i] + bd->dir[dir];
    // resort
    if(j > 0) { // the first one never needs sorting
	// move it right
	while(j <  TYPE(t).last && tmp > pcs[j+1]) {
	    pcs[j] = pcs[j+1];
	    j++;
	}
	// move it left
	while(j > TYPE(t-1).last + 1 && tmp < pcs[j-1]) {
	    pcs[j] = pcs[j-1];
	    j--;
	}
    }
    pcs[j] = tmp;

}

/** Put all the @a pcs into @a grid
 */
void board_fill(Board *bd, u16 *pcs, u8 *grid)
{
    int i, f, t;
    memcpy(grid, bd->grid, bd->w * bd->h);
    for(i=0, t=0; i < bd->npcs; t += (i==TYPE(t).last), i++) {
	// add each fragment of this piece
	for(f=0; f < TYPE(t).frag.length; f++)
	    grid[FRAG(t,f) + pcs[i]] = i+1;
    }
}

void board_debug_state(Board *bd, u16 *pcs)
{
    int i,t;
    for(i=0, t=0; i < bd->npcs; t+=(i==TYPE(t).last), i++) {
	printf("%2d ", pcs[i]);
	if(i == TYPE(t).last)
	    printf("| ");
    }
    printf("\n");
}

