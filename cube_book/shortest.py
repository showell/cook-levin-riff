# shortest.py: the shortest program for every pattern of lit tiles, found by
# a breadth-first search over the cube's states.
#
#   python3 cube_book/shortest.py 3
#
# A state is where the cube stands, where its painted face points and which
# tiles are lit. The search starts on a1 (row 0, column 0) with the paint up
# and nothing lit, and tries the rolls N, E, S and W from each state in the
# order it found them. The first program to reach a state is a shortest one,
# and a pattern's par is the length of the first state found with it. A roll
# off the board halts a program with the tiles it lit before that roll, the
# set a shorter program lights, so the search skips it.
import sys
from collections import Counter, deque

# Where the painted face goes on each roll. Rolling north tips the cube over
# its north edge: the top goes north, north goes down, down goes south and
# south comes up. A face missing from a roll's map stays where it is.
TURN = {
    "N": {"up": "north", "north": "down", "down": "south", "south": "up"},
    "E": {"up": "east", "east": "down", "down": "west", "west": "up"},
    "S": {"up": "south", "south": "down", "down": "north", "north": "up"},
    "W": {"up": "west", "west": "down", "down": "east", "east": "up"},
}
STEP = {"N": (-1, 0), "E": (0, 1), "S": (1, 0), "W": (0, -1)}
START = (0, 0, "up", 0)


def roll(n, state, r):
    """The state after roll r, or None for a roll off the board."""
    row, col, face, lit = state
    row, col = row + STEP[r][0], col + STEP[r][1]
    if not (0 <= row < n and 0 <= col < n):
        return None
    face = TURN[r].get(face, face)
    if face == "down":
        lit ^= 1 << (row * n + col)
    return (row, col, face, lit)


def search(n):
    """Every state the cube can reach, with the first shortest program to it,
    in the order the search found them."""
    program = {START: ""}
    queue = deque([START])
    while queue:
        state = queue.popleft()
        for r in "NESW":
            after = roll(n, state, r)
            if after is not None and after not in program:
                program[after] = program[state] + r
                queue.append(after)
    return program


def shortest(n):
    """Each reachable pattern's first shortest program."""
    best = {}
    for state, rolls in search(n).items():
        best.setdefault(state[3], rolls)
    return best


def main():
    n = int(sys.argv[1])
    states = len(search(n))
    best = shortest(n)
    pars = Counter(len(rolls) for rolls in best.values())
    print(f"{n}x{n}: {states} states reached, {len(best)} patterns lit")
    print("par  patterns")
    for par in range(max(pars) + 1):
        print(f"{par:3}  {pars[par]:8}")


if __name__ == "__main__":
    main()
