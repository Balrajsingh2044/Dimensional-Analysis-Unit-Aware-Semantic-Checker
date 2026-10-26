/* frontend_main.c — front-end test driver (Balraj).
 *
 * Runs the lexer + parser on a .dim file, prints the AST, seeds the symbol
 * table from the declarations, and dumps it. This is the Week-2 milestone
 * ("parser feeds AST; symbol table works") WITHOUT Ayansh's analyzer —
 * units stay "(unfilled)" until semantic.c hooks in here.
 *
 * Build:  make dimc        Run:  ./output/dimc program.dim
 */
#include "ast.h"
#include "semantic.h"   /* Ayansh's analyzer — hooked in at Integration Point #2 */
#include "symtab.h"
#include "common.h"

extern FILE  *yyin;        /* set by the driver; read by yylex */
extern int    yyparse(void);
extern AstNode *g_program;      /* produced by parser.y */
extern int      g_parse_errors;

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
    return exit_code;
}
