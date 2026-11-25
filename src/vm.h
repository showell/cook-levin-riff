// vm.h: the machine. One 2-bit register, AX (0 to 3), four opcodes, and
// programs of exactly MAX_STEPS instructions. A port of Steve's
// virtual_machine.py; names follow it, `run_progam`'s typo included.

#ifndef COOK_LEVIN_VM_H
#define COOK_LEVIN_VM_H

#include <stddef.h>
#include <stdint.h>

enum { MAX_STEPS = 6, NUM_INPUTS = 4, NUM_OPS = 4, NUM_LANGUAGES = 16 };

// The number of programs: 4 ** MAX_STEPS.
enum { NUM_PROGRAMS = 4096 };

// An opcode is its code: the base-4 digit `assemble` writes.
typedef enum { OP_NADA = 0, OP_ZERO = 1, OP_DECR = 2, OP_MOD2 = 3 } Op;

typedef struct {
    Op ops[MAX_STEPS];
} Program;

// What `run_progam` gives, Python's three values: None (ran off the end,
// an implicit reject), True (accepted by `zero`) and False (rejected by
// `decr` at 0). Callers test only whether it is STATUS_ACCEPTED.
typedef enum { STATUS_NONE, STATUS_ACCEPTED, STATUS_REJECTED } Status;

// A language: a subset of the inputs, its members ascending, as Python's
// list. `language` of a code with bits past the 32nd is refused.
typedef struct {
    int items[32];
    size_t len;
} Lang;

const char *op_name(Op op);
const char *op_comment(Op op);
// The op a name spells; an unknown name stops the program, as Python's
// `OPS[op]` raises.
Op op_from_name(const char *name);

Status run_progam(int n, Program program);
// The same run, and AX when it ended: when it halted (the ops after a halt
// are skipped, so AX keeps that value) or after the last op.
Status run_progam_ax(int n, Program program, int *final_ax);

// Each op is base-4 digit i, least significant first. Takes any number of
// ops, as Python's does (`assemble(["zero", "decr"])` is 9).
uint32_t assemble(const Op *ops, size_t count);
// Always MAX_STEPS ops; the missing high digits are `nada`.
Program disassemble(uint32_t n);

uint32_t encoded_language(Lang lang);
Lang language(uint32_t code);
Lang get_language_that_program_accepts(Program program);
Lang complement(Lang lang);
int lang_has(Lang lang, int x);

// Python's `test()`: its six asserts, each a loud stop when it fails.
void vm_test(void);

#endif
