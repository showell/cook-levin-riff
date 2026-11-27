// decides.c: the step and the question "does this program decide L?" as
// Boolean expressions, for the chapter "The program left unknown".
// It reads boolean.c's expressions, and a unity build includes both.

#include <stdint.h>

// One step of the machine as Boolean expressions of its six input bits:
// AX's hb and lb, the flags halted and accepted, and the line's op_hb and
// op_lb. The first eight name small facts; the last four are the next
// state, read with the facts above them. src/stepper.c's step is the rule
// they are held to, on all 64 settings.
typedef struct {
    const char *name;
    const char *text;
} BoolDefinition;

enum { STEP_DEFINITIONS = 12 };

static const BoolDefinition STEP_EXPRESSIONS[STEP_DEFINITIONS] = {
    {"ax_is_0", "NOT hb AND NOT lb"},
    {"ax_is_2", "hb AND NOT lb"},
    {"ax_is_3", "hb AND lb"},
    {"is_zero", "NOT op_hb AND op_lb"},
    {"is_decr", "op_hb AND NOT op_lb"},
    {"is_mod2", "op_hb AND op_lb"},
    {"runs", "NOT halted"},
    {"keeps", "halted OR NOT op_hb"},
    {"hb'", "(hb AND keeps) OR (ax_is_3 AND is_decr AND runs)"},
    {"lb'", "(lb AND (keeps OR (is_mod2 AND runs))) OR (ax_is_2 AND is_decr AND runs)"},
    {"halted'", "halted OR (ax_is_0 AND is_decr)"},
    {"accepted'", "accepted OR (ax_is_0 AND is_zero AND runs)"},
};

// The next state's four bits, hb' lb' halted' accepted', as bits 3 to 0 of
// the answer, from the six input bits.
static int step_by_expressions(int hb, int lb, int halted, int accepted, int op_hb, int op_lb) {
    BoolEnv env = {0};
    bool_set(&env, "hb", hb);
    bool_set(&env, "lb", lb);
    bool_set(&env, "halted", halted);
    bool_set(&env, "accepted", accepted);
    bool_set(&env, "op_hb", op_hb);
    bool_set(&env, "op_lb", op_lb);
    for (int i = 0; i < STEP_DEFINITIONS; i++) {
        bool_set(&env, STEP_EXPRESSIONS[i].name, bool_eval(STEP_EXPRESSIONS[i].text, &env));
    }
    int out = 0;
    for (int i = STEP_DEFINITIONS - 4; i < STEP_DEFINITIONS; i++) {
        out = 2 * out + env.values[env.count - STEP_DEFINITIONS + i];
    }
    return out;
}

// A sum of products over the program's twelve bits v1 to v12, in the SAT
// chapter's numbering: instruction i is 2·v(2i−1) + v(2i). A point is a
// 12-bit number, bit k − 1 holding v(k). A cube is a product of literals,
// held as the bits it fixes (`care`) and their values (`value`).
enum { PROGRAM_BITS = 12, POINTS = 4096, CUBES = 531441, MAX_COVER = 4096 };

typedef struct {
    int care;
    int value;
} Cube;

typedef struct {
    int count;
    Cube cubes[MAX_COVER];
} Cover;

// The program a point names, as `disassemble` takes it.
static uint32_t program_of_point(int point) {
    uint32_t code = 0;
    for (int i = 0; i < MAX_STEPS; i++) {
        int op = 2 * ((point >> (2 * i)) & 1) + ((point >> (2 * i + 1)) & 1);
        code += (uint32_t)op << (2 * i);
    }
    return code;
}

// The points whose programs decide language `code`, by running them.
static void points_deciding(int code, unsigned char *is_true) {
    for (int point = 0; point < POINTS; point++) {
        Program p = disassemble(program_of_point(point));
        is_true[point] = (int)encoded_language(get_language_that_program_accepts(p)) == code;
    }
}

static Cube cube_of_index(int index) {
    Cube c = {0, 0};
    for (int k = 0; k < PROGRAM_BITS; k++) {
        int digit = index % 3;
        index /= 3;
        if (digit < 2) {
            c.care |= 1 << k;
            c.value |= digit << k;
        }
    }
    return c;
}

// How many of the points `uncovered` marks the cube holds.
static int cube_gain(Cube c, const unsigned char *uncovered) {
    int free_bits = ~c.care & (POINTS - 1);
    int gain = 0;
    int sub = free_bits;
    for (;;) {
        gain += uncovered[c.value | sub];
        if (sub == 0) break;
        sub = (sub - 1) & free_bits;
    }
    return gain;
}

static void cube_mark(Cube c, unsigned char *uncovered) {
    int free_bits = ~c.care & (POINTS - 1);
    int sub = free_bits;
    for (;;) {
        uncovered[c.value | sub] = 0;
        if (sub == 0) break;
        sub = (sub - 1) & free_bits;
    }
}

// A small sum of products for the true points: the prime implicants, then
// a greedy cover, the cube holding the most points left taken first.
static void cover_points(const unsigned char *is_true, Cover *out) {
    static unsigned char implicant[CUBES];
    static int powers[PROGRAM_BITS];
    powers[0] = 1;
    for (int k = 1; k < PROGRAM_BITS; k++) powers[k] = 3 * powers[k - 1];
    for (int index = 0; index < CUBES; index++) {
        int dash = -1;
        int point = 0;
        int left = index;
        for (int k = 0; k < PROGRAM_BITS; k++) {
            int digit = left % 3;
            left /= 3;
            if (digit == 2 && dash < 0) dash = k;
            if (digit == 1) point |= 1 << k;
        }
        if (dash < 0) {
            implicant[index] = is_true[point];
        } else {
            int low = index - 2 * powers[dash];
            implicant[index] = implicant[low] && implicant[low + powers[dash]];
        }
    }
    static Cube primes[CUBES];
    int prime_count = 0;
    for (int index = 0; index < CUBES; index++) {
        if (!implicant[index]) continue;
        int prime = 1;
        int left = index;
        for (int k = 0; k < PROGRAM_BITS && prime; k++) {
            int digit = left % 3;
            left /= 3;
            if (digit < 2 && implicant[index + (2 - digit) * powers[k]]) prime = 0;
        }
        if (prime) primes[prime_count++] = cube_of_index(index);
    }
    static unsigned char uncovered[POINTS];
    memcpy(uncovered, is_true, POINTS);
    out->count = 0;
    for (;;) {
        int best = -1;
        int best_gain = 0;
        for (int i = 0; i < prime_count; i++) {
            int gain = cube_gain(primes[i], uncovered);
            if (gain > best_gain) {
                best_gain = gain;
                best = i;
            }
        }
        if (best < 0) break;
        out->cubes[out->count++] = primes[best];
        cube_mark(primes[best], uncovered);
    }
}

// Whether the cover holds exactly the true points.
static int cover_is_exact(const Cover *cover, const unsigned char *is_true) {
    for (int point = 0; point < POINTS; point++) {
        int held = 0;
        for (int i = 0; i < cover->count && !held; i++) {
            held = (point & cover->cubes[i].care) == cover->cubes[i].value;
        }
        if (held != is_true[point]) return 0;
    }
    return 1;
}

// A cube as the book prints it: v1 to v12, two a line of the program, 1
// for v, 0 for NOT v and - for a bit the cube leaves free.
static void cube_text(Cube c, char *text) {
    int at = 0;
    for (int k = 0; k < PROGRAM_BITS; k++) {
        if (k > 0 && k % 2 == 0) text[at++] = ' ';
        text[at++] = (c.care >> k) & 1 ? (char)('0' + ((c.value >> k) & 1)) : '-';
    }
    text[at] = 0;
}
