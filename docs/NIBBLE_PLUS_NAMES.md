# Names for Nibble++'s languages

A Nibble++ program decides a language, a subset of the inputs 0 to 15, so
there are 65,536 of them. The Nibble++ sandbox (`web/plus.html`) names the
one a program decides as a programmer would name a predicate:

> This program decides **is_small_fib**, **{1–3, 5}**, #46 of 65,536: it
> accepts {1, 2, 3, 5} and rejects the rest.

`web/plus_names.c` works out every name ahead of time;
`web/plus_names_check.c` parses each one back to its set with a parser and
evaluator of its own and holds the page's table to them. `web/build.shale`
runs both.

## The vocabulary

Every word below is an exact set over 0 to 15, and each is an **atom**.

| atom | the set |
|---|---|
| `even`, `odd` | 0, 2, ..., 14 and 1, 3, ..., 15 |
| `mult_of_k`, k = 3 to 8 | the multiples of k, 0 among them: `mult_of_4` is {0, 4, 8, 12} |
| `prime` | {2, 3, 5, 7, 11, 13} |
| `square` | {0, 1, 4, 9} (0 = 0²) |
| `fib` | {1, 2, 3, 5, 8, 13}: the Fibonacci numbers from 1, 2, so 0 is not one |
| `power_of_2` | {1, 2, 4, 8} |
| `tiny`, `small` | 0 to 3, 0 to 7 |
| `medium` | 4 to 11 |
| `large`, `huge` | 8 to 15, 12 to 15 |
| `has_bit_k`, `lacks_bit_k`, k = 0 to 3 | n with bit k set, or clear |
| `at_least_n`, `below_n`, n = 1 to 15 | n to 15, 0 to n - 1 |
| `from_a_to_b`, a < b | a to b, both ends in |
| `n`, n = 0 to 15 | {n} alone |

The sizes are the toy Nibble's quarters scaled by four: there, on 0 to 3,
`is_tiny` is {0}, `is_small` {0, 1}, `is_middling` {1, 2}, `is_big` {2, 3}
and `is_huge` {3}. Here `tiny` and `huge` are the bottom and top quarters,
`small` and `large` the halves, and `medium` the middle half; the toy's
`middling` and `big` are `medium` and `large`, as Steve named them. With
`small` below 8, `is_small_fib` is exactly {1, 2, 3, 5}.

The toy spells its numbers (`is_one`, `is_not_two`); Nibble++ writes them
as digits, as its ranges and multiples need (`is_not_14`, `below_11`).
{ } and all sixteen inputs are `grumpy` and `always_true`, as in the toy.

## The grammar

    name    = "grumpy" | "always_true" | ["is_"] sum ["_except_" n]
    sum     = product ("_or_" product)*
    product = factor ("_and_" factor)*
    factor  = ["not_"] atom | ["not_"] size "_" noun

- A name starts `is_`, unless its first word is `has_` or `lacks_`
  (`has_bit_1_or_tiny`, `lacks_bit_1`): a predicate reads either way.
- `and` binds tighter than `or`, as in every programming language:
  `is_mult_of_4_or_odd_and_has_bit_1_or_1` is mult_of_4 or (odd and
  has_bit_1) or 1.
- `not_` negates the one factor after it: `is_not_14`.
- An adjective before a noun is the two anded: `small_fib` is small and
  fib, `medium_prime` {5, 7, 11}. The adjectives are the five sizes; the
  nouns `even`, `odd`, `prime`, `square`, `fib`, `power_of_2` and
  `mult_of_k`.
- `_except_n`, last, takes one member out of the whole sum:
  `is_from_5_to_9_or_mult_of_7_or_2_except_7` is {0, 2, 5, 6, 8, 9, 14}.
  A bare number in a sum is the short exception the other way:
  `is_small_fib_or_from_11_to_12_or_15`.
- A sum's products, and a product's factors, are written largest set
  first, then the one with the smaller least member, then by name; a
  `not_` factor comes last in its product. So `is_prime_or_mult_of_4`, and
  single numbers trail.

## The cost, and which name wins

A name's **cost** is its atoms, with one more for each `not_` and for the
`_except_`: `is_prime_or_mult_of_4` costs 2, `is_small_fib` 2 (an
adjective and a noun are two atoms), `is_not_14` 2, `grumpy` and
`always_true` 1.

Each set's name is its cheapest. A tie goes to the shorter name, then to
the one first in byte order. Length does most of the choosing among
synonyms: `small` beats `below_8` and `from_0_to_7`, `odd` beats
`has_bit_0`, `small_fib` beats `small_and_fib`, `from_1_to_3_or_5` and
`from_1_to_5_except_4` (all cost 2).

The search is a census of names by cost, as `src/census.c` is of programs
by length: the factors first, then products and sums layer by layer, each
set kept once in each class at its cheapest. It names every set up to cost
6 in about half a second.

## Coverage

Every one of the 65,536 languages has a name of cost 6 or less:

| cost | languages | in the page's table |
|---|---|---|
| 1 | 153 | yes |
| 2 | 3,260 | yes |
| 3 | 19,364 | yes |
| 4 | 31,684 | yes |
| 5 | 10,669 | no |
| 6 | 406 | no |
| none | 0 | |

The page keeps the 54,461 names of cost 4 or less (83%). Past cost 4 a
name is mostly single numbers and ranges, usually longer than the set it
names (`is_from_3_to_4_or_from_7_to_8_or_from_11_to_12_or_1_or_14`), so
the page shows the set alone: "This program decides the language {0, 2,
4, 5, 7, 8, 11}". Even at cost 3 and 4 most names lean on ranges and
numbers; the vocabulary's predicates carry the small and structured sets.

Some names: the three hardest languages for a program, each needing 28
lines, are `is_not_14` {0–13, 15}, `is_not_13` {0–12, 14, 15} and
`is_below_11_or_mult_of_3` {0–10, 12, 15}. {1, 2, 3, 5, 11, 12, 15} is
`is_small_fib_or_from_11_to_12_or_15`; {0, 2, 3, 4, 5, 7, 8, 12, 14, 15}
is `is_mult_of_4_or_small_prime_or_at_least_14`; {0, 2, 4, 5, 7, 8, 11}
costs 5 (`is_medium_prime_or_mult_of_8_or_2_or_4`), so the page doesn't
name it.

## The elegance rating

Under the language, the page rates how elegant the program is. It is a
joke, but a joke with a rule: the rating is the cost of the language's
name, and nothing else. How long the program is, or how many of its lines
do nothing, doesn't count.

| the name's cost | stars | its lines |
|---|---|---|
| 1 | ★★★★★ | "Dijkstra would nod." "Says it in one word." "Clean as a whistle." |
| 2 | ★★★★☆ | "Clean enough to frame." "Two words, no wasted breath." "Neat, with a little flourish." |
| 3 | ★★★☆☆ | "It works, and it knows it." "Respectable. Mostly." "Fine print, but legible." |
| 4 | ★★☆☆☆ | "A program only its author could love." "Reads like a tax form." "Has strong opinions about some numbers." |
| no name on the page | ★☆☆☆☆ | "Spaghetti, al dente." "There's no short way to say this." "A list pretending to be a rule." |

A language gets the line its number picks (the number mod 3), so it
always reads the same. Six languages have a line of their own, with the
stars their cost gives (all cost 1):

- `grumpy`: "Accepts nothing. Commitment issues? No: standards."
- `always_true`: "Accepts everything. Not picky, just kind."
- `is_even`: "Every other number, starting at zero. Textbook."
- `is_odd`: "Odd, in the best possible way."
- `is_prime`: "Prime real estate."
- a single number, `is_9`: "Only 9 will do. Picky, and proud of it."

The page reads the cost from the table: a name's tokens but `and`, `or`
and `except` (whose number counts instead). `plus_names_check` holds that
count to the tsv's cost for every name, and the page check pins ratings
of five, three, two and one star, three of the specials among them.

## The page's table

`plus_names.bin`, 389,985 bytes (151 KB gzipped): first its 197 tokens,
each ended by a newline, then a 0 byte. A token is an atom
(`from_3_to_9`, `fib`, `mult_of_4`) or a word between atoms (`and`, `or`,
`not`, `except`, `grumpy`, `always_true`). Then, for each language in code
order, its name's tokens, a byte each (the k-th token is k), and a 0 byte;
a language the table doesn't name has no tokens. A name is its tokens
joined by `_`, after `is_` unless it starts with `has_`, `lacks_`,
`grumpy` or `always_true`. The same names as one line of text each are
1.9 MB (191 KB gzipped), and as JSON larger still.

`plus_names_check` decodes the table as `web/plus.js` does and holds it to
`out/web/plus_names.tsv`, every name with its code and cost, which it has
parsed back to its set: each atom from its definition (a prime by trial
division, the Fibonacci numbers by their recursion), the cost recounted.
The page check (`web/page_check.mjs`) loads programs for six known
languages and sees their names, and one past the table and sees its set
alone.
