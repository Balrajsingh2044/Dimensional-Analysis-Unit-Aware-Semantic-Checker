/* semantic_test.c — semantic analyzer contract test (Ayansh).
 *
 * Builds ASTs in code (no lexer/parser needed) and checks that the analyzer
 * accepts valid programs and flags exactly the right invalid ones.
 * Run: make semantic_test   (prints PASS/FAIL per check)
 */
#include "semantic.h"
#include <string.h>

static int g_checks = 0;
static int g_fails  = 0;

static void check(int cond, const char *what) {
    g_checks++;
    if (!cond) {
        g_fails++;
        printf("FAIL: %s\n", what);
    } else {
        printf("ok: %s\n", what);
    }
}

static int fv(UnitVec v, int i) { return v.dim[i]; }

int main(void) {
    SymTab *st;
    AstNode *prog;
    int errs;
    Symbol *sym;

    /* ------- 1. velocity: let speed = distance / time ------- */
    st = symtab_create();
    prog = ast_new_let(dim_strdup("speed"),
        ast_new_binary(OP_DIV,
            ast_new_binary(OP_MUL, ast_new_num(5, 1), ast_new_unit_lit(dim_strdup("meters"), 1), 1),
            ast_new_binary(OP_MUL, ast_new_num(10, 1), ast_new_unit_lit(dim_strdup("seconds"), 1), 1),
            1), 1);
    errs = semantic_check(st, prog);
    check(errs == 0, "velocity program accepted");
    sym = symtab_lookup(st, "speed");
    check(sym && sym->unit.dim[DIM_S] == -1, "speed unit = [m,s^-1]");

    /* distance/time must NOT add: distance + time */
    prog = ast_new_binary(OP_ADD,
        ast_new_binary(OP_MUL, ast_new_num(5, 1), ast_new_unit_lit(dim_strdup("meters"), 1), 1),
        ast_new_binary(OP_MUL, ast_new_num(10, 1), ast_new_unit_lit(dim_strdup("seconds"), 1), 1),
        1);
    errs = semantic_check(st, prog);
    check(errs == 1, "meters + seconds rejected (1 error)");
    ast_free(prog);

    /* ------- 2. subtraction wording ------- */
    prog = ast_new_binary(OP_SUB,
        ast_new_unit_lit(dim_strdup("seconds"), 1),
        ast_new_unit_lit(dim_strdup("meters"), 1), 1);
    errs = semantic_check(st, prog);
    check(errs == 1, "seconds - meters rejected");
    ast_free(prog);

    /* ------- 3. force = 2 * kg * meters / (seconds ^ 2) ------- */
    symtab_destroy(st);
    st = symtab_create();
    prog = ast_new_let(dim_strdup("force"),
        ast_new_binary(OP_DIV,
            ast_new_binary(OP_MUL,
                ast_new_binary(OP_MUL, ast_new_num(2, 1),
                    ast_new_unit_lit(dim_strdup("kg"), 1), 1),
                ast_new_unit_lit(dim_strdup("meters"), 1), 1),
            ast_new_binary(OP_POW, ast_new_unit_lit(dim_strdup("seconds"), 1),
                ast_new_num(2, 1), 1), 1), 1);
    errs = semantic_check(st, prog);
    check(errs == 0, "force program accepted");
    sym = symtab_lookup(st, "force");
    check(sym && fv(sym->unit, DIM_KG) == 1 && fv(sym->unit, DIM_S) == -2,
          "force unit = [1,1,-2,...]");
    ast_free(prog);

    /* ------- 4. cancellation: m / m is Scalar ------- */
    prog = ast_new_binary(OP_ADD,
        ast_new_binary(OP_DIV, ast_new_unit_lit(dim_strdup("meters"), 1),
            ast_new_unit_lit(dim_strdup("meters"), 1), 1),
        ast_new_num(7, 1), 1);
    errs = semantic_check(st, prog);
    check(errs == 0, "m/m == 7 accepted (cancellation is dimensionless)");
    ast_free(prog);

    /* ------- 5. use-before-declare ------- */
    symtab_destroy(st);
    st = symtab_create();
    prog = ast_new_let(dim_strdup("speed"),
        ast_new_var(dim_strdup("distance"), 1), 1);   /* distance not yet let */
    errs = semantic_check(st, prog);
    check(errs == 1, "use-before-declare rejected");
    ast_free(prog);

    /* ------- 6. redeclaration ------- */
    prog = ast_new_let(dim_strdup("a"), ast_new_num(5, 1), 1);
    prog = ast_stmt_push(prog,
        ast_new_let(dim_strdup("a"), ast_new_num(6, 2), 2));
    errs = semantic_check(st, prog);
    check(errs == 1, "redeclaration rejected");
    ast_free(prog);

    /* ------- 7. assignment to undeclared ------- */
    prog = ast_new_assign(dim_strdup("x"), ast_new_num(5, 1), 1);
    errs = semantic_check(st, prog);
    check(errs == 1, "assign to undeclared rejected");
    ast_free(prog);

    /* ------- 8. assigning new units to existing var updates (with warn) ------- */
    prog = ast_new_let(dim_strdup("d"), ast_new_unit_lit(dim_strdup("meters"), 1), 1);
    prog = ast_stmt_push(prog,
        ast_new_binary(OP_ADD, ast_new_var(dim_strdup("d"), 2),
            ast_new_num(3, 2), 2));
    errs = semantic_check(st, prog);
    check(errs == 1, "d (meters) + 3 rejected (meters != Scalar)");
    ast_free(prog);

    symtab_destroy(st);
    st = symtab_create();

    /* ------- 9. pow with fractional exponent ------- */
    prog = ast_new_binary(OP_POW, ast_new_unit_lit(dim_strdup("meters"), 1),
        ast_new_num(0, 1) /* placeholder replaced below */, 1);
    ast_free(prog); /* rebuild properly */
    prog = ast_new_binary(OP_POW, ast_new_unit_lit(dim_strdup("meters"), 1),
        /* fractional: */
        ast_new_num(1, 1), 1);
    /* rebuild rhs with 1.5 */
    ast_free(prog->rhs);
    prog->rhs = ast_new_num(1.5, 1);
    errs = semantic_check(st, prog);
    check(errs == 1, "fractional power rejected");
    ast_free(prog);

    /* ------- 10. energy: kg * meters^2 / seconds^2 ------- */
    prog = ast_new_let(dim_strdup("energy"),
        ast_new_binary(OP_DIV,
            ast_new_binary(OP_MUL, ast_new_unit_lit(dim_strdup("kg"), 1),
                ast_new_binary(OP_POW, ast_new_unit_lit(dim_strdup("meters"), 1),
                    ast_new_num(2, 1), 1), 1),
            ast_new_binary(OP_POW, ast_new_unit_lit(dim_strdup("seconds"), 1),
                ast_new_num(2, 1), 1), 1), 1);
    errs = semantic_check(st, prog);
    check(errs == 0, "energy program accepted");
    sym = symtab_lookup(st, "energy");
    check(sym && fv(sym->unit, DIM_M) == 2 && fv(sym->unit, DIM_KG) == 1
               && fv(sym->unit, DIM_S) == -2, "energy unit = kg*m^2/s^2");
    ast_free(prog);

    /* ------- 11. cast velo: distance / time with parens ------- */
    prog = ast_new_let(dim_strdup("speed"),
        ast_new_binary(OP_DIV,
            ast_new_binary(OP_ADD,
                ast_new_binary(OP_MUL, ast_new_num(3, 1),
                    ast_new_unit_lit(dim_strdup("meters"), 1), 1),
                ast_new_binary(OP_MUL, ast_new_num(2, 1),
                    ast_new_unit_lit(dim_strdup("meters"), 1), 1), 1),
            ast_new_unit_lit(dim_strdup("seconds"), 1), 1), 1);
    errs = semantic_check(st, prog);
    check(errs == 0, "(3*meters + 2*meters) / seconds accepted");
    ast_free(prog);

    symtab_destroy(st);

    printf("\n%d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
