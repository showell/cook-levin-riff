# census4.py: the shortest Nibble++ program for every language over 4-bit inputs,
# an oracle for src/census.c that shares no code with it.
# A state is, for each input 0..15, its AX (0..15), or 16 for accepted, or
# 17 for rejected. The ops follow src/plus.c: zero accepts at AX = 0, decr
# rejects at 0 and otherwise takes 1, mod2 is AX % 2 and half is AX >> 1. A
# run that passes the last line rejects. Prefixes that reach the same state
# have the same futures, so a breadth-first search keeps each state once.
# build.shale runs this file and holds its output to tests/golden/census.txt,
# which src/census_main.c prints too.

A, R = 16, 17

# The opcode sets the census runs, each in the order zero, decr, mod2, half.
SETS = [
    ["zero", "decr"],
    ["zero", "decr", "mod2"],
    ["zero", "decr", "half"],
    ["zero", "decr", "mod2", "half"],
]


def step(state, op):
    out = []
    for v in state:
        if v >= A:
            out.append(v)
        elif op == "zero":
            out.append(A if v == 0 else v)
        elif op == "decr":
            out.append(R if v == 0 else v - 1)
        elif op == "mod2":
            out.append(v % 2)
        elif op == "half":
            out.append(v >> 1)
    return tuple(out)


def lang(state):
    code = 0
    for i, v in enumerate(state):
        if v == A:
            code |= 1 << i
    return code


# Each language's shortest length, and the number of states seen. The search
# stops at the end of the level that reaches the last language.
def census(ops):
    start = tuple(range(16))
    seen = {start}
    frontier = [start]
    best = {lang(start): 0}
    depth = 0
    while frontier and len(best) < 65536:
        depth += 1
        nxt = []
        for s in frontier:
            for op in ops:
                t = step(s, op)
                if t not in seen:
                    seen.add(t)
                    nxt.append(t)
                    c = lang(t)
                    if c not in best:
                        best[c] = depth
        frontier = nxt
    assert len(best) == 65536, f"{ops} reach {len(best)} languages"
    return best, len(seen)


def report(ops):
    best, states = census(ops)
    hist = {}
    for d in best.values():
        hist[d] = hist.get(d, 0) + 1
    longest = max(hist)
    total = sum(best.values())
    lines = [
        f"ops {','.join(ops)}: {states} states, 65536 languages, longest {longest}, "
        f"total length {total}, mean {total / 65536:.2f}"
    ]
    for d in range(longest + 1):
        lines.append(f"  length {d}: {hist.get(d, 0)}")
    return "\n".join(lines)


if __name__ == "__main__":
    for ops in SETS:
        print(report(ops))
