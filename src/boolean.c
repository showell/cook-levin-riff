// boolean.c: Boolean expressions as the book writes them, and their values.
// An expression is text over named bits with NOT, AND, OR and XOR,
// parentheses, and the constants 0 and 1. AND binds tighter than OR and
// XOR, which are read left to right, and NOT tightest: `NOT hb AND lb` is
// (NOT hb) AND lb. The book's tables print the same strings this file
// evaluates, so a table and its check have one text.
//
// A unity build's piece: a file that needs it includes it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BOOL_MAX_NAMES = 64, BOOL_NAME_LEN = 32 };

// Named bits, as an expression reads them.
typedef struct {
    int count;
    char names[BOOL_MAX_NAMES][BOOL_NAME_LEN];
    int values[BOOL_MAX_NAMES];
} BoolEnv;

static void bool_set(BoolEnv *env, const char *name, int value) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            env->values[i] = value;
            return;
        }
    }
    if (env->count == BOOL_MAX_NAMES || strlen(name) >= BOOL_NAME_LEN) {
        fprintf(stderr, "boolean: no room for the name %s\n", name);
        exit(1);
    }
    strcpy(env->names[env->count], name);
    env->values[env->count] = value;
    env->count++;
}

typedef struct {
    const char *at;
    const BoolEnv *env;
    const char *text;
} BoolParse;

static void bool_stop(const BoolParse *p, const char *why) {
    fprintf(stderr, "boolean: %s at \"%s\" in \"%s\"\n", why, p->at, p->text);
    exit(1);
}

static void bool_skip_spaces(BoolParse *p) {
    while (*p->at == ' ') p->at++;
}

static int bool_is_name_byte(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '\'';
}

// The next word, a name or an operator, copied into `word`; its length.
static size_t bool_peek_word(BoolParse *p, char *word) {
    bool_skip_spaces(p);
    size_t n = 0;
    while (bool_is_name_byte(p->at[n]) && n + 1 < BOOL_NAME_LEN) {
        word[n] = p->at[n];
        n++;
    }
    word[n] = 0;
    return n;
}

static int bool_expr(BoolParse *p);

static int bool_factor(BoolParse *p) {
    char word[BOOL_NAME_LEN];
    size_t n = bool_peek_word(p, word);
    if (n == 0) {
        if (*p->at != '(') bool_stop(p, "expected a name, NOT or (");
        p->at++;
        int v = bool_expr(p);
        bool_skip_spaces(p);
        if (*p->at != ')') bool_stop(p, "expected )");
        p->at++;
        return v;
    }
    p->at += n;
    if (strcmp(word, "NOT") == 0) return !bool_factor(p);
    if (strcmp(word, "0") == 0) return 0;
    if (strcmp(word, "1") == 0) return 1;
    for (int i = 0; i < p->env->count; i++) {
        if (strcmp(p->env->names[i], word) == 0) return p->env->values[i];
    }
    p->at -= n;
    bool_stop(p, "an unknown name");
    return 0;
}

static int bool_term(BoolParse *p) {
    int v = bool_factor(p);
    char word[BOOL_NAME_LEN];
    for (;;) {
        size_t n = bool_peek_word(p, word);
        if (strcmp(word, "AND") != 0) return v;
        p->at += n;
        int w = bool_factor(p);
        v = v && w;
    }
}

static int bool_expr(BoolParse *p) {
    int v = bool_term(p);
    char word[BOOL_NAME_LEN];
    for (;;) {
        size_t n = bool_peek_word(p, word);
        if (strcmp(word, "OR") == 0) {
            p->at += n;
            int w = bool_term(p);
            v = v || w;
        } else if (strcmp(word, "XOR") == 0) {
            p->at += n;
            int w = bool_term(p);
            v = v != w;
        } else {
            return v;
        }
    }
}

// The value of `text` with the names `env` gives; a malformed text or an
// unknown name stops the program, named.
static int bool_eval(const char *text, const BoolEnv *env) {
    BoolParse p = {text, env, text};
    int v = bool_expr(&p);
    bool_skip_spaces(&p);
    if (*p.at != 0) bool_stop(&p, "text left over");
    return v;
}

// Each language over {0, 1, 2, 3} as a Boolean expression of AX's two bits,
// hb and lb, by language code: input n = 2·hb + lb is in the language when
// the expression is 1 there.
static const char *const LANGUAGE_EXPRESSIONS[16] = {
    "0",
    "NOT (hb OR lb)",
    "NOT hb AND lb",
    "NOT hb",
    "hb AND NOT lb",
    "NOT lb",
    "hb XOR lb",
    "NOT (hb AND lb)",
    "hb AND lb",
    "NOT (hb XOR lb)",
    "lb",
    "NOT hb OR lb",
    "hb",
    "hb OR NOT lb",
    "hb OR lb",
    "1",
};

// The language code an expression of hb and lb decides: bit n set when it
// is 1 at n's two bits.
static int language_of_expression(const char *text) {
    int code = 0;
    for (int n = 0; n < 4; n++) {
        BoolEnv env = {0};
        bool_set(&env, "hb", n >> 1);
        bool_set(&env, "lb", n & 1);
        if (bool_eval(text, &env)) code |= 1 << n;
    }
    return code;
}
