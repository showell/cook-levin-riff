// poly.c: the general polynomial (poly.h).
//
// Every Poly is made by `canonical`, the port of Python's constructor: the
// terms gathered in a Builder, in the order Python's code would list them,
// then sorted (stably, so like terms keep that order and their
// coefficients are summed in Python's order), like terms combined, zeros
// dropped.

#include "poly.h"

#include "ring_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void poly_die(const char *what, const char *detail) {
    fprintf(stderr, "poly: %s%s\n", what, detail);
    abort();
}

static void *grown(void *p, size_t count, size_t size) {
    if (count == 0) count = 1;
    if (count > SIZE_MAX / size) poly_die("out of memory", "");
    void *q = realloc(p, count * size);
    if (q == NULL) poly_die("out of memory", "");
    return q;
}

// ── Variable names, interned ──────────────────────────────────────────────

static char **names;
static size_t names_len;
static size_t names_cap;

static const char *intern(const char *name) {
    for (size_t i = 0; i < names_len; i++) {
        if (strcmp(names[i], name) == 0) return names[i];
    }
    if (names_len == names_cap) {
        names_cap = names_cap ? names_cap * 2 : 16;
        names = grown(names, names_cap, sizeof *names);
    }
    size_t n = strlen(name) + 1;
    char *copy = grown(NULL, n, 1);
    memcpy(copy, name, n);
    names[names_len++] = copy;
    return copy;
}

static void check_legal_name(const char *name) {
    for (const char *c = ",*+/-()"; *c; c++) {
        if (strchr(name, *c) != NULL) {
            char what[2] = {*c, 0};
            fprintf(stderr, "poly: Do not use variable names that include %s.\n", what);
            abort();
        }
    }
}

// ── The builder and the canonical form ────────────────────────────────────

typedef struct {
    Term *terms;
    size_t len;
    size_t cap;
    VarPower *vps;
    size_t nvps;
    size_t vcap;
} Builder;

static void b_term(Builder *b, int64_t coeff) {
    if (b->len == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 16;
        b->terms = grown(b->terms, b->cap, sizeof *b->terms);
    }
    if (b->nvps > UINT32_MAX) poly_die("too many variable powers", "");
    b->terms[b->len++] = (Term){coeff, (uint32_t)b->nvps, 0};
}

// Adds a variable power to the term begun last; the caller keeps the names
// sorted and distinct.
static void b_vp(Builder *b, const char *name, uint32_t exp) {
    if (b->nvps == b->vcap) {
        b->vcap = b->vcap ? b->vcap * 2 : 16;
        b->vps = grown(b->vps, b->vcap, sizeof *b->vps);
    }
    b->vps[b->nvps++] = (VarPower){name, exp};
    b->terms[b->len - 1].count++;
}

static void b_copy_term(Builder *b, int64_t coeff, const VarPower *vps, uint32_t count) {
    b_term(b, coeff);
    for (uint32_t i = 0; i < count; i++) b_vp(b, vps[i].name, vps[i].exp);
}

static void b_add_poly(Builder *b, const Poly *p) {
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        b_copy_term(b, t->coeff, p->vps + t->first, t->count);
    }
}

// 1 when monomial a comes before b in Python's order (its exponent tuple
// over the sorted variables is the greater), -1 when after, 0 when equal.
static int monomial_cmp(const VarPower *a, uint32_t na, const VarPower *b, uint32_t nb) {
    uint32_t i = 0;
    uint32_t j = 0;
    while (i < na && j < nb) {
        if (a[i].name == b[j].name) {
            if (a[i].exp != b[j].exp) return a[i].exp > b[j].exp ? 1 : -1;
            i++;
            j++;
        } else {
            // The smaller name is a variable one term has and the other lacks.
            return strcmp(a[i].name, b[j].name) < 0 ? 1 : -1;
        }
    }
    if (i < na) return 1;
    if (j < nb) return -1;
    return 0;
}

static int term_cmp(const Builder *b, uint32_t x, uint32_t y) {
    const Term *s = &b->terms[x];
    const Term *t = &b->terms[y];
    return monomial_cmp(b->vps + s->first, s->count, b->vps + t->first, t->count);
}

// A stable merge sort of term indices, first in Python's order first.
static void sort_terms(const Builder *b, uint32_t *idx, uint32_t *tmp, size_t n) {
    if (n < 2) return;
    size_t half = n / 2;
    sort_terms(b, idx, tmp, half);
    sort_terms(b, idx + half, tmp, n - half);
    size_t i = 0;
    size_t j = half;
    size_t k = 0;
    while (i < half && j < n) {
        // Take from the right only when it strictly comes first: stable.
        if (term_cmp(b, idx[j], idx[i]) > 0) {
            tmp[k++] = idx[j++];
        } else {
            tmp[k++] = idx[i++];
        }
    }
    while (i < half) tmp[k++] = idx[i++];
    while (j < n) tmp[k++] = idx[j++];
    memcpy(idx, tmp, n * sizeof *idx);
}

static void b_free(Builder *b) {
    free(b->terms);
    free(b->vps);
    memset(b, 0, sizeof *b);
}

// Python's constructor: simplify, then put_terms_in_order. Frees the builder.
static Poly canonical(Builder *b) {
    Poly out = {NULL, 0, NULL, 0};
    size_t n = b->len;
    if (n > UINT32_MAX) poly_die("too many terms", "");
    uint32_t *idx = grown(NULL, n, sizeof *idx);
    uint32_t *tmp = grown(NULL, n, sizeof *tmp);
    for (size_t i = 0; i < n; i++) idx[i] = (uint32_t)i;
    sort_terms(b, idx, tmp, n);
    free(tmp);

    out.terms = grown(NULL, n, sizeof *out.terms);
    out.vps = grown(NULL, b->nvps, sizeof *out.vps);
    size_t i = 0;
    while (i < n) {
        const Term *first = &b->terms[idx[i]];
        int64_t coeff = first->coeff;
        size_t j = i + 1;
        while (j < n && term_cmp(b, idx[i], idx[j]) == 0) {
            coeff = ring_add(coeff, b->terms[idx[j]].coeff);
            j++;
        }
        if (coeff != RING_ZERO) {
            out.terms[out.len++] = (Term){coeff, (uint32_t)out.nvps, first->count};
            for (uint32_t k = 0; k < first->count; k++) out.vps[out.nvps++] = b->vps[first->first + k];
        }
        i = j;
    }
    free(idx);
    b_free(b);
    return out;
}

// ── Construction ──────────────────────────────────────────────────────────

Poly poly_zero(void) {
    Builder b = {0};
    return canonical(&b);
}

Poly poly_constant(int64_t c) {
    Builder b = {0};
    b_term(&b, c);
    return canonical(&b);
}

Poly poly_one(void) {
    return poly_constant(RING_ONE);
}

Poly poly_var(const char *name) {
    check_legal_name(name);
    Builder b = {0};
    b_term(&b, RING_ONE);
    b_vp(&b, intern(name), 1);
    return canonical(&b);
}

Poly poly_copy(const Poly *p) {
    Builder b = {0};
    b_add_poly(&b, p);
    return canonical(&b);
}

void poly_free(Poly *p) {
    free(p->terms);
    free(p->vps);
    memset(p, 0, sizeof *p);
}

int poly_is_zero(const Poly *p) {
    return p->len == 0;
}

int poly_is_one(const Poly *p) {
    return p->len == 1 && p->terms[0].coeff == RING_ONE && p->terms[0].count == 0;
}

int poly_equal(const Poly *p, const Poly *q) {
    if (p->len != q->len) return 0;
    for (size_t i = 0; i < p->len; i++) {
        const Term *s = &p->terms[i];
        const Term *t = &q->terms[i];
        if (s->coeff != t->coeff) return 0;
        if (monomial_cmp(p->vps + s->first, s->count, q->vps + t->first, t->count) != 0) return 0;
    }
    return 1;
}

// ── Arithmetic ────────────────────────────────────────────────────────────

Poly poly_add(const Poly *p, const Poly *q) {
    Builder b = {0};
    b_add_poly(&b, p);
    b_add_poly(&b, q);
    return canonical(&b);
}

Poly poly_neg(const Poly *p) {
    Builder b = {0};
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        b_copy_term(&b, ring_negate(t->coeff), p->vps + t->first, t->count);
    }
    return canonical(&b);
}

Poly poly_sub(const Poly *p, const Poly *q) {
    if (poly_is_zero(q)) return poly_copy(p);
    Poly neg = poly_neg(q);
    Poly out = poly_add(p, &neg);
    poly_free(&neg);
    return out;
}

// Python's _Term.multiply_terms: the exponents merged by name, sorted.
static void b_product_term(Builder *b, const Poly *p, const Term *s, const Poly *q, const Term *t) {
    b_term(b, ring_mul(s->coeff, t->coeff));
    const VarPower *x = p->vps + s->first;
    const VarPower *y = q->vps + t->first;
    uint32_t i = 0;
    uint32_t j = 0;
    while (i < s->count || j < t->count) {
        if (j == t->count || (i < s->count && x[i].name != y[j].name && strcmp(x[i].name, y[j].name) < 0)) {
            b_vp(b, x[i].name, x[i].exp);
            i++;
        } else if (i == s->count || x[i].name != y[j].name) {
            b_vp(b, y[j].name, y[j].exp);
            j++;
        } else {
            if (x[i].exp > UINT32_MAX - y[j].exp) poly_die("exponent overflow on ", x[i].name);
            b_vp(b, x[i].name, x[i].exp + y[j].exp);
            i++;
            j++;
        }
    }
}

Poly poly_mul(const Poly *p, const Poly *q) {
    if (poly_is_zero(p) || poly_is_zero(q)) return poly_zero();
    if (poly_is_one(p)) return poly_copy(q);
    if (poly_is_one(q)) return poly_copy(p);
    Builder b = {0};
    for (size_t i = 0; i < p->len; i++) {
        for (size_t j = 0; j < q->len; j++) b_product_term(&b, p, &p->terms[i], q, &q->terms[j]);
    }
    return canonical(&b);
}

Poly poly_pow(const Poly *p, int64_t k) {
    if (k < 0) poly_die("we do not support negative exponents", "");
    if (k == 0) return poly_one();
    Poly acc = poly_copy(p);
    for (int64_t i = 2; i <= k; i++) {
        Poly next = poly_mul(p, &acc);
        poly_free(&acc);
        acc = next;
    }
    return acc;
}

Poly poly_add_int(const Poly *p, int64_t c) {
    if (c == RING_ZERO) return poly_copy(p);
    Poly k = poly_constant(c);
    Poly out = poly_add(p, &k);
    poly_free(&k);
    return out;
}

// Python's multiply_by_constant: each term's coefficient times c (by 0, a
// zero term, which the canonical form drops).
Poly poly_mul_int(const Poly *p, int64_t c) {
    if (c == RING_ZERO) return poly_zero();
    Builder b = {0};
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        int64_t coeff = c == RING_ONE ? t->coeff : ring_mul(c, t->coeff);
        b_copy_term(&b, coeff, p->vps + t->first, t->count);
    }
    return canonical(&b);
}

Poly poly_int_sub(int64_t c, const Poly *p) {
    Poly k = poly_constant(c);
    Poly out = poly_sub(&k, p);
    poly_free(&k);
    return out;
}

// ── Evaluation and substitution ───────────────────────────────────────────

static int has_var(const Poly *p, const char *name) {
    for (size_t i = 0; i < p->nvps; i++) {
        if (strcmp(p->vps[i].name, name) == 0) return 1;
    }
    return 0;
}

// The binding for `name`, or NULL.
static const Binding *bound(const Binding *vals, size_t n, const char *name) {
    for (size_t i = 0; i < n; i++) {
        if (strcmp(vals[i].name, name) == 0) return &vals[i];
    }
    return NULL;
}

static void check_bindings(const Binding *vals, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (bound(vals, i, vals[i].name) != NULL) poly_die("a variable bound twice: ", vals[i].name);
    }
}

int64_t poly_eval(const Poly *p, const Binding *vals, size_t n) {
    check_bindings(vals, n);
    for (size_t i = 0; i < p->nvps; i++) {
        if (bound(vals, n, p->vps[i].name) == NULL) {
            fprintf(stderr, "poly: The var %s was not supplied.\n", p->vps[i].name);
            abort();
        }
    }
    int64_t result = RING_ZERO;
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        int64_t product = RING_ONE;
        for (uint32_t k = 0; k < t->count; k++) {
            const VarPower *vp = &p->vps[t->first + k];
            product = ring_mul(product, ring_power(bound(vals, n, vp->name)->value, vp->exp));
        }
        result = ring_add(result, ring_mul(t->coeff, product));
    }
    return result;
}

Poly poly_apply(const Poly *p, const Binding *vals, size_t n) {
    check_bindings(vals, n);
    for (size_t i = 0; i < n; i++) {
        if (!has_var(p, vals[i].name)) {
            char *s = poly_str(p);
            fprintf(stderr, "poly: %s is not a variable for %s\n", vals[i].name, s);
            free(s);
            abort();
        }
    }
    Builder b = {0};
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        int64_t coeff = t->coeff;
        for (uint32_t k = 0; k < t->count; k++) {
            const VarPower *vp = &p->vps[t->first + k];
            const Binding *v = bound(vals, n, vp->name);
            if (v != NULL) coeff = ring_mul(coeff, ring_power(v->value, vp->exp));
        }
        b_term(&b, coeff);
        for (uint32_t k = 0; k < t->count; k++) {
            const VarPower *vp = &p->vps[t->first + k];
            if (bound(vals, n, vp->name) == NULL) b_vp(&b, vp->name, vp->exp);
        }
    }
    return canonical(&b);
}

// Each term split on the variable, the rest times q ** exponent, all summed
// in one canonical form, as Python's Poly.sum does.
Poly poly_substitute(const Poly *p, const char *name, const Poly *q) {
    if (!has_var(p, name)) poly_die("Unknown variable: ", name);
    Builder b = {0};
    for (size_t i = 0; i < p->len; i++) {
        const Term *t = &p->terms[i];
        Builder small = {0};
        b_term(&small, t->coeff);
        uint32_t exp = 0;
        for (uint32_t k = 0; k < t->count; k++) {
            const VarPower *vp = &p->vps[t->first + k];
            if (strcmp(vp->name, name) == 0) {
                exp = vp->exp;
            } else {
                b_vp(&small, vp->name, vp->exp);
            }
        }
        Poly small_poly = canonical(&small);
        if (exp == 0) {
            b_add_poly(&b, &small_poly);
        } else {
            Poly power = poly_pow(q, exp);
            Poly product = poly_mul(&small_poly, &power);
            b_add_poly(&b, &product);
            poly_free(&power);
            poly_free(&product);
        }
        poly_free(&small_poly);
    }
    return canonical(&b);
}

// ── Printing ──────────────────────────────────────────────────────────────

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} Text;

static void text_add(Text *t, const char *s, size_t n) {
    if (t->len + n + 1 > t->cap) {
        while (t->len + n + 1 > t->cap) t->cap = t->cap ? t->cap * 2 : 64;
        t->s = grown(t->s, t->cap, 1);
    }
    memcpy(t->s + t->len, s, n);
    t->len += n;
    t->s[t->len] = 0;
}

static void text_str(Text *t, const char *s) {
    text_add(t, s, strlen(s));
}

// Python's coeff_str: str(c) if c > 0, else "(c)".
static void text_coeff(Text *t, int64_t c) {
    char buf[32];
    int n = snprintf(buf, sizeof buf, c > 0 ? "%lld" : "(%lld)", (long long)c);
    text_add(t, buf, (size_t)n);
}

// A term's sig: its variable powers joined by "*", each `x` or `(x**3)`.
static void text_sig(Text *t, const VarPower *vps, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (i > 0) text_str(t, "*");
        if (vps[i].exp == 1) {
            text_str(t, vps[i].name);
        } else {
            char buf[16];
            text_str(t, "(");
            text_str(t, vps[i].name);
            snprintf(buf, sizeof buf, "**%u)", (unsigned)vps[i].exp);
            text_str(t, buf);
        }
    }
}

char *poly_str(const Poly *p) {
    Text t = {NULL, 0, 0};
    if (p->len == 0) {
        // Python's str(Math.zero), not the coefficient form "(0)".
        text_str(&t, "0");
        return t.s;
    }
    for (size_t i = 0; i < p->len; i++) {
        const Term *term = &p->terms[i];
        if (i > 0) text_str(&t, "+");
        if (term->count == 0) {
            text_coeff(&t, term->coeff);
        } else if (term->coeff == RING_ONE) {
            text_sig(&t, p->vps + term->first, term->count);
        } else {
            text_coeff(&t, term->coeff);
            text_str(&t, "*");
            text_sig(&t, p->vps + term->first, term->count);
        }
    }
    return t.s;
}
