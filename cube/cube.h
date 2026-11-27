// cube.h: the rolling cube, the third toy machine.
//
// One die-sized cube sits on an n x n grid of tiles, every tile dark at the
// start. A roll tips the cube over one of its bottom edges into the cell
// next to it: N, E, S or W, two bits a roll. A roll off the grid is refused.
// One face of the cube is painted, and each time the cube lands with the
// painted face down, the tile under it flips, dark to lit or lit to dark.
//
// A level is n, the cube's start cell and orientation, and the set of tiles
// that must be lit once the program ends, every other tile dark. The cube may
// end anywhere. The certificate for "this level is solved in k rolls" is the
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

// Rolls `rolls` (as letters, `k` of them) from the start and gives the
// pattern of lit tiles, or -1 when a roll leaves the grid. *end_cell and
// *end_face, when not NULL, get where the cube stopped.
int64_t cube_play(int n, int start_cell, CubeFace start_face, const char *rolls, int k, int *end_cell, CubeFace *end_face);

// The checker: 1 when the rolls stay on the grid and end with exactly
// `target` lit, else 0.
int cube_check(int n, int start_cell, CubeFace start_face, uint32_t target, const char *rolls, int k);

#endif
