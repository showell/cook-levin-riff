# The rolling cube

The third toy machine, after Nibble and Nibble++. Their shortest programs
turned out to write themselves (`docs/NIBBLE_PLUS_THEORY.md`). The cube is
a toy you can picture, and its puzzles are meant to have no recipe.

**The board is 4 × 4.** A program's meaning is only which tiles end lit:
one of 65,536 sets. Where the cube ends and which way its paint faces are
details of the machine, so the end is free. The 3 × 3 board is a small
check; 5 × 5 is where SAT takes over from brute force.

**The headline:** on 4 × 4, from a1 with the painted face up, all 65,536
lit sets are reachable (none unreachable), with pars from 0 to 47 (the
table below, "The census").

## The rules

| | |
|---|---|
| board | an n × n grid of tiles, all dark at the start |
| cells | columns a, b, c, … west to east; rows 1, 2, 3, … north to south; a1 is the north-west corner |
| the cube | one die-sized cube with one face painted |
| a roll | the cube tips over one bottom edge into the next cell: N, E, S or W |
| off the grid | a roll off the grid is refused, and the checker rejects that program |
| a landing | each time the cube lands with the painted face down, the tile under it flips (dark to lit, lit to dark) |
| the start | a1, painted face up; the start itself flips nothing |
| a level | n, the start, and the target: the set of tiles lit at the end, every other tile dark |
| the end | the cube may end anywhere |
| the question | can this level be solved in k rolls? |
| the certificate | the roll sequence; the checker rolls it and compares |

**A roll is two bits:** N 00, E 01, S 10, W 11. A k-roll program is 2k bits.

**The state is six faces, not 24 orientations.** The cube has 24
orientations, but only the painted face touches the tiles, and where it goes
on a roll depends only on where it is. So a state is (cell, where the painted
face points: up, down, N, S, E or W, the tile pattern):

| grid | states | patterns |
|---|---|---|
| 3 × 3 | 9 × 6 × 2^9 = 27,648 | 512 |
| 4 × 4 | 16 × 6 × 2^16 = 6,291,456 | 65,536 |
| 5 × 5 | 25 × 6 × 2^25 ≈ 5.0 billion | 33,554,432 |

Rolling north tips the cube over its north bottom edge: the top goes north,
north goes down, down goes south, south comes up; east and west stay. The
other three rolls are the same, turned.

**Where the cube ends** is free, so a pattern's par is the first level of
the search with that pattern at any cell and face.

## Files

| file | what it is |
|---|---|
| `cube.h`, `cube.c` | the rules: a roll's face and cell, `cube_play`, and the checker `cube_check` |
| `census.c` | the census: a breadth-first search over every state, a byte a state, from a1 face up |
| `golden/census3.txt`, `golden/census4.txt` | the census's reports, held by `build.shale` |
| `oracle.py` | the Python oracle, sharing no code with the C |
| `cube_sat.c` | the CNF of "solved in exactly k rolls?" and a checker command line |
| `sat.shale` | rocsat on the CNF, each answer checked; writes `sat/grid4.txt`, `sat/grid5.txt` |
| `../roc/Cube.roc`, `../roc/CubeWasm.roc` | the rules in Roc (`Cube.play`: a program's run frame by frame, or the roll that left the board), and the playground's wasm |
| `../web/cube.html`, `cube.js`, `cube_data.c`, `cube_check.mjs` | the playground (below) |

```sh
out/tcc/census 4                  # the 4x4 report
out/tcc/census 4 pars.txt         # the report, and every pattern's par and solution to pars.txt
out/tcc/census 4 starts           # one line for every start (cell, face)
shale cube/sat.shale grid=4       # the SAT cross-check; grid=5 for the 5x5 levels
```

## The checks

`build.shale`'s fast tier (every tier, in fact) builds `census.c`, runs it
on 3 × 3 and 4 × 4, and holds each report to its golden (8 ms and 2.5 s
under tcc). Each run also writes every pattern's par and solution, and
`oracle.py` checks that file:

| check | 3 × 3 | 4 × 4 |
|---|---|---|
| every solution rolled by a plain simulator: on the grid, exactly its pattern, the par's length | 512 | 65,536 |
| every par against a search on sets of patterns (one big integer a place, a flip a block swap) | 512 | 65,536 |
| every par against a plain search over tuples in a dict | 512 | too big for a dict |

So the oracle agrees with the census on every pattern of both grids, not
just a sample (about 0.1 s and 1.1 s). A par one less than the census's,
planted in the file, is refused. The census itself checks every solution it
prints with `cube_check`, and checks each pattern against the floor below.

## The playground

`web/cube.html` shows the board and the cube in 3D, on a canvas with a
perspective projection of its own (tiles drawn far to near, the cube last
with its back faces culled), and animates each roll as a quarter turn about
the cube's bottom edge. Rolls come from the N, E, S, W buttons, the arrow
keys or a typed program; Play, Back, Forward and Reset move through the
run. It shows the program's length, its lit set as a small map and the
census's par for that set ("par 8; yours is 8"), with four samples: ESWN,
b2 alone, the diagonal and every tile. Drag the board to turn it.

The rules are `roc/Cube.roc` built to wasm on `web/platform/cube.roc` (the
same C host as Nibble's pages), and the pars are the census's,
`cube_pars.bin`, a byte a lit set. `shale web/build.shale` builds it and
runs `web/cube_check.mjs`, which holds the wasm to `cube.c` on 400 programs
frame by frame, rolls all 65,536 census solutions through it, holds the
table to the census and plays the page in headless Chromium.

## The census

From a1, painted face up. The 4 × 4 board is the toy; 3 × 3 sits beside it
as the small check. Every state is reached, so every lit set is reachable:
there are no unreachable sets on either grid.

| | 3 × 3 | 4 × 4 |
|---|---|---|
| states reached | 27,648 of 27,648 | 6,291,456 of 6,291,456 |
| levels of the search | 32 | 51 |
| unreachable patterns | 0 | 0 |
| largest par | 26 (every tile lit) | 47 (every tile lit) |

### The par distribution

| par | 3 × 3 | 4 × 4 | | par | 4 × 4 |
|---|---|---|---|---|---|
| 0 | 1 | 1 | | 24 | 3,801 |
| 1 | 0 | 0 | | 25 | 4,706 |
| 2 | 2 | 2 | | 26 | 4,388 |
| 3 | 2 | 2 | | 27 | 4,802 |
| 4 | 1 | 1 | | 28 | 4,457 |
| 5 | 4 | 10 | | 29 | 4,443 |
| 6 | 10 | 14 | | 30 | 3,836 |
| 7 | 8 | 20 | | 31 | 3,598 |
| 8 | 12 | 35 | | 32 | 2,948 |
| 9 | 27 | 65 | | 33 | 2,378 |
| 10 | 22 | 96 | | 34 | 2,039 |
| 11 | 30 | 122 | | 35 | 1,375 |
| 12 | 38 | 232 | | 36 | 1,115 |
| 13 | 48 | 344 | | 37 | 738 |
| 14 | 39 | 407 | | 38 | 466 |
| 15 | 64 | 654 | | 39 | 309 |
| 16 | 40 | 867 | | 40 | 186 |
| 17 | 44 | 1,236 | | 41 | 94 |
| 18 | 44 | 1,310 | | 42 | 60 |
| 19 | 25 | 2,105 | | 43 | 28 |
| 20 | 24 | 2,209 | | 44 | 8 |
| 21 | 9 | 2,930 | | 45 | 8 |
| 22 | 8 | 3,084 | | 46 | 0 |
| 23 | 5 | 4,006 | | 47 | 1 |
| 24 | 4 | | | | |
| 25 | 0 | | | | |
| 26 | 1 | | | | |

### Par by the number of lit tiles

| lit | 3 × 3 min | max | mean | 4 × 4 min | max | mean |
|---|---|---|---|---|---|---|
| 1 | 2 | 8 | 4.67 | 2 | 8 | 5.12 |
| 2 | 5 | 14 | 7.83 | 5 | 14 | 8.73 |
| 3 | 8 | 16 | 10.64 | 8 | 17 | 12.06 |
| 4 | 11 | 18 | 13.22 | 11 | 21 | 15.19 |
| 5 | 14 | 22 | 15.63 | 14 | 25 | 18.16 |
| 6 | 17 | 20 | 18.19 | 17 | 28 | 21.01 |
| 7 | 20 | 22 | 20.64 | 20 | 30 | 23.77 |
| 8 | 23 | 24 | 23.44 | 23 | 33 | 26.46 |
| 9 | 26 | 26 | 26.00 | 26 | 34 | 29.09 |
| 10 | | | | 29 | 36 | 31.68 |
| 11 | | | | 32 | 37 | 34.26 |
| 12 | | | | 35 | 39 | 36.81 |
| 13 | | | | 38 | 41 | 39.38 |
| 14 | | | | 41 | 43 | 41.97 |
| 15 | | | | 44 | 45 | 44.50 |
| 16 | | | | 47 | 47 | 47.00 |

**The floor: m lit tiles take at least 3m − 1 rolls.** The first landing
face down takes 2 rolls (up, a side, down). After a landing face down, any
roll puts the painted face on a side, and from a side only one roll brings
it down again; two rolls from down to down go there and back, onto the same
tile. So each new tile costs at least 3 more rolls. The census meets the
floor for every m on both grids.

### Rolls over the floor

| over | 3 × 3 | 4 × 4 |
|---|---|---|
| 0 | 88 | 1,472 |
| 1 | 161 | 6,717 |
| 2 | 131 | 13,361 |
| 3 | 61 | 15,771 |
| 4 | 31 | 13,010 |
| 5 | 18 | 8,336 |
| 6 | 11 | 4,353 |
| 7 | 6 | 1,761 |
| 8 | 3 | 570 |
| 9 | 1 | 154 |
| 10 | | 28 |
| 11 | | 2 |

### Levels

| level | 3 × 3 par | a shortest solution | 4 × 4 par | a shortest solution |
|---|---|---|---|---|
| the start tile, a1 | 6 | ESSWNN | 6 | ESSWNN |
| the far corner | 4 | ESSE | 8 | ESWSEEES |
| the diagonal | 16 | ESENWSNESWSENNWW | 18 | ESSENNWWESESESNNWW |
| the border | 23 | EESWNSWNESWESWNESNESWNE | 35 | EESENWSEWSEWSENWSNWSNWSENWENWENWSEN |
| the checkerboard | 22 | EESSWWNEESNNWWESWNSNES | 32 | EEESSWNNWWESSWNENESENWWSEESSNWWS |
| every tile (the largest par) | 26 | EESWNESWNESWSENWSNWSENWENW | 47 | EEESWNWSENWSWNESWESWESWNESENWNESESWNESWNEWNEWNE |

The levels farthest over the floor are small and sparse:

```
3x3, a1 and b2, par 14 (floor 5)     4x4, par 28 (floor 17), 2 such levels
#..                                  #..#
.#.                                  .#..
...                                  ...#
                                     #.#.
```

### One tile alone, and the macros

The par of each tile lit alone (the census's report prints these grids):

```
3x3          4x4
 6  7  2      6  7  2  5
 7  8  3      7  8  3  6
 2  3  4      2  3  4  5
              5  6  5  8
```

| level | par | a shortest solution |
|---|---|---|
| 3 × 3, b2 alone (the centre) | 8 | ESENSNWS |
| 4 × 4, b2 alone | 8 | ESENSNWS |
| 4 × 4, c2 alone | 3 | ESE |
| 4 × 4, b3 alone | 3 | SES |
| 4 × 4, c3 alone | 4 | ESSE |

ESENSNWS lights b2 with a there-and-back: E, S, E lands the face down on
c2 (a flip), N, S lands it down on c2 again (the flip undone), and N, W, S
brings it down on b2.

**The macros:** the shortest roll sequences that end on a1 with every tile
dark and the painted face turned somewhere else.

| painted face ends | rolls | the first shortest |
|---|---|---|
| north | 4 | ESWN |
| west | 4 | SENW |
| down | 8 | ESSWNNEW |
| south | 8 | ESENSNWW |
| east | 8 | SESNSWNN |

The 4-roll macros are the loop around the 2 × 2 square a1, b1, b2, a2,
clockwise (ESWN) or anticlockwise (SENW). Neither lands the painted face
down. Each turns the whole cube a third of a turn about a body diagonal, so
it cycles the painted face up → north → west → up (or back), and three
loops, 12 rolls, put the whole cube back as it started: that is where a
factor of 12 lives. The down macro is the start tile's solution, ESSWNN,
then E, W to land on a1 again and flip it back.

**No multiple-of-something pattern in the pars.** The single-tile pars on
4 × 4 take every value from 2 to 8; the par distribution has gaps only at
par 1 (one roll can't land the face down) and next to the top (3 × 3 has no
par 25, 4 × 4 none at 46: one tile short of every tile costs 23 or 24 on
3 × 3, and 44 or 45 on 4 × 4). Lighting b2 doesn't need the 12-roll loop:
its par is 8.

### Other starts

`census N starts` runs the census from every cell and face. Every start
reaches every pattern on both grids.

| start | 3 × 3 largest par | mean | 4 × 4 largest par | mean |
|---|---|---|---|---|
| a1 up (the census's) | 26 | 14.38 | 47 | 26.34 |
| a1 down | 26 | 14.61 | 47 | 26.45 |
| a1, the face north or west (toward the wall) | 28 | 15.77 | 48 | 27.13 |
| a1, the face south or east | 26 | 14.26 | 46 | 26.08 |
| the centre, up (3 × 3: b2) | 30 | 17.08 | 47 (b2) | 26.14 |
| an edge cell, up (b1) | 27 | 14.84 | 47 | 26.18 |

**Symmetry:** the square's eight symmetries carry a corner start with the
face up to each other corner's, so the four corners give the same census
(their lines in `starts` are equal), and the diagonal through a1 maps the
census to itself (a pattern and its transpose have one par). Other cells and
faces give other pars, but only a little: the largest par moves by one or
two, and the mean by under a roll on 4 × 4. A corner start, face up, loses
nothing.

## Does the cube write itself?

**Partly, and less than Nibble++ did.**

| what | the cube |
|---|---|
| the par's leading term | a recipe: 3 × lit − 1, met on every lit count |
| the hardest level | every tile lit, at exactly the floor (26, 47): a tour, which is easy |
| the excess over the floor | 0 to 9 (3 × 3) and 0 to 11 (4 × 4), with a bell-shaped spread and no rule visible in the tables |
| the hardest excess | small, sparse patterns: two diagonal tiles beside the start cost 9 rolls over the floor |
| other starts | the same picture within a roll or two |

The count of lit tiles explains most of the par, and the largest par is a
plain tour of the whole grid. What it leaves is the excess: how many wasted
rolls the tiles' places force, from the painted face's three-roll cycle,
the walls and the cell colours. That part has no pattern the tables show:
patterns with the same lit count range over 11 rolls on 4 × 4, and the
worst ones are not the dense ones. So a level's par is "3 per tile plus a
puzzle", and the puzzle part grows with the grid (its range is 9 on 3 × 3,
11 on 4 × 4).

## SAT

`cube_sat.c` writes "can this level be solved in exactly k rolls?" as
DIMACS CNF, the machine unrolled k times, as `src/plus_sat.c` does for
Nibble++:

| variables | what they are |
|---|---|
| 1 to 2k | the rolls' bits, roll t at 2t − 1 (high) and 2t (low), so a model reads off as a program |
| R[t][r] | roll t is r, tied to its two bits (3 clauses each) |
| P[t][c], F[t][f] | the cube's cell and painted face at time t, one-hot (at most one, pairwise) |
| L[t][c] | tile c lit at time t; time 0 all dark |
| D[t][c] | a landing face down on c: P[t][c] AND F[t][down], Tseitin |

A step is P[t−1][c] ∧ R[t][r] → P[t][c′] (refused together for a roll off
the grid), F[t−1][f] ∧ R[t][r] → F[t][f′], and L[t][c] = L[t−1][c] XOR
D[t][c] (4 clauses). The end is a unit clause a tile. A 4 × 4 CNF at
k = 47 is 2,858 variables and 16,363 clauses.

"Exactly k" is the question the CNF asks; the par is the least k that is
SAT, so the census predicts SAT at the par and UNSAT at every k below it.

`sat.shale` checks each SAT model's rolls with `cube_sat check` (the
rules' checker) and its `v` lines with `drat model`, and each UNSAT's DRAT
proof with `src/drat.c`. The build doesn't run it.

The results are in `sat/grid4.txt` and `sat/grid5.txt`, one line a case;
the tables below are their times on this box (2 cores, 2026-11-27, niced,
beside other work). Every SAT model's rolls solved its level, and every
DRAT proof checked, all by RUP alone.

### 4 × 4, against the census

| level | par | SAT at par | UNSAT at par − 1 | proof | DRAT check |
|---|---|---|---|---|---|
| d4 alone | 8 | 22 ms | 24 ms | 42 KB | 10 ms |
| the diagonal | 18 | 0.19 s | 0.50 s | 0.97 MB | 0.42 s |
| the checkerboard | 32 | 10.2 s | 15.2 s | 17.7 MB | 15.9 s |
| the border | 35 | 46.9 s | 53.7 s | 45.3 MB | 52.4 s |
| every tile | 47 | 372 s | no answer in 900 s | | |

Each SAT at the par and each UNSAT one roll short agrees with the census.
The one miss is telling: every tile at 46 rolls is UNSAT by the floor's
three-line argument (16 tiles need 3 × 16 − 1 = 47), but rocsat, which
only sees clauses, didn't prove it in 15 minutes.

### 5 × 5, past the census

For each level, k runs from the floor 3 × lit − 1 upward; every k below the
floor is ruled out by the floor's argument, every k from the floor to the
par − 1 is UNSAT with a checked proof, and the par is SAT. So these are
5 × 5 pars, which the census can't reach (5 billion states).

| level | lit | floor | par | over | UNSAT cases | the slowest UNSAT | SAT at par | a shortest solution |
|---|---|---|---|---|---|---|---|---|
| e5 alone (the far corner) | 1 | 2 | 10 | 8 | 8 | 77 ms | 20 ms | EEEWSSSEES |
| the four corners | 4 | 11 | 18 | 7 | 7 | 0.43 s | 0.16 s | SEENEESSSSWWWWNNNN |
| the diagonal | 5 | 14 | 24 | 10 | 10 | 1.96 s | 1.07 s | SEENWWSSEESSEEWNWNWNEESS |
| a plus (row 3 and column c) | 9 | 26 | 28 | 2 | 2 | 48.9 s | 9.97 s | SSSENNEESNESNNWWESWWSEESWESW |

The 5 × 5 diagonal is 10 rolls over its floor, against 7 on 4 × 4 and
8 on 3 × 3. The far corner alone is 8 over: on 4 × 4 it is 6 over, on
3 × 3 2 over.
