/* semantic.c — the unit-aware semantic analyzer (Ayansh, Phase 3 core).
 *
 * Traversal: one ordered pass over the statement list; for each statement
 * the rhs expression is folded bottom-up. Folding rules (exponent math):
 *
 *   NUM           -> dimensionless (plain constant, e.g. the 5 in 5 * meters)
 *   UNIT_LIT      -> unit vector of the named unit (uv_from_name)
 *   VAR           -> unit vector from the symbol table (declared by `let`)
 *   a * b         -> uv_mul   (exponents ADD)
 *   a / b         -> uv_div   (exponents SUBTRACT — m/m cancels to Scalar)
 *   a ^ n         -> uv_pow   (base may be dimensional, n must be a number)
 *   a + b, a - b  -> operands must be uv_equal, result keeps that unit
 *
 * Statement rules:
 *   `let x = expr;`          declares x with the folded unit (in source order,
 *                            so use-before-declare is caught).
 *   `x = expr;`              x must exist; its unit is updated.
 *   `expr;`                  expression statement: folded & validated only.
 *
 * Errors DO NOT stop the pass — every violation in a file is reported, so a
 * whole test file's mistakes show up in one run.
 */
#include "semantic.h"
#include <math.h>
#include <string.h>

/* diagnostics emitters from semantic_ops.c */
void sem_err_add_sub(const AstNode *n, const AstNode *lhs, const AstNode *rhs,
                     UnitVec lu, UnitVec ru);
void sem_err_var(const AstNode *n);
void sem_err_redecl(const AstNode *n, const Symbol *old);
void sem_err_assign_unit_change(const AstNode *n, const Symbol *old,
                                UnitVec new_u);
PrimType sem_node_type(const AstNode *n);

/* ------------------------------------------------ expression folding ---- */

static UnitVec fold_expr(AstNode *n, SymTab *st, int *errs) {
    UnitVec result = uv_dimensionless();
    Symbol *sym;

    if (!n) return result;

    switch (n->kind) {
        case AST_NUM:
            result = uv_dimensionless();
            break;

        case AST_UNIT_LIT:
            result = uv_from_name(n->unit_name);
            break;

        case AST_VAR:
            sym = symtab_lookup(st, n->name);
            if (!sym) {
                sem_err_var(n);
                (*errs)++;
                /* keep going with dimensionless so one bad variable doesn't
                 * cascade into a wall of follow-on errors */
            } else {
                result = sym->unit;
            }
            break;

        case AST_PAREN:
            result = fold_expr(n->lhs, st, errs);
            break;

        case AST_BINARY: {
            UnitVec lu = fold_expr(n->lhs, st, errs);
            UnitVec ru = fold_expr(n->rhs, st, errs);
            AstNode *base, *expo;

            switch (n->op) {
                case OP_ADD:
                case OP_SUB:
                    if (!uv_equal(lu, ru))
                        { sem_err_add_sub(n, n->lhs, n->rhs, lu, ru); (*errs)++; }
                    result = lu;          /* on error, pick one to continue */
                    break;

                case OP_MUL:
                    result = uv_mul(lu, ru);
                    break;

                case OP_DIV:
                    result = uv_div(lu, ru);   /* m/m -> Scalar, free */
                    break;

                case OP_POW: {
                    double ipart;
                    base = n->lhs;
                    expo = n->rhs;
                    /* exponent must be a plain number — NUM always is, but a
                     * parenthesized dimensional base like (x)^2.5 needs a
                     * fractional-exponent warning (we only support ints) */
                    if (expo->kind != AST_NUM) {
                        char ex[160];
                        semantic_expr_str(n, ex, sizeof ex);
                        DIM_ERR("line %d: exponent of '^' must be a plain number",
                                n->line);
                        DIM_NOTE("while checking '%s'", ex);
                        (*errs)++;
                    } else if (modf(expo->num, &ipart) != 0.0) {
                        char ex[160];
                        semantic_expr_str(n, ex, sizeof ex);
                        DIM_ERR("line %d: fractional exponent %g is not supported",
                                n->line, expo->num);
                        DIM_NOTE("while checking '%s'; use integer exponents "
                                 "(e.g. seconds ^ 2)", ex);
                        (*errs)++;
                    } else {
                        result = uv_pow(base->unit, (int)expo->num);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    n->unit       = result;
    n->unit_known = 1;
    n->prim       = sem_node_type(n);
    return result;
}

/* ------------------------------------------------ statement handling ---- */

/* Returns error contribution; folds the rhs first so `let` inserts the
 * CORRECT unit, not a placeholder. */
static int check_stmt(AstNode *stmt, SymTab *st) {
    int errs = 0;
    UnitVec u;
    Symbol *sym;

    if (!stmt) return 0;

    switch (stmt->kind) {
        case AST_LET:
            u = fold_expr(stmt->rhs, st, &errs);
            sym = symtab_lookup(st, stmt->name);
            if (sym) {
                sem_err_redecl(stmt, sym);
                errs++;
            } else if (!symtab_insert(st, stmt->name, TYPE_DOUBLE, u,
                                      stmt->line)) {
                errs++;
            } else {
                char desc[64], vec[64];
                uv_describe(u, desc, sizeof desc);
                uv_str(u, vec, sizeof vec);
                /* stderr on purpose: the final ./dimc must emit PURE C on
                 * stdout; the declared: lines are diagnostics/demo info
                 * (integration fix, Week 4) */
                fprintf(stderr, "declared: %-16s : %-20s %s\n",
                        stmt->name, desc, vec);
            }
            break;

        case AST_ASSIGN:
            u = fold_expr(stmt->rhs, st, &errs);
            sym = symtab_lookup(st, stmt->name);
            if (!sym) {
                DIM_ERR("line %d: assignment to undeclared identifier '%s'",
                        stmt->line, stmt->name ? stmt->name : "?");
                DIM_NOTE("declare it with 'let %s = ...' first",
                         stmt->name ? stmt->name : "?");
                errs++;
            } else {
                if (!uv_equal(sym->unit, u))
                    sem_err_assign_unit_change(stmt, sym, u);
                symtab_update_unit(st, stmt->name, u);
            }
            break;

        default: /* bare expression statement */
            fold_expr(stmt, st, &errs);
            break;
    }

    stmt->prim = TYPE_DOUBLE; /* statements themselves aren't typed */
    return errs;
}

/* ------------------------------------------------ entry point ---- */

int semantic_check(SymTab *st, AstNode *prog) {
    int total = 0;
    AstNode *stmt;

    /* declarations happen in source order inside check_stmt (AST_LET branch),
     * so `speed = distance / time;` before any `let distance ...` is caught */
    for (stmt = prog; stmt; stmt = stmt->next)
        total += check_stmt(stmt, st);

    return total;
}

void semantic_fold_expr(AstNode *node, SymTab *st, int *error_count) {
    (void)fold_expr(node, st, error_count);
}
