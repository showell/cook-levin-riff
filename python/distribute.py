# distribute.py: the decider's equations turned into an equivalent CNF by
# distribution alone, with no new variables, for the book's chapter "From
# equations to CNF", beside the Tseitin encoding of roc/Cnf.roc.
#
#   python3 python/distribute.py EQUATIONS         checks itself
#   python3 python/distribute.py EQUATIONS book    prints the chapter's block
#
# EQUATIONS is `circuit_main equations`'s text, read by python/equations.py's
# rules. Each wire has two forms, each a set of frozensets of literals: its
# CNF (a set of clauses) and its DNF (a set of terms), each made only when
# asked for. NOT swaps the two and flips every literal (De Morgan); AND
# unions the CNFs and crosses the DNFs; OR crosses the CNFs and unions the
# DNFs. Tautologous clauses (x OR NOT x)
# are dropped, and a set of up to SUBSUME clauses is reduced by subsumption,
# so the sizes are those of a careful expansion. A set past BUDGET clauses
# stops the run.
#
# The check: the CNF of `decides`, evaluated at all 65,536 points with
# python/equations.py's masks, equals the circuit's `decides` there. It
# prints `python/distribute.py: 65536 checks passed`, or exits 1.

import sys

import equations

BUDGET = 2_000_000
SUBSUME = 4000

# The wires the chapter reports: core 3's `accepted` after each line, then
# the verdict.
WATCHED = ["core3.step%d.accepted'" % k for k in range(1, 7)] + ["decides"]


class Blown(Exception):
    pass


def read_wires(text):
    """Each wire's definition: ("var",), ("alias", a), ("not", a), or
    ("and" | "or", a, b)."""
    wires = {}
    for line in text.strip().split("\n"):
        if line.startswith("inputs:"):
            for name in line.split()[1:]:
                wires[name] = ("var",)
            continue
        name, _, rhs = line.partition(" = ")
        words = rhs.split()
        if len(words) == 1:
            wires[name] = ("alias", words[0])
        elif words[0] == "NOT":
            wires[name] = ("not", words[1])
        else:
            wires[name] = (words[1].lower(), words[0], words[2])
    return wires


def reduced(s):
    """`s` without tautologies, and subsumed when it is small enough."""
    s = {c for c in s if not any((n, not v) in c for (n, v) in c)}
    if frozenset() in s:
        return {frozenset()}
    if len(s) <= SUBSUME:
        # Shorter sets first; each kept set is filed under its smallest
        # literal, so a set is compared only with those sharing a literal.
        kept = set()
        filed = {}
        for c in sorted(s, key=lambda c: (len(c), sorted(c))):
            if not any(k <= c for lit in c for k in filed.get(lit, ())):
                kept.add(c)
                filed.setdefault(min(c), []).append(c)
        s = kept
    if len(s) > BUDGET:
        raise Blown(len(s))
    return s


def cross(a, b):
    out = set()
    for x in a:
        out.update([x | y for y in b])
        if len(out) > BUDGET:
            raise Blown(len(out))
    return out


def flipped(s):
    return {frozenset((n, not v) for (n, v) in t) for t in s}


def form(wires, memo, w, dnf):
    """Wire w's CNF, or its DNF when `dnf`. Each is made only when asked
    for: a NOT asks for its operand's other form."""
    if (w, dnf) in memo:
        return memo[(w, dnf)]
    if w in ("0", "1"):
        # 0 is the empty clause, or no term; 1 no clause, or the empty term.
        r = {frozenset()} if (w == "1") == dnf else set()
    else:
        kind = wires[w]
        if kind[0] == "var":
            r = {frozenset([(w, True)])}
        elif kind[0] == "alias":
            r = form(wires, memo, kind[1], dnf)
        elif kind[0] == "not":
            r = flipped(form(wires, memo, kind[1], not dnf))
        else:
            a = form(wires, memo, kind[1], dnf)
            b = form(wires, memo, kind[2], dnf)
            # AND's CNF and OR's DNF are unions; the other two are crosses.
            r = reduced(a | b) if (kind[0] == "and") != dnf else reduced(cross(a, b))
    memo[(w, dnf)] = r
    return r


def value_of(cnf):
    """A CNF's 65,536 bits, as python/equations.py lays out the points."""
    masks = equations.input_masks()
    value = equations.ALL
    for clause in cnf:
        m = 0
        for (n, v) in clause:
            m |= masks[n] if v else (equations.ALL ^ masks[n])
        value &= m
    return value


def main():
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "book"):
        raise SystemExit("usage: python3 python/distribute.py EQUATIONS [book]")
    with open(sys.argv[1]) as f:
        text = f.read()
    wires = read_wires(text)
    memo = {}
    sizes = [(w, len(form(wires, memo, w, False))) for w in WATCHED]
    circuit = equations.evaluate(text)[0]
    if value_of(form(wires, memo, "decides", False)) != circuit:
        print("python/distribute.py: the CNF of decides differs from the circuit")
        sys.exit(1)
    if len(sys.argv) == 3:
        print("```text")
        for w, n in sizes:
            print("%s: %s clauses" % (w, format(n, ",")))
        print("decides by distribution equals the circuit at all %s points" % format(equations.POINTS, ","))
        print("```")
    else:
        print("python/distribute.py: %d checks passed" % equations.POINTS)


main()
