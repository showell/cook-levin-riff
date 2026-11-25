## A polynomial in the Boolean ring: integer coefficients over variables
## that are only ever 0 or 1, so x² = x and every polynomial is multilinear.
## A monomial is then a set of variables, kept as a mask (bit i for variable
## i), and the product of two monomials is the union of their sets, the OR
## of their masks.
##
## A BoolPoly is canonical: its terms ascend by mask, each mask once, and no
## coefficient is 0. A 0/1 function of n variables has exactly one such
## polynomial, so `==` on two BoolPolys is equality of the functions.
##
## The ring's `+`, `-` and `*` are Roc's operators (`plus`, `minus`,
## `times`, `negate`). Coefficients are I64, and Roc's I64 `+` and `*` crash
## on overflow, so every coefficient is exact or the program stops.
##
## Like terms are summed in a dense table indexed by mask, as long as the
## union of the masks involved, so a product or a substitution writes each
## term product into one slot and reads the table back in mask order,
## already canonical. The table is a single-owner list, written in place.
## Variables run from 0 to 19, which bounds the table at 2^20 slots.
BoolPoly :: List({ mask : U64, coeff : I64 }).{
	is_eq : _

	zero : BoolPoly
	zero = BoolPoly.([])

	one : BoolPoly
	one = BoolPoly.constant(1)

	constant : I64 -> BoolPoly
	constant = |c| if (c == 0) BoolPoly.zero else BoolPoly.([{ mask: 0, coeff: c }])

	## Variable `index`, from 0 to 19.
	variable : U64 -> BoolPoly
	variable = |index|
		if index >= variable_limit {
			crash "BoolPoly.variable: variable ${index.to_str()} is past the last, ${(variable_limit - 1).to_str()}"
		} else {
			BoolPoly.([{ mask: bit(index), coeff: 1 }])
		}

	plus : BoolPoly, BoolPoly -> BoolPoly
	plus = |BoolPoly.(p), BoolPoly.(q)| summed(p.concat(q))

	minus : BoolPoly, BoolPoly -> BoolPoly
	minus = |p, q| p + -q

	negate : BoolPoly -> BoolPoly
	negate = |BoolPoly.(p)| BoolPoly.(p.map(|t| { mask: t.mask, coeff: -t.coeff }))

	## Every pair of terms, masks ORed and coefficients multiplied, summed
	## by mask.
	times : BoolPoly, BoolPoly -> BoolPoly
	times = |BoolPoly.(p), BoolPoly.(q)| {
		var $sums = List.repeat(0, union_of(p).bitwise_or(union_of(q)) + 1)
		for a in p {
			for b in q {
				$sums = add_at($sums, { mask: a.mask.bitwise_or(b.mask), coeff: a.coeff * b.coeff })
			}
		}
		from_sums($sums)
	}

	## Composition: variable i replaced by `replacements[i]`. A variable with no
	## value is a caller's bug. The product of the values over each of p's
	## monomials is made once, from the product of its monomial without
	## its highest variable, which p's own terms or their prefixes share.
	substitute : BoolPoly, List(BoolPoly) -> BoolPoly
	substitute = |BoolPoly.(p), replacements| {
		if union_of(p) >= bit(replacements.len()) {
			crash "BoolPoly.substitute: a variable of the polynomial has no value"
		} else {
			products = monomial_products(p, replacements)
			width = replacements.fold(0, |u, BoolPoly.(v)| u.bitwise_or(union_of(v)))
			var $sums = List.repeat(0, width + 1)
			for t in p {
				product =
					match products.get(t.mask) {
						Ok(BoolPoly.(found)) => found
						Err(KeyNotFound) => crash "BoolPoly.substitute: every monomial's product is made"
					}
				for u in product {
					$sums = add_at($sums, { mask: u.mask, coeff: t.coeff * u.coeff })
				}
			}
			from_sums($sums)
		}
	}

	## The value when the variables in `ones` are 1 and the rest 0: the sum
	## of the coefficients of the monomials inside `ones`.
	eval : BoolPoly, U64 -> I64
	eval = |BoolPoly.(p), ones| p.fold(0, |sum, t| if (t.mask.bitwise_and(ones) == t.mask) sum + t.coeff else sum)

	## The value at every point of `count` variables, by point: the
	## coefficients summed over subsets (the zeta transform), one variable
	## at a time, so 2^count × count additions, not 2^count × the terms.
	values : BoolPoly, U64 -> List(I64)
	values = |BoolPoly.(p), count| {
		size = bit(count)
		if union_of(p) >= size {
			crash "BoolPoly.values: the polynomial has a variable past the ${count.to_str()} asked for"
		} else {
			var $sums = p.fold(List.repeat(0, size), add_at)
			for index in 0..<count {
				with_it = bit(index)
				for point in 0..<size {
					if point.bitwise_and(with_it) != 0 {
						without = value_at($sums, point - with_it)
						$sums = add_at($sums, { mask: point, coeff: without })
					}
				}
			}
			$sums
		}
	}

	term_count : BoolPoly -> U64
	term_count = |BoolPoly.(p)| p.len()

	## The most variables in one term, 0 for a constant.
	degree : BoolPoly -> U64
	degree = |BoolPoly.(p)| p.fold(0, |most, t| U64.max(most, t.mask.count_one_bits().to_u64()))

	## The largest |coefficient|, 0 for the zero polynomial.
	max_abs_coeff : BoolPoly -> I64
	max_abs_coeff = |BoolPoly.(p)| p.fold(0, |most, t| I64.max(most, t.coeff.abs()))

	## Terms by degree, then by their variables' indices compared as lists
	## (with x, y, z: x*y before x*z before y*z); `+` between terms, a
	## negative coefficient in parentheses and a coefficient of 1 left out,
	## as src/boolring.c's `bp_str` prints; the zero polynomial is `0`.
	## Variable i is named `names[i]`.
	to_str : BoolPoly, List(Str) -> Str
	to_str = |BoolPoly.(p), names|
		if p.is_empty() {
			"0"
		} else {
			Str.join_with(p.sort_with(display_order).map(|t| term_text(t, names)), "+")
		}
}

variable_limit : U64
variable_limit = 20

bit : U64 -> U64
bit = |index| U64.shl_wrap(1, index.to_u8_wrap())

## Every variable a term of `terms` holds.
union_of : List({ mask : U64, coeff : I64 }) -> U64
union_of = |terms| terms.fold(0, |u, t| u.bitwise_or(t.mask))

## A canonical polynomial from terms whose masks may repeat.
summed : List({ mask : U64, coeff : I64 }) -> BoolPoly
summed = |terms| from_sums(terms.fold(List.repeat(0, union_of(terms) + 1), add_at))

## The table with `term.coeff` added at `term.mask`. Read, then written, so
## the table stays the one owner of its list and the write is in place.
add_at : List(I64), { mask : U64, coeff : I64 } -> List(I64)
add_at = |sums, term| {
	sum = value_at(sums, term.mask) + term.coeff
	sums.set(term.mask, sum) ?? crash "BoolPoly.add_at: a mask inside the table"
}

value_at : List(I64), U64 -> I64
value_at = |sums, mask| sums.get(mask) ?? crash "BoolPoly.value_at: a mask inside the table"

## The table's nonzero slots as terms, in mask order: canonical.
from_sums : List(I64) -> BoolPoly
from_sums = |sums| BoolPoly.(
	sums.fold_with_index(
		[],
		|terms, coeff, mask| if (coeff == 0) terms else terms.append({ mask: mask, coeff: coeff }),
	),
)

## The products of the values over each monomial of `terms` and over each
## of its prefixes (the monomial less its highest variables), by mask. A
## prefix is a smaller mask, so in ascending order each product's prefix is
## made before it.
monomial_products : List({ mask : U64, coeff : I64 }), List(BoolPoly) -> Dict(U64, BoolPoly)
monomial_products = |terms, replacements| {
	masks = terms.fold([], |all, t| prefixes_onto(all, t.mask)).sort_with(ascending)
	masks.fold(
		Dict.empty(),
		|products, mask|
			if products.contains(mask) {
				products
			} else if mask == 0 {
				products.insert(0, BoolPoly.one)
			} else {
				top = 63 - mask.count_leading_zero_bits().to_u64()
				rest =
					match products.get(mask - bit(top)) {
						Ok(found) => found
						Err(KeyNotFound) => crash "BoolPoly.monomial_products: a prefix comes first"
					}
				value = replacements.get(top) ?? crash "BoolPoly.monomial_products: substitute checked every variable has a value"
				products.insert(mask, rest * value)
			},
	)
}

## `mask` and each of its prefixes, appended.
prefixes_onto : List(U64), U64 -> List(U64)
prefixes_onto = |masks, mask|
	if mask == 0 {
		masks.append(0)
	} else {
		top = 63 - mask.count_leading_zero_bits().to_u64()
		prefixes_onto(masks.append(mask), mask - bit(top))
	}

ascending : U64, U64 -> [Before, Same, After]
ascending = |a, b| if (a < b) Before else if (a > b) After else Same

## By degree, then where two masks of one degree first differ, the one
## holding the lower variable comes first.
display_order : { mask : U64, coeff : I64 }, { mask : U64, coeff : I64 } -> [Before, Same, After]
display_order = |s, t| {
	degree_s = s.mask.count_one_bits()
	degree_t = t.mask.count_one_bits()
	if degree_s != degree_t {
		if (degree_s < degree_t) Before else After
	} else if s.mask == t.mask {
		Same
	} else {
		differ = s.mask.bitwise_xor(t.mask)
		lowest = differ.bitwise_and(differ.bitwise_xor(differ - 1))
		if (s.mask.bitwise_and(lowest) != 0) Before else After
	}
}

term_text : { mask : U64, coeff : I64 }, List(Str) -> Str
term_text = |t, names| {
	variables = List.from_iter(U64.until(0, variable_limit))
		.keep_if(|index| t.mask.bitwise_and(bit(index)) != 0)
		.map(|index| names.get(index) ?? crash "BoolPoly.to_str: variable ${index.to_str()} has no name")
	coefficient = if (t.coeff > 0) t.coeff.to_str() else "(${t.coeff.to_str()})"
	if t.mask == 0 {
		coefficient
	} else if t.coeff == 1 {
		Str.join_with(variables, "*")
	} else {
		Str.join_with(variables.prepend(coefficient), "*")
	}
}

# x, y, z, and small polynomials over them, for the ring laws.
x = BoolPoly.variable(0)

y = BoolPoly.variable(1)

z = BoolPoly.variable(2)

samples = [
	BoolPoly.zero,
	BoolPoly.constant(-3),
	x,
	x * y - z,
	BoolPoly.one - x,
	BoolPoly.constant(2) * x + y * z - BoolPoly.constant(5),
	(x + y) * (y - z) * BoolPoly.constant(-2),
	x * y * z + BoolPoly.constant(7) * y,
]

xyz_names = ["x", "y", "z"]

expect x * x == x
expect x - x == BoolPoly.zero
expect (BoolPoly.one - x) * x == BoolPoly.zero
expect
	((BoolPoly.one - x) * (y + z) * BoolPoly.constant(3) - BoolPoly.constant(2)).to_str(xyz_names)
		== "(-2)+3*y+3*z+(-3)*x*y+(-3)*x*z"
expect BoolPoly.zero.to_str(xyz_names) == "0"

# The ring laws, on every pair and triple of the samples.
expect samples.all(|a| samples.all(|b| a * b == b * a and a + b == b + a))
expect samples.all(|a| samples.all(|b| samples.all(|c| (a * b) * c == a * (b * c) and a * (b + c) == a * b + a * c)))
expect samples.all(|a| a - a == BoolPoly.zero and a + -a == BoolPoly.zero and a * BoolPoly.one == a)

# Evaluation is a ring map, at all 8 points.
expect
	samples.all(
		|a| samples.all(
			|b| List.from_iter(U64.until(0, 8)).all(
				|ones| (a * b).eval(ones) == a.eval(ones) * b.eval(ones) and (a + b).eval(ones) == a.eval(ones) + b.eval(ones),
			),
		),
	)

# The zeta transform gives what `eval` gives, point by point.
expect samples.all(|a| a.values(3) == List.from_iter(U64.until(0, 8)).map(|ones| a.eval(ones)))

# p = x*y - z with x := u, y := 1 - u, z := u*w, over u and w: u(1 - u) is
# 0, so the result is -u*w.
expect {
	u = BoolPoly.variable(0)
	w = BoolPoly.variable(1)
	(x * y - z).substitute([u, BoolPoly.one - u, u * w]).to_str(["u", "w"]) == "(-1)*u*w"
}
