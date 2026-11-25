// vm.c: the machine and its interpreter (vm.h).

#include "vm.h"

// The machine needs no libc but for its stops. The wasm build (web/nibble.c)
// defines VM_FREESTANDING: there a stop is a trap, which the page reports.
#ifdef VM_FREESTANDING
#define VM_STOP(...) __builtin_trap()
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VM_STOP(...) (fprintf(stderr, __VA_ARGS__), abort())
#endif

static const char *const OP_NAMES[NUM_OPS] = {"nada", "zero", "decr", "mod2"};

static const char *const OP_COMMENTS[NUM_OPS] = {
    "# do nothing",
    "# accept original input if AX = 0",
    "# reject zero or decrement AX",
    "# subtract 2 from AX if AX >= 2",
};

static void vm_die(const char *what, long value) {
    (void)what;
    (void)value;
    VM_STOP("vm: %s: %ld\n", what, value);
}

static int same_name(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void check_op(Op op) {
    if ((int)op < 0 || (int)op >= NUM_OPS) vm_die("unknown op", (long)op);
}

const char *op_name(Op op) {
    check_op(op);
    return OP_NAMES[op];
}

const char *op_comment(Op op) {
    check_op(op);
    return OP_COMMENTS[op];
}

Op op_from_name(const char *name) {
    for (int i = 0; i < NUM_OPS; i++) {
        if (same_name(OP_NAMES[i], name)) return (Op)i;
    }
    VM_STOP("vm: unknown op name: %s\n", name);
    return OP_NADA; // not reached; tcc can't see that abort doesn't return
}

Machine vm_start(int n) {
    Machine m = {n, 0, STATUS_NONE, 0};
    return m;
}

Machine vm_step(Machine m, Op op) {
    if (m.halted) return m;
    switch (op) {
    case OP_NADA:
        break;
    case OP_ZERO:
        if (m.ax == 0) {
            m.halted = 1;
            m.status = STATUS_ACCEPTED;
        }
        break;
    case OP_DECR:
        if (m.ax == 0) {
            m.halted = 1;
            m.status = STATUS_REJECTED;
        } else {
            m.ax -= 1;
        }
        break;
    case OP_MOD2:
        m.ax = m.ax % 2;
        break;
    default:
        vm_die("unknown op", (long)op);
    }
    return m;
}

Status run_progam(int n, Program program) {
    int ax;
    return run_progam_ax(n, program, &ax);
}

Status run_progam_ax(int n, Program program, int *final_ax) {
    Machine m = vm_start(n);
    for (int i = 0; i < MAX_STEPS; i++) m = vm_step(m, program.ops[i]);
    *final_ax = m.ax;
    return m.status;
}

uint32_t assemble(const Op *ops, size_t count) {
    if (count > 16) vm_die("assemble: too many ops for 32 bits", (long)count);
    uint32_t sum = 0;
    uint32_t place = 1;
    for (size_t i = 0; i < count; i++) {
        check_op(ops[i]);
        sum += (uint32_t)ops[i] * place;
        place *= 4;
    }
    return sum;
}

Program disassemble(uint32_t n) {
    Program program;
    for (int i = 0; i < MAX_STEPS; i++) {
        program.ops[i] = (Op)(n % 4);
        n = n / 4;
    }
    return program;
}

uint32_t encoded_language(Lang lang) {
    uint32_t code = 0;
    for (size_t i = 0; i < lang.len; i++) {
        int x = lang.items[i];
        if (x < 0 || x >= 32) vm_die("encoded_language: member out of range", (long)x);
        code += (uint32_t)1 << x;
    }
    return code;
}

Lang language(uint32_t code) {
    Lang lang = {{0}, 0};
    int i = 0;
    while (code) {
        if (code % 2 == 1) lang.items[lang.len++] = i;
        code = code / 2;
        i += 1;
    }
    return lang;
}

Lang get_language_that_program_accepts(Program program) {
    Lang lang = {{0}, 0};
    for (int i = 0; i < NUM_INPUTS; i++) {
        if (run_progam(i, program) == STATUS_ACCEPTED) lang.items[lang.len++] = i;
    }
    return lang;
}

int lang_has(Lang lang, int x) {
    for (size_t i = 0; i < lang.len; i++) {
        if (lang.items[i] == x) return 1;
    }
    return 0;
}

static void vm_test_check(int ok, const char *what) {
    (void)what;
    if (!ok) VM_STOP("vm test failed: %s\n", what);
}

void vm_test(void) {
    Op zd[2] = {OP_ZERO, OP_DECR};
    vm_test_check(assemble(zd, 2) == 9, "assemble([zero, decr]) == 9");
    Program p = disassemble(9);
    Op want[MAX_STEPS] = {OP_ZERO, OP_DECR, OP_NADA, OP_NADA, OP_NADA, OP_NADA};
    for (int i = 0; i < MAX_STEPS; i++) vm_test_check(p.ops[i] == want[i], "disassemble(9)");

    Lang empty = {{0}, 0};
    vm_test_check(encoded_language(empty) == 0, "encoded_language([]) == 0");
    vm_test_check(language(0).len == 0, "language(0) == []");
    Lang odd = {{1, 3}, 2};
    vm_test_check(encoded_language(odd) == 10, "encoded_language([1, 3]) == 10");
    Lang ten = language(10);
    vm_test_check(ten.len == 2 && ten.items[0] == 1 && ten.items[1] == 3, "language(10) == [1, 3]");
}

Lang complement(Lang lang) {
    Lang out = {{0}, 0};
    for (int i = 0; i < NUM_INPUTS; i++) {
        if (!lang_has(lang, i)) out.items[out.len++] = i;
    }
    return out;
}
