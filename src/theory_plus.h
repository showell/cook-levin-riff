// theory_plus.h: the shortest Nibble++ program for every formal language,
// from a recursion instead of a search. docs/NIBBLE_PLUS_THEORY.md states
// and proves it; tests/test_theory_plus.c holds it to the census
// (census.h) on all 65,536 formal languages for each opcode set.
//
// The recursion splits a program at its first `mod2` or `half`. Before it
// stands a walk, `zero` and `decr` alone, with d `decr`s; the inputs still
// running then have AX n - d. The `mod2` or `half` maps those values onto
// fewer, and the inputs that share a value behave alike from then on, so
// the rest of the program is a program on a smaller machine whose inputs
// are those values. The smaller machine's inputs are 0 to N' - 1, and its
// shortest lengths are the same recursion's, one size down.

#ifndef COOK_LEVIN_THEORY_PLUS_H
#define COOK_LEVIN_THEORY_PLUS_H

#include <stdint.h>

#include "plus.h"

// How a size-N formal language's shortest program begins: a walk alone
// (`op` is -1), or a walk of `d` `decr`s, a `zero` after the last `decr`
// when `zero_at_d`, then `op` (PLUS_MOD2 or PLUS_HALF) and the rest, a
// program for `rest` on `rest_size` inputs.
typedef struct {
    int8_t op;
    int8_t d;
    int8_t zero_at_d;
    int8_t rest_size;
    uint16_t rest;
} TheoryChoice;

typedef struct {
    unsigned ops;
    // For each size N from 1 to 16, each formal language over inputs 0 to
    // N - 1: its shortest length, and the choice that reaches it.
    int8_t *shortest[PLUS_INPUTS + 1];
    TheoryChoice *choice[PLUS_INPUTS + 1];
} Theory;

// Fills every size's table for an opcode set, which always holds `zero`
// and `decr`.
void theory_build(Theory *t, unsigned ops);
void theory_free(Theory *t);

// The 16-input shortest length, from the recursion.
int theory_shortest(const Theory *t, uint32_t lang);

// Writes the recursion's program for a 16-input formal language to `ops`
// (room for CENSUS_MAX_LENGTH lines) and gives its length.
int theory_witness(const Theory *t, uint32_t lang, PlusOp *ops);

// max(S) + |S|, the {zero, decr} length, 0 for the empty formal language;
// over any number of inputs.
int theory_walk_length(uint32_t lang);

// The {zero, decr, mod2} closed form on 16 inputs: the least of the walk,
// d + |S below d| + 2 when S from d on is the inputs of d's parity, and
// d + |S below d| + 4 when S holds every input from d on.
int theory_zdm_closed(uint32_t lang);

#endif
