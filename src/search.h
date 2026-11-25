// search.h: find_solutions, the search over all 4,096 programs and its
// report, as virtual_machine.py prints it.

#ifndef COOK_LEVIN_SEARCH_H
#define COOK_LEVIN_SEARCH_H

#include <stdio.h>

#include "vm.h"

// Every program number, bucketed by the code of the language it accepts,
// ascending within a bucket.
typedef struct {
    int programs[NUM_LANGUAGES][NUM_PROGRAMS];
    size_t count[NUM_LANGUAGES];
} Solutions;

void solutions_fill(Solutions *s);

// Writes Python's lists: `[]`, `[0, 1, 3]`.
void print_lang(FILE *out, Lang lang);

// The search and its report, to `out`.
void find_solutions(FILE *out);

#endif
