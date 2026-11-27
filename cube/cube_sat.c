// cube_sat.c: "can this cube level be solved in exactly k rolls?" as DIMACS
// CNF, for rocsat, and the checker that holds its answers to the rules.
//
//   cube_sat cnf N TARGET K      the CNF on stdout: an N x N grid, the cube
//                                in the north-west corner with the painted
//                                face up, TARGET the pattern to end with
//                                (tile r * N + c is bit r * N + c)
//   cube_sat check N TARGET ROLLS
//                                exit 0 and "solves" when the rolls (letters
//                                N, E, S, W) solve the level, else exit 1;
//                                a roll off the grid halts them (cube.h)
//
// The encoding is the machine unrolled, one copy of its state a step:
//
//   - the rolls first, so a model reads straight off: variables 2t - 1 and
//     2t are roll t's high and low bit (N 00, E 01, S 10, W 11);
//   - per step t = 1..k, R[t][r], roll t is r, tied to the two bits;
//   - per time t = 0..k, the cube's cell P[t][c] and its painted face
//     F[t][f], each one-hot, the tiles L[t][c], and H[t], the machine
//     halted; time 0 is the start, fixed by unit clauses, every tile dark,
//     not halted;
//   - a step, while the machine runs (not H[t-1]): P[t-1][c] and R[t][r]
//     imply P[t][c'] and not H[t] for the cell c' the roll reaches, and
//     H[t] when the roll leaves the grid, which halts the machine;
//   - H[t-1] implies H[t]; a halted machine stands still: H[t] and
//     P[t-1][c] imply P[t][c], and H[t] and F[t-1][f] imply F[t][f];
//     otherwise F[t-1][f] and R[t][r] imply F[t][f'], the face cube.c's
//     rule gives;
//   - D[t][c] = P[t][c] AND F[t][down] AND NOT H[t] is a landing face down
//     on c, by Tseitin's four clauses, and L[t][c] = L[t-1][c] XOR D[t][c],
//     by four;
//   - at most one cell and one face at each time, pairwise, so no extra
//     true P or F can flip a tile;
//   - the end: L[k][c] is TARGET's bit c, a unit clause a tile.
//
// So a model is a k-roll solution, halted or not, and UNSAT says there is
// none of exactly k rolls. "At most k" is the OR of the CNFs for 0..k; the census's par is
// the least k that is SAT.

#include "cube.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int n, tiles, k;
    int next;  // the next free variable
    long clauses;
    FILE *out;
    int counting;  // 1 on the first pass, which only counts
} Cnf;

static int fresh(Cnf *c) { return c->next++; }

static void clause(Cnf *c, const int *lits, int len) {
    c->clauses++;
    if (c->counting) return;
    for (int i = 0; i < len; i++) fprintf(c->out, "%d ", lits[i]);
    fprintf(c->out, "0\n");
}

#define CLAUSE(c, ...) do { const int lits_[] = {__VA_ARGS__}; clause((c), lits_, (int)(sizeof lits_ / sizeof lits_[0])); } while (0)

static void at_most_one(Cnf *c, const int *v, int len) {
    for (int i = 0; i < len; i++) {
        for (int j = i + 1; j < len; j++) CLAUSE(c, -v[i], -v[j]);
    }
}

// The whole formula, written (or only counted) once.
static void encode(Cnf *c, uint32_t target) {
    int n = c->n, tiles = c->tiles, k = c->k;
    c->next = 2 * k + 1;
    c->clauses = 0;
    int P[k + 1][CUBE_MAX_N * CUBE_MAX_N];
    int F[k + 1][CUBE_FACES];
    int L[k + 1][CUBE_MAX_N * CUBE_MAX_N];
    int H[k + 1];
    for (int t = 0; t <= k; t++) {
        for (int x = 0; x < tiles; x++) P[t][x] = fresh(c);
        for (int f = 0; f < CUBE_FACES; f++) F[t][f] = fresh(c);
        for (int x = 0; x < tiles; x++) L[t][x] = fresh(c);
        H[t] = fresh(c);
    }
    CLAUSE(c, -H[0]);
    // The start.
    for (int x = 0; x < tiles; x++) {
        CLAUSE(c, x == 0 ? P[0][x] : -P[0][x]);
        CLAUSE(c, -L[0][x]);
    }
    for (int f = 0; f < CUBE_FACES; f++) CLAUSE(c, f == FACE_UP ? F[0][f] : -F[0][f]);

    for (int t = 1; t <= k; t++) {
        int hi = 2 * t - 1, lo = 2 * t;
        int R[CUBE_ROLLS];
        for (int r = 0; r < CUBE_ROLLS; r++) {
            R[r] = fresh(c);
            int h = (r >> 1) ? hi : -hi;
            int l = (r & 1) ? lo : -lo;
            // R[r] <-> h AND l.
            CLAUSE(c, -R[r], h);
            CLAUSE(c, -R[r], l);
            CLAUSE(c, R[r], -h, -l);
        }
        int was = H[t - 1], now = H[t];
        CLAUSE(c, -was, now);
        for (int x = 0; x < tiles; x++) {
            for (int r = 0; r < CUBE_ROLLS; r++) {
                int to = cube_cell_after(n, x, (CubeRoll)r);
                if (to < 0) {
                    CLAUSE(c, was, -P[t - 1][x], -R[r], now);
                } else {
                    CLAUSE(c, was, -P[t - 1][x], -R[r], P[t][to]);
                    CLAUSE(c, was, -P[t - 1][x], -R[r], -now);
                }
            }
            CLAUSE(c, -now, -P[t - 1][x], P[t][x]);
        }
        for (int f = 0; f < CUBE_FACES; f++) {
            for (int r = 0; r < CUBE_ROLLS; r++) {
                CLAUSE(c, now, -F[t - 1][f], -R[r], F[t][cube_face_after((CubeFace)f, (CubeRoll)r)]);
            }
            CLAUSE(c, -now, -F[t - 1][f], F[t][f]);
        }
        at_most_one(c, P[t], tiles);
        at_most_one(c, F[t], CUBE_FACES);
        int down = F[t][FACE_DOWN];
        for (int x = 0; x < tiles; x++) {
            int d = fresh(c);
            CLAUSE(c, -d, P[t][x]);
            CLAUSE(c, -d, down);
            CLAUSE(c, -d, -now);
            CLAUSE(c, d, -P[t][x], -down, now);
            int a = L[t][x], b = L[t - 1][x];
            // a = b XOR d.
            CLAUSE(c, -a, b, d);
            CLAUSE(c, -a, -b, -d);
            CLAUSE(c, a, -b, d);
            CLAUSE(c, a, b, -d);
        }
    }
    for (int x = 0; x < tiles; x++) CLAUSE(c, (target >> x & 1) ? L[k][x] : -L[k][x]);
}

static int usage(void) {
    fprintf(stderr, "usage: cube_sat cnf N TARGET K | cube_sat check N TARGET ROLLS\n");
    return 2;
}

int main(int argc, char **argv) {
    if (argc != 5) return usage();
    int n = atoi(argv[2]);
    if (n < 2 || n > CUBE_MAX_N) {
        fprintf(stderr, "cube_sat: N must be 2 to %d, not %s\n", CUBE_MAX_N, argv[2]);
        return 2;
    }
    uint32_t target = (uint32_t)strtoul(argv[3], NULL, 10);
    if (n * n < 32 && target >> (n * n) != 0) {
        fprintf(stderr, "cube_sat: pattern %s has a tile past %d\n", argv[3], n * n);
        return 2;
    }
    if (strcmp(argv[1], "check") == 0) {
        const char *rolls = strcmp(argv[4], "-") == 0 ? "" : argv[4];
        int ok = cube_check(n, 0, FACE_UP, target, rolls, (int)strlen(rolls));
        printf("%s\n", ok ? "solves" : "does not solve");
        return ok ? 0 : 1;
    }
    if (strcmp(argv[1], "cnf") != 0) return usage();
    int k = atoi(argv[4]);
    if (k < 0 || k > 1000) {
        fprintf(stderr, "cube_sat: K must be 0 to 1000, not %s\n", argv[4]);
        return 2;
    }
    Cnf c = {.n = n, .tiles = n * n, .k = k, .out = stdout, .counting = 1};
    encode(&c, target);
    printf("c cube %dx%d, target %u, exactly %d rolls; variables 1..%d are the rolls' bits\n", n, n, target, k, 2 * k);
    printf("p cnf %d %ld\n", c.next - 1, c.clauses);
    c.counting = 0;
    encode(&c, target);
    return fflush(stdout) == 0 ? 0 : 1;
}
