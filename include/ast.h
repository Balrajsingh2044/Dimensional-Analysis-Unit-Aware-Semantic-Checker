/* ast.h — AST node contract (Balraj owns the parser that builds these;
 * Ayansh's analyzer and Shreyansh's codegen walk the same nodes).
 * ⭐ THIS FILE IS THE TEAM CONTRACT: DO NOT EDIT WITHOUT DISCUSSION.
 *
 * Design: every expression node carries an (optional) slot for the folded
 * UnitVec. The analyzer fills it bottom-up; -1 exponents flag "not yet known".
 */
#ifndef AST_H
#define AST_H

#include "symtab.h"   /* brings common.h, unitvec.h, and PrimType */

/* ---------------- node kinds ---------------- */
typedef enum {
    /* statements */
    AST_LET,        /* let distance = 5 * meters;     */
    AST_ASSIGN,     /* distance = 5 * meters;         */
    AST_EXPR_STMT,  /* bare expression statement      */

    /* expressions */
    AST_NUM,        /* 5 (unit taken from a sibling * meters node or scalar) */
    AST_UNIT_LIT,   /* meters  — bare unit literal, UnitVec filled by lexer */
    AST_VAR,        /* distance  — resolves via symbol table               */
    AST_BINARY,     /* op(lhs, rhs): + - * / ^                            */
    AST_PAREN       /* wrapper kept for debugging prints                  */
} AstKind;

typedef enum {
    OP_NONE = 0,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW
} BinOp;

/* ---------------- the node ---------------- */
typedef struct AstNode {
    AstKind        kind;
    int            line;        /* source line, for diagnostics */
    BinOp          op;          /* AST_BINARY only            */

    char          *name;        /* AST_VAR / AST_LET / AST_ASSIGN: identifier */
    char          *unit_name;   /* AST_UNIT_LIT: "meters", "kg", ...          */
    double         num;        /* AST_NUM                                     */
    PrimType       prim;        /* promoted primitive type, filled by analyzer */

    UnitVec        unit;        /* folded unit vector; valid only while        */
    int            unit_known;  /* unit_known == 1                             */
    double         scale;       /* canonical SI factor carried by the folded
                                 * unit (Week-3 metric-vs-imperial); default
                                 * 1.0; analyzer keeps it in sync with
                                 * node->unit.scale                            */

    struct AstNode *lhs, *rhs;  /* children (NULL when not applicable)         */
    struct AstNode *next;       /* statement list link                         */
} AstNode;

/* ---- constructors (Balraj calls these from parser.y) --------
 * Ownership: name / unit_name pointers ARE TAKEN OVER by the node (they come
 * strdup'd from the lexer) and are freed by ast_free(). Callers must not
 * free them afterwards. */
AstNode *ast_new_num(double value, int line);
AstNode *ast_new_unit_lit(const char *unit_name, int line);
AstNode *ast_new_var(const char *name, int line);
AstNode *ast_new_binary(BinOp op, AstNode *lhs, AstNode *rhs, int line);
AstNode *ast_new_let(const char *name, AstNode *rhs, int line);
AstNode *ast_new_assign(const char *name, AstNode *rhs, int line);

/* Shallow chain helpers for statement blocks. */
AstNode *ast_stmt_push(AstNode *list, AstNode *stmt);

/* Deep free (recurses into children). */
void ast_free(AstNode *n);

/* Debug printer (used pre-analysis; analyzer output comes later). */
void ast_print(const AstNode *n, FILE *out);

#endif /* AST_H */
