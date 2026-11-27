# The six steps in the general ring: an experiment

The Delightful Monstrosity in the book composes the six steps in the Boolean
ring, where x² = x. This experiment tries the whole program in the general
integer ring instead: step k's four output polynomials substituted into step
k+1's, every product expanded in full, and no power ever reduced. Ignoring
x² = x is the one trick allowed. Nothing is squared or padded.

**The result: it does not complete, and it cannot on any machine.** Step 1
composes in milliseconds. Step 2 overflows int64 coefficients, and its `hb`
and `lb` would need about 10 to 20 GB. The whole program would have about
10^55 terms.

## How it runs

The composer was a one-off C program built with `gcc -O2` (lane dm-general,
2026-11-27). It is not committed, because Steve dropped the general-ring
polynomials the same day. It had an exact build with checked int64
coefficients, a measuring build with coefficients mod 2^61 − 1, and a mode
for step 2's `halted` and `accepted` alone.

The program sets its own limits, so a blow-up dies cleanly. It runs at nice
19, its address space is capped at 3 GB, and an alarm stops it after 20
minutes. A failed allocation, an int64 overflow or an exponent past 16 bits
stops it with one line that says the step and the gate.

- **Each step is `construct_polynomials`, evaluated gate by gate** on the
  state so far: NOT is 1 − x, AND is x·y, OR is x + y − x·y. Substitution
  is a ring homomorphism, so running the circuit on polynomials gives exactly
  the canonical step polynomial with the state substituted in. It is the
  same polynomial by the cheapest route.
- **The gate sequence is a copy of stepper.c's.** Before composing, the run
  holds the copy to stepper.c's four one-step polynomials term for term
  (786, 2,979, 25 and 33 terms).
- **A monomial is 14 16-bit exponents.** A product accumulates in a dense
  array when its box of exponents has at most 2^25 points. Otherwise it
  packs each monomial into 64 bits and accumulates in a hash table. Sums are
  merges.
- **Two checks run after every step.** At 8 random points of Z_p^14
  (p = 2^61 − 1), the composed polynomials must equal six steps of
  stepper.c's own polynomials, which only the general ring's polynomials
  pass. And reducing them by x² = x must give compose.c's Boolean-ring
  polynomials after the same step.
- **Bounds come without expanding anything.** The largest exponent of each
  variable, the total degree and the sum of |coefficients| are carried
  through the step polynomials' terms. The number of monomials inside the
  degree bounds is counted exactly, which bounds the number of terms.

## What came out

Measured on 2026-11-27 on the box, which had 2 cores and other lanes
running. The fill is the share of the monomials inside a polynomial's own
exponent and degree bounds that it actually holds.

| step | poly | terms | degree | max exp | max coefficient | fill |
|---|---|---|---|---|---|---|
| 1 | hb | 175 | 20 | 5 | 36 (6 bits) | 13.5% |
| 1 | lb | 529 | 26 | 7 | 42 (6 bits) | 16.9% |
| 1 | halted | 8 | 4 | 1 | 1 | 50% |
| 1 | accepted | 8 | 4 | 1 | 1 | 50% |
| 2 | halted | 22,128 | 56 | 14 | 549,274 (20 bits) | 12.5% |
| 2 | accepted | 21,878 | 56 | 14 | 628,680 (20 bits) | 12.4% |
| 2 | hb, lb | not reached | | | | |

Step 1 takes about 1 ms. Step 2's `halted` and `accepted` take 0.4 s, at
0.8 MiB each. They need no `hb'` or `lb'` (the `ha=1` run), and both pass
both checks.

Step 2's `hb` and `lb` do not finish, and they stop at two limits:

- **The exact build overflows int64 at step 2**, in the gate `becomes_1`,
  about 12 s in. So the true coefficients need big integers.
- **The measuring build runs into the time cap at step 2.** Its
  coefficients are mod 2^61 − 1, so they never overflow. The gate
  `becomes_1 = OR(a, b)` multiplies an `a` by a
  `b`, each about 600,000 to 900,000 terms. That is about 5 × 10^11 term
  pairs. Its box of exponents is too large
  for the dense array, so it goes to the hash table. At about 75 ns a pair
  that is roughly 11 hours, for one of the roughly 30 products in a step. The smaller
  dense products before it ran at about 7 ns a pair.
  The run was stopped about 17 minutes in, still inside that one product,
  when the experiment was dropped.

## The bounds, and what they predict

The degree bounds are tight wherever they can be checked. At step 1 every
degree and largest exponent equals its bound. At step 2, `halted` and
`accepted` reach their bounds exactly: degree 56 and largest exponent 14.
The fill has stayed between 12% and 17% for every large polynomial measured,
the step-2 intermediates included (14.0% and 17.4%, and one at 4.5%). The
coefficient bound runs about a third high in bits: 20 bits measured
against a bound of 29.3 for step 2's `halted`.

| step | hb: degree | hb: terms ≤ | lb: degree | lb: terms ≤ | coefficient bits ≤ (lb) |
|---|---|---|---|---|---|
| 1 | 20 | 1.3e3 | 26 | 3.1e3 | 12 |
| 2 | 260 | 5.8e8 | 358 | 2.9e9 | 179 |
| 3 | 3,380 | 5.9e16 | 4,674 | 5.5e17 | 2,357 |
| 4 | 43,940 | 1.0e27 | 60,782 | 1.8e28 | 30,666 |
| 5 | 571,220 | 3.0e39 | 790,186 | 1.0e41 | 398,691 |
| 6 | 7,425,860 | 1.5e54 | 10,272,438 | 9.8e55 | 5,183,013 |

`halted` and `accepted` at step 6 are bounded by degree 1,608,936 and
2.0e45 terms. At every step the degree grows about 13 times.

The extrapolation applies a 12% to 17% fill to the term bounds. It takes
the coefficients at about two thirds of their bound in bits, and the
monomial at 8 bytes packed while it fits:

- **Step 2:** `hb` would have about 7–10 × 10^7 terms and `lb` about
  3.5–5 × 10^8. Their coefficients would be roughly 90–120 bits, two
  words. At about 24 bytes a term that is about 2 GB for `hb` and 8–12 GB
  for `lb`. The box has 7 GB of RAM and no swap, and the cap is 3 GB. The
  products alone would be around 10^12 to 10^13 term pairs, hours to days
  on one core.
- **Step 3:** about 10^16 to 10^17 terms with coefficients of about 1,500
  bits, so around 10^19 to 10^21 bytes, tens to hundreds of exabytes.
- **Step 6, the whole program:** about 10^54 to 10^55 terms of degree up to
  about 10 million, with coefficients of up to a few million bits. Even
  written one bit a term, that is more than 10^40 times all the storage in
  the world.

So the general-ring monstrosity of the whole program does not exist on
any computer. The largest general-ring polynomials made here are one step,
786 and 2,979 terms, which is what the book prints. Next come step 2's
`halted` and `accepted`, at 22,128 and 21,878 terms. Step 2's `hb` and
`lb` would be next, and they are already past this box.

## Python's 2023 approach, for comparison

Python composing in the general ring would have needed the same polynomials,
so the same term counts. Its `Poly` holds each term as an object with a list
of variable-power objects, a few hundred bytes to about 1 KB a term in
CPython. This is an estimate; the 2023 code was not rerun. At that size,
step 2's `hb` and `lb` (about 5 × 10^8 terms) would need roughly 100 to 500
GB. At about a microsecond a term pair, step 2's products would take weeks
or more. That fits what was seen: step 1 in about 0.9 s, and step 2 past its
30 s cap ([1a5f6fa](https://github.com/showell/virtual-machine/commit/1a5f6fa)'s
message).
