## The rolling cube, the third toy machine, as cube/cube.c has it on the
## 4 x 4 board. One die-sized cube with one painted face starts on a1 (the
## north-west corner) with the paint up, every tile dark. A roll tips it over
## one bottom edge into the next cell, N, E, S or W, and a roll off the board
## is refused. Each time it lands with the paint down, the tile under it
## flips. Cell r * 4 + c is row r + 1 (0 the north row) and column c (0 the
## west column, a), and tile i of the lit set is bit i, as cube.h has them.
##
## Only the painted face touches the tiles, and where it goes on a roll
## depends on where it is alone, so a state is the cell, where the paint
## points (one of six) and the lit set.
Cube :: [].{

	Roll : [N, E, S, W]

	Face : [Up, Down, North, South, East, West]

	State : { cell : U64, face : Cube.Face, lit : U64 }

	## The board's side.
	size : U64
	size = 4

	start : Cube.State
	start = { cell: 0, face: Up, lit: 0 }

	## The face a roll tips toward, and the one it tips away from.
	ahead : Cube.Roll -> Cube.Face
	ahead = |move|
		match move {
			N => North
			E => East
			S => South
			W => West
		}

	behind : Cube.Roll -> Cube.Face
	behind = |move|
		match move {
			N => South
			E => West
			S => North
			W => East
		}

	## Rolling north tips the cube over its north bottom edge: the top goes
	## north, north goes down, the bottom goes south, and south comes up;
	## east and west stay. The other three rolls alike.
	face_after : Cube.Face, Cube.Roll -> Cube.Face
	face_after = |face, move| {
		toward = Cube.ahead(move)
		away = Cube.behind(move)
		if face == Up {
			toward
		} else if face == toward {
			Down
		} else if face == Down {
			away
		} else if face == away {
			Up
		} else {
			face
		}
	}

	## The cell a roll moves to, or `OffBoard`.
	cell_after : U64, Cube.Roll -> Try(U64, [OffBoard])
	cell_after = |cell, move| {
		row = cell // Cube.size
		column = cell % Cube.size
		match move {
			N => if (row == 0) Err(OffBoard) else Ok(cell - Cube.size)
			S => if (row + 1 == Cube.size) Err(OffBoard) else Ok(cell + Cube.size)
			E => if (column + 1 == Cube.size) Err(OffBoard) else Ok(cell + 1)
			W => if (column == 0) Err(OffBoard) else Ok(cell - 1)
		}
	}

	## One roll, with the flip when the paint lands down.
	roll : Cube.State, Cube.Roll -> Try(Cube.State, [OffBoard])
	roll = |state, move| {
		cell = Cube.cell_after(state.cell, move)?
		face = Cube.face_after(state.face, move)
		lit = if (face == Down) state.lit.bitwise_xor(U64.shl_wrap(1, cell.to_u8_wrap())) else state.lit
		Ok({ cell, face, lit })
	}

	## The roll a letter names; a space is no roll.
	roll_of : U8 -> Try(Cube.Roll, [Space, NotARoll(U8)])
	roll_of = |byte|
		match byte {
			'N' => Ok(N)
			'E' => Ok(E)
			'S' => Ok(S)
			'W' => Ok(W)
			' ' => Err(Space)
			_ => Err(NotARoll(byte))
		}

	## A program's run, frame by frame: the state after each roll, spaces
	## skipped. A roll off the board is `OffBoard(k)`, k the rolls before it.
	play : Str -> Try(List(Cube.State), [OffBoard(U64), NotARoll(U8)])
	play = |program| {
		var $state = Cube.start
		var $frames = []
		for byte in program.to_utf8() {
			match Cube.roll_of(byte) {
				Err(Space) => {}
				Err(NotARoll(b)) => {
					return Err(NotARoll(b))
				}
				Ok(move) =>
					match Cube.roll($state, move) {
						Ok(next) => {
							$state = next
							$frames = $frames.append(next)
						}
						Err(OffBoard) => {
							return Err(OffBoard($frames.len()))
						}
					}
			}
		}
		Ok($frames)
	}

	## The tiles a program leaves lit, its meaning.
	lit_by : Str -> Try(U64, [OffBoard(U64), NotARoll(U8)])
	lit_by = |program| {
		frames = Cube.play(program)?
		match frames.last() {
			Ok(last) => Ok(last.lit)
			Err(_) => Ok(0)
		}
	}
}

# The faces of cube.c's rule: up goes ahead, ahead goes down.
expect Cube.face_after(Up, N) == North
expect Cube.face_after(North, N) == Down
expect Cube.face_after(Down, N) == South
expect Cube.face_after(South, N) == Up
expect Cube.face_after(East, N) == East
# A roll off the board, and the roll it was.
expect Cube.play("N") == Err(OffBoard(0))
expect Cube.play("EEEE") == Err(OffBoard(3))
expect Cube.play("EX") == Err(NotARoll('X'))
expect Cube.play("") == Ok([])
# E, S, E lands the paint down on c2 (cell 6), which flips; the S keeps it east.
expect Cube.play("E S E") == Ok([{ cell: 1, face: East, lit: 0 }, { cell: 5, face: East, lit: 0 }, { cell: 6, face: Down, lit: 64 }])
# The census's levels (cube/golden/census4.txt): each solution's set.
expect Cube.lit_by("ESSWNN") == Ok(1)
expect Cube.lit_by("ESWSEEES") == Ok(32768)
expect Cube.lit_by("ESSENNWWESESESNNWW") == Ok(1 + 32 + 1024 + 32768)
expect Cube.lit_by("ESENSNWS") == Ok(32)
expect Cube.lit_by("EEESWNWSENWSWNESWESWESWNESENWNESESWNESWNEWNEWNE") == Ok(65535)
# The 4-roll macro ends on a1 with nothing lit, the paint north.
expect Cube.play("ESWN").map_ok(|frames| frames.last()) == Ok(Ok({ cell: 0, face: North, lit: 0 }))
