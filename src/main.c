/* main.c — final unified pipeline driver (Shreyansh's slot; wired up in the
 * Week-4 integration pass so ./dimc could materialize as agreed — no
 * semantic/design change, just the calling order we all signed off on).
 *
 *   dimc FILE.dim [out.c]
 *
 *   1. lex + parse                  → exit 3 on parse error (stderr only)
 *   2. semantic_check(fresh SymTab) → exit 1 on any unit violation
 *   3. codegen_emit(): prints unit-stripped C to stdout (or out.c with a
 *      second argument)
 *   4. exit 0 — and stdout is pure C (quiet, diff-friendly).
 *
 * NOTE: unlike dimc-checker there are NO debug dumps and no "front end ok:"
 * line on stdout — the test-runner contract lines belong to dimc-checker.
 */
#include "ast.h"
#include "symtab.h"
#include "semantic.h"
#include "codegen.h"
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

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: %s program.dim [out.c]\n", argv[0]);
        return EXIT_USAGE;
    }

    fp = fopen(argv[1], "r");
    if (!fp) {
        DIM_ERR("cannot open input file '%s'", argv[1]);
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
    symtab_destroy(st);      /* codegen reads AST units, not the table */
    if (sem_errors > 0) {
        DIM_ERR("pipeline stopped: %d semantic error(s), not emitting C",
                sem_errors);
        ast_free(g_program);
        g_program = NULL;
        fclose(fp);
        lexer_release_scanner();
        return EXIT_COMPILE_ERROR;
    }

    /* ── 3. emit ── */
    cg = codegen_emit(g_program, argc == 3 ? argv[2] : NULL);

    ast_free(g_program);
    g_program = NULL;
    fclose(fp);
    lexer_release_scanner();        /* free flex's scanner buffer at exit */
    return cg == 0 ? EXIT_COMPILE_OK : EXIT_BUILD_ERROR;
}
