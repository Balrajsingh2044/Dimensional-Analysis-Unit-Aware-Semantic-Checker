/* frontend_main.c — Phase-3 checker driver (Balraj).
 *
 * Parser + semantic analyzer, printed ASTs and symbolic dumps for debugging.
 * TWO binaries on purpose (tooling decision, see Makefile):
 *   out/dimc-checker : THIS driver  (front end + analyzer; debug-friendly)
 *   out/dimc         : Shreyansh's main.c (same skeleton minus the dumps,
 *                      plus codegen) — final project deliverable
 *
 * BUILD:  make checker FILE=tests/valid/00_basic_fe_check.dim
 * RUN :   ./output/dimc-checker program.dim
 *
 * ── Test-runner contract (frozen; do not change formats) ──────────────────
 * exit 0  and BOTH lines on stdout iff parse AND semantic analysis are clean:
 *     front end ok: <N> symbol(s)
 *     semantic ok:  <N> symbol(s), 0 error(s)   (tag: 'semantic ok:')
 * Parse failure  -> NO such lines, exit 3, 'ERROR: parse error ...' on stderr.
 * Semantic error -> 'front end ok:' still printed, NO 'semantic ok:' line,
 *                   'ERROR: semantic check failed…' on stderr, exit 1.
 * Every other ERROR/ WARNING diagnostic goes to stderr.
 * Grep guidance for run_tests.sh: grep -F 'front end ok:' — never parse
 * the AST dump; the AST dump format is debug output only.
 */
#include "ast.h"
#include "semantic.h"   /* Ayansh's analyzer — hooked in at Integration Point #2 */
#include "symtab.h"
#include "common.h"

extern FILE  *yyin;        /* set by the driver; read by yylex */
extern int    yyparse(void);
extern AstNode *g_program;      /* produced by parser.y */
extern int      g_parse_errors;
extern void     lexer_release_scanner(void);  /* flex scanner teardown */

/* ------------------------------------------------ declaration seeding */

/* NOTE (Ayansh -> Balraj): collect_decls() is RETIRED as of the semantic
 * hook-up. It seeded every `let` with a placeholder unit BEFORE analysis,
 * which (a) made use-before-declare undetectable and (b) would collide
 * with semantic_check()'s own declare-on-`let` (false redeclarations).
 * The analyzer now owns symbol-table population in source order.
 * If you want the old front-end-only behavior, use `make fe FILE=...`
 * against a checkout before this change, or pass a flag later.
 */
#if 0
static void collect_decls(const AstNode *prog, SymTab *st) {
    const AstNode *stmt;

    for (stmt = prog; stmt; stmt = stmt->next) {
        switch (stmt->kind) {
            case AST_LET:
                /* 'let' = declaration. Unknown unit for now (dimensionless
                 * placeholder) until the analyzer folds the rhs. */
                if (!symtab_insert(st, stmt->name, TYPE_DOUBLE,
                                   uv_dimensionless(), stmt->line))
                    DIM_WARN("line %d: redeclaration of '%s' (ignored)",
                             stmt->line, stmt->name);
                break;

            case AST_ASSIGN:
                /* plain assignment: target must already exist (this much
                 * scope checking is front-end, the rest is Ayansh's job) */
                if (!symtab_lookup(st, stmt->name))
                    DIM_ERR("line %d: assignment to undeclared identifier '%s'",
                            stmt->line, stmt->name);
                break;

            default:
                break;   /* bare expression statements carry no symbols */
        }
    }
}

#endif /* retired collect_decls */

/* ------------------------------------------------ main */

/* Two drivers share this skeleton — BUILD_CHECKER selects which binary is
 * the debug checker (with AST/symtab dumps + contract lines). The unified
 * ./dimc defines its own quiet main() (src/main.c). */
#if defined(BUILD_CHECKER)
int main(int argc, char **argv) {
    FILE *fp;
    SymTab *st;
    int exit_code = EXIT_COMPILE_OK;

    if (argc != 2) {
        fprintf(stderr, "usage: %s program.dim\n", argv[0]);
        return EXIT_USAGE;
    }

    fp = fopen(argv[1], "r");
    if (!fp) {
        DIM_ERR("cannot open input file '%s'", argv[1]);
        return EXIT_USAGE;
    }
    yyin = fp;

    if (yyparse() != 0 || g_parse_errors > 0) {
        DIM_ERR("front end failed with %d parse error(s)", g_parse_errors);
        exit_code = EXIT_BUILD_ERROR;
    } else {
        int sem_errors;
        ast_print(g_program, stdout);

        /* Ayansh's analyzer: folds units, declares on `let`, reports errors */
        st = symtab_create();
        sem_errors = semantic_check(st, g_program);
        symtab_print(st, stdout);

        /* snapshot lines for the test runner (keep both greppable) */
        printf("front end ok: %zu symbol(s)\n", symtab_count(st));
        if (sem_errors == 0)
            printf("semantic ok: %zu symbol(s), 0 error(s)\n", symtab_count(st));
        else
            DIM_ERR("semantic check failed with %d error(s)", sem_errors);
        exit_code = (sem_errors > 0) ? EXIT_COMPILE_ERROR : EXIT_COMPILE_OK;

        symtab_destroy(st);
    }

    ast_free(g_program);
    g_program = NULL;
    fclose(fp);
    lexer_release_scanner();        /* free flex's scanner buffer at exit */
    return exit_code;
}
#endif /* BUILD_CHECKER */
