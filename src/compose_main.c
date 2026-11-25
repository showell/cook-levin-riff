// compose_main.c: compose all six steps (compose.h) and say what came out:
// each step's four term counts and its time, then the final polynomials'
// largest coefficients and degrees. The four final polynomials are written
// to out/accepted.txt, out/hb.txt, out/lb.txt and out/halted.txt, one line
// each, as bp_str prints them. Run from the repository's root.
// A unity build, as main.c is.

#define _POSIX_C_SOURCE 200809L

#include "vm.c"
#include "stepper.c"
#include "compose.c"
#include "boolring.c"
#include "poly.c"
#include "ring_int.c"

#include <sys/resource.h>
#include <time.h>

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static int write_poly(const char *path, const BPoly *p, const BVars *v) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "compose: cannot write %s\n", path);
        return 0;
    }
    char *s = bp_str(p, v);
    int ok = fprintf(f, "%s\n", s) >= 0;
    free(s);
    return fclose(f) == 0 && ok;
}

int main(void) {
    BVars v = compose_vars();
    double start = seconds();
    BState s = compose_start();
    printf("step  hb  lb  halted  accepted  ms\n");
    for (int i = 1; i <= MAX_STEPS; i++) {
        double t0 = seconds();
        BState next = compose_step(&s, i);
        bstate_free(&s);
        s = next;
        printf("%d  %zu  %zu  %zu  %zu  %.1f\n", i, s.hb.len, s.lb.len, s.halted.len, s.accepted.len,
               (seconds() - t0) * 1000);
    }
    double total = seconds() - start;
    const char *names[4] = {"hb", "lb", "halted", "accepted"};
    const BPoly *polys[4] = {&s.hb, &s.lb, &s.halted, &s.accepted};
    for (int k = 0; k < 4; k++) {
        printf("%s: %zu terms, degree %d, max |coeff| %lld\n", names[k], polys[k]->len, bp_degree(polys[k]),
               (long long)bp_max_abs_coeff(polys[k]));
    }
    printf("composed in %.1f ms\n", total * 1000);
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) printf("max resident %ld KiB\n", ru.ru_maxrss);
    int ok = write_poly("out/accepted.txt", &s.accepted, &v) && write_poly("out/hb.txt", &s.hb, &v) &&
             write_poly("out/lb.txt", &s.lb, &v) && write_poly("out/halted.txt", &s.halted, &v);
    bstate_free(&s);
    return ok && fflush(stdout) == 0 ? 0 : 1;
}
