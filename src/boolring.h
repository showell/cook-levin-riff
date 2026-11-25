// boolring.h: the Boolean ring beside the general Poly. A multilinear
// polynomial over up to 32 variables that are only ever 0 or 1, so x² = x:
// a monomial is a uint32_t mask of the variables it holds, and multiplying
// two monomials is their masks' OR. Coefficients are int64, every add and
// multiply checked by ring_int (an overflow stops the program, named).
//
// A BPoly is always canonical: its terms sorted ascending by mask, each mask
// once, no zero coefficient. A 0/1 function of n variables has exactly one
// such polynomial, so two BPolys are equal exactly when their terms are.
//
// The variables have no names inside a BPoly; bit i is variable i of a
// BVars table, which `bp_from_poly` and `bp_str` take.
//
// Ownership: every function that gives a BPoly gives a new one, which the
// caller frees with `bp_free`; BPolys passed in are only read. `bp_str`
// gives a string the caller frees with `free`.

#ifndef COOK_LEVIN_BOOLRING_H
#define COOK_LEVIN_BOOLRING_H

#include <stddef.h>
#include <stdint.h>

#include "poly.h"

enum { BP_MAX_VARS = 32 };

typedef struct {
    uint32_t mask; // bit i set when the term holds variable i
    int64_t coeff; // never zero in a BPoly
} BTerm;

typedef struct {
    BTerm *terms;
    size_t len;
} BPoly;

// The variables' names, bit i named names[i].
typedef struct {
    const char *names[BP_MAX_VARS];
    int count;
} BVars;

BPoly bp_zero(void);
BPoly bp_constant(int64_t c);
// Variable `bit`, 0 <= bit < BP_MAX_VARS.
BPoly bp_var(int bit);
BPoly bp_copy(const BPoly *p);
void bp_free(BPoly *p);

BPoly bp_add(const BPoly *p, const BPoly *q);
BPoly bp_sub(const BPoly *p, const BPoly *q);
BPoly bp_neg(const BPoly *p);
// Every pair of terms, masks ORed, coefficients multiplied and summed.
BPoly bp_mul(const BPoly *p, const BPoly *q);

// A general Poly with x**e cut to x: each of p's variables must be named in
// `vars` (an unknown name stops the program). Exact on 0/1 values.
BPoly bp_from_poly(const Poly *p, const BVars *vars);

// Composition: p with variable i replaced by vals[i], for i < n; p may hold
// no variable at or past n.
BPoly bp_substitute(const BPoly *p, const BPoly *vals, int n);

// p's value when the variables in `ones` are 1 and the rest 0.
int64_t bp_eval(const BPoly *p, uint32_t ones);

int bp_equal(const BPoly *p, const BPoly *q);
// The largest |coefficient|, 0 for the zero polynomial.
int64_t bp_max_abs_coeff(const BPoly *p);
// The most variables in one term.
int bp_degree(const BPoly *p);

// Readable and canonical: terms by degree, then by their variables' indices
// compared as lists (h0 before l0 before op1_hb ...); `+` between terms, a
// negative coefficient in parentheses and a coefficient of 1 left out, as
// poly.h prints (`1+(-1)*h0+h0*l0`); the zero polynomial is `0`.
char *bp_str(const BPoly *p, const BVars *vars);

#endif
