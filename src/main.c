/* main.c — final unified pipeline driver (Shreyansh's slot; wired up in the
 * Week-4 integration pass so ./dimc could materialize as agreed — no
 * semantic/design change, just the calling order we all signed off on).
 *
 *   dimc FILE.dim [out.c]
 *   dimc -p FILE.dim [out.c]        (Week-4 additive: also print the most
 *                                    recently assigned variable — the
 *                                    "compiler hands you the answer" flag)
 *   dimc -r FILE.dim                (Week-5 additive: interpreter mode —
 *                                    evaluate the math at compile time and
 *                                    print "Result: ... (unit)" on stdout)
 *   dimc --run FILE.dim             (same as -r)
 *
 *   1. lex + parse                  → exit 3 on parse error (stderr only)
 *   2. semantic_check(fresh SymTab) → exit 1 on any unit violation
 *   3a. default:   codegen_emit(): unit-stripped C to stdout/out.c
 *   3b. -r/--run:  eval_run(): interpret the AST, print Result lines
 *   4. exit 0 — stdout is either pure C (quiet, diff-friendly) or pure
 *      Result lines (every diagnostic stays on stderr).
 *
 * NOTE: unlike dimc-checker there are NO debug dumps and no "front end ok:"
 * line on stdout — the test-runner contract lines belong to dimc-checker.
 */
#include "ast.h"
#include "symtab.h"
#include "semantic.h"
#include "codegen.h"
#include "evaluator.h"
#include "common.h"

extern FILE     *yyin;         /* set by driver; read by yylex */
extern int       yyparse(void);
extern AstNode  *g_program;    /* produced by parser.y */
extern int       g_parse_errors;
extern void      lexer_release_scanner(void);   /* flex scanner teardown */

int main(int argc, char **argv) {
    FILE   *fp;
    SymTab *st;
    int     sem_errors;
    int     cg;
    int     idx = 1;          /* first positional argument */
    int     print_last = 0;
    int     run_mode = 0;     /* Week-5: -r / --run selects the interpreter */

    /* Flags are additive and optional; all original 2-/3-argument forms
     * behave exactly as before. */
    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "-p") == 0) {
            print_last = 1;
        } else if (strcmp(argv[idx], "-r") == 0 ||
                   strcmp(argv[idx], "--run") == 0) {
            run_mode = 1;
        } else {
            fprintf(stderr, "usage: %s [-p] [-r] program.dim [out.c]\n"
                            "Unknown flag '%s'\n", argv[0], argv[idx]);
            return EXIT_USAGE;
        }
        idx++;
    }
    if (argc != idx + 1 && argc != idx + 2) {
        fprintf(stderr, "usage: %s [-p] [-r] program.dim [out.c]\n", argv[0]);
        return EXIT_USAGE;
    }

    fp = fopen(argv[idx], "r");
    if (!fp) {
        DIM_ERR("cannot open input file '%s'", argv[idx]);
        return EXIT_USAGE;
    }
    yyin = fp;

    /* ── 1. parse ── */
    if (yyparse() != 0 || g_parse_errors > 0) {
        DIM_ERR("front end failed with %d parse error(s)", g_parse_errors);
        ast_free(g_program);
        g_program = NULL;
        fclose(fp);
        lexer_release_scanner();
        return EXIT_BUILD_ERROR;
    }

    /* ── 2. analyze (fresh table: analyzer declares on `let` itself) ── */
    st = symtab_create();
    sem_errors = semantic_check(st, g_program);
    if (sem_errors > 0) {
        DIM_ERR("pipeline stopped: %d semantic error(s), not %s",
                sem_errors, run_mode ? "evaluating" : "emitting C");
        symtab_destroy(st);
        ast_free(g_program);
        g_program = NULL;
        fclose(fp);
        lexer_release_scanner();
        return EXIT_COMPILE_ERROR;
    }

    /* ── 3a. interpreter mode: run the math, print Results ── */
    if (run_mode) {
        int eval_errors = eval_run(g_program, st, stdout);
        symtab_destroy(st);
        ast_free(g_program);
        g_program = NULL;
        fclose(fp);
        lexer_release_scanner();
        if (eval_errors > 0)
            DIM_ERR("evaluation stopped with %d error(s)", eval_errors);
        return eval_errors > 0 ? EXIT_COMPILE_ERROR : EXIT_COMPILE_OK;
    }
    symtab_destroy(st);      /* codegen reads AST units, not the table */

    /* ── 3b. emit (default behavior, unchanged) ── */
    cg = codegen_emit_print(g_program, argc == idx + 2 ? argv[idx + 1] : NULL,
                            print_last);

    ast_free(g_program);
    g_program = NULL;
    fclose(fp);
    lexer_release_scanner();        /* free flex's scanner buffer at exit */
    return cg == 0 ? EXIT_COMPILE_OK : EXIT_BUILD_ERROR;
}
