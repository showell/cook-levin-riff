// census.c: the rolling cube's census, the par of every target pattern.
//
// A breadth-first search over the cube's states from one start: a state is
// the cube's cell, where its painted face points (cube.h: six, not 24) and
// the pattern of lit tiles, so an n x n grid has n^2 * 6 * 2^(n^2) of them,
// 6,291,456 at n = 4. Each is kept once, in a byte array indexed by the state
// itself, at the level that first reaches it. A pattern's par, the fewest
// rolls that end with exactly it lit, is the first level holding a state with
// that pattern, since the cube may end anywhere.
//
// A roll off the board halts the machine, the tiles lit so far its set. The
// search keeps, for each pattern, the first level at which a halted program
// has it, and checks that halting changes no par: a halted program's set is
// its prefix's, which is shorter. On 4 x 4 the report adds the language's
// bound, CUBE_L (cube.h), and the sets no program within it decides.
//
//   census N              the report for an N x N grid, N 3 or 4, from the
//                         north-west corner with the painted face up
//   census N PATH         the report, and every pattern's par and a
//                         shortest solution, a line a pattern, written to
//                         PATH (cube/oracle.py reads it)
//   census N starts       one line for every start (cell, painted face)
//
// The report's levels and solutions follow the search's order: a state's
// rolls are tried N, E, S, W, so the solution given is the first shortest
// one in that order.

#include "cube.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UNSEEN = 255, MAX_PAR = 254 };

typedef struct {
    int n;
    int tiles;
    int start_cell;
    CubeFace start_face;
    uint32_t patterns;   // 2^tiles
    uint32_t states;     // tiles * 6 * patterns
    uint8_t *level;      // per state: its level, or UNSEEN
    uint8_t *roll;       // per state: the roll that reached it
    uint32_t *queue;     // the states in the order found
    uint32_t found;      // states reached
    uint8_t *par;        // per pattern: its par, or UNSEEN
    uint32_t *first;     // per pattern: the first state with it
    uint8_t *halt;       // per pattern: the first level a halted program has it, or UNSEEN
    int levels;          // the deepest level reached
} Census;

static void *must_alloc(size_t bytes) {
    void *p = malloc(bytes);
    if (p == NULL) {
        fprintf(stderr, "census: out of memory for %zu bytes\n", bytes);
        exit(2);
    }
    return p;
}

static uint32_t state_of(const Census *c, int cell, CubeFace face, uint32_t pattern) {
    return ((uint32_t)(cell * CUBE_FACES + (int)face) << c->tiles) | pattern;
}

static int cell_of(const Census *c, uint32_t s) { return (int)(s >> c->tiles) / CUBE_FACES; }
static CubeFace face_of(const Census *c, uint32_t s) { return (CubeFace)((int)(s >> c->tiles) % CUBE_FACES); }
static uint32_t pattern_of(const Census *c, uint32_t s) { return s & (c->patterns - 1); }

static void census_run(Census *c, int n, int start_cell, CubeFace start_face) {
    c->n = n;
    c->tiles = n * n;
    c->start_cell = start_cell;
    c->start_face = start_face;
    c->patterns = 1u << c->tiles;
    c->states = (uint32_t)c->tiles * CUBE_FACES * c->patterns;
    c->level = must_alloc(c->states);
    c->roll = must_alloc(c->states);
    c->queue = must_alloc((size_t)c->states * sizeof(uint32_t));
    c->par = must_alloc(c->patterns);
    c->first = must_alloc((size_t)c->patterns * sizeof(uint32_t));
    c->halt = must_alloc(c->patterns);
    memset(c->level, UNSEEN, c->states);
    memset(c->par, UNSEEN, c->patterns);
    memset(c->halt, UNSEEN, c->patterns);

    // The start: nothing lit, and nothing flips, since the cube has not
    // landed.
    uint32_t s0 = state_of(c, start_cell, start_face, 0);
    c->level[s0] = 0;
    c->queue[0] = s0;
    c->found = 1;
    c->par[0] = 0;
    c->first[0] = s0;
    c->levels = 0;

    for (uint32_t head = 0; head < c->found; head++) {
        uint32_t s = c->queue[head];
        int d = c->level[s] + 1;
        if (d > MAX_PAR) {
            fprintf(stderr, "census: a level past %d\n", MAX_PAR);
            exit(2);
        }
        int cell = cell_of(c, s);
        CubeFace face = face_of(c, s);
        uint32_t pattern = pattern_of(c, s);
        for (int r = 0; r < CUBE_ROLLS; r++) {
            int to = cube_cell_after(n, cell, (CubeRoll)r);
            if (to < 0) {
                // The roll halts the machine with `pattern` lit.
                if (c->halt[pattern] == UNSEEN) c->halt[pattern] = (uint8_t)d;
                continue;
            }
            CubeFace f = cube_face_after(face, (CubeRoll)r);
            uint32_t p = f == FACE_DOWN ? pattern ^ (1u << to) : pattern;
            uint32_t t = state_of(c, to, f, p);
            if (c->level[t] != UNSEEN) continue;
            c->level[t] = (uint8_t)d;
            c->roll[t] = (uint8_t)r;
            c->queue[c->found++] = t;
            if (d > c->levels) c->levels = d;
            if (c->par[p] == UNSEEN) {
                c->par[p] = (uint8_t)d;
                c->first[p] = t;
            }
        }
    }
}

static void census_free(Census *c) {
    free(c->level);
    free(c->roll);
    free(c->queue);
    free(c->par);
    free(c->first);
    free(c->halt);
}

// The roll that undoes a roll.
static CubeRoll back(CubeRoll r) { return (CubeRoll)((r + 2) % CUBE_ROLLS); }

// A shortest solution for a reached pattern, as letters, into `out` (room
// for its par and a NUL); gives its length.
// The rolls that first reach state `s`, as letters, into `out`; gives
// their count.
static int path_to(const Census *c, uint32_t s, char *out) {
    int k = c->level[s];
    out[k] = '\0';
    for (int i = k - 1; i >= 0; i--) {
        CubeRoll r = (CubeRoll)c->roll[s];
        out[i] = cube_roll_letter(r);
        int cell = cell_of(c, s);
        CubeFace face = face_of(c, s);
        uint32_t p = pattern_of(c, s);
        if (face == FACE_DOWN) p ^= 1u << cell;
        int from = cube_cell_after(c->n, cell, back(r));
        CubeFace f = cube_face_after(face, back(r));
        s = state_of(c, from, f, p);
    }
    if (s != state_of(c, c->start_cell, c->start_face, 0)) {
        fprintf(stderr, "census: a path does not lead back to the start\n");
        exit(2);
    }
    return k;
}

static int solution(const Census *c, uint32_t pattern, char *out) {
    int k = path_to(c, c->first[pattern], out);
    if (!cube_check(c->n, c->start_cell, c->start_face, pattern, out, k)) {
        fprintf(stderr, "census: the checker refuses the solution of pattern %u: %s\n", pattern, out);
        exit(2);
    }
    return k;
}

static int popcount(uint32_t x) {
    int k = 0;
    for (; x != 0; x &= x - 1) k++;
    return k;
}

static const char *FACE_NAMES[CUBE_FACES] = {"up", "down", "north", "south", "east", "west"};

// A pattern as the grid, `#` lit and `.` dark, each row indented.
static void print_grid(FILE *out, int n, uint32_t pattern) {
    for (int r = 0; r < n; r++) {
        fprintf(out, "    ");
        for (int col = 0; col < n; col++) fputc(pattern >> (r * n + col) & 1 ? '#' : '.', out);
        fputc('\n', out);
    }
}

static void print_level(FILE *out, const Census *c, const char *name, uint32_t pattern) {
    char sol[MAX_PAR + 1];
    if (c->par[pattern] == UNSEEN) {
        fprintf(out, "  %s: unreachable\n", name);
    } else {
        int k = solution(c, pattern, sol);
        fprintf(out, "  %s: par %d, %s\n", name, k, k == 0 ? "(no rolls)" : sol);
    }
    print_grid(out, c->n, pattern);
}

static void report(FILE *out, const Census *c) {
    int n = c->n;
    fprintf(out, "cube %dx%d: start at a1 (the north-west corner), painted face %s; the cube may end anywhere\n", n, n, FACE_NAMES[c->start_face]);
    fprintf(out, "states: %u reached of %u (cell, painted face, pattern), %d levels\n", c->found, c->states, c->levels);

    long at[MAX_PAR + 1] = {0};
    uint32_t reached = 0;
    int top = 0;
    for (uint32_t p = 0; p < c->patterns; p++) {
        if (c->par[p] == UNSEEN) continue;
        at[c->par[p]]++;
        reached++;
        if (c->par[p] > top) top = c->par[p];
    }
    fprintf(out, "patterns: %u, reachable %u, unreachable %u\n", c->patterns, reached, c->patterns - reached);

    // Halting: the par with halted programs counted is the least of the two
    // levels, and it must be the par without them.
    uint32_t halted = 0, same = 0;
    for (uint32_t p = 0; p < c->patterns; p++) {
        int with = c->par[p];
        if (c->halt[p] != UNSEEN) {
            halted++;
            if (c->halt[p] < with) with = c->halt[p];
        }
        if (with == c->par[p]) same++;
    }
    if (same != c->patterns) {
        fprintf(stderr, "census: halting changes %u pars\n", c->patterns - same);
        exit(2);
    }
    fprintf(out, "halting: a roll off the board halts the machine; %u patterns are a halted program's, and every pattern's par is the same with halting as without (%u of %u)\n", halted, same, c->patterns);
    fprintf(out, "\npar  patterns\n");
    for (int d = 0; d <= top; d++) fprintf(out, "%3d  %8ld\n", d, at[d]);

    // Par by how many tiles are lit.
    fprintf(out, "\nlit  patterns  reachable  min par  max par  mean par\n");
    for (int k = 0; k <= c->tiles; k++) {
        long count = 0, hit = 0, sum = 0;
        int lo = MAX_PAR + 1, hi = -1;
        for (uint32_t p = 0; p < c->patterns; p++) {
            if (popcount(p) != k) continue;
            count++;
            if (c->par[p] == UNSEEN) continue;
            hit++;
            sum += c->par[p];
            if (c->par[p] < lo) lo = c->par[p];
            if (c->par[p] > hi) hi = c->par[p];
        }
        if (hit == 0) {
            fprintf(out, "%3d  %8ld  %9ld        -        -         -\n", k, count, hit);
        } else {
            fprintf(out, "%3d  %8ld  %9ld  %7d  %7d  %8.2f\n", k, count, hit, lo, hi, (double)sum / (double)hit);
        }
    }

    // The floor: the first landing face down takes 2 rolls (up, a side,
    // down), and the next one on another tile 3 more, since from a side
    // face only one roll brings it down, and two rolls down to down return
    // to the same tile. So k lit tiles take at least 3k - 1 rolls.
    long over[MAX_PAR + 1] = {0};
    int over_top = 0;
    for (uint32_t p = 1; p < c->patterns; p++) {
        if (c->par[p] == UNSEEN) continue;
        int e = c->par[p] - (3 * popcount(p) - 1);
        if (e < 0) {
            fprintf(stderr, "census: pattern %u has par %d, under the floor\n", p, c->par[p]);
            exit(2);
        }
        over[e]++;
        if (e > over_top) over_top = e;
    }
    fprintf(out, "\nrolls over the floor 3 * lit - 1, for the patterns with a tile lit\nover  patterns\n");
    for (int e = 0; e <= over_top; e++) fprintf(out, "%4d  %8ld\n", e, over[e]);
    for (uint32_t p = 1; p < c->patterns; p++) {
        if (c->par[p] != UNSEEN && c->par[p] - (3 * popcount(p) - 1) == over_top) {
            fprintf(out, "\nthe most over the floor, %d, has %ld pattern(s); the first:\n", over_top, over[over_top]);
            print_level(out, c, "farthest over", p);
            break;
        }
    }

    // Each tile lit alone, as the grid of its pars.
    fprintf(out, "\nthe par of each tile lit alone\n");
    for (int r = 0; r < n; r++) {
        fprintf(out, "   ");
        for (int col = 0; col < n; col++) fprintf(out, " %3d", c->par[1u << (r * n + col)]);
        fputc('\n', out);
    }

    // The macros: back on the start cell, every tile dark, the painted face
    // turned elsewhere.
    fprintf(out, "\nback at the start cell with every tile dark, the painted face turned\nface   rolls  the first shortest\n");
    for (int f = 0; f < CUBE_FACES; f++) {
        if (f == (int)c->start_face) continue;
        uint32_t s = state_of(c, c->start_cell, (CubeFace)f, 0);
        char path[MAX_PAR + 1];
        if (c->level[s] == UNSEEN) {
            fprintf(out, "%-5s  unreachable\n", FACE_NAMES[f]);
        } else {
            int k = path_to(c, s, path);
            int end_cell;
            CubeFace end_face;
            if (cube_play(n, c->start_cell, c->start_face, path, k, &end_cell, &end_face, NULL) != 0 || end_cell != c->start_cell || end_face != (CubeFace)f) {
                fprintf(stderr, "census: the macro to face %s does not roll there\n", FACE_NAMES[f]);
                exit(2);
            }
            fprintf(out, "%-5s  %5d  %s\n", FACE_NAMES[f], k, path);
        }
    }

    // The hardest levels: the first in pattern order is the example.
    uint32_t hardest = 0;
    for (uint32_t p = 0; p < c->patterns; p++) {
        if (c->par[p] == top) {
            hardest = p;
            break;
        }
    }
    fprintf(out, "\nthe largest par, %d, has %ld pattern(s); the first:\n", top, at[top]);
    print_level(out, c, "hardest", hardest);

    // The 4 x 4 language's bound, and what it leaves out.
    if (n == 4) {
        long past[MAX_PAR + 1] = {0};
        for (int d = top - 1; d >= 0; d--) past[d] = past[d + 1] + at[d + 1];
        fprintf(out, "\nthe language: programs of at most L = %d rolls (cube.h's CUBE_L)\n", CUBE_L);
        fprintf(out, "decided within L: %ld of %u, the rest:", (long)c->patterns - past[CUBE_L], c->patterns);
        for (uint32_t p = 0; p < c->patterns; p++) {
            if (c->par[p] > CUBE_L) fprintf(out, " %u%s (par %d)", p, p == c->patterns - 1 ? ", every tile" : "", c->par[p]);
        }
        fprintf(out, "\nsets with a par past each length:");
        for (int d = top - 5; d < top; d++) fprintf(out, " %d: %ld%s", d, past[d], d + 1 < top ? "," : "");
        fputc('\n', out);
    }

    // A few levels by eye.
    uint32_t all = c->patterns - 1;
    uint32_t checker = 0, border = 0, diagonal = 0;
    for (int r = 0; r < n; r++) {
        for (int col = 0; col < n; col++) {
            uint32_t bit = 1u << (r * n + col);
            if ((r + col) % 2 == 0) checker |= bit;
            if (r == 0 || col == 0 || r == n - 1 || col == n - 1) border |= bit;
            if (r == col) diagonal |= bit;
        }
    }
    fprintf(out, "\nsample levels:\n");
    print_level(out, c, "the start tile", 1u << c->start_cell);
    print_level(out, c, "the far corner", 1u << (c->tiles - 1));
    print_level(out, c, "the diagonal", diagonal);
    print_level(out, c, "the border", border);
    print_level(out, c, "the checkerboard", checker);
    print_level(out, c, "every tile", all);
}

static int write_pars(const Census *c, const char *path) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "census: cannot write %s\n", path);
        return 2;
    }
    char sol[MAX_PAR + 1];
    for (uint32_t p = 0; p < c->patterns; p++) {
        if (c->par[p] == UNSEEN) {
            fprintf(f, "%u -\n", p);
        } else {
            int k = solution(c, p, sol);
            fprintf(f, "%u %d %s\n", p, k, k == 0 ? "-" : sol);
        }
    }
    return fclose(f) == 0 ? 0 : 2;
}

// One line a start: its reachable patterns, largest par and mean par.
static void starts(FILE *out, int n) {
    fprintf(out, "cube %dx%d, every start: cell face  reachable  largest par  mean par\n", n, n);
    static Census c;
    for (int cell = 0; cell < n * n; cell++) {
        for (int face = 0; face < CUBE_FACES; face++) {
            census_run(&c, n, cell, (CubeFace)face);
            uint32_t reached = 0;
            long sum = 0;
            int top = 0;
            for (uint32_t p = 0; p < c.patterns; p++) {
                if (c.par[p] == UNSEEN) continue;
                reached++;
                sum += c.par[p];
                if (c.par[p] > top) top = c.par[p];
            }
            fprintf(out, "  %c%d   %-5s  %9u  %11d  %8.2f\n", 'a' + cell % n, cell / n + 1, FACE_NAMES[face], reached, top, (double)sum / (double)reached);
            census_free(&c);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: census N [PATH | starts]\n");
        return 2;
    }
    int n = atoi(argv[1]);
    if (n < 2 || n > 4) {
        fprintf(stderr, "census: N must be 2, 3 or 4, not %s\n", argv[1]);
        return 2;
    }
    if (argc == 3 && strcmp(argv[2], "starts") == 0) {
        starts(stdout, n);
        return fflush(stdout) == 0 ? 0 : 1;
    }
    static Census c;
    census_run(&c, n, 0, FACE_UP);
    int code = 0;
    if (argc > 3) {
        fprintf(stderr, "usage: census N [PATH | starts]\n");
        code = 2;
    } else {
        report(stdout, &c);
        if (argc == 3) code = write_pars(&c, argv[2]);
    }
    census_free(&c);
    if (fflush(stdout) != 0) code = 1;
    return code;
}
