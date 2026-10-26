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

factor
    : NUM                     { $$ = ast_new_num($1, @1.first_line); }
    | UNIT                    { $$ = ast_new_unit_lit($1, @1.first_line); }
    | ID                      { $$ = ast_new_var($1, @1.first_line); }
    | factor '^' NUM          { AstNode *e = ast_new_num($3, @3.first_line);
                                $$ = ast_new_binary(OP_POW, $1, e, @1.first_line); }
    | '(' expr ')'            { $$ = $2; }
    ;

%%

void yyerror(const char *msg)
{
    extern int yylineno;               /* maintained by lexer.l */
    DIM_ERR("parse error (line %d): %s", yylineno, msg);
    g_parse_errors++;
}
