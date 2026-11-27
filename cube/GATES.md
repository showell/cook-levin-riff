# The cube as gates

One roll of the rolling cube as a circuit of AND, OR and NOT gates, then k
rolls chained into one circuit, and that circuit turned into CNF by
Tseitin's encoding. `cube_sat.c` writes the same question ("is this level
solved in exactly k rolls?") as CNF by hand; this page puts the two side by
side.

| file | what it is |
|---|---|
| `gates.h`, `gates.c` | the one-roll circuit, on `src/circuit.h`'s netlist; the k-roll chain; its CNF |
| `gates_main.c` | the command line: `count`, `cnf`, `size`, `dot` |
| `../tests/test_cube_gates.c` | the checks below, in `build.shale`'s fast tier |
| `gates_sat.shale` | both CNFs on rocsat, every answer checked; writes `sat/gates.txt` |

```sh
out/tcc/cube_gates count 4              # the gates, part by part
out/tcc/cube_gates cnf 4 33825 18       # the diagonal at par, by gates
out/tcc/cube_gates dot 4                # the parts as boxes, Graphviz
shale cube/gates_sat.shale              # both encodings on rocsat
```

## The encoding: one-hot

| choice | wires on 4 × 4 | what a roll is |
|---|---|---|
| **one-hot (chosen)** | 16 cells + 6 faces + 16 tiles + halted = 39 | wiring: each cell's wire comes from its neighbour's, each face's from the face that turns into it |
| binary | 4 cell bits + 3 face bits + 16 tiles + halted = 24 | arithmetic: a row or column counter up or down, a borrow for the edge, a 6-entry table for the face, a decoder to find the tile |

One-hot is chosen because every rule becomes a wire a reader can trace. A
roll north carries the cube from the cell to the south; nothing is added
or compared. The paint's turn is a permutation of six wires. The tile to
flip is the cell wire itself, so no decoder is needed. The cost is more
state wires, and so a bigger CNF, which the sizes below measure.

A state's wires, in the circuit's input order:

| wires | count | on when |
|---|---|---|
| cell a1 … | n² | the cube is on that cell (exactly one) |
| face up, down, north, south, east, west | 6 | the painted face points that way (exactly one) |
| tile a1 … | n² | the tile is lit |
| halted | 1 | a roll off the grid has halted the machine |
| roll hi, roll lo | 2 | the roll: N 00, E 01, S 10, W 11 |

## The parts

| part | what it computes |
|---|---|
| decode | the roll's two bits as four wires N, E, S, W, exactly one on |
| halt | off = OR over the four sides of (roll toward that side AND the cube on that edge); halted' = halted OR off; run = NOT halted'; go r = roll r AND run |
| move | cell c' = OR over each roll r of (go r AND the cell r comes from), OR (cell c AND halted'): the cube arrives from a neighbour or stays put |
| paint | face g' = OR over the four rolls of (go r AND the face r turns into g), OR (face g AND halted') |
| flip | land = face down' AND run; tile c' = tile c XOR (cell c' AND land), the XOR as (a OR b) AND NOT (a AND b) |

A neighbour off the grid is simply no term: a corner cell has two
arrivals, an edge cell three, an inside cell four. The geometry in
`gates.c` is written from rows and columns, with a table of the paint's
turns, not from `cube.c`. So the check against `cube.c` compares two
separate writings of the rules.

How the parts are wired, wires a line, on n × n:

| from | to | wires |
|---|---|---|
| roll | decode | 2 |
| decode | halt | 4 |
| cell (the edge cells), halted | halt | 4(n − 1), 1 |
| halt (go N, E, S, W and halted') | move, paint | 5 each |
| cell, face | move, paint | n², 6 |
| move (cell'), paint (face down'), halt (run), tile | flip | n², 1, 1, n² |
| move, paint, flip, halt | cell', face', tile', halted' | n², 6, n², 1 |

`out/tcc/cube_gates dot N` writes the same picture as Graphviz source, from
the links the circuit records (`net_boxes_dot`).

## Gate counts

With A = 4n(n − 1), the number of (cell, roll) pairs that stay on the grid:

| part | NOT | AND | OR | formula | 2 × 2 | 3 × 3 | 4 × 4 | 5 × 5 |
|---|---|---|---|---|---|---|---|---|
| decode | 2 | 4 | 0 | 6 | 6 | 6 | 6 | 6 |
| halt | 1 | 8 | 4n | 4n + 9 | 17 | 21 | 25 | 29 |
| move | 0 | A + n² | A | 2A + n² | 20 | 57 | 112 | 185 |
| paint | 0 | 30 | 24 | 54 | 54 | 54 | 54 | 54 |
| flip | n² | 3n² + 1 | n² | 5n² + 1 | 21 | 46 | 81 | 126 |
| **one roll** | | | | | **118** | **184** | **278** | **400** |

On 4 × 4 that is 19 NOTs, 155 ANDs and 104 ORs. `tests/test_cube_gates.c`
holds every count for n from 2 to 5.

## The checks

| check | cases | what must hold |
|---|---|---|
| one roll, every 3 × 3 state | 9 cells × 6 faces × 512 tile patterns × 2 (halted) × 4 rolls = **221,184** | the circuit's next state is `cube_step`'s, every wire |
| one roll, every 4 × 4 state | 16 × 6 × 65,536 × 2 × 4 = **50,331,648** | the same |
| the gate counts | n = 2 to 5 | the table above |
| the chain on every 6-roll 3 × 3 program | 4,096 | last cell, face, tiles and halted are `cube_play`'s; the output is true exactly when the tiles are c3 alone |
| the chain on the census's solutions | the diagonal, border, checkerboard and every tile (4 × 4, 18 to 47 rolls) | the output is true |
| the chain on random 45-roll 4 × 4 programs | 2,000, most halting | the last state is `cube_play`'s |
| the CNF on every 6-roll 3 × 3 program | 4,096 | unit propagation from the roll bits alone sets every variable to the circuit's value, with a conflict exactly when the program fails |

The state checks run 64 states at once, one bit of a 64-bit word each: the
64 lanes are the settings of tiles 0 to 5. Every lane is also stepped by
`cube_step` on its own. The whole file takes about 3.7 s under tcc.

## The CNF

The chain is k copies of the one-roll netlist. Each copy's state inputs
are wired to the copy before it, and its roll bits are the chain's inputs.
A match part ANDs the last tiles, each one negated where the target is
dark, into one output wire. Tseitin's encoding then gives:

| what | variables | clauses |
|---|---|---|
| roll bits | 2k, roll t at 2t − 1 (hi) and 2t (lo), as in `cube_sat.c` | |
| the start state's wires | 2n² + 7 | one unit clause each (a1, face up, dark, running) |
| each AND and OR gate | one | three |
| each NOT gate | none: its input's literal, negated | none |
| the output | (a gate) | one unit clause |

So on 4 × 4 each roll adds 261 variables (its 2 roll bits and 259 gates)
and 777 clauses, against 61 variables and 434 clauses a roll in
`cube_sat.c` (the differences between k = 45 and 47 below, halved).

### Sizes against cube_sat.c

| grid | k | `cube_sat.c` vars | clauses | gates vars | clauses | vars × | clauses × |
|---|---|---|---|---|---|---|---|
| 3 × 3 | 26 | 1,065 | 6,144 | 4,557 | 13,466 | 4.3 | 2.2 |
| 4 × 4 | 8 | 527 | 3,527 | 2,142 | 6,301 | 4.1 | 1.8 |
| 4 × 4 | 18 | 1,137 | 7,867 | 4,752 | 14,071 | 4.2 | 1.8 |
| 4 × 4 | 32 | 1,991 | 13,943 | 8,406 | 24,949 | 4.2 | 1.8 |
| 4 × 4 | 45 (L) | 2,784 | 19,585 | 11,799 | 35,050 | 4.2 | 1.8 |
| 4 × 4 | 47 | 2,906 | 20,453 | 12,321 | 36,604 | 4.2 | 1.8 |
| 5 × 5 | 28 | 2,521 | 21,446 | 10,553 | 31,378 | 4.2 | 1.5 |

The hand encoding is smaller because it states each rule as a clause
directly: an XOR in 4 clauses with no helper variable, where the gates take
3 gates and 9 clauses; a move as one implication, where the gates build an
AND and an OR; and the one-hot rules as pairwise at-most-one clauses, which
the circuit gets for free.

## On rocsat, against cube_sat.c

`gates_sat.shale` ran both encodings on rocsat at each level's par and at
par − 1, with 5 minutes a case. Every SAT model's rolls solved its level
(`cube_sat check`) and its `v` lines passed `drat model`. Every UNSAT
proof passed `src/drat.c`, by RUP alone. The lines are in `sat/gates.txt`;
these are rocsat's CPU times on this box (2 cores, 2026-11-27, niced,
beside other work).

| level | par | SAT at par, hand | gates | UNSAT at par − 1, hand | gates |
|---|---|---|---|---|---|
| 3 × 3 c3 alone | 4 | 3 ms | 5 ms | 2 ms | 2 ms |
| 3 × 3 b2 alone | 8 | 9 ms | 24 ms | 14 ms | 30 ms |
| 3 × 3 a1 and b2 | 14 | 97 ms | 1.26 s | 100 ms | 0.59 s |
| 3 × 3 the diagonal | 16 | 0.21 s | 1.94 s | 0.19 s | 1.27 s |
| 3 × 3 the checkerboard | 22 | 0.12 s | 43.9 s | 0.65 s | 93.9 s (178 MB proof) |
| 3 × 3 every tile | 26 | 2.0 s | no answer in 300 s | 3.7 s | no answer in 300 s |
| 4 × 4 c2 alone | 3 | 4 ms | 8 ms | 3 ms | 3 ms |
| 4 × 4 d4 alone | 8 | 14 ms | 19 ms | 15 ms | 16 ms |
| 4 × 4 the diagonal | 18 | 0.38 s | 5.2 s | 0.69 s | 3.2 s |
| 4 × 4 the checkerboard | 32 | 7.9 s | no answer in 300 s | 67.8 s | no answer in 300 s |

The two encodings never disagree. Of the 20 cases, 16 got an answer from
both, each the census's; the other 4 are the gates' timeouts. The gates'
CNF is the same question, but rocsat finds it much harder: up to 360 times
slower on the 3 × 3 checkerboard's SAT, with proofs up to 120 times
bigger. The hand encoding states the one-hot facts and each XOR as clauses
directly. The circuit reaches them only through helper gates, which the
solver must learn its way through.
