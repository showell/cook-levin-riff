// plus_names.c: a predicate-style name for each of Nibble++'s 65,536
// languages (subsets of the inputs 0 to 15), worked out ahead of time for
// the Nibble++ sandbox (web/plus.html). docs/NIBBLE_PLUS_NAMES.md has the
// vocabulary, the grammar and the cost.
//
//   plus_names <names.tsv> <plus_names.bin>
//
// writes every named language as `<code>\t<cost>\t<name>`, in code order,
// for web/plus_names_check.c and the coverage report, and the page's table,
// plus_names.bin: its tokens (each an atom, like `from_3_to_9` or `fib`, or
// a word: `and`, `or`, `not`, `except`, `grumpy`, `always_true`), each
// ended by a newline, then a 0 byte; then for each language in code order
// its name's tokens, a byte each (token k is the k-th, from 1), then a 0
// byte; no tokens for a language the table doesn't name. A name is its
// tokens joined by `_`, after `is_` unless it starts with `has_`, `lacks_`,
// `grumpy` or `always_true`. It prints the coverage, a line a cost.
//
// The search is a census of names by cost, as src/census.c is of programs
// by length. A name is
//
//   is_<sum>[_except_<n>]
//   sum     = product (_or_ product)*
//   product = factor (_and_ factor)*
//   factor  = [not_] atom | [not_] <size>_<noun>
//
// and its cost is its atoms, with one more for each `not` and the `except`.
// Each class keeps, for every set, its cheapest name; a tie goes to the
// shorter name, then the one strcmp puts first. Layer c is built from the
// layers below it alone, so a set's name is final once its layer is done.
// A sum's last product and a product's last factor are its cheapest, which
// reaches every sum and product while halving the work.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETS 65536
#define FULL 0xffffu
#define NAME_MAX_LEN 120
// The largest cost the search names a set at, and the largest the page's
// table keeps: past it a name is mostly numbers and ranges, longer than the
// set it names.
#define MAX_COST 6
#define PAGE_COST 4
#define NONE 255

typedef struct {
    uint8_t cost[SETS];
    uint8_t len[SETS];
    // The sets of the parts this class's name joins, factors for a product
    // and products for a sum; one, the set itself, for a name taken whole.
    uint8_t num_parts[SETS];
    uint16_t parts[SETS][MAX_COST];
    char name[SETS][NAME_MAX_LEN];
} Class;

static Class factor_c, product_c, sum_c, final_c;

static int popcount(uint16_t s) {
    int n = 0;
    for (; s; s &= (uint16_t)(s - 1)) n++;
    return n;
}
static int lowest(uint16_t s) {
    int n = 0;
    while (!((s >> n) & 1)) n++;
    return n;
}

// The order a name's parts are written in: a `not_` factor after the
// others, then the larger set first, then the one with the smaller least
// member, then by strcmp.
static const Class *order_by;
static int part_order(const void *pa, const void *pb) {
    uint16_t a = *(const uint16_t *)pa, b = *(const uint16_t *)pb;
    const char *na = order_by->name[a], *nb = order_by->name[b];
    int nota = strncmp(na, "not_", 4) == 0, notb = strncmp(nb, "not_", 4) == 0;
    if (nota != notb) return nota - notb;
    if (popcount(a) != popcount(b)) return popcount(b) - popcount(a);
    if (lowest(a) != lowest(b)) return lowest(a) - lowest(b);
    return strcmp(na, nb);
}

// Set `s` in class `k` to the name its `num` parts of class `part` make,
// joined by `sep`, at `cost`, if that is cheaper, or as cheap and shorter,
// or as short and first by strcmp.
static void offer_parts(Class *k, uint16_t s, int cost, const Class *part, const uint16_t *parts, int num, const char *sep) {
    if (s == 0 || s == FULL || k->cost[s] < cost) return;
    size_t len = (size_t)(num - 1) * strlen(sep);
    for (int i = 0; i < num; i++) len += part->len[parts[i]];
    if (len >= NAME_MAX_LEN) {
        fprintf(stderr, "plus_names: a name is longer than %d\n", NAME_MAX_LEN - 1);
        exit(1);
    }
    if (k->cost[s] == cost && k->len[s] < len) return;
    uint16_t sorted[MAX_COST];
    memcpy(sorted, parts, sizeof(uint16_t) * (size_t)num);
    order_by = part;
    qsort(sorted, (size_t)num, sizeof(uint16_t), part_order);
    char buf[NAME_MAX_LEN];
    size_t at = 0;
    for (int i = 0; i < num; i++) {
        if (i) {
            memcpy(buf + at, sep, strlen(sep));
            at += strlen(sep);
        }
        memcpy(buf + at, part->name[sorted[i]], part->len[sorted[i]]);
        at += part->len[sorted[i]];
    }
    buf[at] = 0;
    if (k->cost[s] == cost && k->len[s] == len && strcmp(k->name[s], buf) <= 0) return;
    k->cost[s] = (uint8_t)cost;
    k->len[s] = (uint8_t)len;
    k->num_parts[s] = (uint8_t)num;
    memcpy(k->parts[s], sorted, sizeof(uint16_t) * (size_t)num);
    memcpy(k->name[s], buf, len + 1);
}

// A name taken whole: the parts `a`, `sep` and `b` joined.
static void offer(Class *k, uint16_t s, int cost, const char *a, const char *sep, const char *b) {
    if (s == 0 || s == FULL) return;
    size_t la = strlen(a), ls = strlen(sep), lb = strlen(b), len = la + ls + lb;
    if (len >= NAME_MAX_LEN) {
        fprintf(stderr, "plus_names: a name is longer than %d: %s%s%s\n", NAME_MAX_LEN - 1, a, sep, b);
        exit(1);
    }
    if (k->cost[s] < cost) return;
    char buf[NAME_MAX_LEN];
    memcpy(buf, a, la);
    memcpy(buf + la, sep, ls);
    memcpy(buf + la + ls, b, lb);
    buf[len] = 0;
    if (k->cost[s] == cost) {
        if (k->len[s] < len) return;
        if (k->len[s] == len && strcmp(k->name[s], buf) <= 0) return;
    }
    k->cost[s] = (uint8_t)cost;
    k->len[s] = (uint8_t)len;
    k->num_parts[s] = 1;
    k->parts[s][0] = s;
    memcpy(k->name[s], buf, len + 1);
}

static void clear(Class *k) {
    memset(k->cost, NONE, sizeof k->cost);
}

static uint16_t set_where(int (*holds)(int, int), int arg) {
    uint16_t s = 0;
    for (int n = 0; n < 16; n++) if (holds(n, arg)) s |= (uint16_t)(1u << n);
    return s;
}
static int is_mult(int n, int k) { return n % k == 0; }
static int has_bit(int n, int k) { return (n >> k) & 1; }
static int lacks_bit(int n, int k) { return !((n >> k) & 1); }
static int at_least(int n, int k) { return n >= k; }
static int below(int n, int k) { return n < k; }

// {a, ..., b}.
static uint16_t range(int a, int b) {
    uint16_t s = 0;
    for (int n = a; n <= b; n++) s |= (uint16_t)(1u << n);
    return s;
}
static uint16_t of(const int *ns, int count) {
    uint16_t s = 0;
    for (int i = 0; i < count; i++) s |= (uint16_t)(1u << ns[i]);
    return s;
}

typedef struct {
    uint16_t set;
    char name[24];
} Word;

// The nouns an adjective may stand before, and the adjectives.
static Word nouns[16], sizes[5], atoms[256];
static int num_nouns, num_atoms;

static void add(Word *list, int *count, uint16_t set, const char *name) {
    list[*count].set = set;
    snprintf(list[*count].name, sizeof list[*count].name, "%s", name);
    (*count)++;
}

static void vocabulary(void) {
    static const int primes[] = {2, 3, 5, 7, 11, 13}, squares[] = {0, 1, 4, 9}, fibs[] = {1, 2, 3, 5, 8, 13},
                     powers[] = {1, 2, 4, 8};
    char name[24];
    add(nouns, &num_nouns, set_where(is_mult, 2), "even");
    add(nouns, &num_nouns, (uint16_t)~set_where(is_mult, 2), "odd");
    add(nouns, &num_nouns, of(primes, 6), "prime");
    add(nouns, &num_nouns, of(squares, 4), "square");
    add(nouns, &num_nouns, of(fibs, 6), "fib");
    add(nouns, &num_nouns, of(powers, 4), "power_of_2");
    for (int k = 3; k <= 8; k++) {
        snprintf(name, sizeof name, "mult_of_%d", k);
        add(nouns, &num_nouns, set_where(is_mult, k), name);
    }
    int num_sizes = 0;
    add(sizes, &num_sizes, range(0, 3), "tiny");
    add(sizes, &num_sizes, range(0, 7), "small");
    add(sizes, &num_sizes, range(4, 11), "medium");
    add(sizes, &num_sizes, range(8, 15), "large");
    add(sizes, &num_sizes, range(12, 15), "huge");
    for (int i = 0; i < num_nouns; i++) add(atoms, &num_atoms, nouns[i].set, nouns[i].name);
    for (int i = 0; i < 5; i++) add(atoms, &num_atoms, sizes[i].set, sizes[i].name);
    for (int k = 0; k < 4; k++) {
        snprintf(name, sizeof name, "has_bit_%d", k);
        add(atoms, &num_atoms, set_where(has_bit, k), name);
        snprintf(name, sizeof name, "lacks_bit_%d", k);
        add(atoms, &num_atoms, set_where(lacks_bit, k), name);
    }
    for (int k = 1; k <= 15; k++) {
        snprintf(name, sizeof name, "at_least_%d", k);
        add(atoms, &num_atoms, set_where(at_least, k), name);
        snprintf(name, sizeof name, "below_%d", k);
        add(atoms, &num_atoms, set_where(below, k), name);
    }
    for (int a = 0; a <= 15; a++) {
        for (int b = a + 1; b <= 15; b++) {
            snprintf(name, sizeof name, "from_%d_to_%d", a, b);
            add(atoms, &num_atoms, range(a, b), name);
        }
    }
    for (int n = 0; n <= 15; n++) {
        snprintf(name, sizeof name, "%d", n);
        add(atoms, &num_atoms, (uint16_t)(1u << n), name);
    }
}

// Every set of class `k` at exactly `cost`.
static int at_cost(const Class *k, int cost, uint16_t *out) {
    int count = 0;
    for (int s = 0; s < SETS; s++) if (k->cost[s] == cost) out[count++] = (uint16_t)s;
    return count;
}

static uint16_t left[SETS], right[SETS];

// Class `to` from `from` (its single members) joined by `op` (and: 1, or: 0)
// with the members of `part`, layer by layer.
static void build(Class *to, const Class *from, const Class *part, int is_and, const char *sep) {
    clear(to);
    for (int s = 0; s < SETS; s++) {
        if (from->cost[s] != NONE) offer(to, (uint16_t)s, from->cost[s], from->name[s], "", "");
    }
    for (int c = 2; c <= MAX_COST; c++) {
        for (int b = 1; 2 * b <= c; b++) {
            int a = c - b;
            int nl = at_cost(to, a, left), nr = at_cost(part, b, right);
            for (int i = 0; i < nl; i++) {
                uint16_t x = left[i];
                for (int j = 0; j < nr; j++) {
                    uint16_t y = right[j];
                    uint16_t s = is_and ? (uint16_t)(x & y) : (uint16_t)(x | y);
                    if (s == x || s == y) continue;
                    if (to->cost[s] < c) continue;
                    uint16_t parts[MAX_COST];
                    int num = to->num_parts[x];
                    memcpy(parts, to->parts[x], sizeof(uint16_t) * (size_t)num);
                    parts[num++] = y;
                    offer_parts(to, s, c, part, parts, num, sep);
                }
            }
        }
    }
}

// The page table's tokens: the atoms, then the words between them.
static const char *const WORDS[] = {"and", "or", "not", "except", "grumpy", "always_true"};
#define NUM_WORDS 6

static void write_vocabulary(FILE *out) {
    if (num_atoms + NUM_WORDS > 255) {
        fprintf(stderr, "plus_names: %d tokens don't fit a byte\n", num_atoms + NUM_WORDS);
        exit(1);
    }
    for (int i = 0; i < num_atoms; i++) fprintf(out, "%s\n", atoms[i].name);
    for (int i = 0; i < NUM_WORDS; i++) fprintf(out, "%s\n", WORDS[i]);
    fputc(0, out);
}

// The token numbered from 1 that is `word`; one that isn't stops the program.
static int token_of(const char *word) {
    for (int i = 0; i < num_atoms; i++) if (strcmp(atoms[i].name, word) == 0) return i + 1;
    for (int i = 0; i < NUM_WORDS; i++) if (strcmp(WORDS[i], word) == 0) return num_atoms + i + 1;
    fprintf(stderr, "plus_names: no token %s\n", word);
    exit(1);
}

// How many of a name's `_`-separated words, from `w`, one token takes.
static int token_words(char **w, int left) {
    int n = 1;
    if (strcmp(w[0], "from") == 0) n = 4;
    else if (strcmp(w[0], "has") == 0 || strcmp(w[0], "lacks") == 0 || strcmp(w[0], "at") == 0 ||
             strcmp(w[0], "mult") == 0 || strcmp(w[0], "power") == 0) n = 3;
    else if (strcmp(w[0], "below") == 0 || strcmp(w[0], "always") == 0) n = 2;
    if (n > left) {
        fprintf(stderr, "plus_names: a name ends inside the token %s\n", w[0]);
        exit(1);
    }
    return n;
}

// `name`, without its `is_`, as its tokens' bytes.
static void write_tokens(FILE *out, const char *name) {
    char copy[NAME_MAX_LEN + 8], *w[NAME_MAX_LEN];
    snprintf(copy, sizeof copy, "%s", name);
    int num = 0;
    for (char *p = strtok(copy, "_"); p != NULL; p = strtok(NULL, "_")) w[num++] = p;
    for (int i = 0; i < num;) {
        int n = token_words(w + i, num - i);
        char token[NAME_MAX_LEN + 8] = "";
        for (int j = 0; j < n; j++) {
            if (j) strcat(token, "_");
            strcat(token, w[i + j]);
        }
        fputc(token_of(token), out);
        i += n;
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: plus_names <names.tsv> <plus_names.bin>\n");
        return 2;
    }
    vocabulary();
    clear(&factor_c);
    for (int i = 0; i < num_atoms; i++) {
        offer(&factor_c, atoms[i].set, 1, atoms[i].name, "", "");
        offer(&factor_c, (uint16_t)~atoms[i].set, 2, "not_", atoms[i].name, "");
    }
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < num_nouns; j++) {
            char adj[24];
            snprintf(adj, sizeof adj, "%s_", sizes[i].name);
            uint16_t s = sizes[i].set & nouns[j].set;
            offer(&factor_c, s, 2, adj, nouns[j].name, "");
            offer(&factor_c, (uint16_t)~s, 3, "not_", adj, nouns[j].name);
        }
    }
    build(&product_c, &factor_c, &factor_c, 1, "_and_");
    build(&sum_c, &product_c, &product_c, 0, "_or_");
    clear(&final_c);
    for (int s = 0; s < SETS; s++) {
        if (sum_c.cost[s] == NONE) continue;
        offer(&final_c, (uint16_t)s, sum_c.cost[s], sum_c.name[s], "", "");
    }
    for (int s = 0; s < SETS; s++) {
        if (sum_c.cost[s] == NONE || sum_c.cost[s] + 1 > MAX_COST) continue;
        for (int n = 0; n < 16; n++) {
            if (!((s >> n) & 1)) continue;
            char tail[16];
            snprintf(tail, sizeof tail, "_except_%d", n);
            offer(&final_c, (uint16_t)(s & ~(1u << n)), sum_c.cost[s] + 1, sum_c.name[s], tail, "");
        }
    }

    FILE *tsv = fopen(argv[1], "w"), *page = fopen(argv[2], "wb");
    if (tsv == NULL || page == NULL) {
        perror("plus_names");
        return 1;
    }
    write_vocabulary(page);
    int at[MAX_COST + 1] = {0}, unnamed = 0, kept = 0;
    for (int s = 0; s < SETS; s++) {
        char name[NAME_MAX_LEN + 8];
        int cost = final_c.cost[s];
        if (s == 0 || s == (int)FULL) {
            snprintf(name, sizeof name, "%s", s == 0 ? "grumpy" : "always_true");
            cost = 1;
        } else if (cost != NONE) {
            const char *n = final_c.name[s];
            int bare = strncmp(n, "has_", 4) == 0 || strncmp(n, "lacks_", 6) == 0;
            snprintf(name, sizeof name, "%s%s", bare ? "" : "is_", n);
        }
        if (cost == NONE) {
            unnamed++;
            fputc(0, page);
            continue;
        }
        at[cost]++;
        fprintf(tsv, "%d\t%d\t%s\n", s, cost, name);
        if (cost <= PAGE_COST) {
            kept++;
            write_tokens(page, s == 0 || s == (int)FULL ? name : final_c.name[s]);
        }
        fputc(0, page);
    }
    fclose(tsv);
    fclose(page);
    printf("atoms %d\n", num_atoms);
    for (int c = 1; c <= MAX_COST; c++) printf("cost %d: %d\n", c, at[c]);
    printf("none: %d\n", unnamed);
    printf("page: %d named, cost %d or less\n", kept, PAGE_COST);
    return 0;
}
