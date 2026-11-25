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

static int occ_slot(const DratDb *db, int lit) { return lit + db->num_vars; }

static void db_init(DratDb *db, int num_vars) {
    memset(db, 0, sizeof *db);
    db->num_vars = num_vars;
    size_t slots = 2 * (size_t)num_vars + 1;
    db->occ = calloc(slots, sizeof *db->occ);
    db->occ_len = calloc(slots, sizeof *db->occ_len);
    db->occ_cap = calloc(slots, sizeof *db->occ_cap);
    db->val = malloc((size_t)num_vars + 1);
    db->trail = malloc(((size_t)num_vars + 1) * sizeof *db->trail);
    if (!db->occ || !db->occ_len || !db->occ_cap || !db->val || !db->trail) {
        fprintf(stderr, "drat: out of memory\n");
        abort();
    }
    memset(db->val, -1, (size_t)num_vars + 1);
}

void drat_free(DratDb *db) {
    if (db->occ != NULL) {
        for (int s = 0; s < 2 * db->num_vars + 1; s++) free(db->occ[s]);
    }
    free(db->occ);
    free(db->occ_len);
    free(db->occ_cap);
    free(db->val);
    free(db->trail);
    free(db->lits);
    free(db->clauses);
    memset(db, 0, sizeof *db);
}

// Adds a clause of sorted, distinct literals.
static void add_clause(DratDb *db, const int *lits, int n) {
    if (db->num_clauses == db->clauses_cap) {
        db->clauses_cap = db->clauses_cap ? 2 * db->clauses_cap : 1024;
        db->clauses = grown(db->clauses, db->clauses_cap, sizeof *db->clauses);
    }
    while (db->lits_len + (size_t)n > db->lits_cap) {
        db->lits_cap = db->lits_cap ? 2 * db->lits_cap : 4096;
        db->lits = grown(db->lits, db->lits_cap, sizeof *db->lits);
    }
    int index = (int)db->num_clauses++;
    db->clauses[index] = (DratClause){(int)db->lits_len, n, 1};
    for (int i = 0; i < n; i++) {
        db->lits[db->lits_len++] = lits[i];
        int s = occ_slot(db, lits[i]);
        if (db->occ_len[s] == db->occ_cap[s]) {
            db->occ_cap[s] = db->occ_cap[s] ? 2 * db->occ_cap[s] : 8;
            db->occ[s] = grown(db->occ[s], (size_t)db->occ_cap[s], sizeof *db->occ[s]);
        }
        db->occ[s][db->occ_len[s]++] = index;
    }
}

// Removes a live clause with these sorted, distinct literals; gives 0 when
// there is none.
static int delete_clause(DratDb *db, const int *lits, int n) {
    for (size_t c = db->num_clauses; c-- > 0;) {
        DratClause *cl = &db->clauses[c];
        if (cl->live && cl->len == n && (n == 0 || memcmp(db->lits + cl->start, lits, (size_t)n * sizeof *lits) == 0)) {
            cl->live = 0;
            return 1;
        }
    }
    return 0;
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
        add_clause(db, lits, normalized(lits, n));
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

static void set_true(DratDb *db, int lit) {
    db->val[abs_int(lit)] = (signed char)(lit > 0);
    db->trail[db->trail_len++] = abs_int(lit);
}

static void unset_all(DratDb *db) {
    for (int i = 0; i < db->trail_len; i++) db->val[db->trail[i]] = -1;
    db->trail_len = 0;
}

// A clause under the assignment: gives 1 on a conflict (every literal
// false); sets its last literal when every other is false.
static int visit(DratDb *db, int c) {
    const DratClause *cl = &db->clauses[c];
    int open = 0, last = 0;
    for (int i = 0; i < cl->len; i++) {
        int lit = db->lits[cl->start + i];
        int v = lit_value(db, lit);
        if (v == 1) return 0;
        if (v < 0) {
            open++;
            last = lit;
        }
    }
    if (open == 0) return 1;
    if (open == 1) set_true(db, last);
    return 0;
}

// Unit propagation from the assignment made so far, over every live clause;
// gives 1 when it reaches a conflict. First every clause is visited once,
// which sets the units; then each variable set, in order, visits the clauses
// holding its literal made false, the only ones that can become unit.
static int propagate(DratDb *db) {
    for (size_t c = 0; c < db->num_clauses; c++) {
        if (db->clauses[c].live && visit(db, (int)c)) return 1;
    }
    for (int head = 0; head < db->trail_len; head++) {
        int v = db->trail[head];
        int false_lit = db->val[v] ? -v : v;
        int s = occ_slot(db, false_lit);
        for (int k = 0; k < db->occ_len[s]; k++) {
            int c = db->occ[s][k];
            if (db->clauses[c].live && visit(db, c)) return 1;
        }
    }
    return 0;
}

// RUP: the clause's literals all false, propagation conflicts. A clause
// holding a literal and its negation holds trivially.
static int has_rup(DratDb *db, const int *lits, int n) {
    int conflict = 0;
    for (int i = 0; i < n && !conflict; i++) {
        int v = lit_value(db, lits[i]);
        if (v == 1) conflict = 1; // its negation was set: a tautology
        if (v < 0) set_true(db, -lits[i]);
    }
    if (!conflict) conflict = propagate(db);
    unset_all(db);
    return conflict;
}

// RAT on `pivot`: every resolvent with a live clause holding -pivot has RUP
// or is a tautology. `scratch` grows to hold one resolvent.
static int has_rat(DratDb *db, const int *lits, int n, int pivot, int **scratch, int *cap) {
    int s = occ_slot(db, -pivot);
    for (int k = 0; k < db->occ_len[s]; k++) {
        const DratClause *d = &db->clauses[db->occ[s][k]];
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
            if (!delete_clause(db, lits, n)) {
                r.failed_line = line_no;
                say(r.why, sizeof r.why, "a deletion of a clause the database does not hold");
                break;
            }
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
            add_clause(db, lits, n);
        } else if (has_rat(db, lits, n, pivot, &scratch, &scratch_cap)) {
            r.rat_lemmas++;
            add_clause(db, lits, n);
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
