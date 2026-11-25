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
- `web/`: the playground (below).
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

The page runs this repository's C machine: `web/nibble.c` is `src/vm.c`
compiled to wasm by `zig cc -target wasm32-freestanding`, with no libc
(`VM_FREESTANDING` makes a stop a trap). The table of puzzles comes from
C too: `web/puzzles.c` counts every program's language.

```sh
shale web/build.shale      # the table, the wasm, the page in out/web/site, and its check
```

The check, `web/page_check.mjs`, holds the wasm to the C on all 4,096 × 4
runs and to the 16-puzzle table and its order, then loads the page in
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

```sh
shale roc/check.shale      # the gate, from the repository's root
shale roc/bench.shale      # one release build, and the composition timed beside the C's
```

The gate runs `roc fmt --check`; `roc check` of the three apps, with no
errors and no warnings; `roc test` of each, its expects held exactly (30,
28 and 28); and each app built on the dev backend, its output held byte
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

## License

[UPL 1.0](LICENSE).
