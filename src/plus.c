// plus.c: the Nibble++ interpreter (plus.h).

#include "plus.h"

#include <stdio.h>
#include <stdlib.h>

static const char *const PLUS_OP_NAMES[PLUS_NUM_OPS] = {"half", "zero", "decr", "mod2"};

const char *plus_op_name(PlusOp op) {
    if ((int)op < 0 || (int)op >= PLUS_NUM_OPS) {
        fprintf(stderr, "plus_op_name: no op has code %d\n", (int)op);
        abort();
    }
    return PLUS_OP_NAMES[op];
}

int plus_accepts(int n, const PlusOp *ops, int len) {
    int ax = n;
    for (int i = 0; i < len; i++) {
        switch (ops[i]) {
        case PLUS_ZERO:
            if (ax == 0) return 1;
            break;
        case PLUS_DECR:
            if (ax == 0) return 0;
            ax = ax - 1;
            break;
        case PLUS_MOD2: ax = ax % 2; break;
        case PLUS_HALF: ax = ax / 2; break;
        }
    }
    return 0;
}

uint32_t plus_language(const PlusOp *ops, int len) {
    uint32_t code = 0;
    for (int n = 0; n < PLUS_INPUTS; n++) {
        if (plus_accepts(n, ops, len)) code |= 1u << n;
    }
    return code;
}

uint64_t plus_pack(const PlusOp *ops, int len) {
    if (len > PLUS_MAX_LINES) {
        fprintf(stderr, "plus_pack: %d lines, and a word holds %d\n", len, PLUS_MAX_LINES);
        abort();
    }
    uint64_t word = 0;
    for (int i = 0; i < len; i++) word |= (uint64_t)ops[i] << (2 * i);
    return word;
}

void plus_unpack(uint64_t word, PlusOp *ops) {
    for (int i = 0; i < PLUS_MAX_LINES; i++) ops[i] = (PlusOp)((word >> (2 * i)) & 3);
}
