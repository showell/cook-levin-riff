# shortest.py: the state flood, which finds the shortest program for every
# lit set the cube can reach.
#
#   python3 cube_book/shortest.py 3
#
# A state is where the cube stands, where its painted face points and which
# tiles are lit. The flood starts on a1 (row 0, column 0) with the paint up
# and nothing lit, and tries the rolls N, E, S and W, in that order, from
# each state in the order it reached them. The first program to reach a
# state is a shortest one, and a lit set's par is the length of the first
# state reached with it.
import sys
from collections import Counter, deque

# Where the painted face goes on each roll, as Cube.roc's face_after has it.
# A face missing from a roll's map stays where it is.
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


def flood(n):
    """Every state the cube can reach, with the first shortest program to it,
    in the order the flood reached them."""
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


def shortest(program):
    """Each reachable lit set's first shortest program, from a flood's."""
    best = {}
    for state, rolls in program.items():
        best.setdefault(state[3], rolls)
    return best


def main():
    n = int(sys.argv[1])
    program = flood(n)
    best = shortest(program)
    pars = Counter(len(rolls) for rolls in best.values())
    print(f"{n}x{n}: {len(program)} states reached, {len(best)} patterns lit")
    print("par  patterns")
    for par in range(max(pars) + 1):
        print(f"{par:3}  {pars[par]:8}")


if __name__ == "__main__":
    main()
