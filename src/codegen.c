/* codegen.c — Unit-stripped C code generator (Shreyansh)
 * Emits plain C with dimensions removed; zero runtime overhead by design.
 * Walks the AST post-analysis (units already checked by Ayansh).
 */
#include "ast.h"
#include "symtab.h"
#include "common.h"
#include "codegen.h"
#include <string.h>
#include <math.h>

static FILE *out = NULL;
static int need_mul = 0;

static void emit_expr_strip(const AstNode *n) {
    if (!n) return;

    switch (n->kind) {
        case AST_NUM: {
            double ipart;
            if (modf(n->num, &ipart) == 0.0 && n->num >= -1000000 && n->num <= 1000000) {
                fprintf(out, "%lld", (long long)n->num);
            } else {
                fprintf(out, "%g", n->num);
            }
            break;
        }

        case AST_UNIT_LIT:
            /* Strip all unit literals */
            break;

        case AST_VAR:
            if (n->name)
                fprintf(out, "%s", n->name);
            break;

        case AST_PAREN:
            fprintf(out, "(");
            emit_expr_strip(n->lhs);
            fprintf(out, ")");
            break;

        case AST_BINARY: {
            if (n->op == OP_POW) {
                if (n->rhs && n->rhs->kind == AST_NUM) {
                    int expv = (int)n->rhs->num;
                    if (expv == 1) {
                        emit_expr_strip(n->lhs);
                        break;
                    }
                    if (expv == 2) {
                        fprintf(out, "(");
                        emit_expr_strip(n->lhs);
                        fprintf(out, ")*(");
                        emit_expr_strip(n->lhs);
                        fprintf(out, ")");
                        break;
                    }
                    if (expv == -1) {
                        fprintf(out, "(1.0/");
                        emit_expr_strip(n->lhs);
                        fprintf(out, ")");
                        break;
                    }
                }
                fprintf(out, "pow(");
                emit_expr_strip(n->lhs);
                fprintf(out, ",");
                if (n->rhs && n->rhs->kind == AST_NUM)
                    fprintf(out, "%g", n->rhs->num);
                else
                    emit_expr_strip(n->rhs);
                fprintf(out, ")");
                break;
            }

            /* Handle multiplication/division specially to skip units cleanly */
            if (n->op == OP_MUL) {
                /* Collect non-unit operands */
                int has_left = 0, has_right = 0;
                /* Check if left is unit */
                if (n->lhs && n->lhs->kind == AST_UNIT_LIT) has_left = 0;
                else if (n->lhs) has_left = 1;
                if (n->rhs && n->rhs->kind == AST_UNIT_LIT) has_right = 0;
                else if (n->rhs) has_right = 1;
                
                /* If both are effectively units/numbers that become empty, skip */
                if (!has_left && !has_right) break;
                if (has_left && !has_right) { emit_expr_strip(n->lhs); break; }
                if (!has_left && has_right) { emit_expr_strip(n->rhs); break; }
                
                emit_expr_strip(n->lhs);
                fprintf(out, " * ");
                emit_expr_strip(n->rhs);
                break;
            }
            
            if (n->op == OP_DIV) {
                if (n->rhs && n->rhs->kind == AST_UNIT_LIT) { emit_expr_strip(n->lhs); break; }
                if (n->rhs && n->rhs->kind == AST_BINARY && n->rhs->op == OP_POW && n->rhs->lhs && n->rhs->lhs->kind == AST_UNIT_LIT) { emit_expr_strip(n->lhs); break; }
                if (n->lhs && n->lhs->kind == AST_UNIT_LIT) { fprintf(out, "1.0"); break; }
                emit_expr_strip(n->lhs);
                fprintf(out, " / ");
                emit_expr_strip(n->rhs);
                break;
            }

            /* For add/sub, skip if either side is just unit */
            if (n->op == OP_ADD || n->op == OP_SUB) {
                if (n->lhs && n->lhs->kind == AST_UNIT_LIT && n->rhs && n->rhs->kind == AST_UNIT_LIT) break;
                if (n->lhs && n->lhs->kind == AST_UNIT_LIT) { emit_expr_strip(n->rhs); break; }
                if (n->rhs && n->rhs->kind == AST_UNIT_LIT) { emit_expr_strip(n->lhs); break; }
            }

            emit_expr_strip(n->lhs);
            switch (n->op) {
                case OP_ADD: fprintf(out, " + "); break;
                case OP_SUB: fprintf(out, " - "); break;
                default: break;
            }
            emit_expr_strip(n->rhs);
            break;
        }

        default:
            break;
    }
}

static void emit_stmt(const AstNode *stmt) {
    if (!stmt) return;

    fprintf(out, "    ");
    switch (stmt->kind) {
        case AST_LET: {
            const char *type = "double";
            if (stmt->rhs) {
                int dimless = 1;
                int i;
                if (stmt->rhs->unit_known) {
                    for (i = 0; i < NUM_DIMENSIONS; i++) {
                        if (stmt->rhs->unit.dim[i] != 0) { dimless = 0; break; }
                    }
                    if (dimless && stmt->rhs->prim == TYPE_INT)
                        type = "int";
                }
            }
            fprintf(out, "%s %s = ", type, stmt->name ? stmt->name : "_tmp");
            emit_expr_strip(stmt->rhs);
            /* If we just emitted operators with nothing, clean up */
            fprintf(out, ";\n");
            break;
        }
        case AST_ASSIGN:
            fprintf(out, "%s = ", stmt->name ? stmt->name : "_tmp");
            emit_expr_strip(stmt->rhs);
            fprintf(out, ";\n");
            break;
        case AST_EXPR_STMT:
        default:
            emit_expr_strip(stmt);
            fprintf(out, ";\n");
            break;
    }
}

int codegen_emit(const AstNode *prog, const char *outname) {
    AstNode *stmt;
    FILE *old_out = out;

    out = fopen(outname, "w");
    if (!out) {
        DIM_ERR("codegen: cannot open output file '%s'", outname);
        out = old_out;
        return -1;
    }

    fprintf(out, "/* Generated by Dimensional Analysis Unit-Aware Semantic Checker */\n");
    fprintf(out, "/* Unit-stripped C - zero runtime overhead */\n");
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <math.h>\n\n");
    fprintf(out, "int main(void) {\n");

    for (stmt = prog; stmt; stmt = stmt->next)
        emit_stmt(stmt);

    fprintf(out, "\n    return 0;\n");
    fprintf(out, "}\n");

    fclose(out);
    out = old_out;
    printf("codegen: wrote %s\n", outname);
    return 0;
}
