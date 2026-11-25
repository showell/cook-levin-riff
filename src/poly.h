// poly.h: the general multivariate polynomial over ring_int, a port of
// poly.py's Poly (with its helpers _VarPower and _Term).
//
// A Poly is a list of terms, each a coefficient and a monomial: the
// variables it holds, each with an exponent of 1 or more, sorted by name
// (strcmp's order, which is Python's for ASCII and UTF-8). A Poly is always
// canonical, as Python's constructor makes it: like terms combined, zero
// terms dropped, and the terms in Python's order, descending by the tuple
// of exponents over the variables sorted by name. Printing (`poly_str`) is
// Python's `str` byte for byte.
//
// Layout: the terms are one array, and their variables one array beside
// it, each term naming its run (`first`, `count`). A variable's name is
// interned, so one name is one pointer for the program's life.
//
// Ownership: every function that gives a Poly gives a new one, which the
// caller frees with `poly_free`; the Polys passed in are only read, never
// taken. `poly_str` gives a string the caller frees with `free`. Python's
// shortcuts that hand back an operand (x + 0 is x) are copies here.
//
// Refusals stop the program with a message on stderr, as Python raises: an
// illegal variable name, a missing assignment in `poly_eval`, an unknown
// variable in `poly_apply` or `poly_substitute`, a negative exponent, and
// any coefficient overflow (ring_int.h).

#ifndef COOK_LEVIN_POLY_H
#define COOK_LEVIN_POLY_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name; // interned
    uint32_t exp;     // 1 or more
} VarPower;

typedef struct {
    int64_t coeff;  // never zero in a Poly
    uint32_t first; // its variables are vps[first .. first + count)
    uint32_t count;
} Term;

typedef struct {
    Term *terms;
    size_t len;
    VarPower *vps;
    size_t nvps;
} Poly;

// A variable's value, as Python's keyword arguments give them.
typedef struct {
    const char *name;
    int64_t value;
} Binding;

Poly poly_zero(void);
Poly poly_one(void);
Poly poly_constant(int64_t c);
// x, as Poly.var: a name holding none of ",*+/-()".
Poly poly_var(const char *name);
Poly poly_copy(const Poly *p);
void poly_free(Poly *p);

Poly poly_add(const Poly *p, const Poly *q);
Poly poly_sub(const Poly *p, const Poly *q);
Poly poly_neg(const Poly *p);
Poly poly_mul(const Poly *p, const Poly *q);
// p ** k as Python's p * p ** (k - 1): k - 1 multiplies, k >= 0.
Poly poly_pow(const Poly *p, int64_t k);

// The constant on either side: p + c, p * c, c - p (Python's __radd__,
// __rmul__ and __rsub__; `1 - x` is how the stepper's NOT builds).
Poly poly_add_int(const Poly *p, int64_t c);
Poly poly_mul_int(const Poly *p, int64_t c);
Poly poly_int_sub(int64_t c, const Poly *p);

// Every variable of p must be bound; bindings p lacks are ignored.
int64_t poly_eval(const Poly *p, const Binding *vals, size_t n);
// Partial evaluation: each binding must name a variable of p.
Poly poly_apply(const Poly *p, const Binding *vals, size_t n);
// p with `name` replaced by q; `name` must be a variable of p.
Poly poly_substitute(const Poly *p, const char *name, const Poly *q);

int poly_is_zero(const Poly *p);
int poly_is_one(const Poly *p);
// Python's ==, the canonical strings equal; canonical forms make that the
// terms equal.
int poly_equal(const Poly *p, const Poly *q);

char *poly_str(const Poly *p);

#endif
