// census_main.c: the Nibble++ census report for four opcode sets, each
// language's shortest length counted by length. build.shale holds its stdout
// to tests/golden/census.txt, and python/census4.py prints the same bytes.
// A unity build: this file includes the C it needs, so it is one compile.

#include "plus.c"
#include "census.c"

// Static: a census holds two 64 Ki tables beside its state arrays.
static Census census;

int main(void) {
    const unsigned sets[] = {
        CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR),
        CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_MOD2),
        CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_HALF),
        CENSUS_OP(PLUS_ZERO) | CENSUS_OP(PLUS_DECR) | CENSUS_OP(PLUS_MOD2) | CENSUS_OP(PLUS_HALF),
    };
    for (size_t i = 0; i < sizeof sets / sizeof sets[0]; i++) {
        census_run(&census, sets[i]);
        census_print(stdout, &census);
        census_free(&census);
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
