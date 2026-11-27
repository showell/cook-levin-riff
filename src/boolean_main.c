// boolean_main.c: the book's Boolean tables, printed as Markdown.
//
//   boolean_main
//
// Four blocks, each checked before it is printed: the 16 problems, each
// with its Boolean expression of AX's two bits, hb and lb, held to a program
// that decides it; for each problem, how many programs decide it and how
// many products its sum of products over v1 to v12 takes; and the sums of
// products of the two problems in SHOWN, each held to the machine's
// programs exactly, {0, 2, 3}'s titled as a "1689, here" box, since its
// one product is program 1689. Before them it holds src/decides.c's step expressions
// to src/stepper.c's step on all 64 settings, since the sums of products
// are built from them; the book shows the step from python/steps.py.
// book/book.shale holds the book's copies to this output, byte for byte.
// A unity build, as main.c is.

#include "vm.c"
#include "stepper.c"
#include "poly.c"
#include "ring_int.c"
#include "boolean.c"
#include "decides.c"

// The problems whose sums of products the book prints, by code.
static const int SHOWN[] = {13, 9};

static void print_language(int code) {
    printf("{");
    int first = 1;
    for (int n = 0; n < NUM_INPUTS; n++) {
        if (code & (1 << n)) {
            printf(first ? "%d" : ", %d", n);
            first = 0;
        }
    }
    printf(first ? " }" : "}");
}

// A program that decides language `code`, from the search.
static Program a_program_deciding(int code) {
    for (int n = 0; n < NUM_PROGRAMS; n++) {
        Program p = disassemble((uint32_t)n);
        if ((int)encoded_language(get_language_that_program_accepts(p)) == code) return p;
    }
    fprintf(stderr, "boolean_main: no program decides language %d\n", code);
    exit(1);
    return disassemble(0);
}

int main(void) {
    printf("| code | problem | as hb and lb |\n");
    printf("|---|---|---|\n");
    for (int code = 0; code < NUM_LANGUAGES; code++) {
        const char *text = LANGUAGE_EXPRESSIONS[code];
        Program p = a_program_deciding(code);
        if (language_of_expression(text) != code) {
            fprintf(stderr, "boolean_main: %s is the language %d\n", text, language_of_expression(text));
            return 1;
        }
        for (int n = 0; n < NUM_INPUTS; n++) {
            BoolEnv env = {0};
            bool_set(&env, "hb", n >> 1);
            bool_set(&env, "lb", n & 1);
            if (bool_eval(text, &env) != (run_progam(n, p) == STATUS_ACCEPTED)) {
                fprintf(stderr, "boolean_main: %s differs from the machine at %d\n", text, n);
                return 1;
            }
        }
        printf("| %d | ", code);
        print_language(code);
        printf(" | `%s` |\n", text);
    }

    for (int bits = 0; bits < 64; bits++) {
        int hb = bits & 1, lb = (bits >> 1) & 1, halted = (bits >> 2) & 1;
        int accepted = (bits >> 3) & 1, op_hb = (bits >> 4) & 1, op_lb = (bits >> 5) & 1;
        StepState s = step_compiled(2 * hb + lb, halted, accepted, (Op)(2 * op_hb + op_lb));
        int want = 8 * (s.ax >> 1) + 4 * (s.ax & 1) + 2 * (s.halted != 0) + (s.accepted != 0);
        if (step_by_expressions(hb, lb, halted, accepted, op_hb, op_lb) != want) {
            fprintf(stderr, "boolean_main: the step's expressions differ from the stepper at %d\n", bits);
            return 1;
        }
    }
    static unsigned char is_true[NUM_LANGUAGES][POINTS];
    static Cover covers[NUM_LANGUAGES];
    printf("\n| code | problem | programs | products |\n");
    printf("|---|---|---|---|\n");
    int all = 0;
    for (int code = 0; code < NUM_LANGUAGES; code++) {
        points_deciding(code, is_true[code]);
        cover_points(is_true[code], &covers[code]);
        if (!cover_is_exact(&covers[code], is_true[code])) {
            fprintf(stderr, "boolean_main: language %d's cover differs from its programs\n", code);
            return 1;
        }
        int programs = 0;
        for (int point = 0; point < POINTS; point++) programs += is_true[code][point];
        all += programs;
        printf("| %d | ", code);
        print_language(code);
        printf(" | %d | %d |\n", programs, covers[code].count);
    }
    if (all != NUM_PROGRAMS) {
        fprintf(stderr, "boolean_main: the languages hold %d programs\n", all);
        return 1;
    }
    for (size_t i = 0; i < sizeof SHOWN / sizeof SHOWN[0]; i++) {
        const Cover *c = &covers[SHOWN[i]];
        printf(SHOWN[i] == 13 ? "\n```text 1689, here: the sum of products of {0, 2, 3}, one product\n" : "\n```text\n");
        for (int k = 0; k < c->count; k++) {
            char text[32];
            cube_text(c->cubes[k], text);
            printf("%s\n", text);
        }
        printf("```\n");
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
