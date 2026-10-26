/* ast.c — AST constructors, destructor, and debug printer (Balraj).
 * The parser builds nodes with these; Ayansh's analyzer fills `unit`,
 * Shreyansh's codegen walks the same tree.
 */
#include "ast.h"
#include <string.h>

/* ------------------------------------------------ helpers */

static AstNode *ast_new(AstKind kind, int line) {
    AstNode *n = (AstNode *)calloc(1, sizeof *n);
    if (!n) {
        DIM_ERR("out of memory building AST");
        exit(EXIT_BUILD_ERROR);
    }
    n->kind = kind;
    n->line = line;
    n->op   = OP_NONE;
    n->unit = uv_dimensionless();
    n->unit_known = 0;      /* analyzer flips this to 1 */
    n->prim = TYPE_UNKNOWN;
    return n;
}

/* ------------------------------------------------ constructors */

AstNode *ast_new_num(double value, int line) {
    AstNode *n = ast_new(AST_NUM, line);
    n->num = value;
    return n;
}

AstNode *ast_new_unit_lit(const char *unit_name, int line) {
    AstNode *n = ast_new(AST_UNIT_LIT, line);
    n->unit_name = (char *)unit_name;   /* takes ownership */
    return n;
}

AstNode *ast_new_var(const char *name, int line) {
    AstNode *n = ast_new(AST_VAR, line);
    n->name = (char *)name;             /* takes ownership */
    return n;
}

AstNode *ast_new_binary(BinOp op, AstNode *lhs, AstNode *rhs, int line) {
    AstNode *n = ast_new(AST_BINARY, line);
    n->op  = op;
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

AstNode *ast_new_let(const char *name, AstNode *rhs, int line) {
    AstNode *n = ast_new(AST_LET, line);
    n->name = (char *)name;             /* takes ownership */
    n->rhs  = rhs;      /* NOTE: rhs is the initializer expression */
    return n;
}

AstNode *ast_new_assign(const char *name, AstNode *rhs, int line) {
    AstNode *n = ast_new(AST_ASSIGN, line);
    n->name = (char *)name;             /* takes ownership */
    n->rhs  = rhs;
    return n;
}

AstNode *ast_stmt_push(AstNode *list, AstNode *stmt) {
    AstNode *cur;
    if (!list) return stmt;
    for (cur = list; cur->next; cur = cur->next)
        ;
    cur->next = stmt;
    return list;
}

/* ------------------------------------------------ destructor */

void ast_free(AstNode *n) {
    if (!n) return;
    free(n->name);
    free(n->unit_name);
    ast_free(n->lhs);
    ast_free(n->rhs);
    ast_free(n->next);
    free(n);
}

/* ------------------------------------------------ debug printer */

static const char *op_str(BinOp op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_POW: return "^";
        default:     return "?";
    }
}

static const char *kind_str(AstKind k) {
    switch (k) {
        case AST_LET:       return "LET";
        case AST_ASSIGN:    return "ASSIGN";
        case AST_EXPR_STMT: return "EXPR_STMT";
        case AST_NUM:       return "NUM";
        case AST_UNIT_LIT:  return "UNIT";
        case AST_VAR:       return "VAR";
        case AST_BINARY:    return "BINARY";
        case AST_PAREN:     return "PAREN";
        default:            return "?";
    }
}

static void print_indent(int depth) {
    int i;
    for (i = 0; i < depth; i++) printf("  ");
}

static void ast_print_rec(const AstNode *n, int depth) {
    if (!n) return;

    print_indent(depth);
    switch (n->kind) {
        case AST_LET:
            printf("LET line=%d  '%s'\n", n->line, n->name);
            break;
        case AST_ASSIGN:
            printf("ASSIGN line=%d  '%s'\n", n->line, n->name);
            break;
        case AST_NUM:
            printf("NUM line=%d  %g%s\n", n->line, n->num,
                   n->unit_known ? "" : "   ; unit: (unfilled)");
            break;
        case AST_UNIT_LIT:
            printf("UNIT line=%d  '%s'\n", n->line, n->unit_name);
            break;
        case AST_VAR:
            printf("VAR line=%d  '%s'\n", n->line, n->name);
            break;
        case AST_BINARY:
            printf("BINARY line=%d  op='%s'\n", n->line, op_str(n->op));
            break;
        default:
            printf("%s line=%d\n", kind_str(n->kind), n->line);
            break;
    }

    /* unit annotation, once the analyzer has folded it (Ayansh) */
    if (n->unit_known) {
        char buf[64];
        uv_str(n->unit, buf, sizeof buf);
        print_indent(depth);
        printf("  units: %s\n", buf);
    }

    ast_print_rec(n->lhs, depth + 1);
    ast_print_rec(n->rhs, depth + 1);
}

void ast_print(const AstNode *n, FILE *out) {
    fprintf(out, "── AST ──\n");
    while (n) {
        ast_print_rec(n, 0);
        if (n->next) printf("\n");
        n = n->next;
    }
}
