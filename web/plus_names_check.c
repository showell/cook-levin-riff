// plus_names_check.c: holds web/plus_names.c's names to their sets, by a
// parser and an evaluator of its own (it shares no code with the
// generator).
//
//   plus_names_check <names.tsv> <plus_names.bin> <page cost>
//
// Each line of names.tsv, `<code>\t<cost>\t<name>`, is parsed by the
// grammar of docs/NIBBLE_PLUS_NAMES.md and evaluated input by input, each
// atom from its definition (a prime by trial division, the Fibonacci
// numbers by their recursion from 1 and 2, and so on); the set must be
// `code` and the cost the parse counts must be `cost`. The codes rise, so
// no language is named twice. Then plus_names.bin is decoded as plus.js
// decodes it: each language's name must be the tsv's when its cost is at
// most <page cost>, and absent otherwise. It prints the coverage, a line a
// cost, and exits 0 only when every check passes.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WORDS 64

static int failures;

static void fail_name(const char *name, const char *why) {
    if (failures < 20) fprintf(stderr, "plus_names_check: %s: %s\n", name, why);
    failures++;
}

typedef struct {
    char *w[MAX_WORDS];
    int num, at, cost, bad;
} Parse;

static const char *peek(const Parse *p) { return p->at < p->num ? p->w[p->at] : ""; }
static int next_is(const Parse *p, const char *word) { return strcmp(peek(p), word) == 0; }
static void expect_word(Parse *p, const char *word) {
    if (!next_is(p, word)) p->bad = 1;
    else p->at++;
}

// A number from `lo` to `hi`, written without a leading zero.
static int number(Parse *p, int lo, int hi) {
    const char *s = peek(p);
    int n = 0, len = (int)strlen(s);
    if (len == 0 || len > 2 || (len == 2 && s[0] == '0')) {
        p->bad = 1;
        return lo;
    }
    for (int i = 0; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') {
            p->bad = 1;
            return lo;
        }
        n = 10 * n + (s[i] - '0');
    }
    if (n < lo || n > hi) p->bad = 1;
    p->at++;
    return n;
}

typedef int (*Holds)(int n, int k);
static int h_even(int n, int k) { (void)k; return n % 2 == 0; }
static int h_odd(int n, int k) { (void)k; return n % 2 == 1; }
static int h_prime(int n, int k) {
    (void)k;
    if (n < 2) return 0;
    for (int d = 2; d * d <= n; d++) if (n % d == 0) return 0;
    return 1;
}
static int h_square(int n, int k) {
    (void)k;
    for (int i = 0; i * i <= n; i++) if (i * i == n) return 1;
    return 0;
}
static int h_fib(int n, int k) {
    (void)k;
    for (int a = 1, b = 2; a <= n; b = a + b, a = b - a) if (a == n) return 1;
    return 0;
}
static int h_power(int n, int k) { (void)k; return n > 0 && (n & (n - 1)) == 0; }
static int h_mult(int n, int k) { return n % k == 0; }
static int h_has(int n, int k) { return (n >> k) & 1; }
static int h_lacks(int n, int k) { return !((n >> k) & 1); }
static int h_at_least(int n, int k) { return n >= k; }
static int h_below(int n, int k) { return n < k; }
static int h_is(int n, int k) { return n == k; }

static uint16_t where(Holds h, int k) {
    uint16_t s = 0;
    for (int n = 0; n < 16; n++) if (h(n, k)) s |= (uint16_t)(1u << n);
    return s;
}
static uint16_t between(int a, int b) {
    uint16_t s = 0;
    for (int n = a; n <= b; n++) s |= (uint16_t)(1u << n);
    return s;
}

static const char *const SIZES[] = {"tiny", "small", "medium", "large", "huge"};
static uint16_t size_set(int i) {
    static const int lo[] = {0, 0, 4, 8, 12}, hi[] = {3, 7, 11, 15, 15};
    return between(lo[i], hi[i]);
}
static int size_at(const Parse *p) {
    for (int i = 0; i < 5; i++) if (next_is(p, SIZES[i])) return i;
    return -1;
}

// A noun (even, odd, prime, square, fib, power_of_2, mult_of_k), or -1 with
// nothing read when the next word starts none.
static int noun(Parse *p, uint16_t *set) {
    static const char *const simple[] = {"even", "odd", "prime", "square", "fib"};
    static const Holds holds[] = {h_even, h_odd, h_prime, h_square, h_fib};
    for (int i = 0; i < 5; i++) {
        if (next_is(p, simple[i])) {
            p->at++;
            *set = where(holds[i], 0);
            return 1;
        }
    }
    if (next_is(p, "power")) {
        p->at++;
        expect_word(p, "of");
        expect_word(p, "2");
        *set = where(h_power, 0);
        return 1;
    }
    if (next_is(p, "mult")) {
        p->at++;
        expect_word(p, "of");
        *set = where(h_mult, number(p, 3, 8));
        return 1;
    }
    return -1;
}

// factor = [not] (size [noun] | noun | has_bit_k | lacks_bit_k |
//          at_least_n | below_n | from_a_to_b | n)
static uint16_t factor(Parse *p) {
    int negated = 0;
    if (next_is(p, "not")) {
        p->at++;
        p->cost++;
        negated = 1;
    }
    uint16_t s = 0;
    int size = size_at(p);
    p->cost++;
    if (size >= 0) {
        p->at++;
        s = size_set(size);
        uint16_t n;
        if (noun(p, &n) > 0) {
            p->cost++;
            s &= n;
        }
    } else if (noun(p, &s) > 0) {
    } else if (next_is(p, "has") || next_is(p, "lacks")) {
        int has = next_is(p, "has");
        p->at++;
        expect_word(p, "bit");
        s = where(has ? h_has : h_lacks, number(p, 0, 3));
    } else if (next_is(p, "at")) {
        p->at++;
        expect_word(p, "least");
        s = where(h_at_least, number(p, 1, 15));
    } else if (next_is(p, "below")) {
        p->at++;
        s = where(h_below, number(p, 1, 15));
    } else if (next_is(p, "from")) {
        p->at++;
        int a = number(p, 0, 15);
        expect_word(p, "to");
        int b = number(p, 0, 15);
        if (b <= a) p->bad = 1;
        s = between(a, b);
    } else {
        s = where(h_is, number(p, 0, 15));
    }
    return negated ? (uint16_t)~s : s;
}

static uint16_t product(Parse *p) {
    uint16_t s = factor(p);
    while (next_is(p, "and")) {
        p->at++;
        s &= factor(p);
    }
    return s;
}

static uint16_t sum(Parse *p) {
    uint16_t s = product(p);
    while (next_is(p, "or")) {
        p->at++;
        s |= product(p);
    }
    return s;
}

// The set `name` denotes and its cost; -1 for a name the grammar refuses.
static int parse_name(const char *name, uint16_t *set, int *cost) {
    if (strcmp(name, "grumpy") == 0 || strcmp(name, "always_true") == 0) {
        *set = name[0] == 'g' ? 0 : 0xffff;
        *cost = 1;
        return 0;
    }
    char copy[256];
    if (strlen(name) >= sizeof copy) return -1;
    strcpy(copy, name);
    Parse p = {.num = 0};
    for (char *t = strtok(copy, "_"); t != NULL; t = strtok(NULL, "_")) {
        if (p.num == MAX_WORDS) return -1;
        p.w[p.num++] = t;
    }
    // `is_` stands before every name but one that starts with has_ or lacks_.
    if (next_is(&p, "is")) {
        p.at++;
        if (next_is(&p, "has") || next_is(&p, "lacks")) return -1;
    } else if (!next_is(&p, "has") && !next_is(&p, "lacks")) {
        return -1;
    }
    uint16_t s = sum(&p);
    if (next_is(&p, "except")) {
        p.at++;
        int n = number(&p, 0, 15);
        if (!((s >> n) & 1)) return -1;
        s &= (uint16_t)~(1u << n);
        p.cost++;
    }
    if (p.bad || p.at != p.num) return -1;
    *set = s;
    *cost = p.cost;
    return 0;
}

static char *slurp(const char *path, long *size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)*size + 1);
    if (buf == NULL || fread(buf, 1, (size_t)*size, f) != (size_t)*size) {
        fprintf(stderr, "plus_names_check: can't read %s\n", path);
        exit(1);
    }
    buf[*size] = 0;
    fclose(f);
    return buf;
}

static char *names[65536];
static int costs[65536];

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: plus_names_check <names.tsv> <plus_names.bin> <page cost>\n");
        return 2;
    }
    int page_cost = atoi(argv[3]);
    long size;
    char *tsv = slurp(argv[1], &size);
    int last = -1, lines = 0, by_cost[16] = {0};
    for (char *line = tsv, *nl; *line; line = nl + 1) {
        nl = strchr(line, '\n');
        if (nl == NULL) {
            fail_name(line, "the tsv's last line has no newline");
            break;
        }
        *nl = 0;
        char *tab1 = strchr(line, '\t'), *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
        if (tab2 == NULL) {
            fail_name(line, "not <code>\\t<cost>\\t<name>");
            continue;
        }
        *tab1 = *tab2 = 0;
        int code = atoi(line), cost = atoi(tab1 + 1);
        char *name = tab2 + 1;
        lines++;
        if (code <= last || code > 0xffff) fail_name(name, "its code doesn't rise");
        last = code;
        uint16_t set;
        int parsed;
        if (parse_name(name, &set, &parsed) != 0) {
            fail_name(name, "the grammar refuses it");
            continue;
        }
        if (set != code) fail_name(name, "it denotes another set");
        if (parsed != cost || cost < 1 || cost > 15) fail_name(name, "its cost isn't the tsv's");
        else by_cost[cost]++;
        names[code] = name;
        costs[code] = cost;
    }

    char *bin = slurp(argv[2], &size);
    char *vocab[256];
    int num_tokens = 0;
    long at = 0;
    while (at < size && bin[at] != 0) {
        char *end = memchr(bin + at, '\n', (size_t)(size - at));
        if (end == NULL || num_tokens == 255) {
            fprintf(stderr, "plus_names_check: plus_names.bin's vocabulary is malformed\n");
            return 1;
        }
        *end = 0;
        vocab[++num_tokens] = bin + at;
        at = end - bin + 1;
    }
    at++;
    int page_named = 0;
    for (int code = 0; code < 65536; code++) {
        char name[256] = "";
        int first = 1;
        for (; at < size && bin[at] != 0; at++) {
            int t = (unsigned char)bin[at];
            if (t > num_tokens || strlen(name) + strlen(vocab[t]) + 5 >= sizeof name) {
                fprintf(stderr, "plus_names_check: plus_names.bin's language %d is malformed\n", code);
                return 1;
            }
            if (first) {
                const char *v = vocab[t];
                int bare = strncmp(v, "has_", 4) == 0 || strncmp(v, "lacks_", 6) == 0 || strcmp(v, "grumpy") == 0 ||
                           strcmp(v, "always_true") == 0;
                strcpy(name, bare ? "" : "is_");
            } else {
                strcat(name, "_");
            }
            strcat(name, vocab[t]);
            first = 0;
        }
        if (at >= size) {
            fprintf(stderr, "plus_names_check: plus_names.bin ends at language %d\n", code);
            return 1;
        }
        at++;
        int want = names[code] != NULL && costs[code] <= page_cost;
        if (want != !first || (want && strcmp(name, names[code]) != 0)) {
            fail_name(first ? "(none)" : name, "plus_names.bin's name isn't the tsv's");
        }
        page_named += !first;
    }
    if (at != size) fail_name(argv[2], "bytes past the 65,536th language");

    int named = 0;
    for (int c = 1; c < 16; c++) {
        if (by_cost[c] == 0) continue;
        printf("cost %d: %d\n", c, by_cost[c]);
        named += by_cost[c];
    }
    printf("none: %d\n", 65536 - named);
    if (failures) {
        fprintf(stderr, "plus_names_check: %d failures\n", failures);
        return 1;
    }
    printf("ok: %d names parsed back to their sets; the page's table holds the %d of cost %d or less\n", lines, page_named,
           page_cost);
    return 0;
}
