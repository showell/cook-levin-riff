// drat.h: a DRAT proof checker, forward and plain, for the proofs rocsat
// writes when it says UNSAT (sat/oracle.shale).
//
// A DRAT proof claims that a CNF formula has no model. It is a list of
// lines, each a clause added (a lemma) or, after `d`, a clause deleted, in
// the DIMACS literal syntax, each ended by 0. The checker replays it from
// the start against a clause database that holds the formula:
//
//   - a lemma must follow from the database by RUP (reverse unit
//     propagation): the lemma's literals all set false, unit propagation over
//     the database reaches a conflict. Then it is added. A lemma that fails
//     RUP may still pass as a RAT (resolution asymmetric tautology) on its
//     first literal p: for every clause D in the database that holds -p, the
//     resolvent, the lemma's literals with D's other literals, passes RUP (or
//     holds a literal and its negation). Either way the lemma keeps the
//     formula's satisfiability, so a model of the formula would survive it.
//   - a deletion removes one clause of the database with the same literals,
//     in any order; a deletion that names no clause there is refused. Every
//     deletion is applied, units among them, so each later lemma is checked
//     against a smaller database: never easier.
//   - the proof is done at the empty clause, a lemma of no literals, which
//     passes RUP when propagation over the database alone conflicts. A proof
//     with no empty clause is accepted when that holds at its end.
//
// So an accepted proof says the formula is unsatisfiable, with no trust in
// the solver that wrote it. Propagation is plain: each literal keeps the list
// of clauses that hold it, and a literal set false visits its list. The
// formulas here are small (416 variables, about 1,216 clauses), so clarity
// comes first.

#ifndef COOK_LEVIN_DRAT_H
#define COOK_LEVIN_DRAT_H

#include <stddef.h>

// The clause database: each clause's literals, sorted and without repeats,
// in one pool, and whether it is still live.
typedef struct {
    int start; // into lits
    int len;
    int live;
} DratClause;

typedef struct {
    int num_vars;
    int *lits;
    size_t lits_len;
    size_t lits_cap;
    DratClause *clauses;
    size_t num_clauses;
    size_t clauses_cap;
    // occ[lit + num_vars]: the clauses holding lit, by index.
    int **occ;
    int *occ_len;
    int *occ_cap;
    // The assignment during a check: val[v] is 1, 0, or -1 for unset.
    signed char *val;
    int *trail;
    int trail_len;
} DratDb;

// What a check found. `ok` is 1 when the proof refutes the formula.
typedef struct {
    int ok;
    int lemmas;      // lemma lines checked, the empty clause among them
    int rat_lemmas;  // lemmas that passed by RAT, not RUP
    int deletions;   // deletion lines applied
    int empty_line;  // the line of the empty clause, or 0 when the proof had none
    int failed_line; // the line that failed, or 0
    char why[200];
} DratResult;

// Reads DIMACS text (`c` lines, `p cnf V C`, clauses) into a new database.
// Gives 1, or 0 with `why` filled.
int drat_read_cnf(DratDb *db, const char *text, char *why, size_t why_len);
void drat_free(DratDb *db);

// Replays a text DRAT proof against the database, which it changes.
DratResult drat_check(DratDb *db, const char *proof);

// Whether every clause of the database has a true literal under the
// assignment rocsat's `v` lines name (`v 1 -2 3 ... 0`); every variable must
// be named. Gives 1, or 0 with `why` filled.
int drat_model_satisfies(const DratDb *db, const char *solver_output, char *why, size_t why_len);

#endif
