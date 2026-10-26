/* main.c — Shreyansh's unified driver (codegen pipeline)
 *
 * Contract from Balraj/Ayansh:
 * - Call semantic_check() after parsing; do NOT call collect_decls()
 * - semantic_check() declares symbols itself in source order
 * - Only run codegen if semantic_check() returns 0
 * - Keep greppable lines: "front end ok: N symbol(s)" and "semantic ok: ..."
 * - Exit codes: parse error -> 3, semantic error -> 1, clean -> 0
 */

#include "ast.h"
#include "semantic.h"
#include "symtab.h"
#include "common.h"
#include "codegen.h"

extern FILE  *yyin;
extern int    yyparse(void);
extern AstNode *g_program;
extern int      g_parse_errors;

int main(int argc, char **argv) {
    FILE *fp;
    SymTab *st;
    int sem_errors = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s program.dim [out.c]\n", argv[0]);
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
        ast_free(g_program);
        g_program = NULL;
        fclose(fp);
        return EXIT_BUILD_ERROR;
    }

    st = symtab_create();
    if (!st) {
        DIM_ERR("failed to create symbol table");
        ast_free(g_program);
        g_program = NULL;
        fclose(fp);
        return EXIT_BUILD_ERROR;
    }

    sem_errors = semantic_check(st, g_program);

    printf("front end ok: %zu symbol(s)\n", symtab_count(st));
    if (sem_errors == 0) {
        printf("semantic ok: %zu symbol(s), 0 error(s)\n", symtab_count(st));
    } else {
        DIM_ERR("semantic check failed with %d error(s)", sem_errors);
    }

    if (sem_errors == 0) {
        const char *outname = (argc >= 3) ? argv[2] : "output/out.c";
        codegen_emit(g_program, outname);
        symtab_destroy(st);
        ast_free(g_program);
        g_program = NULL;
        fclose(fp);
        return EXIT_COMPILE_OK;
    }

    symtab_destroy(st);
    ast_free(g_program);
    g_program = NULL;
    fclose(fp);
    return EXIT_COMPILE_ERROR;
}
