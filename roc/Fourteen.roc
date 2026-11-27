## The Nibble machine as one function, f : {0,1}^14 -> {0,1}. The 14 bits
## are a number below 2^14: the top 12 are the program, six lines of two
## bits, line 1 the highest pair (nada 00, zero 01, decr 10, mod2 11), and
## the low 2 are the input, where AX starts. So the program's 12 bits read as
## its number: 1689 is zero, decr, decr, zero, decr, zero.
Fourteen :: [].{

	## Where a run stands: still going, with AX, or done.
	Run : [Going(U64), Accepted, Rejected]

	## One line: `zero` accepts at 0, `decr` rejects at 0 or takes 1 from AX,
	## `mod2` keeps AX's low bit, `nada` does nothing. A run that is done
	## stays done.
	step : Fourteen.Run, U64 -> Fourteen.Run
	step = |run, op|
		match run {
			Going(ax) =>
				if (op == 1 and ax == 0) Accepted
				else if (op == 2 and ax == 0) Rejected
				else if (op == 2) Going(ax - 1)
				else if (op == 3) Going(ax % 2)
				else run
			Accepted | Rejected => run
		}

	## f itself: true when the program accepts the input. A run that gets
	## past line 6 still going rejects.
	f : U64 -> Bool
	f = |bits| {
		program = bits // 4
		input = bits % 4
		# Each line's two bits, line 1 first: 4^5, 4^4, ... 4^0.
		places = [1024, 256, 64, 16, 4, 1]
		places.fold(Going(input), |run, place| Fourteen.step(run, (program // place) % 4)) == Accepted
	}

	## The formal language a program decides: the inputs f accepts.
	decided : U64 -> List(U64)
	decided = |program| [0, 1, 2, 3].keep_if(|input| Fourteen.f(program * 4 + input))
}

# Program 1689 is the one program that decides {0, 2, 3}.
expect Fourteen.decided(1689) == [0, 2, 3]
# Program 2704 (decr, decr, decr, zero, nada, nada) decides {3}.
expect Fourteen.decided(2704) == [3]
# Programs 0 (all nada) and 4095 (all mod2) decide { }.
expect Fourteen.decided(0) == []
expect Fourteen.decided(4095) == []
# f is 1 at 5,596 of its 16,384 points, as the C's cross-check counts.
expect List.from_iter(U64.until(0, 16384)).keep_if(Fourteen.f).len() == 5596
