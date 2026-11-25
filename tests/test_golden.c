// test_golden.c: find_solutions' report equals Python's output,
// tests/golden/find_solutions.txt, byte for byte. Run from the repo's root.
// build.shale also holds the main binary's whole stdout to the same file.

#define _POSIX_C_SOURCE 200809L

#include "test.h"

#include <stdlib.h>

#include "../src/vm.c"
#include "../src/search.c"

static const char *GOLDEN = "tests/golden/find_solutions.txt";

// The whole file, or NULL.
static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    size_t cap = 1 << 16;
    char *buf = malloc(cap);
    *len = fread(buf, 1, cap, f);
    int too_big = !feof(f);
    fclose(f);
    if (too_big) {
        free(buf);
        return NULL;
    }
    return buf;
}

TEST(find_solutions_prints_pythons_output_byte_for_byte) {
    size_t want_len = 0;
    char *want = read_file(GOLDEN, &want_len);
    EXPECT(want != NULL);

    char *got = NULL;
    size_t got_len = 0;
    FILE *out = open_memstream(&got, &got_len);
    EXPECT(out != NULL);
    find_solutions(out);
    fclose(out);

    size_t i = 0;
    while (i < got_len && i < want_len && got[i] == want[i]) i++;
    int same = got_len == want_len && i == got_len;
    if (!same) fprintf(stderr, "  %zu bytes, the golden %zu; first difference at byte %zu\n", got_len, want_len, i);
    free(got);
    free(want);
    EXPECT(same);
}

TEST_MAIN(TEST_CASE(find_solutions_prints_pythons_output_byte_for_byte))
