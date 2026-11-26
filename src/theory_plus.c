// theory_plus.c: the recursion for Nibble++'s shortest lengths
// (theory_plus.h).

#include "theory_plus.h"

#include <stdio.h>
#include <stdlib.h>

#define THEORY_OP(op) (1u << (op))

static int popcount_bits(uint32_t s) {
    int n = 0;
    while (s != 0) {
        n += (int)(s & 1u);
        s >>= 1;
    }
    return n;
}

int theory_walk_length(uint32_t lang) {
    if (lang == 0) return 0;
    int max = 0;
    for (int i = 0; i < 32; i++) {
        if ((lang >> i) & 1u) max = i;
    }
    return max + popcount_bits(lang);
}

// The value `op` gives AX = u.
static int image(PlusOp op, int u) {
    return op == PLUS_HALF ? u >> 1 : u % 2;
}

// How many values `op` gives AX = 0 to `top`: the smaller machine's size.
static int image_size(PlusOp op, int top) {
    return op == PLUS_HALF ? (top >> 1) + 1 : (top >= 1 ? 2 : 1);
}

// The best start of a size-n formal language `s` with `op` after a walk
// of d `decr`s, n > d: its length, or -1 when no such program decides `s`.
// The inputs from d on have AX u = input - d, and `op` sends them to
// image(u); the inputs that share a value are a block, and a block is
// accepted whole or not at all, except that input d alone can be taken by
// a `zero` before `op`.
static int split_length(const Theory *t, int n, uint32_t s, PlusOp op, int d, TheoryChoice *how) {
    int size = image_size(op, n - 1 - d);
    uint32_t rest = 0;
    int zero_at_d = 0;
    for (int v = 0; v < size; v++) {
        uint32_t block = 0;
        for (int u = 0; u <= n - 1 - d; u++) {
            if (image(op, u) == v) block |= 1u << (d + u);
        }
        uint32_t in = s & block;
        if (in == block) {
            rest |= 1u << v;
        } else if (in == 0) {
            continue;
        } else if (v == 0 && in == 1u << d) {
            zero_at_d = 1;
        } else {
            return -1;
        }
    }
    int below = popcount_bits(s & ((1u << d) - 1u));
    how->op = (int8_t)op;
    how->d = (int8_t)d;
    how->zero_at_d = (int8_t)zero_at_d;
    how->rest_size = (int8_t)size;
    how->rest = (uint16_t)rest;
    return d + below + zero_at_d + 1 + t->shortest[size][rest];
}

void theory_build(Theory *t, unsigned ops) {
    if (!(ops & THEORY_OP(PLUS_ZERO)) || !(ops & THEORY_OP(PLUS_DECR))) {
        fprintf(stderr, "theory_build: ops %#x lack zero or decr\n", ops);
        abort();
    }
    t->ops = ops;
    t->shortest[0] = NULL;
    t->choice[0] = NULL;
    const PlusOp splits[2] = {PLUS_MOD2, PLUS_HALF};
    for (int n = 1; n <= PLUS_INPUTS; n++) {
        size_t count = (size_t)1 << n;
        t->shortest[n] = malloc(count);
        t->choice[n] = malloc(count * sizeof(TheoryChoice));
        if (t->shortest[n] == NULL || t->choice[n] == NULL) {
            fprintf(stderr, "theory_build: out of memory at size %d\n", n);
            abort();
        }
        for (size_t s = 0; s < count; s++) {
            int best = theory_walk_length((uint32_t)s);
            TheoryChoice best_how = {-1, 0, 0, 0, 0};
            for (int k = 0; k < 2; k++) {
                if (!(ops & THEORY_OP(splits[k]))) continue;
                for (int d = 0; d < n; d++) {
                    // A smaller machine only: a `half` of a lone input, or a
                    // `mod2` of one or two, maps it onto as many, and the
                    // recursion would not end.
                    if (image_size(splits[k], n - 1 - d) >= n) continue;
                    TheoryChoice how;
                    int len = split_length(t, n, (uint32_t)s, splits[k], d, &how);
                    if (len >= 0 && len < best) {
                        best = len;
                        best_how = how;
                    }
                }
            }
            t->shortest[n][s] = (int8_t)best;
            t->choice[n][s] = best_how;
        }
    }
}

void theory_free(Theory *t) {
    for (int n = 1; n <= PLUS_INPUTS; n++) {
        free(t->shortest[n]);
        free(t->choice[n]);
        t->shortest[n] = NULL;
        t->choice[n] = NULL;
    }
}

int theory_shortest(const Theory *t, uint32_t lang) {
    return t->shortest[PLUS_INPUTS][lang];
}

static int emit(PlusOp *ops, int at, PlusOp op) {
    ops[at] = op;
    return at + 1;
}

static int witness_of(const Theory *t, int n, uint32_t s, PlusOp *ops, int at) {
    const TheoryChoice *how = &t->choice[n][s];
    if (how->op < 0) {
        if (s == 0) return at;
        int max = 0;
        for (int i = 0; i < n; i++) {
            if ((s >> i) & 1u) max = i;
        }
        for (int k = 0; k <= max; k++) {
            if ((s >> k) & 1u) at = emit(ops, at, PLUS_ZERO);
            if (k < max) at = emit(ops, at, PLUS_DECR);
        }
        return at;
    }
    for (int k = 0; k < how->d; k++) {
        if ((s >> k) & 1u) at = emit(ops, at, PLUS_ZERO);
        at = emit(ops, at, PLUS_DECR);
    }
    if (how->zero_at_d) at = emit(ops, at, PLUS_ZERO);
    at = emit(ops, at, (PlusOp)how->op);
    return witness_of(t, how->rest_size, how->rest, ops, at);
}

int theory_witness(const Theory *t, uint32_t lang, PlusOp *ops) {
    return witness_of(t, PLUS_INPUTS, lang, ops, 0);
}

int theory_zdm_closed(uint32_t lang) {
    int best = theory_walk_length(lang);
    for (int d = 0; d < PLUS_INPUTS; d++) {
        uint32_t from_d = 0xFFFFu & ~((1u << d) - 1u);
        uint32_t parity = 0;
        for (int i = d; i < PLUS_INPUTS; i += 2) parity |= 1u << i;
        int below = popcount_bits(lang & ((1u << d) - 1u));
        if ((lang & from_d) == parity && d + below + 2 < best) best = d + below + 2;
        if ((lang & from_d) == from_d && d + below + 4 < best) best = d + below + 4;
    }
    return best;
}
