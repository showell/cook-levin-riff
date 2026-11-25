// compose.h: the whole program as polynomials. Six steps of the machine
// composed symbolically in the Boolean ring (boolring.h), over 14 0/1
// variables: the input's bits h0 and l0 (AX = 2*h0 + l0) and each
// instruction's two bits, op{i}_hb and op{i}_lb for i in 1..6 (op i is
// 2*op{i}_hb + op{i}_lb). halted and accepted start at 0.
//
// Step k substitutes the state so far (four polynomials in the 14) for the
// step polynomials' hb, lb, halted and accepted, and step k's own two
// instruction variables for op_hb and op_lb. After six steps, `accepted` is
// the polynomial "does program P accept input X"; hb and lb are the final
// AX's bits; halted is the stepper's halted (stepper.h: an accept does not
// set it).

#ifndef COOK_LEVIN_COMPOSE_H
#define COOK_LEVIN_COMPOSE_H

#include <stdint.h>

#include "boolring.h"
#include "vm.h"

// Bit 0 is h0, bit 1 l0, then op1_hb, op1_lb, ..., op6_hb, op6_lb.
enum { COMPOSE_H0 = 0, COMPOSE_L0 = 1, COMPOSE_VARS = 2 + 2 * MAX_STEPS };

typedef struct {
    BPoly hb;
    BPoly lb;
    BPoly halted;
    BPoly accepted;
} BState;

BVars compose_vars(void);
// Instruction i's (1..MAX_STEPS) bits.
int compose_op_hb(int i);
int compose_op_lb(int i);

// The state before any step: hb = h0, lb = l0, halted = accepted = 0.
BState compose_start(void);
// The state after step i (1..MAX_STEPS) from the state before it.
BState compose_step(const BState *before, int i);
void bstate_free(BState *s);

// The 14-bit vector of an input (0..3) and a program, as bp_eval takes it.
uint32_t compose_ones(int input, Program program);
// Its inverse: the input and program a vector names.
int compose_input_of(uint32_t ones);
Program compose_program_of(uint32_t ones);

#endif
