## An instruction of the machine. Its code is the base-4 digit a program's
## number holds for it (`Program.disassemble`), so the four are listed in
## code order.
Op := [Nada, Zero, Decr, Mod2].{
	is_eq : _

	## The four ops in code order.
	all : List(Op)
	all = [Op.Nada, Op.Zero, Op.Decr, Op.Mod2]

	code : Op -> U64
	code = |op|
		match op {
			Nada => 0
			Zero => 1
			Decr => 2
			Mod2 => 3
		}

	## The op whose code is `digit`; a digit past 3 is a caller's bug.
	from_code : U64 -> Op
	from_code = |digit|
		match digit {
			0 => Op.Nada
			1 => Op.Zero
			2 => Op.Decr
			3 => Op.Mod2
			_ => crash "Op.from_code: a base-4 digit is 0 to 3"
		}

	name : Op -> Str
	name = |op|
		match op {
			Nada => "nada"
			Zero => "zero"
			Decr => "decr"
			Mod2 => "mod2"
		}

	## The comment virtual_machine.py prints beside the op's name.
	comment : Op -> Str
	comment = |op|
		match op {
			Nada => "# do nothing"
			Zero => "# accept original input if AX = 0"
			Decr => "# reject zero or decrement AX"
			Mod2 => "# subtract 2 from AX if AX >= 2"
		}
}
