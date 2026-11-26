# equations.py: the decider of "The circuit, in full", read back from its
# Boolean equations and run on every program and every want register, with
# no code shared with the C that printed them.
#
#   circuit_main equations > equations.txt
#   python3 python/equations.py equations.txt
#
# The text is src/circuit_main.c's `equations` mode: a line `inputs: ...`
# naming the 16 inputs (want0 .. want3, then op1_hb, op1_lb .. op6_lb), then
# one line a wire in evaluation order, `name = a AND b`, `name = a OR b`,
# `name = NOT a` or `name = a`, where a and b are names defined above or
# the constants 0 and 1. The last line defines `decides`.
#
# Every point is evaluated at once: a wire's value is a Python integer of
# 65,536 bits, bit 16 * program + want, so an AND is `&`, an OR `|` and a
# NOT `^` with all ones. The machine's own answer comes from the small
# interpreter below, written from the book's rules: `decides` must be 1
# exactly where the program accepts exactly the inputs the want bits name,
# which is 4,096 points, one per program.
#
# It prints `python/equations.py: 65536 checks passed` (one a point), and
# exits 1, naming the first point that differs, when any does.

import sys

PROGRAMS = 4096
POINTS = PROGRAMS * 16
ALL = (1 << POINTS) - 1


def accepts(program, ax):
    """The machine: six lines, line 1 the program number's lowest base-4 digit."""
    for i in range(6):
        op = (program >> (2 * i)) & 3
        if op == 1 and ax == 0:  # zero
            return True
        if op == 2:  # decr
            if ax == 0:
                return False
            ax -= 1
        if op == 3:  # mod2
            ax %= 2
    return False


def language(program):
    return sum(1 << n for n in range(4) if accepts(program, n))


def input_masks():
    """Each input's 65,536 bits: want n is bit n of the want register, and
    line k's two bits are the k-th base-4 digit of the program."""
    masks = {}
    for n in range(4):
        masks["want%d" % n] = sum(1 << (16 * p + w) for p in range(PROGRAMS) for w in range(16) if (w >> n) & 1)
    for k in range(6):
        hb = 0
        lb = 0
        for p in range(PROGRAMS):
            op = (p >> (2 * k)) & 3
            block = 0xFFFF << (16 * p)
            if op >> 1:
                hb |= block
            if op & 1:
                lb |= block
        masks["op%d_hb" % (k + 1)] = hb
        masks["op%d_lb" % (k + 1)] = lb
    return masks


def evaluate(text):
    lines = text.strip().split("\n")
    head = lines[0].split()
    if head[0] != "inputs:":
        raise SystemExit("equations: the first line names no inputs")
    masks = input_masks()
    values = {"0": 0, "1": ALL}
    for name in head[1:]:
        values[name] = masks[name]

    def value(name):
        if name not in values:
            raise SystemExit("equations: %s is read before it is defined" % name)
        return values[name]

    last = None
    for line in lines[1:]:
        name, _, rhs = line.partition(" = ")
        words = rhs.split()
        if name in values:
            raise SystemExit("equations: %s is defined twice" % name)
        if len(words) == 3 and words[1] == "AND":
            values[name] = value(words[0]) & value(words[2])
        elif len(words) == 3 and words[1] == "OR":
            values[name] = value(words[0]) | value(words[2])
        elif len(words) == 2 and words[0] == "NOT":
            values[name] = value(words[1]) ^ ALL
        elif len(words) == 1:
            values[name] = value(words[0])
        else:
            raise SystemExit("equations: no equation in %r" % line)
        last = name
    if last != "decides":
        raise SystemExit("equations: the last line defines %s, not decides" % last)
    return values["decides"], len(lines) - 1


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: python3 python/equations.py EQUATIONS")
    with open(sys.argv[1]) as f:
        decides, count = evaluate(f.read())
    want = sum(1 << (16 * p + language(p)) for p in range(PROGRAMS))
    if decides != want:
        first = (decides ^ want) & -(decides ^ want)
        point = first.bit_length() - 1
        print("python/equations.py: decides is %d at program %d, want register %d, of %d equations"
              % ((decides >> point) & 1, point // 16, point % 16, count))
        sys.exit(1)
    if bin(decides).count("1") != PROGRAMS:
        print("python/equations.py: decides lights %d points, not %d" % (bin(decides).count("1"), PROGRAMS))
        sys.exit(1)
    print("python/equations.py: %d checks passed" % POINTS)


if __name__ == "__main__":
    main()
