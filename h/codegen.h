/* codegen.h - x86-64 (System V AMD64, AT&T syntax) code generator. */
#ifndef CC_CODEGEN_H
#define CC_CODEGEN_H

#include <stdio.h>
#include "ast.h"

/* Emit assembly for the whole program to `out`. */
void codegen(FILE *out, Function *prog);

#endif /* CC_CODEGEN_H */
