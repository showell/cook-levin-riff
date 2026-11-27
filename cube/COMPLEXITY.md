# The cube's puzzle is NP-complete

The question "can this level be solved in k rolls?" is in NP, and it is
NP-hard by a reduction from Hamiltonian cycle in grid graphs. The key step
is a lemma: a set meets the floor 3|T| − 1 exactly when a Hamiltonian path
through it starts at c1 or a3. `cube/complexity.c` checks the lemma, the
bound and the reduction on every case the census reaches, in the fast tier.

| claim | status | checked by computation |
|---|---|---|
| the decision problem is in NP (bitmap encoding) | proved | D₃ = 36, D₄ = 52 |
| the Hamiltonian lemma | proved | every set on 3 × 3 (511) and 4 × 4 (65,535) |
| the one-dark-tile pars, 44 or 45 by colour | proved from the lemma; 45 itself is the census's | all 16 on 4 × 4 |
| NP-hard: the reduction from Hamiltonian cycle | proved here, on the cited theorem of Itai, Papadimitriou and Szwarcfiter | every grid graph in a 4 × 4 box (5,049 reduced) |
| every tile lit costs 3n² − 1 | proved for every n ≥ 3 | rolled on 3 × 3, 4 × 4 and 5 × 5 |
| no set costs more than every tile lit | open | true on 3 × 3 and 4 × 4 (the census) |
| in NP when T is a list of cells with n in binary | open | |

The proofs here are hand proofs, written for this repo and not refereed.
The computations check them on small boards, which is evidence and not a
proof.

## The decision problem

| | |
|---|---|
| instance | n ≥ 3, a target T ⊆ the n × n board, and k |
| encoding | n² bits for T (a bitmap, row by row), k in binary |
| question | is par(T) ≤ k: does some program of at most k rolls, from a1 with the paint up, end with exactly T lit? |
| certificate | the rolls, two bits each |
| checker | `cube_check`, one pass over the rolls |

"At most k" is the question here. The census's par is the least such k.

## Membership in NP

A certificate must be polynomial in n², so par(T) needs a polynomial upper
bound. It comes from the 3 × 3 state graph.

**The 3 × 3 state graph.** A state is (cell, painted face, pattern), and an
edge is a roll that stays on the board. Each roll has exactly one state it
came from, so a state's in-degree equals its out-degree (its cell's number
of neighbours). Flipping a fixed set of tiles in every pattern maps the
graph onto itself, because a roll's flip is an XOR that depends only on the
cell and the face. So one search from each (cell, face) with nothing lit
covers every state.

| | 3 × 3 | 4 × 4 |
|---|---|---|
| states | 27,648 | 6,291,456 |
| rolls that stay on the board | 73,728 | 18,874,368 |
| in-degree = out-degree | every state | every state |
| searches, one a (cell, face) | 54, each reaches every state | 96, each reaches every state |
| strongly connected | yes | yes |
| eccentricity of the start, a1 face up | 32 | 51 |
| **the diameter** | **D₃ = 36** (from b2, face up) | **D₄ = 52** |

The 4 × 4 run is `complexity graph 4`. It takes a minute or two of CPU, so
the build doesn't run it.

The 3 × 3 eccentricities, a grid a face (rows 1 to 3, each row a to c):

| face | row 1 | row 2 | row 3 |
|---|---|---|---|
| up | 32 33 32 | 33 **36** 33 | 32 33 32 |
| down | 32 33 32 | 33 32 33 | 32 33 32 |
| north | 34 35 34 | 31 32 31 | 32 31 32 |
| south | 32 31 32 | 31 32 31 | 34 35 34 |
| east | 32 31 34 | 31 32 35 | 32 31 34 |
| west | 34 31 32 | 35 32 31 | 34 31 32 |

**The bound.** Cover the n × n board with 3 × 3 windows whose corners sit at
rows and columns {0, 3, 6, …} ∪ {n − 3}: ⌈n/3⌉² windows. Visit them in
snake order, so consecutive windows touch or overlap.

1. In each window, roll to the state with the window's tiles set to T's,
   ending on a cell next to (or inside) the next window. Strong connectivity
   says such a path exists inside the window, and it takes at most D₃ rolls.
2. Roll into the next window. That is at most one roll, and it can flip
   only a tile of that window.
3. A window's rolls never leave it, so a tile stays as the last window
   holding it set it.

| | |
|---|---|
| bound | par(T) ≤ ⌈n/3⌉² · (D₃ + 1) − 1 = 37 ⌈n/3⌉² − 1 |
| so | par(T) ≤ D₃ · n² = 36 n² for every n ≥ 3 |
| 3 × 3 | 36 (the largest par is 26) |
| 4 × 4 | 147 (the largest par is 47) |

So when k is at least the bound the answer is yes. Otherwise a certificate
has fewer than 37⌈n/3⌉² rolls, which is polynomial in the input's n² bits.
The problem is in NP. The windows also show that every set is reachable on
every board with n ≥ 3.

## The Hamiltonian lemma

**Lemma.** For n ≥ 3 and a non-empty target T, par(T) ≥ 3|T| − 1. Equality
holds exactly when G[T], the grid graph induced on T, has a Hamiltonian path
that starts at c1 or a3.

**The face mechanics** (`cube_face_after`): a roll moves the face that is up
to the side it rolls toward, that side's face down, the face that is down to
the side behind, and the side behind up. The two sides across the roll stay
put.

| from | roll | the painted face goes |
|---|---|---|
| up | any r | to side r |
| side X | toward X | down (a landing: a flip) |
| side X | away from X | up |
| side X | across X | stays X |
| down | any r | to the side behind r, opposite r |

**The gaps between landings** (a landing is a roll that ends with the paint
down):

| gap | how | where it lands |
|---|---|---|
| start to the first landing | at least 2 rolls. With exactly 2, the same roll twice: EE or SS, since NN and WW leave the board | c1 or a3 |
| landing to landing, 2 rolls | r, then back: the only way | the same tile, so the two flips cancel |
| landing to landing, 3 rolls, nothing down between | r, then s across r, then r back: the paint goes behind r, stays, comes down | the cell one step s away: a neighbour |
| landing to landing, otherwise | 4 or more rolls | anywhere |

**The proof.** Say a program of k rolls lands L times. Group the landings
into runs, where consecutive landings joined by a 2-roll gap are one run.
A run stays on one tile. A tile of T is flipped an odd number of times, so
some run lands on it, and there are R ≥ |T| runs. Count the rolls:

| | rolls at least |
|---|---|
| the first landing | 2 |
| each later run's first landing | 3 |
| each other landing in a run | 2 |
| **k** | 2 + 3(R − 1) + 2(L − R) ≥ 3R − 1 ≥ **3\|T\| − 1** |

Equality forces every inequality to be tight:

- L = R, so no 2-roll gaps.
- R = |T|, so each tile of T is landed on exactly once and no other tile is.
- The first gap is 2 rolls, so the first landing is c1 or a3.
- Every later gap is 3 rolls, so each landing is a neighbour of the one
  before.
- No roll comes after the last landing.

So the landings are a Hamiltonian path of G[T] that starts at c1 or a3. A
halted program changes nothing here, since its set is a shorter prefix's.

Conversely, take such a path. EE (or SS) lands on its first cell. Each step
s is then r, s, r-back, where r runs across s on whichever side keeps the
cube on the board; a board with n ≥ 2 always has such a side. The cells
the cube passes over without landing are never flipped. That makes
3|T| − 1 rolls.

**The check** (`complexity N PARS`, against the census's pars; the path
search is a plain depth-first search with its own adjacency):

| | 3 × 3 | 4 × 4 |
|---|---|---|
| non-empty sets | 511 | 65,535 |
| par at the floor | 88 | 1,472 |
| a Hamiltonian path from c1 or a3 | 88 | 1,472 |
| … from both, from c1 only, from a3 only | 14, 37, 37 | 252, 610, 610 |
| each such path's program rolled to its set in 3\|T\| − 1 | 102 of 102 | 1,724 of 1,724 |
| a Hamiltonian path, but none from c1 or a3 (all over the floor) | 83 | 2,820 |
| **the lemma and the census agree** | **511 of 511** | **65,535 of 65,535** |

**The empty set** is outside the lemma. Its par is 0, while the formula
gives −1. The research lane predicted "88 and 1,472, including the empty
set". In fact 88 and 1,472 are the non-empty sets alone, the same counts as
README.md's "rolls over the floor" table, which counts only sets with a
tile lit. The empty set doesn't meet the formula, and the lemma excludes
it.

The planted fault: c1 alone (par 2) written as par 3 in the census's file
is refused as a counterexample.

## One dark tile

With every tile lit but one on 4 × 4, |T| = 15 and the floor is 44.

| | |
|---|---|
| colours | a cell is a1's colour when its row + column is even. c1 and a3 are a1's colour |
| a path on 15 cells | alternates colours, so it has 8 cells of its first cell's colour and 7 of the other |
| a path from c1 or a3 | starts on a1's colour, so T needs 8 cells of a1's colour |
| a1's colour has | 8 cells on the board |

| the dark tile | a1-colour cells lit | path from c1 or a3 | par |
|---|---|---|---|
| on the other colour: b1, d1, a2, c2, b3, d3, a4, c4 | 8 | yes (every one) | 44, the floor |
| on a1's colour: a1, c1, b2, d2, a3, c3, b4, d4 | 7 | none possible | 45 |

The lemma gives "44 exactly on the other colour" outright. For a1's
colour it gives only "more than 44"; the census says 45. The check holds all
16 rows to the colour rule.

**Every tile lit** has a path from c1 for every n ≥ 3: c1, b1, a1, down
column a, up column b to b2, down column c from c2, up column d, down
column e, and so on. So par(every tile) = 3n² − 1 on every board. The check
rolls it on 3 × 3, 4 × 4 and 5 × 5 (26, 47 and 74 rolls).

## The reduction

**Source:** Hamiltonian cycle in grid graphs (finite induced subgraphs of
the infinite grid) is NP-complete: Itai, Papadimitriou and Szwarcfiter,
"Hamilton Paths in Grid Graphs", SIAM J. Comput. 11(4), 1982. A grid graph
is given by its cells, and a connected one with N cells fits in an
N × N box.

**The construction**, with cells as (row, column) and rows growing south:

| step | |
|---|---|
| 1 | If G has under 3 cells or is not connected, output a fixed no-instance |
| 2 | Let u = (r₀, c₀) be the top row's leftmost cell. Its only possible neighbours are x = (r₀, c₀+1) and y = (r₀+1, c₀). If either is missing, u has degree ≤ 1 and there is no cycle: output the no-instance |
| 3 | Add a = (r₀−1, c₀) above u, b = (r₀−1, c₀+1) above x, and f = (r₀−1, c₀−1), a tip on a |
| 4 | Add a tail from b: (r₀−2, c₀+1), (r₀−3, c₀+1), then west along row r₀−3 to column m = min(c_min(G), c₀−1) + 2. Its end is the second tip |
| 5 | Shift so the second tip is c1 (row 0, column 2). Every cell then has a column ≥ 0. Call the result T, on the smallest n × n board that holds it (n ≥ 3) |
| 6 | Ask: is par(T) ≤ 3\|T\| − 1? |

**Why the new cells touch nothing else:** rows above r₀ are empty in G, and
so is row r₀ west of u. Row r₀−1 holds only f, a and b. Row r₀−2 holds only
the tail's cell above b. Row r₀−3 holds only the tail. So the only new
edges are f–a, a–u, a–b, b–x, b–(tail) and the tail's own, and both tips
have degree 1.

**Proof that it is right:**

| direction | argument |
|---|---|
| G has a Hamiltonian cycle ⇒ | u has degree 2, so the cycle uses u–x. Drop that edge: a path u … x covering G. Then f, a, u … x, b, tail is a Hamiltonian path of G[T], from the tip at c1 when read backward |
| ⇐ G[T] has a path from c1 or a3 | Both tips have degree 1, so any Hamiltonian path runs tip to tip: f, a, … , b, tail. After a comes u, not b, since f, a, b, tail would miss G. Before b comes x, since a's two path neighbours are f and u. So the path is f, a, u, … , x, b, tail, and its middle covers G from u to x. Adding the edge x–u gives a cycle |
| the lemma | par(T) ≤ 3\|T\| − 1 exactly when G[T] has a Hamiltonian path from c1 or a3. Since the tip at c1 is an end of every Hamiltonian path, this is exactly when G[T] has a Hamiltonian path |
| size | T has \|G\| + 5 + (c₀ + 1 − m) cells, the board is O(N) on a side, and k = 3\|T\| − 1. All of it is polynomial |

**The check** (`complexity 4 PARS`, its last section): every one of the
65,536 cell sets in a 4 × 4 box is put through steps 1 to 5. A Hamiltonian
cycle is searched in G, and a path from c1 or a3 in T.

| | count |
|---|---|
| turned away by steps 1 and 2 | 60,487, none with a Hamiltonian cycle |
| reduced | 5,049, on boards up to 7 × 7 |
| … with a Hamiltonian cycle | 196 |
| both tips of degree 1, one at c1 | 5,049 of 5,049 |
| **a cycle in G exactly when T has a path from c1 or a3** | **5,049 of 5,049** |

The census reaches only 4 × 4, so this check goes from G to the path, not to
the par. The lemma, proved for every n, supplies the last step.

**What rests on whom:**

| part | status |
|---|---|
| Hamiltonian cycle in grid graphs is NP-complete | cited (Itai, Papadimitriou and Szwarcfiter 1982) |
| the gadget (steps 1 to 5) and its two directions | proved above; checked on every grid graph in a 4 × 4 box |
| the lemma, which links paths to pars | proved above; checked against the census on 3 × 3 and 4 × 4 |
| membership in NP | proved above, with D₃ = 36 computed |

The research lane's sketch, "attach tails from c1 and from a degree-1 tip",
left the geometry open, and that is where the gap was. The natural forced
edge u–x has its ends in adjacent columns, so tails hung straight off both
ends touch each other, and below the top row a tail can touch G. The
construction above moves the forced edge up a row to a–b, puts the tip f
west of a, and runs b's tail up and then west through empty rows.

## What is open

| question | what is known |
|---|---|
| is every tile lit the largest par, 3n² − 1, on every board? | Every tile lit costs exactly 3n² − 1 for all n ≥ 3 (the lemma and the column tour). That nothing costs more is true on 3 × 3 (26) and 4 × 4 (47), where the largest excess over the floor is 9 and 11 and falls on sparse sets. It is unproved for larger boards, where sparse sets travel farther |
| the sparse encoding | With T given as a list of cells and n in binary, the input can be tiny while a single tile at the far corner already needs about 2n rolls, which is exponential in the input. A plain roll-by-roll certificate is then too long, so membership in NP would need a compressed certificate (runs of rolls, say) and a checker for it. Not settled here. NP-hardness carries over, since the reduction's n is O(\|G\|) |
| hardness for a fixed start other than a1 face up | The lemma's c1 and a3 come from a1 face up. Other starts change the first gap, and the reduction would need its tip moved. Not done |
| the exact par, beyond "at the floor or not" | NP-hard as shown. The excess over the floor (0 to 11 on 4 × 4) has no formula |

## Related work

| paper | the puzzle | the result | how the cube here differs |
|---|---|---|---|
| Buchin, Buchin, Demaine, Demaine, El-Khechen, Fekete, Knauer, Schulz and Taslakian, "On Rolling Cube Puzzles", CCCG 2007 ([erikdemaine.org/papers/DiceRolling_CCCG2007](https://erikdemaine.org/papers/DiceRolling_CCCG2007/)) | A cube with a labelled face rolls on a board of blocked, free and labelled cells, under rules on where the label may face down. The paper's sections cover the state graph, a parity property, boards with free cells (NP-hardness), boards without, and "blackening" a board with a black-and-white cube | NP-hardness for boards with free cells; other cases treated separately. This lane couldn't extract the PDF's text, so the exact theorems were not re-read here | Their question is whether a tour or a covering exists under fixed per-cell rules, and their cells are marked once. Here a landing flips a tile (XOR), so a tile can be undone, the board has no blocked cells, the end is free, and the question is a length bound (par ≤ k). Both use the cube's parity and Hamiltonian paths in grid graphs |
| van der Zanden and Bodlaender, "PSPACE-completeness of Bloxorz and of Games with 2-Buttons", arXiv:1411.5951 (2014) | A 1 × 1 × 2 block tilts on a grid toward a goal square. Switches open and close trapdoors | Deciding whether a level is solvable is PSPACE-complete, even with all trapdoors closed (or all open) at the start. The paper also treats variants, among them a 1 × 1 × 1 block | Bloxorz's switches give it state with no polynomial bound on a solution's length, hence PSPACE. Here the board's pattern is the only state, and strong connectivity of the 3 × 3 graph bounds every par by 36 n², so the question sits in NP |

## Files and checks

| | |
|---|---|
| `complexity.c` | the checks: `complexity 3 PARS` (the lemma, the 3 × 3 graph and D₃, the column tours), `complexity 4 PARS` (the lemma, one dark tile, the reduction), `complexity graph 4` (D₄, not in the build) |
| `golden/complexity3.txt`, `golden/complexity4.txt` | the two reports, held by `build.shale`'s every tier against the pars that tier's census writes |
| the planted fault | c1 alone at par 3 instead of 2, which must be refused as a counterexample |

| run (tcc, this box, 2026-11-27) | time |
|---|---|
| `complexity 3` | 0.2 s |
| `complexity 4` | 0.4 s |
| `complexity graph 4` (gcc -O2) | about 1 minute of CPU |
