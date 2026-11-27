# oracle.py: the rolling cube's census once more, an oracle for cube/census.c
# that shares no code with it.
#
#   python3 cube/oracle.py N PARS
#
# PARS is the file `census N PARS` writes, a line a pattern: the pattern
# (tile r * N + c is bit r * N + c, row 0 the north row), its par and a
# shortest solution as letters N, E, S, W ("-" for none). The cube starts in
# the north-west corner with the painted face up, and may end anywhere.
#
# Three checks, each its own code:
#   1. a plain breadth-first search, a state the tuple (row, col, face,
#      lit tiles) kept in a dict, for N = 3 only (N = 4 has 6.3 million
#      states, too many for a dict of tuples);
#   2. the same search on sets of patterns, for N = 3 and 4: for each place
#      of the cube (cell, painted face) the patterns it has been seen with,
#      as one big integer, bit p set when pattern p is in the set. A roll
#      that lands the painted face down on tile j maps pattern p to
#      p ^ 2**j, which on the integer swaps each block of 2**j bits with
#      the block after it. A pattern's par is the first level where it is in
#      any place's set;
#   3. every solution the file gives, rolled by a plain simulator: it stays
#      on the grid, ends with exactly its pattern lit, and has the par's
#      length.
# The census's pars must be the searches' on every pattern. It prints one
# line, "cube/oracle.py: 4x4: 65536 solutions rolled, 65536 pars by 1 search",
# and exits 1 on any difference.

import sys

# Where the painted face points after a roll: rolling north tips the cube
# over its north bottom edge, so the top goes north, north goes down, down
# goes south and south comes up.
ROLL = {
    "N": {"up": "north", "north": "down", "down": "south", "south": "up", "east": "east", "west": "west"},
    "S": {"up": "south", "south": "down", "down": "north", "north": "up", "east": "east", "west": "west"},
    "E": {"up": "east", "east": "down", "down": "west", "west": "up", "north": "north", "south": "south"},
    "W": {"up": "west", "west": "down", "down": "east", "east": "up", "north": "north", "south": "south"},
}
MOVE = {"N": (-1, 0), "S": (1, 0), "E": (0, 1), "W": (0, -1)}
FACES = ["up", "down", "north", "south", "east", "west"]


def roll(n, row, col, face, r):
    """The cube after roll r, or None off the grid; and whether it flips."""
    dr, dc = MOVE[r]
    row, col = row + dr, col + dc
    if not (0 <= row < n and 0 <= col < n):
        return None
    return row, col, ROLL[r][face]


def play(n, rolls):
    """The lit pattern after rolling `rolls` from the start, or None."""
    row, col, face, lit = 0, 0, "up", 0
    for r in rolls:
        moved = roll(n, row, col, face, r)
        if moved is None:
            return None
        row, col, face = moved
        if face == "down":
            lit ^= 1 << (row * n + col)
    return lit


def plain_pars(n):
    start = (0, 0, "up", 0)
    level = {start: 0}
    queue = [start]
    par = {0: 0}
    for s in queue:
        row, col, face, lit = s
        for r in "NESW":
            moved = roll(n, row, col, face, r)
            if moved is None:
                continue
            row2, col2, face2 = moved
            lit2 = lit ^ (1 << (row2 * n + col2)) if face2 == "down" else lit
            t = (row2, col2, face2, lit2)
            if t not in level:
                level[t] = level[s] + 1
                queue.append(t)
                par.setdefault(lit2, level[t])
    return par


def low_mask(j, tiles):
    """The patterns whose bit j is 0, as a set: blocks of 2**j ones, then
    2**j zeros."""
    width = 1 << j
    block = (1 << width) - 1
    low = 0
    for start in range(0, 1 << tiles, 2 * width):
        low |= block << start
    return low


def flip(bits, j, low):
    """The set {p ^ 2**j : p in bits}, `low` being low_mask(j)."""
    width = 1 << j
    return ((bits & low) << width) | ((bits >> width) & low)


def set_pars(n):
    tiles = n * n
    lows = [low_mask(j, tiles) for j in range(tiles)]
    places = [(row, col, face) for row in range(n) for col in range(n) for face in FACES]
    seen = {p: 0 for p in places}
    frontier = {p: 0 for p in places}
    frontier[(0, 0, "up")] = seen[(0, 0, "up")] = 1  # the empty pattern
    par = {0: 0}
    found = 1
    depth = 0
    while any(frontier.values()):
        depth += 1
        nxt = {p: 0 for p in places}
        for (row, col, face), bits in frontier.items():
            if bits == 0:
                continue
            for r in "NESW":
                moved = roll(n, row, col, face, r)
                if moved is None:
                    continue
                if moved[2] == "down":
                    j = moved[0] * n + moved[1]
                    nxt[moved] |= flip(bits, j, lows[j])
                else:
                    nxt[moved] |= bits
        union = 0
        for p in places:
            nxt[p] &= ~seen[p]
            seen[p] |= nxt[p]
            union |= nxt[p]
        new = union & ~found
        found |= new
        while new:
            low = new & -new
            par[low.bit_length() - 1] = depth
            new ^= low
        frontier = nxt
    return par


def main():
    n = int(sys.argv[1])
    lines = open(sys.argv[2]).read().split("\n")
    assert lines[-1] == "", "the file ends with a newline"
    lines = lines[:-1]
    assert len(lines) == 1 << (n * n), f"{len(lines)} lines, not {1 << (n * n)}"
    rolled = 0
    census = {}
    for i, line in enumerate(lines):
        parts = line.split(" ")
        assert int(parts[0]) == i, f"line {i + 1} is pattern {parts[0]}"
        if parts[1] == "-":
            continue
        k = int(parts[1])
        census[i] = k
        rolls = "" if parts[2] == "-" else parts[2]
        assert len(rolls) == k, f"pattern {i}: a solution of {len(rolls)} rolls, par {k}"
        assert play(n, rolls) == i, f"pattern {i}: {rolls} does not solve it"
        rolled += 1
    searches = [set_pars(n)]
    if n <= 3:
        searches.append(plain_pars(n))
    for par in searches:
        assert par == census, f"the pars differ on {len(set(par.items()) ^ set(census.items()))} entries"
    noun = "search" if len(searches) == 1 else "searches"
    print(f"cube/oracle.py: {n}x{n}: {rolled} solutions rolled, {len(census)} pars by {len(searches)} {noun}")


if __name__ == "__main__":
    main()
