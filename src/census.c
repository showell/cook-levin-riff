// census.c: the breadth-first search over Nibble++ states (census.h).

#include "census.h"

#include <stdlib.h>
#include <string.h>

static void *must_alloc(size_t bytes) {
    void *p = malloc(bytes);
    if (p == NULL) {
        fprintf(stderr, "census: out of memory for %zu bytes\n", bytes);
        abort();
    }
    return p;
}

static void *must_realloc(void *p, size_t bytes) {
    void *q = realloc(p, bytes);
    if (q == NULL) {
        fprintf(stderr, "census: out of memory for %zu bytes\n", bytes);
        abort();
    }
    return q;
}

// One line on every input at once. An input that has halted stays halted.
static void step_state(const uint8_t *from, PlusOp op, uint8_t *to) {
    for (int i = 0; i < PLUS_INPUTS; i++) {
        uint8_t v = from[i];
        if (v >= CENSUS_ACCEPTED) {
            to[i] = v;
        } else if (op == PLUS_ZERO) {
            to[i] = v == 0 ? CENSUS_ACCEPTED : v;
        } else if (op == PLUS_DECR) {
            to[i] = v == 0 ? CENSUS_REJECTED : (uint8_t)(v - 1);
        } else if (op == PLUS_MOD2) {
            to[i] = v % 2;
        } else {
            to[i] = v >> 1;
        }
    }
}

static uint32_t state_language(const uint8_t *s) {
    uint32_t code = 0;
    for (int i = 0; i < PLUS_INPUTS; i++) {
        if (s[i] == CENSUS_ACCEPTED) code |= 1u << i;
    }
    return code;
}

// FNV-1a over the state's 16 bytes.
static uint64_t state_hash(const uint8_t *s) {
    uint64_t h = 14695981039346656037ull;
    for (int i = 0; i < PLUS_INPUTS; i++) {
        h ^= s[i];
        h *= 1099511628211ull;
    }
    return h;
}

// The table slot that holds `s`, or the empty slot where it goes.
static size_t slot_of(const Census *c, const uint8_t *s) {
    size_t mask = c->table_size - 1;
    size_t i = (size_t)state_hash(s) & mask;
    while (c->table[i] >= 0 && memcmp(c->states[c->table[i]], s, PLUS_INPUTS) != 0) i = (i + 1) & mask;
    return i;
}

static void grow_table(Census *c) {
    free(c->table);
    c->table_size *= 2;
    c->table = must_alloc(c->table_size * sizeof(int32_t));
    for (size_t i = 0; i < c->table_size; i++) c->table[i] = -1;
    for (size_t k = 0; k < c->num_states; k++) c->table[slot_of(c, c->states[k])] = (int32_t)k;
}

// Adds `s` when it is new, and gives 1; gives 0 for a state already found.
static int add_state(Census *c, const uint8_t *s, int32_t parent, PlusOp op) {
    size_t slot = slot_of(c, s);
    if (c->table[slot] >= 0) return 0;
    if (c->num_states == c->cap_states) {
        c->cap_states *= 2;
        c->states = must_realloc(c->states, c->cap_states * PLUS_INPUTS);
        c->parent = must_realloc(c->parent, c->cap_states * sizeof(int32_t));
        c->op = must_realloc(c->op, c->cap_states);
    }
    size_t k = c->num_states++;
    memcpy(c->states[k], s, PLUS_INPUTS);
    c->parent[k] = parent;
    c->op[k] = (uint8_t)op;
    c->table[slot] = (int32_t)k;
    if (2 * c->num_states > c->table_size) grow_table(c);
    return 1;
}

void census_run(Census *c, unsigned ops) {
    c->ops = ops;
    c->cap_states = 1 << 16;
    c->states = must_alloc(c->cap_states * PLUS_INPUTS);
    c->parent = must_alloc(c->cap_states * sizeof(int32_t));
    c->op = must_alloc(c->cap_states);
    c->num_states = 0;
    c->table_size = 1 << 17;
    c->table = must_alloc(c->table_size * sizeof(int32_t));
    for (size_t i = 0; i < c->table_size; i++) c->table[i] = -1;
    for (int y = 0; y < PLUS_LANGUAGES; y++) {
        c->shortest[y] = -1;
        c->reached_by[y] = -1;
    }
    for (int d = 0; d <= CENSUS_MAX_LENGTH; d++) c->histogram[d] = 0;

    uint8_t start[PLUS_INPUTS];
    for (int i = 0; i < PLUS_INPUTS; i++) start[i] = (uint8_t)i;
    add_state(c, start, -1, PLUS_HALF);
    uint32_t lang0 = state_language(start);
    c->shortest[lang0] = 0;
    c->reached_by[lang0] = 0;
    c->histogram[0] = 1;
    long found = 1;

    // Each level expands the states the level before found, in order, and
    // the search stops at the end of the level that reaches the last
    // language, as python/census4.py does.
    size_t level_start = 0;
    size_t level_end = c->num_states;
    int depth = 0;
    while (level_start < level_end && found < PLUS_LANGUAGES) {
        depth++;
        if (depth > CENSUS_MAX_LENGTH) break;
        for (size_t k = level_start; k < level_end; k++) {
            for (int op = 0; op < PLUS_NUM_OPS; op++) {
                if (!(ops & CENSUS_OP(op))) continue;
                uint8_t next[PLUS_INPUTS];
                step_state(c->states[k], (PlusOp)op, next);
                if (!add_state(c, next, (int32_t)k, (PlusOp)op)) continue;
                uint32_t lang = state_language(next);
                if (c->shortest[lang] < 0) {
                    c->shortest[lang] = (int8_t)depth;
                    c->reached_by[lang] = (int32_t)(c->num_states - 1);
                    c->histogram[depth]++;
                    found++;
                }
            }
        }
        level_start = level_end;
        level_end = c->num_states;
    }
    c->levels = depth;
    if (found < PLUS_LANGUAGES) {
        for (int y = 0; y < PLUS_LANGUAGES; y++) {
            if (c->shortest[y] < 0) {
                fprintf(stderr, "census: ops %#x reach %ld languages; language %d is not among them\n", ops, found, y);
                abort();
            }
        }
    }
}

void census_free(Census *c) {
    free(c->states);
    free(c->parent);
    free(c->op);
    free(c->table);
    c->states = NULL;
    c->parent = NULL;
    c->op = NULL;
    c->table = NULL;
}

int census_witness(const Census *c, uint32_t lang, PlusOp *ops) {
    int len = c->shortest[lang];
    int32_t k = c->reached_by[lang];
    for (int i = len - 1; i >= 0; i--) {
        ops[i] = (PlusOp)c->op[k];
        k = c->parent[k];
    }
    return len;
}

void census_ops_name(unsigned ops, char *out, size_t size) {
    static const PlusOp ORDER[PLUS_NUM_OPS] = {PLUS_ZERO, PLUS_DECR, PLUS_MOD2, PLUS_HALF};
    size_t at = 0;
    out[0] = '\0';
    for (int i = 0; i < PLUS_NUM_OPS; i++) {
        if (!(ops & CENSUS_OP(ORDER[i]))) continue;
        at += (size_t)snprintf(out + at, size - at, "%s%s", at > 0 ? "," : "", plus_op_name(ORDER[i]));
    }
}

void census_print(FILE *out, const Census *c) {
    char name[64];
    census_ops_name(c->ops, name, sizeof name);
    int longest = 0;
    long total = 0;
    for (int d = 0; d <= CENSUS_MAX_LENGTH; d++) {
        if (c->histogram[d] > 0) longest = d;
        total += c->histogram[d] * d;
    }
    fprintf(out, "ops %s: %zu states, %d languages, longest %d, total length %ld, mean %.2f\n", name, c->num_states,
            PLUS_LANGUAGES, longest, total, (double)total / PLUS_LANGUAGES);
    for (int d = 0; d <= longest; d++) fprintf(out, "  length %d: %ld\n", d, c->histogram[d]);
}
