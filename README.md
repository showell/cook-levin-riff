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

## Status

The machine, the search, the general polynomial and the stepper are
ported. The output matches the Python's byte for byte, and so do the
polynomials' printing, the four step polynomials included; `find_solutions`
cross-checks the stepper on its example programs as the Python does. Still
to come: a Boolean ring (x² = x) that composes all six steps.

## License

[UPL 1.0](LICENSE).
