## An instruction of Nibble++ (src/plus.h). Its code is the two bits a line
## takes, so the four are listed in code order: `half` takes the code 0
## that Nibble's `nada` has, and the other three keep Nibble's codes.
PlusOp := [Half, Zero, Decr, Mod2].{
	is_eq : _

	## The four ops in code order, the order the census tries them in.
	all : List(PlusOp)
	all = [PlusOp.Half, PlusOp.Zero, PlusOp.Decr, PlusOp.Mod2]

	code : PlusOp -> U64
	code = |op|
		match op {
			Half => 0
			Zero => 1
			Decr => 2
			Mod2 => 3
		}

	name : PlusOp -> Str
	name = |op|
		match op {
			Half => "half"
			Zero => "zero"
			Decr => "decr"
			Mod2 => "mod2"
		}

	## The op a name names, as `name` writes it.
	from_name : Str -> Try(PlusOp, [NoSuchOp(Str)])
	from_name = |text|
		match PlusOp.all.find_first(|op| op.name() == text) {
			Ok(op) => Ok(op)
			Err(NotFound) => Err(NoSuchOp(text))
		}
}

expect PlusOp.all.map(PlusOp.code) == [0, 1, 2, 3]
expect PlusOp.all.map(|op| PlusOp.from_name(op.name())) == PlusOp.all.map(|op| Ok(op))
expect PlusOp.from_name("nada") == Err(NoSuchOp("nada"))
