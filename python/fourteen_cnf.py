# fourteen_cnf.py: the Nibble machine's one function, f : {0,1}^14 -> {0,1}
# (roc/Fourteen.roc), as a CNF formula a SAT tool can read, and the claim
# "f is 1 at this point" as a full assignment a model checker can verify.
#
#   python3 python/fourteen_cnf.py                 checks itself on all 16,384 points
#   python3 python/fourteen_cnf.py cnf             the formula, DIMACS, on stdout
#   python3 python/fourteen_cnf.py model N P       the assignment for input N, program P
#   python3 python/fourteen_cnf.py book            the blocks the book shows
#
# The formula's variables: v1 to v12 are the program's twelve bits, line 1's
# two first, as in every CNF of this repository, and v13 and v14 are the
# input's two (high, low), so v1 .. v14 read as the 14-bit number
# Fourteen.roc's f takes. The step is python/steps.py's
# four functions, composed six times with every bit unknown; each AND and OR
# gate gets a variable of its own (Tseitin's encoding: three clauses tie it
# to its two inputs), and NOT is a literal's sign. The last clause asks that
# `accepted` after line 6 be 1, so the formula's models are the points where
# f is 1, each with its gates' values.
#
# A model for one point is no search: every variable's value is computed by
# running the gates on that point's 14 bits. A checker that finds every
# clause satisfied has verified the claim that f is 1 there.

import sys

import steps
from steps import FALSE, step, var


def formula():
    state = (var(13), var(14), FALSE, FALSE)
    for k in range(1, 7):
        state = step(var(2 * k - 1), var(2 * k), state)
    return state[3]


# Gives each AND and OR gate under `out` a variable from 15 up, children
# first, and returns (literal of, gates in order).
def numbered(out):
    number = {}
    order = []

    def visit(f):
        if f.op in ("var", "const") or f in number:
            return
        for a in f.args:
            visit(a)
        if f.op in ("and", "or"):
            number[f] = 15 + len(order)
            order.append(f)

    visit(out)

    def lit(f):
        if f.op == "var":
            return f.args[0]
        if f.op == "not":
            return -lit(f.args[0])
        if f.op in ("and", "or"):
            return number[f]
        raise ValueError("a constant reached the formula: " + f.text())

    return lit, order


def clauses_of(out):
    lit, gates = numbered(out)
    clauses = []
    for g in gates:
        a, b = lit(g.args[0]), lit(g.args[1])
        v = lit(g)
        if g.op == "and":
            clauses += [[-v, a], [-v, b], [v, -a, -b]]
        else:
            clauses += [[v, -a], [v, -b], [-v, a, b]]
    clauses.append([lit(out)])
    return lit, gates, clauses


def cnf_text():
    lit, gates, clauses = clauses_of(formula())
    head = [
        "c f : {0,1}^14 -> {0,1}, the Nibble machine as one function",
        "c v1 .. v12: the program's bits, line 1 first; v13 v14: the input's",
        f"c v15 .. v{14 + len(gates)}: AND and OR gates; the last clause: f = 1",
        f"p cnf {14 + len(gates)} {len(clauses)}",
    ]
    return "\n".join(head + [" ".join(str(x) for x in c) + " 0" for c in clauses]) + "\n"


# Every variable's value at one point: the 14 bits, then each gate run on
# them, in the gates' order.
def values_at(gates, n, program):
    env = {13: n >> 1, 14: n & 1}
    for k in range(12):
        env[1 + k] = (program >> (11 - k)) & 1
    memo = {}
    return [env[i] for i in range(1, 15)] + [g.value(env, memo) for g in gates]


def model_text(n, program):
    lit, gates = numbered(formula())
    values = values_at(gates, n, program)
    lits = [str(i + 1) if v else str(-(i + 1)) for i, v in enumerate(values)]
    return "v " + " ".join(lits) + " 0\n"


# The program numbered `program` (line 1 the highest base-4 digit), as ops.
def ops_of(program):
    return [(program >> (10 - 2 * k)) & 3 for k in range(6)]


# At every one of the 16,384 points, the computed assignment satisfies every
# gate's three clauses, and it satisfies the last clause, f = 1, exactly when
# steps.py's ordinary interpreter accepts. One check a point.
def check():
    lit, gates, clauses = clauses_of(formula())
    checks = 0
    for n in range(4):
        for program in range(4096):
            values = values_at(gates, n, program)

            def true(x):
                return values[abs(x) - 1] == (1 if x > 0 else 0)

            assert all(any(true(x) for x in c) for c in clauses[:-1])
            assert any(true(x) for x in clauses[-1]) == steps.run_program(n, ops_of(program))
            checks += 1
    return checks


# How long `f` is written out as a tree, every shared part repeated: its
# characters, counted without building the text.
def written_length(f, memo):
    if f in memo:
        return memo[f]
    if f.op in ("var", "const"):
        n = len(f.text())
    elif f.op == "not":
        n = len("NOT ") + written_length(f.args[0], memo)
    else:
        parts = f.chain(f.op)
        sep = len(" AND ") if f.op == "and" else len(" OR ")
        n = 2 + sum(written_length(p, memo) for p in parts) + sep * (len(parts) - 1)
    memo[f] = n
    return n


# The blocks the book shows, a blank line between them: the growth of
# `accepted` line by line; the four state bits after line 1; `accepted`
# after line 2; the CNF's head and its first five gates' clauses; its last
# lines; and the assignment for input 2 and program 1689.
def book_blocks():
    state = (var(13), var(14), FALSE, FALSE)
    rows = ["| after line | gates in the graph | written out, characters |", "|---|---|---|"]
    after = []
    for k in range(1, 7):
        state = step(var(2 * k - 1), var(2 * k), state)
        after.append(state)
        rows.append(f"| {k} | {state[3].gates()} | {written_length(state[3], {}):,} |")
    one = after[0]
    first = [
        "hb'       = " + one[0].text(),
        "lb'       = " + one[1].text(),
        "halted'   = " + one[2].text(),
        "accepted' = " + one[3].text(),
    ]
    cnf = cnf_text().splitlines()
    return [
        "\n".join(rows),
        "```text\n" + "\n".join(first) + "\n```",
        "```text\n" + after[1][3].text() + "\n```",
        "```text\n" + "\n".join(cnf[:19]) + "\n```",
        "```text\n" + "\n".join(cnf[-4:]) + "\n```",
        "```text\n" + model_text(2, 1689).rstrip("\n") + "\n```",
    ]


if __name__ == "__main__":
    if sys.argv[1:] == []:
        print(f"python/fourteen_cnf.py: {check()} checks passed")
    elif sys.argv[1:] == ["cnf"]:
        sys.stdout.write(cnf_text())
    elif len(sys.argv) == 4 and sys.argv[1] == "model":
        sys.stdout.write(model_text(int(sys.argv[2]), int(sys.argv[3])))
    elif sys.argv[1:] == ["book"]:
        print("\n\n".join(book_blocks()))
    else:
        sys.exit("usage: fourteen_cnf.py [cnf | model N P | book]")
