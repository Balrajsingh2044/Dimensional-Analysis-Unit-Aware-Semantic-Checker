#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

/* Frozen team-contract entry point (Shreyansh). Emits unit-stripped C:
 * outname == NULL -> stdout; returns 0 on success. Equivalent to
 * codegen_emit_print(prog, outname, 0). MUST NOT CHANGE. */
int codegen_emit(const AstNode *prog, const char *outname);

/* ADDITIVE Week-4 extension (Balraj, presentation support): same emission
 * as codegen_emit, but when print_last != 0 the generated main() also
 * PRINTS its most recently assigned variable (printf("%g\n", ...)) —
 * "the compiler not only protects the calculation, it hands you the
 * answer." Silent no-op flag programs with no assignable statement.
 * stdout stays pure compilable C either way. */
int codegen_emit_print(const AstNode *prog, const char *outname,
                       int print_last);

#endif
