# decides.py: deciding a problem, a subset of {0, 1, 2, 3}, with no machine at all.
# A problem is its "want" code, four bits, bit n set when input n is in
# it: {0, 2, 3} is 0b1101, 13, as the C's `encoded_language` writes it.
# build.shale runs this file; it checks itself and prints one line.


def decides(want, n):
    # The members of the problem: each input whose bit is set in `want`.
    members = []
    for i in range(4):
        if want & (1 << i):
            members.append(i)
    # The problem holds n when n is one of its members.
    return n in members


decides_golfed = lambda want, n: want >> n & 1


# A few of the 16 problems as one test of n, by want code.
ONE_LINERS = {
    14: lambda n: n != 0,  # {1, 2, 3}
    5: lambda n: n % 2 == 0,  # {0, 2}
    8: lambda n: n == 3,  # {3}
    6: lambda n: n in (1, 2),  # {1, 2}: hb XOR lb
}


def check():
    checks = 0
    for want in range(16):
        for n in range(4):
            assert decides(want, n) == (decides_golfed(want, n) == 1)
            checks += 1
    for want, test in ONE_LINERS.items():
        for n in range(4):
            assert test(n) == decides(want, n)
            checks += 1
    hb_xor_lb = [(n >> 1) ^ (n & 1) == 1 for n in range(4)]
    assert hb_xor_lb == [decides(6, n) for n in range(4)]
    checks += 1
    return checks


if __name__ == "__main__":
    print(f"python/decides.py: {check()} checks passed")
