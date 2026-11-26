# The Roc in this repository, and where it could go

A proposal, 2026-11-26. Nothing here is done unless it says so.

## What lives where

There are three places that hold Roc, on two platforms.

- **`roc/`** holds the port, at the pin in `roc/roc-version`, and
  `roc/check.shale` gates it. It has two machines:
  - **The toy:** `Op.roc` and `Machine.roc` (the interpreter),
    `Search.roc` with the app `FindSolutions.roc`, `Fourteen.roc` (the
    machine as one function of 14 bits, which the book quotes),
    `BoolPoly.roc`, `Stepper.roc`, `Compose.roc` with the apps
    `StepPolynomials.roc` and `Composition.roc`, and `Cnf.roc` with the app
    `ClaimCnf.roc`.
  - **Nibble++:** `PlusOp.roc`, `Plus.roc`, `Census.roc` and the app
    `PlusCensus.roc`.

  Each app runs on Roc's default command-line platform, except
  `NibbleWasm.roc`.
- **`web/platform/`** (`main.roc` and `host.zig`) is the Nibble page's wasm
  platform. Its only app is `roc/NibbleWasm.roc`, which wraps `Machine.roc`.
  `web/build.shale` builds it and holds it to `web/puzzles.c`.
- **`book/app/`** is the book's builder app, on roc-apps' book_builder
  platform. It quotes this repository's code, but it imports none of
  `roc/`. It belongs to the book, and it stays apart from the rest.

## What is shared, and what could merge

- **Done here:** `Plus.roc` runs on `Machine.State`, and it steps `zero`,
  `decr` and `mod2` through `Machine.step`. So those three rules live in
  one place for both machines. `Plus.language_code` uses
  `Machine.language_code`. `Machine.roc` and `Op.roc` are unchanged, so
  the wasm and `web/page_check.mjs` are not affected.
- **`Op` and `PlusOp` stay two types.** They share three tags, but each
  type's code is the two bits a line takes, and code 0 means `nada` in one
  and `half` in the other. A five-tag union would lose that, and both
  machines would have to handle an op that the other one has.
- **`Fourteen.roc`'s `step` repeats `Machine.step`'s rule on numeric
  codes.** It could be `Machine.step` over `Op.from_code`. I tried that
  change and its expects pass, but I took it back, because the book quotes
  `Fourteen.f` whole (`{{function roc/Fourteen.roc f}}`), and that quote
  needs the function to stand alone. The merge is the book's call. If the
  book also quoted `Machine.step`, the copy could go.
- **`Search.roc` could fold into `FindSolutions.roc`.** It is that app's
  only module, and the app is nine lines long. This is a small gain, so it
  is not needed.
- **`Stepper.roc`'s `reference_step` is not a copy of `Machine.step`.** It
  is the rule of Python's stepper, which keeps the stepper's `halted`
  quirk. It stays.
- **Nothing in `roc/` is dead.** Each module is reached by an app that the
  gate checks.

## Which C could become Roc

The C is the oracle for each port. A port earns its place when it adds a
check, or when a reader gets the code in a language the book shows. It
does not earn it by replacing the C.

- **`src/theory_plus.c`, next.** It is the recursion of
  `docs/NIBBLE_PLUS_THEORY.md`: pure arithmetic over formal languages, 171
  lines, with no I/O. In Roc it would sit beside `Census.roc`, and an app
  could hold the recursion to the Roc census on all 65,536 languages for
  each set, as `tests/test_theory_plus.c` does in C. That would be a check
  of the theory in a second language.
- **`src/plus_sat.c`, later.** It encodes Nibble++ as a circuit and CNF on
  `sat.c`'s gates. `Cnf.roc` already has a Tseitin encoder and a DIMACS
  writer, but it reads its circuit from `circuit_main equations`. Porting
  it means building Nibble++'s unrolled step as `Cnf.roc` expressions,
  with gates reused, and holding the CNF to `plus_sat_main`'s. That work is
  real, and the C, beside `sat.c`, stays its oracle.
- **`web/puzzles.c` stays C, at least its `runs` table.** `page_check.mjs`
  holds the wasm, which is `Machine.roc`, to that table. A Roc table built
  from `Machine.roc` would hold the machine to itself. The `puzzles` half
  (the order, the locks, the examples) could be Roc, but nothing would be
  gained by porting it.
- **`src/drat.c` stays C.** The book trusts rocsat's UNSAT answers
  because a separate checker, written in C, replays each DRAT proof. That
  checker shares no code or language with rocsat, which is Roc. A Roc
  checker would weaken the very claim the book makes.
- **The other oracles stay C** (`vm.c`, `census.c`, `boolring.c`,
  `poly.c`, `stepper.c`, `compose.c`), and so does the SAT side (`sat.c`,
  `circuit.c`). The C tests and the book's tables are built on them, and
  each Roc port is held to them.
