// test_drat.c: the DRAT checker (drat.h), both ways. Small hand-made proofs
// that must verify, planted bad ones that must fail (a lemma that does not
// follow, a missing empty clause, a deletion of a clause a later lemma
// needs), a RAT lemma, and models. Then rocsat's committed proofs
// (sat/proofs/, written by sat/oracle.shale) against the CNFs sat.c makes
// here: each of the 15 verifies against its bounded formula, and each fails
// against the formula one line longer, which is satisfiable.

#include "test.h"

#include "../src/vm.c"
#include "../src/sat.c"
#include "../src/drat.c"

#include <stdlib.h>

// (1 2) (1 -2) (-1 2) (-1 -2): every assignment of two variables breaks one.
static const char *FOUR = "c the four clauses of two variables\np cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n";

static DratResult checked(const char *cnf, const char *proof) {
    DratDb db;
    char why[200];
    if (!drat_read_cnf(&db, cnf, why, sizeof why)) {
        fprintf(stderr, "  %s\n", why);
        DratResult bad = {0};
        return bad;
    }
    DratResult r = drat_check(&db, proof);
    drat_free(&db);
    return r;
}

static int contains(const char *s, const char *part) { return strstr(s, part) != NULL; }

TEST(a_rup_proof_ending_in_the_empty_clause_verifies) {
    DratResult r = checked(FOUR, "1 0\n0\n");
    EXPECT(r.ok);
    EXPECT_EQ(r.lemmas, 2);
    EXPECT_EQ(r.rat_lemmas, 0);
    EXPECT_EQ(r.empty_line, 2);
}

TEST(a_proof_whose_lemmas_leave_a_conflict_by_propagation_verifies) {
    DratResult r = checked(FOUR, "c comments and blank lines are skipped\n\n1 0\n");
    EXPECT(r.ok);
    EXPECT_EQ(r.lemmas, 1);
    EXPECT_EQ(r.empty_line, 0);
}

TEST(deletions_match_a_clause_in_any_order_and_are_counted) {
    DratResult r = checked(FOUR, "1 0\nd 2 1 0\nd -2 1 0\n0\n");
    EXPECT(r.ok);
    EXPECT_EQ(r.deletions, 2);
    EXPECT_EQ(r.empty_line, 4);
}

TEST(a_lemma_that_does_not_follow_is_refuted) {
    // (1 2) and (1 -2) force 1, so -1 follows by neither RUP nor RAT.
    DratResult r = checked("p cnf 3 3\n1 2 0\n1 -2 0\n-1 3 0\n", "-1 0\n0\n");
    EXPECT(!r.ok);
    EXPECT_EQ(r.failed_line, 1);
    EXPECT(contains(r.why, "neither RUP nor RAT"));
}

TEST(the_empty_clause_before_its_lemmas_is_refuted) {
    DratResult r = checked(FOUR, "0\n");
    EXPECT(!r.ok);
    EXPECT_EQ(r.failed_line, 1);
    EXPECT(contains(r.why, "the empty clause does not follow"));
}

TEST(a_proof_with_no_empty_clause_and_no_conflict_at_its_end_is_refuted) {
    DratResult r = checked(FOUR, "");
    EXPECT(!r.ok);
    EXPECT_EQ(r.failed_line, 0);
    EXPECT(contains(r.why, "no empty clause"));
    // A lemma that follows, but leaves the formula open by propagation.
    r = checked(FOUR, "1 2 0\n");
    EXPECT(!r.ok);
    EXPECT_EQ(r.lemmas, 1);
    EXPECT(contains(r.why, "no empty clause"));
}

TEST(a_deletion_of_a_clause_a_later_lemma_needs_makes_the_proof_fail) {
    // The same proof as the first test's, with (1 -2) deleted first: 1 then
    // follows by neither RUP nor RAT.
    DratResult r = checked(FOUR, "d -2 1 0\n1 0\n0\n");
    EXPECT(!r.ok);
    EXPECT_EQ(r.deletions, 1);
    EXPECT_EQ(r.failed_line, 2);
}

TEST(a_deletion_of_a_clause_the_database_does_not_hold_is_refused) {
    DratResult r = checked(FOUR, "d 1 0\n");
    EXPECT(!r.ok);
    EXPECT_EQ(r.failed_line, 1);
    EXPECT(contains(r.why, "does not hold"));
    // A clause deleted twice is held only once.
    r = checked(FOUR, "d 1 2 0\nd 1 2 0\n");
    EXPECT_EQ(r.failed_line, 2);
}

TEST(a_rat_lemma_passes_on_its_first_literal_and_is_counted) {
    // 3 has no RUP, and its one resolvent, with (-3 4), is (3 4), which
    // has: -4 forces 1 by (4 1) and breaks (4 -1).
    const char *cnf = "p cnf 4 3\n-3 4 0\n4 1 0\n4 -1 0\n";
    DratResult r = checked(cnf, "3 0\n");
    EXPECT_EQ(r.lemmas, 1);
    EXPECT_EQ(r.rat_lemmas, 1);
    EXPECT_EQ(r.failed_line, 0);
    EXPECT(contains(r.why, "no empty clause"));
    // The planted one: -4 has no RUP, and its resolvent with (-3 4), (-3 -4),
    // has none either.
    r = checked(cnf, "-4 0\n");
    EXPECT_EQ(r.failed_line, 1);
}

TEST(a_malformed_line_is_refused) {
    DratResult r = checked(FOUR, "1 2\n");
    EXPECT_EQ(r.failed_line, 1);
    r = checked(FOUR, "1 x 0\n");
    EXPECT_EQ(r.failed_line, 1);
    r = checked(FOUR, "3 0\n");
    EXPECT_EQ(r.failed_line, 1);
    DratDb db;
    char why[200];
    EXPECT(!drat_read_cnf(&db, "p cnf 2 3\n1 2 0\n", why, sizeof why));
    EXPECT(contains(why, "says 3 clauses"));
}

static int model_ok(const char *cnf, const char *out) {
    DratDb db;
    char why[200];
    EXPECT(drat_read_cnf(&db, cnf, why, sizeof why));
    int ok = drat_model_satisfies(&db, out, why, sizeof why);
    drat_free(&db);
    return ok;
}

TEST(a_model_is_held_to_every_clause) {
    const char *cnf = "p cnf 2 2\n1 2 0\n-1 2 0\n";
    EXPECT(model_ok(cnf, "c a solver's stats\ns SATISFIABLE\nv 1 2 0\n"));
    EXPECT(model_ok(cnf, "s SATISFIABLE\nv -1\nv 2 0\n"));
    EXPECT(!model_ok(cnf, "s SATISFIABLE\nv 1 -2 0\n"));
    EXPECT(!model_ok(cnf, "s SATISFIABLE\nv 2 0\n"));
    EXPECT(!model_ok(cnf, "v 1 2 0\n"));
    EXPECT(!model_ok(cnf, "s SATISFIABLE\nv 1 2 -1 0\n"));
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char *text = malloc((size_t)n + 1);
    EXPECT(fread(text, 1, (size_t)n, f) == (size_t)n);
    text[n] = 0;
    fclose(f);
    return text;
}

// Puzzle `lang`'s CNF with lines past `len` forced to nada, as DIMACS text,
// by way of a file under out/ (the tests run from the repository's root).
static char *cnf_text(int lang, int len) {
    SatPuzzle p = sat_puzzle(lang);
    p.max_len = len;
    Cnf c = sat_encode(&p);
    const char *path = "out/test_drat.cnf";
    FILE *f = fopen(path, "wb");
    EXPECT(f != NULL);
    EXPECT(sat_write_dimacs(f, &c, &p));
    sat_free(&c);
    EXPECT(fclose(f) == 0);
    char *text = read_file(path);
    EXPECT(text != NULL);
    return text;
}

TEST(rocsats_committed_proofs_verify_and_fail_one_line_longer) {
    int found = 0;
    for (int lang = 0; lang < NUM_LANGUAGES; lang++) {
        for (int len = 0; len < MAX_STEPS; len++) {
            char path[64];
            snprintf(path, sizeof path, "sat/proofs/%d-len%d.drat", lang, len);
            char *proof = read_file(path);
            if (proof == NULL) continue;
            found++;
            char *bounded = cnf_text(lang, len);
            char *longer = cnf_text(lang, len + 1);
            DratResult r = checked(bounded, proof);
            if (!r.ok) fprintf(stderr, "  %s: line %d: %s\n", path, r.failed_line, r.why);
            EXPECT(r.ok);
            EXPECT(r.empty_line > 0);
            DratResult planted = checked(longer, proof);
            EXPECT(!planted.ok);
            free(bounded);
            free(longer);
            free(proof);
        }
    }
    EXPECT_EQ(found, 15);
}

TEST_MAIN(TEST_CASE(a_rup_proof_ending_in_the_empty_clause_verifies),
          TEST_CASE(a_proof_whose_lemmas_leave_a_conflict_by_propagation_verifies),
          TEST_CASE(deletions_match_a_clause_in_any_order_and_are_counted),
          TEST_CASE(a_lemma_that_does_not_follow_is_refuted),
          TEST_CASE(the_empty_clause_before_its_lemmas_is_refuted),
          TEST_CASE(a_proof_with_no_empty_clause_and_no_conflict_at_its_end_is_refuted),
          TEST_CASE(a_deletion_of_a_clause_a_later_lemma_needs_makes_the_proof_fail),
          TEST_CASE(a_deletion_of_a_clause_the_database_does_not_hold_is_refused),
          TEST_CASE(a_rat_lemma_passes_on_its_first_literal_and_is_counted),
          TEST_CASE(a_malformed_line_is_refused),
          TEST_CASE(a_model_is_held_to_every_clause),
          TEST_CASE(rocsats_committed_proofs_verify_and_fail_one_line_longer))
