// gates.h: one roll of the rolling cube as a circuit of AND, OR and NOT
// gates, on the netlist of src/circuit.h, and k rolls unrolled into one
// circuit and its CNF by Tseitin's encoding. cube/GATES.md has the parts,
// their gate counts and the sizes.
//
// The state is one-hot, a wire a fact: cell c's wire is on when the cube is
// on c, face f's when the painted face points f, then a wire a tile (on when
// lit) and the halted wire. A roll is its two bits, N 00, E 01, S 10, W 11.
// One-hot makes each rule a piece of wiring: a roll carries the cube from a
// cell's wire to its neighbour's, and turns the paint from one face's wire to
// another's, so no part does arithmetic.
//
// The parts of one roll, in the order they are made:
//
//   decode   the roll's two bits as four wires, N, E, S and W, one on.
//   halt     off: the roll leaves the grid (the cube on the edge it rolls
//            over); halted' = halted OR off; stay = halted', the cube stands
//            still; go r = roll r AND NOT stay, the roll that happens.
//   move     cell c' = (go N AND the cell south of c) OR (go E AND the cell
//            west of c) OR ... OR (cell c AND stay): the cube arrives from a
//            neighbour or stays put. A neighbour off the grid is no term.
//   paint    face g' = (go r AND the face that turns into g on roll r) OR ...
//            for the four rolls, OR (face g AND stay).
//   flip     land = face down' AND NOT stay; tile c' = tile c XOR (cell c'
//            AND land), the XOR as (a OR b) AND NOT (a AND b).
//
// The geometry here is written from the grid's rows and columns and a table
// of the paint's turns, not from cube.c, so tests/test_cube_gates.c's check
// against cube.c's cube_step is a check of two writings of the rules.

#ifndef COOK_LEVIN_CUBE_GATES_H
#define COOK_LEVIN_CUBE_GATES_H

#include <stdint.h>
#include <stdio.h>

#include "../src/circuit.h"
#include "cube.h"

enum { CUBE_MAX_TILES = CUBE_MAX_N * CUBE_MAX_N };

// The parts' names, in the order they are made.
enum { CUBE_PARTS = 5 };
extern const char *const CUBE_PART_NAMES[CUBE_PARTS];

// The state's wires.
typedef struct {
    int cell[CUBE_MAX_TILES];
    int face[CUBE_FACES];
    int tile[CUBE_MAX_TILES];
    int halted;
} CubeWires;

// The number of a state's wires on an n x n grid: n * n cells, six faces,
// n * n tiles and halted.
int cube_state_wires(int n);

// One roll on the state `s`, the roll's bits `hi` and `lo`; the next state
// in *next. Its gates are in the parts named "<prefix>decode" and so on.
void part_cube_roll(Net *net, const char *prefix, int n, const CubeWires *s, int hi, int lo, CubeWires *next);

// One roll alone: the inputs, made in this order, are the cells (a1 first,
// row by row), the faces (cube.h's order: up, down, north, south, east,
// west), the tiles, halted, then the roll's hi and lo bits; the ports, part
// "", are each state wire's name and a prime: "cell a1'", "face down'",
// "tile c2'", "halted'". It also records how the five parts are wired, in
// scope "", for net_boxes_dot.
void cube_roll_circuit(Net *net, int n);

// A wire's name in a state: "cell a1", "face up", "tile b2", "halted".
// `i` counts the state's wires in cube_roll_circuit's input order.
void cube_state_wire_name(int n, int i, char *out, size_t size);

// ── k rolls unrolled ────────────────────────────────────────────────────

// A flat circuit too big for a Net: k copies of one roll's gates, each
// copy's state inputs wired to the copy before it, and a match part on the
// last tiles. Wires 0 and 1 are the constants; then the 2k roll bits (roll
// t's hi at 2t, its lo at 2t + 1, t from 1), then the start state's wires in
// cube_roll_circuit's order, then the gates.
typedef struct {
    int n, k;
    uint32_t target;
    int num_wires, cap;
    unsigned char *kind; // WireKind: W_ZERO, W_ONE, W_INPUT, W_NOT, W_AND, W_OR
    int *a, *b;
    int num_inputs;
    int start; // the start state's first wire
    int out;   // true exactly when the last tiles are `target`
    // The last state's wires, in cube_roll_circuit's input order.
    int last[2 * CUBE_MAX_TILES + CUBE_FACES + 1];
} CubeChain;

// Builds the chain from one roll's netlist, copied k times.
CubeChain cube_chain(int n, int k, uint32_t target);
void cube_chain_free(CubeChain *c);

// Every wire's value: the rolls (letters N, E, S, W, k of them) on the roll
// bits, the start a1 with the face up, every tile dark, not halted.
void cube_chain_eval(const CubeChain *c, const char *rolls, unsigned char *values);

// The chain's CNF by Tseitin's encoding: a variable for each input (the
// roll bits are variables 1 to 2k, roll t's hi 2t - 1, so a model reads
// off as cube_sat.c's does) and each AND and OR gate; a NOT is its input's
// literal negated, no variable. Each AND and OR is three clauses; the start
// state is a unit clause a wire, and the output one more. *vars and
// *clauses get the sizes; `f` NULL only counts.
void cube_chain_dimacs(const CubeChain *c, FILE *f, int *vars, long *clauses);

// The literal of each wire in that CNF (0 for a constant), for the tests.
// `lits` holds c->num_wires.
void cube_chain_literals(const CubeChain *c, int *lits);

#endif
