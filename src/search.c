// search.c: find_solutions (search.h).

#include "search.h"

#include "stepper.h"

#include <stdlib.h>

void solutions_fill(Solutions *s) {
    for (int y = 0; y < NUM_LANGUAGES; y++) s->count[y] = 0;
    for (int n = 0; n < NUM_PROGRAMS; n++) {
        Lang lang = get_language_that_program_accepts(disassemble((uint32_t)n));
        uint32_t code = encoded_language(lang);
        s->programs[code][s->count[code]++] = n;
    }
}

void print_lang(FILE *out, Lang lang) {
    fputc('[', out);
    for (size_t i = 0; i < lang.len; i++) {
        if (i > 0) fputs(", ", out);
        fprintf(out, "%d", lang.items[i]);
    }
    fputc(']', out);
}

// Python's find_solutions asserts, for the example program and each input
// 0..3, that `stepper.test_with_stepper(program, i)` equals `i in lang`.
// It prints nothing, so the golden output does not depend on it; a
// disagreement stops the program, as Python's assert raises.
// tests/test_cross_check.c holds every program, not only the examples.
static void cross_check_with_stepper(Program example, Lang lang) {
    for (int i = 0; i < NUM_INPUTS; i++) {
        int accepted = test_with_stepper(example, i);
        if (accepted != (lang_has(lang, i) != 0)) {
            fprintf(stderr, "find_solutions: the stepper gives %d on input %d, the language says %d\n", accepted, i,
                    lang_has(lang, i));
            abort();
        }
    }
}

void find_solutions(FILE *out) {
    // Static: 16 x 4,096 ints is 256 KiB, too much for a stack frame.
    static Solutions s;
    solutions_fill(&s);

    for (int y = 0; y < NUM_LANGUAGES; y++) {
        if (s.count[y] == 0) {
            fprintf(stderr, "find_solutions: no program accepts language %d\n", y);
            abort();
        }
    }

    for (int y = 0; y < NUM_LANGUAGES; y++) {
        Lang lang = language((uint32_t)y);
        Lang rejected_lang = complement(lang);
        print_lang(out, lang);
        fprintf(out, " is solved by %zu possible program\n", s.count[y]);
        fputs("See an example program below.\n", out);
        fputs("   it accepts ", out);
        print_lang(out, lang);
        fputs("\n   it rejects ", out);
        print_lang(out, rejected_lang);
        fputs("\n--\n", out);
        Program example_program = disassemble((uint32_t)s.programs[y][0]);
        for (int i = 0; i < MAX_STEPS; i++) {
            Op cmd = example_program.ops[i];
            fprintf(out, "%s %s\n", op_name(cmd), op_comment(cmd));
        }
        fputs("--\n\n", out);

        cross_check_with_stepper(example_program, lang);
    }
}
