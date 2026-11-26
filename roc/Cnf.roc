## Boolean expressions to CNF by Tseitin's encoding, and CNF as DIMACS text.
##
## An expression is variables and the constants 0 and 1 under NOT, AND and
## OR. Equations name expressions one wire at a time, each wire's operands
## named above it, as `circuit_main equations` prints the decider. The
## encoding gives each input a variable, numbered from 1 in order, and each
## AND or OR a fresh variable after them, tied to its two operands by three
## clauses. A NOT is a literal's sign, and a constant operand is folded into
## its gate, so a gate with one costs nothing.
Cnf :: [].{

	## A Boolean expression over named variables.
	Expr := [Var(Str), Const(Bool), Not(Cnf.Expr), And(Cnf.Expr, Cnf.Expr), Or(Cnf.Expr, Cnf.Expr)]

	## One equation: a wire's name and its expression.
	Wire : { name : Str, expr : Cnf.Expr }

	## The equations: the inputs, then the wires in evaluation order.
	Equations : { inputs : List(Str), wires : List(Cnf.Wire) }

	## What an expression comes to: a constant, or a literal, a variable
	## number signed for its complement.
	Value : [Fixed(Bool), Literal(I64)]

	## The CNF: its variable count, its clauses, and every input's and
	## wire's value, in order.
	Formula : { variables : I64, clauses : List(List(I64)), named : List({ name : Str, value : Cnf.Value }) }

	## The equations text: a line `inputs: a b ...`, then one line a wire,
	## `name = a AND b`, `name = a OR b`, `name = NOT a` or `name = a`, an
	## operand being a name or 0 or 1.
	parse : Str -> Try(Cnf.Equations, [BadEquation(Str)])
	parse = |text| {
		rows = text.split_on("\n").drop_if(|row| row.is_empty())
		head = rows.first() ?? ""
		if head.starts_with("inputs: ") {
			inputs = head.drop_prefix("inputs: ").split_on(" ").drop_if(|word| word.is_empty())
			wires = rows.drop_first(1).map_try(wire_of)?
			Ok({ inputs, wires })
		} else {
			Err(BadEquation(head))
		}
	}

	## Tseitin's encoding of the equations.
	encode : Cnf.Equations -> Try(Cnf.Formula, [Undefined(Str), Twice(Str)])
	encode = |{ inputs, wires }| {
		var $state = inputs.fold(
			{ next: 1, clauses: [], values: Dict.empty(), named: [] },
			|state, name| { ..named_as(state, name, Literal(state.next)), next: state.next + 1 },
		)
		for wire in wires {
			unnamed(wire.name, $state)?
			found = value_of(wire.expr, $state)?
			$state = named_as(found.state, wire.name, found.value)
		}
		Ok({ variables: $state.next - 1, clauses: $state.clauses, named: $state.named })
	}

	## A name's value in the formula.
	value_named : Cnf.Formula, Str -> Try(Cnf.Value, [Undefined(Str)])
	value_named = |formula, name|
		match formula.named.find_first(|n| n.name == name) {
			Ok(found) => Ok(found.value)
			Err(_) => Err(Undefined(name))
		}

	## The formula with each name fixed to its bit by a unit clause. A name
	## that is a constant already needs none when it agrees, and is the empty
	## clause when it differs.
	fixed : Cnf.Formula, List({ name : Str, bit : Bool }) -> Try(Cnf.Formula, [Undefined(Str)])
	fixed = |formula, units| {
		var $clauses = formula.clauses
		for unit in units {
			match Cnf.value_named(formula, unit.name)? {
				Literal(lit) => {
					$clauses = $clauses.append([if (unit.bit) lit else -lit])
				}
				Fixed(bit) => {
					if bit != unit.bit {
						$clauses = $clauses.append([])
					}
				}
			}
		}
		Ok({ ..formula, clauses: $clauses })
	}

	## The formula as DIMACS: the comment lines given, one line a name with
	## its value, the `p cnf` line, then the clauses, each ending in 0.
	dimacs : List(Str), Cnf.Formula -> Str
	dimacs = |comments, formula| {
		said = comments.map(|line| "c ${line}\n")
		names = formula.named.map(|n| "c ${n.name} = ${value_text(n.value)}\n")
		head = "p cnf ${formula.variables.to_str()} ${formula.clauses.len().to_str()}\n"
		body = formula.clauses.map(|clause| "${Str.join_with(clause.map(|lit| lit.to_str()).append("0"), " ")}\n")
		Str.join_with(said.concat(names).append(head).concat(body), "")
	}
}

## The encoding's state: the next free variable, the clauses so far, and the
## value of every name defined so far, by name and in order.
State : { next : I64, clauses : List(List(I64)), values : Dict(Str, Cnf.Value), named : List({ name : Str, value : Cnf.Value }) }

## Ok when `name` is not yet defined.
unnamed : Str, State -> Try({}, [Twice(Str)])
unnamed = |name, state| if (state.values.contains(name)) Err(Twice(name)) else Ok({})

named_as : State, Str, Cnf.Value -> State
named_as = |state, name, value| { ..state, values: state.values.insert(name, value), named: state.named.append({ name, value }) }

## An expression's value, with the clauses of any gates it makes.
value_of : Cnf.Expr, State -> Try({ value : Cnf.Value, state : State }, [Undefined(Str)])
value_of = |expr, state|
	match expr {
		Var(name) =>
			match state.values.get(name) {
				Ok(value) => Ok({ value, state })
				Err(KeyNotFound) => Err(Undefined(name))
			}
		Const(bit) => Ok({ value: Fixed(bit), state })
		Not(inner) => {
			found = value_of(inner, state)?
			Ok({ ..found, value: negated(found.value) })
		}
		And(a, b) => gate(Both, a, b, state)
		Or(a, b) => gate(Either, a, b, state)
	}

## An AND (`Both`) or an OR (`Either`) of two expressions. A constant operand
## settles the gate or leaves the other operand; otherwise the gate is a new
## variable g, and for AND its clauses are (-g a), (-g b), (g -a -b), for OR
## (g -a), (g -b), (-g a b).
gate : [Both, Either], Cnf.Expr, Cnf.Expr, State -> Try({ value : Cnf.Value, state : State }, [Undefined(Str)])
gate = |kind, a, b, state| {
	left = value_of(a, state)?
	right = value_of(b, left.state)?
	after = right.state
	is_and = kind == Both
	match (left.value, right.value) {
		(Fixed(bit), other) | (other, Fixed(bit)) =>
			if (bit == is_and) Ok({ value: other, state: after }) else Ok({ value: Fixed(bit), state: after })
		(Literal(x), Literal(y)) => {
			g = after.next
			clauses =
				if is_and {
					[[-g, x], [-g, y], [g, -x, -y]]
				} else {
					[[g, -x], [g, -y], [-g, x, y]]
				}
			Ok({ value: Literal(g), state: { ..after, next: g + 1, clauses: after.clauses.concat(clauses) } })
		}
	}
}

negated : Cnf.Value -> Cnf.Value
negated = |value|
	match value {
		Fixed(bit) => Fixed(!bit)
		Literal(lit) => Literal(-lit)
	}

value_text : Cnf.Value -> Str
value_text = |value|
	match value {
		Fixed(bit) => if (bit) "true" else "false"
		Literal(lit) => lit.to_str()
	}

## One equation's line.
wire_of : Str -> Try(Cnf.Wire, [BadEquation(Str)])
wire_of = |line|
	match line.split_on(" = ") {
		[name, right] =>
			match right.split_on(" ") {
				[a, "AND", b] => Ok({ name, expr: And(operand(a), operand(b)) })
				[a, "OR", b] => Ok({ name, expr: Or(operand(a), operand(b)) })
				["NOT", a] => Ok({ name, expr: Not(operand(a)) })
				[a] => Ok({ name, expr: operand(a) })
				_ => Err(BadEquation(line))
			}
		_ => Err(BadEquation(line))
	}

operand : Str -> Cnf.Expr
operand = |word|
	match word {
		"0" => Const(False)
		"1" => Const(True)
		_ => Var(word)
	}

## A test's equations text encoded.
formula_of : Str -> Cnf.Formula
formula_of = |text|
	match Cnf.parse(text) {
		Ok(equations) =>
			match Cnf.encode(equations) {
				Ok(formula) => formula
				Err(_) => crash "Cnf: the test's equations encode"
			}
		Err(_) => crash "Cnf: the test's equations parse"
	}

clauses_of : Str -> List(List(I64))
clauses_of = |text| formula_of(text).clauses

# An AND: v3 = v1 AND v2, three clauses.
expect clauses_of("inputs: a b\ng = a AND b\n") == [[-3, 1], [-3, 2], [3, -1, -2]]

# An OR of a NOT: the NOT is a sign, so v3 = -v1 OR v2.
expect clauses_of("inputs: a b\nn = NOT a\ng = n OR b\n") == [[3, 1], [3, -2], [-3, -1, 2]]

# Constants fold: a AND 0 is 0, NOT of it 1, and 1 OR a is 1, no clauses.
expect clauses_of("inputs: a\nk = 0\ng = a AND k\nh = NOT g\nf = h OR a\n") == []

# A constant that leaves its operand: a AND 1 is a, so the OR is a's gate.
expect clauses_of("inputs: a b\ng = a AND 1\nf = g OR b\n") == [[3, -1], [3, -2], [-3, 1, 2]]

# A nested expression, (a AND NOT b) OR c: two gates, inner first.
expect {
	expr = Or(And(Var("a"), Not(Var("b"))), Var("c"))
	Cnf.encode({ inputs: ["a", "b", "c"], wires: [{ name: "f", expr }] }).map_ok(|f| f.clauses) == Ok([[-4, 1], [-4, -2], [4, -1, 2], [5, -4], [5, -3], [-5, 4, 3]])
}

# The claim's units, and the DIMACS text whole.
expect {
	formula = formula_of("inputs: a b\ng = a AND b\nn = NOT g\n")
	text = Cnf.fixed(formula, [{ name: "a", bit: True }, { name: "n", bit: True }]).map_ok(|f| Cnf.dimacs(["a test"], f))
	text == Ok("c a test\nc a = 1\nc b = 2\nc g = 3\nc n = -3\np cnf 3 5\n-3 1 0\n-3 2 0\n3 -1 -2 0\n1 0\n-3 0\n")
}

# A name read before it is defined, a name defined twice and a line that is
# no equation are refused.
expect Cnf.parse("inputs: a\ng = a AND b\n").map_ok(Cnf.encode) == Ok(Err(Undefined("b")))
expect Cnf.parse("inputs: a\na = NOT a\n").map_ok(Cnf.encode) == Ok(Err(Twice("a")))
expect Cnf.parse("inputs: a\ng = a XOR a\n").map_ok(|_| {}) == Err(BadEquation("g = a XOR a"))
