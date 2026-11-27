// test_cube_gates.c: the rolling cube's one-roll circuit (cube/gates.h),
// held to cube.c's cube_step on every state and every roll of the 3 x 3 and
// 4 x 4 grids; its gate counts part by part; the k-roll chain held to
// cube_play; and the chain's CNF held to the chain on every 6-roll program
// of the 3 x 3 grid.
//
// A state is one-hot: one cell, one face, any tiles, halted or not. The
// check runs 64 states at once, a bit of a 64-bit word each: the circuit's
// wires are words, NOT is ~, AND is &, OR is |, and the 64 lanes are the
// 64 settings of tiles 0 to 5, so every state is one lane of one run.

// open_memstream, for the CNF written to memory.
#define _POSIX_C_SOURCE 200809L

#include "test.h"

#include "../src/circuit.c"
#include "../cube/cube.c"
#include "../cube/gates.c"

#include <stdlib.h>

static Net NET;
static uint64_t WORDS[CIRCUIT_MAX_WIRES];

// The net's gates as a compact list, for the 786,432 runs of the 4x4 check:
// read from NET's wires, which carry their names, the whole file took 5.3 s
// under tcc, against 3.7 s so.
typedef struct {
    int kind, out, a, b;
} Op;
static Op OPS[CIRCUIT_MAX_WIRES];
static int NUM_OPS, NUM_INPUTS;
static int INPUT_WIRE[CIRCUIT_MAX_WIRES];

static void compile_net(void) {
    NUM_OPS = NUM_INPUTS = 0;
    for (int i = 0; i < NET.num_wires; i++) {
        const Wire *w = &NET.wires[i];
        if (w->kind == W_INPUT) INPUT_WIRE[NUM_INPUTS++] = i;
        else if (w->kind == W_ZERO) WORDS[i] = 0;
        else if (w->kind == W_ONE) WORDS[i] = ~(uint64_t)0;
        else OPS[NUM_OPS++] = (Op){w->kind, i, w->a, w->b};
    }
}

static void eval_words(const uint64_t *inputs) {
    for (int i = 0; i < NUM_INPUTS; i++) WORDS[INPUT_WIRE[i]] = inputs[i];
    for (int i = 0; i < NUM_OPS; i++) {
        const Op *o = &OPS[i];
        if (o->kind == W_AND) WORDS[o->out] = WORDS[o->a] & WORDS[o->b];
        else if (o->kind == W_OR) WORDS[o->out] = WORDS[o->a] | WORDS[o->b];
        else if (o->kind == W_NOT) WORDS[o->out] = ~WORDS[o->a];
        else WORDS[o->out] = WORDS[o->a];
    }
}

static uint64_t all(int bit) { return bit ? ~(uint64_t)0 : 0; }

// Every state and roll of an n x n grid through the circuit and through
// cube_step; gives how many (state, roll) pairs it checked.
static long check_every_state(int n) {
    cube_roll_circuit(&NET, n);
    int tiles = n * n, count = cube_state_wires(n);
    int out[2 * CUBE_MAX_TILES + CUBE_FACES + 1];
    char name[CIRCUIT_NAME];
    for (int i = 0; i < count; i++) {
        cube_state_wire_name(n, i, name, sizeof name);
        strcat(name, "'");
        out[i] = net_port_wire(&NET, "", name);
    }
    compile_net();
    // LANE[i]: lane j's tile i is bit i of j.
    uint64_t lane[6];
    for (int i = 0; i < 6; i++) {
        lane[i] = 0;
        for (int j = 0; j < 64; j++) lane[i] |= (uint64_t)(j >> i & 1) << j;
    }
    uint64_t in[2 * CUBE_MAX_TILES + CUBE_FACES + 3];
    long checked = 0;
    for (int cell = 0; cell < tiles; cell++) {
        for (int face = 0; face < CUBE_FACES; face++) {
            for (int halted = 0; halted < 2; halted++) {
                for (int r = 0; r < CUBE_ROLLS; r++) {
                    // What the roll does to the cube and which tiles it flips:
                    // the same for all tiles, which each lane checks.
                    CubeState flips = {cell, (CubeFace)face, 0, halted};
                    cube_step(n, &flips, (CubeRoll)r);
                    for (uint32_t high = 0; high < 1u << (tiles - 6); high++) {
                        int i = 0;
                        for (int x = 0; x < tiles; x++) in[i++] = all(x == cell);
                        for (int f = 0; f < CUBE_FACES; f++) in[i++] = all(f == face);
                        for (int x = 0; x < tiles; x++) in[i++] = x < 6 ? lane[x] : all(high >> (x - 6) & 1);
                        in[i++] = all(halted);
                        in[i++] = all(r >> 1);
                        in[i++] = all(r & 1);
                        eval_words(in);
                        for (int j = 0; j < 64; j++) {
                            CubeState s = {cell, (CubeFace)face, (uint32_t)j | high << 6, halted};
                            uint32_t lit = s.lit;
                            cube_step(n, &s, (CubeRoll)r);
                            EXPECT(s.cell == flips.cell && s.face == flips.face && s.halted == flips.halted);
                            EXPECT_EQ(s.lit, lit ^ flips.lit);
                        }
                        // So every lane's next state is this, by cube_step.
                        i = 0;
                        for (int x = 0; x < tiles; x++, i++) EXPECT(WORDS[out[i]] == all(x == flips.cell));
                        for (int f = 0; f < CUBE_FACES; f++, i++) EXPECT(WORDS[out[i]] == all(f == (int)flips.face));
                        for (int x = 0; x < tiles; x++, i++) EXPECT(WORDS[out[i]] == (in[i] ^ all(flips.lit >> x & 1)));
                        EXPECT(WORDS[out[i]] == all(flips.halted));
                        checked += 64;
                    }
                }
            }
        }
    }
    return checked;
}

TEST(one_roll_is_cube_step_on_every_3x3_state) {
    // 9 cells, 6 faces, 2^9 tile patterns, halted or not, 4 rolls.
    EXPECT_EQ(check_every_state(3), 9L * 6 * 512 * 2 * 4);
}

TEST(one_roll_is_cube_step_on_every_4x4_state) {
    EXPECT_EQ(check_every_state(4), 16L * 6 * 65536 * 2 * 4);
}

static int gates(const char *part) {
    GateCount g = net_count(&NET, part);
    return g.nots + g.ands + g.ors;
}

// The counts gates.h's parts give, for n from 2 to 5: A = 4n(n - 1) is the
// number of (cell, roll) pairs that stay on the grid.
TEST(the_parts_have_their_gate_counts) {
    for (int n = 2; n <= CUBE_MAX_N; n++) {
        cube_roll_circuit(&NET, n);
        int a = 4 * n * (n - 1);
        EXPECT_EQ(gates("decode"), 6);
        EXPECT_EQ(gates("halt"), 4 * n + 9);
        EXPECT_EQ(gates("move"), 2 * a + n * n);
        EXPECT_EQ(gates("paint"), 54);
        EXPECT_EQ(gates("flip"), 5 * n * n + 1);
        EXPECT_EQ(gates(""), 6 + 4 * n + 9 + 2 * a + n * n + 54 + 5 * n * n + 1);
    }
    cube_roll_circuit(&NET, 3);
    EXPECT_EQ(gates(""), 184);
    cube_roll_circuit(&NET, 4);
    EXPECT_EQ(gates(""), 278);
    GateCount g = net_count(&NET, "");
    EXPECT_EQ(g.nots, 19);
    EXPECT_EQ(g.ands + g.ors, 259);
}

// The chain's last state and its output against cube_play.
static void check_chain(const CubeChain *c, unsigned char *values, const char *rolls) {
    cube_chain_eval(c, rolls, values);
    int end_cell, halt_at;
    CubeFace end_face;
    int64_t lit = cube_play(c->n, 0, FACE_UP, rolls, c->k, &end_cell, &end_face, &halt_at);
    int tiles = c->n * c->n, i = 0;
    for (int x = 0; x < tiles; x++, i++) EXPECT_EQ(values[c->last[i]], x == end_cell);
    for (int f = 0; f < CUBE_FACES; f++, i++) EXPECT_EQ(values[c->last[i]], f == (int)end_face);
    for (int x = 0; x < tiles; x++, i++) EXPECT_EQ(values[c->last[i]], lit >> x & 1);
    EXPECT_EQ(values[c->last[i]], halt_at >= 0);
    EXPECT_EQ(values[c->out], lit == (int64_t)c->target);
}

static const char LETTERS[] = "NESW";

TEST(the_chain_is_cube_play_on_every_6_roll_3x3_program) {
    CubeChain c = cube_chain(3, 6, 256); // c3 alone, par 4
    unsigned char *values = malloc((size_t)c.num_wires);
    char rolls[7] = {0};
    int solved = 0;
    for (int p = 0; p < 4096; p++) {
        for (int t = 0; t < 6; t++) rolls[t] = LETTERS[p >> (2 * t) & 3];
        check_chain(&c, values, rolls);
        solved += values[c.out];
    }
    EXPECT(solved > 0);
    free(values);
    cube_chain_free(&c);
}

// The census's shortest solutions (cube/README.md, "Levels") make the output
// true at their par; random 4x4 programs of L rolls, most of them halting,
// end as cube_play ends them.
TEST(the_chain_solves_the_census_levels_and_matches_random_programs) {
    static const struct {
        uint32_t target;
        const char *rolls;
    } LEVELS[] = {
        {33825, "ESSENNWWESESESNNWW"},
        {63903, "EESENWSEWSEWSENWSNWSNWSENWENWENWSEN"},
        {42405, "EEESSWNNWWESSWNENESENWWSEESSNWWS"},
        {65535, "EEESWNWSENWSWNESWESWESWNESENWNESESWNESWNEWNEWNE"},
    };
    for (size_t i = 0; i < sizeof LEVELS / sizeof LEVELS[0]; i++) {
        int k = (int)strlen(LEVELS[i].rolls);
        CubeChain c = cube_chain(4, k, LEVELS[i].target);
        unsigned char *values = malloc((size_t)c.num_wires);
        check_chain(&c, values, LEVELS[i].rolls);
        EXPECT_EQ(values[c.out], 1);
        free(values);
        cube_chain_free(&c);
    }
    CubeChain c = cube_chain(4, CUBE_L, 0);
    unsigned char *values = malloc((size_t)c.num_wires);
    char rolls[CUBE_L + 1] = {0};
    uint32_t seed = 12345;
    int halted = 0;
    for (int p = 0; p < 2000; p++) {
        for (int t = 0; t < CUBE_L; t++) {
            seed = seed * 1103515245u + 12345u;
            rolls[t] = LETTERS[seed >> 16 & 3];
        }
        check_chain(&c, values, rolls);
        halted += values[c.last[cube_state_wires(4) - 1]];
    }
    EXPECT(halted > 0 && halted < 2000);
    free(values);
    cube_chain_free(&c);
}

// The CNF as text, parsed back: every clause's literals, each clause ended
// by 0.
typedef struct {
    int vars;
    int *lits;
    long len, clauses;
} Dimacs;

static Dimacs parse_dimacs(const char *text) {
    Dimacs d = {0, NULL, 0, 0};
    long cap = 0;
    const char *p = text;
    while (*p) {
        if (*p == 'c') {
            while (*p && *p != '\n') p++;
        } else if (*p == 'p') {
            long clauses;
            if (sscanf(p, "p cnf %d %ld", &d.vars, &clauses) != 2) abort();
            while (*p && *p != '\n') p++;
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            char *end;
            long v = strtol(p, &end, 10);
            p = end;
            if (d.len == cap) {
                cap = cap ? 2 * cap : 4096;
                d.lits = realloc(d.lits, (size_t)cap * sizeof(int));
                if (d.lits == NULL) abort();
            }
            d.lits[d.len++] = (int)v;
            if (v == 0) d.clauses++;
        } else {
            p++;
        }
    }
    return d;
}

// The CNF written to memory: the whole file, NUL-ended.
static char *dimacs_text(const CubeChain *c, int *vars, long *clauses) {
    char *text = NULL;
    size_t size = 0;
    FILE *f = open_memstream(&text, &size);
    if (f == NULL) abort();
    cube_chain_dimacs(c, f, vars, clauses);
    fclose(f);
    return text;
}

// Unit propagation to a fixed point, from what `assign` holds (-1 unset).
// Gives 0 at a conflict, else 1.
static int propagate(const Dimacs *d, signed char *assign) {
    int changed = 1;
    while (changed) {
        changed = 0;
        long start = 0;
        for (long i = 0; i < d->len; i++) {
            if (d->lits[i] != 0) continue;
            int sat = 0, unset = 0, last = 0;
            for (long j = start; j < i; j++) {
                int l = d->lits[j], v = assign[abs(l)];
                if (v < 0) {
                    unset++;
                    last = l;
                } else if ((l > 0) == (v == 1)) {
                    sat = 1;
                }
            }
            start = i + 1;
            if (sat) continue;
            if (unset == 0) return 0;
            if (unset == 1) {
                assign[abs(last)] = last > 0;
                changed = 1;
            }
        }
    }
    return 1;
}

// Propagation from the roll bits alone settles every variable, to the
// circuit's own values, with a conflict exactly when the program does not
// solve the level: so the CNF's models are exactly the solving programs,
// each with its circuit's values. As tests/test_sat.c proves sat.c's.
TEST(the_cnf_propagates_to_exactly_the_solving_6_roll_3x3_programs) {
    CubeChain c = cube_chain(3, 6, 256);
    int vars;
    long clauses;
    char *text = dimacs_text(&c, &vars, &clauses);
    Dimacs d = parse_dimacs(text);
    EXPECT_EQ(d.vars, vars);
    EXPECT_EQ(d.clauses, clauses);
    // 2k roll bits, the start state's 25 wires, k rolls' 184 - 12 = 172
    // ANDs and ORs and the match's 8 ANDs; three clauses a gate, a unit a
    // start wire and one for the output.
    EXPECT_EQ(vars, 12 + 25 + 6 * 172 + 8);
    EXPECT_EQ(clauses, 25 + 3 * (6 * 172 + 8) + 1);
    int *lits = malloc((size_t)c.num_wires * sizeof(int));
    cube_chain_literals(&c, lits);
    unsigned char *values = malloc((size_t)c.num_wires);
    signed char *assign = malloc((size_t)vars + 1);
    char rolls[7] = {0};
    int models = 0, solved = 0;
    for (int p = 0; p < 4096; p++) {
        for (int t = 0; t < 6; t++) rolls[t] = LETTERS[p >> (2 * t) & 3];
        memset(assign, -1, (size_t)vars + 1);
        // The roll bits are variables 1 to 2k, roll t's hi at 2t - 1.
        for (int t = 0; t < 6; t++) {
            int r = cube_roll_of(rolls[t]);
            assign[2 * t + 1] = (signed char)(r >> 1);
            assign[2 * t + 2] = (signed char)(r & 1);
        }
        int ok = propagate(&d, assign);
        int solves = cube_check(3, 0, FACE_UP, 256, rolls, 6);
        solved += solves;
        EXPECT_EQ(ok, solves);
        if (ok) {
            cube_chain_eval(&c, rolls, values);
            for (int i = 0; i < c.num_wires; i++) {
                if (lits[i] > 0) EXPECT_EQ(assign[lits[i]], values[i]);
            }
            for (int v = 1; v <= vars; v++) EXPECT(assign[v] >= 0);
            models++;
        }
    }
    EXPECT_EQ(models, solved);
    EXPECT(solved > 0);
    free(assign);
    free(values);
    free(lits);
    free(d.lits);
    free(text);
    cube_chain_free(&c);
}

TEST_MAIN(TEST_CASE(one_roll_is_cube_step_on_every_3x3_state), TEST_CASE(one_roll_is_cube_step_on_every_4x4_state),
          TEST_CASE(the_parts_have_their_gate_counts), TEST_CASE(the_chain_is_cube_play_on_every_6_roll_3x3_program),
          TEST_CASE(the_chain_solves_the_census_levels_and_matches_random_programs),
          TEST_CASE(the_cnf_propagates_to_exactly_the_solving_6_roll_3x3_programs))
