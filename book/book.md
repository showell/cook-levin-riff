# A riff on Cook-Levin

A small book about [cook-levin-riff](https://github.com/showell/cook-levin-riff): a toy machine whose every program is a polynomial and a Boolean formula, the six-step composition that took three years to arrive, and six independent ways of computing the same answers that all agree.

## Introduction

The machine in this book has one 2-bit register and four instructions, and every program is six instructions long. It is small enough that every program, every input and every step can be listed, so every claim about it can be checked by running all of them.

Steve Howell wrote the first version by hand in Python, in [virtual-machine](https://github.com/showell/virtual-machine): the machine (`virtual_machine.py`), a general polynomial library (`poly.py`) and a stepper that writes one step of the machine as polynomials (`stepper.py`). In the cook-levin-riff repository, the code was written by AI coding agents (Claude), with human supervision, as its `README.md` says in its second line.

### What the book covers

{{chapter The machine}} sets out the machine, and {{chapter Languages and machines}} gives the computer science behind the word "language", back to Turing. {{chapter Programs as languages}} is the search over all 4,096 programs, and {{chapter Nibble, the puzzle page}} turns its 16 languages into puzzles. {{chapter Arithmetization}} turns one step into polynomials. {{chapter Why composing blew up}} and {{chapter Six steps composed}} take the step through a whole program. {{chapter Cook-Levin as SAT}} turns the same machine into a formula for a SAT solver. {{chapter The oracles}} lines up every implementation that computes the answers, and {{chapter The Roc port}} reads the mathematics in Roc.

### Why the name fits

The Cook-Levin theorem says that satisfiability of Boolean formulas is NP-complete: any problem whose answers a machine can check in polynomial time reduces to it. The proof unrolls the checking machine, step by step, into one formula. This repository does that unrolling on a toy instrument. It makes no claim about NP, and it reduces one machine's questions, never an arbitrary verifier's. It also goes one step past the formula, into arithmetic: the whole program becomes four polynomials with integer coefficients. "A riff" is the honest word for that.

### Where the numbers come from

Each number here comes from the repository: a commit, cited by its short hash, the `README.md`, a golden file, or a program run while the book was written, named where it is used. Code is quoted from the repository as it stands when the book is built. The Python repository's history is cited by links to its commits.

## How it got here

This chapter is the short history, as two git logs record it.

### The Python, 2023

The virtual-machine log opens on 2023-03-11 with [f01c1c3](https://github.com/showell/virtual-machine/commit/f01c1c3) and runs through 158 commits to 2023-04-08, all under Steve's address. The stepper arrived on its first day ([ba9388a](https://github.com/showell/virtual-machine/commit/ba9388a)), the polynomial library from 2023-03-12 ([6bb7429](https://github.com/showell/virtual-machine/commit/6bb7429)), and the step polynomials on 2023-03-14 ([a4e4765](https://github.com/showell/virtual-machine/commit/a4e4765)), the same day as `MAX_STEPS` of 6. The last 2023 commits, on 2023-04-08, begin with "Eliminate most poly-related stuff" ([72bf4c4](https://github.com/showell/virtual-machine/commit/72bf4c4)).

### The composition that stalled, 2026

Five commits marked "wip" followed on 2026-01-28. On 2026-04-16 a measuring harness, `compose.py`, was added with Claude as co-author ([1a5f6fa](https://github.com/showell/virtual-machine/commit/1a5f6fa)). Its message says that step 1 composed in about 0.9 s and that step 2 ran past 30 s in Python, and that those numbers motivated a port to Go. The same afternoon `compose.py` was removed, "porting six-step composition to Go instead" ([a2406b5](https://github.com/showell/virtual-machine/commit/a2406b5)). The next day an essay Claude had written about the project was parked as `DEEP_READ.md` ([94c3d31](https://github.com/showell/virtual-machine/commit/94c3d31)); it lists the six-step composition as the step the project stops before.

The Go port lives in a repository of its own, outside this record. By Steve's account it drowned in big integers; that account is unchecked here.

### The finish, 2026-11-25

The cook-levin-riff log covers one day. The C port of the machine and the search came first (`8051ce2`), then the general polynomial (`ccde4f8`) and the stepper (`5b14d74`). The Boolean ring came at 15:59 (`0c233e3`), and all six steps composed, proved on all 2^14 points, at 16:23 (`1146d43`). The Nibble page (`b1fe11f`), the Roc port (`6206236`), the SAT encoding (`8de51b7`), rocsat as an oracle (`f977bcc`) and the JavaScript port (`e996749`, `c251895`) followed. The first commit is stamped 15:32 and the last 19:21.

## The machine

The machine holds one register, `AX`, of 2 bits, so `AX` is 0, 1, 2 or 3. A run starts with `AX` set to the input. Its program is six instructions, each one of four opcodes, and the machine's assembly language is called Nibble.

### Four opcodes

`src/vm.c` holds the rule, as `run_progam` and its one-step helper:

{{lines src/vm.c from "Machine vm_step(Machine m, Op op) {" to "m.ax = m.ax % 2;"}}

- **`nada`** leaves the state as it is.
- **`zero`** accepts when `AX` is 0, and the run ends; at any other value the run goes on.
- **`decr`** takes 1 from `AX`, and rejects when `AX` is already 0.
- **`mod2`** takes 2 from `AX` when it is 2 or 3, leaving its last bit.

A run that reaches the end of its six instructions rejects. So a program accepts some of the four inputs and rejects the rest, and the set it accepts is its language.

### Counting

Each instruction is one of 4 opcodes, so there are 4^6 = 4,096 programs. Each language is a subset of {0, 1, 2, 3}, so there are 2^4 = 16 languages. Each program is a 12-bit number, two bits an instruction, and `assemble` and `disassemble` in `src/vm.c` convert between the two forms.

## Languages and machines

In computer science a *language* is a set of strings. Pick an alphabet, say the digits 0 to 9, and any set of strings over it is a language: the decimal numerals of the primes, the palindromes, the strings that are valid Roc programs. A language is the cleanest way to state a decision question. "Is 91 prime?" becomes "is the string `91` in the language of primes?"

### Machines that decide

A machine *decides* a language when, given any string, it halts, accepting the strings in the language and rejecting the rest. A program that decides the primes is a primality test; a compiler's parser decides the language of well-formed programs, and rejects the rest with an error.

### Turing's machine

In 1936 Alan Turing described a machine with a finite set of states and an unbounded tape, in "On Computable Numbers, with an Application to the Entscheidungsproblem". It reads and writes the tape one cell at a time, and every algorithm anyone has written since can be carried out by such a machine; that claim is the Church-Turing thesis. Turing also showed that some languages are undecidable: whatever machine one builds for them, some string defeats it. The best known is the halting problem, the language of programs that halt on their input. A machine can *recognize* it, accepting every member by running it, but for some strings outside the language it runs forever, still waiting to answer.

### When deciding is cheap, and when checking is

Decidable languages differ in cost. P is the class decided in time polynomial in the input's length. NP is the class whose members have short certificates that a machine can check in polynomial time: a formula is in SAT, the language of satisfiable Boolean formulas, when an assignment makes it true, and checking an assignment is quick. In 1971 Stephen Cook, in "The Complexity of Theorem-Proving Procedures", and independently Leonid Levin, published in 1973, showed that SAT is as hard as anything in NP. Their proof takes the checking machine, which runs for a polynomial number of steps, and writes each step of its run as clauses: the whole run becomes one formula, satisfiable exactly when a certificate exists.

### This book's machine

The machine here is Turing's idea at its smallest. Its tape is one 2-bit register, its inputs are the four strings 0, 1, 2 and 3, and every run halts within six steps, so every question about it is decidable by trying everything. A language is a subset of {0, 1, 2, 3}, and there are 16. A program decides exactly one of them, the set of inputs it accepts. {{chapter Programs as languages}} asks which programs decide which languages, by running all of them. {{chapter Cook-Levin as SAT}} does what Cook and Levin did: it writes the machine's six steps as clauses, so a puzzle's answer becomes a satisfying assignment.

## Programs as languages

`find_solutions` runs every program on every input and groups the programs by the language each accepts. Its report is the Python's output, and the C prints it byte for byte: `tests/golden/find_solutions.txt` holds it, and `tests/test_golden.c` holds `src/main.c` to it.

### The 16 counts

The counts below come from that golden file, and the shortest lengths from `out/tcc/sat facts` (`src/sat_main.c`), run for this book. The language's code is its inputs as bits: input i accepted sets bit i.

| code | language | programs | shortest |
|---|---|---|---|
| 0 | { } | 844 | 0 |
| 1 | {0} | 681 | 1 |
| 2 | {1} | 303 | 2 |
| 3 | {0, 1} | 172 | 3 |
| 4 | {2} | 248 | 3 |
| 5 | {0, 2} | 883 | 2 |
| 6 | {1, 2} | 74 | 4 |
| 7 | {0, 1, 2} | 13 | 5 |
| 8 | {3} | 63 | 4 |
| 9 | {0, 3} | 12 | 5 |
| 10 | {1, 3} | 520 | 3 |
| 11 | {0, 1, 3} | 150 | 4 |
| 12 | {2, 3} | 13 | 5 |
| 13 | {0, 2, 3} | 1 | 6 |
| 14 | {1, 2, 3} | 15 | 5 |
| 15 | {0, 1, 2, 3} | 104 | 4 |

The counts sum to 4,096, so every program lands in exactly one language, and every language has a program. The search is a proof by construction that the machine recognizes all 16.

### The rare one

{0, 2, 3} has a single program, and it uses all six lines:

```text
zero
decr
decr
zero
decr
zero
```

Input 0 is accepted on line 1. Input 1 drops to 0 on line 2 and is rejected by the `decr` on line 3. Input 2 reaches 0 on line 3 and is accepted on line 4. Input 3 reaches 0 on line 5 and is accepted on line 6. The language skips 1, and the only way past it is to count down past it.

### The common ones

The empty language has 844 programs, since any program that rejects every input will do, and {0, 2} has 883: `mod2` then `zero` is two lines, and many longer programs reach the same verdicts.

## Nibble, the puzzle page

[Nibble](https://roc.lynrummy.com/nibble/) is a puzzle page with one puzzle for each of the 16 languages: pick instructions until the program accepts exactly the puzzle's inputs and rejects the rest. The search of {{chapter Programs as languages}} says every puzzle has an answer, and how many programs are answers.

### What runs in the page

The page runs this repository's C. `web/nibble.c` is `src/vm.c` compiled to freestanding wasm by `zig cc`, and the puzzle table is printed by `web/puzzles.c`, which counts every program's language and finds each puzzle's shortest program (`README.md`, "Playground"). The JavaScript in the page draws; the machine's verdicts are the C's. The page's check, `web/page_check.mjs`, holds the wasm to the C on all 4,096 programs and all four inputs, then plays every puzzle in headless Chromium by clicks.

### Playing a puzzle

Every pick runs the program at once on all four inputs, so the trace always shows the program as it stands. A trace shows each input's `AX` after every step, then how each run ended, what the puzzle wants, and a row marking each input correct or wrong. A wrong program's verdict names every way it is wrong, as in "Your program falsely accepts 0. It falsely rejects 1 and 3." When the program is right, the page says so above the program, folds the instruction buttons away so the whole trace shows, and says how many of the 4,096 programs solve the puzzle. Beside the buttons, the program's machine code shows its twelve bits, two a line, and its number. Each opcode button's tip says what it does to each `AX`: `decr` is "0->R, 1->0, 2->1, 3->2". A reload starts fresh at puzzle 1.

### The lessons

The first four puzzles are lessons, one opcode each:

- **`is_tiny`** ({0}): one line, `zero`.
- **`is_one`** ({1}): line 1 is given as `decr`, and the player picks line 2.
- **`is_even`** ({0, 2}): line 1 is given as `mod2`, and the player picks line 2.
- **`grumpy`** ({ }), "Reject all inputs": one line that opens as `zero`. Any of `nada`, `decr` or `mod2` solves it, since only `zero` ever accepts.

A given line is drawn locked, in light blue, and the hint names it: "Line 1 is already fixed as decr. Your job: fix line 2."

### The ramp

The other twelve ramp by effort: the length of the shortest program, with each `mod2` in it counted as one line more, since `mod2` takes more thought. A tie goes to the puzzle free of `mod2`, then to fewer `zero`s, then to more solutions. The table that `web/puzzles.c` prints puts them in this order:

| # | puzzle | language | shortest | programs |
|---|---|---|---|---|
| 5 | `is_two` | {2} | 3 | 248 |
| 6 | `is_small` | {0, 1} | 3 | 172 |
| 7 | `is_huge` | {3} | 4 | 63 |
| 8 | `is_middling` | {1, 2} | 4 | 74 |
| 9 | `is_odd` | {1, 3} | 3 | 520 |
| 10 | `is_big` | {2, 3} | 5 | 13 |
| 11 | `is_extreme` | {0, 3} | 5 | 12 |
| 12 | `is_not_huge` | {0, 1, 2} | 5 | 13 |
| 13 | `is_not_two` | {0, 1, 3} | 4 | 150 |
| 14 | `always_true` | {0, 1, 2, 3} | 4 | 104 |
| 15 | `is_not_one` | {0, 2, 3} | 6 | 1 |
| 16 | `is_not_tiny` | {1, 2, 3} | 5 | 15 |

A program's length is its instructions before its trailing `nada`s. Each puzzle says how short its best program is, and the lines past that are locked to `nada`. Past the lessons, every puzzle's last open line is fixed as `zero`, because an instruction after a program's last `zero` can only reject, as running off the end does, so every shortest program ends in `zero`. The last puzzle, `is_not_tiny`, also gives its `mod2` on line 2, the line where the solution folds 2 onto 0; the player still picks the `decr` on line 1.

### Help for a newcomer

The page's intro, behind a Help link after puzzle 1, says what a language is here, how the machine runs a program four times to find the language it recognizes, and works `decr, zero` through to {1}; the page's check holds that example to the machine.

## Arithmetization

The stepper writes one step of the machine as polynomials. Each bit of the state is a variable that is only ever 0 or 1, and logic becomes arithmetic:

- NOT x is 1 - x;
- x AND y is x·y;
- x OR y is x + y - x·y.

On 0/1 values each of these gives 0 or 1, and the right one. In Roc (`roc/Stepper.roc`) the first and third read as they are written:

{{function roc/Stepper.roc complement}}

{{function roc/Stepper.roc either}}

### One step

A step reads six bits: `AX`'s high and low bits (`hb`, `lb`), the flags `halted` and `accepted`, and the instruction's two bits (`op_hb`, `op_lb`). It writes four: the next `hb`, `lb`, `halted` and `accepted`.

One step of the machine
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  state [label="hb, lb, halted, accepted"];
  op [label="op_hb, op_lb"];
  step [label="step polynomials"];
  next [label="hb', lb', halted', accepted'"];
  state -> step;
  op -> step;
  step -> next;
}
```

The construction names each fact about the state with an indicator, a polynomial that is 1 when the fact holds: `AX` is 0, the instruction is `decr` and the machine is still running, and so on. The new state is what any of the four instructions would make of the old one:

{{function roc/Stepper.roc next_state}}

### The step's sizes

In Python's general polynomial ring, where x·x is x², the four polynomials come out large: hb' has 786 terms and lb' 2,979 (`README.md`, its layout's `src/stepper.c`). The C port prints them exactly as `stepper.py` does, held to `tests/golden/step_hb.txt` and its three siblings. In the Boolean ring of {{chapter Why composing blew up}} the same four have 7, 13, 17 and 33 terms, every coefficient 1 or -1, as an expect in `roc/Stepper.roc` holds.

Here is `halted'` in that ring. It says: halted already, or a `decr` at `AX` = 0 on a running machine.

```text
halted+op_hb+(-1)*hb*op_hb+(-1)*lb*op_hb+(-1)*halted*op_hb+(-1)*op_hb*op_lb
+hb*lb*op_hb+hb*halted*op_hb+hb*op_hb*op_lb+lb*halted*op_hb+lb*op_hb*op_lb
+halted*op_hb*op_lb+(-1)*hb*lb*halted*op_hb+(-1)*hb*lb*op_hb*op_lb
+(-1)*hb*halted*op_hb*op_lb+(-1)*lb*halted*op_hb*op_lb+hb*lb*halted*op_hb*op_lb
```

### The stepper as a second machine

Evaluated at the bits of a state and an instruction, the four polynomials give the next state's bits. So the stepper is a second implementation of the machine, written as algebra. `find_solutions` runs each example program on the stepper too, as the Python does, and `tests/test_stepper.c` holds the stepper to the interpreter on all 4,096 programs and all 4 inputs (`0921ec4`), where the Python checks the 16 examples.

## Why composing blew up

A whole program is six steps. Composing them means substituting: step 1's four polynomials go in for the state variables of step 2, and so on, until the final state is four polynomials in 14 variables, the input's two bits and each instruction's two. One polynomial would then hold the whole machine.

### The general ring

In the general ring each substitution multiplies polynomials together, and every product raises powers: x·x is x², x²·x is x³. Terms of a state that will only ever see 0 and 1 are kept apart because their exponents differ, and products of sums of them multiply their coefficients. The Python harness measured it: step 1 in about 0.9 s, and step 2 past its 30 s cap ([1a5f6fa](https://github.com/showell/virtual-machine/commit/1a5f6fa)'s message). The general polynomial in this repository's C, `src/poly.c`, checks every coefficient's add and multiply in `src/ring_int.c` and stops the program on an int64 overflow, since Python's integers grow as large as they need and C's stop at 64 bits.

### The Boolean ring

The fix is one identity. Every variable here is only ever 0 or 1, and on those values x² = x. A ring where every x² is x is a Boolean ring: every polynomial in it is multilinear, each variable to the first power at most. A monomial is then a set of variables, kept as a bit mask, and the product of two monomials is the union of their sets, the OR of their masks.

`roc/BoolPoly.roc` is that ring. Its product, with like terms summed in a table indexed by mask:

{{function roc/BoolPoly.roc times}}

`src/boolring.c` is the C's version (`0c233e3`), with a 32-bit mask and checked int64 coefficients.

### Why it is exact

The Boolean ring keeps every value the machine can see. Two polynomials that agree on every 0/1 point reduce to the same multilinear polynomial, and a multilinear polynomial is fixed by its values on {0, 1}^n. So reducing by x² = x keeps every value the machine can see, and the canonical form makes `==` equality of functions (`roc/BoolPoly.roc`'s header).

With the identity in place the step polynomials shrink from 786 and 2,979 terms to 7 and 13, and the composition of {{chapter Six steps composed}} takes a fraction of a second.

## Six steps composed

`src/compose.c` substitutes the step polynomials into themselves six times (`1146d43`). The Roc port's step, from `roc/Compose.roc`:

{{function roc/Compose.roc after_step}}

Six steps, one substitution each
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  s0 [label="input"];
  s1 [label="step 1"];
  s2 [label="step 2"];
  s3 [label="..."];
  s6 [label="step 6"];
  s0 -> s1 [label="op1"];
  s1 -> s2 [label="op2"];
  s2 -> s3;
  s3 -> s6 [label="op6"];
}
```

### The sizes

Each step's term counts, from `roc/golden/composition.txt` and `out/tcc/compose_main` run for this book:

| step | hb | lb | halted | accepted |
|---|---|---|---|---|
| 1 | 4 | 7 | 8 | 8 |
| 2 | 11 | 25 | 30 | 40 |
| 3 | 27 | 79 | 92 | 162 |
| 4 | 63 | 241 | 282 | 642 |
| 5 | 143 | 727 | 878 | 2,568 |
| 6 | 319 | 2,185 | 2,732 | 10,324 |

The final `hb` has degree 9 and its largest coefficient is 6; `lb` has degree 14 and 21; `halted` 14 and 20; `accepted` 14 and 63. Sixty-three is the largest coefficient of the four final polynomials, so int64 holds each with room to spare. Built with `gcc -O2` for this book, `compose_main` composed all six in 233 ms, 199 of them in step 6, at a peak of 4,224 KiB resident.

### The proof

Fourteen 0/1 variables make 2^14 = 16,384 points, and each point is one input with one program. `tests/test_compose.c` evaluates the four final polynomials at every point and holds them to the machine. The Roc port's proof prints the same verdict (`roc/golden/composition.txt`):

```text
proof: 16384 points, 0 differ, 5596 accepted, 2891 accepted with no halt
```

The last count is the Python's own reading of `halted`, kept as it is: a `zero` that accepts leaves the stepper's `halted` at 0, so 2,891 accepting runs end with it unset. The verdict is `accepted`, and it agrees with the machine at every point.

The proof is exhaustive: it checks every input with every program. And since a multilinear polynomial is fixed by its values on {0, 1}^14, any implementation whose four polynomials pass it has exactly these four.

## Cook-Levin as SAT

`src/sat.c` writes each puzzle as a Boolean formula in DIMACS CNF, the input format of SAT solvers (`8de51b7`). This is the Cook-Levin construction itself: the question "which six-instruction programs accept exactly this language?" becomes one formula whose satisfying assignments are exactly those programs.

### The unrolling

The formula's first 12 variables are the program's bits, two an instruction, so a solver's model reads straight off as a program. The machine is then unrolled: each of the six steps, on each of the four inputs, is a copy of the step's logic, wired to the state the step before left. The logic is the stepper's, gate for gate, NOT, AND and OR as in {{chapter Arithmetization}}.

Each AND and OR becomes a Tseitin variable, tied to its inputs by three clauses, and a NOT is a literal's sign:

{{lines src/sat.c from "Lit sat_and(Cnf *c, Lit x, Lit y) {" to "Lit c3[3] = {out, -a, -b};"}}

A gate on a constant is folded, and a gate already built from the same inputs is reused, so every puzzle's formula has 416 variables and 1,216 clauses. `out/tcc/sat facts`, run for this book, prints those two numbers for all 16. The last four clauses ask that each input's final `accepted` be what the language wants. `len=N` forces the lines past N to `nada`, and `fix=K:OP` fixes line K, as the puzzle page does.

### The proof by propagation

`tests/test_sat.c` proves the encoding by itself. For every puzzle it sets the program bits to each of the 4,096 programs and unit-propagates them through the formula. Each gate's inputs come before it, so propagation alone decides every Tseitin variable. The assignments that survive are exactly the programs the machine says solve the puzzle, 4,096 over the 16 languages. With lines past N forced to `nada`, the formula is satisfiable exactly when N is at least the puzzle's shortest length.

### rocsat as an oracle

`sat/oracle.shale` hands the formulas to [rocsat](https://github.com/mpizenberg/rocsat), Matt Pizenberg's CDCL SAT solver, written in Roc (`f977bcc`). For each puzzle it solves the formula and runs the model's program on the machine. It counts the solutions by adding a clause that blocks each model and solving again until rocsat says UNSAT, and holds that count to the machine's. Then it proves the shortest length: one line shorter is UNSAT, and the shortest length itself is SAT.

An UNSAT answer claims that the formula is unsatisfiable, and rocsat backs each one with a DRAT proof, a list of clause additions and deletions that a separate checker can replay. Here an UNSAT at length N - 1 says that every program of N - 1 lines fails the puzzle, and its proof is written to `out/sat/`. The proofs are written and left unchecked by the oracle (`README.md`, "SAT"). The oracle runs about 4,150 solves in about a minute, so it stands outside `build.shale`'s tiers; it was left unrun for this book.

## The oracles

Every implementation in this repository computes the same answers in its own way, and each is held to the others. The spine of the project is that they all agree.

Six ways to the same answers
```dot
digraph {
  node [shape=box, style=rounded];
  answers [label="16 counts, 4 polynomials"];
  python [label="Python"];
  c [label="C"];
  roc [label="Roc"];
  sat [label="SAT, rocsat"];
  js [label="JavaScript x3"];
  nibble [label="Nibble's wasm"];
  python -> answers;
  c -> answers;
  roc -> answers;
  sat -> answers;
  js -> answers;
  nibble -> answers;
}
```

### What each one checks

- **Python**, the original, is the source of `tests/golden/find_solutions.txt` and of the step polynomials' strings, `tests/golden/step_*.txt`.
- **C** prints that report byte for byte (`tests/test_golden.c`), checks the stepper against the interpreter on all 4,096 × 4 runs, and proves the composition at all 2^14 points (`tests/test_compose.c`). `shale build.shale` ran its fast tier for this book: ten test files and the golden check, all passing.
- **Roc** prints the same report, the four step polynomials as the C's `bp_str` prints them (`roc/golden/step_polynomials.txt`) and the composition's counts and proof (`roc/golden/composition.txt`). `roc/check.shale` is its gate.
- **SAT** reaches the 16 counts through `tests/test_sat.c`'s propagation and again through rocsat's blocked models, and each shortest length through an UNSAT and a SAT.
- **JavaScript**, `js/cook_levin.js`, prints the report and then the composition, byte for byte the two goldens, under three runners.
- **Nibble's wasm** is `src/vm.c` itself, and `web/page_check.mjs` holds it to the C on all 4,096 × 4 runs and to the 16-puzzle table.

### One program, three runners

`js/oracle.shale` runs the JavaScript port three ways (`c251895`), each held to the goldens. The runners are node, and two of [roc-js-interpreter](https://github.com/showell/roc-js-interpreter)'s, both written in Roc: its bytecode VM and its compiler to x86-64. The port is written in the subset of JavaScript that roc-js-interpreter runs, which leaves out the bitwise operators, so the OR and AND of two masks come from tables of their 7-bit halves.

By `README.md`'s timings on this box, 2026-11-25: node takes about 0.3 s, the VM about 80 s, in about 15 million of its steps, and the compiled executable, 111,698 bytes, 0.6 to 1.0 s, at a peak of 33 MB. The compiled program is slower than node here, and that is unprofiled. These runs were left unrepeated for this book.

### Why so many

Each pair shares only the machine's rule. The interpreter branches on opcodes, the stepper evaluates polynomials, the composition substitutes them, the SAT encoding propagates clauses, and rocsat searches. A bug would have to land the same way in all of them to pass.

## The Roc port

`roc/` is the same machine and the same mathematics in Roc, at the compiler that `roc/roc-version` pins (`6206236`). Its point is that the mathematics reads like mathematics.

### The ring as operators

`BoolPoly` is an opaque type over a canonical list of terms, and its `plus`, `minus`, `times` and `negate` are what Roc's `+`, `-`, `*` and unary `-` call. So OR is written `x + y - x * y`, as the algebra writes it. Roc's I64 arithmetic crashes on overflow, so every coefficient is exact or the program stops.

### Substitution

A substitution replaces each variable by a polynomial. The product of the replacements over each monomial is made once, from the product of its monomial less its highest variable, which other terms often share:

{{function roc/BoolPoly.roc substitute}}

### Every point at once

The proof wants each final polynomial's value at all 16,384 points. `values` gets them all at once by the zeta transform: each coefficient is summed into every point above its mask, one variable at a time, so the cost is 2^14 × 14 additions, whatever the terms:

{{function roc/BoolPoly.roc values}}

### The speed, honestly

On this box, the release build of `composition 6` composes all six steps and runs the proof in about 30 ms, and the C's `compose_main` at -O2, which only composes, takes about 260 ms (`README.md`, "The Roc port"). The gap is the data structure, and the language has little to do with it: the C sums like terms in a hash table it grows and then sorts, while the Roc sums them in a dense table indexed by mask, a straight array write that reads back already in order. The C could use the same table, and it has yet to be tried (`9e0c86c`).

## What comes next

A few steps stand open, each small:

- **The DRAT proofs** are written to `out/sat/` and left unchecked; a checker such as drat-trim would close that loop.
- **The C's dense table.** The C composition could sum like terms in the Roc port's dense table, and its time would then measure the language.
- **The compiled JavaScript.** Its slowness is unprofiled. `README.md` guesses that an array's items are values of unknown kind to the compiler, so each read and write passes the runtime's tag checks.
- **Nondeterminism.** The machine is deterministic. The SAT encoding already treats the program as the unknown, which is the theorem's "a certificate exists"; a machine with choice instructions would make the riff closer to the theorem.
