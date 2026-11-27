# tables.py: the tables and facts the book prints about the 3 x 3 and 2 x 2
# boards, from the search in shortest.py.
#
#   python3 cube_book/tables.py
#
# It prints blocks a blank line apart, and the book's build holds each in
# cube_book/book.md byte for byte. A block that starts with the line `facts`
# is a list of phrases instead, one a line, each of which the book's prose
# must hold.
from shortest import START, roll, search, shortest

FACES = ["up", "down", "north", "south", "east", "west"]


def tile(n, i):
    """Tile i's name: its column as a letter, then its row from 1."""
    return "abcdefgh"[i % n] + str(i // n + 1)


def tiles(n, lit):
    """A pattern's lit tiles, by name, or a dash for a dark board."""
    names = [tile(n, i) for i in range(n * n) if lit >> i & 1]
    return ", ".join(names) if names else "—"


def pattern(n, names):
    """The pattern with exactly these tiles lit."""
    return sum(1 << i for i in range(n * n) if tile(n, i) in names)


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


def trace(n, rolls):
    """A program's run, a row a roll: where the cube is, where the paint
    points and what is lit."""
    rows = ["| Roll | The cube on | The paint points | Lit tiles |", "|---|---|---|---|"]
    state = START
    rows.append(f"| start | {tile(n, 0)} | up | — |")
    for k, r in enumerate(rolls, 1):
        after = roll(n, state, r)
        if after is None:
            rows.append(f"| {k}: {r} | off the board, so the program halts | | {tiles(n, state[3])} |")
            break
        state = after
        rows.append(f"| {k}: {r} | {tile(n, state[0] * n + state[1])} | {state[2]} | {tiles(n, state[3])} |")
    return "\n".join(rows)


CHALLENGES = [
    ("the corner you start on", ["a1"]),
    ("the tile beside it", ["b1"]),
    ("the centre", ["b2"]),
    ("the far corner", ["c3"]),
    ("the other corner of the top row", ["c1"]),
    ("two tiles on a diagonal", ["a1", "b2"]),
    ("the diagonal", ["a1", "b2", "c3"]),
    ("the checkerboard", ["a1", "c1", "b2", "a3", "c3"]),
    ("the border", ["a1", "b1", "c1", "a2", "c2", "a3", "b3", "c3"]),
    ("every tile", ["a1", "b1", "c1", "a2", "b2", "c2", "a3", "b3", "c3"]),
]


def challenges(best):
    rows = ["| Challenge | Lit tiles | Par | A shortest program |", "|---|---|---:|---|"]
    for name, names in CHALLENGES:
        rolls = best[pattern(3, names)]
        rows.append(f"| {name} | {', '.join(names)} | {len(rolls)} | `{rolls}` |")
    return "\n".join(rows)


def corners(n):
    """Each cell of the board and the faces the paint points at there."""
    seen = {}
    for row, col, face, lit in search(n):
        seen.setdefault(tile(n, row * n + col), set()).add(face)
    rows = ["| The cube on | The paint points |", "|---|---|"]
    for i in range(n * n):
        rows.append(f"| {tile(n, i)} | {', '.join(f for f in FACES if f in seen[tile(n, i)])} |")
    return "\n".join(rows)


def floors(best):
    """For each count of lit tiles, the first pattern whose par is 3m - 1."""
    rows = ["| Lit tiles m | 3m − 1 | A pattern at the floor | Its program |", "|---:|---:|---|---|"]
    for m in range(1, 10):
        found = [p for p in sorted(best) if bin(p).count("1") == m and len(best[p]) == 3 * m - 1]
        p = found[0]
        rows.append(f"| {m} | {3 * m - 1} | {tiles(3, p)} | `{best[p]}` |")
    return "\n".join(rows)


def sizes():
    rows = ["| Board | Tiles | Patterns | States |", "|---|---:|---:|---:|"]
    for n in (2, 3, 4, 5):
        rows.append(f"| {n} × {n} | {n * n} | {commas(2 ** (n * n))} | {commas(n * n * len(FACES) * 2 ** (n * n))} |")
    return "\n".join(rows)


def main():
    best = shortest(3)
    longest = max(len(rolls) for rolls in best.values())
    back = [rolls for rolls in ("EW", "SN") if play(3, rolls) == START]
    blocks = [
        trace(3, "ESSWNN"),
        trace(3, "ESENNSSS"),
        challenges(best),
        trace(3, "ESENSNWS"),
        corners(2),
        floors(best),
        sizes(),
        "\n".join([
            "facts",
            f"every tile takes {longest} rolls",
            f"{commas(4 ** longest)} programs of {longest} rolls",
            f"the programs {' and '.join(f'`{rolls}`' for rolls in back)} both bring the cube back to the start",
        ]),
    ]
    print("\n\n".join(blocks))


if __name__ == "__main__":
    main()
