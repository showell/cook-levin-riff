// plus.h: Nibble++, Nibble's bigger sibling. AX is 4 bits (inputs 0 to 15),
// programs are straight lines of any length, and the four opcodes are
// `half`, `zero`, `decr` and `mod2`, two bits a line. `half` takes the code
// 0 that Nibble's `nada` has; the other three keep Nibble's codes and rules.
// This file is the plain interpreter: one input, one AX, one line at a
// time. census.c searches the machine's states its own way, and the tests
// hold its witnesses to this interpreter.

#ifndef COOK_LEVIN_PLUS_H
#define COOK_LEVIN_PLUS_H

#include <stdint.h>

enum { PLUS_INPUTS = 16, PLUS_LANGUAGES = 65536, PLUS_NUM_OPS = 4 };

// The longest program a 64-bit word holds, two bits a line.
enum { PLUS_MAX_LINES = 32 };

typedef enum { PLUS_HALF = 0, PLUS_ZERO = 1, PLUS_DECR = 2, PLUS_MOD2 = 3 } PlusOp;

const char *plus_op_name(PlusOp op);

// Runs `ops` on input `n`. `zero` accepts and halts when AX is 0; `decr`
// rejects and halts when AX is 0, and otherwise takes 1 from AX; `mod2` sets
// AX to AX % 2; `half` sets AX to AX / 2, rounding down. A run that passes
// the last line rejects. Gives 1 when the run accepts, else 0.
int plus_accepts(int n, const PlusOp *ops, int len);

// The language `ops` decide: bit n set when input n is accepted.
uint32_t plus_language(const PlusOp *ops, int len);

// A program of up to 32 lines as a 64-bit word, line i at bits 2i and
// 2i + 1, and back. A word always holds 32 lines; `plus_unpack` writes all
// of them.
uint64_t plus_pack(const PlusOp *ops, int len);
void plus_unpack(uint64_t word, PlusOp *ops);

#endif
