// cube.c: the rolling cube's rules and its checker (cube.h).

#include "cube.h"

#include <stddef.h>

CubeFace cube_face_after(CubeFace f, CubeRoll r) {
    // For each roll, the face that comes up, goes this way, goes down and
    // goes the other way: up -> ahead -> down -> behind -> up.
    static const CubeFace AHEAD[CUBE_ROLLS] = {FACE_NORTH, FACE_EAST, FACE_SOUTH, FACE_WEST};
    static const CubeFace BEHIND[CUBE_ROLLS] = {FACE_SOUTH, FACE_WEST, FACE_NORTH, FACE_EAST};
    CubeFace ahead = AHEAD[r];
    CubeFace behind = BEHIND[r];
    if (f == FACE_UP) return ahead;
    if (f == ahead) return FACE_DOWN;
    if (f == FACE_DOWN) return behind;
    if (f == behind) return FACE_UP;
    return f;
}

int cube_cell_after(int n, int cell, CubeRoll r) {
    int row = cell / n;
    int col = cell % n;
    if (r == CUBE_N) row--;
    if (r == CUBE_S) row++;
    if (r == CUBE_E) col++;
    if (r == CUBE_W) col--;
    if (row < 0 || row >= n || col < 0 || col >= n) return -1;
    return row * n + col;
}

char cube_roll_letter(CubeRoll r) { return "NESW"[r]; }

int cube_roll_of(char c) {
    switch (c) {
    case 'N': return CUBE_N;
    case 'E': return CUBE_E;
    case 'S': return CUBE_S;
    case 'W': return CUBE_W;
    default: return -1;
    }
}

int64_t cube_play(int n, int start_cell, CubeFace start_face, const char *rolls, int k, int *end_cell, CubeFace *end_face, int *halt_at) {
    int cell = start_cell;
    CubeFace face = start_face;
    uint32_t lit = 0;
    int halted = -1;
    for (int i = 0; i < k; i++) {
        int r = cube_roll_of(rolls[i]);
        if (r < 0) return -1;
        if (halted >= 0) continue;
        int to = cube_cell_after(n, cell, (CubeRoll)r);
        if (to < 0) {
            halted = i;
            continue;
        }
        cell = to;
        face = cube_face_after(face, (CubeRoll)r);
        if (face == FACE_DOWN) lit ^= 1u << cell;
    }
    if (end_cell != NULL) *end_cell = cell;
    if (end_face != NULL) *end_face = face;
    if (halt_at != NULL) *halt_at = halted;
    return lit;
}

int cube_check(int n, int start_cell, CubeFace start_face, uint32_t target, const char *rolls, int k) {
    return cube_play(n, start_cell, start_face, rolls, k, NULL, NULL, NULL) == (int64_t)target;
}
