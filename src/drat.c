// drat.c: a DRAT proof checker (drat.h).

#include "drat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *grown(void *p, size_t n, size_t size) {
    void *q = realloc(p, n * size);
    if (q == NULL) {
        fprintf(stderr, "drat: out of memory\n");
        abort();
    }
    return q;
}

static void say(char *why, size_t why_len, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(why, why_len, fmt, ap);
    va_end(ap);
}

// ── Lines and literals ───────────────────────────────────────────────────

// A line of text, NUL-ended in a buffer of its own, and the text after it.
typedef struct {
    char *buf;
    size_t cap;
} LineBuf;

static const char *next_line(const char *at, LineBuf *line) {
    const char *end = strchr(at, '\n');
    size_t n = end ? (size_t)(end - at) : strlen(at);
    if (n + 1 > line->cap) {
        line->cap = n + 1;
        line->buf = grown(line->buf, line->cap, 1);
    }
    memcpy(line->buf, at, n);
    line->buf[n] = 0;
    return end ? end + 1 : at + n;
}

// The literals of a list `a b c ... 0` in `s`, into a growing buffer. Gives
// how many, or -1 when the text is not a list of integers ended by one 0.
static int parse_lits(const char *s, int **lits, int *cap) {
    int n = 0;
    for (;;) {
        while (*s == ' ' || *s == '\t' || *s == '\r') s++;
        if (*s == 0) return -1;
        char *end;
        long x = strtol(s, &end, 10);
        if (end == s || x > 0x7fffffff || x < -0x7fffffff) return -1;
        s = end;
        if (x == 0) break;
        if (n == *cap) {
            *cap = *cap ? 2 * *cap : 64;
            *lits = grown(*lits, (size_t)*cap, sizeof **lits);
        }
        (*lits)[n++] = (int)x;
    }
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    return *s == 0 ? n : -1;
}

static int compare_ints(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

// Sorts the literals and drops repeats; gives how many are left.
static int normalized(int *lits, int n) {
    if (n < 2) return n;
    qsort(lits, (size_t)n, sizeof *lits, compare_ints);
    int kept = 0;
    for (int i = 0; i < n; i++) {
        if (kept == 0 || lits[kept - 1] != lits[i]) lits[kept++] = lits[i];
    }
    return kept;
}

static int is_tautology(const int *sorted, int n) {
    for (int i = 0; i < n; i++) {
        int neg = -sorted[i];
        if (bsearch(&neg, sorted, (size_t)n, sizeof *sorted, compare_ints) != NULL) return 1;
    }
    return 0;
}

static int abs_int(int x) { return x < 0 ? -x : x; }

// ── The database ─────────────────────────────────────────────────────────
static int slot(const DratDb *db, int lit) { return lit + db->num_vars; }

static void push(DratList *list, int x) {
    if (list->len == list->cap) {
        list->cap = list->cap ? 2 * list->cap : 8;
        list->at = grown(list->at, (size_t)list->cap, sizeof *list->at);
    }
    list->at[list->len++] = x;
}

static void *zeroed(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (p == NULL) {
        fprintf(stderr, "drat: out of memory\n");
        abort();
    }
    return p;
}

static void db_init(DratDb *db, int num_vars) {
    memset(db, 0, sizeof *db);
    db->num_vars = num_vars;
    size_t slots = 2 * (size_t)num_vars + 1;
    db->occ = zeroed(slots, sizeof *db->occ);
    db->watches = zeroed(slots, sizeof *db->watches);
    db->val = zeroed((size_t)num_vars + 1, 1);
    db->reason = zeroed((size_t)num_vars + 1, sizeof *db->reason);
    db->trail = zeroed((size_t)num_vars + 1, sizeof *db->trail);
    memset(db->val, -1, (size_t)num_vars + 1);
}

void drat_free(DratDb *db) {
    if (db->occ != NULL) {
        for (int s = 0; s < 2 * db->num_vars + 1; s++) {
            free(db->occ[s].at);
            free(db->watches[s].at);
        }
    }
    free(db->occ);
    free(db->watches);
    free(db->units.at);
    free(db->buckets);
    free(db->val);
    free(db->reason);
    free(db->trail);
    free(db->lits);
    free(db->clauses);
    memset(db, 0, sizeof *db);
}

static unsigned hash_of(const int *lits, int n) {
    unsigned h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ (unsigned)lits[i]) * 16777619u;
    return h;
}

static int *bucket_of(DratDb *db, const int *lits, int n) {
    return &db->buckets[hash_of(lits, n) & (db->num_buckets - 1)];
}

// Doubles the buckets once there are as many clauses as buckets, before a
// clause is stored. Rehashed in
// index order, so each bucket still lists its newest clause first.
static void grow_buckets(DratDb *db) {
    if (db->num_clauses < db->num_buckets) return;
    db->num_buckets = db->num_buckets ? 2 * db->num_buckets : 1024;
    free(db->buckets);
    db->buckets = grown(NULL, db->num_buckets, sizeof *db->buckets);
    for (size_t b = 0; b < db->num_buckets; b++) db->buckets[b] = -1;
    for (size_t c = 0; c < db->num_clauses; c++) {
        DratClause *cl = &db->clauses[c];
        if (!cl->live) continue;
        int *b = bucket_of(db, db->lits + cl->start, cl->len);
        cl->next = *b;
        *b = (int)c;
    }
}

static void watch(DratDb *db, int c, int a, int b) {
    db->clauses[c].watch[0] = a;
    db->clauses[c].watch[1] = b;
    push(&db->watches[slot(db, a)], c);
    push(&db->watches[slot(db, b)], c);
}

// Stores a clause of sorted, distinct literals; gives its index. A clause of
// two or more is not yet watched.
static int store_clause(DratDb *db, const int *lits, int n) {
    grow_buckets(db);
    if (db->num_clauses == db->clauses_cap) {
        db->clauses_cap = db->clauses_cap ? 2 * db->clauses_cap : 1024;
        db->clauses = grown(db->clauses, db->clauses_cap, sizeof *db->clauses);
    }
    while (db->lits_len + (size_t)n > db->lits_cap) {
        db->lits_cap = db->lits_cap ? 2 * db->lits_cap : 4096;
        db->lits = grown(db->lits, db->lits_cap, sizeof *db->lits);
    }
    int index = (int)db->num_clauses++;
    db->clauses[index] = (DratClause){(int)db->lits_len, n, 1, {0, 0}, -1};
    for (int i = 0; i < n; i++) {
        db->lits[db->lits_len++] = lits[i];
        push(&db->occ[slot(db, lits[i])], index);
    }
    if (n == 0) db->empty_live++;
    if (n == 1) push(&db->units, index);
    int *b = bucket_of(db, lits, n);
    db->clauses[index].next = *b;
    *b = index;
    return index;
}

// Removes a live clause with these sorted, distinct literals, the newest of
// them; gives its index, or -1 when there is none.
static int delete_clause(DratDb *db, const int *lits, int n) {
    if (db->num_buckets == 0) return -1;
    int *link = bucket_of(db, lits, n);
    for (int c = *link; c >= 0; c = *link) {
        DratClause *cl = &db->clauses[c];
        if (cl->len == n && (n == 0 || memcmp(db->lits + cl->start, lits, (size_t)n * sizeof *lits) == 0)) {
            *link = cl->next;
            cl->live = 0;
            if (n == 0) db->empty_live--;
            return c;
        }
        link = &cl->next;
    }
    return -1;
}

static int vars_in_range(const DratDb *db, const int *lits, int n) {
    for (int i = 0; i < n; i++) {
        if (abs_int(lits[i]) > db->num_vars) return 0;
    }
    return 1;
}

int drat_read_cnf(DratDb *db, const char *text, char *why, size_t why_len) {
    memset(db, 0, sizeof *db);
    LineBuf line = {0};
    int *lits = NULL, cap = 0, n = 0;
    int vars = -1;
    long clauses = -1, seen = 0, line_no = 0;
    int ok = 1;
    const char *at = text;
    while (ok && *at) {
        at = next_line(at, &line);
        line_no++;
        const char *s = line.buf;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == 0 || *s == 'c') continue;
        if (*s == 'p') {
            if (vars >= 0 || sscanf(s, "p cnf %d %ld", &vars, &clauses) != 2 || vars < 1 || clauses < 0) {
                say(why, why_len, "line %ld: a bad or second `p cnf` line", line_no);
                ok = 0;
            } else {
                db_init(db, vars);
            }
            continue;
        }
        if (vars < 0) {
            say(why, why_len, "line %ld: a clause before the `p cnf` line", line_no);
            ok = 0;
            continue;
        }
        // One clause a line, as sat.c and rocsat write them.
        n = parse_lits(s, &lits, &cap);
        if (n < 0 || !vars_in_range(db, lits, n)) {
            say(why, why_len, "line %ld: not a clause of variables 1..%d ended by 0", line_no, vars);
            ok = 0;
            continue;
        }
        n = normalized(lits, n);
        int c = store_clause(db, lits, n);
        // Nothing is assigned yet, so any two literals may be watched.
        if (n >= 2) watch(db, c, lits[0], lits[1]);
        seen++;
    }
    if (ok && vars < 0) {
        say(why, why_len, "no `p cnf` line");
        ok = 0;
    }
    if (ok && seen != clauses) {
        say(why, why_len, "the `p cnf` line says %ld clauses, and the text has %ld", clauses, seen);
        ok = 0;
    }
    free(line.buf);
    free(lits);
    if (!ok) drat_free(db);
    return ok;
}

// ── Propagation ──────────────────────────────────────────────────────────

// 1 true, 0 false, -1 unset.
static int lit_value(const DratDb *db, int lit) {
    int v = db->val[abs_int(lit)];
    if (v < 0) return -1;
    return lit > 0 ? v : !v;
}

static void set_true(DratDb *db, int lit, int reason) {
    int v = abs_int(lit);
    db->val[v] = (signed char)(lit > 0);
    db->reason[v] = reason;
    db->trail[db->trail_len++] = v;
}

// Unsets the trail past its first `keep` entries.
static void unset_to(DratDb *db, int keep) {
    while (db->trail_len > keep) db->val[db->trail[--db->trail_len]] = -1;
}

// Unit propagation from trail entry `head` on; gives 1 when it reaches a
// conflict. Each literal made false visits the clauses watching it: a clause
// whose other watch is true is skipped; else the watch moves to a literal
// not false, and with none the clause is unit (its other watch is set) or a
// conflict (that one is false too).
static int propagate(DratDb *db, int head) {
    for (; head < db->trail_len; head++) {
        int v = db->trail[head];
        int false_lit = db->val[v] ? -v : v;
        DratList *list = &db->watches[slot(db, false_lit)];
        int kept = 0, conflict = 0;
        for (int i = 0; i < list->len; i++) {
            int c = list->at[i];
            DratClause *cl = &db->clauses[c];
            if (!cl->live) continue;
            if (conflict) {
                list->at[kept++] = c;
                continue;
            }
            int side = cl->watch[0] == false_lit ? 0 : 1;
            int other = cl->watch[1 - side];
            if (lit_value(db, other) == 1) {
                list->at[kept++] = c;
                continue;
            }
            int moved = 0;
            for (int k = 0; k < cl->len && !moved; k++) {
                int lit = db->lits[cl->start + k];
                if (lit != other && lit_value(db, lit) != 0) {
                    cl->watch[side] = lit;
                    push(&db->watches[slot(db, lit)], c);
                    moved = 1;
                }
            }
            if (moved) continue;
            list->at[kept++] = c;
            if (lit_value(db, other) == 0) {
                conflict = 1;
            } else {
                set_true(db, other, c);
            }
        }
        list->len = kept;
        if (conflict) return 1;
    }
    return 0;
}

// Propagates the top level again from its first `keep` entries: each live
// unit clause set, then every entry propagated, so a clause left unit by
// what was cut is found.
static void settle_top(DratDb *db, int keep) {
    unset_to(db, keep);
    db->top_conflict = 0;
    for (int i = 0; i < db->units.len && !db->top_conflict; i++) {
        int c = db->units.at[i];
        if (!db->clauses[c].live) continue;
        int lit = db->lits[db->clauses[c].start];
        int v = lit_value(db, lit);
        if (v == 0) db->top_conflict = 1;
        if (v < 0) set_true(db, lit, c);
    }
    if (!db->top_conflict) db->top_conflict = propagate(db, 0);
    db->top_len = db->trail_len;
}

// After clause `c` was deleted: the top level is cut back to the literal it
// was the reason for, if any, and settled again; rebuilt whole if it
// conflicted, since the conflict may have needed `c`.
static void after_deletion(DratDb *db, int c) {
    if (db->top_conflict) {
        settle_top(db, 0);
        return;
    }
    const DratClause *cl = &db->clauses[c];
    for (int i = 0; i < cl->len; i++) {
        int v = abs_int(db->lits[cl->start + i]);
        if (db->val[v] < 0 || db->reason[v] != c) continue;
        for (int t = 0; t < db->top_len; t++) {
            if (db->trail[t] == v) {
                settle_top(db, t);
                return;
            }
        }
    }
}

// Adds a lemma to the database and to the top level. Its watches are two
// literals not false where there are two; else a true one and any other, or
// its one open literal, which is set, and a false one; else it conflicts.
static void add_lemma(DratDb *db, const int *lits, int n) {
    int c = store_clause(db, lits, n);
    if (db->top_conflict) {
        if (n >= 2) watch(db, c, lits[0], lits[1]);
        return;
    }
    int open[2] = {0, 0}, num_open = 0, true_lit = 0;
    for (int i = 0; i < n; i++) {
        int v = lit_value(db, lits[i]);
        if (v == 1 && true_lit == 0) true_lit = lits[i];
        if (v < 0 && num_open < 2) open[num_open++] = lits[i];
    }
    if (n >= 2) {
        if (true_lit != 0) {
            watch(db, c, true_lit, true_lit == lits[0] ? lits[1] : lits[0]);
        } else if (num_open == 2) {
            watch(db, c, open[0], open[1]);
        } else if (num_open == 1) {
            watch(db, c, open[0], open[0] == lits[0] ? lits[1] : lits[0]);
        } else {
            watch(db, c, lits[0], lits[1]);
        }
    }
    if (true_lit != 0) return;
    if (num_open == 0) {
        db->top_conflict = 1;
    } else if (num_open == 1) {
        int head = db->trail_len;
        set_true(db, open[0], c);
        db->top_conflict = propagate(db, head);
        db->top_len = db->trail_len;
    }
}

// RUP: the clause's literals all false, propagation conflicts. A clause
// holding a literal and its negation holds trivially. The check starts from
// the top level and is undone back to it.
static int has_rup(DratDb *db, const int *lits, int n) {
    if (db->top_conflict || db->empty_live > 0) return 1;
    int conflict = 0;
    for (int i = 0; i < n && !conflict; i++) {
        int v = lit_value(db, lits[i]);
        if (v == 1) conflict = 1; // its negation was set: a tautology, or it holds at the top level
        if (v < 0) set_true(db, -lits[i], -1);
    }
    if (!conflict) conflict = propagate(db, db->top_len);
    unset_to(db, db->top_len);
    return conflict;
}

// RAT on `pivot`: every resolvent with a live clause holding -pivot has RUP
// or is a tautology. `scratch` grows to hold one resolvent.
static int has_rat(DratDb *db, const int *lits, int n, int pivot, int **scratch, int *cap) {
    const DratList *holding = &db->occ[slot(db, -pivot)];
    for (int k = 0; k < holding->len; k++) {
        const DratClause *d = &db->clauses[holding->at[k]];
        if (!d->live) continue;
        int need = n + d->len;
        if (need > *cap) {
            *cap = need;
            *scratch = grown(*scratch, (size_t)*cap, sizeof **scratch);
        }
        int m = 0;
        for (int i = 0; i < n; i++) (*scratch)[m++] = lits[i];
        for (int i = 0; i < d->len; i++) {
            int lit = db->lits[d->start + i];
            if (lit != -pivot) (*scratch)[m++] = lit;
        }
        m = normalized(*scratch, m);
        if (is_tautology(*scratch, m)) continue;
        if (!has_rup(db, *scratch, m)) return 0;
    }
    return 1;
}

// ── The proof ────────────────────────────────────────────────────────────

DratResult drat_check(DratDb *db, const char *proof) {
    DratResult r = {0};
    LineBuf line = {0};
    int *lits = NULL, cap = 0;
    int *scratch = NULL, scratch_cap = 0;
    int line_no = 0;
    int done = 0;
    settle_top(db, 0);
    const char *at = proof;
    while (!done && *at) {
        at = next_line(at, &line);
        line_no++;
        const char *s = line.buf;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == 0 || *s == 'c') continue;
        int deletion = *s == 'd';
        if (deletion) s++;
        int n = parse_lits(s, &lits, &cap);
        if (n < 0 || !vars_in_range(db, lits, n)) {
            r.failed_line = line_no;
            say(r.why, sizeof r.why, "not a clause of variables 1..%d ended by 0", db->num_vars);
            break;
        }
        int pivot = n > 0 ? lits[0] : 0;
        n = normalized(lits, n);
        if (deletion) {
            int c = delete_clause(db, lits, n);
            if (c < 0) {
                r.failed_line = line_no;
                say(r.why, sizeof r.why, "a deletion of a clause the database does not hold");
                break;
            }
            after_deletion(db, c);
            r.deletions++;
            continue;
        }
        r.lemmas++;
        if (n == 0) {
            if (has_rup(db, lits, 0)) {
                r.ok = 1;
                r.empty_line = line_no;
            } else {
                r.failed_line = line_no;
                say(r.why, sizeof r.why, "the empty clause does not follow: propagation over the database leaves no conflict");
            }
            done = 1;
            continue;
        }
        if (has_rup(db, lits, n)) {
            add_lemma(db, lits, n);
        } else if (has_rat(db, lits, n, pivot, &scratch, &scratch_cap)) {
            r.rat_lemmas++;
            add_lemma(db, lits, n);
        } else {
            r.failed_line = line_no;
            say(r.why, sizeof r.why, "a lemma that follows by neither RUP nor RAT on its first literal, %d", pivot);
            done = 1;
        }
    }
    if (!done && r.failed_line == 0) {
        if (has_rup(db, lits, 0)) {
            r.ok = 1;
        } else {
            say(r.why, sizeof r.why, "the proof ends with no empty clause, and propagation over the formula and its lemmas leaves no conflict");
        }
    }
    free(line.buf);
    free(lits);
    free(scratch);
    return r;
}

// ── A model ──────────────────────────────────────────────────────────────

int drat_model_satisfies(const DratDb *db, const char *solver_output, char *why, size_t why_len) {
    signed char *val = malloc((size_t)db->num_vars + 1);
    if (val == NULL) {
        fprintf(stderr, "drat: out of memory\n");
        abort();
    }
    memset(val, -1, (size_t)db->num_vars + 1);
    LineBuf line = {0};
    int ok = 1, said_sat = 0, ended = 0;
    const char *at = solver_output;
    while (ok && *at) {
        at = next_line(at, &line);
        if (strcmp(line.buf, "s SATISFIABLE") == 0) said_sat = 1;
        if (line.buf[0] != 'v') continue;
        const char *s = line.buf + 1;
        for (;;) {
            while (*s == ' ' || *s == '\t' || *s == '\r') s++;
            if (*s == 0) break;
            char *end;
            long x = strtol(s, &end, 10);
            if (end == s || x > db->num_vars || x < -db->num_vars) {
                say(why, why_len, "a `v` line holds something other than literals of variables 1..%d", db->num_vars);
                ok = 0;
                break;
            }
            s = end;
            if (x == 0) {
                ended = 1;
                continue;
            }
            int v = abs_int((int)x);
            if (val[v] >= 0) {
                say(why, why_len, "variable %d is named twice", v);
                ok = 0;
                break;
            }
            val[v] = (signed char)(x > 0);
        }
    }
    if (ok && !said_sat) {
        say(why, why_len, "no `s SATISFIABLE` line");
        ok = 0;
    }
    if (ok && !ended) {
        say(why, why_len, "the `v` lines are not ended by 0");
        ok = 0;
    }
    for (int v = 1; ok && v <= db->num_vars; v++) {
        if (val[v] < 0) {
            say(why, why_len, "variable %d has no value", v);
            ok = 0;
        }
    }
    for (size_t c = 0; ok && c < db->num_clauses; c++) {
        const DratClause *cl = &db->clauses[c];
        if (!cl->live) continue;
        int satisfied = 0;
        for (int i = 0; i < cl->len && !satisfied; i++) {
            int lit = db->lits[cl->start + i];
            satisfied = val[abs_int(lit)] == (lit > 0);
        }
        if (!satisfied) {
            say(why, why_len, "clause %zu has no true literal", c + 1);
            ok = 0;
        }
    }
    free(line.buf);
    free(val);
    return ok;
}
