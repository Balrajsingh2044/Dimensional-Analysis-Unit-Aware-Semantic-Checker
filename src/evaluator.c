/* evaluator.c — compile-time interpreter / evaluator pass (Week-5).
 *
 * Post-order traversal of the AST that actually RUNS the math:
 * every node computes a double value AND re-derives its UnitVec, so unit
 * cancellation happens dynamically in the same algebra the analyzer folds
 * (uv_mul adds exponents, uv_div subtracts — m / s is fine, m / m goes
 * dimensionless with no special case).
 *
 * Value semantics (agreed with codegen's constant-folding rule "unit
 * literals act as their SI factor"):
 *   NUM      -> its literal number, dimensionless
 *   UNIT_LIT -> its canonical SI factor (meters=1, km=1000, feet=0.3048)
 *   VAR      -> the value persisted in the symbol table when it was
 *               declared / assigned
 *
 * So `5 * feet` evaluates to 5 * 0.3048 = 1.524 carrying the Length vector
 * — everything downstream is canonical SI, which is exactly why the result
 * can be printed with a clean base-symbol unit name like (m/s^2).
 *
 * Errors during evaluation are counted (the pass keeps walking statements
 * so a file's errors are all reported, mirroring the analyzer's behavior),
 * but the driver maps any nonzero error count to EXIT_COMPILE_ERROR, so a
 * failing evaluation still fails the compile.
 */
#include "evaluator.h"
#include "semantic.h"    /* semantic_expr_str(), for diagnostic context */
#include "common.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------ evaluated value slot ---- */

/* The (value, unit) pair every expression either produces or fails on. */
typedef struct EvalValue {
    double  value;
    UnitVec unit;
} EvalValue;

static EvalValue eval_none(void) {
    EvalValue v;
    v.value = 0.0;
    v.unit  = uv_dimensionless();
    return v;
}

/* ------------------------------------------------ helper: value formatting */

/* "%.2f" for clean rounded numbers (4500.00, matching the demo spec) and
 * "%.6g" otherwise (0.555556, 1.5e-8) so precision is never lost. */
static void fmt_num(double v, char *buf, size_t n) {
    double ipart;
    if (fabs(v) < 1e13 && modf(v, &ipart) == 0.0)
        snprintf(buf, n, "%.2f", v);
    else
        snprintf(buf, n, "%.6g", v);
}

/* Base-symbol rendering of a unit vector: force prints (kg*m/s^2), speed
 * (m/s), Joules (kg*m^2/s^2). Mirrors uv_describe()'s numerator/denominator
 * split, but with SI symbols instead of dimension names. */
static void unit_symbol_str(UnitVec v, char *buf, size_t buflen) {
    static const char *sym[NUM_DIMENSIONS] = {"m", "kg", "s", "A", "K", "mol", "cd"};
    int i, printed = 0;
    if (!buf || buflen == 0) return;
    buf[0] = '\0';

    if (uv_is_dimensionless(v)) {
        snprintf(buf, buflen, "Scalar");
        return;
    }

    /* numerator: positive exponents */
    for (i = 0; i < NUM_DIMENSIONS; i++) {
        if (v.dim[i] > 0) {
            size_t off = strlen(buf);
            off += (size_t)snprintf(buf + off, buflen - off, printed ? "*" : "");
            if (v.dim[i] == 1)
                snprintf(buf + off, buflen - off, "%s", sym[i]);
            else
                snprintf(buf + off, buflen - off, "%s^%d", sym[i], v.dim[i]);
            printed = 1;
        }
    }
    /* denominator: negative exponents */
    for (i = 0; i < NUM_DIMENSIONS; i++) {
        if (v.dim[i] < 0) {
            size_t off = strlen(buf);
            off += (size_t)snprintf(buf + off, buflen - off, printed ? "/" : "1/");
            if (v.dim[i] == -1)
                snprintf(buf + off, buflen - off, "%s", sym[i]);
            else
                snprintf(buf + off, buflen - off, "%s^%d", sym[i], -v.dim[i]);
            printed = 1;
        }
    }
}

/* "Result: 4500.00 [1,1,-2,0,0,0,0] (kg*m/s^2)" — exact demo format. */
static void print_result(FILE *out, double v, UnitVec u) {
    char vbuf[40], vec[64], sym[96];
    if (!out) return;
    fmt_num(v, vbuf, sizeof vbuf);
    uv_str(u, vec, sizeof vec);
    unit_symbol_str(u, sym, sizeof sym);
    fprintf(out, "Result: %s %s (%s)\n", vbuf, vec, sym);
}

/* Fatal message + context note, in the same error style as the analyzer.
 * `expr` may be NULL (statement-level errors then print reason only). */
static void eval_fatal(int line, const AstNode *expr, const char *reason) {
    if (expr) {
        char ex[160];
        semantic_expr_str(expr, ex, sizeof ex);
        DIM_ERR("line %d: Cannot calculate: %s", line, reason);
        DIM_NOTE("while evaluating '%s'", ex);
    } else {
        DIM_ERR("line %d: Cannot calculate: %s", line, reason);
    }
}

/* ------------------------------------------------ expression evaluation ---- */

/* Post-order: children first, then fold value + unit with the operator's
 * algebra. Returns 0 on success and stores the (value, unit) into *out. */
static int eval_expr(AstNode *n, SymTab *st, EvalValue *out) {
    double    value = 0.0;
    UnitVec   unit  = uv_dimensionless();
    Symbol   *sym;

    if (!n)
        { DIM_ERR("Cannot calculate: empty expression"); return -1; }

    switch (n->kind) {
        case AST_NUM:
            value = n->num;
            unit  = uv_dimensionless();
            break;

        case AST_UNIT_LIT:
            unit = uv_from_name(n->unit_name);
            /* a bare unit literal acts as its SI factor (meters=1, km=1000),
             * matching codegen's folding of `5 * kilometers / meters` = 5000 */
            value = uv_scale(unit);
            break;

        case AST_VAR:
            sym = symtab_lookup(st, n->name);
            if (!sym)
                { eval_fatal(n->line, n, "undeclared identifier"); return -1; }
            value = sym->value;
            unit  = sym->unit;
            if (!(unit.scale > 0.0)) unit.scale = sym->scale; /* belt+braces */
            break;

        case AST_PAREN:
            return eval_expr(n->lhs, st, out);

        case AST_BINARY: {
            EvalValue l = eval_none(), r = eval_none();

            if (eval_expr(n->lhs, st, &l) != 0) return -1;
            if (eval_expr(n->rhs, st, &r) != 0) return -1;

            switch (n->op) {
                case OP_ADD:
                case OP_SUB:
                    /* the checker already guarantees these, but re-assert
                     * them here so the evaluator is independently correct */
                    if (!uv_equal(l.unit, r.unit))
                        { eval_fatal(n->line, n, "dimension mismatch"); return -1; }
                    if (!scale_equal(l.unit, r.unit))
                        { eval_fatal(n->line, n, "scale mismatch across metric/imperial units"); return -1; }
                    value = (n->op == OP_ADD) ? (l.value + r.value)
                                              : (l.value - r.value);
                    unit  = l.unit;
                    break;

                case OP_MUL:
                    value = l.value * r.value;
                    unit  = uv_mul(l.unit, r.unit);     /* exponents ADD,
                                                           scales multiply */
                    break;

                case OP_DIV:
                    if (r.value == 0.0) {
                        char ex[160];
                        semantic_expr_str(n, ex, sizeof ex);
                        DIM_ERR("line %d: division by zero", n->line);
                        DIM_NOTE("while evaluating '%s'; this would trap at "
                                 "runtime", ex);
                        return -1;
                    }
                    value = l.value / r.value;
                    unit  = uv_div(l.unit, r.unit);     /* m/m cancels, free */
                    break;

                case OP_POW: {
                    /* semantic pass guarantees: rhs is NUM, integral exponent */
                    double e = n->rhs->num;
                    double ipart;
                    if (n->rhs->kind != AST_NUM || modf(e, &ipart) != 0.0)
                        { eval_fatal(n->line, n, "fractional exponent"); return -1; }
                    if (l.value == 0.0 && e < 0.0)
                        { eval_fatal(n->line, n, "zero base with negative exponent"); return -1; }
                    value = pow(l.value, e);
                    unit  = uv_pow(l.unit, (int)e);
                    break;
                }
                default:
                    eval_fatal(n->line, n, "unknown operator"); return -1;
            }
            break;
        }

        default:
            eval_fatal(n->line, NULL, "cannot evaluate this node kind"); return -1;
    }

    /* Persist what we computed on the node itself (post-order storage) and
     * hand it to the caller. */
    n->value      = value;
    n->unit       = unit;
    n->unit_known = 1;
    n->scale      = unit.scale;
    if (out) { out->value = value; out->unit = unit; }
    return 0;
}

/* ------------------------------------------------ statements ---- */

/* Mirrors semantic.c's statement rules; returns an error contribution.
 *   `let x = rhs;`    evaluate rhs, re-point x's value (the analyzer
 *                      already created the symbol with the right unit)
 *   `x = rhs;`        evaluate, re-point value AND unit/scale
 *   `rhs;`            bare expression: PRINT its Result line right away —
 *                      expression statements double as a REPL-style print
 */
static int eval_stmt(AstNode *stmt, SymTab *st, FILE *out,
                     EvalValue *last, int *last_ok) {
    EvalValue res = eval_none();
    Symbol *sym;

    if (!stmt) return 0;

    switch (stmt->kind) {
        case AST_LET:
            if (eval_expr(stmt->rhs, st, &res) != 0) return 1;
            sym = symtab_lookup(st, stmt->name);
            if (!sym)
                { eval_fatal(stmt->line, stmt->rhs, "let target lost from symbol table"); return 1; }
            symtab_update_value(st, stmt->name, res.value);
            symtab_update_unit_s(st, stmt->name, res.unit, res.unit.scale);
            *last = res;
            *last_ok = 1;
            break;

        case AST_ASSIGN:
            if (eval_expr(stmt->rhs, st, &res) != 0) return 1;
            sym = symtab_lookup(st, stmt->name);
            if (!sym)
                { eval_fatal(stmt->line, stmt->rhs, "assignment to undeclared identifier"); return 1; }
            /* NOTE: the analyzer explicitly ALLOWS assignment that changes
             * the unit (it only emits a WARNING, e.g. distance = time), so
             * the evaluator must not treat that as fatal — re-point value
             * AND unit/scale exactly like check_stmt does. */
            symtab_update_value(st, stmt->name, res.value);
            symtab_update_unit_s(st, stmt->name, res.unit, res.unit.scale);
            *last = res;
            *last_ok = 1;
            break;

        default: /* bare expression statement */
            if (eval_expr(stmt, st, &res) != 0) return 1;
            print_result(out, res.value, res.unit);
            *last = res;
            *last_ok = 1;
            break;
    }
    return 0;
}

/* ------------------------------------------------ entry points ---- */

int eval_run(const AstNode *prog, SymTab *st, FILE *out) {
    int errors = 0;
    AstNode *stmt;
    EvalValue last = eval_none();
    int last_ok = 0;
    const AstNode *last_stmt = NULL;

    /* Interpreter order: statements run in source order, so later
     * expressions see values persisted by earlier ones. */
    for (stmt = (AstNode *)prog; stmt; stmt = stmt->next) {
        errors += eval_stmt(stmt, st, out, &last, &last_ok);
        last_stmt = stmt;
    }

    /* Requirement 4: after the root evaluation, print the final result.
     * Only when the last statement evaluated cleanly — a failed expression
     * must not fabricate a Result line from a zero placeholder. Bare
     * expression statements already printed in the loop; nothing prints
     * twice. */
    if (last_ok && out &&
        (last_stmt->kind == AST_LET || last_stmt->kind == AST_ASSIGN))
        print_result(out, last.value, last.unit);

    return errors;
}

int eval_expression(const AstNode *node, SymTab *st, double *out_value,
                    UnitVec *out_unit) {
    EvalValue res = eval_none();
    if (node && node->kind == AST_PAREN)
        node = node->lhs;
    if (eval_expr((AstNode *)node, st, &res) != 0) return -1;
    if (out_value) *out_value = res.value;
    if (out_unit)  *out_unit  = res.unit;
    return 0;
}
