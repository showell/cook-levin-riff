// sat.h: a problem as a SAT formula, in DIMACS CNF.
//
// This is the Cook-Levin construction, on a machine small enough to see
// whole: "some program of six instructions accepts exactly language L" is
// written as one Boolean formula, in conjunctive normal form, that is
// satisfiable exactly when such a program exists, and whose satisfying
// assignments are those programs. The machine is unrolled: each of its six
// steps on each of the four inputs is a copy of the step's logic, wired to
// the state the step before it left, and the formula asks that every
// input's final `accepted` be what the language wants.
//
// The variables. The 12 program bits come first, numbered as compose.h
// numbers them less one (compose's bits 0 and 1 are the input, which here is
// a constant in each input's copy): DIMACS variable 2i - 1 is instruction
// i's hb and 2i its lb, for i in 1..6, op i being 2 * hb + lb. So a model
// maps straight to a program: op i is 2 * v[2i - 1] + v[2i], and the
// program's code (vm.h's `assemble`) is the sum of op i * 4^(i - 1).
// `sat_program_code` reads it so. Every variable after the 12 is a Tseitin
// variable: one gate's output, an AND or an OR of two literals, tied to its
// inputs by three clauses (c = a AND b: -c a, -c b, c -a -b; c = a OR b:
// c -a, c -b, -c a b), and NOT is a literal's sign, no variable at all. The
// unrolled machine's state, AX's two bits, halted and accepted for each
// input after each of the six steps, is among those gates' outputs; the
// writer names each one in a `c` line.
//
// The step is the stepper's. `sat_step` is construct_polynomials
// (stepper.c) line for line, the same gates in the same order, with NOT,
// AND and OR the gates the polynomials 1 - x, x * y and x + y - x * y stand
// for on 0/1 values. Two things make the formula smaller, and neither
// changes a gate's function: a gate of a constant is folded (x AND FALSE is
// FALSE, x OR FALSE is x, x AND x is x, x AND NOT x is FALSE, and the same
// for OR), and a gate already made from the same inputs is reused
// (structural hashing), so the op's decoding is made once a step and shared
// by the four inputs. With every input a constant, folding evaluates
// `sat_step`, and tests/test_sat.c holds that to the stepper on all 64 of
// its inputs.
//
// The proof that the encoding is right is tests/test_sat.c's: for every
// puzzle, every one of the 4,096 assignments of the program bits is unit-
// propagated through the CNF, which determines every Tseitin variable, and
// the assignments that satisfy it are exactly the programs the machine says
// solve the puzzle.

#ifndef COOK_LEVIN_SAT_H
#define COOK_LEVIN_SAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "vm.h"

// A literal: a DIMACS literal (a nonzero variable, negative for its NOT), or
// one of the two constants, which never reach a clause.
typedef int Lit;
#define SAT_TRUE 0x7fffffff
#define SAT_FALSE (-SAT_TRUE)

enum { SAT_PROGRAM_VARS = 2 * MAX_STEPS };

// Instruction i's (1..MAX_STEPS) bits as DIMACS variables.
int sat_op_hb(int i);
int sat_op_lb(int i);

// The machine between steps, as literals.
typedef struct {
    Lit hb;
    Lit lb;
    Lit halted;
    Lit accepted;
} SatState;

typedef enum { GATE_AND, GATE_OR } GateKind;

typedef struct {
    GateKind kind;
    Lit a;
    Lit b; // a < b
    int out;
} SatGate;

// A puzzle as the encoder takes it: the language (vm.h's code, a subset of
// the inputs as bits), lines past `max_len` (0..MAX_STEPS) forced to nada,
// and the lines in `fixed` (line k, from 1, its bit 1 << (k - 1), as
// web/puzzles.c's) locked to their op in program `fixed_ops`.
typedef struct {
    int lang;
    int max_len;
    int fixed;
    uint32_t fixed_ops;
} SatPuzzle;

// A puzzle with no bound and nothing fixed.
SatPuzzle sat_puzzle(int lang);

typedef struct {
    int num_vars;
    int *lits; // the clauses, one after another, each ended by 0
    size_t len;
    size_t cap;
    size_t num_clauses;
    SatGate *gates;
    size_t num_gates;
    size_t gates_cap;
    // The gates by (kind, a, b), open addressing: a gate's index + 1, 0
    // empty, `index_cap` a power of 2 kept at least twice num_gates. It
    // only finds a gate faster; which gate is found is the same.
    int *index;
    size_t index_cap;
    // trace[n][k]: input n's state after step k (k = 0 the start).
    SatState trace[NUM_INPUTS][MAX_STEPS + 1];
} Cnf;

// A CNF of the 12 program variables and no clauses.
Cnf sat_new(void);
void sat_free(Cnf *c);

// A clause; a FALSE literal is left out, and a clause holding TRUE is
// dropped. A clause left empty is written as (1) and (-1), so the CNF stays
// plain DIMACS and is unsatisfiable.
void sat_clause(Cnf *c, const Lit *lits, size_t n);
void sat_unit(Cnf *c, Lit x);

Lit sat_not(Lit x);
Lit sat_and(Cnf *c, Lit x, Lit y);
Lit sat_or(Cnf *c, Lit x, Lit y);

// One step of the machine: construct_polynomials' gates.
SatState sat_step(Cnf *c, SatState s, Lit op_hb, Lit op_lb);

// The whole puzzle: the locks, the four inputs' six steps, and each input's
// final accepted held to the language.
Cnf sat_encode(const SatPuzzle *p);

// DIMACS: `c` lines naming the program bits and each state's literals, then
// `p cnf V C` and the clauses.
int sat_write_dimacs(FILE *f, const Cnf *c, const SatPuzzle *p);

// The program a model's first 12 variables name: bit v - 1 of `bits` is
// variable v's value. And back.
uint32_t sat_program_code(uint32_t bits);
uint32_t sat_program_bits(uint32_t code);

// What the machine says: program `code` accepts exactly `lang`.
int sat_solves(int lang, uint32_t code);

#endif
