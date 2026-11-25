// stepper.h: one step of the machine as four polynomials in six 0/1
// variables, a port of Steve's stepper.py. AX is 2*hb + lb, the op is
// 2*op_hb + op_lb; NOT, AND and OR are 1 - x, x*y and (x + y) - x*y, exact
// on 0/1 values. The polynomials are the general Poly (poly.h), unreduced,
// so they carry squares: hb' has 786 terms and lb' 2,979, and their `str`
// is Python's byte for byte (tests/golden/step_*.txt).
//
// The stepper's `halted` is Python's, kept as is (Steve's call): a `zero`
// that accepts does not set it (check_halts is FALSE), so after an accept a
// later `decr` at AX = 0 can set it. Nothing reads it; the verdict is
// `accepted`, which agrees with the interpreter on every program.

#ifndef COOK_LEVIN_STEPPER_H
#define COOK_LEVIN_STEPPER_H

#include <stdint.h>

#include "poly.h"
#include "vm.h"

// construct_polynomials' four results, in Python's order.
typedef struct {
    Poly hb;
    Poly lb;
    Poly halted;
    Poly accepted;
} StepPolys;

// One step built from the six inputs, as Python builds it, every
// intermediate freed; the caller frees the result with `step_polys_free`.
StepPolys construct_polynomials(const Poly *hb, const Poly *lb, const Poly *halted, const Poly *accepted,
                                const Poly *op_hb, const Poly *op_lb);
void step_polys_free(StepPolys *s);

// STEP_POLYNOMIALS: construct_polynomials of the six variables named
// hb, lb, halted, accepted, op_hb and op_lb, built once at the first call
// and kept for the program's life.
const StepPolys *step_polynomials(void);

// What `step` gives: Python's (AX, halted, accepted).
typedef struct {
    int ax;
    int halted;
    int accepted;
} StepState;

// Python's step: AX 0..3, halted and accepted 0 or 1, op 0..3 (a value out
// of range stops the program, as Python's asserts do). Each of the four
// step polynomials is evaluated by `poly_eval`, as Python's `eval`.
StepState step(int ax, int halted, int accepted, Op op);

// Python's test_with_stepper: six steps from halted = accepted = 0; gives
// accepted, 0 or 1.
int test_with_stepper(Program program, int ax);

// ── The fast path ───────────────────────────────────────────────────────
//
// `poly_eval` finds each variable by name (strcmp over the bindings), which
// is fine for Python's 16 example programs but slow for all 4,096. A
// compiled step polynomial is the same terms over a 6-slot value array: a
// term is its coefficient and a mask of the slots it holds (any exponent).
// On 0/1 values x**e is x, so a term counts its coefficient when every slot
// of its mask is 1. Exact on 0/1 values, the only ones `step` takes; held
// equal to `poly_eval` on all 64 assignments (tests/test_stepper.c).

enum { SLOT_HB, SLOT_LB, SLOT_HALTED, SLOT_ACCEPTED, SLOT_OP_HB, SLOT_OP_LB, NUM_SLOTS };

typedef struct {
    int64_t coeff;
    uint8_t mask; // bit s set when the term holds slot s's variable
} CompiledTerm;

typedef struct {
    CompiledTerm *terms;
    size_t len;
} CompiledPoly;

// Each variable of p must be one of the six slots' names.
CompiledPoly compile_step_poly(const Poly *p);
void compiled_free(CompiledPoly *c);
// vals[s] is slot s's value, each 0 or 1.
int64_t compiled_eval(const CompiledPoly *c, const int64_t vals[NUM_SLOTS]);

// `step` and `test_with_stepper` over the compiled step polynomials, built
// once from step_polynomials() at the first call.
StepState step_compiled(int ax, int halted, int accepted, Op op);
int test_with_compiled_stepper(Program program, int ax);

#endif
