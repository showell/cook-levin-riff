// boolean_main.c: the book's Boolean tables, printed as Markdown.
//
//   boolean_main
//
// The table of the 16 languages, each with its Boolean expression of AX's
// two bits, hb and lb, from boolean.c, each expression checked against the
// machine first: the language some program accepts. book/book.shale holds
// the book's tables to this output, byte for byte.
// A unity build, as main.c is.

#include "vm.c"
#include "boolean.c"

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
    printf("| code | language | as hb and lb |\n");
    printf("|---|---|---|\n");
    for (int code = 0; code < NUM_LANGUAGES; code++) {
        const char *text = LANGUAGE_EXPRESSIONS[code];
        Program p = a_program_deciding(code);
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
    return fflush(stdout) == 0 ? 0 : 1;
}
