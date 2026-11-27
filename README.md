# A riff on Cook-Levin

The code here was written by AI coding agents (Claude), with human supervision.

A C port of Steve Howell's virtual-machine riff on the Cook-Levin theorem: a
machine with one 2-bit register, AX, and four opcodes (`nada`, `zero`,
`decr`, `mod2`), whose programs are exactly six instructions long. Every
program accepts some language, a subset of the inputs {0, 1, 2, 3}, and the
search over all 4,096 programs finds a program for each of the 16 languages.
Every step of the machine can also be written as a polynomial in six 0/1
variables, and composing six steps gives the whole program as polynomials.

## Layout

- `src/vm.c`: the machine, its interpreter `run_progam` (named as the
  Python names it), assemble and disassemble, the language codes.
- `src/search.c`: `find_solutions`, the search and its report.
- `src/main.c`: what the Python prints when it runs, byte for byte.
- `src/ring_int.c`: the coefficient ring, int64 with every add, multiply,
  negate and power checked; an overflow stops the program naming the
  operation and its operands (Python's integers never overflow).
- `src/poly.c`: the general multivariate polynomial of poly.py, its
  canonical order and its printing exactly Python's `str`.
- `src/stepper.c`: stepper.py, one machine step as four polynomials in six
  0/1 variables (NOT, AND and OR as 1 - x, x*y and x + y - x*y), built
  once, unreduced (hb' has 786 terms, lb' 2,979), and `step` and
  `test_with_stepper` over them; beside them a compiled form of each
  polynomial over a 6-slot value array, for checking every program fast.
- `src/boolring.c`: the Boolean ring, where x² = x: a monomial is a 32-bit
  mask of its variables, so multiplying two is OR; coefficients are
  checked int64.
- `src/compose.c`: all six steps composed in the Boolean ring, over 14
  variables (the input's two bits and each instruction's two);
  `src/compose_main.c` prints each step's term counts and writes the four
  final polynomials to `out/`.
- `src/sat.c`: each puzzle as DIMACS CNF, the Cook-Levin construction;
  `src/sat_main.c` is its command line, and `sat/oracle.shale` checks it
  against rocsat (below, "SAT").
- `src/plus.c`, `src/census.c`, `src/plus_sat.c`: Nibble++ (below, "Nibble++").
- `web/`: the playground (below).
- `js/`: the machine, the search and the composition in JavaScript, run by
  node, roc-js-interpreter's VM and its x86-64 compiler (below,
  "JavaScript").
- `shale/`: the same in Shale, Steve's scripting language (below, "Shale").
- `tests/`: the Python's asserts, every program on every input against a
  table of each opcode's rule, and the golden test against the Python's
  output (`tests/golden/find_solutions.txt`); Poly's strings against
  poly.py's, a product's and a power's as golden files; the four step
  polynomials' strings against stepper.py's (`tests/golden/step_*.txt`);
  and the stepper against the interpreter on every one of the 4,096
  programs and all 4 inputs (Python checks only the 16 example programs).

## Building and testing

The builds and tests run through Shale, Steve's scripting language, from the repository's root:

```sh
shale build.shale          # fast tier: tcc runs every test and the golden check
shale build.shale full=1   # also gcc -O2 -Wall -Wextra -Werror, and ASan/UBSan
```

Each check prints one line; the run exits 0 only when all pass. Binaries go
to `out/`.

## Nibble++

Nibble++ is Nibble with a 4-bit AX (inputs 0 to 15, so 65,536 languages),
programs of any length, and the opcodes `half` (AX >> 1, code 0, where
Nibble has `nada`), `zero`, `decr` and `mod2`. `src/plus.c` is its plain
interpreter. `src/census.c` finds every language's shortest program
exactly, with a witness, by breadth-first search over the machine's states
(each input's AX, or accepted, or rejected), each state kept once; with all
four opcodes it reaches 374,003 states. `src/census_main.c` prints the
count of languages at each shortest length for {zero, decr},
{zero, decr, mod2}, {zero, decr, half} and all four, held by `build.shale`
to `tests/golden/census.txt`, and `python/census4.py` is a second oracle
held to the same file, and so is the Roc port's census (`roc/Census.roc`,
below). `tests/test_census.c` runs every witness on the
interpreter, enumerates every program of up to 9 lines, and checks three
theorems: `decr` pads any witness to 32 lines (64 bits) safely, {zero,
decr} needs max(S) + |S| lines, and the Fibonacci counts hold where the
census says they do. `docs/NIBBLE_PLUS_THEORY.md` explains the census's
lengths without a search: a recursion for all four opcode sets
(`src/theory_plus.c`), a closed form and its counts for {zero, decr, mod2},
and when `half` helps; `tests/test_theory_plus.c` holds each claim to the
census.

Nibble++ meets rocsat in `src/plus_sat.c`, a second machine on `sat.c`'s
gates (the same folding and structural hashing): the Nibble++ step on a
4-bit AX, unrolled for a k-line program. One run's circuit has the 2k
program bits and AX's four as inputs and accept as its output; at k = 28
it is 1,340 gates, so 1,400 variables and 4,020 clauses. The search CNF for
a language W at k lines shares the program bits among 16 runs, one per
input, each held to W's want bit, so a model is a k-line program that
decides W. `src/plus_sat_main.c` is the command line, and
`tests/test_plus_sat.c` holds the CNF to the interpreter by unit
propagation (every program of up to 4 lines, random ones up to 28, and
sampled census witnesses). `sat/plus.shale`, which `build.shale` runs,
hands rocsat evaluation claims (a member SAT and its model checked; a
non-member's wrong model refused and its forced accept UNSAT, the proof
checked by `src/drat.c`) and the cheap search cases, and writes
`sat/plus/verdicts.txt`. `sat/plus_search.shale` runs the expensive search
cases, SAT at the census length and UNSAT one line shorter with a checked
DRAT proof, and writes their times to `sat/plus/search.txt`; the build
doesn't run it. `sat/plus_guided.shale` runs the same cases as a guided
search (`plus_sat search-on`): rocsat is asked with the runs of a few
inputs, from 0 and 15, and an input the found program gets wrong joins them
each round, until the program decides W (SAT) or no program gets even
those inputs right (UNSAT, its proof over the smaller formula checked). It
writes `sat/plus/guided.txt` beside the full search's numbers; the build
runs one of its cases, {15}, in `sat/plus.shale`. The three share
`sat/plus_rocsat.shale`.

## Playground
The machine's assembly language is called **Nibble**. `web/` is a puzzle
page for it, with 16 puzzles, one for each of the 16 languages: accept
exactly its inputs ("accept {0, 3}") and reject the rest. Each is solved by
picking the instructions. There is no Run
button: every pick runs the program, and the trace shows `AX` after every
step on all four inputs (a row a step, a column an input, so it fits a
phone), then how each run ended and what the puzzle wants, and says which
input went wrong.

The first four puzzles are lessons, each with a sentence on one opcode:
`is_tiny`, {0}, one line (`zero`); `is_one`, {1}, whose line 1 is fixed and
locked as `decr`, so the player picks line 2 (`zero`); `is_even`, {0, 2},
the same with line 1 fixed as `mod2`; and `grumpy`, { } ("Reject all
inputs"), one line that opens as `zero`, which `nada`, `decr` or `mod2`
each solve, since none of them ever accepts on its own. The other twelve
languages ramp up by effort: the length of the shortest program, with each
`mod2` in it counted as a line more, since `mod2` takes more thought; a tie
goes to the puzzle with no `mod2`, then fewer `zero`s, then more solutions.
So `is_two` (`decr, decr, zero`) comes next, `is_odd` (three lines with a
`mod2`) is ninth, and `is_not_tiny` (five lines with a `mod2`) is last. Each puzzle says how short its best program is ("This
can be solved with a three-line Nibble program", a program's length being
its instructions before its trailing `nada`s), and its lines past that
length are locked to `nada`; every other line starts as `nada`, so the
player chooses where to replace it. Past the lessons, a puzzle whose
shortest program can end in `zero` has that `zero` fixed as a hint: its
last open line opens as `zero` and is locked. All twelve do, since an op
after a program's last `zero` can only reject, as running off the end
does, so a shortest program always ends in one. A fixed line is locked and
drawn light blue, its op highlighted, and the hint says so ("This can be
solved with a four-line Nibble program. Line 4 is already fixed as zero.
Your job: fix lines 1 to 3. Lines 5 to 6 are locked to nada."). The lessons
leave theirs alone: on `is_tiny`, `is_one` and `is_even` the final `zero`
is the lesson's answer. The table records each puzzle's open lines
(`lines`), its fixed ones (`fixed`, a bitmask, line k's bit `1 << (k - 1)`,
each locked to its op in `start`) and the program it opens with (`start`);
`web/puzzles.c` checks that each start leaves its puzzle unsolved, that each
fixed line is open and holds an op in `start`, that a line is left to pick,
and that a shortest program solves it within its lines and fixed ones. Every puzzle
opens unsolved. The moment a pick solves it, the page says so, with how
many six-line programs solve it, and the next puzzle opens. Nothing is
kept between visits: a reload starts at puzzle 1 with only it open.

The page runs the Roc port's machine. `roc/NibbleWasm.roc` is
`roc/Machine.roc` behind the three functions the page calls: `status(n,
code)`, `after(n, code, k)` (AX, halted and the Status packed in one
number) and `accepts(code)`. The pinned Roc builds it for wasm32
(`--opt=speed`) on the small platform in `web/platform/`, and links it with
`web/platform/host.c`, the allocator and crash functions, which the
system's clang compiles to a wasm object. The result is an 8.3 KB module
that imports nothing and exports those three functions (beside its memory
and the stack's globals). The
table of puzzles still comes from C: `web/puzzles.c` counts every
program's language and finds each puzzle's shortest program.

```sh
shale web/build.shale      # the table, the wasm, the page in out/web/site, and its check
```

The check, `web/page_check.mjs`, holds the Roc wasm to the C on all
4,096 × 4 runs, by `status` and by six `after` steps, and to the C's
16-puzzle table and its order, so the page is a cross-check of the two
ports. It then loads the page in
headless Chromium at a phone's width (390px), solves the first puzzle by
clicking, sees that neither the page nor the trace scrolls sideways and that
the locked and fixed lines are locked, sees a reload start fresh with
nothing in localStorage, solves `is_one` and `is_even` by one click on
line 2's `zero` and `grumpy` by one click, then opens puzzles 5 to 16 in
turn, checks each one's locks (its fixed `zero` among them) and hint, and
solves it by clicks; then it follows the link to the sandbox and plays it by
clicks and typed numbers, and on to the Nibble++ sandbox (below). It
saves screenshots to `out/web/page.png`, `out/web/page-given.png`,
`out/web/page-fixed.png`, `out/web/page-sandbox.png` and
`out/web/page-plus.png`. The page's files are
named by their bytes, and its URLs are relative, so `out/web/site` can be
served from any folder.

Beside the puzzles, `web/sandbox.html` is the same machine with no puzzle:
six free lines, all `nada` to start, the trace, the op tips and the machine
code, and in place of a verdict the problem the program decides, its
accepted set and its name in the puzzle table (program #1689 decides
`is_not_one`, {0, 2, 3}). A program number from 0 to 4095, typed in, loads
that program. The two pages link to each other and share `web/machine.js`
(the wasm, the table and the parts both draw) and `web/nibble.css`.

`web/plus.html` is the same sandbox for Nibble++: `AX` holds 0 to 15, the
ops are `zero`, `decr`, `mod2` and `half`, shown in that order (the
workhorses first; the codes stay half 0, zero 1, decr 2, mod2 3), and a
program is 1 to 30 lines (`+ line` and `− line`), opening as `zero`,
`decr`, `zero`, `decr`, `zero` and 25 `decr`s, which decide {0, 1, 2}. Its trace is a row an
input, 16 of them, showing `AX` after the line the step buttons (Start,
Back, Step, End) are on, then how each run ends; the box above the program
is the language it decides: its name, its accepted set (runs written as
ranges, {0–13, 15}) and its code of the 65,536. The names are predicates,
`is_small_fib` or `is_prime_or_mult_of_4`, each the cheapest that denotes
its set exactly; `web/plus_names.c` names all 65,536 ahead of time, the
page's table `plus_names.bin` keeps the 54,461 of cost 4 or less, and
`web/plus_names_check.c` parses every name back to its set
(`docs/NIBBLE_PLUS_NAMES.md`).
Its number is two bits a line, line 1 first, up to 60 bits, so the page
keeps it as a BigInt; a number typed in loads into the lines there are.
Its machine is `roc/PlusWasm.roc`, `roc/Plus.roc` behind `status(n, code,
lines)`, `after(n, code, k)` and `accepts(code, lines)`, a program's code
being a U64 as `src/plus.h`'s `plus_pack` lays it out; it is built on
`web/platform/plus.roc`, `main.roc`'s twin over the same `host.c`, to a
5.3 KB module. `web/plus_runs.c` writes `src/plus.c`'s verdicts on 27,848
programs (every one of 1 to 7 lines, the four one-op 30-line programs and
6,000 random ones of 8 to 30 lines), and the check holds the wasm to them
on every input and at every line. It links to the Nibble sandbox and back,
and shares `web/nibble.css` but not `web/machine.js`, which loads the
Nibble wasm and puzzle table as it starts.

## SAT

`src/sat.c` writes each Nibble puzzle as a SAT problem in DIMACS CNF. This
is the Cook-Levin construction itself, on a machine small enough to see
whole: the question "which six-instruction programs accept exactly this
language?" becomes one Boolean formula whose satisfying assignments are
exactly those programs.

The formula's first 12 variables are the program's bits: variable 2i - 1 is
instruction i's high bit and 2i its low bit, as `compose.h` numbers them
(less the input's two bits), so a model reads straight off as a program.
The machine is then unrolled: each of its six steps, on each of the four
inputs, is a copy of the step's logic, wired to the state the step before
left. That logic is `construct_polynomials` from `stepper.c`, gate for gate,
with NOT, AND and OR the gates that 1 - x, x*y and x + y - x*y stand for.
Each AND and OR is a Tseitin variable tied to its inputs by three clauses,
and NOT is just a literal's sign. A gate on a constant is folded, and a gate
already built from the same inputs is reused, so every puzzle's formula has
416 variables and 1,216 clauses. The last four clauses ask that each
input's final `accepted` be what the language wants. Options lock lines:
`len=N` forces the lines past N to `nada`, and `fix=K:OP` fixes line K, as
the playground does.

```sh
out/tcc/sat 9 len=5 > is_extreme.cnf   # the CNF of {0, 3}, at most five lines
out/tcc/sat facts                      # each language's count, shortest length and CNF size
```

`tests/test_sat.c` proves the encoding without any solver. With every input
a constant, folding evaluates the step, and it matches the stepper on all
64 of the step's inputs. Then, for every puzzle, it sets the program bits to
each of the 4,096 programs in turn and unit-propagates them through the
CNF. Propagation alone decides every Tseitin variable, because each gate's
inputs come before it. The assignments that survive are exactly the
programs the machine says solve the puzzle: 844, 681, 303 and so on,
4,096 over the 16 languages. With lines past N forced to `nada`, the
survivors are exactly the solving programs of length N or less, so the CNF
is satisfiable exactly when N is at least the puzzle's shortest length.

`sat/oracle.shale` uses Matt Pizenberg's
[rocsat](https://github.com/mpizenberg/rocsat), a CDCL SAT solver written
in Roc, as an independent oracle. For each puzzle, it first solves the CNF
and checks the model's program on the machine. It then counts the solutions
by adding a clause that blocks each model and solving again until rocsat
says UNSAT, and checks that count against the machine's. Finally it proves
the shortest length: one line shorter is UNSAT, and the shortest length
itself is SAT. The script prints one line per puzzle, then a table of the
15 DRAT proofs, and fails if any check fails. It runs about 4,250 programs
in about a minute, so it isn't in `build.shale`'s tiers:

```sh
shale sat/oracle.shale     # needs rocsat built at build:rocsat/src/rocsat (or rocsat=PATH)
```

Each UNSAT comes with rocsat's DRAT proof, and `src/drat.c` checks it, so
the shortest lengths rest on no trust in the solver. The checker replays
the proof forward: each added lemma must give a conflict by unit
propagation once its literals are set false (RUP), over the formula and the
lemmas so far; each deletion removes a clause the database holds, or the
proof is refused; and the proof ends at the empty clause, or with the
database refuted by propagation. A lemma that fails RUP is tried as a RAT
on its first literal. Propagation watches two literals a clause and keeps
what the database alone propagates between lemmas, so Nibble++'s search
proofs, up to 145,384 lines and 17 MB, check in under a minute (`src/drat.h`). All 15 of
rocsat's proofs pass by RUP alone: 286 to 821 lemmas each, 7,440 to 14,698
bytes, each checked in 2 to 6 ms by the tcc build.
`drat model` holds each SAT answer's `v` lines to every clause too, beside
the machine's check of its program.

```sh
out/tcc/drat check out/sat/13-len5.cnf sat/proofs/13-len5.drat   # verified: 286 lemmas (0 by RAT), ...
out/tcc/drat model out/sat/13-len6.cnf sat/proofs/13-len6.model  # model: every one of 1216 clauses ...
```

The oracle keeps rocsat's outputs in `sat/proofs/`: each proof,
`<lang>-len<N>.drat`, and its whole answer at the shortest length,
`<lang>-len<N>.model`. It writes them on every run, so `git status` after a
run says whether rocsat answered as before; its search is deterministic.
`tests/test_drat.c` checks the committed proofs against the CNFs `sat.c`
makes, and holds each to fail against the formula one line longer, which is
satisfiable, beside small
hand-made proofs that must pass and planted bad ones that must fail.

`sat/claims.shale` hands rocsat claims about the whole decider of "The
circuit, in full": its equations (`circuit_main equations`) turned into CNF
by `roc/ClaimCnf.roc`, with the 16 inputs and `decides` = 1 fixed by unit
clauses. Program 1689 deciding {0, 2, 3} and program 2704 deciding {3} are
SAT, and 1689 deciding {0, 2} is UNSAT, each with 0 decisions, since
propagation alone settles every gate. It writes `sat/claims/verdicts.txt`
and `sat/claims/gates.txt`, which the book shows:

```sh
shale sat/claims.shale     # needs rocsat built at build:rocsat/src/rocsat (or rocsat=PATH)
```

`sat/intro.shale` shows rocsat's two jobs, for the book's chapter "The SAT
solver", on a three-clause CNF: `rocsat check` confirms a true assignment
and names the clause a false one breaks, and `rocsat solve` finds an
assignment and answers UNSATISFIABLE once two unit clauses contradict the
third clause. It fails unless each answer is the expected one, and writes
the files it hands rocsat and `sat/intro/verdicts.txt` to `sat/intro/`:

```sh
shale sat/intro.shale      # needs rocsat built at build:rocsat/src/rocsat (or rocsat=PATH)
```

DIMACS writes (x1 OR NOT x2) as `1 -2 0`, which is cryptic to read, so
two thin scripts convert a CNF or a model to a readable form and back:
`(x1 OR NOT x2)` a clause, `3 variables, 2 clauses` for the `p cnf 3 2`
line, `x1 = true, x2 = false` for a model's `v 1 -2 0`. A DIMACS comment
`c g1 = 4`, as `roc/ClaimCnf.roc` writes its names, names variable 4, and
the readable form writes it `g1` and the name as `g1 = x4`. The
conversion is exact both ways, and malformed input stops naming its line.
`sat/readable_form.shale` has the form; `sat/check_readable.shale`, which
`build.shale` runs, round-trips every committed CNF and model under `sat/`,
the named Tseitin example `sat/readable/circuit.txt` (out = (a AND b) OR
(NOT c), nine clauses), and stops each planted file of `sat/readable/bad/`
with the line `sat/readable/bad/stops.txt` holds:

```sh
shale sat/to_readable.shale file=sat/intro/example.cnf
shale sat/to_dimacs.shale file=sat/readable/circuit.txt
```

rocsat builds with its pinned Roc nightly (`roc-version`,
`nightly-2026-08-18-e9be50a`), via `make build`. Its `scripts/install-roc`
installs that nightly under `~/.roc`, or under `ROC_INSTALL_DIR`.

## Status

The machine, the search, the general polynomial and the stepper are
ported. The output matches the Python's byte for byte, and so do the
polynomials' printing, the four step polynomials included; `find_solutions`
cross-checks the stepper on its example programs as the Python does.

All six steps compose. The final polynomials have 319 terms (hb), 2,185
(lb), 2,732 (halted) and 10,324 (accepted), the largest coefficient is 63,
and `tests/test_compose.c` holds all four to the machine on every program
and every input (2^14 cases). The composition takes about a quarter of a
second at -O2.

## The Roc port

`roc/` is the same machine and the same mathematics in Roc, at the compiler
that `roc/roc-version` pins (0.1.0_RC1, at `~/tools/roc-0.1.0_RC1/roc`).

- `Op.roc` and `Machine.roc`: the four ops, the interpreter, assemble and
  disassemble, and the language codes. Python's six asserts are expects.
- `Search.roc` and the app `FindSolutions.roc`: find_solutions' report,
  with each example program also run on the step polynomials, as the
  Python does.
- `BoolPoly.roc`: the Boolean ring. A polynomial is an opaque, canonical
  list of terms (a mask of variables and an I64 coefficient), and Roc's
  `+`, `-` and `*` are the ring's operations, so `x + y - x * y` is
  written as it reads. Roc's I64 arithmetic crashes on overflow, so every
  coefficient is exact or the program stops. Like terms are summed in a
  dense table indexed by mask, which a product or a substitution writes in
  place, so variables run from 0 to 19 (the C's ring takes 32). `values`
  gives a polynomial's value at every point at once, by the zeta transform.
- `Stepper.roc`: the step polynomials, built directly in the ring from
  NOT = 1 - x, AND = x·y and OR = x + y - x·y. The general `Poly` is not
  ported: it exists to print as Python prints, and the Boolean ring's
  canonical form is what the composition needs. The app
  `StepPolynomials.roc` prints the four, held to the C's (below).
- `Compose.roc` and the app `Composition.roc`: the six steps substituted in
  turn, and the proof at all 2^14 (input, program) points. A multilinear
  polynomial is fixed by its values on {0, 1}^n, so agreeing with the
  machine everywhere makes the four final polynomials exactly the C's.
- The app `NibbleWasm.roc`: `Machine.roc` as the Nibble page's wasm
  (Playground, above).
- `Cnf.roc` and the app `ClaimCnf.roc`: Boolean expressions, a parser for
  `circuit_main equations`, Tseitin's encoding (a fresh variable per AND and
  OR, three clauses each, constants folded) and a DIMACS writer; the app
  writes "program P decides problem W" as a CNF for `sat/claims.shale`.
  Its expects hold the whole decider's CNF at 477 variables and 1,400
  clauses, read from `roc/golden/equations.txt`, which `build.shale` holds to
  `circuit_main equations`. `python/distribute.py` expands the same
  equations into CNF by distribution, 3,869 clauses for `decides`.
- `PlusOp.roc`, `Plus.roc`, `Census.roc` and the app `PlusCensus.roc`:
  Nibble++. `Plus.roc` is the interpreter on `Machine.State`, its `zero`,
  `decr` and `mod2` stepped by `Machine.step`. `Census.roc` is
  `src/census.c`'s breadth-first search, a state packed five bits an input
  into a U128, expanded in the C's order, so its witnesses are the C's.
  The app prints `census_main`'s report for the opcode sets it is given,
  after running every witness on `Plus.roc`.

```sh
shale roc/check.shale      # the gate, from the repository's root
shale roc/bench.shale      # one release build, and the composition timed beside the C's
```

The gate runs `roc fmt --check`; `roc check` of the eight apps and
modules, with no errors and no warnings; `roc test` of each, its expects
held exactly (13, 9, 30, 28, 5, 21, 37 and 28); and the four apps but `NibbleWasm.roc` (whose wasm
`web/build.shale` builds and checks) built and run, each output held byte
for byte to a golden. `PlusCensus.roc` is built on LLVM (about 2 s; its
dev build's code takes about 50 s for the four sets) and held to
`tests/golden/census.txt`; the others are built on the dev backend and held to
`tests/golden/find_solutions.txt`,
`roc/golden/step_polynomials.txt` (written by the C's `bp_str`) and
`roc/golden/composition.txt` (`tests/test_compose.c`'s term counts for
every step, the final degrees and coefficients, and the proof: 16,384
points, none differing, 5,596 accepted).

On this box (2 cores, 2026-11-25), the release build of `composition 6`
composes all six steps and runs the proof in about 30 ms; the C's
`compose_main` at -O2, which composes and proves nothing, takes about 260
ms. The gap is the data structure, not the language: the C sums like terms
in a hash table it grows and then sorts, while the Roc sums them in a dense
table indexed by mask (14 variables make at most 16,384 masks), a straight
array write that reads back already in order. The C could use the same
table, and has not been tried with it. The search's report is worked out by
the compiler as it builds, since every input to it is a constant.

The Nibble++ census's release build (2026-11-26) runs the four searches in
about 0.8 s, against `census_main`'s 0.5 s at -O2, and about 1.35 s with
every one of the 262,144 witnesses run on the interpreter, as the gate runs it.

## JavaScript

`js/cook_levin.js` is the machine, the search over all 4,096 programs, the
step polynomials and the six-step composition in one JavaScript program. It
prints find_solutions' report, then the composition's term counts, sizes
and proof, and its output is `tests/golden/find_solutions.txt` followed by
`roc/golden/composition.txt`, byte for byte.

It is written in the subset of JavaScript that
[roc-js-interpreter](https://github.com/showell/roc-js-interpreter) runs,
so the program has three runners and each is an oracle for the others.
That subset has no bitwise operators, so the OR and AND of two masks come
from tables of their 7-bit halves (with 14 variables, every mask is below
2^14), and a bit is a division and a `%`. A polynomial is two arrays, its
masks and its coefficients, kept in mask order. Like terms are summed in
one dense table indexed by mask, as the Roc port sums them; the table is
made once and each slot is put back to 0 as it is read. Every coefficient is
a small integer, so it is exact in a double, and a sum past 2^53 stops the
program. The step polynomials are built from NOT = 1 - x, AND = xy and
OR = x + y - xy, as `roc/Stepper.roc` builds them. The stepper reads each
step from the step polynomials' values at their 64 points, and the proof
evaluates the four final polynomials at all 2^14 points by the zeta
transform, into four arrays made once.

```sh
shale js/oracle.shale      # node, the roc-js VM and js-x86, each held to the goldens
```

The oracle runs the three one at a time, niced, and prints one line for
each:

- **node** runs the file as written.
- **js-vm** is roc-js-interpreter's bytecode VM, in its release build. It
  runs the same program text, all six steps and the proof, in about 15
  million of its steps (its budget is 200 million).
- **js-x86** is roc-js-interpreter's x86-64 compiler. It compiles the
  program to a static executable with no libc, which is then run under GNU
  time to measure its peak memory. Compiled programs have no garbage
  collector yet (a bump allocator over 1 GiB), which is why the big arrays
  are reused.

Every runner's output must match the goldens byte for byte, and the run
fails unless all three do. On this box (2 cores, 2026-11-25, node v22),
node takes about 0.3 s. The VM takes about 80 s. The compiled executable
is 111,698 bytes, takes 0.6 to 1.0 s and peaks at 33 MB resident. So the
compiled program is slower than node here. It hasn't been profiled yet. The
likely cause is that an array's items are values whose kind the compiler
doesn't know, so every read and write of them goes through the runtime's
tag checks.

The VM and the compiler are roc-js-interpreter's, written in Roc. They are
the one cross-repo piece of this repository: `js/oracle.shale` builds both
in the roc-js-interpreter checkout beside this one
(`repos:roc-js-interpreter`), through that repository's own
`tools/roc_build.shale`, which reuses a build its cache already holds. With
no such checkout, the oracle stops and says so.

## Shale

`shale/cook_levin.shale` is the machine, the search, the step polynomials
and the six-step composition once more, in Shale, the scripting language
that runs every shell command on this box. It follows `js/cook_levin.js`
closely. Shale's numbers are 64-bit integers, and every operation on them
is checked, so a coefficient is exact or the run stops. Like the JavaScript
subset, Shale has no bitwise operators, so the OR and AND of two masks come
from tables of their 7-bit halves. Like terms are summed in one dense table
indexed by mask. `part=search` prints find_solutions' report alone,
`part=composition` the composition's alone, and `steps=N` composes only
the first N steps.

```sh
shale shale/check.shale    # the port held to both goldens
```

The check runs the port twice, once for each part, and holds each run's
output byte for byte to its golden: `tests/golden/find_solutions.txt` for
the search, `roc/golden/composition.txt` for the composition and the proof.
It prints one line for each, with its time and step count.

On this box (2 cores, 2026-11-25), the search takes about 0.25 s and 13
million steps. The composition and the proof take about 5.4 s and 397
million steps. Of those, building the tables and the step polynomials
takes about 8 million, steps 1 to 5 take about 22 million, step 6 takes
about 193 million, and the proof at all 16,384 points takes about 174
million. The interpreter runs about 70 million steps a second here. A run's
values are freed only when it ends, so the composition needs between 100
and 300 MB of arena; the script's `-- budget:` line gives it 400 MB and a
billion steps.

## License

[UPL 1.0](LICENSE).
