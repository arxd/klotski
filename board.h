#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include "list.h"

#define DIR_N  0
#define DIR_E  1
#define DIR_S  2
#define DIR_W  3

struct s_PieceType {
    List frag;    // type:u16 fragments of piece
    List edge[4]; // type:s16 piece edges, test locations 
    int last;     // the index into a pcs array of the last piece of this type
};

struct s_Board {
    int w, h, npcs, nsp;
    char *name;
    List types;     // type:PieceType   the first piece type is the main piece
    int dir[4];     // direction offsets for moving in grid
    u16 end;        // position for end condition
    u8 *grid;       // blank grid with just walls
    u16 *pcs;      // initial state size=npcs
};

void board_init(Board *bd, FILE *stream);
void board_fini(Board *bd);
void board_fill(Board *bd, u16 *pcs, u8 *grid);
int board_can_move(Board *bd, u8 *grid, int type, u16 loc, int dir);
void board_assert_sorted(Board *bd, u16 *pcs);
void board_apply_move(Board *bd, u16 *pcs, int ipcs, int dir);
void board_debug_state(Board *bd, u16 *pcs);

#endif

