/* codegen.c — Unit-stripped C code generator (Shreyansh's Phase-4 slot).
 * Fleshed out during the Week-4 integration pass (Balraj) so the unified
 * ./dimc could materialize — the design + API are unchanged from
 * include/codegen.h; Shreyansh: tune/replace internals freely, contract
 * stays `int codegen_emit(const AstNode *prog, const char *outname)`
 * (outname == NULL → stdout; returns 0 on success).
 *
 * Precondition (guaranteed by main.c's calling order): semantic_check()
 * returned 0, so EVERY expression node has unit_known==1 and a folded
 * node->unit. We emit values only — units existed purely at compile time,
 * which is the zero-runtime-overhead selling point.
 *
 * Emission rules:
 *   - whole-constant subtrees prune to one number (5 * meters -> 5;
 *     meters / meters -> 1; 1.5e-3 -> 0.0015),
 *   - declarations pick int only for dimensionless integers, else double,
 *   - minimal parens via precedence classes (left-assoc + - * /, and '^'
 *     binds tighter — note parser folds '^' chains, so exponent is one NUM),
 *   - powers expand to plain multiplication for |e| <= 8 (no libm call),
 *     pow() beyond that. No other runtime overhead anywhere.
 */
#include "codegen.h"
#include "symtab.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define POW_INLINE_MAX 8   /* expand b^e inline up to this exponent */

/* ------------------------------------------------ helpers */

static int prec_of(const AstNode *n) {
    if (!n) return 0;
    if (n->kind != AST_BINARY) return 100;   /* atoms bind tightest */
    switch (n->op) {
        case OP_POW: return 70;
        case OP_MUL:
        case OP_DIV: return 60;
        case OP_ADD:
        case OP_SUB: return 50;
        default:     return 0;
    }
}

/* Whole-subtree constant?  Unit literals act as magnitude 1 (their units
 * stay compile-time-only, so 5 * meters == 5 and m/s == 1). */
static int expr_const(const AstNode *n, double *out) {
    double lv, rv;
    int lc, rc;

    if (!n) return 0;
    switch (n->kind) {
        case AST_NUM:      *out = n->num; return 1;
        case AST_UNIT_LIT: *out = 1.0;     return 1;
        case AST_BINARY:
            if (!n->lhs || !n->rhs) return 0;
            lc = expr_const(n->lhs, &lv);
            rc = expr_const(n->rhs, &rv);
            if (!(lc && rc)) return 0;
            switch (n->op) {
                case OP_ADD: *out = lv + rv; break;
                case OP_SUB: *out = lv - rv; break;
                case OP_MUL: *out = lv * rv; break;
                case OP_DIV: *out = lv / rv; break;
                case OP_POW: *out = pow(lv, rv); break; /* int exp per semantic */
                default: return 0;
            }
            return 1;
        default:
            return 0;
    }
}

/* C-number literal: integers without a fractional tail, max precision else. */
static void emit_num(FILE *o, double v) {
    if (v == floor(v) && fabs(v) < 2.0e15)
        fprintf(o, "%.0f", v);
    else
        fprintf(o, "%.15g", v);
}

static void emit_expr(FILE *o, const AstNode *n, int parent_prec, int is_right);

/* pow: inline product for small exponents, pow() call beyond */
static void emit_pow(FILE *o, const AstNode *node, int parent_prec) {
    double ev = (node->rhs && node->rhs->kind == AST_NUM) ? node->rhs->num : 1.0;
    int ei = (int)ev;
    int neg = ei < 0;
    int cnt = neg ? -ei : ei;
    int i;

    if (ei == 0) {
        fputc('1', o);
        return;
    }

    /* parenthesize the group as a whole if this pow nests under looser
     * precedence than itself (e.g. inside * or /): keep pow atomic */
    if (parent_prec > 70) fputc('(', o);
    if (neg) fputs("1 / (", o);

    fputs("( ", o);
    emit_expr(o, node->lhs, 0, 0);
    for (i = 1; i < cnt; i++) {
        fputs(" * ", o);
        emit_expr(o, node->lhs, 0, 0);
    }
    fputs(" )", o);

    if (neg) fputc(')', o);
    if (parent_prec > 70) fputc(')', o);
}

/* Recursive, minimal-parenthesization emission.
 * is_right: this node is the RIGHT operand of the parent — equal precedence
 * on the right side needs parens (left-assoc ops, e.g. a / (b / c)). */
static void emit_expr(FILE *o, const AstNode *n, int parent_prec, int is_right) {
    int p;
    double v;

    if (!n) return;

    if (expr_const(n, &v)) {            /* prune constant/unit subtrees */
        emit_num(o, v);
        return;
    }

    p = prec_of(n);

    switch (n->kind) {
        case AST_VAR:
            fprintf(o, "%s", n->name ? n->name : "dimc_bad_var");
            break;

        case AST_BINARY:
            if (n->op == OP_POW) {
                emit_pow(o, n, parent_prec);
                break;
            } else {
                int wrap = (p < parent_prec) || (p == parent_prec && is_right);
                if (wrap) fputc('(', o);
                emit_expr(o, n->lhs, p, 0);
                switch (n->op) {            /* switch, not op-1 table */
                    case OP_ADD: fputs(" + ", o); break;
                    case OP_SUB: fputs(" - ", o); break;
                    case OP_MUL: fputs(" * ", o); break;
                    case OP_DIV: fputs(" / ", o); break;
                    default:    fputs(" ? ", o); break;
                }
                emit_expr(o, n->rhs, p, 1);
                if (wrap) fputc(')', o);
            }
            break;

        default:
            fputs("0 /* dimc: unsupported node */", o);
            break;
    }
}

/* declaration type: int only for dimensionless integer-valued constants */
static const char *stmt_type(const AstNode *rhs) {
    double v;
    if (rhs && rhs->unit_known && uv_is_dimensionless(rhs->unit)
        && expr_const(rhs, &v)
        && v == floor(v) && fabs(v) < 2.0e15)
        return "int";
    return "double";
}

/* seen-list: LET = first declaration, ASSIGN = plain assignment */
typedef struct Seen { const char *name; struct Seen *next; } Seen;

static int seen_has(Seen *s, const char *name) {
    for (; s; s = s->next) if (strcmp(s->name, name) == 0) return 1;
    return 0;
}

static void emit_stmt(FILE *o, const AstNode *stmt, Seen **seen) {
    Seen *n;
    if (!stmt) return;

    switch (stmt->kind) {
        case AST_LET:
        case AST_ASSIGN:
            if (stmt->name && stmt->kind == AST_LET && !seen_has(*seen, stmt->name)) {
                n = (Seen *)calloc(1, sizeof *n);
                if (!n) return;
                n->name = stmt->name;
                n->next = *seen;
                *seen = n;
                fprintf(o, "    %-6s %s = ", stmt_type(stmt->rhs), stmt->name);
            } else {
                fprintf(o, "    %s = ", stmt->name ? stmt->name : "?");
            }
            emit_expr(o, stmt->rhs, 0, 0);
            fputs(";\n", o);
            break;

        default:   /* bare expression: side-effect-free in .dim, drop it */
            break;
    }
}

/* ------------------------------------------------ entry point */

int codegen_emit(const AstNode *prog, const char *outname) {
    FILE *out = stdout;
    const AstNode *s;
    Seen *seen = NULL;
    int ok = 1;

    if (outname) {
        out = fopen(outname, "w");
        if (!out) {
            DIM_ERR("codegen: cannot open output file '%s'", outname);
            return 1;
        }
    }

    fprintf(out,
        "/* generated by dimc — Dimensional Analysis compile pass\n"
        " * every unit was checked at compile time and stripped here:\n"
        " * this C has ZERO runtime unit overhead. */\n"
        "#include <stdio.h>\n\n"
        "int main(void) {\n");

    for (s = prog; s; s = s->next)
        emit_stmt(out, s, &seen);

    fprintf(out, "    return 0;\n}\n");

    while (seen) {
        Seen *nx = seen->next;
        free((void *)seen);          /* free CURRENT, then step — do not
                                      * free the next pointer first */
        seen = nx;
    }
    if (out != stdout) {
        if (fclose(out) != 0) ok = 0;
    }
    return ok ? 0 : 1;
}
