# A theory of Nibble++'s shortest programs

The census (`src/census.c`) finds the shortest Nibble++ program for every
one of the 65,536 formal languages by searching the machine's states. This
note explains those lengths instead of searching for them: one recursion
that covers all four opcode sets, a closed form for {zero, decr, mod2} with
its counts by length, and an exact test for when `half` helps. Each claim is
marked THEOREM (proved here, and checked against the census) or CONJECTURE
(checked against the census only). `tests/test_theory_plus.c` checks every
claim on all 65,536 formal languages, and `build.shale` runs it.

The machine: AX holds an input from 0 to 15. `zero` accepts if AX = 0;
`decr` rejects if AX = 0 and otherwise takes 1 from AX; `mod2` sets AX to
AX % 2; `half` sets AX to AX >> 1. A program that ends still running
rejects. S is a formal language (a set of inputs), |S| its size, max(S) its
largest member. "S from d on" means S ∩ {d, ..., 15}, and "S below d" means
S ∩ {0, ..., d - 1}.

It helps to allow machines with fewer inputs: the N-input machine has inputs
0 to N - 1 and the same rules. Nibble++ is the 16-input machine.

## Walks

A walk is a program of `zero` and `decr` alone. Input k is at AX 0 after
exactly k `decr`s, so a `zero` standing after the k-th `decr` (and before
the next) accepts input k, and the next `decr` rejects it. So a walk with d
`decr`s accepts the inputs k ≤ d that have a `zero` in place k, rejects the
other inputs below d, and leaves every input n ≥ d not yet accepted running,
with AX n - d. Two `zero`s in one place do no more than one.

**THEOREM 0** (the book's): with {zero, decr}, the shortest program for S
has max(S) + |S| lines, and the empty formal language none. A walk deciding
S needs max(S) `decr`s and a `zero` for each member. This is "the walk
length" below.

## The recursion, for every opcode set

Call `mod2` and `half` the split lines. Take a walk of d `decr`s followed
by a split line f. The inputs still running have AX u = n - d, for u from 0
to N - 1 - d, and f sends u to f(u). The inputs that f sends to the same
value v form a block, B(v). For `half`, the blocks are {d, d + 1},
{d + 2, d + 3}, and so on, with {N - 1} alone at the end when N - 1 - d is
even. For `mod2`, they are the inputs from d of d's parity and those of the
other parity. After the split line, the inputs of a block have the same AX
forever, so they share a verdict.

Say S lines up at d for f when every block is either inside S or outside it,
with one exception: block B(0) may meet S in input d alone, which a `zero`
right before the split line takes (write z = 1 then, else z = 0). Then S'
is the set of values whose blocks are inside S, a formal language on the
N'-input machine, where N' is the number of values.

**THEOREM 1** (the recursion). For every opcode set holding `zero` and
`decr`, the shortest program for S on the N-input machine has as many lines
as the least of:

- the walk length, max(S) + |S|;
- for each split line f in the set and each d < N where S lines up and
  N' < N: d + |S below d| + z + 1 + shortest(S' on the N'-input machine).

*Proof.* The construction first: for k from 0 to d - 1, a `zero` if k is in
S, then a `decr`; a `zero` if z = 1; the split line; then a shortest
program for S' on the N'-input machine. On the inputs below d it is a walk.
After the split line, each running input sits at the value of its block,
and from there it behaves exactly as that value does on the smaller
machine, which accepts it when the value is in S'. So the program decides
S, with the stated number of lines.

Now no program is shorter. Take a shortest program P for S. If it has no
split line, it is a walk, and Theorem 0 bounds it. Otherwise cut it at its
first split line f, after a walk with d `decr`s. If d ≥ N, every input has
halted before f, and dropping f and everything after it gives a shorter
program for S, which can't be. So d < N. The walk accepts some inputs at
most d and needs at least d + |S below d| lines, plus one more if it
accepts input d. Everything after f is a program on the N'-input machine,
run on the values: it accepts some set S', and P accepts a block exactly
when its value is in S'. So S lines up at d with that S', and the part
after f has at least shortest(S') lines. (If the walk accepts d while B(0)
is accepted anyway, the count only grows.) If N' = N, then d = 0 and f
changes no input's AX at all, so dropping f gives a shorter program, which
can't be. Adding up gives the bound. ∎

The smaller machines shrink fast: `half` leaves at most ⌈N/2⌉ inputs and
`mod2` at most 2, so the recursion needs the machines of 8, 4, 2 and 1
inputs. `src/theory_plus.c` fills their tables and rebuilds a program from
the choices.

*Checked:* the recursion's length is the census's for all 65,536 formal
languages under each of the four opcode sets, and its programs, run on the
plain interpreter (`src/plus.c`), decide their formal languages at those
lengths with only the set's opcodes.

**THEOREM 1a** (few split lines). Every shortest program has at most four
split lines, at most one `mod2`, and at most one
`half` after a `mod2`.

*Proof.* A split line that leaves every running AX unchanged can be dropped,
so in a shortest program each split line lowers the largest running AX.
`half` takes a largest value M to M >> 1, and `mod2` takes it to at most 1
and changes something only when M ≥ 2. From 15, the most such steps is
15, 7, 3, 1, 0: four. After a `mod2` every value is 0 or 1, so another
`mod2` changes nothing and only one `half` can still change something. ∎

*Checked:* the recursion's programs obey these bounds for every formal
language and opcode set.

## {zero, decr, mod2}: a closed form

**THEOREM 2.** With {zero, decr, mod2}, the shortest program for S has as
many lines as the least of:

- the walk length, max(S) + |S|;
- d + |S below d| + 2, for each d where S from d on is exactly the inputs
  from d of d's parity (the program: the walk to d, then `mod2, zero`);
- d + |S below d| + 4, for each d where S from d on is every input from d
  (the walk to d, then `mod2, zero, decr, zero`).

*Proof.* Apply Theorem 1 with f = `mod2`. After the split, the even block
E (inputs from d of d's parity) sits at 0 and the odd block O at 1. On the
2-input machine, `mod2` changes nothing, so by Theorem 0 the shortest
programs for {}, {0}, {1} and {0, 1} have 0, 1, 2 and 3 lines. S' = {}
gives a program longer than a walk. S' = {0} and S' = {0, 1} give the
second and third forms. That leaves S' = {1}: S from d + 1 on is the odd
block, at cost d + |S below d| + z + 3. That is never needed: S from d + 1
on is then exactly the inputs from d + 1 of (d + 1)'s parity, so the second
form at d + 1 applies, at cost (d + 1) + |S below d + 1| + 2, which is the
same number whether input d is in S (z = 1) or not (z = 0). ∎

The task's guess at a canonical form holds here: a walk, at most one
`mod2`, then one of two tails, `zero` or `zero, decr, zero`.

*Checked:* the closed form is the census's length for all 65,536 formal
languages.

## {zero, decr, mod2}: the counts by length

The census's counts for {zero, decr, mod2} are F(L + 1) at each length L
from 1 to 15, except that length 4 has 6 (F(5) is 5), and length 16 has
1,595, two short of F(17) = 1,597. (F(0) = 0, F(1) = F(2) = 1.)

**THEOREM 3.** For 1 ≤ L ≤ 15 the count at length L is F(L + 1), except 6
at L = 4; at L = 16 it is F(17) - 2.

*Proof.* Write a walk to d with the zeros of P = S below d as a row of
tiles: for each place k below d, the tile `zero, decr` if k is in P, else
`decr`. A row of m lines is a tiling of m by tiles of sizes 1 and 2, and
there are F(m + 1) of them. Call the second form of Theorem 2 a parity form
and the third an all-from form; each is a row followed by its tail.

*Which form is cheapest.* A parity form at d is also a parity form at
d - 2 exactly when d - 2 is in P and d - 1 is not, and that one is a line
shorter; at d + 2 it costs three lines more; at a d of the other parity it
is impossible, since the two tails would differ at every input from the
larger d on. So the cheapest parity form of S is the one whose row does not
end in `zero, decr, decr`. In the same way an all-from form at d is also
one at d - 1, two lines shorter, exactly when d - 1 is in P, so the cheapest
row is empty or ends in `decr`.

*Lengths up to 15.* A parity form of at most 15 lines has d ≤ 13, so S
holds d and d + 2 and reaches 14 or 15: its walk length is at least 16. An
all-from form of at most 15 lines has d ≤ 11, so S holds 12 to 15, and its
walk length is at least 19. So no formal language of length L ≤ 15 is
counted twice, and a walk language of walk length L ≤ 15 has no cheaper
form. Neither form can be the other: an all-from S holds 14 and 15 both,
and a parity form's S holds only one of them. Adding up at length L:

- walks: the S with max(S) + |S| = L, F(L) of them (Theorem 0's count);
- parity forms: rows of L - 2 lines not ending in `zero, decr, decr`,
  F(L - 1) - F(L - 4) (rows of L - 5 lines are what such an ending
  leaves; none when L < 5);
- all-from forms: rows of L - 4 lines, empty or ending in `decr`: F(L - 4)
  when L ≥ 5, none when L < 4, and 1 when L = 4 (the empty row).

That is F(L) + F(L - 1) = F(L + 1) for every L but 4, where the empty row
adds one more: `mod2, zero, decr, zero`, which accepts all 16 inputs. The
other five of length 4 are the three walk languages {0, 2}, {1, 2} and {3},
and the parity forms {0, 1, 3, 5, ..., 15} (`zero, decr, mod2, zero`) and
{2, 4, ..., 14} (`decr, decr, mod2, zero`).

*Length 16.* The same three counts give F(17), and exactly two of those are
wrong. A parity form of 16 lines with d = 14 is S = {14}, whose walk is 15
lines: it belongs at 15. Every other parity form there has d ≤ 13 and a walk
length of at least 16, with 16 only for {12, 14}; and {12, 14} is a parity
form of 14 lines, so it belongs at 14, not among the 16-line walks.
All-from forms hold 14 and 15 and have walk lengths of at least 17. So the
count is F(17) - 2. ∎

*Checked:* each of the three pieces, as counted by tilings, sums to the
census at every length from 1 to 16, with {14} at 15 and {12, 14} at 14.

## {zero, decr, half}: when `half` helps

`half`'s blocks from d are {d, d + 1}, {d + 2, d + 3}, ..., ending with {15}
alone when 15 - d is even.

**THEOREM 4.** With {zero, decr, half}, `half` gives S a program shorter
than its walk exactly when one of these holds:

- (A) for some d, S lines up at d for `half`, and one of the blocks inside S
  is a pair;
- (B) S holds 15 and none of 12, 13 and 14.

*Proof.* (A) suffices. Take the split at d. Let k be the number of pairs
inside S and V = max(S'). S's largest member is in the top accepted block,
so max(S) ≥ d + 2V, plus 1 if that block is a pair, and |S| is
|S below d| + z plus the sizes of the accepted blocks. The split costs
d + |S below d| + z + 1 + shortest(S'), and shortest(S') ≤ V + |S'| (its
walk). So the walk minus the split is at least V + (1 if the top block is a
pair) + k - 1. With k ≥ 1 this is at least 1: if V = 0 the only accepted
block is B(0), the top one, and it is a pair.

(B) suffices. Let P be S without 15, so max(P) ≤ 11, and S's walk is
15 + |P| + 1 lines. At d = 11 the blocks are {11, 12}, {13, 14} and {15},
so S lines up (input 11, if in S, alone by its `zero`) with S' = {2} on the
3-input machine, whose shortest program is `decr, decr, zero`. The split
costs 11 + |P| + 1 + 3 = |P| + 15, one line less.

Nothing else is helped. If `half` helps, Theorem 1 gives a split at some d
that beats the walk, with S' not empty: an empty S' leaves S inside
{0, ..., d}, and then the split costs at least d + |S| + 1, more than the
walk. If an accepted block is a pair, (A) holds. Otherwise the only
accepted block is the lone {15}, so 15 is in S and S has nothing from d + 1
to 14. If S holds 14, the split at 14 has the pair {14, 15}, and (A) holds.
If S's largest member below 15 is m = 12 or 13, every split is checked: a
split at d < m puts m in a block with m + 1, which is not in S, so S does
not line up, or with m - 1, where lining up would make an accepted pair,
which (A) has ruled out; the split at 12 has the block {14, 15} half inside S; the
split at 13 costs 13 + |S below 13| + z + 1 + 2, which is exactly the walk;
the splits at 14 and 15 do not line up or cost more. So with m = 12 or 13,
`half` does not help, and what is left is (B). ∎

*Checked:* `half` helps 38,911 formal languages, exactly those that (A) or
(B) names. 13,654 formal languages hold 15 and line up only with {15}
accepted, and 3,414 of them are helped: those that (B) names.

## Conjectures

**CONJECTURE 1.** `half` helps a formal language over {zero, decr} exactly
when it helps over {zero, decr, mod2}: the shortest length with all four is
less than with {zero, decr, mod2} exactly when the shortest with
{zero, decr, half} is less than the walk. The census agrees for all 65,536
(38,911 on each side); I have no proof. One direction would need: when
`mod2` already beats the walk (S from some d on is a parity class or
everything), `half` together with `mod2` beats `mod2` alone whenever `half`
alone beats the walk.

## What failed

- **The four-op length is the better of the two three-op lengths:
  REFUTED.** 4,178 formal languages need `mod2` and `half` both. One is
  the inputs whose bit 2 is clear, {0, ..., 3, 8, ..., 11}:
  `half, half, mod2, zero` decides it in 4 lines, while {zero, decr, mod2}
  takes 19 and {zero, decr, half} takes 6.
- **`half` helps only a formal language with two neighbouring members:
  REFUTED.** 377 that `half` helps have none, {15} first: `decr, half, decr,
  half, decr, decr, decr, zero` takes 8 lines against the walk's 16.
- **Lining up at some d with S' not empty is enough for `half` to help:
  REFUTED.** That is the necessary half of Theorem 4 only; 10,240 formal
  languages line up that way, with only {15} accepted, and are not helped
  (those with a member from 12 to 13 and none at 14).

## Open

- A closed form for {zero, decr, half}, or its counts by length, as Theorem
  3 does for `mod2`. The recursion gives every length, but its depth (up to
  four splits) has so far resisted a formula.
- A proof of Conjecture 1.
- Which formal languages need `mod2` and `half` both, beyond the count.
