// cube.h: the rolling cube, the third toy machine.
//
// One die-sized cube sits on an n x n grid of tiles, every tile dark at the
// start. A roll tips the cube over one of its bottom edges into the cell
// next to it: N, E, S or W, two bits a roll. A roll off the grid halts the
// machine: the cube stays where it was, the tiles lit so far are the
// program's set, and the rolls after it do nothing.
// One face of the cube is painted, and each time the cube lands with the
// painted face down, the tile under it flips, dark to lit or lit to dark.
//
// A level is n, the cube's start cell and orientation, and the set of tiles
// that must be lit once the program ends (or halts), every other tile dark.
// The cube may end anywhere. The certificate for "this level is solved in k rolls" is the
// roll sequence, and the checker rolls it and compares.
//
// The cube has 24 orientations, but only the painted face touches the tiles,
// and where that face goes on a roll depends on where it is alone. So the
// state the rules need is where the painted face points: one of six.
//
// Cells are named as on a chessboard turned to the north: columns a, b, c,
// ... west to east, rows 1, 2, 3, ... north to south, so a1 is the
// north-west corner. In the code, cell r * n + c is row r + 1 (r = 0 the
// north row) and column c (0 the west column, a). Tile i of a pattern is
// bit i.

#ifndef COOK_LEVIN_CUBE_H
#define COOK_LEVIN_CUBE_H

#include <stdint.h>

// The largest grid the CNF takes; the census takes 4 x 4 at most (16 tiles,
// 65,536 patterns), since 5 x 5 has 5 billion states.
enum { CUBE_MAX_N = 5 };

// The 4 x 4 language is finite: a program is at most CUBE_L rolls, as a
// Nibble program is six lines. 45 is the least length that decides every lit
// set but one: every tile lit takes 47 rolls (the census, cube/README.md).
enum { CUBE_L = 45 };

// The rolls, by their two-bit codes.
typedef enum { CUBE_N = 0, CUBE_E = 1, CUBE_S = 2, CUBE_W = 3 } CubeRoll;

// Where the painted face points.
typedef enum { FACE_UP = 0, FACE_DOWN = 1, FACE_NORTH = 2, FACE_SOUTH = 3, FACE_EAST = 4, FACE_WEST = 5 } CubeFace;

enum { CUBE_ROLLS = 4, CUBE_FACES = 6 };

// The painted face after a roll. Rolling north tips the cube over its north
// bottom edge: the top goes north, north goes down, the bottom goes south,
// and south comes up; east and west stay. The other three rolls alike.
CubeFace cube_face_after(CubeFace f, CubeRoll r);

// The cell a roll moves to, or -1 for a roll off the grid.
int cube_cell_after(int n, int cell, CubeRoll r);

// The letter of a roll, "NESW"[r], and the roll of a letter, -1 for none.
char cube_roll_letter(CubeRoll r);
int cube_roll_of(char c);

// The machine between rolls: the cube's cell and painted face, the lit
// tiles, and whether a roll off the grid has halted it.
typedef struct {
    int cell;
    CubeFace face;
    uint32_t lit;
    int halted;
} CubeState;

// One roll of the machine, in place: a halted machine stays as it is; a roll
// off the grid halts it where it stands; any other roll moves the cube and
// turns its face, and flips the tile it lands on when the face lands down.
void cube_step(int n, CubeState *s, CubeRoll r);

// Rolls `rolls` (as letters, `k` of them) from the start and gives the
// pattern of lit tiles, or -1 for a letter that is not a roll. A roll off
// the grid halts the run there. *end_cell and *end_face, when not NULL, get
// where the cube stopped; *halt_at, when not NULL, gets the number of rolls
// before the one that halted it, or -1 when none did.
int64_t cube_play(int n, int start_cell, CubeFace start_face, const char *rolls, int k, int *end_cell, CubeFace *end_face, int *halt_at);

// The checker: 1 when the rolls end (or halt) with exactly `target` lit,
// else 0. It takes any length; CUBE_L is the 4 x 4 language's bound.
int cube_check(int n, int start_cell, CubeFace start_face, uint32_t target, const char *rolls, int k);

#endif
