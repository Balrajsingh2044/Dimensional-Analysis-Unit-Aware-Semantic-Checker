/* semantic.h — semantic analyzer entry point (Ayansh owns this).
 * IMPORTANT: NEW team-contract file: adding it doesn't change anyone else's API —
 *    other members only need to care about ONE function, semantic_check().
 *
 * Reads the AST the parser produced, folds UnitVecs bottom-up through every
 * expression node, and enforces the dimensional rules:
 *
 *   + / -        operand vectors must be IDENTICAL (meters + seconds => error)
 *                AND carry the same canonical scale (meters + feet => error:
 *                both are Length, but 1 m != 0.3048 m — mixing metric with
 *                imperial is rejected, WEEK-3 scale field, team-agreed)
 *   * / /        element-wise exponent add / subtract (m/s is fine, m/m cancels)
 *   ^            base may be dimensional, exponent must be a plain number
 *   let x = ...  declares x with the folded rhs unit (redeclaration error)
 *   x = ...      x must already exist; waring if the new unit differs from old
 *
 * Returns the number of semantic errors found (0 == program is clean).
 * The driver translates this into EXIT_COMPILE_ERROR (see common.h).
 */
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symtab.h"

/* Analyze the whole statement list in order. st must be a fresh symbol
 * table (the analyzer performs declare-on-`let` itself, so decl order /
 * use-before-declare are checked correctly). prog may be NULL (empty file
 * counts as zero errors). */
int semantic_check(SymTab *st, AstNode *prog);

/* Fold unit vectors bottom-up through `node` and record its type.
 * Exposed so the test suite (and later Shreyansh's codegen driver) can
 * analyze a fragment without whole-program context. */
void semantic_fold_expr(AstNode *node, SymTab *st, int *error_count);

/* Compact, copy-friendly reconstruction of an expression, for diagnostics:
 *      ((5 * meters) + (10 * seconds))
 * buf must have room, or the text is truncated safely. */
void semantic_expr_str(const AstNode *e, char *buf, size_t buflen);

#endif /* SEMANTIC_H */
