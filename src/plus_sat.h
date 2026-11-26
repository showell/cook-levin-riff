// plus_sat.h: Nibble++ (plus.h) as a circuit and as CNF, on sat.h's gates.
//
// The toy's encoder, sat.c, builds its CNF from AND and OR gates with NOT a
// literal's sign, constants folded and a gate made twice reused. This file
// is a second machine on the same gates: the Nibble++ step on a 4-bit AX,
// unrolled for a k-line program.
//
// The variables. Line i's (1..k) two bits come first, as sat.c numbers
// them: variable 2i - 1 is its high bit and 2i its low bit, the op being
// 2 * hb + lb (plus.h's codes: half 0, zero 1, decr 2, mod2 3). The circuit
// of one run (`plus_sat_circuit`) then has the input's four bits, variable
// 2k + 1 + j for AX's bit j (value 2^j); every variable after those is a
// Tseitin variable, one gate's output.
//
// The step, on AX's bits a0..a3, `halted` and `accepted`, and the line's
// bits h and l:
//
//   is_zero    NOT (a0 OR a1 OR a2 OR a3)
//   live       NOT halted
//   accepted'  accepted OR (live AND op = zero AND is_zero)
//   halted'    halted OR (live AND (op = zero OR op = decr) AND is_zero)
//   AX'        by the op: half (a1, a2, a3, 0), zero AX, decr AX - 1 (a
//              borrow chain, bit j' = a_j XOR "every bit below j is 0"),
//              mod2 (a0, 0, 0, 0); each bit a 4-way multiplexer on (h, l).
//
// AX' is not held once the run halts: a halted run's AX is read by nothing
// but the gates of later steps, and each of those is gated by `live`, so
// `accepted` and `halted` are the interpreter's whatever AX does. (decr at 0
// halts and wraps AX to 15; a later decr and zero cannot accept it, as
// `live` is 0.) A run that ends without halting rejects: the output is the
// last step's `accepted`.

#ifndef COOK_LEVIN_PLUS_SAT_H
#define COOK_LEVIN_PLUS_SAT_H

#include "plus.h"
#include "sat.h"

enum { PLUS_AX_BITS = 4 };

// Line i's (1..k) bits as DIMACS variables, and AX's bit j (0..3) in the
// circuit of one run of a k-line program.
int plus_sat_hb(int i);
int plus_sat_lb(int i);
int plus_sat_ax(int k, int j);

// The machine between steps, as literals: AX's bits, bit j worth 2^j.
typedef struct {
    Lit ax[PLUS_AX_BITS];
    Lit halted;
    Lit accepted;
} PlusSatState;

// One step: the gates above, on line bits h and l.
PlusSatState plus_sat_step(Cnf *c, PlusSatState s, Lit h, Lit l);

// The circuit of one run of a k-line program: 2k program variables, then
// AX's four, then the gates. Gives the output literal, `accepted` after k
// steps, which may be a constant (k = 0).
Lit plus_sat_circuit(Cnf *c, int k);

// The search for a k-line program that decides `lang` (bit n for input n):
// the 2k program variables shared by 16 copies of the run, input n's AX
// the constant n, and each copy's output held to its bit of `lang` by a
// unit clause. Satisfiable exactly when some k-line program decides it.
Cnf plus_sat_search(uint32_t lang, int k);

// A model's program: line i's op from variables 2i - 1 and 2i, `bits[v]`
// being variable v's value (index 0 unused).
void plus_sat_ops(const signed char *bits, int k, PlusOp *ops);

#endif
