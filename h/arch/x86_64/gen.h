#ifndef GEN_X86_64_H
#define GEN_X86_64_H

#include <stdio.h>

#include "ast/ast.h"
#include "arch/x86_64/asm.h"

// Emits x86-64 assembly (System V AMD64, AT&T syntax) for the program to out.
void Gen_x86_64_CodegenAsm(FILE *out, Ast_Func *prog);

// Encodes the program as a freestanding ELF executable written to out.
void Gen_x86_64_CodegenElf(FILE *out, Ast_Func *prog);

// Returns the next unique label number.
int Gen_x86_64_Count(void);

// Pushes %rax onto the stack and tracks the depth.
void Gen_x86_64_EmitPush(void);

// Pops the top of the stack into reg and tracks the depth.
void Gen_x86_64_EmitPop(Asm_x86_64_Reg reg);

// Rounds n up to the nearest multiple of align.
int Gen_x86_64_AlignTo(int n, int align);

// Computes the address of an lvalue into %rax.
void Gen_x86_64_EmitAddr(Ast_Node *node);

// Emits code for an expression, leaving its result in %rax.
void Gen_x86_64_EmitExpr(Ast_Node *node);

// Emits code for a statement.
void Gen_x86_64_EmitStmt(Ast_Node *node);

// Assigns each local a stack slot and records the frame size.
void Gen_x86_64_AssignLvarOffsets(Ast_Func *func);

// Emits the .rodata section holding all string literals.
void Gen_x86_64_EmitDataSection(void);

// Emits the .text section; freestanding output also gets a _start runtime.
void Gen_x86_64_EmitTextSection(Ast_Func *prog, int freestanding);

#endif // GEN_X86_64_H
