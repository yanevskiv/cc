#ifndef GEN_X86_64_H
#define GEN_X86_64_H

#include <stdio.h>

#include "ast/ast.h"
#include "arch/x86_64/asm.h"

// Top-level code generation
void Gen_x86_64_CodegenAsm(FILE *out, Ast_Func *prog);
void Gen_x86_64_CodegenExec(FILE *out, Ast_Func *prog);
void Gen_x86_64_CodegenRel(FILE *out, Ast_Func *prog);
void Gen_x86_64_CodegenRelStart(FILE *out, Ast_Func *prog);

// Code emission helpers
int  Gen_x86_64_Count(void);
void Gen_x86_64_EmitPush(void);
void Gen_x86_64_EmitPop(Asm_x86_64_Reg reg);
int  Gen_x86_64_AlignTo(int n, int align);
void Gen_x86_64_EmitAddr(Ast_Node *node);
void Gen_x86_64_EmitExpr(Ast_Node *node);
void Gen_x86_64_EmitStmt(Ast_Node *node);
void Gen_x86_64_AssignLvarOffsets(Ast_Func *func);
void Gen_x86_64_EmitDataSection(void);
void Gen_x86_64_EmitTextSection(Ast_Func *prog, int freestanding);

#endif // GEN_X86_64_H
