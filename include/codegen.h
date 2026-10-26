#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

int codegen_emit(const AstNode *prog, const char *outname);

#endif
