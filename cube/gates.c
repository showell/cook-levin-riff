// gates.c: one roll of the rolling cube as gates, and k rolls unrolled into
// CNF (gates.h).

#include "gates.h"

#include <stdlib.h>
#include <string.h>

const char *const CUBE_PART_NAMES[CUBE_PARTS] = {"decode", "halt", "move", "paint", "flip"};

static const char *const FACE_NAMES[CUBE_FACES] = {"up", "down", "north", "south", "east", "west"};
static const char *const ROLL_NAMES[CUBE_ROLLS] = {"N", "E", "S", "W"};

// TURN[r][f]: where the painted face points after roll r, when it pointed f
// before. A roll tips the cube over the bottom edge ahead of it: up goes
// ahead, ahead goes down, down goes behind, behind comes up, and the two
// faces beside the roll stay.
static const CubeFace TURN[CUBE_ROLLS][CUBE_FACES] = {
    // up          down        north       south       east        west
    {FACE_NORTH, FACE_SOUTH, FACE_DOWN, FACE_UP, FACE_EAST, FACE_WEST},   // N
    {FACE_EAST, FACE_WEST, FACE_NORTH, FACE_SOUTH, FACE_DOWN, FACE_UP},   // E
    {FACE_SOUTH, FACE_NORTH, FACE_UP, FACE_DOWN, FACE_EAST, FACE_WEST},   // S
    {FACE_WEST, FACE_EAST, FACE_NORTH, FACE_SOUTH, FACE_UP, FACE_DOWN},   // W
};

static void gates_die(const char *what, long value) {
    fprintf(stderr, "cube gates: %s: %ld\n", what, value);
    abort();
}

static void cell_name(int n, int cell, char *out, size_t size) { snprintf(out, size, "%c%d", 'a' + cell % n, cell / n + 1); }

int cube_state_wires(int n) { return 2 * n * n + CUBE_FACES + 1; }

void cube_state_wire_name(int n, int i, char *out, size_t size) {
    int tiles = n * n;
    char cell[16];
    if (i < tiles) {
        cell_name(n, i, cell, sizeof cell);
        snprintf(out, size, "cell %s", cell);
    } else if (i < tiles + CUBE_FACES) {
        snprintf(out, size, "face %s", FACE_NAMES[i - tiles]);
    } else if (i < 2 * tiles + CUBE_FACES) {
        cell_name(n, i - tiles - CUBE_FACES, cell, sizeof cell);
        snprintf(out, size, "tile %s", cell);
    } else {
        snprintf(out, size, "halted");
    }
}

// The cell that roll r carries into `cell`, or -1 when none does: the
// cube arrives rolling north from the cell to the south, east from the
// west, south from the north, west from the east.
static int from_cell(int n, int cell, int r) {
    int row = cell / n, col = cell % n;
    if (r == CUBE_N) row++;
    if (r == CUBE_E) col--;
    if (r == CUBE_S) row--;
    if (r == CUBE_W) col++;
    if (row < 0 || row >= n || col < 0 || col >= n) return -1;
    return row * n + col;
}

// Whether roll r from `cell` leaves the grid: the cell is on the edge the
// roll goes over.
static int rolls_off(int n, int cell, int r) {
    int row = cell / n, col = cell % n;
    if (r == CUBE_N) return row == 0;
    if (r == CUBE_E) return col == n - 1;
    if (r == CUBE_S) return row == n - 1;
    return col == 0;
}

static void name_of(char *to, const char *prefix, const char *name) {
    char buf[2 * CIRCUIT_NAME];
    snprintf(buf, sizeof buf, "%s%s", prefix, name);
    if (strlen(buf) >= CIRCUIT_NAME) gates_die("a part's name past the limit", (long)strlen(buf));
    strcpy(to, buf);
}

// An OR of the wires so far and `w`: the first is the wire itself.
static int or_into(Net *net, const char *part, const char *name, int acc, int w) {
    return acc < 0 ? w : net_or(net, part, name, acc, w);
}

void part_cube_roll(Net *net, const char *prefix, int n, const CubeWires *s, int hi, int lo, CubeWires *next) {
    int tiles = n * n;
    char part[CIRCUIT_NAME], name[CIRCUIT_NAME], cell[8];

    // decode: the four rolls, N 00, E 01, S 10, W 11.
    name_of(part, prefix, "decode");
    int not_hi = net_not(net, part, "not hi", hi);
    int not_lo = net_not(net, part, "not lo", lo);
    int roll[CUBE_ROLLS];
    roll[CUBE_N] = net_and(net, part, "N", not_hi, not_lo);
    roll[CUBE_E] = net_and(net, part, "E", not_hi, lo);
    roll[CUBE_S] = net_and(net, part, "S", hi, not_lo);
    roll[CUBE_W] = net_and(net, part, "W", hi, lo);

    // halt: off the grid, halted', stay, run and the rolls that go.
    name_of(part, prefix, "halt");
    int off = -1;
    for (int r = 0; r < CUBE_ROLLS; r++) {
        int edge = -1;
        snprintf(name, sizeof name, "on edge %s", ROLL_NAMES[r]);
        for (int x = 0; x < tiles; x++) {
            if (rolls_off(n, x, r)) edge = or_into(net, part, name, edge, s->cell[x]);
        }
        snprintf(name, sizeof name, "off %s", ROLL_NAMES[r]);
        int this_off = net_and(net, part, name, roll[r], edge);
        off = or_into(net, part, "off", off, this_off);
    }
    int stay = net_or(net, part, "halted'", s->halted, off);
    int run = net_not(net, part, "run", stay);
    int go[CUBE_ROLLS];
    for (int r = 0; r < CUBE_ROLLS; r++) {
        snprintf(name, sizeof name, "go %s", ROLL_NAMES[r]);
        go[r] = net_and(net, part, name, roll[r], run);
    }
    next->halted = stay;

    // move: each cell's wire from the neighbour a roll brings the cube from,
    // or from itself when the cube stays.
    name_of(part, prefix, "move");
    for (int x = 0; x < tiles; x++) {
        cell_name(n, x, cell, sizeof cell);
        int acc = -1;
        for (int r = 0; r < CUBE_ROLLS; r++) {
            int from = from_cell(n, x, r);
            if (from < 0) continue;
            snprintf(name, sizeof name, "%s by %s", cell, ROLL_NAMES[r]);
            int term = net_and(net, part, name, go[r], s->cell[from]);
            snprintf(name, sizeof name, "%s'", cell);
            acc = or_into(net, part, name, acc, term);
        }
        snprintf(name, sizeof name, "%s stays", cell);
        int kept = net_and(net, part, name, s->cell[x], stay);
        snprintf(name, sizeof name, "%s'", cell);
        next->cell[x] = or_into(net, part, name, acc, kept);
    }

    // paint: each face's wire from the face a roll turns into it, or from
    // itself when the cube stays.
    name_of(part, prefix, "paint");
    for (int g = 0; g < CUBE_FACES; g++) {
        int acc = -1;
        for (int r = 0; r < CUBE_ROLLS; r++) {
            int from = -1;
            for (int f = 0; f < CUBE_FACES; f++) {
                if (TURN[r][f] == (CubeFace)g) from = f;
            }
            snprintf(name, sizeof name, "%s by %s", FACE_NAMES[g], ROLL_NAMES[r]);
            int term = net_and(net, part, name, go[r], s->face[from]);
            snprintf(name, sizeof name, "%s'", FACE_NAMES[g]);
            acc = or_into(net, part, name, acc, term);
        }
        snprintf(name, sizeof name, "%s stays", FACE_NAMES[g]);
        int kept = net_and(net, part, name, s->face[g], stay);
        snprintf(name, sizeof name, "%s'", FACE_NAMES[g]);
        next->face[g] = or_into(net, part, name, acc, kept);
    }

    // flip: a landing face down on the cell the cube moved to flips its tile.
    name_of(part, prefix, "flip");
    int land = net_and(net, part, "land", next->face[FACE_DOWN], run);
    for (int x = 0; x < tiles; x++) {
        cell_name(n, x, cell, sizeof cell);
        snprintf(name, sizeof name, "hit %s", cell);
        int hit = net_and(net, part, name, next->cell[x], land);
        snprintf(name, sizeof name, "%s either", cell);
        int either = net_or(net, part, name, s->tile[x], hit);
        snprintf(name, sizeof name, "%s both", cell);
        int both = net_and(net, part, name, s->tile[x], hit);
        snprintf(name, sizeof name, "%s not both", cell);
        int not_both = net_not(net, part, name, both);
        snprintf(name, sizeof name, "%s'", cell);
        next->tile[x] = net_and(net, part, name, either, not_both);
    }
}

// How cube_roll_circuit's five parts are wired: a link a wire.
static void roll_links(Net *net, int n) {
    int tiles = n * n;
    for (int i = 0; i < 2; i++) net_link(net, "", "decode", "bits", "", "roll");
    for (int r = 0; r < CUBE_ROLLS; r++) net_link(net, "", "halt", "roll", "decode", ROLL_NAMES[r]);
    // The halt reads each cell on the edge once, a corner for two rolls.
    for (int x = 0; x < tiles; x++) {
        int edge = 0;
        for (int r = 0; r < CUBE_ROLLS; r++) edge |= rolls_off(n, x, r);
        if (edge) net_link(net, "", "halt", "cell", "", "cell");
    }
    net_link(net, "", "halt", "halted", "", "halted");
    for (int r = 0; r <= CUBE_ROLLS; r++) {
        net_link(net, "", "move", "go", "halt", "go");
        net_link(net, "", "paint", "go", "halt", "go");
    }
    for (int x = 0; x < tiles; x++) net_link(net, "", "move", "cell", "", "cell");
    for (int f = 0; f < CUBE_FACES; f++) net_link(net, "", "paint", "face", "", "face");
    net_link(net, "", "flip", "run", "halt", "run");
    net_link(net, "", "flip", "down", "paint", "down'");
    for (int x = 0; x < tiles; x++) {
        net_link(net, "", "flip", "cell", "move", "cell'");
        net_link(net, "", "flip", "tile", "", "tile");
        net_link(net, "", "", "cell'", "move", "cell'");
        net_link(net, "", "", "tile'", "flip", "tile'");
    }
    for (int f = 0; f < CUBE_FACES; f++) net_link(net, "", "", "face'", "paint", "face'");
    net_link(net, "", "", "halted'", "halt", "halted'");
}

static void wires_to_list(int n, const CubeWires *w, int *list) {
    int tiles = n * n, i = 0;
    for (int x = 0; x < tiles; x++) list[i++] = w->cell[x];
    for (int f = 0; f < CUBE_FACES; f++) list[i++] = w->face[f];
    for (int x = 0; x < tiles; x++) list[i++] = w->tile[x];
    list[i] = w->halted;
}

static void list_to_wires(int n, const int *list, CubeWires *w) {
    int tiles = n * n, i = 0;
    for (int x = 0; x < tiles; x++) w->cell[x] = list[i++];
    for (int f = 0; f < CUBE_FACES; f++) w->face[f] = list[i++];
    for (int x = 0; x < tiles; x++) w->tile[x] = list[i++];
    w->halted = list[i];
}

void cube_roll_circuit(Net *net, int n) {
    if (n < 2 || n > CUBE_MAX_N) gates_die("a grid of this size", n);
    net_init(net);
    int count = cube_state_wires(n);
    int in[2 * CUBE_MAX_TILES + CUBE_FACES + 1], out[2 * CUBE_MAX_TILES + CUBE_FACES + 1];
    char name[CIRCUIT_NAME];
    for (int i = 0; i < count; i++) {
        cube_state_wire_name(n, i, name, sizeof name);
        in[i] = net_input(net, name);
    }
    int hi = net_input(net, "roll hi");
    int lo = net_input(net, "roll lo");
    CubeWires s, next;
    list_to_wires(n, in, &s);
    part_cube_roll(net, "", n, &s, hi, lo, &next);
    wires_to_list(n, &next, out);
    for (int i = 0; i < count; i++) {
        cube_state_wire_name(n, i, name, sizeof name);
        strcat(name, "'");
        net_port(net, "", name, out[i]);
    }
    roll_links(net, n);
}

// ── k rolls unrolled ────────────────────────────────────────────────────

static int chain_wire(CubeChain *c, WireKind kind, int a, int b) {
    if (c->num_wires == c->cap) {
        c->cap = c->cap ? 2 * c->cap : 1024;
        c->kind = realloc(c->kind, (size_t)c->cap);
        c->a = realloc(c->a, (size_t)c->cap * sizeof(int));
        c->b = realloc(c->b, (size_t)c->cap * sizeof(int));
        if (c->kind == NULL || c->a == NULL || c->b == NULL) gates_die("out of memory at wires", c->cap);
    }
    c->kind[c->num_wires] = (unsigned char)kind;
    c->a[c->num_wires] = a;
    c->b[c->num_wires] = b;
    return c->num_wires++;
}

CubeChain cube_chain(int n, int k, uint32_t target) {
    static Net roll;
    cube_roll_circuit(&roll, n);
    int count = cube_state_wires(n);
    int out[2 * CUBE_MAX_TILES + CUBE_FACES + 1];
    char name[CIRCUIT_NAME];
    for (int i = 0; i < count; i++) {
        cube_state_wire_name(n, i, name, sizeof name);
        strcat(name, "'");
        out[i] = net_port_wire(&roll, "", name);
    }

    CubeChain c = {.n = n, .k = k, .target = target};
    chain_wire(&c, W_ZERO, -1, -1);
    chain_wire(&c, W_ONE, -1, -1);
    for (int i = 0; i < 2 * k; i++) chain_wire(&c, W_INPUT, -1, -1);
    c.start = c.num_wires;
    int state[2 * CUBE_MAX_TILES + CUBE_FACES + 1];
    for (int i = 0; i < count; i++) state[i] = chain_wire(&c, W_INPUT, -1, -1);
    c.num_inputs = c.num_wires - 2;

    int *map = malloc((size_t)roll.num_wires * sizeof(int));
    if (map == NULL) gates_die("out of memory at the map", roll.num_wires);
    for (int t = 1; t <= k; t++) {
        int next_input = 0;
        for (int i = 0; i < roll.num_wires; i++) {
            const Wire *w = &roll.wires[i];
            switch (w->kind) {
            case W_ZERO: map[i] = 0; break;
            case W_ONE: map[i] = 1; break;
            case W_INPUT:
                // The state's wires, then the roll's hi and lo.
                map[i] = next_input < count ? state[next_input] : 2 + 2 * (t - 1) + (next_input - count);
                next_input++;
                break;
            case W_ALIAS: map[i] = map[w->a]; break;
            case W_NOT: map[i] = chain_wire(&c, W_NOT, map[w->a], -1); break;
            case W_AND:
            case W_OR: map[i] = chain_wire(&c, w->kind, map[w->a], map[w->b]); break;
            }
        }
        for (int i = 0; i < count; i++) state[i] = map[out[i]];
    }
    free(map);
    for (int i = 0; i < count; i++) c.last[i] = state[i];

    // match: every last tile as the target has it, ANDed.
    int tiles = n * n, first_tile = tiles + CUBE_FACES, acc = -1;
    for (int x = 0; x < tiles; x++) {
        int w = state[first_tile + x];
        if (!(target >> x & 1)) w = chain_wire(&c, W_NOT, w, -1);
        acc = acc < 0 ? w : chain_wire(&c, W_AND, acc, w);
    }
    c.out = acc;
    return c;
}

void cube_chain_free(CubeChain *c) {
    free(c->kind);
    free(c->a);
    free(c->b);
    c->kind = NULL;
    c->a = c->b = NULL;
}

// The start state's value of state wire i: a1, the face up, dark, running.
static int start_value(int n, int i) { return i == 0 || i == n * n + FACE_UP; }

void cube_chain_eval(const CubeChain *c, const char *rolls, unsigned char *values) {
    for (int i = 0; i < c->num_wires; i++) {
        switch ((WireKind)c->kind[i]) {
        case W_ZERO: values[i] = 0; break;
        case W_ONE: values[i] = 1; break;
        case W_INPUT:
            if (i < c->start) {
                int t = (i - 2) / 2, r = cube_roll_of(rolls[t]);
                if (r < 0) gates_die("a roll that is no letter N, E, S or W, at", t + 1);
                values[i] = (unsigned char)((i - 2) % 2 == 0 ? r >> 1 : r & 1);
            } else {
                values[i] = (unsigned char)start_value(c->n, i - c->start);
            }
            break;
        case W_NOT: values[i] = !values[c->a[i]]; break;
        case W_AND: values[i] = values[c->a[i]] & values[c->b[i]]; break;
        case W_OR: values[i] = values[c->a[i]] | values[c->b[i]]; break;
        default: gates_die("a wire of no kind the chain makes", c->kind[i]);
        }
    }
}

void cube_chain_literals(const CubeChain *c, int *lits) {
    int next = 1;
    for (int i = 0; i < c->num_wires; i++) {
        switch ((WireKind)c->kind[i]) {
        case W_ZERO:
        case W_ONE: lits[i] = 0; break;
        case W_INPUT:
        case W_AND:
        case W_OR: lits[i] = next++; break;
        case W_NOT: lits[i] = -lits[c->a[i]]; break;
        default: gates_die("a wire of no kind the chain makes", c->kind[i]);
        }
        if (c->kind[i] >= W_NOT && (lits[c->a[i]] == 0 || (c->kind[i] != W_NOT && lits[c->b[i]] == 0))) {
            gates_die("a gate reads a constant, wire", i);
        }
    }
}

static void put_clause(FILE *f, long *clauses, const int *lits, int len) {
    (*clauses)++;
    if (f == NULL) return;
    for (int i = 0; i < len; i++) fprintf(f, "%d ", lits[i]);
    fprintf(f, "0\n");
}

#define PUT(f, count, ...) do { const int lits_[] = {__VA_ARGS__}; put_clause((f), (count), lits_, (int)(sizeof lits_ / sizeof lits_[0])); } while (0)

// The clauses, written to `f` or only counted.
static void chain_clauses(const CubeChain *c, const int *lits, FILE *f, long *clauses) {
    *clauses = 0;
    int count = cube_state_wires(c->n);
    for (int i = 0; i < count; i++) PUT(f, clauses, start_value(c->n, i) ? lits[c->start + i] : -lits[c->start + i]);
    for (int i = 0; i < c->num_wires; i++) {
        int g = lits[i];
        if (c->kind[i] == W_AND) {
            int x = lits[c->a[i]], y = lits[c->b[i]];
            PUT(f, clauses, -g, x);
            PUT(f, clauses, -g, y);
            PUT(f, clauses, g, -x, -y);
        } else if (c->kind[i] == W_OR) {
            int x = lits[c->a[i]], y = lits[c->b[i]];
            PUT(f, clauses, g, -x);
            PUT(f, clauses, g, -y);
            PUT(f, clauses, -g, x, y);
        }
    }
    PUT(f, clauses, lits[c->out]);
}

void cube_chain_dimacs(const CubeChain *c, FILE *f, int *vars, long *clauses) {
    int *lits = malloc((size_t)c->num_wires * sizeof(int));
    if (lits == NULL) gates_die("out of memory at the literals", c->num_wires);
    cube_chain_literals(c, lits);
    int v = 0;
    for (int i = 0; i < c->num_wires; i++) {
        if (lits[i] > v) v = lits[i];
    }
    long count = 0;
    chain_clauses(c, lits, NULL, &count);
    if (f != NULL) {
        fprintf(f, "c cube %dx%d by gates, target %u, exactly %d rolls; variables 1..%d are the rolls' bits\n", c->n, c->n, c->target, c->k,
                2 * c->k);
        fprintf(f, "p cnf %d %ld\n", v, count);
        long again = 0;
        chain_clauses(c, lits, f, &again);
    }
    free(lits);
    *vars = v;
    *clauses = count;
}
