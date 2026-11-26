## The shortest Nibble++ program for each of the 65,536 languages, exactly,
## by breadth-first search over the machine's states, as src/census.c finds
## them.
##
## A state is the whole machine on all 16 inputs at once: for each input,
## its AX (0 to 15), or accepted, or rejected. Two prefixes that reach the
## same state have the same futures, so the search keeps each state once,
## at the length that first reaches it. A prefix's language is the set of
## inputs in the accepted state, so a language's shortest length is the
## first level of the search where a state with that language appears.
##
## The search expands each level's states in the order they were found,
## each by the ops in code order, as the C does, so its witnesses are the
## C's too.
import Plus
import PlusOp

Census :: [].{

	## What a search found for one opcode set.
	Found : {
		ops : List(PlusOp),

		## Every state reached, by the order found.
		origins : List(Census.Origin),

		## By language code: its shortest length, and the first state that
		## decides it.
		shortest : List({ length : U64, state : U64 }),
	}

	## How the search first reached a state: the start, or one line after
	## an earlier state, by its index.
	Origin : [Start, Stepped({ parent : U64, line : PlusOp })]

	## The census for an opcode set. Every language must be reached; one
	## that isn't stops the program, named.
	search : List(PlusOp) -> Census.Found
	search = |ops| {
		var $states = List.with_capacity(65536).append(start_state)
		var $origins = List.with_capacity(65536).append(Start)
		var $table = List.repeat(0, 131072)
		$table = $table.set(slot_of({ table: $table, states: $states, state: start_state }), 1) ?? crash "Census.search: the slot is in the table"
		var $reach = List.repeat(Unreached, Plus.language_count)
		$reach = $reach.set(language_of(start_state), Reached({ length: 0, state: 0 })) ?? crash "Census.search: a language code is below 65,536"
		var $found = 1
		var $level_start = 0
		var $level_end = 1
		var $depth = 0
		while $level_start < $level_end and $found < Plus.language_count and $depth < max_length {
			$depth = $depth + 1
			for k in $level_start..<$level_end {
				state = $states.get(k) ?? crash "Census.search: a level's states are in the list"
				for op in ops {
					next = state_after(state, op)
					slot = slot_of({ table: $table, states: $states, state: next })
					if ($table.get(slot) ?? crash "Census.search: the slot is in the table") == 0 {
						index = $states.len()
						$states = $states.append(next)
						$origins = $origins.append(Stepped({ parent: k, line: op }))
						$table = $table.set(slot, index + 1) ?? crash "Census.search: the slot is in the table"
						if 2 * $states.len() > $table.len() {
							$table = grown_table({ size: 2 * $table.len(), states: $states })
						}
						language = language_of(next)
						match $reach.get(language) {
							Ok(Unreached) => {
								$reach = $reach.set(language, Reached({ length: $depth, state: index })) ?? crash "Census.search: a language code is below 65,536"
								$found = $found + 1
							}
							Ok(Reached(_)) => {}
							Err(OutOfBounds) => crash "Census.search: a language code is below 65,536"
						}
					}
				}
			}
			$level_start = $level_end
			$level_end = $states.len()
		}
		shortest = $reach.map_with_index(
			|reach, language|
				match reach {
					Reached(at) => at
					Unreached => crash "Census.search: ops ${Census.ops_name(ops)} reach ${$found.to_str()} languages; language ${language.to_str()} is not among them"
				},
		)
		{ ops, origins: $origins, shortest }
	}

	## The witness for a language: a program of its shortest length that
	## decides it, the lines from the start state to the first state that
	## decides it.
	witness : Census.Found, U64 -> List(PlusOp)
	witness = |found, language| {
		at = found.shortest.get(language) ?? crash "Census.witness: a language code is below 65,536"
		var $lines = List.with_capacity(at.length)
		var $state = at.state
		var $walking = True
		while $walking {
			match found.origins.get($state) {
				Ok(Stepped({ parent, line })) => {
					$lines = $lines.append(line)
					$state = parent
				}
				Ok(Start) => {
					$walking = False
				}
				Err(OutOfBounds) => crash "Census.witness: a parent is a state found earlier"
			}
		}
		# The walk goes from the end back to the start.
		$lines.fold_rev(List.with_capacity(at.length), |line, forward| forward.append(line))
	}

	## The opcode set's name, its ops in the order zero, decr, mod2, half,
	## joined by commas: "zero,decr,mod2".
	ops_name : List(PlusOp) -> Str
	ops_name = |ops| Str.join_with([PlusOp.Zero, PlusOp.Decr, PlusOp.Mod2, PlusOp.Half].keep_if(|op| ops.contains(op)).map(PlusOp.name), ",")

	## The census report, as src/census.c's census_print writes it: one
	## header line, then each length's count of languages.
	report : Census.Found -> Str
	report = |found| {
		lengths = found.shortest.map(|at| at.length)
		longest = lengths.fold(0, |most, length| if (length > most) length else most)
		total = lengths.sum()
		counts = List.from_iter(U64.until(0, longest + 1)).map(|length| { length, count: lengths.count_if(|l| l == length) })
		rows = counts.map(|row| "  length ${row.length.to_str()}: ${row.count.to_str()}\n")
		"ops ${Census.ops_name(found.ops)}: ${found.origins.len().to_str()} states, ${Plus.language_count.to_str()} languages, longest ${longest.to_str()}, total length ${total.to_str()}, mean ${mean_text(total)}\n${Str.join_with(rows, "")}"
	}
}

## The longest a search runs.
max_length : U64
max_length = 64

## An input's place in a state is five bits: AX, or one of these two.
accepted : U64
accepted = 16

rejected : U64
rejected = 17

## Input n's place is bits 5n to 5n + 4.
place_shift : U64 -> U8
place_shift = |input| (5 * input).to_u8_wrap()

## Each input at its own AX, nothing halted.
start_state : U128
start_state = List.from_iter(U64.until(0, Plus.input_count)).fold(0, |state, input| state.bitwise_or(U128.shl_wrap(input.to_u128(), place_shift(input))))

## One input's place after one line. An input that has halted stays halted.
place_after : U64, PlusOp -> U64
place_after = |place, op|
	if place >= accepted {
		place
	} else {
		match op {
			Half => place // 2
			Zero => if (place == 0) accepted else place
			Decr => if (place == 0) rejected else place - 1
			Mod2 => place % 2
		}
	}

## One line on every input at once.
state_after : U128, PlusOp -> U128
state_after = |state, op| {
	var $next = 0
	for input in 0..<Plus.input_count {
		place = U128.shr_wrap(state, place_shift(input)).bitwise_and(31).to_u64_wrap()
		$next = $next.bitwise_or(U128.shl_wrap(place_after(place, op).to_u128(), place_shift(input)))
	}
	$next
}

## The state's language: bit n set when input n is accepted.
language_of : U128 -> U64
language_of = |state| {
	var $code = 0
	for input in 0..<Plus.input_count {
		if U128.shr_wrap(state, place_shift(input)).bitwise_and(31).to_u64_wrap() == accepted {
			$code = $code.bitwise_or(U64.shl_wrap(1, input.to_u8_wrap()))
		}
	}
	$code
}

## A state's two halves, multiplied and mixed into 64 bits.
state_hash : U128 -> U64
state_hash = |state| {
	low = state.to_u64_wrap()
	high = U128.shr_wrap(state, 64).to_u64_wrap()
	mixed = U64.times_wrap(low.bitwise_xor(U64.times_wrap(high, 0x9E3779B97F4A7C15)), 0xBF58476D1CE4E5B9)
	mixed.bitwise_xor(U64.shr_wrap(mixed, 31))
}

## The table slot that holds the state, or the empty slot where it goes. A
## slot holds a state's index plus 1, and 0 when it is empty; the table's
## size is a power of 2.
slot_of : { table : List(U64), states : List(U128), state : U128 } -> U64
slot_of = |{ table, states, state }| {
	mask = table.len() - 1
	var $slot = state_hash(state).bitwise_and(mask)
	while is_other_state({ table, states, slot: $slot, state }) {
		$slot = ($slot + 1).bitwise_and(mask)
	}
	$slot
}

## Whether the slot holds a state, and not this one.
is_other_state : { table : List(U64), states : List(U128), slot : U64, state : U128 } -> Bool
is_other_state = |{ table, states, slot, state }|
	match table.get(slot) {
		Ok(0) => False
		Ok(held) => (states.get(held - 1) ?? crash "Census.is_other_state: a slot holds a state's index") != state
		Err(OutOfBounds) => crash "Census.is_other_state: the slot is in the table"
	}

## A table of the given size holding every state.
grown_table : { size : U64, states : List(U128) } -> List(U64)
grown_table = |{ size, states }|
	states.fold_with_index(
		List.repeat(0, size),
		|table, state, index| table.set(slot_of({ table, states, state }), index + 1) ?? crash "Census.grown_table: the slot is in the table",
	)

## A total length over 65,536 languages as printf's `%.2f` writes the mean:
## total / 65,536 is exact in binary, so its hundredths are total · 25 /
## 16,384, rounded to the nearest, a tie to the even one.
mean_text : U64 -> Str
mean_text = |total| {
	scaled = total * 25
	whole = scaled // 16384
	rest = scaled % 16384
	hundredths = if (rest > 8192 or (rest == 8192 and whole % 2 == 1)) whole + 1 else whole
	digits = hundredths % 100
	"${(hundredths // 100).to_str()}.${if (digits < 10) "0" else ""}${digits.to_str()}"
}

expect mean_text(1441794) == "22.00"
expect mean_text(1380359) == "21.06"
expect mean_text(1250009) == "19.07"
# A tie goes to the even hundredth, as glibc's printf rounds 0.125 and 0.375.
expect mean_text(8192) == "0.12"
expect mean_text(24576) == "0.38"
expect mean_text(0) == "0.00"
expect state_after(start_state, PlusOp.Half) == List.from_iter(U64.until(0, 16)).fold(0, |state, input| state.bitwise_or(U128.shl_wrap((input // 2).to_u128(), place_shift(input))))
expect language_of(state_after(start_state, PlusOp.Zero)) == 1
expect language_of(state_after(state_after(start_state, PlusOp.Mod2), PlusOp.Zero)) == 0x5555
