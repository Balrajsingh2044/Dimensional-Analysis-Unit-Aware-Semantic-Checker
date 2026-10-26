/* parser.y — Bison grammar for the .dim physics language (Balraj).
 *
 *   let distance = 5 * meters;
 *   speed = distance / 10 * seconds;
 *   let force = 2 * kg * meters / (seconds ^ 2);
 *
 * Precedence is encoded structurally (expr > term > factor), so there are no
 * precedence declarations and (hopefully) no conflicts. Unit info is NOT
 * folded here — every expression node is built op/children-first; Ayansh's
 * analyzer sets `unit`/`unit_known` bottom-up afterwards.
 *
 * Built with classic yacc style for compatibility with bison 2.4.1.
 */
%{
#include <stdlib.h>
#include "ast.h"
#include "common.h"

/* Parse result handed to the driver (frontend_main.c). */
AstNode *g_program     = NULL;
int      g_parse_errors = 0;

int  yylex(void);
void yyerror(const char *msg);
%}

%locations

%union {
    double   num;
    char    *str;
    AstNode *node;
}

%token <str> ID
%token <str> UNIT
%token <num> NUM
%token LET

/* Free strdup'd token strings when they are DISCARDED (error paths) —
 * keeps invalid-test runs closer to Valgrind-clean. NOTE: no <node>
 * destructor — bison 2.4.1 applies it to consumed values and corrupts
 * the heap (add it back once bison is upgraded). */
%destructor { free($$); } ID UNIT

%type <node> program stmt_list stmt expr term factor

/* Exactly 2 shift/reduce conflicts are CORRECT and REQUIRED by the unary
 * minus rules: after '- factor' / '+ factor', a following '^' must SHIFT so
 * the power binds the inner factor (-x^2 == -(x^2), C-style). If you add
 * rules and see these counts change, STOP and check parser.output. */
%expect 2

%start program

%%

program
    : stmt_list               { g_program = $1; $$ = $1; }   /* $$ set: keep bison quiet */
    ;

stmt_list
    : stmt                    { $$ = $1; }
    | stmt_list stmt          { $$ = ast_stmt_push($1, $2); }
    ;

stmt
    : LET ID '=' expr ';'     { $$ = ast_new_let($2, $4, @1.first_line); }
    | ID '=' expr ';'         { $$ = ast_new_assign($1, $3, @1.first_line); }
    | expr ';'                { $$ = $1; }
    ;

expr
    : expr '+' term           { $$ = ast_new_binary(OP_ADD, $1, $3, @1.first_line); }
    | expr '-' term           { $$ = ast_new_binary(OP_SUB, $1, $3, @1.first_line); }
    | term                    { $$ = $1; }
    ;

term
    : term '*' factor         { $$ = ast_new_binary(OP_MUL, $1, $3, @1.first_line); }
    | term '/' factor         { $$ = ast_new_binary(OP_DIV, $1, $3, @1.first_line); }
    | factor                  { $$ = $1; }
    ;

/* '^' chains: grammatically left-nested, FOLDED here into ONE POW node:
 *    meters ^ 3 ^ 2  ->  POW(meters, 6)      because (b^a)^c == b^(a*c)
 * This preserves the unit value exactly (exponents just multiply) and gives
 * Shreyansh's codegen a single node per power chain. Analyzer stays simple:
 * it only ever sees POW with a plain-number rhs. */
factor
    : NUM                     { $$ = ast_new_num($1, @1.first_line); }
    | UNIT                    { $$ = ast_new_unit_lit($1, @1.first_line); }
    | ID                      { $$ = ast_new_var($1, @1.first_line); }
    | '-' factor              { /* unary minus, desugared — no AST shape the
                                 analyzer doesn't already grok: (-x) == -1 * x */
                                $$ = ast_new_binary(OP_MUL,
                                  ast_new_num(-1.0, @1.first_line), $2,
                                  @1.first_line); }
    | '+' factor              { $$ = $2; }
    | '(' expr ')'            { $$ = $2; }
    | factor '^' NUM          {
        AstNode *base = $1;
        AstNode *prev = NULL;
        double exp = $3;
        int line = @1.first_line;

        if ($1->kind == AST_BINARY && $1->op == OP_POW
            && $1->rhs && $1->rhs->kind == AST_NUM) {
            prev = $1->rhs;
            base = $1->lhs;
            $1->lhs = NULL;          /* disconnect children… */
            $1->rhs = NULL;
            ast_free($1);            /* …so freeing the wrapper is safe */
            exp = prev->num * $3;
        }
        $$ = ast_new_binary(OP_POW, base, ast_new_num(exp, line), line);
        if (prev) ast_free(prev);
      }
    ;

%%

void yyerror(const char *msg)
{
    extern int yylineno;               /* maintained by lexer.l */
    DIM_ERR("parse error (line %d): %s", yylineno, msg);
    g_parse_errors++;
}
