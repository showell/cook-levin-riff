# A riff on Cook-Levin

A small book about [cook-levin-riff](https://github.com/showell/cook-levin-riff): a toy machine whose every program is a polynomial and a Boolean formula, the six-step composition that took three years to arrive, and seven independent ways of computing the same answers that all agree.

## Introduction

Steve Howell devised a virtual machine with a single two-bit register, `AX`, and four instructions: `nada`, `zero`, `decr` and `mod2`. Every program is exactly six of those instructions. A run starts with `AX` holding the input, a number from 0 to 3, and ends in one of three ways:

- **`zero` accepts** the input, the moment it finds `AX` at 00;
- **`decr` rejects** it, the moment it finds `AX` at 00 (otherwise it takes 1 from `AX`);
- the run **rejects implicitly** when it gets past the sixth instruction with neither of them having seen 00.

`nada` leaves `AX` as it is, and `mod2` takes 2 from `AX` when it is 2 or 3. So each program sorts the four inputs into accepted and rejected, and the accepted ones are the set the program decides. Complexity theory calls such a set a *language* or a *decision problem*; this book calls each of the 16 subsets of {0, 1, 2, 3} a *problem*. The machine is small enough that every program, every input and every step can be listed, 4,096 programs in all, so every claim about it can be checked by running all of them.

Once Steve was confident in how the mundane machine worked, he used techniques inspired by the Cook-Levin proof to answer the same questions with polynomials. Each step of the machine becomes four polynomials in the state's bits and the instruction's bits, so that evaluating them at 0s and 1s gives the next state. Chained through all six steps, they become polynomials that answer "does this program accept this input?" for every program and every input at once.

Then Steve proved that the Cook-Levin machinery matches the toy. His Python's search runs each problem's example program twice, once on the interpreter and once through the step polynomials, and checks that the two give the same verdicts. Three pieces of his Python, quoted as they stand in [virtual-machine](https://github.com/showell/virtual-machine) at commit [94c3d31](https://github.com/showell/virtual-machine/commit/94c3d31), carry the whole idea. First, logic becomes arithmetic ([stepper.py, lines 8 to 17](https://github.com/showell/virtual-machine/blob/94c3d31/stepper.py#L8-L17)):

```python
def NOT(x):
    return 1 - x


def AND(x, y):
    return x * y


def OR(x, y):
    return (x + y) - (x * y)
```

Second, a program runs through the step polynomials instead of the interpreter, each step evaluating them at the state's 0s and 1s ([stepper.py, lines 147 to 158](https://github.com/showell/virtual-machine/blob/94c3d31/stepper.py#L147-L158)):

```python
def test_with_stepper(program, ax):
    AX = ax
    halted = False
    accepted = False
    for cmd in program:
        (AX, halted, accepted) = step(
            AX=AX,
            halted=halted,
            accepted=accepted,
            op=OPS[cmd],
        )
    return accepted
```

Third, the assertions, in `find_solutions` ([virtual_machine.py, lines 159 to 160 and 178 to 180](https://github.com/showell/virtual-machine/blob/94c3d31/virtual_machine.py#L159-L180)). After running all 4,096 programs on the interpreter, every one of the 16 problems must have a program, and each problem's example program, run through the polynomials on each input, must accept exactly the problem's inputs:

```python
    for y in range(16):
        assert len(solutions[y]) > 0
```

```python
        for i in range(4):
            accepted = stepper.test_with_stepper(example_program, i)
            assert accepted == (i in lang)
```

**That agreement is the thesis of this book: the machine and its Cook-Levin translation answer every question the same way.** Everything after it widens the check. The C runs both on all 4,096 programs and all four inputs; the six steps are chained into four polynomials that agree with the machine on all 2^14 points; a SAT solver, given the same machine as a formula, finds exactly the machine's programs; and seven implementations in five languages compute the same answers. His Python stopped short of chaining all six steps, and this repository finishes the chain ({{chapter How it got here}}).

Steve wrote the first version by hand in Python, in [virtual-machine](https://github.com/showell/virtual-machine): the machine (`virtual_machine.py`), a general polynomial library (`poly.py`) and a stepper that writes one step of the machine as polynomials (`stepper.py`). In the cook-levin-riff repository, the code was written by AI coding agents (Claude), with human supervision, as its `README.md` says in its second line.

### What the book covers

The first part is the toy for its own sake: a finite machine, completely understood, every question settled by running everything, even in Python. It runs from {{chapter The machine}} and the theory of languages through plain Python, the search of all 4,096 programs, the puzzle page, and the machine as Boolean functions and as hardware, to {{chapter Some history of the approach}}, the order in which Steve built it.

The second part is Cook-Levin: {{chapter How it got here}}, {{chapter P, NP and Cook-Levin}}, {{chapter Six expressions, composed}}, then the step as polynomials, from {{chapter Arithmetization}} to {{chapter The polynomials, in full}}, and the circuit as a tableau and a formula, from {{chapter The Cook-Levin machine}} to {{chapter The solver's work, in full}}.

The third part is the cross-checks: {{chapter The oracles}}, {{chapter The Roc port}}, {{chapter What is Shale?}} and {{chapter The VM in Shale}}, then {{chapter What comes next}}. The theorems are numbered through the book, each proved by an exhaustive run that its proof names.

### Why the name fits

The Cook-Levin theorem says that satisfiability of Boolean formulas is NP-complete: any problem whose answers a machine can check in polynomial time reduces to it. The proof unrolls the checking machine, step by step, into one formula. This repository does that unrolling on a toy instrument. It makes no claim about NP, and it reduces one machine's questions, never an arbitrary verifier's. It also goes one step past the formula, into arithmetic: the whole program becomes four polynomials with integer coefficients. "A riff" is the honest word for that.

### Where the numbers come from

Each number here comes from the repository: a commit, cited by its short hash, the `README.md`, a golden file, or a program run while the book was written, named where it is used. Code is quoted from the repository as it stands when the book is built. The Python repository's history is cited by links to its commits.

## The machine

The machine holds one register, `AX`, of 2 bits, so `AX` is 0, 1, 2 or 3. A run starts with `AX` set to the input. Its program is six instructions, each one of four opcodes, and the machine's assembly language is called Nibble.

### Steve's spec

The toy's plain-English spec is the docstring at the top of Steve's `virtual_machine.py` ([lines 10 to 35](https://github.com/showell/virtual-machine/blob/94c3d31/virtual_machine.py#L10-L35), at commit [94c3d31](https://github.com/showell/virtual-machine/commit/94c3d31)), quoted as it stands:

```text
    Every program is a sequence of these opcodes (aka instructions):

        00 (nada):
            (do nothing)

        01 (zero):
            AX = 3 -> ignore and continue
            AX = 2 -> ignore and continue
            AX = 1 -> ignore and continue
            AX = 0 -> halt and accept input

        10 (decr):
            AX = 3 -> AX = 2 and continue
            AX = 2 -> AX = 1 and continue
            AX = 1 -> AX = 0 and continue
            AX = 0 -> halt and reject input

        11 (mod2):
            AX = 3 -> AX = 1
            AX = 2 -> AX = 0
            AX = 1 -> AX = 1
            AX = 0 -> AX = 0

    Every program must include exactly six instructions.  If at the end
    of that program you have not accepted the input, then the input is
    implicitly rejected.
```

Every behaviour of the machine is in those 26 lines. The rest of this part only runs them.

### Four opcodes

`src/vm.c` holds the rule, as `run_progam` and its one-step helper:

{{lines src/vm.c from "Machine vm_step(Machine m, Op op) {" to "m.ax = m.ax % 2;"}}

- **`nada`** leaves the state as it is.
- **`zero`** accepts when `AX` is 0, and the run ends; at any other value the run goes on.
- **`decr`** takes 1 from `AX`, and rejects when `AX` is already 0.
- **`mod2`** takes 2 from `AX` when it is 2 or 3, leaving its last bit.

A run that reaches the end of its six instructions rejects. So a program accepts some of the four inputs and rejects the rest, and the set it accepts is the problem it decides.

### One program, input by input

Here is a program that decides {3}, the problem the puzzle page calls `is_huge`. In an ordinary programming language it is `n == 3`. In Nibble it is program 2704:

```text
decr
decr
decr
zero
nada
nada
```

Read it once for each input:

- **From 0**, line 1's `decr` rejects.
- **From 1**, line 1 takes `AX` to 0, and line 2 rejects.
- **From 2**, lines 1 and 2 take `AX` to 0, and line 3 rejects.
- **From 3**, three `decr`s take `AX` to 0, and line 4's `zero` accepts.

So the program accepts {3} and rejects the rest. Every input's run ends by line 4, so lines 5 and 6 could hold any opcodes. That is one reason most problems have many programs.

Steve's own view of `nada` and of the design, in his words: "nada is kinda not a real instruction under my machine. It just makes it so that every program is exactly 12 bits. It's also worth saying that the language was designed to have some complexity. It's using an imperfect machine to solve the problem somewhat by design." {{chapter Which instructions are needed}} makes both points exact: `nada` is redundant (Theorem 6), and two instructions of another design would decide every problem in four lines (Theorem 7).

### Counting

Each instruction is one of 4 opcodes, so there are 4^6 = 4,096 programs. Each problem is a subset of {0, 1, 2, 3}, so there are 2^4 = 16 problems. Each program is a 12-bit number, two bits an instruction, and `assemble` and `disassemble` in `src/vm.c` convert between the two forms.

### Program numbers

This book names a program by its number. Read the six lines as the digits of a base-4 number, line 1 first, with `nada` 0, `zero` 1, `decr` 2 and `mod2` 3: the number is op₁·4⁵ + op₂·4⁴ + op₃·4³ + op₄·4² + op₅·4 + op₆. So program 0 (`nada, nada, nada, nada, nada, nada`) is the first, program 4095 (`mod2, mod2, mod2, mod2, mod2, mod2`) is the last, and the number's twelve bits, line 1's two first, are the program's machine code as the puzzle page shows it. A program of fewer than six lines ends in `nada`, so its number ends in base-4 zeros. Steve's Python, and the C that matches it byte for byte, count through the programs in the other order, line 1 as the lowest digit. That order only decides which program the search finds first; the numbers in this book are all of the kind defined here, and the build checks each against the program it names.

## Languages and machines

In computer science a *language* is a set of strings. Pick an alphabet, say the digits 0 to 9, and any set of strings over it is a language: the decimal numerals of the primes, the palindromes, the strings that are valid Roc programs. A language is the cleanest way to state a decision question. "Is 91 prime?" becomes "is the string `91` in the language of primes?"

### Machines that decide

A machine *decides* a language when, given any string, it halts, accepting the strings in the language and rejecting the rest. A program that decides the primes is a primality test; a compiler's parser decides the language of well-formed programs, and rejects the rest with an error.

### Turing's machine

In 1936 Alan Turing described a machine with a finite set of states and an unbounded tape, in "On Computable Numbers, with an Application to the Entscheidungsproblem" (*Proceedings of the London Mathematical Society*, series 2, volume 42; a [free scan](https://www.cs.virginia.edu/~robins/Turing_Paper_1936.pdf)). It reads and writes the tape one cell at a time, and every algorithm anyone has written since can be carried out by such a machine; that claim is the Church-Turing thesis. Turing also showed that some languages are undecidable: whatever machine one builds for them, some string defeats it. The best known is the halting problem, the language of programs that halt on their input. A machine can *recognize* it, accepting every member by running it, but for some strings outside the language it runs forever, still waiting to answer.

### A simple example

Take the alphabet {a, b} and the language of strings with an even count of a's: `aba` and `bb` are in it, `ab` and `aaab` are out. A Turing machine decides it with two working states, `even` and `odd`, and a head that reads the tape left to right. The input is written on the tape, with blank cells after it, and the machine starts in `even` on the first symbol.

| state | reads | writes | moves | next state |
|---|---|---|---|---|
| `even` | a | a | right | `odd` |
| `even` | b | b | right | `even` |
| `even` | blank | blank | stays | accept |
| `odd` | a | a | right | `even` |
| `odd` | b | b | right | `odd` |
| `odd` | blank | blank | stays | reject |

Each row is one rule: in this state, reading this symbol, write this, move, and go to that state. The state remembers the one fact that matters, whether the a's seen so far are even, and the blank after the input is where the machine answers.

A run on `aba`:

| step | state | reading | tape, head in brackets |
|---|---|---|---|
| 1 | `even` | a | [a] b a _ |
| 2 | `odd` | b | a [b] a _ |
| 3 | `odd` | a | a b [a] _ |
| 4 | `even` | blank | a b a [_] |
|   | accept | | |

And on `ab`: `even` reads a and moves to `odd`, `odd` reads b and stays `odd`, `odd` reads the blank and rejects. Every run takes one step per symbol and then halts, so the machine decides the language: it accepts exactly the strings with an even count of a's.

This machine only reads and moves right, so it is also a finite automaton; Turing's machines earn their tape on harder languages, such as the strings of a's followed by the same number of b's, where the machine writes marks on the tape to pair each a with a b. The idea carries over unchanged: states, a rule table, a run that ends in accept or reject, and a language made of the strings accepted.

### This book's machine

The machine here is Turing's idea at its smallest. Its tape is one 2-bit register, its inputs are the four strings 0, 1, 2 and 3, and every run halts within six steps, so every question about it is decidable by trying everything. Its languages are the 16 subsets of {0, 1, 2, 3}, the problems of the Introduction, which a program, a circuit and a SAT formula each decide alike. A program decides exactly one of them, the set of inputs it accepts. {{chapter Programs as languages}} asks which programs decide which problems, by running all of them. The second part of the book asks what deciding costs when trying everything is out of reach, the question Cook and Levin answered.

## Deciding in plain Python

A problem is four answers, one per input, so deciding one takes a line of ordinary code. The code here is `python/decides.py`, which `build.shale` runs on every build: it holds its two versions to each other on all 16 problems and all four inputs, and each one-line test below to its problem.

### Two versions of `decides`

A problem's *want* code has bit n set when input n is in it, as in the table of {{chapter Programs as languages}}: {0, 2, 3} is 1 + 4 + 8 = 13. The plain version lists the members and looks n up among them:

{{lines python/decides.py from "def decides(want, n):" to "    return n in members"}}

The golfed version reads bit n of `want` directly:

{{lines python/decides.py from "decides_golfed =" to "decides_golfed ="}}

### Some problems as one test

Many of the 16 problems have a natural test of n, or of its two bits, `hb` and `lb` (n is 2·`hb` + `lb`):

{{lines python/decides.py from "ONE_LINERS = {" to "hb XOR lb"}}

{{chapter The problems, seen from outside}} lists all 16 this way, as Boolean expressions of the two bits.

### What the machine adds

The want code already says everything about a problem. The Nibble machine takes up to six lines to say the same thing, and for {0, 2, 3} it takes all six, in its one program. The composed polynomial of {{chapter Six steps composed}}, which runs every program on every input at once, takes 10,324 terms. The machine is there to be the thing analysed, a small checker that Cook and Levin's construction can turn into a formula; the question it answers is a lookup.

## Programs as languages

Steve settled which programs decide which problems twice in Python: first by construction, then by running everything.

### The analytical solution, 2023

The first, `find_solutions_analytically`, came on the machine's first day, 2023-03-11, in [d6bb78f](https://github.com/showell/virtual-machine/commit/d6bb78f) ("Add analytical solution."), stamped 13:02, and the stepper followed at 15:47 ([ba9388a](https://github.com/showell/virtual-machine/commit/ba9388a)). The machine was smaller then. That commit has two opcodes, `check` (today's `zero`) and `decr`, programs of up to 8 instructions, and a brute-force search over the program numbers 0 to 255. Its `solve` ([virtual_machine.py, lines 185 to 195](https://github.com/showell/virtual-machine/blob/d6bb78f/virtual_machine.py#L185-L195)) builds a problem's program directly: `decr` down to the least member, `check`, and the same for the rest.

```python
    def solve(lang):
        if not lang:
            # The program can terminate to reject all further numbers
            return []

        program = []
        while lang[0] > 0:
            program.append("decr")
            lang = [i-1 for i in lang]

        return program + ["check"] + solve(lang[1:])
```

For each of the 16 problems it asserted the program shorter than 8 and ran it on all four inputs (`assert f(x) == y`). Later commits removed it; it is absent at [94c3d31](https://github.com/showell/virtual-machine/commit/94c3d31).

### The complete calculation

Today's `find_solutions` ([virtual_machine.py, lines 148 to 180](https://github.com/showell/virtual-machine/blob/94c3d31/virtual_machine.py#L148-L180)) runs every program on every input and groups the programs by the problem each decides. The runs are `run_progam`'s ([lines 76 to 105](https://github.com/showell/virtual-machine/blob/94c3d31/virtual_machine.py#L76-L105)), the plain interpreter: a loop over the six instructions, an `if`/`elif` on each opcode and on `AX`, the spec above as code. That is the complete calculation, 4,096 programs on 4 inputs, 16,384 runs. It also re-runs each problem's first program, 16 programs on 4 inputs, through the step polynomials of the second part (lines 178 to 180); the theorems here rest on the interpreter alone.

### The theorems

**Theorem 1.** Every one of the 16 problems is decided by some six-instruction program.

*Proof by construction.* On today's machine `solve` builds a program of six lines or fewer for 15 problems, and each decides its problem. For {0, 1, 2, 3} it builds seven, `zero, decr, zero, decr, zero, decr, zero`, one past the limit, and Python's report gives `mod2, zero, decr, zero` for it. `tests/test_theorems.c` runs the construction on every problem.

*Proof by exhaustion.* `find_solutions` runs all 4,096 programs on `run_progam` and asserts `len(solutions[y]) > 0` for every problem `y` (lines 159 to 160).

**Theorem 2.** With `nada`, `zero` and `decr` alone, exactly 15 of the 16 problems are decided by a six-instruction program; {0, 1, 2, 3} needs `mod2`.

*Proof.* `tests/test_theorems.c` runs the 3^6 = 729 programs over `nada`, `zero` and `decr` and finds 15 problems, {0, 1, 2, 3} the one missing.

**Theorem 3.** The number of programs deciding each problem is the table's programs column, and the 16 counts sum to 4,096.

*Proof.* Each program is filed under the one problem it decides, so the counts partition the 4,096 programs. Python's report gives each count, and `tests/golden/find_solutions.txt` holds that report.

In the table, a problem's code is its inputs as bits: input i accepted sets bit i. A program's length is its instructions before its trailing `nada`s, and the shortest column is the least length among a problem's programs.

| code | problem | programs | shortest |
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

**Theorem 4.** Each problem's shortest program has the length in the table's shortest column.

*Proof.* `tests/test_theorems.c` measures the length of all 4,096 programs, takes each problem's least, and holds the 16 to the table. The second part proves it again a different way: a SAT solver shows each length one line shorter impossible, with a proof a checker replays ({{chapter The solver's work, in full}}).

### The rare one

**Theorem 5.** {0, 2, 3} is decided by exactly one program, and that program uses all six lines. It is program 1689:

```text
zero
decr
decr
zero
decr
zero
```

*Proof.* Python's report gives {0, 2, 3} a count of 1 and this program as its example, and Theorem 3's search is exhaustive. `solve` builds the same program.

Input 0 is accepted on line 1. Input 1 drops to 0 on line 2 and is rejected by the `decr` on line 3. Input 2 reaches 0 on line 3 and is accepted on line 4. Input 3 reaches 0 on line 5 and is accepted on line 6. The problem skips 1, and the only way past it is to count down past it.

### The common ones

The empty problem, { }, has 844 programs, since any program that rejects every input will do, and {0, 2} has 883: `mod2` then `zero` is two lines, and many longer programs reach the same verdicts.

## Which instructions are needed

{{chapter Programs as languages}} counted what the four opcodes decide together. This chapter takes them apart: which of the four are needed, and what instructions of other designs could do.

### The four opcodes

**Theorem 6.** The six-line programs over `zero`, `decr` and `mod2` decide all 16 problems, and each of the three is needed: dropping `zero` leaves only { } decided, dropping `decr` leaves 3 problems, and dropping `mod2` leaves 15 (Theorem 2). `nada` is redundant: the programs over the other three decide all 16. And every shortest program is made of `zero`, `decr` and `mod2` alone.

*Proof.* `tests/test_theorems.c` runs the 4,096 programs, keeps those whose every instruction is in a given set, and counts the problems they decide, for all four opcodes and for each set of three. Only `zero` accepts, so dropping it leaves every input rejected. Dropping `decr` leaves { }, {0} and {0, 2}: `mod2` brings 2 to 0, and 1 and 3 to 1, where they stay. For the last part, take a `nada` out of a program and close the gap: every input meets the same lines in the same order, so the shorter program decides the same problem. So every line of a shortest program is `zero`, `decr` or `mod2`, and the same test checks each one.

### What it means for a player

The last part of Theorem 6 is a lemma a player can use. A puzzle with three open lines offers 4^3 = 64 programs to try, but once `nada` is set aside it offers 3^3 = 27. A player who clicks through those 27, knowing only the buttons, will find the answer. That is the toy's version of the question {{chapter P, NP and Cook-Levin}} asks: whether finding an answer can ever be easier than trying the possibilities.

### Instructions of any kind

The four opcodes are one choice among many. An instruction of any kind says, for each value of `AX`, what happens there: a new value, or accept, or reject. That makes 6^4 = 1,296 instructions, and the toy's four are among them.

**Theorem 7.** Two instructions of that kind decide all 16 problems in four lines: `decr`, and its twin that accepts at 0 and otherwise takes 1 from `AX`. One instruction, repeated, decides at most 5 problems at any length.

*Proof.* For a problem L, line i + 1 is the twin when i is in L and `decr` otherwise, for i from 0 to 3. Input n reaches 0 on line n + 1, and that line's verdict is L's. `tests/test_theorems.c` builds the 16 programs and runs them. For one instruction it runs all 1,296, each repeated from 0 to 1,296 times on all four inputs, and finds at most 5 problems for each, and 5 for some. Five is also the bound by argument: an accept is for good, so each length's problem contains the one before it, and a chain of subsets of four inputs has at most five members.

## Nibble, the puzzle page

[Nibble](https://roc.lynrummy.com/nibble/) is a puzzle page with one puzzle for each of the 16 problems: pick instructions until the program accepts exactly the puzzle's inputs and rejects the rest. The search of {{chapter Programs as languages}} says every puzzle has an answer, and how many programs are answers.

### What runs in the page

The page runs the Roc port's machine. `roc/NibbleWasm.roc` exports three functions built on `roc/Machine.roc`, compiled to a 9.5 KB wasm module by the pinned Roc with a small Zig host (`web/platform/`), and the puzzle table is printed by the C's `web/puzzles.c`, which files every program under the problem it decides and finds each puzzle's shortest program (`README.md`, "Playground"). The JavaScript in the page draws; the machine's verdicts are Roc's. The page's check, `web/page_check.mjs`, holds the Roc wasm to the C on all 4,096 programs and all four inputs, so every build of the page is also a cross-check of the two ports, then plays every puzzle in headless Chromium by clicks.

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

| # | puzzle | problem | shortest | programs |
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

A program's length is its instructions before its trailing `nada`s. Each puzzle says how short its best program is, and the lines past that are locked to `nada`. Past the lessons, every puzzle's last open line is fixed as `zero`, by this theorem:

**Theorem 8.** Every shortest program of a line or more ends in `zero`.

*Proof.* An instruction after a program's last `zero` can only reject, as running off the end does, so a program whose last instruction is `decr` or `mod2` decides the same problem with that instruction cut, in fewer lines. `tests/test_theorems.c` checks it on all 4,096 programs: every shortest program of a line or more ends in `zero`, and longer programs that end otherwise exist.

The last puzzle, `is_not_tiny`, also gives its `mod2` on line 2, the line where the solution folds 2 onto 0; the player still picks the `decr` on line 1.

### Help for a newcomer

The page's intro, behind a Help link after puzzle 1, says what a language is here (the page's word for a problem), how the machine runs a program four times to find the language it recognizes, and works `decr, zero` through to {1}; the page's check holds that example to the machine.

### The sandbox

A second page, the [sandbox](https://roc.lynrummy.com/nibble/sandbox.html), drops the puzzles. All six lines are open, each starting as `nada`, and the page shows the trace on the four inputs and the problem the program decides, by its set and its name. A program can also be typed in by its number, from 0 to 4095. It runs on the same machine as the puzzles, and the page's check holds program 1689 to {0, 2, 3} there too.

## The Boolean view

Everything so far ran the machine as a program, an interpreter reading its lines. This chapter runs the same machine as a computer's hardware does: a clock, and at each tick, one step of the machine, a fixed bank of Boolean gates, AND, OR and the complement, that takes bits in and puts bits out. The gates are the same at every tick, for every program and every input. They read the line's two opcode bits and the four state bits, and they write four state bits. They see bits and only bits: what a program means, which of the 16 problems it decides, even that problems exist, all lies outside them.

### One step, four functions

A step reads six bits: the line's opcode, `op_hb` and `op_lb`, and the machine's state, the two bits of `AX`, `hb` and `lb`, and the flags `halted` and `accepted`. It writes the four state bits again, and each new bit is a Boolean function of its own of those six. The gates know bits, and the machine is defined by its opcodes, so a step first decodes: the line's two bits name one of the four opcodes, and `AX`'s two bits name its value. Here it is in Python (`python/steps.py`), where `~`, `&` and `|` are the complement, AND and OR:

{{lines python/steps.py from "def opcodes(op_hb, op_lb):" to "    return ax_is_0, ax_is_2, ax_is_3"}}

Each `is_` wire is 1 for exactly one opcode, and each `ax_is_` wire for exactly one value. Then each new bit says, opcode by opcode, what happens to it:

{{lines python/steps.py from "def next_hb(" to "    return accepted | (~halted & (is_zero & ax_is_0))"}}

These are the machine's rules, one bit at a time. `nada` and `zero` leave `AX` alone. `decr` takes 3 to 2, 2 to 1 and 1 to 0, so the new high bit is 1 only when it starts at 3, and the new low bit only when it starts at 2; at 0 it halts. `mod2` keeps the low bit and clears the high one. `zero` at 0 accepts. A halted machine keeps every bit as it is, and once a flag is on it stays on.

### Six steps, everything fixed

Now fix the input and the program. With `AX` = 2 and program 2916 (`decr, mod2, zero, decr, zero, nada`), each of the six bits a step reads is a known 0 or 1, and so is each bit it writes. Composing the six steps is running the machine, one row a step:

```text
                hb lb halted accepted
start            1  0     0        0
step 1  decr 10   0  1     0        0
step 2  mod2 11   0  1     0        0
step 3  zero 01   0  1     0        0
step 4  decr 10   0  0     0        0
step 5  zero 01   0  0     0        1
step 6  nada 00   0  0     0        1
```

The last row's `accepted` is 1: the program accepts 2. `python/steps.py` runs each of the 4,096 programs on all four inputs this way and holds every answer to an ordinary interpreter written beside it.

### Four inputs, one verdict

Whether a program decides a problem is more gates of the same kind. Run the six ticks once for each input, four copies side by side, and each copy leaves one `accepted` bit. The problem enters only here, as four more bits, the *want* bits: input n's bit is 1 when n is in the problem. Each `accepted` bit is compared with its want bit, a gate that gives 1 when the two agree, and one AND of the four comparisons is the verdict. For the program above and the problem {1, 2, 3}, the four `accepted` bits are 0, 1, 1, 1, the want bits are 0, 1, 1, 1, and the verdict is 1. The gates matched bits; what {1, 2, 3} is stayed outside them.

{{chapter Six expressions, composed}}, in the second part, keeps the same gates and the same six ticks, and leaves the program's bits unknown.

## The problems, seen from outside

{{chapter The Boolean view}} ran the machine as gates that see only bits. We can see more than the gates do. Read an input n as its two bits, n = 2·`hb` + `lb`, and each problem is itself a Boolean function of them: {3} is `hb AND lb`, {1, 2, 3} is `hb OR lb`, {1, 2} is `hb XOR lb`, and {0, 2, 3} is `hb OR NOT lb`. That is a fact about the problems, found by looking at all 16 at once. The machine reaches each answer the long way instead, six ticks of the same gates.

**Theorem 9.** The 16 problems are exactly the 16 Boolean functions of two bits: each problem is one function of `hb` and `lb`, and each function is one problem.

*Proof.* A function of two bits is its four values, one per input, and a problem is the same four answers, so the two are counted alike, 2^4 = 16. `tests/test_theorems.c` evaluates the 16 expressions below at all four inputs, finds each one true at its problem's members alone and the 16 truth tables all different, and finds each problem decided by some program. `src/boolean_main.c` holds each expression to a program run on the machine and prints the table, and the book's build holds the table to that output.

| code | problem | as hb and lb |
|---|---|---|
| 0 | { } | `0` |
| 1 | {0} | `NOT (hb OR lb)` |
| 2 | {1} | `NOT hb AND lb` |
| 3 | {0, 1} | `NOT hb` |
| 4 | {2} | `hb AND NOT lb` |
| 5 | {0, 2} | `NOT lb` |
| 6 | {1, 2} | `hb XOR lb` |
| 7 | {0, 1, 2} | `NOT (hb AND lb)` |
| 8 | {3} | `hb AND lb` |
| 9 | {0, 3} | `NOT (hb XOR lb)` |
| 10 | {1, 3} | `lb` |
| 11 | {0, 1, 3} | `NOT hb OR lb` |
| 12 | {2, 3} | `hb` |
| 13 | {0, 2, 3} | `hb OR NOT lb` |
| 14 | {1, 2, 3} | `hb OR lb` |
| 15 | {0, 1, 2, 3} | `1` |

So the 16 problems, the 16 puzzles of Nibble, are the 16 two-input connectives.

## One formula for the whole machine

{{chapter The Boolean view}} ran the machine as gates on known bits. Its four step functions never ask what their bits are, so they can be handed something else: formulas. The machine is then one function of 14 bits, f, the input's two and the program's twelve, true where the program accepts the input, and running the step functions on formulas writes f down. The Roc port states f directly, in `roc/Fourteen.roc`:

{{function roc/Fourteen.roc f}}

### Formulas as values

`python/steps.py` keeps a formula as a graph of nodes, each a variable, a constant, or a complement, AND or OR of nodes before it. Its operators are the step functions' `~`, `&` and `|`. They fold the plain cases away (a constant, a doubled complement, x AND x, and x OR (¬x AND y), which is x OR y), and a node already built is reused, so equal parts of a formula are one node:

{{lines python/steps.py from "class F:" to "            return self | other.args[1]"}}

### Six steps, every bit unknown

Name the 14 bits v1 to v14: v1 and v2 the input's, high then low, and v3 to v14 the program's, two a line, line 1 first, so that v3 to v14 read as the program's number. Start from the input's two unknowns with both flags 0, and run the six steps, each on its line's two unknowns (`python/fourteen_cnf.py`):

{{lines python/fourteen_cnf.py from "def formula():" to "    return state[3]"}}

What comes back is f. After line 1 the four state bits are short enough to read:

```text
hb'       = ((NOT v3 AND NOT v4 AND v1) OR (NOT v3 AND v4 AND v1) OR (v3 AND NOT v4 AND v1 AND v2))
lb'       = ((NOT v3 AND NOT v4 AND v2) OR (NOT v3 AND v4 AND v2) OR (v3 AND v4 AND v2) OR (v3 AND NOT v4 AND v1 AND NOT v2))
halted'   = (v3 AND NOT v4 AND NOT v1 AND NOT v2)
accepted' = (NOT v3 AND v4 AND NOT v1 AND NOT v2)
```

`accepted'` is line 1 being `zero` (`NOT v3 AND v4`) on an `AX` of 0 (`NOT v1 AND NOT v2`). `hb'` is the high bit kept by `nada`, kept by `zero`, or kept by a `decr` from 3; `mod2` clears it. After line 2, `accepted` has the line-1 bits pasted in wherever it uses them:

```text
((NOT v3 AND v4 AND NOT v1 AND NOT v2) OR (NOT (v3 AND NOT v4 AND NOT v1 AND NOT v2) AND NOT v5 AND v6 AND NOT ((NOT v3 AND NOT v4 AND v1) OR (NOT v3 AND v4 AND v1) OR (v3 AND NOT v4 AND v1 AND v2)) AND NOT ((NOT v3 AND NOT v4 AND v2) OR (NOT v3 AND v4 AND v2) OR (v3 AND v4 AND v2) OR (v3 AND NOT v4 AND v1 AND NOT v2))))
```

It reads: line 1 accepted, or line 1 left the machine running, line 2 is `zero`, and `AX` after line 1 is 0.

### Graph and tree

Written out as text, each line's formula repeats the last line's state bits wherever it uses them, and the text grows about six-fold a line. Kept as a graph, the same formula grows by about 35 gates a line:

| after line | gates in the graph | written out, characters |
|---|---|---|
| 1 | 6 | 37 |
| 2 | 34 | 322 |
| 3 | 69 | 2,131 |
| 4 | 104 | 13,598 |
| 5 | 139 | 86,211 |
| 6 | 174 | 546,105 |

The last row is f: 174 gates, or more than half a million characters. {{chapter Checking a claim}} works with the graph.

## Checking a claim

A claim such as "program 1689 accepts input 2" can be checked by a tool that knows only formulas and clauses. This chapter turns f's graph into the format SAT tools read and hands the claim to the checker inside Matthieu Pizenberg's [rocsat](https://github.com/mpizenberg/rocsat).

### Naming each gate

SAT tools read CNF: a list of clauses, each an OR of literals, all of which must hold. Expanding f into CNF would start from its half-million characters. Tseitin's encoding names instead: each AND and OR node gets a variable of its own, and three clauses say that variable equals its gate. A complement is a literal's minus sign.

| gate | its three clauses | what they say |
|---|---|---|
| g = a AND b | (¬g ∨ a), (¬g ∨ b), (g ∨ ¬a ∨ ¬b) | g gives a, g gives b, a and b give g |
| g = a OR b | (g ∨ ¬a), (g ∨ ¬b), (¬g ∨ a ∨ b) | a gives g, b gives g, g gives a or b |

{{lines python/fourteen_cnf.py from "def clauses_of(out):" to "    return lit, gates, clauses"}}

The gates are numbered from v15 up, inputs before the gates that use them. The last clause holds f's own literal alone: it states f = 1. Its head, in DIMACS, the format the tools read:

```text
c f : {0,1}^14 -> {0,1}, the Nibble machine as one function
c v1 v2: the input's bits; v3 .. v14: the program's, line 1 first
c v15 .. v160: AND and OR gates; the last clause: f = 1
p cnf 160 439
-15 -3 0
-15 4 0
15 3 -4 0
-16 -1 0
-16 -2 0
16 1 2 0
-17 15 0
-17 16 0
17 -15 -16 0
-18 3 0
-18 -4 0
18 -3 4 0
-19 18 0
-19 16 0
19 -18 -16 0
```

Each group of three is one gate: v15 is `NOT v3 AND v4`, line 1 is `zero`; v16 is `NOT v1 AND NOT v2`, `AX` starts at 0; v17 is v15 AND v16, line 1 accepts; v18 is `v3 AND NOT v4`, line 1 is `decr`; v19 is v18 AND v16, line 1 halts. The formula has 160 variables and 439 clauses, three for each of the 146 gates and one for f = 1. It ends:

```text
160 -130 0
160 -159 0
-160 130 159 0
160 0
```

v160 is f, the OR of v130, accepted by line 5, and v159, line 6 accepting.

### A claim is an assignment

A claim gives every variable a value. The 14 bits come from the claim; each gate's value comes from running the gates on them:

{{lines python/fourteen_cnf.py from "def values_at(gates, n, program):" to "    return [env[i] for i in range(1, 15)] + [g.value(env, memo) for g in gates]"}}

For input 2 (`10`) and program 1689 (`01 10 10 01 10 01`), the 160 values, in the solvers' `v` format, a minus sign for false:

```text
v 1 -2 -3 4 5 -6 7 -8 -9 10 11 -12 -13 14 15 -16 -17 -18 -19 -20 -21 -22 23 24 -25 -26 27 -28 -29 -30 -31 -32 -33 34 -35 -36 -37 -38 -39 -40 41 -42 -43 -44 -45 -46 -47 -48 -49 -50 -51 -52 -53 -54 -55 -56 -57 -58 -59 -60 -61 62 63 64 65 66 -67 -68 -69 -70 71 -72 -73 74 -75 -76 -77 -78 -79 -80 -81 -82 -83 -84 -85 -86 -87 -88 -89 -90 -91 -92 -93 -94 -95 -96 97 98 99 100 -101 -102 -103 -104 -105 -106 -107 -108 -109 -110 -111 -112 -113 -114 -115 -116 -117 -118 -119 -120 -121 -122 -123 -124 -125 -126 127 -128 -129 130 131 132 133 134 -135 -136 -137 -138 -139 -140 -141 -142 -143 -144 -145 -146 -147 -148 -149 -150 -151 -152 -153 -154 -155 -156 157 158 -159 160 0
```

### rocsat checks it

`rocsat check` reads a formula and an assignment and tests every clause, one by one, and that is all it does. `sat/fourteen.shale` hands it the claim for each of the four inputs, with program 1689:

```text
input 0, program 1689: exit 0: c model ok
input 1, program 1689: exit 1: c model falsifies clause 438
input 2, program 1689: exit 0: c model ok
input 3, program 1689: exit 0: c model ok
```

Clause 438, counted from 0, is the last of the 439, `160 0`: f = 1. So program 1689 accepts 0, 2 and 3 and rejects 1, the problem {0, 2, 3}, as Theorem 5 says.

The one complaint is a proof. The first 438 clauses say only that each gate equals its gate's AND or OR, so once the 14 bits are fixed, exactly one assignment satisfies them, the one the gates compute. At input 1 that assignment has v160 false, so every assignment that keeps the gates' clauses breaks f = 1. `python/fourteen_cnf.py`, run by `build.shale`, checks the same at all 16,384 points: the computed assignment keeps every gate's clauses, and it keeps f = 1 exactly where the machine accepts. The book's build holds the blocks here to its output, and rocsat's lines to `sat/fourteen/verdicts.txt`, which `sat/fourteen.shale` writes.

Every bit here was given; the check only computes. A solver that finds the bits when they are left unknown is where the second part, {{chapter Cook-Levin as SAT}}, takes the same encoding.

## Parts for a small computer

This chapter and the two after it build the machine as hardware, a plain digital circuit, from the bottom up. Three kinds of gate are allowed, AND, OR and NOT, and each part below is made of them alone. Each part is a literal transcription of one rule of the machine, as `vm_step` in `src/vm.c` states it. {{chapter The machine as a circuit}} turns the parts into named boxes and wires them into a stepper, and {{chapter The circuit, in full}} chains the steppers into the whole machine.

The parts are functions in `src/circuit.c`, which records every gate in a netlist with its name and its part's name. `src/circuit_main.c` draws each drawing here from that netlist and fills each table by running the netlist's simulator on every input the part can be given; the book's build holds both to its output, byte for byte, and `tests/test_circuit.c` checks the same answers.

### Two ways to carry a value

A value from 0 to 3 travels on wires in one of two ways. As *bits*, it is two wires, `hb` and `lb`, the way `AX` holds it. As *one-hot* wires, it is four wires, `is_0` to `is_3`, exactly one of them on: the wire for the value. The opcode's rules are easiest to state on one-hot wires, where each rule is a question of which wire goes where.

### The four opcode machines

Each opcode has a machine of its own. It reads `is_0` to `is_3` and gives four one-hot wires for the value `AX` becomes, `next_0` to `next_3`, and two flags, `accepts` and `halts`. A wire tied to 0 is drawn from a `0`.

- **`nada`** is wires alone: each value goes straight across.
- **`zero`** is wires too: each value goes straight across, and `is_0` is tapped to accept. A circuit runs every step, so after an accept the machine keeps stepping, and the accepted latch holds the verdict.
- **`decr`** is a *shifter*: each wire moves down one place, and `is_0` falls out as a halt.
- **`mod2`** folds 3 onto 1 and 2 onto 0, with two OR gates.

`nada`, wires alone
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w0; w2; w3; w4; w5; }
  w0 [shape=plaintext, label="0"];
  w2 [shape=plaintext, label="is_0"];
  w3 [shape=plaintext, label="is_1"];
  w4 [shape=plaintext, label="is_2"];
  w5 [shape=plaintext, label="is_3"];
  { rank=sink; p0; p1; p2; p3; p4; p5; }
  p0 [shape=plaintext, label="next_0"];
  p1 [shape=plaintext, label="next_1"];
  p2 [shape=plaintext, label="next_2"];
  p3 [shape=plaintext, label="next_3"];
  p4 [shape=plaintext, label="accepts"];
  p5 [shape=plaintext, label="halts"];
  w2 -> p0;
  w3 -> p1;
  w4 -> p2;
  w5 -> p3;
  w0 -> p4;
  w0 -> p5;
}
```

`zero`, wires and a tap
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w0; w2; w3; w4; w5; }
  w0 [shape=plaintext, label="0"];
  w2 [shape=plaintext, label="is_0"];
  w3 [shape=plaintext, label="is_1"];
  w4 [shape=plaintext, label="is_2"];
  w5 [shape=plaintext, label="is_3"];
  { rank=sink; p0; p1; p2; p3; p4; p5; }
  p0 [shape=plaintext, label="next_0"];
  p1 [shape=plaintext, label="next_1"];
  p2 [shape=plaintext, label="next_2"];
  p3 [shape=plaintext, label="next_3"];
  p4 [shape=plaintext, label="accepts"];
  p5 [shape=plaintext, label="halts"];
  w2 -> p0;
  w3 -> p1;
  w4 -> p2;
  w5 -> p3;
  w2 -> p4;
  w0 -> p5;
}
```

`decr`, a shifter
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w0; w2; w3; w4; w5; }
  w0 [shape=plaintext, label="0"];
  w2 [shape=plaintext, label="is_0"];
  w3 [shape=plaintext, label="is_1"];
  w4 [shape=plaintext, label="is_2"];
  w5 [shape=plaintext, label="is_3"];
  { rank=sink; p0; p1; p2; p3; p4; p5; }
  p0 [shape=plaintext, label="next_0"];
  p1 [shape=plaintext, label="next_1"];
  p2 [shape=plaintext, label="next_2"];
  p3 [shape=plaintext, label="next_3"];
  p4 [shape=plaintext, label="accepts"];
  p5 [shape=plaintext, label="halts"];
  w3 -> p0;
  w4 -> p1;
  w5 -> p2;
  w0 -> p3;
  w0 -> p4;
  w2 -> p5;
}
```

`mod2`, two folds
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w0; w2; w3; w4; w5; }
  w0 [shape=plaintext, label="0"];
  w2 [shape=plaintext, label="is_0"];
  w3 [shape=plaintext, label="is_1"];
  w4 [shape=plaintext, label="is_2"];
  w5 [shape=plaintext, label="is_3"];
  w6 [label="OR\nfold_0"];
  w7 [label="OR\nfold_1"];
  { rank=sink; p0; p1; p2; p3; p4; p5; }
  p0 [shape=plaintext, label="next_0"];
  p1 [shape=plaintext, label="next_1"];
  p2 [shape=plaintext, label="next_2"];
  p3 [shape=plaintext, label="next_3"];
  p4 [shape=plaintext, label="accepts"];
  p5 [shape=plaintext, label="halts"];
  w2 -> w6;
  w4 -> w6;
  w3 -> w7;
  w5 -> w7;
  w6 -> p0;
  w7 -> p1;
  w0 -> p2;
  w0 -> p3;
  w0 -> p4;
  w0 -> p5;
}
```

Each cell names the wires on for that value of `AX`. `decr` at 0 lights only `halts`, and `AX` stays at 0.

| AX | `nada` | `zero` | `decr` | `mod2` |
|---|---|---|---|---|
| 0 | next_0 | next_0, accepts | halts | next_0 |
| 1 | next_1 | next_1 | next_0 | next_1 |
| 2 | next_2 | next_2 | next_1 | next_0 |
| 3 | next_3 | next_3 | next_2 | next_1 |

### The decoder

The decoder turns two bits into the four one-hot wires the machines read. Two NOT gates give the complements of `hb` and `lb`, and one AND gate per value lights its wire: `is_0` is `~hb` AND `~lb`.

The decoder, 6 gates
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w2; w3; }
  w2 [shape=plaintext, label="hb"];
  w3 [shape=plaintext, label="lb"];
  w4 [label="NOT\n~hb"];
  w5 [label="NOT\n~lb"];
  w6 [label="AND\nis_0"];
  w7 [label="AND\nis_1"];
  w8 [label="AND\nis_2"];
  w9 [label="AND\nis_3"];
  { rank=sink; p0; p1; p2; p3; }
  p0 [shape=plaintext, label="is_0"];
  p1 [shape=plaintext, label="is_1"];
  p2 [shape=plaintext, label="is_2"];
  p3 [shape=plaintext, label="is_3"];
  w2 -> w4;
  w3 -> w5;
  w4 -> w6;
  w5 -> w6;
  w4 -> w7;
  w3 -> w7;
  w2 -> w8;
  w5 -> w8;
  w2 -> w9;
  w3 -> w9;
  w6 -> p0;
  w7 -> p1;
  w8 -> p2;
  w9 -> p3;
}
```

| AX | hb | lb | is_0 | is_1 | is_2 | is_3 |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| 1 | 0 | 1 | 0 | 1 | 0 | 0 |
| 2 | 1 | 0 | 0 | 0 | 1 | 0 |
| 3 | 1 | 1 | 0 | 0 | 0 | 1 |

The same part, with its outputs named `op_nada` to `op_mod2`, decodes a program line's two bits.

### The encoder

The encoder turns one-hot wires back into bits: `hb'` is `next_2` OR `next_3`, and `lb'` is `next_1` OR `next_3`. Value 0 has both bits 0, and it is what remains when wires 1 to 3 are all off, so the encoder reads only those three.

The encoder, 2 gates
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w3; w4; w5; }
  w3 [shape=plaintext, label="next_1"];
  w4 [shape=plaintext, label="next_2"];
  w5 [shape=plaintext, label="next_3"];
  w6 [label="OR\nhb'"];
  w7 [label="OR\nlb'"];
  { rank=sink; p0; p1; }
  p0 [shape=plaintext, label="hb'"];
  p1 [shape=plaintext, label="lb'"];
  w4 -> w6;
  w5 -> w6;
  w3 -> w7;
  w5 -> w7;
  w6 -> p0;
  w7 -> p1;
}
```

| lit wire | hb' | lb' |
|---|---|---|
| next_0 | 0 | 0 |
| next_1 | 0 | 1 |
| next_2 | 1 | 0 |
| next_3 | 1 | 1 |

### The hold

A halted machine stays as it is. The op decoder gives `op_nada` to `op_mod2`, and the hold turns them into four enables: `en_nada` is `op_nada` OR `halted`, and each other enable is its op wire AND `~halted`. A halted machine runs `nada`, whatever its line says.

The hold, 5 gates
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w2; w3; w4; w5; w6; }
  w2 [shape=plaintext, label="op_nada"];
  w3 [shape=plaintext, label="op_zero"];
  w4 [shape=plaintext, label="op_decr"];
  w5 [shape=plaintext, label="op_mod2"];
  w6 [shape=plaintext, label="halted"];
  w7 [label="NOT\n~halted"];
  w8 [label="OR\nen_nada"];
  w9 [label="AND\nen_zero"];
  w10 [label="AND\nen_decr"];
  w11 [label="AND\nen_mod2"];
  { rank=sink; p0; p1; p2; p3; }
  p0 [shape=plaintext, label="en_nada"];
  p1 [shape=plaintext, label="en_zero"];
  p2 [shape=plaintext, label="en_decr"];
  p3 [shape=plaintext, label="en_mod2"];
  w6 -> w7;
  w2 -> w8;
  w6 -> w8;
  w3 -> w9;
  w7 -> w9;
  w4 -> w10;
  w7 -> w10;
  w5 -> w11;
  w7 -> w11;
  w8 -> p0;
  w9 -> p1;
  w10 -> p2;
  w11 -> p3;
}
```

| halted | op | en_nada | en_zero | en_decr | en_mod2 |
|---|---|---|---|---|---|
| 0 | `nada` | 1 | 0 | 0 | 0 |
| 0 | `zero` | 0 | 1 | 0 | 0 |
| 0 | `decr` | 0 | 0 | 1 | 0 |
| 0 | `mod2` | 0 | 0 | 0 | 1 |
| 1 | `nada` | 1 | 0 | 0 | 0 |
| 1 | `zero` | 1 | 0 | 0 | 0 |
| 1 | `decr` | 1 | 0 | 0 | 0 |
| 1 | `mod2` | 1 | 0 | 0 | 0 |

### The selector

Exactly one enable is on. The selector builds each output wire of the step as an OR, over the four machines, of that machine's wire AND its enable, so the OR passes the enabled machine's wire. Only the wires a machine drives get gates, and the ORs pair up. Here is the selector for one wire, `next_1`:

The selector for `next_1`, 7 gates
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w2; w3; w4; w5; w6; w7; w8; w9; }
  w2 [shape=plaintext, label="en_nada"];
  w3 [shape=plaintext, label="en_zero"];
  w4 [shape=plaintext, label="en_decr"];
  w5 [shape=plaintext, label="en_mod2"];
  w6 [shape=plaintext, label="nada's"];
  w7 [shape=plaintext, label="zero's"];
  w8 [shape=plaintext, label="decr's"];
  w9 [shape=plaintext, label="mod2's"];
  w10 [label="AND\nnada"];
  w11 [label="AND\nzero"];
  w12 [label="AND\ndecr"];
  w13 [label="AND\nmod2"];
  w14 [label="OR\nnext_1"];
  w15 [label="OR\nnext_1"];
  w16 [label="OR\nnext_1"];
  { rank=sink; p0; }
  p0 [shape=plaintext, label="next_1"];
  w6 -> w10;
  w2 -> w10;
  w7 -> w11;
  w3 -> w11;
  w8 -> w12;
  w4 -> w12;
  w9 -> w13;
  w5 -> w13;
  w10 -> w14;
  w11 -> w14;
  w12 -> w15;
  w13 -> w15;
  w14 -> w16;
  w15 -> w16;
  w16 -> p0;
}
```

The whole selector does the same for `next_0` to `next_3`, `accepts` and `halts`: 24 gates. `tests/test_circuit.c` checks each wire on all four enables and all 16 settings of its drivers.

### The latches

A flag, once on, stays on: `accepted'` is `accepted` OR `accepts`, and `halted'` is `halted` OR `halts`.

The latches, 2 gates
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  { rank=source; w2; w3; w4; w5; }
  w2 [shape=plaintext, label="halted"];
  w3 [shape=plaintext, label="halts"];
  w4 [shape=plaintext, label="accepted"];
  w5 [shape=plaintext, label="accepts"];
  w6 [label="OR\nhalted'"];
  w7 [label="OR\naccepted'"];
  { rank=sink; p0; p1; }
  p0 [shape=plaintext, label="halted'"];
  p1 [shape=plaintext, label="accepted'"];
  w2 -> w6;
  w3 -> w6;
  w4 -> w7;
  w5 -> w7;
  w6 -> p0;
  w7 -> p1;
}
```

| accepted | accepts | accepted' |
|---|---|---|
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 1 |

## The machine as a circuit

{{chapter Parts for a small computer}} built ten parts from gates. This chapter gives each part a name and draws it from here on as a box, its gates inside, and wires the boxes into a *stepper*: one step of the machine. The circuit is combinational: the whole run is laid out in space, and every wire settles at once. {{chapter The Cook-Levin machine}}, in the second part, turns the same circuit around.

### The parts as boxes

Each box is known by its wires. The table counts them from the netlist, with the gates inside each box:

| box | wires in | wires out | gates |
|---|---|---|---|
| decoder | 2 | 4 | 6 |
| op decoder | 2 | 4 | 6 |
| hold | 5 | 4 | 5 |
| nada | 4 | 6 | 0 |
| zero | 4 | 6 | 0 |
| decr | 4 | 6 | 0 |
| mod2 | 4 | 6 | 2 |
| selector | 19 | 6 | 24 |
| encoder | 3 | 2 | 2 |
| latches | 4 | 2 | 2 |

The op decoder is the decoder again, on the program line's two bits. The selector reads 20 wires: four enables from the hold, and every wire the four machines drive.

### A stepper, from its boxes

A stepper has six wires in, `hb` and `lb` for `AX`, the flags `halted` and `accepted`, and `op_hb` and `op_lb` for its program line, and four wires out, the same state for the next step. Inside, it decodes, computes every opcode at once, and selects: the decoder feeds all four machines side by side, the op decoder and the hold pick one, the selector passes that machine's wires, and the encoder turns the chosen value back into bits. The latches sit beside them. A number on a line is how many wires it carries.

A stepper, ten boxes
```dot
digraph {
  rankdir=TB;
  node [shape=box, style=rounded];
  i0 [shape=plaintext, label="hb"];
  i1 [shape=plaintext, label="lb"];
  i2 [shape=plaintext, label="op_hb"];
  i3 [shape=plaintext, label="op_lb"];
  i4 [shape=plaintext, label="halted"];
  i5 [shape=plaintext, label="accepted"];
  b0 [label="decoder"];
  b1 [label="op decoder"];
  b2 [label="hold"];
  b3 [label="nada"];
  b4 [label="zero"];
  b5 [label="decr"];
  b6 [label="mod2"];
  b7 [label="selector"];
  b8 [label="encoder"];
  b9 [label="latches"];
  o0 [shape=plaintext, label="hb'"];
  o1 [shape=plaintext, label="lb'"];
  o2 [shape=plaintext, label="halted'"];
  o3 [shape=plaintext, label="accepted'"];
  i0 -> b0;
  i1 -> b0;
  i2 -> b1;
  i3 -> b1;
  b1 -> b2 [label="4"];
  i4 -> b2;
  b0 -> b3 [label="4"];
  b0 -> b4 [label="4"];
  b0 -> b5 [label="4"];
  b0 -> b6 [label="4"];
  b2 -> b7 [label="4"];
  b3 -> b7 [label="4"];
  b4 -> b7 [label="5"];
  b5 -> b7 [label="4"];
  b6 -> b7 [label="2"];
  b7 -> b8 [label="3"];
  i4 -> b9;
  b7 -> b9 [label="2"];
  i5 -> b9;
  b8 -> o0;
  b8 -> o1;
  b9 -> o2;
  b9 -> o3;
}
```

`tests/test_circuit.c` holds the stepper to `sat_step` on all 64 settings of its six inputs. Its gates, box by box:

| part | NOT | AND | OR | gates |
|---|---|---|---|---|
| decoder | 2 | 4 | 0 | 6 |
| op decoder | 2 | 4 | 0 | 6 |
| hold | 1 | 3 | 1 | 5 |
| nada | 0 | 0 | 0 | 0 |
| zero | 0 | 0 | 0 | 0 |
| decr | 0 | 0 | 0 | 0 |
| mod2 | 0 | 0 | 2 | 2 |
| selector | 0 | 15 | 9 | 24 |
| encoder | 0 | 0 | 2 | 2 |
| latches | 0 | 0 | 2 | 2 |
| **the stepper** | 5 | 26 | 16 | 47 |
| **a core**, six steppers | 30 | 156 | 96 | 282 |
| **the decider**, four cores and the comparators | 128 | 635 | 388 | 1,151 |

The three machines `nada`, `zero` and `decr` are wires alone, at 0 gates. The selector is more than half the stepper: one AND for each wire a machine drives, and the ORs that join them. One wire ends at the selector: it builds `next_0`, and the encoder reads only wires 1 to 3. It stays, because each part does its whole job as its rule says.

### The stepper of the second part

`sat_step` in `src/sat.c`, the stepper of {{chapter The Cook-Levin machine}}, is the same design in 34 gates, and `tests/test_circuit.c` holds this stepper to it on all 64 settings. It folds and merges where this stepper is literal; the 13 gates between them are these:

- it writes each NOT as a literal's sign, which is free in a formula: 5 gates;
- it leaves out wire 0, since value 0 is what remains when wires 1 to 3 are off: `fold_0` and `next_0`'s 4 AND and 3 OR gates, 8 in all.

## The circuit, in full

{{chapter The machine as a circuit}} ends with a stepper as one box. This chapter chains six of them into the machine, then puts four machines side by side to decide a problem. Every drawing is still generated from the netlist of `src/circuit.c`, which records how each box is wired.

### Six steppers make a core

A *core* is the machine unrolled. Its input arrives as `AX`'s two bits, both flags start at the constant 0, and the program's twelve bits split into six lines of two. Each stepper takes the four state wires the one before it left and its own line's two bits, and the sixth stepper's `accepted'` is the core's answer.

One core: six steppers, 282 gates
```dot
digraph {
  rankdir=TB;
  node [shape=box, style=rounded];
  i0 [shape=plaintext, label="AX"];
  i1 [shape=plaintext, label="0"];
  i2 [shape=plaintext, label="line 1"];
  i3 [shape=plaintext, label="line 2"];
  i4 [shape=plaintext, label="line 3"];
  i5 [shape=plaintext, label="line 4"];
  i6 [shape=plaintext, label="line 5"];
  i7 [shape=plaintext, label="line 6"];
  b0 [label="step 1"];
  b1 [label="step 2"];
  b2 [label="step 3"];
  b3 [label="step 4"];
  b4 [label="step 5"];
  b5 [label="step 6"];
  o0 [shape=plaintext, label="accepted"];
  i0 -> b0 [label="2"];
  i1 -> b0 [label="2"];
  i2 -> b0 [label="2"];
  b0 -> b1 [label="4"];
  i3 -> b1 [label="2"];
  b1 -> b2 [label="4"];
  i4 -> b2 [label="2"];
  b2 -> b3 [label="4"];
  i5 -> b3 [label="2"];
  b3 -> b4 [label="4"];
  i6 -> b4 [label="2"];
  b4 -> b5 [label="4"];
  i7 -> b5 [label="2"];
  b5 -> o0;
}
```

`tests/test_circuit.c` runs the core on all 16,384 runs, 4,096 programs by four inputs, holds every stepper's state, step by step, to `sat_step`'s, and holds the core's answer to the interpreter's.

### Four cores decide a problem

Whether a program decides a problem takes four cores, one per input. The *want* register holds four bits, one per input, saying which inputs the problem holds: {1, 2, 3} is `0111`, input 0's bit first. The program's twelve bits go to all four cores, and each core has its input wired in as constants. A comparator, `same`, checks each core's answer against its want bit, and an AND of the four lights "decides".

The decider: four cores, 1,151 gates
```dot
digraph {
  rankdir=LR;
  node [shape=box, style=rounded];
  i0 [shape=plaintext, label="program"];
  i1 [shape=plaintext, label="AX = 0"];
  i2 [shape=plaintext, label="want_0"];
  i3 [shape=plaintext, label="AX = 1"];
  i4 [shape=plaintext, label="want_1"];
  i5 [shape=plaintext, label="AX = 2"];
  i6 [shape=plaintext, label="want_2"];
  i7 [shape=plaintext, label="AX = 3"];
  i8 [shape=plaintext, label="want_3"];
  b0 [label="core 0"];
  b1 [label="same 0"];
  b2 [label="all"];
  b3 [label="core 1"];
  b4 [label="same 1"];
  b5 [label="core 2"];
  b6 [label="same 2"];
  b7 [label="core 3"];
  b8 [label="same 3"];
  o0 [shape=plaintext, label="decides"];
  i0 -> b0 [label="12"];
  i1 -> b0 [label="2"];
  b0 -> b1;
  i2 -> b1;
  b1 -> b2;
  i0 -> b3 [label="12"];
  i3 -> b3 [label="2"];
  b3 -> b4;
  i4 -> b4;
  b4 -> b2;
  i0 -> b5 [label="12"];
  i5 -> b5 [label="2"];
  b5 -> b6;
  i6 -> b6;
  b6 -> b2;
  i0 -> b7 [label="12"];
  i7 -> b7 [label="2"];
  b7 -> b8;
  i8 -> b8;
  b8 -> b2;
  b2 -> o0;
}
```

A comparator is five gates: (`accepted` AND `want`) OR (`~accepted` AND `~want`). `tests/test_circuit.c` runs the decider on all 4,096 programs and all 16 want registers: it lights "decides" exactly where the program decides the problem the register holds, 4,096 times in all.

### A program through it

Load the want register with `0111` and the program register with program 2916 (`decr, mod2, zero, decr, zero, nada`), the five-line answer to `is_not_tiny`, whose bits are `10 11 01 10 01 00`. The four cores settle like this:

| core | the run | accepted | want | same? |
|---|---|---|---|---|
| 0 | `decr` finds `AX` at 0: reject | 0 | 0 | yes |
| 1 | `decr` to 0, `mod2` keeps 0, `zero`: accept | 1 | 1 | yes |
| 2 | `decr` to 1, `mod2` keeps 1, `zero` passes, `decr` to 0, `zero`: accept | 1 | 1 | yes |
| 3 | `decr` to 2, `mod2` folds 2 to 0, `zero`: accept | 1 | 1 | yes |

All four comparators say yes, so the AND lights "decides".

## Some history of the approach

This chapter is mostly story, and a reader in a hurry may skip it: the rest of the book reads the same. It says in what order Steve built the Python, and which view of the toy the order passed over.

### The order of the work

The virtual-machine log records the order. All of it came in four days of March 2023:

1. **2023-03-11, 01:49:** the machine, `run_progam`, and the search `find_solutions`, in the first commit ([f01c1c3](https://github.com/showell/virtual-machine/commit/f01c1c3)).
2. **13:02 the same day:** the analytical solution ([d6bb78f](https://github.com/showell/virtual-machine/commit/d6bb78f)), as {{chapter Programs as languages}} tells.
3. **15:47 the same day:** the stepper ([ba9388a](https://github.com/showell/virtual-machine/commit/ba9388a)), with its `NOT`, `AND` and `OR` already arithmetic on integers (`NOT x` was (x − 1)·(x − 1) then), and the assertion that the stepper accepts exactly the problem's inputs.
4. **2023-03-12:** `mod2` and `pass`, today's `nada` ([cad8e18](https://github.com/showell/virtual-machine/commit/cad8e18)), then the polynomial library from 14:35 ([6bb7429](https://github.com/showell/virtual-machine/commit/6bb7429)).
5. **2023-03-14:** the step polynomials at 12:22 ([a4e4765](https://github.com/showell/virtual-machine/commit/a4e4765)), and at 12:32 programs of six lines, with the assertion that every problem has a program ([8646cbe](https://github.com/showell/virtual-machine/commit/8646cbe)).

### The view the order passed over

From the machine Steve went straight to polynomials: he wrote the machine's logic as arithmetic on integers, so that the machine's whole run could become one big polynomial. The plain Boolean view of the toy, its problems and its step written with ordinary logic gates, sits between the two, and the order passed over it. {{chapter The Boolean view}} sets it out.

### Why polynomials

The polynomials were part of the point. Steve, on 2026-11-25, said that part of the project was "to generate obscenely large [polynomials] that actually mapped to a simple Nibble VM solving our 2-bit language". The second part of the book composes them, and {{chapter The polynomials, in full}} prints them whole.

## How it got here

The first part ran the toy. This part translates it the way Cook and Levin translated a checking machine, and checks that the translation answers as the toy does. This chapter is the short history of that translation, as two git logs record it.

### The Python, 2023

The virtual-machine log opens on 2023-03-11 with [f01c1c3](https://github.com/showell/virtual-machine/commit/f01c1c3) and runs through 158 commits to 2023-04-08, all under Steve's address; {{chapter Some history of the approach}} gives the order of its first four days. The last 2023 commits, on 2023-04-08, begin with "Eliminate most poly-related stuff" ([72bf4c4](https://github.com/showell/virtual-machine/commit/72bf4c4)).

### The composition that stalled, 2026

Five commits marked "wip" followed on 2026-01-28. On 2026-04-16 a measuring harness, `compose.py`, was added with Claude as co-author ([1a5f6fa](https://github.com/showell/virtual-machine/commit/1a5f6fa)). Its message says that step 1 composed in about 0.9 s and that step 2 ran past 30 s in Python, and that those numbers motivated a port to Go. The same afternoon `compose.py` was removed, "porting six-step composition to Go instead" ([a2406b5](https://github.com/showell/virtual-machine/commit/a2406b5)). The next day an essay Claude had written about the project was parked as `DEEP_READ.md` ([94c3d31](https://github.com/showell/virtual-machine/commit/94c3d31)); it lists the six-step composition as the step the project stops before.

The Go port lives in a repository of its own, outside this record. By Steve's account it drowned in big integers; that account is unchecked here.

### The finish, 2026-11-25

The cook-levin-riff log covers one day. The C port of the machine and the search came first (`8051ce2`), then the general polynomial (`ccde4f8`) and the stepper (`5b14d74`). The Boolean ring came at 15:59 (`0c233e3`), and all six steps composed, proved on all 2^14 points, at 16:23 (`1146d43`). The Nibble page (`b1fe11f`), the Roc port (`6206236`), the SAT encoding (`8de51b7`), rocsat as an oracle (`f977bcc`) and the JavaScript port (`e996749`, `c251895`) followed. The first commit is stamped 15:32 and the last 19:21.

## P, NP and Cook-Levin

The toy's every question was settled by trying everything: 4,096 programs on four inputs. For a machine with unbounded inputs, trying every certificate takes time that grows exponentially with the input's length, and complexity theory asks which questions can be answered faster.

### When deciding is cheap, and when checking is

Decidable languages differ in cost. P is the class decided in time polynomial in the input's length. NP is the class whose members have short certificates that a machine can check in polynomial time: a formula is in SAT, the language of satisfiable Boolean formulas, when an assignment makes it true, and checking an assignment is quick. In 1971 Stephen Cook, in "The Complexity of Theorem-Proving Procedures", and independently Leonid Levin, published in 1973, showed that SAT is as hard as anything in NP. Their proof takes the checking machine, which runs for a polynomial number of steps, and writes each step of its run as clauses: the whole run becomes one formula, satisfiable exactly when a certificate exists.

### Guessing, and the computation history

Michael Sipser's MIT lecture on the theorem ([lecture 16](https://learn.mit.edu/video/7036/lecture-16-cook-levin-theorem?playlist=7021)) frames NP with a nondeterministic machine. At each choice it forks and follows every branch at once, and it accepts if any branch accepts. Such a machine forks for free and follows every branch, which is different from guessing at random. For the toy, the choices are the program's twelve bits. A nondeterministic machine guesses them two at a time, then runs the guessed program on the four inputs. A player clicking through a puzzle's buttons, knowing only the buttons, is that machine run one branch at a time, as {{chapter Which instructions are needed}} counts.

Sipser calls the record of one branch's run, every state from the start to the verdict, its *computation history*. Cook and Levin write a formula that is satisfiable exactly when some branch has an accepting computation history. For the toy, a history is the tableau of {{chapter The Cook-Levin machine}}: seven rows by four cores.

### The toy as a checker

The toy is such a checker. Given a problem and a program, it runs the program on four inputs and says whether the program decides the problem; the program is the certificate. The chapters that follow take that checker apart two ways, after {{chapter Six expressions, composed}} sets the two side by side. {{chapter Arithmetization}} through {{chapter The polynomials, in full}} write its step as polynomials and compose all six, the riff's own route. {{chapter The Cook-Levin machine}} through {{chapter The solver's work, in full}} take Cook and Levin's route: the run as a tableau, the tableau as clauses, and a SAT solver to find the certificates.

## Six expressions, composed

{{chapter The Boolean view}} ran the machine as four Boolean functions of six bits, composed over six steps, with the input and the program fixed. Cook and Levin's move is to stop fixing the program. Keep the same four functions and the same six steps, give the program's twelve bits names in place of values, v1 to v12, and what comes out of the sixth step is a formula where it was a bit. This chapter does that, turns the formula into the question a puzzle asks, and then names the two ways of composing that the rest of this part follows.

### The program unknown

Line k's opcode bits are v(2k − 1) and v(2k), so instruction k is 2·v(2k − 1) + v(2k), numbered as in {{chapter Cook-Levin as SAT}}. The input stays fixed, so `hb` and `lb` start as constants and both flags as 0. `python/steps.py` composes the six steps with the same Python functions, now given formulas: a constant folds away, and an equal subformula is built once. The final `accepted`, for each input, has this many gates:

| AX | gates in accepted |
|---|---|
| 0 | 41 |
| 1 | 91 |
| 2 | 145 |
| 3 | 148 |

For input 0 it is small enough to read:

```text
   (NOT v1 AND v2)
OR (NOT (v1 AND NOT v2) AND NOT v3 AND v4)
OR (NOT ((v1 AND NOT v2) OR (v3 AND NOT v4)) AND NOT v5 AND v6)
OR (NOT ((v1 AND NOT v2) OR (v3 AND NOT v4) OR (v5 AND NOT v6)) AND NOT v7 AND v8)
OR (NOT ((v1 AND NOT v2) OR (v3 AND NOT v4) OR (v5 AND NOT v6) OR (v7 AND NOT v8)) AND NOT v9 AND v10)
OR (NOT ((v1 AND NOT v2) OR (v3 AND NOT v4) OR (v5 AND NOT v6) OR (v7 AND NOT v8) OR (v9 AND NOT v10)) AND NOT v11 AND v12)
```

Line k's bits `0 1` are `zero`, and `1 0` are `decr`. So the formula says: some line is `zero`, and every line before it is `nada`, `zero` or `mod2`. That is input 0's whole story, since `AX` starts at 0: `zero` accepts it, `decr` rejects it, and `nada` and `mod2` leave it as it is. `python/steps.py` holds all four formulas to the interpreter on all 4,096 programs.

### Composition

Free the input's two bits as well. The first step reads them as `hb` and `lb`, both flags at 0, and line 1's two bits. Each later step reads the four bits the one before it wrote, and its own line's two bits. So the only bits that enter from outside the chain are the input's two and each line's two, six lines of them:

| | bits |
|---|---|
| the input, `hb` and `lb` at the start | 2 |
| the flags at the start | 0 (both constant 0) |
| the program, two a line for six lines | 12 |
| **the composed expression's variables** | **14** |

Everything else is internal: each step's gates, and of the 24 state bits the steps write, the 20 that a next step reads. After the sixth step, `accepted` is an expression accepted(n, v) of those 14 bits.

### The question as one expression

A puzzle asks whether a program decides L. That is one more expression, of the twelve program bits alone:

```text
decides_L(v1, ..., v12) = AND over n in 0..3 of (accepted(n, v) == L(n))
```

L(n) is the constant 1 or 0, so each of the four comparisons is `accepted(n, v)` or its complement. The true points of `decides_L` are exactly the programs that decide L.

### The payoff

Expanded and simplified, `decides_L` is a sum of products over v1 to v12. `src/boolean_main.c` computes one for every problem: it finds the programs that decide L by running all 4,096, takes the prime implicants of that set, and covers it greedily, the product holding the most programs left taken first. Each product is written as its twelve bits, two a line of the program: 1 for v, 0 for its complement, and - for a bit the product leaves free. For {0, 2, 3} the expression is one product, the conjunction of 12 literals that spells its one program, `zero, decr, decr, zero, decr, zero`:

```text
01 10 10 01 10 01
```

For {0, 3}, 12 programs take 8 products. The first is `zero, decr, decr, decr, zero` with line 6 free:

```text
01 10 10 10 01 --
0- 01 10 10 10 01
01 10 10 00 10 01
01 10 00 10 10 01
01 0- 10 10 10 01
01 10 10 11 10 01
01 10 10 10 -1 01
01 10 10 10 0- 01
```

The counts for all 16, programs and products, from the same run:

| code | problem | programs | products |
|---|---|---|---|
| 0 | { } | 844 | 87 |
| 1 | {0} | 681 | 41 |
| 2 | {1} | 303 | 32 |
| 3 | {0, 1} | 172 | 26 |
| 4 | {2} | 248 | 80 |
| 5 | {0, 2} | 883 | 121 |
| 6 | {1, 2} | 74 | 31 |
| 7 | {0, 1, 2} | 13 | 8 |
| 8 | {3} | 63 | 30 |
| 9 | {0, 3} | 12 | 8 |
| 10 | {1, 3} | 520 | 130 |
| 11 | {0, 1, 3} | 150 | 54 |
| 12 | {2, 3} | 13 | 9 |
| 13 | {0, 2, 3} | 1 | 1 |
| 14 | {1, 2, 3} | 15 | 10 |
| 15 | {0, 1, 2, 3} | 104 | 40 |

`tests/test_boolean.c` holds every problem's sum of products to `run_progam`, program by program, and the book's build holds the blocks here to `boolean_main`'s output.

### Two ways to compose

The composing above ran all 4,096 programs, which a toy allows. For a real checker there are two ways to compose the step's expressions, and the rest of this part takes each in turn.

- **Expand it.** Substitute each step's expressions into the next and multiply them out, into one big expression of the fourteen bits. {{chapter Arithmetization}} does this with the logic written as arithmetic on integers, AND as a product and the complement of x as 1 − x, which makes each expression a polynomial; that move is called arithmetization. {{chapter Six steps composed}} expands all six, to 10,324 terms for `accepted`. The size grows with every step substituted.
- **Name it.** Give each intermediate wire a fresh variable, and state only how it relates to its inputs. That is Tseitin's encoding and SAT: {{chapter Cook-Levin as SAT}} has 416 variables and 1,216 clauses for each problem, and the count grows linearly with the steps, one stepper's gates per step.

Naming is why Cook-Levin's formula stays polynomial in size: each step adds the same few clauses, whatever came before it.

## Arithmetization

The stepper writes one step of the machine as polynomials, the first of the two ways to compose in {{chapter Six expressions, composed}}: expand. Each bit of the state is a variable that is only ever 0 or 1, and logic becomes arithmetic:

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

In Python's general polynomial ring, where x·x is x², the four polynomials come out large: hb' has 786 terms and lb' 2,979 (`README.md`, its layout's `src/stepper.c`). The C port prints them exactly as `stepper.py` does, held to `tests/golden/step_hb.txt` and its three siblings. In the Boolean ring of {{chapter Why composing blew up}} the same four have 7, 13, 17 and 33 terms, every coefficient 1 or -1, as an expect in `roc/Stepper.roc` holds. {{chapter The polynomials, in full}} prints all eight whole.

Here is `halted'` in that ring. It says: halted already, or a `decr` at `AX` = 0 on a running machine.

```text
halted+op_hb+(-1)*hb*op_hb+(-1)*lb*op_hb+(-1)*halted*op_hb+(-1)*op_hb*op_lb
+hb*lb*op_hb+hb*halted*op_hb+hb*op_hb*op_lb+lb*halted*op_hb+lb*op_hb*op_lb
+halted*op_hb*op_lb+(-1)*hb*lb*halted*op_hb+(-1)*hb*lb*op_hb*op_lb
+(-1)*hb*halted*op_hb*op_lb+(-1)*lb*halted*op_hb*op_lb+hb*lb*halted*op_hb*op_lb
```

### The stepper as a second machine

Evaluated at the bits of a state and an instruction, the four polynomials give the next state's bits. So the stepper is a second implementation of the machine, written as algebra.

**Theorem 10.** On every one of the 4,096 programs and each of the four inputs, the stepper, evaluating the step polynomials one step at a time, accepts exactly when `run_progam` does.

*Proof.* `tests/test_cross_check.c` runs all 16,384 pairs both ways and holds the differences to 0 and the accepting pairs to 5,596. The Python's `find_solutions` makes the same check on each problem's example program alone, 16 programs on 4 inputs.

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

With the identity in place the step polynomials shrink from 786 and 2,979 terms to 7 and 13, and the composition of {{chapter Six steps composed}} takes a fraction of a second. {{chapter The polynomials, in full}} shows the step both ways, Python's 147,274 characters of lb' beside its 230 in the Boolean ring.

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

The final `hb` has degree 9 and its largest coefficient is 6; `lb` has degree 14 and 21; `halted` 14 and 20; `accepted` 14 and 63. Sixty-three is the largest coefficient of the four final polynomials, so int64 holds each with room to spare. {{chapter The polynomials, in full}} prints the four. Built with `gcc -O2` for this book, `compose_main` composed all six in 233 ms, 199 of them in step 6, at a peak of 4,224 KiB resident.

### The proof

Fourteen 0/1 variables make 2^14 = 16,384 points, and each point is one input with one program. This is the book's thesis, as a theorem:

**Theorem 11.** At every one of the 2^14 points, the composed polynomial `accepted` is 1 when the machine accepts that input with that program and 0 when it rejects it.

*Proof.* `tests/test_compose.c` evaluates the four final polynomials at every point and holds them to `run_progam` (its test `the_composed_polynomials_agree_with_the_machine_on_all_2_to_the_14`). The Roc port's proof prints the same verdict (`roc/golden/composition.txt`):

```text
proof: 16384 points, 0 differ, 5596 accepted, 2891 accepted with no halt
```

The last count is the Python's own reading of `halted`, kept as it is: a `zero` that accepts leaves the stepper's `halted` at 0, so 2,891 accepting runs end with it unset. The verdict is `accepted`, and it agrees with the machine at every point.

The proof is exhaustive: it checks every input with every program. And since a multilinear polynomial is fixed by its values on {0, 1}^14, any implementation whose four polynomials pass it has exactly these four.

## The polynomials, in full

This chapter prints the polynomials of the last three chapters whole, every term of each. Over each box is what the book's app (`book/app/Polynomials.roc`) counts in the text it shows: terms, degree, the largest coefficient and characters. The chapter states each term count again in its source, and the build stops if a box's text has any other.

### What the sizes mean

Each final polynomial is a function of 14 bits: the input's two, `h0` and `l0`, and two for each instruction, `op1_hb` and `op1_lb` through `op6_hb` and `op6_lb`. Set them to one input and one program, and `accepted` evaluates to 1 when that program accepts that input and to 0 when it rejects it. Its 10,324 terms are all 4,096 programs run on all four inputs, written as one sum. `tests/test_compose.c` evaluates it at all 16,384 points and holds every value to the machine.

### One step, as the Python printed it

These are the step polynomials of {{chapter Arithmetization}} in the general ring, where x·x is x², as Steve's `stepper.py` prints them. `tests/golden/step_hb.txt` and its three siblings hold Python's own `str` of each, and `tests/test_stepper.c` holds the C's printing to them byte for byte (`5b14d74`). For this book `stepper.py` was run again, at virtual-machine's [94c3d31](https://github.com/showell/virtual-machine/commit/94c3d31), and its four strings matched the goldens byte for byte.

```polynomial tests/golden/step_hb.txt
hb', one step in the general ring, as Python's stepper.py prints it
786
```

```polynomial tests/golden/step_lb.txt
lb', one step in the general ring, as Python's stepper.py prints it
2979
```

```polynomial tests/golden/step_halted.txt
halted', one step in the general ring, as Python's stepper.py prints it
25
```

```polynomial tests/golden/step_accepted.txt
accepted', one step in the general ring, as Python's stepper.py prints it
33
```

### The same step, with x² = x

In the Boolean ring of {{chapter Why composing blew up}} each power folds to its variable, and like terms meet:

| polynomial | general ring | Boolean ring |
|---|---|---|
| hb' | 786 terms, 36,547 characters | 7 terms, 116 characters |
| lb' | 2,979 terms, 147,274 characters | 13 terms, 230 characters |
| halted' | 25 terms, 514 characters | 17 terms, 294 characters |
| accepted' | 33 terms, 728 characters | 33 terms, 728 characters |

`accepted'` is already multilinear in the general ring, so the identity keeps its 33 terms; the two printers order them differently. The four below are the C's `bp_str`, as `roc/golden/step_polynomials.txt` holds them, and the Roc port's gate, `roc/check.shale`, prints the same file.

```polynomial roc/golden/step_polynomials.txt hb'
hb', one step in the Boolean ring, printed by the C's bp_str
7
```

```polynomial roc/golden/step_polynomials.txt lb'
lb', one step in the Boolean ring, printed by the C's bp_str
13
```

```polynomial roc/golden/step_polynomials.txt halted'
halted', one step in the Boolean ring, printed by the C's bp_str
17
```

```polynomial roc/golden/step_polynomials.txt accepted'
accepted', one step in the Boolean ring, printed by the C's bp_str
33
```

### The whole program

The four final polynomials come from `src/compose_main.c` (`1146d43`). `book/book.shale` builds it with `gcc -O2` and runs it each time the book is built, and holds what it prints, each step's term counts and each final polynomial's terms, degree and largest coefficient, to `roc/golden/composition.txt`, which the Roc and JavaScript ports print too. Then the app reads the four files it writes. `accepted` is the largest, at 510,387 characters.

```polynomial book/build/out/hb.txt
hb after six steps, composed by the C's compose_main at the book's build
319
```

```polynomial book/build/out/lb.txt
lb after six steps, composed by the C's compose_main at the book's build
2185
```

```polynomial book/build/out/halted.txt
halted after six steps, composed by the C's compose_main at the book's build
2732
```

```polynomial book/build/out/accepted.txt
accepted after six steps, composed by the C's compose_main at the book's build
10324
```

## The Cook-Levin machine

The decider of {{chapter The circuit, in full}} is a checker: hand it a problem and a program, and it says true or false. Cook and Levin's idea is to run a checker backwards. Fix the problem, leave the program unknown, pin the output to true, and ask which programs fit. The circuit does the same work as before; only the question changes.

### The circuit, turned around

The want register is fixed at the problem's bits. The program register becomes twelve unknown wires, `v1` to `v12`. The output wire is required to be 1. Everything in between is the same 24 steppers and four comparators, and every wire in them is now an unknown too, tied to the wires feeding it by the rule of its gate.

The question the circuit now asks
```dot
digraph {
  rankdir=TB;
  node [shape=box, style=rounded];
  x [label="program: v1 .. v12\nunknown"];
  want [label="want register\nfixed"];
  c [label="the same circuit\nevery wire an unknown"];
  out [label="decides = 1\nrequired"];
  x -> c;
  want -> c;
  c -> out;
}
```

A setting of the twelve unknowns that satisfies every gate's rule and lights the output is a program that decides the problem. The circuit has a satisfying setting exactly when some program decides the problem.

### The tableau

Cook's proof draws the run of a checking machine as a grid, the tableau: one row for each moment of the run, each row the machine's whole state, and each row fixed by the row above it through a small local rule. Here the grid has seven rows, the start and the six steps, and four columns, one per core. Each cell is a state of four bits, `hb`, `lb`, `halted` and `accepted`. The top row is known, since core `i` starts with `AX` = `i` and both flags 0, and each cell below it is one stepper applied to the cell above and to that row's line of the unknown program. The local rule is the stepper, drawn from its parts in {{chapter The machine as a circuit}}.

For a general checker the grid is as wide as its tape and as tall as its running time, and when the time is polynomial in the input's length, the grid and its rules are polynomial in size too. That is the whole of the theorem's construction: any question a fast checker can answer becomes the question of whether such a grid can be filled in consistently.

### The gates, counted

The stepper's gates as the SAT encoding builds them, in `sat_step` (`src/sat.c`): 34, where the stepper of {{chapter The machine as a circuit}} has 47, and that chapter accounts for the 13 between them.

{{lines src/sat.c from "    Lit is_3 = AND(hb, lb);" to "    Lit lb_set = OR(becomes_3, becomes_1);"}}

`tests/test_sat.c` holds these gates to the machine's step on all 64 settings of the stepper's six input wires. A stepper is 34 gates here when all six inputs are unknown, so 24 steppers would be 816. The top row's constants fold many away, since each core's `AX` is known at the start and both flags are 0, and a gate already built from the same two inputs is reused. The whole circuit comes to 404 gates over the 12 unknown program bits.

{{chapter Cook-Levin as SAT}} takes the last step: each gate becomes three clauses, the required output becomes four more, and the circuit becomes one formula for a SAT solver.

## The tableau, filled in

{{chapter The Cook-Levin machine}} describes Cook's grid; this chapter prints it, once for one program and once for every program at once.

### The tableau of one program

Here is the tableau filled in for `is_not_tiny`'s answer, program 2916 (`decr, mod2, zero, decr, zero, nada`): each cell is the state after that row's line, from the stepper's compiled step (`src/tableau_main.c`, which the book's build runs and holds these tables to).

| step | line | core 0 | core 1 | core 2 | core 3 |
|---|---|---|---|---|---|
| start | | AX=0 | AX=1 | AX=2 | AX=3 |
| 1 | `decr` | AX=0, halted | AX=0 | AX=1 | AX=2 |
| 2 | `mod2` | AX=0, halted | AX=0 | AX=1 | AX=0 |
| 3 | `zero` | AX=0, halted | AX=0, accepted | AX=1 | AX=0, accepted |
| 4 | `decr` | AX=0, halted | AX=0, accepted, halted | AX=0 | AX=0, accepted, halted |
| 5 | `zero` | AX=0, halted | AX=0, accepted, halted | AX=0, accepted | AX=0, accepted, halted |
| 6 | `nada` | AX=0, halted | AX=0, accepted, halted | AX=0, accepted | AX=0, accepted, halted |

The bottom row's `accepted` flags read 0, 1, 1, 1, which is {1, 2, 3}, the problem. One detail of the circuit shows here: an accept leaves the core running, since only `decr` at 0 sets `halted`. Cores 1 and 3 accept on line 3 and halt on line 4, and `accepted` stays 1, because the flags only ever turn on. The interpreter stops at the accept instead; the bottom row agrees either way.

### The tableau of every program

Now leave the program free. The same grid, as the SAT encoding builds it for {1, 2, 3}, has in each cell the four wires of one core's state, written `hb lb · halted accepted`:

- **The first two are `AX`**, as two bits: `0 0` is 0, `0 1` is 1, `1 0` is 2, `1 1` is 3. The start row is each core's input in binary: core 0 starts at `0 0`, core 1 at `0 1`, core 2 at `1 0`, core 3 at `1 1`.
- **The last two are the verdict so far**, `halted` and `accepted`, both 0 at the start. They are there because the circuit runs to the end: all six steppers run in every core, even after the program has accepted or rejected. The flags carry "this run already accepted" or "already halted" down to the bottom row, where the comparators read them; `AX` alone has only its value to carry.

A 1 or 0 is a wire whose value is fixed whatever the program, and `vN` is the formula's variable N, a wire whose value depends on the program. Variables 1 to 12 are the program's bits, two per line.

| step | line's bits | core 0 | core 1 | core 2 | core 3 |
|---|---|---|---|---|---|
| start | | 0 0 · 0 0 | 0 1 · 0 0 | 1 0 · 0 0 | 1 1 · 0 0 |
| 1 | v1 v2 | 0 0 · v15 v14 | 0 v68 · 0 0 | v67 v15 · 0 0 | v272 v68 · 0 0 |
| 2 | v3 v4 | 0 0 · v26 v25 | 0 v75 · v72 v71 | v150 v151 · v141 v138 | v295 v296 · v286 v283 |
| 3 | v5 v6 | 0 0 · v36 v35 | 0 v88 · v86 v85 | v180 v181 · v173 v172 | v325 v326 · v318 v317 |
| 4 | v7 v8 | 0 0 · v46 v45 | 0 v101 · v99 v98 | v210 v211 · v203 v202 | v355 v356 · v348 v347 |
| 5 | v9 v10 | 0 0 · v56 v55 | 0 v114 · v112 v111 | v240 v241 · v233 v232 | v385 v386 · v378 v377 |
| 6 | v11 v12 | 0 0 · v66 v65 | 0 v127 · v125 v124 | v270 v271 · v263 v262 | v415 v416 · v408 v407 |

Each variable here is a gate's output, and its three clauses tie it to the cell above and to its row's two program bits. Some cells are constants the whole way down, and the constants carry facts. Core 0 starts at `AX` = 0, and every opcode keeps 0 at 0 or halts, so its `AX` bits are the constant 0 in every row, and the encoder folds them away. Core 1 starts at 1, and its `hb` stays 0 because `AX` can only fall. Some literals repeat across cores: `v15` is core 0's `halted` after line 1 and core 2's `lb`, since both are the same function of `v1` and `v2`: line 1 is `decr`. The encoder builds that gate once and reuses it. Setting `v1` to `v12` to the program above and propagating fills every cell with the first table's values; the last four clauses then require the bottom row's `accepted` literals, `v65`, `v124`, `v262` and `v407`, to be 0, 1, 1 and 1.

## Cook-Levin as SAT

`src/sat.c` writes each problem as one Boolean formula in DIMACS CNF, the input format of SAT solvers (`8de51b7`), whose satisfying assignments are exactly the programs that decide the problem: the Cook-Levin construction itself. Expanded as polynomials, the circuit's `accepted` alone reached 10,324 terms ({{chapter The polynomials, in full}}). This chapter takes the other way of {{chapter Six expressions, composed}} and names every wire instead: each gate's output gets a variable, tied to the gate's inputs by three clauses, so the formula grows linearly with the steps.

### One gate, three clauses

A clause is an OR of literals, each a variable or its complement, and a CNF formula is an AND of clauses; in DIMACS, `-14 2` says "v14 is 0 or v2 is 1". The gate g = a AND b becomes `-g a`, `-g b` and `g -a -b`: g forces a, g forces b, and a and b together force g. `src/sat_hand_main.c`, run at the book's build, checks the eight rows: the three hold together exactly where g equals a AND b.

| a | b | g | `-g a` | `-g b` | `g -a -b` | all three | a AND b |
|---|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 1 | 1 | 1 | 1 | 0 |
| 0 | 0 | 1 | 0 | 0 | 1 | 0 | 0 |
| 0 | 1 | 0 | 1 | 1 | 1 | 1 | 0 |
| 0 | 1 | 1 | 0 | 1 | 1 | 0 | 0 |
| 1 | 0 | 0 | 1 | 1 | 1 | 1 | 0 |
| 1 | 0 | 1 | 1 | 0 | 1 | 0 | 0 |
| 1 | 1 | 0 | 1 | 1 | 0 | 0 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

The OR gate g = a OR b is the mirror image: `g -a`, `g -b` and `-g a b`. Each AND and OR becomes a Tseitin variable, tied to its inputs by three clauses, and a NOT is a literal's sign:

{{lines src/sat.c from "Lit sat_and(Cnf *c, Lit x, Lit y) {" to "Lit c3[3] = {out, -a, -b};"}}

### One stepper

The formula's first 12 variables are the program's bits: line k's `hb` is v(2k − 1) and its `lb` v(2k), so `nada` is 0 0, `zero` 0 1, `decr` 1 0 and `mod2` 1 1. The logic is the stepper's, gate for gate, NOT, AND and OR as in {{chapter Arithmetization}}. A gate on a constant is folded, and a gate already built from the same two inputs is reused. Core 0 starts with `AX` = 0 and both flags 0, so its state after line 1 reads two gates:

| wire | gate | its clauses | 1 when line 1 is |
|---|---|---|---|
| v14 | -v1 AND v2 | `-14 -1` `-14 2` `14 1 -2` | `zero` |
| v15 | -v2 AND v1 | `-15 -2` `-15 1` `15 2 -1` | `decr` |

```text
core 0 after line 1: hb 0, lb 0, halted v15, accepted v14
```

Core 0 is accepted after line 1 exactly when line 1 is `zero`, and halted exactly when it is `decr`; its `AX` stays the constant 0. These are the names in the symbolic tableau of {{chapter The tableau, filled in}}. Core 1 starts with `AX` = 1. Its unit reads three of line 1's four decoders, built once for all four cores, and adds two OR gates:

| wire | gate | its clauses | 1 when line 1 is |
|---|---|---|---|
| v13 | -v2 AND -v1 | `-13 -2` `-13 -1` `13 2 1` | `nada` |
| v14 | -v1 AND v2 | `-14 -1` `-14 2` `14 1 -2` | `zero` |
| v16 | v1 AND v2 | `-16 1` `-16 2` `16 -1 -2` | `mod2` |
| v67 | v13 OR v14 | `67 -13` `67 -14` `-67 13 14` | `nada` or `zero` |
| v68 | v16 OR v67 | `68 -16` `68 -67` `-68 16 67` | `nada`, `zero` or `mod2` |

```text
core 1 after line 1: hb 0, lb v68, halted 0, accepted 0
```

Core 1's `AX` falls to 0 exactly when line 1 is `decr`.

### The unrolling

Each of the six lines, on each of the four cores, is such a stepper, wired to the state the stepper above it left, and the last four clauses ask that each core's final `accepted` be what the problem wants. Every problem's formula has 416 variables and 1,216 clauses (`out/tcc/sat facts`, run for this book). `len=N` forces the lines past N to `nada`, two unit clauses a line, and `fix=K:OP` fixes line K.

### The proof by propagation

**Theorem 12.** For each of the 16 problems, the satisfying assignments of its formula, read as programs by their first 12 variables, are exactly the programs that decide it. With the lines past N forced to `nada`, the formula is satisfiable exactly when N is at least the problem's shortest length (Theorem 4).

*Proof.* `tests/test_sat.c` proves the encoding by itself. For every problem it sets the program bits to each of the 4,096 programs and unit-propagates them through the formula. Each gate's inputs come before it, so propagation alone decides every Tseitin variable. The assignments that survive are exactly the programs the machine says decide the problem, 4,096 over the 16 problems (`every_puzzles_models_are_exactly_its_solving_programs`), and with a length bound, exactly those that short (`a_length_bound_admits_the_solving_programs_that_short`).

### rocsat as an oracle

`sat/oracle.shale` hands the formulas to [rocsat](https://github.com/mpizenberg/rocsat), Matt Pizenberg's CDCL SAT solver, written in Roc (`f977bcc`). For each problem it counts the models by blocking each one and solving again until UNSAT, runs each model's program on the machine, and proves the shortest length: SAT at it, UNSAT one line shorter, with a DRAT proof the repository's checker replays. That is Theorem 4 again, by Theorem 12. It starts rocsat 4,143 times, so it runs as its own script; its last run, on 2026-11-25, spent 62.6 s in its programs and ended "rocsat agrees with the machine on all 16 puzzles", with 4,096 models and all 15 proofs verified.

## Solved by hand

With the constants folded, the smallest problem's formula solves on paper, and the start of the hardest problem's proof reads as plain statements about the program.

### {0} in one line

`is_tiny` accepts exactly {0} (code 1), and its shortest program has one line (Theorem 4). With lines 2 to 6 forced to `nada` (`sat 1 len=1`), the formula opens with ten unit clauses, `-3` to `-12`, and ends with four, one an input. Unit propagation repeats one rule: a clause whose literals are all false but one makes that one true. From the formula's own unit clauses, `src/sat_hand_main.c` finds that the rule decides every variable:

```text
{0} at length 1: 416 variables, 1226 clauses; unit propagation fixes 416 of the variables in 9 passes, with 0 conflicts
v65 = 1 by `65`: core 0's accepted after line 6
v55 = 1 by `-65 55 62`: core 0's accepted after line 5
v45 = 1 by `-55 45 52`: core 0's accepted after line 4
v35 = 1 by `-45 35 42`: core 0's accepted after line 3
v25 = 1 by `-35 25 32`: core 0's accepted after line 2
v14 = 1 by `-25 14 22`: core 0's accepted after line 1
v1 = 0 by `-14 -1`: line 1's hb
v2 = 1 by `-14 2`: line 1's lb
the program: line 1 is `zero`, lines 2 to 6 `nada`
```

Input 0's unit clause sets core 0's final `accepted`, v65. Each line's `accepted` is an OR of the one above and that line's new accept, which is 0 on a `nada` line (v62 at line 6, and so on), so each OR's third clause passes the demand one line up, to v14, line 1's `zero` decoder. Its two clauses `-14 -1` and `-14 2` then set the program's bits. rocsat agrees: `sat/proofs/1-len1.model` records 0 conflicts and 0 decisions.

### The other three ops

Run through the gates, each other op on line 1 leaves core 0's final `accepted` at 0 and breaks the unit clause `65`:

| line 1 | accepted after line 6, inputs 0 1 2 3 | {0} asks for | unit clauses broken |
|---|---|---|---|
| `nada` | 0 0 0 0 | 1 0 0 0 | `65` (input 0) |
| `zero` | 1 0 0 0 | 1 0 0 0 | all four hold |
| `decr` | 0 0 0 0 | 1 0 0 0 | `65` (input 0) |
| `mod2` | 0 0 0 0 | 1 0 0 0 | `65` (input 0) |

`nada` and `mod2` keep core 0 at 0 with both flags 0, and `decr` at 0 halts it.

### Five lines for {0, 2, 3}

`is_not_one` accepts {0, 2, 3} (code 13), and its one program uses all six lines (Theorem 5). For the programs of `nada`, `zero` and `decr`, counting lines settles it. Input 0 needs a `zero` before any `decr`, since `decr` at 0 halts. Input 2 reaches 0 after two `decr`s and input 3 after three, on different lines, and each then needs a `zero`. Input 1 reaches 0 after the first `decr`, so the second `decr` has to come before any later `zero`, and it halts input 1. That is three `zero`s and three `decr`s: six lines. Theorem 3's search, over all 4,096 programs, settles the ones with `mod2` too.

The formula says the same. `sat 13 len=5` is the full formula plus the unit clauses `-11` and `-12`, and rocsat answers UNSAT with a DRAT proof. Such a proof is a list of lemmas, each a clause that follows from the formula and the lemmas before it by unit propagation, with deletions mixed in, ending at the empty clause; a checker replays it, and once the empty clause follows, the formula is unsatisfiable. The first 12 distinct lemmas of this one, each literal named by `src/sat_hand_main.c` from the encoder's own record of its wires:

```text
`-11 0`: v11 = 0 (line 6's hb)
`-12 0`: v12 = 0 (line 6's lb)
`57 0`: v57 = 1 (1 when line 6 is `nada`)
`-58 0`: v58 = 0 (1 when line 6 is `zero`)
`-59 0`: v59 = 0 (1 when line 6 is `decr`)
`-60 0`: v60 = 0 (1 when line 6 is `mod2`)
`61 0`: v61 = 1 (v56 OR v57)
`-62 0`: v62 = 0 (-v56 AND v58)
`-63 0`: v63 = 0 (-v56 AND v59)
`-64 0`: v64 = 0 (-v56 AND v60)
`55 -65 0`: v55 = 1 (core 0's accepted after line 5), or v65 = 0 (core 0's accepted after line 6)
`56 -66 0`: v56 = 1 (core 0's halted after line 5), or v66 = 0 (core 0's halted after line 6)
```

The first two restate the length bound, and the next four decode line 6 as `nada`. v61 to v64 say which op core 0 runs on line 6, v56 being its `halted` after line 5: it runs `nada`. The last two say that core 0's flags after line 6 are those after line 5. {{chapter The solver's work, in full}} prints the whole proof, 286 lemmas, and checks it.

## The solver's work, in full

This chapter prints what rocsat was given and what it answered for two problems, whole, the way {{chapter The polynomials, in full}} prints the polynomials. Over each box are the counts that the book's app (`book/app/SatFiles.roc`) makes from the text it shows. The chapter states one count for each box again in its source, and the build stops if the text has another.

### Where the files come from

The formulas come from the repository's C: `book/book.shale` builds `src/sat_main.c` at each build of the book and writes them to `book/build/sat/`. rocsat's answers and proofs are data files in `sat/proofs/`, which `sat/oracle.shale` writes on every run (`19bff9e`). Two runs of the oracle on 2026-11-25 wrote the same bytes, and the 15 proofs match an earlier run's byte for byte, so rocsat's search is deterministic, and the book builds from the committed files alone. Before it shows them, the build checks each one again: every proof with the repository's DRAT checker, `src/drat.c`, against the formula it has just written, and every answer against its formula's clauses and, as a program, on the machine.

### The checker

A DRAT proof is a list of lines, each a clause added, a lemma, or a clause deleted, after a `d`. `src/drat.c` replays it forward. Each lemma has to follow by reverse unit propagation: set all its literals false, propagate over the formula and the lemmas so far, and some clause must come out with every literal false. Each deletion has to name a clause the database holds, and it is applied. The proof ends at the empty clause, which follows when propagation over the database alone reaches a conflict. A lemma that fails this test may still pass as a RAT on its first literal; all 15 of rocsat's proofs pass by the first test alone.

`tests/test_drat.c` holds the checker both ways in 12 tests: hand-made proofs that pass, planted ones that fail (a lemma the formula leaves open, a proof that stops short of the empty clause, a deletion of a clause a later lemma needs), and each committed proof, which verifies against its formula and fails against the formula one line longer.

### {0, 2, 3}, the problem with one answer

`is_not_one` accepts exactly {0, 2, 3} (code 13), and one program of the 4,096 solves it (`tests/golden/find_solutions.txt`). Here is its formula, `sat 13`, in full. The `c` lines at its top name the variables: the program's 12 bits, then each input's state after each step as the literals that carry it, `T` and `F` where folding left a constant.

```dimacs book/build/sat/13-len6.cnf
the CNF of {0, 2, 3}, written by src/sat_main.c at the book's build
1216
```

rocsat's answer is its statistics line, its verdict and a value for every variable:

```dimacs sat/proofs/13-len6.model
rocsat's answer to that CNF, as sat/oracle.shale kept it
416
```

Instruction i is 2·v[2i − 1] + v[2i], so the first 12 values read as program 1689 (`zero, decr, decr, zero, decr, zero`). The build decodes the program from the answer and holds this sentence to it, and `sat solves 13` confirms on the machine that the program accepts exactly {0, 2, 3}.

### Five lines are too few

With line 6 forced to `nada` (`sat 13 len=5`), the formula is the one above with two unit clauses at its top, 1,218 clauses in all, and {{chapter Solved by hand}} reads the start of rocsat's proof. Here it is whole:

```dimacs sat/proofs/13-len5.drat
rocsat's DRAT proof that the CNF of {0, 2, 3} at length 5 is unsatisfiable
286
```

The checker, run on it at the book's build, says `verified: 286 lemmas (0 by RAT), 387 deletions, over 416 variables and 1218 clauses; the empty clause at line 673`. A program of fewer than five lines has its line 6 at `nada` too, so the formula covers every program of five lines or fewer, and the proof shows that each one decides another problem. No program of five or fewer lines decides {0, 2, 3}. Its shortest program is the six-line one above.

### The largest proof

Of the 15 proofs, the largest by bytes is that of `is_big`, {2, 3} (code 12). Its shortest program has five lines, and this proof shows that four are too few:

```dimacs sat/proofs/12-len4.drat
rocsat's DRAT proof that the CNF of {2, 3} at length 4 is unsatisfiable
490
```

The checker says `verified: 490 lemmas (0 by RAT), 788 deletions, over 416 variables and 1220 clauses; the empty clause at line 1278`.

### All 15 proofs

Each problem whose shortest program has a line or more has one proof; `grumpy`, {} (code 0), is solved by the empty program. The build checks each proof again and holds each row to what it finds. By size:

| code | puzzle | problem | shortest | proof bytes | lemmas | verified |
|---|---|---|---|---|---|---|
| 12 | `is_big` | {2, 3} | 5 | 14,698 | 490 | yes |
| 14 | `is_not_tiny` | {1, 2, 3} | 5 | 13,743 | 463 | yes |
| 15 | `always_true` | {0, 1, 2, 3} | 4 | 12,170 | 485 | yes |
| 8 | `is_huge` | {3} | 4 | 10,992 | 452 | yes |
| 6 | `is_middling` | {1, 2} | 4 | 10,892 | 449 | yes |
| 1 | `is_tiny` | {0} | 1 | 10,847 | 821 | yes |
| 11 | `is_not_two` | {0, 1, 3} | 4 | 10,805 | 446 | yes |
| 9 | `is_extreme` | {0, 3} | 5 | 10,474 | 378 | yes |
| 5 | `is_even` | {0, 2} | 2 | 9,876 | 744 | yes |
| 7 | `is_not_huge` | {0, 1, 2} | 5 | 9,171 | 347 | yes |
| 13 | `is_not_one` | {0, 2, 3} | 6 | 9,012 | 286 | yes |
| 3 | `is_small` | {0, 1} | 3 | 8,890 | 691 | yes |
| 10 | `is_odd` | {1, 3} | 3 | 8,862 | 693 | yes |
| 2 | `is_one` | {1} | 2 | 8,731 | 566 | yes |
| 4 | `is_two` | {2} | 3 | 7,440 | 477 | yes |

The proofs run from 7,440 to 14,698 bytes and from 286 to 821 lemmas. The largest by bytes has fewer lemmas than five others, since its lemmas are longer.

## The oracles

Every implementation in this repository computes the same answers in its own way, and each is held to the others. The spine of the project is that they all agree.

Seven ways to the same answers
```dot
digraph {
  node [shape=box, style=rounded];
  answers [label="16 counts, 4 polynomials"];
  python [label="Python"];
  c [label="C"];
  roc [label="Roc"];
  sat [label="SAT, rocsat"];
  js [label="JavaScript x3"];
  shale [label="Shale"];
  nibble [label="Nibble's wasm"];
  python -> answers;
  c -> answers;
  roc -> answers;
  sat -> answers;
  js -> answers;
  shale -> answers;
  nibble -> answers;
}
```

### What each one checks

- **Python**, the original, is the source of `tests/golden/find_solutions.txt` and of the step polynomials' strings, `tests/golden/step_*.txt`.
- **C** prints that report byte for byte (`tests/test_golden.c`), checks the stepper against the interpreter on all 4,096 × 4 runs, and proves the composition at all 2^14 points (`tests/test_compose.c`). `shale build.shale` ran its fast tier for this book: eleven test files and the golden check, all passing.
- **Roc** prints the same report, the four step polynomials as the C's `bp_str` prints them (`roc/golden/step_polynomials.txt`) and the composition's counts and proof (`roc/golden/composition.txt`). `roc/check.shale` is its gate.
- **SAT** reaches the 16 counts through `tests/test_sat.c`'s propagation and again through rocsat's blocked models, and each shortest length through an UNSAT, with a DRAT proof that `src/drat.c` verifies, and a SAT.
- **JavaScript**, `js/cook_levin.js`, prints the report and then the composition, byte for byte the two goldens, under three runners.
- **Shale**, `shale/cook_levin.shale`, prints the same report and composition, held to the two goldens byte for byte by `shale/check.shale`.
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

## What is Shale?

Shale is a small scripting language for working on code, Lua-like on the surface, and the language of every script in this repository and of the port in the next chapter. This chapter describes the language itself, from its own documentation, and leaves Cook-Levin aside; a reader here for the machine can skip ahead to {{chapter The VM in Shale}}.

### What it is for

Shale lives in a public repository of its own, [showell/shale](https://github.com/showell/shale), under the same license as this one, the Universal Permissive License 1.0. Its `README.md` opens: "Shale is a small scripting language for working on code. One line greps a whole repo. A short script refactors one. Every run is budgeted, confined, readable. Agents write it in place of bash, in place of Python." The same README says its code was written by AI coding agents (Claude), with human supervision, as this repository's says of its own.

Its builtins cover the common Linux work, by the README's list: reading, editing and searching files, walking folders, running programs and background jobs, git, text and JSON. A task that would be a pipeline of `grep`, `sed` and `sort` in a shell is a short script, launched as a heredoc (`shale <<'EOF'`) or as a file with `name=value` arguments, which the script reads as `arg("name", default)`. In this repository, `build.shale` runs the C's tiers, `roc/check.shale` and `js/oracle.shale` hold the Roc and JavaScript ports to the goldens, `sat/oracle.shale` drives rocsat, and `book/book.shale` builds this book.

### How a script runs

A C runtime reads each script, compiles it to bytecode and runs it on a small virtual machine. The README says the whole runtime "builds as one C file, links only the C library, and weighs under a megabyte", and runs on Linux x86-64. The first implementation was in Zig; it is frozen in the repository's `legacy_zig/`, and the C was written against it as its oracle, with golden tests, each input frozen with its output, as the judge of behaviour.

Every run has budgets: steps, reads, bytes read, bytes printed, and the arena, the memory the run's values live in. `docs/SHALE.md` gives the defaults as 5,000,000 steps, 10,000 reads, 64 MiB read and a 16 MiB arena, and the README gives 64 KiB of output. A step is each statement, expression and loop iteration, and every byte or item a builtin touches. A run that reaches a budget stops and says which one. `--budget NAME=N` raises one on the command line, and a script that always needs more says so in a `-- budget:` line among its opening comments, as the port's first line does.

A stop is one line that says where and why, with a class (`type`, `index`, `refused`, `budget reads`) and the calls that led there. An argument is read by the script's `arg` calls, and one whose name they leave unread, a misspelled one, stops the run before it starts. Past half of any budget, or on a stop, the runner prints what the run used; `--verbose` prints it always, and ends with the step count that the port's check reads.

### What it refuses

The README's guard rails come down to four rules.

- **A run is confined.** A script reads and writes only under the folder it runs in and the mounts it is given (`name:path`); `..` and absolute paths are refused. A program it starts is held to the same folders by the kernel's Landlock, and so is everything that program starts.
- **A program runs bounded.** `run` takes a timeout and a keep (`head`, `tail` or `whole`), so a hung test is stopped and a long build log comes back as its last lines.
- **Risky shortcuts are refused by name.** `run` refuses a shell or an interpreter (`bash`, `python3`, `node`), a text tool Shale already has (`grep`, `sed`), and a program at a path git leaves untracked. A `{want = "tag: why"}` option runs one anyway, and the run records it.
- **Git is the undo.** A run makes its changes as it goes, and each `edit` prints its diff. Working in a checkout, `git diff` shows what a run did, and `--undo-on-stop` puts back every file a stopped run changed.

The style is an allow-list: the language and its runner name what they permit, and a refusal names what was asked for and what to write instead.

### The language

`docs/SHALE.md` says that Shale "looks like Lua, with smaller semantics of its own". Its integers are 64-bit and checked, so an overflow stops the script. Lists count from 1. A function is declared at the top level, above the code that calls it, and `const` names a top-level value made once. Reading a map's missing key stops the run, and `get(m, k, default)` asks for a default instead; a list that would hold `nil` is a `type` error.

A long list is refused, each item by name: floats and `/`; `^`, hex literals and the bitwise operators; function values, closures, nested functions and `local function`; `:` method calls, `...`, `goto` and `repeat`; multiple assignment and multiple returns; and Lua's library, `string.`, `pairs` and the rest. What remains is integers, strings, booleans, lists and maps, loops, and functions by name, with a large set of builtins beside them. {{chapter The VM in Shale}} is the machine and its mathematics in that much.

## The VM in Shale

`shale/cook_levin.shale` is the machine, the search over all 4,096 programs, the step polynomials and the six-step composition once more, in Shale (`4942a49`). It follows `js/cook_levin.js` closely, and it prints the same two reports, byte for byte: `tests/golden/find_solutions.txt`, then `roc/golden/composition.txt`. `part=search` prints the first alone and `part=composition` the second.

### The machine

A machine's state is a map of two fields, its `AX` and how its run has ended so far, the three endings named by string constants. One step is the rule of {{chapter The machine}}:

{{function shale/cook_levin.shale machine_step}}

A program is a list of six opcodes, the base-4 digits of its number, lowest first (`disassemble`). A run starts with `AX` at the input and folds `machine_step` over the list.

### The search

The search runs each of the 4,096 programs on the four inputs, makes its problem's code from the inputs it accepts, and files the program under that code. Lists count from 1, so problem `y`'s programs are at slot `y + 1`:

{{lines shale/cook_levin.shale from "function find_solutions()" to "if #programs[y + 1] == 0 then fail"}}

It then prints each problem's count and first program, and runs that program on the step polynomials too, failing the run where they disagree with the machine.

### The ring's times

A polynomial of the Boolean ring is a map of two lists, `masks` and `coeffs`, its terms in ascending order of mask. A monomial is a set of variables, bit `i` of its mask for variable `i`, and the product of two monomials is the OR of their masks. Shale refuses the bitwise operators, so the OR comes from a table. Every mask is below 2^14, so it splits into two 7-bit halves, and a table of the OR of every two 7-bit numbers, 16,384 entries, is made once:

{{function shale/cook_levin.shale half_table}}

{{function shale/cook_levin.shale or14}}

The product of two polynomials takes every pair of terms. Like terms are summed in `SUMS`, one dense list of 16,384 slots indexed by mask, the Roc port's table, and `from_sums` reads it back in order and puts each slot back to 0. `q`'s halves are split once, before the pairs, so each pair costs two reads of `OR7`:

{{function shale/cook_levin.shale times}}

### The step polynomials

The step is built in the ring as {{chapter Arithmetization}} builds it: NOT x is 1 − x, x AND y is x·y and x OR y is x + y − x·y.

{{lines shale/cook_levin.shale from "function negation(x)" to "function either(x, y)"}}

`next_state` takes the six variables and builds the step from them, starting with which value `AX` holds and which opcode runs:

{{lines shale/cook_levin.shale from "local is0 = times(negation(hb), negation(lb))" to "local runs_mod2 = times(times(op_hb, op_lb), negation(halted))"}}

Its four answers, at variables 0 to 5, are the step polynomials. Each one's value at all 64 points is computed once, so the stepper's step is four reads of those tables.

### The composition

After step k the state is four polynomials in 14 variables. The next step substitutes them into the step polynomials. Each monomial of the step's six variables becomes the product of its variables' replacements, made from the product of the monomial less its highest variable and kept for the rest of the step. A list holding `nil` is refused, so `PRODUCTS` starts as 64 zeros and a second list, `MADE`, marks each slot that has been filled:

{{function shale/cook_levin.shale product_of}}

`substitute` is a builtin already, a text replacement, and a function by a builtin's name is refused, so the substitution is `substituted`. Every product is made first, since a product is itself summed in `SUMS`, and then each term's coefficient times its product is summed in:

{{function shale/cook_levin.shale substituted}}

The proof then takes each final polynomial's value at all 16,384 points by the zeta transform, as the Roc port does, and holds every point to the machine.

### The check

`shale/check.shale` (`ac3ee9f`) runs the port twice, each run a child `shale` with `--verbose`, niced, and holds each run's output byte for byte to its golden, leaving out the runner's own lines. Its run for this book, on 2026-11-25, printed:

- `part=search`: 4,712 bytes, the golden's, in 210 ms and 13,162,754 steps.
- `part=composition`: 401 bytes, the golden's, in 5,002 ms and 396,539,820 steps, ending in the proof's line: 16,384 points, 0 differing.

By `README.md`'s breakdown, of the composition's 397 million steps, the tables and the step polynomials take about 8 million, steps 1 to 5 about 22 million, step 6 about 193 million, and the proof about 174 million, at about 70 million steps a second.

### What the port cost

- **Memory.** By `README.md`, a run's values are freed only when it ends, so every polynomial the composition makes on its way is held, and it needs between 100 and 300 MB of arena, against 16 MiB by default. The port's `-- budget:` line gives it 400 MB and a billion steps. Shale's `yield()` copies the values still reachable into an arena of their own, at most 256 MiB, and lets the rest go; the port has yet to try it.
- **Bits.** The OR and AND of masks are two 16,384-entry tables and two reads each, and a single bit is a `//` and a `%`.
- **Lists.** A list holds values only, so an unfilled slot is marked in a list beside it.
- **Names.** `substitute` is a builtin and `not` a keyword, so the port's names are `substituted` and `negation`.
- **Time.** The composition and the proof take about 5 s here, where the Roc release build takes about 30 ms ({{chapter The Roc port}}) and node runs the JavaScript in about 0.3 s ({{chapter The oracles}}).

## What comes next

A few steps stand open, each small:

- **The C's dense table.** The C composition could sum like terms in the Roc port's dense table, and its time would then measure the language.
- **The compiled JavaScript.** Its slowness is unprofiled. `README.md` guesses that an array's items are values of unknown kind to the compiler, so each read and write passes the runtime's tag checks.
- **Nondeterminism.** The machine is deterministic. The SAT encoding already treats the program as the unknown, which is the theorem's "a certificate exists"; a machine with choice instructions would make the riff closer to the theorem.
