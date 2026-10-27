/* evaluator.h — compile-time interpreter entry point (Week-5 eval pass).
 *
 * Team-contract situation: the other passes (semantic checking, codegen)
 * prove units and strip them; this pass instead COMPUTES the program at
 * compile time and prints the final numerical result together with its
 * derived 7-D unit vector:
 *
 *     Result: 4500.00 [1,1,-2,0,0,0,0] (kg*m/s^2)
 *
 * It re-folds the unit vectors itself, bottom-up (does NOT depend on the
 * analyzer's folded node->unit), because its job is to be an independent
 * execution of the same algebra the checker validated.
 *
 * Run ordering (the driver calls it like this):
 *   1. yyparse()             -> exit 3 on parse error
 *   2. semantic_check()      -> exit 1 on any unit violation
 *   3. eval_run(st, prog)    -> exit 1 on evaluation error (e.g. a
 *                               division that would be zero at runtime)
 *
 * What it prints on stdout: one "Result:" line per bare expression
 * statement (expression statements double as a REPL-style print), and a
 * final "Result:" line for the last let/assign. stdout stays free of
 * diagnostics; everything the evaluator complains about goes to stderr.
 */
#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <stdio.h>
#include "ast.h"
#include "symtab.h"

/* Interpret every statement of `prog` in order, computing values bottom-up
 * and persisting each declared/assigned variable's numeric value into the
 * symbol table. `st` is the SAME table the semantic pass just used
 * (symbols already declared; the evaluator fills in values).
 *
 * Returns the number of evaluation errors (0 == clean). out receives the
 * "Result:" lines (pass stdout; use NULL to stay silent). */
int eval_run(const AstNode *prog, SymTab *st, FILE *out);

/* Same engine over a single expression — exposed for the test suite and
 * for error-free fragment evaluation. Returns 0 on success, -1 on any
 * evaluation error (reported to stderr). */
int eval_expression(const AstNode *node, SymTab *st, double *out_value,
                    UnitVec *out_unit);

#endif /* EVALUATOR_H */
