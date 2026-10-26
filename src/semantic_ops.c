/* semantic_ops.c — diagnostics helpers for the semantic analyzer (Ayansh).
 *
 * Pure formatting / error-emission logic, split from the traversal in
 * semantic.c so the tricky parts of the checker stay readable:
 *   - re-printing an expression as text ("while checking 'distance + time'")
 *   - the exact error wording per operator (agreed format in common.h)
 *   - primitive-type promotion used by codegen later (Shreyansh reads this)
 */
#include "semantic.h"
#include <math.h>
#include <ctype.h>

/* ------------------------------------------------ expression -> text ---- */

static void expr_str_rec(const AstNode *e, char *buf, size_t buflen) {
    if (!e) { snprintf(buf, buflen, "?"); return; }

    switch (e->kind) {
        case AST_NUM: {
            double ipart;
            if (modf(e->num, &ipart) == 0.0)
                snprintf(buf, buflen, "%d", (int)e->num);
            else
                snprintf(buf, buflen, "%g", e->num);
            break;
        }
        case AST_UNIT_LIT:
            snprintf(buf, buflen, "%s", e->unit_name ? e->unit_name : "?unit");
            break;
        case AST_VAR:
            snprintf(buf, buflen, "%s", e->name ? e->name : "?var");
            break;
        case AST_BINARY: {
            char lbuf[128], rbuf[128];
            expr_str_rec(e->lhs, lbuf, sizeof lbuf);
            expr_str_rec(e->rhs, rbuf, sizeof rbuf);
            /* NOTE: OP_NONE==0 occupies slot 0, so add/sub are 1..5 —
             * subtract 1 when indexing the operator table. */
            if (e->op >= OP_ADD && e->op <= OP_POW)
                snprintf(buf, buflen, "(%s %c %s)", lbuf,
                         "+-*/^"[e->op - 1], rbuf);
            else
                snprintf(buf, buflen, "(%s ? %s)", lbuf, rbuf);
            break;
        }
        case AST_PAREN:
            expr_str_rec(e->lhs, buf, buflen);
            break;
        default:
            snprintf(buf, buflen, "<expr>");
            break;
    }
}

void semantic_expr_str(const AstNode *e, char *buf, size_t buflen) {
    if (!buf || buflen == 0) return;
    expr_str_rec(e, buf, buflen);
}

/* ------------------------------------------------ error emitters ---- */

void sem_err_add_sub(const AstNode *n, const AstNode *lhs, const AstNode *rhs,
                     UnitVec lu, UnitVec ru) {
    (void)lhs; (void)rhs;   /* vectors lu/ru carry all needed info */
    char ld[64], rd[64], lv[64], rv[64], ex[160];
    uv_describe(lu, ld, sizeof ld);
    uv_describe(ru, rd, sizeof rd);
    uv_str(lu, lv, sizeof lv);
    uv_str(ru, rv, sizeof rv);
    semantic_expr_str(n, ex, sizeof ex);

    if (n->op == OP_SUB)
        DIM_ERR("Cannot subtract %s %s from %s %s", rd, rv, ld, lv);
    else
        DIM_ERR("Cannot add %s %s to %s %s", ld, lv, rd, rv);
    DIM_NOTE("line %d: while checking '%s'", n->line, ex);
}

/* Week-3 (metric vs imperial): same dim vector, different canonical size. */
void sem_err_add_sub_scale(const AstNode *n, UnitVec lu, UnitVec ru) {
    char ld[64], lv[64], ex[160];
    uv_describe(lu, ld, sizeof ld);
    uv_str(lu, lv, sizeof lv);
    semantic_expr_str(n, ex, sizeof ex);

    DIM_ERR("Cannot add %s %s across scales: %g (SI) vs %g (SI)",
            ld, lv, uv_scale(lu), uv_scale(ru));
    DIM_NOTE("line %d: while checking '%s'; same dimension but different "
             "unit size (metric vs imperial?) — convert explicitly first",
             n->line, ex);
}

void sem_err_var(const AstNode *n) {
    char ex[160];
    semantic_expr_str(n, ex, sizeof ex);
    DIM_ERR("line %d: undeclared identifier '%s'", n->line,
            n->name ? n->name : "?");
    DIM_NOTE("while checking '%s'; 'let' declares a name before first use",
             ex);
}

void sem_err_redecl(const AstNode *n, const Symbol *old) {
    char ex[160];
    semantic_expr_str(n->rhs, ex, sizeof ex);
    DIM_ERR("line %d: redeclaration of '%s' (first declared line %d)",
            n->line, n->name ? n->name : "?", old->line);
    DIM_NOTE("while checking 'let %s = %s'", n->name ? n->name : "?", ex);
}

void sem_err_assign_unit_change(const AstNode *n, const Symbol *old,
                                UnitVec new_u) {
    char od[64], nd[64], ex[160];
    uv_describe(old->unit, od, sizeof od);
    uv_describe(new_u,  nd, sizeof nd);
    semantic_expr_str(n->rhs, ex, sizeof ex);
    DIM_WARN("line %d: '%s' changes dimension from %s to %s",
             n->line, n->name ? n->name : "?", od, nd);
    DIM_NOTE("while checking '%s = %s'", n->name ? n->name : "?", ex);
}

void sem_err_assign_scale_change(const AstNode *n, const Symbol *old,
                                 UnitVec new_u) {
    char dd[64], ex[160];
    uv_describe(old->unit, dd, sizeof dd);
    semantic_expr_str(n->rhs, ex, sizeof ex);
    DIM_WARN("line %d: '%s' keeps dimension %s but changes scale from "
             "%g (SI) to %g (SI)",
             n->line, n->name ? n->name : "?", dd,
             uv_scale(old->unit), uv_scale(new_u));
    DIM_NOTE("while checking '%s = %s'", n->name ? n->name : "?", ex);
}

/* ------------------------------------------------ type promotion ----
 * Shreyansh's codegen picks a C type per node. Rule agreed here:
 *   anything carrying units, or any fractional number, is double;
 *   a dimensionless integer literal (5 in `distance + 5`) is int.
 */
PrimType sem_node_type(const AstNode *n) {
    double ipart;
    if (!n) return TYPE_UNKNOWN;
    switch (n->kind) {
        case AST_NUM:
            return (modf(n->num, &ipart) == 0.0) ? TYPE_INT : TYPE_DOUBLE;
        default:
            return TYPE_DOUBLE;
    }
}
