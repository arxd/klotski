#ifndef TYPES_H
#define TYPES_H

#define CACHE_LINE 64
#define FANOUT 8
#define HASHTBLSIZE 1023

typedef unsigned int u32;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;

typedef u32 Iptr;         // points into BlockMem
typedef Iptr Iint;        // same size as Iptr but its a number
typedef Iptr IndexPtr;    // points into Index::nodes/states
typedef Iptr StatePtr;    // points into States

typedef void*(*ThreadMain)(void *);

typedef u32 HashVal;

typedef struct s_State State;
typedef struct s_Solver Solver;
typedef struct s_BlockMem BlockMem;
typedef struct s_Index Index;
typedef struct s_PQueue Queue;
typedef struct s_Board Board;
typedef struct s_PieceType PieceType;

#endif

