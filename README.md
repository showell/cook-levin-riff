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
(`--opt=speed`) on the small platform in `web/platform/`, and Zig links it
with `web/platform/host.zig`, the allocator and crash functions, into a
9.5 KB module that imports nothing and exports only those three. The
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
solves it by clicks; it saves screenshots to `out/web/page.png`,
`out/web/page-given.png` and `out/web/page-fixed.png`. The page's files are
named by their bytes, and its URLs are relative, so `out/web/site` can be
served from any folder.

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
the shortest length: one line shorter is UNSAT, with rocsat's DRAT proof
written to `out/sat/`, and the shortest length itself is SAT. The DRAT
proofs are written but not checked here. The script prints one line per
puzzle and fails if any check fails. It runs about 4,150 programs in about
a minute, so it isn't in `build.shale`'s tiers:

```sh
shale sat/oracle.shale     # needs rocsat built at build:rocsat/src/rocsat (or rocsat=PATH)
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

```sh
shale roc/check.shale      # the gate, from the repository's root
shale roc/bench.shale      # one release build, and the composition timed beside the C's
```

The gate runs `roc fmt --check`; `roc check` of the four apps, with no
errors and no warnings; `roc test` of each, its expects held exactly (30,
28, 21 and 28); and the three apps but `NibbleWasm.roc` (whose wasm
`web/build.shale` builds and checks) built on the dev backend, its output held byte
for byte to a golden: `tests/golden/find_solutions.txt`,
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
