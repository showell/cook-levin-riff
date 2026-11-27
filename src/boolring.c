// boolring.c: the multilinear Boolean-ring polynomial (boolring.h).

#include "boolring.h"

#include "ring_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void bp_die(const char *what) {
    fprintf(stderr, "boolring: %s\n", what);
    abort();
}

static void *bp_alloc(size_t count, size_t size) {
    void *p = calloc(count ? count : 1, size);
    if (p == NULL) bp_die("out of memory");
    return p;
}

static int popcount32(uint32_t m) {
    int n = 0;
    while (m) {
        m &= m - 1;
        n++;
    }
    return n;
}

BPoly bp_zero(void) {
    BPoly p = {bp_alloc(1, sizeof(BTerm)), 0};
    return p;
}

BPoly bp_constant(int64_t c) {
    BPoly p = bp_zero();
    if (c != 0) {
        p.terms[0] = (BTerm){0, c};
        p.len = 1;
    }
    return p;
}

BPoly bp_var(int bit) {
    if (bit < 0 || bit >= BP_MAX_VARS) bp_die("variable index out of range");
    BPoly p = bp_zero();
    p.terms[0] = (BTerm){1u << bit, 1};
    p.len = 1;
    return p;
}

BPoly bp_copy(const BPoly *p) {
    BPoly q = {bp_alloc(p->len, sizeof(BTerm)), p->len};
    if (p->len) memcpy(q.terms, p->terms, p->len * sizeof(BTerm));
    return q;
}

void bp_free(BPoly *p) {
    free(p->terms);
    p->terms = NULL;
    p->len = 0;
}

// p + sign * q, by merging the two sorted term lists.
static BPoly merge(const BPoly *p, const BPoly *q, int64_t sign) {
    BPoly r = {bp_alloc(p->len + q->len, sizeof(BTerm)), 0};
    size_t i = 0;
    size_t j = 0;
    while (i < p->len || j < q->len) {
        BTerm t;
        if (j == q->len || (i < p->len && p->terms[i].mask < q->terms[j].mask)) {
            t = p->terms[i++];
        } else if (i == p->len || q->terms[j].mask < p->terms[i].mask) {
            t = (BTerm){q->terms[j].mask, ring_mul(sign, q->terms[j].coeff)};
            j++;
        } else {
            t = (BTerm){p->terms[i].mask, ring_add(p->terms[i].coeff, ring_mul(sign, q->terms[j].coeff))};
            i++;
            j++;
        }
        if (t.coeff != 0) r.terms[r.len++] = t;
    }
    return r;
}

BPoly bp_add(const BPoly *p, const BPoly *q) { return merge(p, q, 1); }
BPoly bp_sub(const BPoly *p, const BPoly *q) { return merge(p, q, -1); }

BPoly bp_neg(const BPoly *p) {
    BPoly z = bp_zero();
    BPoly r = merge(&z, p, -1);
    bp_free(&z);
    return r;
}

// ── An accumulator: mask -> coefficient, open addressing ───────────────────

typedef struct {
    uint32_t *masks;
    int64_t *coeffs;
    uint8_t *used;
    size_t cap; // a power of two
    size_t len;
} Acc;

static Acc acc_new(size_t want) {
    size_t cap = 16;
    while (cap < 2 * want) cap *= 2;
    Acc a = {bp_alloc(cap, sizeof(uint32_t)), bp_alloc(cap, sizeof(int64_t)), bp_alloc(cap, 1), cap, 0};
    return a;
}

static void acc_free(Acc *a) {
    free(a->masks);
    free(a->coeffs);
    free(a->used);
}

static size_t acc_slot(const Acc *a, uint32_t mask) {
    size_t h = (size_t)(mask * 2654435761u) & (a->cap - 1);
    while (a->used[h] && a->masks[h] != mask) h = (h + 1) & (a->cap - 1);
    return h;
}

static void acc_grow(Acc *a);

static void acc_put(Acc *a, uint32_t mask, int64_t c) {
    size_t h = acc_slot(a, mask);
    if (a->used[h]) {
        a->coeffs[h] = ring_add(a->coeffs[h], c);
        return;
    }
    a->used[h] = 1;
    a->masks[h] = mask;
    a->coeffs[h] = c;
    a->len++;
    if (2 * a->len > a->cap) acc_grow(a);
}

static void acc_grow(Acc *a) {
    Acc b = acc_new(a->cap);
    for (size_t i = 0; i < a->cap; i++) {
        if (a->used[i]) {
            size_t h = acc_slot(&b, a->masks[i]);
            b.used[h] = 1;
            b.masks[h] = a->masks[i];
            b.coeffs[h] = a->coeffs[i];
            b.len++;
        }
    }
    acc_free(a);
    *a = b;
}

static int by_mask(const void *x, const void *y) {
    uint32_t a = ((const BTerm *)x)->mask;
    uint32_t b = ((const BTerm *)y)->mask;
    return a < b ? -1 : a > b;
}

// The accumulator's nonzero terms as a canonical BPoly; frees it.
static BPoly acc_take(Acc *a) {
    BPoly r = {bp_alloc(a->len, sizeof(BTerm)), 0};
    for (size_t i = 0; i < a->cap; i++) {
        if (a->used[i] && a->coeffs[i] != 0) r.terms[r.len++] = (BTerm){a->masks[i], a->coeffs[i]};
    }
    acc_free(a);
    qsort(r.terms, r.len, sizeof(BTerm), by_mask);
    return r;
}

static uint32_t all_masks(const BPoly *p) {
    uint32_t u = 0;
    for (size_t i = 0; i < p->len; i++) u |= p->terms[i].mask;
    return u;
}

// ── A dense accumulator: one slot a mask, 0 to the union of the masks ──────
//
// When every mask a sum can reach lies within `width` (the OR of the masks
// involved), like terms are summed in a table of width + 1 slots indexed by
// mask: each term product is one array write, and the table read back in
// slot order is already sorted. roc/BoolPoly.roc sums so. Past
// DENSE_MAX_SLOTS slots the hash accumulator above is used instead.

enum { DENSE_MAX_SLOTS = 1 << 20 };

typedef struct {
    int64_t *sums;
    size_t slots;
} Dense;

static int dense_fits(uint32_t width) { return (size_t)width < DENSE_MAX_SLOTS; }

static Dense dense_new(uint32_t width) {
    Dense d = {bp_alloc((size_t)width + 1, sizeof(int64_t)), (size_t)width + 1};
    return d;
}

static void dense_put(Dense *d, uint32_t mask, int64_t c) { d->sums[mask] = ring_add(d->sums[mask], c); }

// The table's nonzero slots as a canonical BPoly, in mask order; frees it.
static BPoly dense_take(Dense *d) {
    size_t n = 0;
    for (size_t m = 0; m < d->slots; m++) n += d->sums[m] != 0;
    BPoly r = {bp_alloc(n, sizeof(BTerm)), 0};
    for (size_t m = 0; m < d->slots; m++) {
        if (d->sums[m] != 0) r.terms[r.len++] = (BTerm){(uint32_t)m, d->sums[m]};
    }
    free(d->sums);
    d->sums = NULL;
    return r;
}

BPoly bp_mul(const BPoly *p, const BPoly *q) {
    uint32_t width = all_masks(p) | all_masks(q);
    if (dense_fits(width)) {
        Dense d = dense_new(width);
        for (size_t i = 0; i < p->len; i++) {
            for (size_t j = 0; j < q->len; j++) {
                dense_put(&d, p->terms[i].mask | q->terms[j].mask, ring_mul(p->terms[i].coeff, q->terms[j].coeff));
            }
        }
        return dense_take(&d);
    }
    // The product has at most 2^(variables in either) terms, and at most
    // |p| * |q|: the accumulator starts at the smaller, capped, and grows.
    int vars = popcount32(all_masks(p) | all_masks(q));
    size_t bound = (size_t)p->len * q->len;
    if (vars < 20 && bound > ((size_t)1 << vars)) bound = (size_t)1 << vars;
    if (bound > ((size_t)1 << 16)) bound = (size_t)1 << 16;
    Acc a = acc_new(bound);
    for (size_t i = 0; i < p->len; i++) {
        for (size_t j = 0; j < q->len; j++) {
            acc_put(&a, p->terms[i].mask | q->terms[j].mask, ring_mul(p->terms[i].coeff, q->terms[j].coeff));
        }
    }
    return acc_take(&a);
}

BPoly bp_from_poly(const Poly *p, const BVars *vars) {
    Acc a = acc_new(p->len);
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        uint32_t mask = 0;
        for (uint32_t j = 0; j < t->count; j++) {
            const char *name = p->vps[t->first + j].name;
            int bit = -1;
            for (int v = 0; v < vars->count; v++) {
                if (strcmp(vars->names[v], name) == 0) bit = v;
            }
            if (bit < 0) {
                fprintf(stderr, "boolring: %s is not one of the variables\n", name);
                abort();
            }
            mask |= 1u << bit;
        }
        acc_put(&a, mask, t->coeff);
    }
    return acc_take(&a);
}

// The products of vals over a mask's variables, kept by mask: p's terms
// share their low variables, so each product is the one of its mask
// without the top variable, times that variable's value.
typedef struct {
    uint32_t masks[256];
    BPoly products[256];
    size_t len;
} Products;

static const BPoly *product_of(Products *k, uint32_t mask, const BPoly *vals) {
    for (size_t i = 0; i < k->len; i++) {
        if (k->masks[i] == mask) return &k->products[i];
    }
    BPoly r;
    if (mask == 0) {
        r = bp_constant(1);
    } else {
        int top = 31;
        while (!(mask & (1u << top))) top--;
        const BPoly *rest = product_of(k, mask & ~(1u << top), vals);
        r = bp_mul(rest, &vals[top]);
    }
    if (k->len == sizeof k->masks / sizeof k->masks[0]) bp_die("bp_substitute: too many distinct products");
    k->masks[k->len] = mask;
    k->products[k->len] = r;
    return &k->products[k->len++];
}

BPoly bp_substitute(const BPoly *p, const BPoly *vals, int n) {
    if (n < 0 || n > BP_MAX_VARS) bp_die("bp_substitute: bad variable count");
    if (n < BP_MAX_VARS && (all_masks(p) >> n) != 0) bp_die("bp_substitute: a variable has no value");
    // Every product's masks lie within the union of the values' masks.
    uint32_t width = 0;
    for (int i = 0; i < n; i++) width |= all_masks(&vals[i]);
    Products *k = bp_alloc(1, sizeof *k);
    BPoly r;
    if (dense_fits(width)) {
        Dense d = dense_new(width);
        for (size_t i = 0; i < p->len; i++) {
            const BPoly *prod = product_of(k, p->terms[i].mask, vals);
            for (size_t j = 0; j < prod->len; j++) {
                dense_put(&d, prod->terms[j].mask, ring_mul(p->terms[i].coeff, prod->terms[j].coeff));
            }
        }
        r = dense_take(&d);
    } else {
        Acc a = acc_new(1024);
        for (size_t i = 0; i < p->len; i++) {
            const BPoly *prod = product_of(k, p->terms[i].mask, vals);
            for (size_t j = 0; j < prod->len; j++) {
                acc_put(&a, prod->terms[j].mask, ring_mul(p->terms[i].coeff, prod->terms[j].coeff));
            }
        }
        r = acc_take(&a);
    }
    for (size_t i = 0; i < k->len; i++) bp_free(&k->products[i]);
    free(k);
    return r;
}

int64_t bp_eval(const BPoly *p, uint32_t ones) {
    int64_t sum = RING_ZERO;
    for (size_t i = 0; i < p->len; i++) {
        if ((p->terms[i].mask & ones) == p->terms[i].mask) sum = ring_add(sum, p->terms[i].coeff);
    }
    return sum;
}

int bp_equal(const BPoly *p, const BPoly *q) {
    if (p->len != q->len) return 0;
    for (size_t i = 0; i < p->len; i++) {
        if (p->terms[i].mask != q->terms[i].mask || p->terms[i].coeff != q->terms[i].coeff) return 0;
    }
    return 1;
}

int64_t bp_max_abs_coeff(const BPoly *p) {
    int64_t most = 0;
    for (size_t i = 0; i < p->len; i++) {
        int64_t c = p->terms[i].coeff;
        int64_t a = c < 0 ? ring_negate(c) : c;
        if (a > most) most = a;
    }
    return most;
}

int bp_degree(const BPoly *p) {
    int most = 0;
    for (size_t i = 0; i < p->len; i++) {
        int d = popcount32(p->terms[i].mask);
        if (d > most) most = d;
    }
    return most;
}

// ── Printing ────────────────────────────────────────────────────────────────

// By degree, then by the variables' indices as lists: where two masks of
// one degree first differ, the one holding the lower bit comes first.
static int display_order(const void *x, const void *y) {
    uint32_t a = ((const BTerm *)x)->mask;
    uint32_t b = ((const BTerm *)y)->mask;
    int da = popcount32(a);
    int db = popcount32(b);
    if (da != db) return da < db ? -1 : 1;
    if (a == b) return 0;
    uint32_t d = a ^ b;
    uint32_t low = d & (~d + 1);
    return (a & low) ? -1 : 1;
}

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} Buf;

static void put(Buf *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 256;
        b->s = realloc(b->s, b->cap);
        if (b->s == NULL) bp_die("out of memory");
    }
    memcpy(b->s + b->len, s, n + 1);
    b->len += n;
}

char *bp_str(const BPoly *p, const BVars *vars) {
    Buf b = {NULL, 0, 0};
    put(&b, "");
    if (p->len == 0) {
        put(&b, "0");
        return b.s;
    }
    BTerm *order = bp_alloc(p->len, sizeof(BTerm));
    memcpy(order, p->terms, p->len * sizeof(BTerm));
    qsort(order, p->len, sizeof(BTerm), display_order);
    char num[32];
    for (size_t i = 0; i < p->len; i++) {
        if (i > 0) put(&b, "+");
        int64_t c = order[i].coeff;
        uint32_t m = order[i].mask;
        if (m == 0 || c != 1) {
            snprintf(num, sizeof num, c > 0 ? "%lld" : "(%lld)", (long long)c);
            put(&b, num);
            if (m != 0) put(&b, "*");
        }
        int first = 1;
        for (int v = 0; v < BP_MAX_VARS; v++) {
            if (!(m & (1u << v))) continue;
            if (v >= vars->count) bp_die("bp_str: a variable has no name");
            if (!first) put(&b, "*");
            put(&b, vars->names[v]);
            first = 0;
        }
    }
    free(order);
    return b.s;
}
