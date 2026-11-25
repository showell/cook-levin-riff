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
- `web/`: the playground (below).
- `tests/`: the Python's asserts, every program on every input against a
  table of each opcode's rule, and the golden test against the Python's
  output (`tests/golden/find_solutions.txt`); Poly's strings against
  poly.py's, a product's and a power's as golden files.

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
page for it: each of the 16 languages is a puzzle ("accept exactly {0, 3}"),
solved by picking each of the six instructions and pressing Run, which shows
`AX` after every step on all four inputs and says which input went wrong.
The puzzles go easiest first, by how many of the 4,096 programs solve each
(883 for `is_even` down to 1 for `is_not_one`). Each says how short its
best program is ("This can be solved with a two-line Nibble program", a
program's length being its instructions before its trailing `nada`s), and
once solved, how many programs solve it. Progress stays in the browser.

The page runs this repository's C machine: `web/nibble.c` is `src/vm.c`
compiled to wasm by `zig cc -target wasm32-freestanding`, with no libc
(`VM_FREESTANDING` makes a stop a trap). The table of puzzles comes from
C too: `web/puzzles.c` counts every program's language.

```sh
shale web/build.shale      # the table, the wasm, the page in out/web/site, and its check
```

The check, `web/page_check.mjs`, holds the wasm to the C on all 4,096 × 4
runs and to the puzzle table, then loads the page in headless Chromium at a
phone's width, solves the first puzzle by clicking, and sees the progress
outlive a reload; it saves a screenshot to `out/web/page.png`. The page's
files are named by their bytes, and its URLs are relative, so `out/web/site`
can be served from any folder.

## Status

The machine and the search are ported, and their output matches the
Python's byte for byte, and so does the general polynomial's printing. Still
to come: the stepper and its cross-check in `find_solutions` (a TODO hook in
`src/search.c` for now), then a Boolean ring that composes all six steps.

## License

[UPL 1.0](LICENSE).
