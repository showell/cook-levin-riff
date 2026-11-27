# tables.py: the tables and facts the book prints, from the census's pars on
# the 4 x 4 board and the search in shortest.py on the small boards.
#
#   python3 cube_book/tables.py cube_book/build/pars4.txt
#
# The file named is `census 4 PATH`'s: one line a lit set, its number, its
# par and a shortest program. It prints blocks a blank line apart, and the
# book's build holds each in cube_book/book.md byte for byte. A block that
# starts with the line `facts` is a list of phrases instead, one a line, each
# of which the book's prose must hold.
import sys

from shortest import START, roll, search, shortest

FACES = ["up", "down", "north", "south", "east", "west"]
N = 4


def tile(n, i):
    """Tile i's name: its column as a letter, then its row from 1."""
    return "abcdefgh"[i % n] + str(i // n + 1)


def cells(lit):
    """A lit set's cells by number, or a dash for a dark board."""
    numbers = [str(i) for i in range(N * N) if lit >> i & 1]
    return ", ".join(numbers) if numbers else "—"


def lit_of(numbers):
    return sum(1 << i for i in numbers)


def play(n, rolls):
    """The state a program leaves, halting at a roll off the board."""
    state = START
    for r in rolls:
        after = roll(n, state, r)
        if after is None:
            break
        state = after
    return state


def commas(k):
    return f"{k:,}"


def fence(rows):
    """Rows as a text fence, each column as wide as its widest cell."""
    widths = [max(len(row[c]) for row in rows) for c in range(len(rows[0]))]
    out = ["  ".join(cell.ljust(w) for cell, w in zip(row, widths)).rstrip() for row in rows]
    return "```text\n" + "\n".join(out) + "\n```"


def trace(rolls):
    """A program's run on 4 x 4, a line a roll: where the cube is, where the
    paint points, the lit cells and the lit set as a number."""
    rows = [("roll", "the cube on", "the paint", "lit cells", "lit set")]
    state = START
    rows.append(("start", "a1, 0", "up", "—", "0"))
    for k, r in enumerate(rolls, 1):
        after = roll(N, state, r)
        if after is None:
            rows.append((f"{k}: {r}", "off the board", "halted", cells(state[3]), str(state[3])))
            break
        state = after
        i = state[0] * N + state[1]
        rows.append((f"{k}: {r}", f"{tile(N, i)}, {i}", state[2], cells(state[3]), str(state[3])))
    return fence(rows)


# The challenges: questions about the numbers 0 to 15, each lit set the
# numbers whose answer is yes.
CHALLENGES = [
    ("is_zero", [0]),
    ("is_one", [1]),
    ("is_two", [2]),
    ("is_five", [5]),
    ("is_fifteen", [15]),
    ("is_tiny", range(0, 4)),
    ("is_small_even", [0, 2, 4, 6]),
    ("is_power_of_2", [1, 2, 4, 8]),
    ("is_square", [0, 1, 4, 9]),
    ("is_huge", range(12, 16)),
    ("is_prime", [2, 3, 5, 7, 11, 13]),
    ("is_small", range(0, 8)),
    ("is_even", range(0, 16, 2)),
    ("is_odd", range(1, 16, 2)),
]


def challenges(pars):
    rows = [("question", "lit cells", "lit set", "par", "a shortest program")]
    for name, numbers in CHALLENGES:
        lit = lit_of(numbers)
        par, rolls = pars[lit]
        rows.append((name, cells(lit), commas(lit), str(par), rolls))
    return fence(rows)


def corners(n):
    """Each cell of the board and the faces the paint points at there."""
    seen = {}
    for row, col, face, lit in search(n):
        seen.setdefault(tile(n, row * n + col), set()).add(face)
    rows = ["| The cube on | The paint points |", "|---|---|"]
    for i in range(n * n):
        rows.append(f"| {tile(n, i)} | {', '.join(f for f in FACES if f in seen[tile(n, i)])} |")
    return "\n".join(rows)


def floors(pars):
    """For each count of lit tiles, the least par, which must be the floor
    3m - 1, and the lowest-numbered lit set at it."""
    rows = [("m", "3m − 1", "lit set", "a shortest program")]
    for m in range(1, N * N + 1):
        sized = [p for p in pars if bin(p).count("1") == m]
        least = min(pars[p][0] for p in sized)
        assert least == 3 * m - 1, (m, least)
        p = min(q for q in sized if pars[q][0] == least)
        rows.append((str(m), str(3 * m - 1), commas(p), pars[p][1]))
    return fence(rows)


def sizes():
    rows = [("board", "tiles", "lit sets", "states")]
    for n in (2, 3, 4, 5):
        rows.append((f"{n} × {n}", str(n * n), commas(2 ** (n * n)), commas(n * n * len(FACES) * 2 ** (n * n))))
    return fence(rows)


def read_pars(path):
    pars = {}
    for line in open(path):
        p, k, rolls = line.split()
        pars[int(p)] = (int(k), rolls)
    assert len(pars) == 2 ** (N * N)
    return pars


def main():
    pars = read_pars(sys.argv[1])
    every = pars[lit_of(range(N * N))]
    but_last = pars[lit_of(range(N * N - 1))]
    pair = lit_of([0, 5])
    back = [rolls for rolls in ("EW", "SN") if play(N, rolls) == START]
    powers = [1 << i for i in (1, 2, 4, 8)]
    best3 = shortest(3)
    eight = sorted({len(r) for p, r in best3.items() if bin(p).count("1") == 8})
    fewer = max(len(r) for p, r in best3.items() if bin(p).count("1") <= 7)
    assert fewer < 25 < min(len(r) for p, r in best3.items() if bin(p).count("1") == 9)
    assert max(eight) < 25
    detour = play(N, "ESENWS")[3]
    assert detour == lit_of([5, 6])
    assert play(N, "NEE") == START and play(N, "EE")[3] == lit_of([2])
    blocks = [
        trace("ESSWNN"),
        trace("ESENNSSS"),
        challenges(pars),
        trace(pars[lit_of([5])][1]),
        corners(2),
        floors(pars),
        sizes(),
        "\n".join([
            "facts",
            f"every tile takes {every[0]} rolls",
            f"{commas(4 ** every[0])} programs of {every[0]} rolls",
            f"every tile but d4 in {but_last[0]} rolls",
            f"{commas(sum(powers))} = {' + '.join(str(p) for p in powers)}",
            f"cost {pars[pair][0]} rolls against a floor of {3 * 2 - 1}",
            f"the programs {' and '.join(f'`{rolls}`' for rolls in back)} both bring the cube back to the start",
            f"sets of eight lit tiles have par {' or '.join(str(k) for k in eight)}",
            f"seven or fewer have par at most {fewer}",
            f"the {commas(2 ** (N * N))} lit sets",
            f"`ESENWS` lights {' and '.join(tile(N, i) for i in (5, 6))} both",
        ]),
    ]
    print("\n\n".join(blocks))


if __name__ == "__main__":
    main()
