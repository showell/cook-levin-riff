// census.h: the shortest Nibble++ program for each of the 65,536 languages,
// exactly, by breadth-first search over the machine's states.
//
// A state is the whole machine on all 16 inputs at once: for each input,
// its AX (0 to 15), or accepted, or rejected. Two prefixes that reach the
// same state have the same futures, so the search keeps each state once,
// at the length that first reaches it. A prefix's language is the set of
// inputs in the accepted state, so a language's shortest length is the
// first level of the search where a state with that language appears.

#ifndef COOK_LEVIN_CENSUS_H
#define COOK_LEVIN_CENSUS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "plus.h"

// An input's place in a state: AX, or one of these two.
enum { CENSUS_ACCEPTED = 16, CENSUS_REJECTED = 17 };

// The longest a search runs; a language left unreached past it stops the
// census, naming it.
enum { CENSUS_MAX_LENGTH = 64 };

// An opcode set as bits, bit k for the op with code k.
#define CENSUS_OP(op) (1u << (op))

typedef struct {
    unsigned ops;
    // Each state found, in the order found, which is by length.
    uint8_t (*states)[PLUS_INPUTS];
    int32_t *parent;  // the state one line shorter, -1 for the start
    uint8_t *op;      // the line that led here from the parent
    size_t num_states;
    size_t cap_states;
    // Open addressing over state indices, -1 empty.
    int32_t *table;
    size_t table_size;
    // Per language: its shortest length, and the first state that decides it.
    int8_t shortest[PLUS_LANGUAGES];
    int32_t reached_by[PLUS_LANGUAGES];
    // The levels searched, and the count of languages at each length.
    int levels;
    long histogram[CENSUS_MAX_LENGTH + 1];
} Census;

// Runs the search for an opcode set; every language must be reached, or the
// census stops the program naming one that was not.
void census_run(Census *c, unsigned ops);
void census_free(Census *c);

// Writes the witness for a language to `ops` (room for its shortest length)
// and gives its length.
int census_witness(const Census *c, uint32_t lang, PlusOp *ops);

// The opcode set's name, its ops in the order zero, decr, mod2, half,
// joined by commas: "zero,decr,mod2".
void census_ops_name(unsigned ops, char *out, size_t size);

// The census report: one header line, then each length's count.
void census_print(FILE *out, const Census *c);

#endif
