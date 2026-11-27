# The cube page's machine: Cube.roc as the page's wasm, on the platform in
# ../web/platform/cube.roc, which web/build.shale builds and links. The page
# keeps the state as one number and asks for each roll in turn:
#
# - a state is the lit set in bits 0-15 (tile i bit i), the cell in bits
#   16-19 (row * 4 + column, a1 0) and the painted face in bits 20-22, as
#   cube.h numbers them (up 0, down 1, north 2, south 3, east 4, west 5);
#   the start, a1 with the paint up and nothing lit, is 0;
# - `roll(state, r)`: the state after roll r (N 0, E 1, S 2, W 3, cube.h's
#   codes), or `off_board`, bit 24, when the roll leaves the board.
#
# web/cube_check.mjs holds it to cube/cube.c on web/cube_data.c's sample
# runs, frame by frame, and to every one of the census's 65,536 solutions.
app [roll] { pf: platform "../web/platform/cube.roc" }

import Cube

off_board : U64
off_board = U64.shl_wrap(1, 24)

face_code : Cube.Face -> U64
face_code = |face|
	match face {
		Up => 0
		Down => 1
		North => 2
		South => 3
		East => 4
		West => 5
	}

face_of_code : U64 -> Cube.Face
face_of_code = |code|
	match code {
		0 => Up
		1 => Down
		2 => North
		3 => South
		4 => East
		5 => West
		_ => crash "CubeWasm.face_of_code: a face's code is 0 to 5"
	}

roll_of_code : U64 -> Cube.Roll
roll_of_code = |code|
	match code {
		0 => N
		1 => E
		2 => S
		3 => W
		_ => crash "CubeWasm.roll_of_code: a roll's code is 0 to 3"
	}

pack : Cube.State -> U64
pack = |state| state.lit.bitwise_or(U64.shl_wrap(state.cell, 16)).bitwise_or(U64.shl_wrap(face_code(state.face), 20))

unpack : U64 -> Cube.State
unpack = |word| {
	lit: word.bitwise_and(0xFFFF),
	cell: U64.shr_wrap(word, 16).bitwise_and(15),
	face: face_of_code(U64.shr_wrap(word, 20).bitwise_and(7)),
}

roll : U64, U64 -> U64
roll = |word, code|
	match Cube.roll(unpack(word), roll_of_code(code)) {
		Ok(next) => pack(next)
		Err(OffBoard) => off_board
	}

# The start is 0; E from a1 goes to b1 with the paint east.
expect pack(Cube.start) == 0
expect roll(0, 1) == U64.shl_wrap(1, 16) + U64.shl_wrap(4, 20)
# N and W from a1 leave the board.
expect roll(0, 0) == off_board
expect roll(0, 3) == off_board
# S from b1 with the paint east keeps it east, on b2; E then lands it down on c2.
expect roll(roll(0, 1), 2) == U64.shl_wrap(5, 16) + U64.shl_wrap(4, 20)
expect roll(roll(roll(0, 1), 2), 1) == 64 + U64.shl_wrap(6, 16) + U64.shl_wrap(1, 20)
expect unpack(pack({ cell: 15, face: West, lit: 0xFFFF })) == { cell: 15, face: West, lit: 0xFFFF }
