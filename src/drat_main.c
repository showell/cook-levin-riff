// drat_main.c: the DRAT checker's command line (drat.h). A unity build, as
// main.c is.
//
//   drat check CNF PROOF   replays the text DRAT proof PROOF against the
//                          DIMACS formula CNF: one line, `verified: ...` with
//                          its counts and exit 0, or `refuted at line N: ...`
//                          and exit 1
//   drat model CNF OUTPUT  whether the `v` lines of a solver's output OUTPUT
//                          (after `s SATISFIABLE`) satisfy every clause of
//                          CNF: `model: ...` and exit 0, or why not and exit 1

#include "drat.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr, "usage: drat check CNF PROOF | drat model CNF OUTPUT\n");
    return 2;
}

// The whole file, NUL-ended, or NULL (said).
static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "drat: cannot open %s\n", path);
        return NULL;
    }
    size_t cap = 65536, len = 0;
    char *text = malloc(cap);
    size_t got;
    while (text != NULL && (got = fread(text + len, 1, cap - len - 1, f)) > 0) {
        len += got;
        if (len + 1 == cap) {
            cap *= 2;
            text = realloc(text, cap);
        }
    }
    int bad = ferror(f);
    fclose(f);
    if (text == NULL || bad) {
        fprintf(stderr, "drat: cannot read %s\n", path);
        free(text);
        return NULL;
    }
    text[len] = 0;
    return text;
}

int main(int argc, char **argv) {
    if (argc != 4 || (strcmp(argv[1], "check") != 0 && strcmp(argv[1], "model") != 0)) return usage();
    char *cnf = slurp(argv[2]);
    char *other = cnf ? slurp(argv[3]) : NULL;
    if (other == NULL) {
        free(cnf);
        return 2;
    }
    DratDb db;
    char why[200];
    int code = 1;
    if (!drat_read_cnf(&db, cnf, why, sizeof why)) {
        printf("%s: %s\n", argv[2], why);
    } else if (strcmp(argv[1], "check") == 0) {
        int clauses = (int)db.num_clauses;
        DratResult r = drat_check(&db, other);
        if (r.ok) {
            printf("verified: %d lemmas (%d by RAT), %d deletions, over %d variables and %d clauses; ", r.lemmas,
                   r.rat_lemmas, r.deletions, db.num_vars, clauses);
            if (r.empty_line > 0) {
                printf("the empty clause at line %d\n", r.empty_line);
            } else {
                printf("no empty clause, and propagation refutes the formula at the end\n");
            }
            code = 0;
        } else {
            printf("refuted at line %d: %s\n", r.failed_line, r.why);
        }
    } else if (drat_model_satisfies(&db, other, why, sizeof why)) {
        printf("model: every one of %zu clauses has a true literal, %d variables named\n", db.num_clauses,
               db.num_vars);
        code = 0;
    } else {
        printf("not a model: %s\n", why);
    }
    drat_free(&db);
    free(cnf);
    free(other);
    return fflush(stdout) == 0 ? code : 1;
}
