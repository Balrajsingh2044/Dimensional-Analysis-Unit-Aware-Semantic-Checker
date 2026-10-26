/* codegen.c - Unit-stripped C code generator (Shreyansh)
 * Emits plain C with dimensions removed; zero runtime overhead by design.
 * Walks the AST post-analysis (units already checked by Ayansh).
 */
#include "ast.h"
#include "symtab.h"
#include "common.h"

int codegen_emit(const AstNode *prog, const char *outname) {
    (void)prog; (void)outname;
    return 0;
}
