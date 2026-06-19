#ifndef ASM_H
#define ASM_H

#include <stdio.h>

// x86-64 registers, numbered by their hardware encoding.
typedef enum {
    ASM_REG_RAX = 0, ASM_REG_RCX, ASM_REG_RDX, ASM_REG_RBX,
    ASM_REG_RSP,     ASM_REG_RBP, ASM_REG_RSI, ASM_REG_RDI,
    ASM_REG_R8,      ASM_REG_R9,  ASM_REG_R10, ASM_REG_R11,
    ASM_REG_R12,     ASM_REG_R13, ASM_REG_R14, ASM_REG_R15
} Asm_Reg;

// Every opcode the back end emits -- a small, closed set.
typedef enum {
    ASM_OP_MOV, ASM_OP_LEA, ASM_OP_PUSH, ASM_OP_POP,
    ASM_OP_ADD, ASM_OP_SUB, ASM_OP_IMUL, ASM_OP_IDIV, ASM_OP_CQO, ASM_OP_NEG,
    ASM_OP_CMP, ASM_OP_SETE, ASM_OP_SETNE, ASM_OP_SETL, ASM_OP_SETLE,
    ASM_OP_MOVZB, ASM_OP_JMP, ASM_OP_JE, ASM_OP_JNE, ASM_OP_CALL,
    ASM_OP_RET, ASM_OP_SYSCALL
} Asm_Op;

// Output sections we place code and data into.
typedef enum {
    ASM_SECTION_TEXT,
    ASM_SECTION_RODATA
} Asm_Section;

// How an operand is addressed.
typedef enum {
    ASM_OPERAND_NONE,
    ASM_OPERAND_REG,   // ao_reg, ao_width      %rax / %al
    ASM_OPERAND_IMM,   // ao_imm                $42
    ASM_OPERAND_MEM,   // ao_reg (base), ao_disp   -8(%rbp)
    ASM_OPERAND_RIP,   // ao_label              .Lstr0(%rip)
    ASM_OPERAND_LABEL  // ao_label              jump / call target
} Asm_OperandKind;

// A single instruction operand.
typedef struct {
    Asm_OperandKind ao_kind;
    Asm_Reg         ao_reg;    // REG, or base of MEM
    long            ao_imm;    // IMM
    int             ao_disp;   // MEM displacement
    const char     *ao_label;  // RIP / LABEL
    int             ao_width;  // REG width in bits: 8 or 64
} Asm_Operand;

// Clears the instruction list before a fresh translation unit.
void Asm_Reset(void);

// Non-instruction items.  The const char * arguments are printf-style formats.
void Asm_EmitLabel(const char *name, ...) __attribute__((format(printf, 1, 2)));
void Asm_EmitSection(Asm_Section sec);
void Asm_EmitGlobl(const char *name, ...) __attribute__((format(printf, 1, 2)));
void Asm_EmitBytes(const void *data, int len);
void Asm_EmitDirective(const char *text, ...) __attribute__((format(printf, 1, 2)));

// Per-instruction helpers.  Register arguments follow AT&T src,dst order.
void Asm_EmitAdd(Asm_Reg src, Asm_Reg dst);
void Asm_EmitSub(Asm_Reg src, Asm_Reg dst);
void Asm_EmitImul(Asm_Reg src, Asm_Reg dst);
void Asm_EmitCmp(Asm_Reg src, Asm_Reg dst);
void Asm_EmitMovRR(Asm_Reg src, Asm_Reg dst);

void Asm_EmitIdiv(Asm_Reg reg);
void Asm_EmitNeg(Asm_Reg reg);
void Asm_EmitCqo(void);

void Asm_EmitSete(Asm_Reg reg);
void Asm_EmitSetne(Asm_Reg reg);
void Asm_EmitSetl(Asm_Reg reg);
void Asm_EmitSetle(Asm_Reg reg);
void Asm_EmitMovzb(Asm_Reg src, Asm_Reg dst);

void Asm_EmitCmpImm(long imm, Asm_Reg dst);
void Asm_EmitMovImm(long imm, Asm_Reg dst);
void Asm_EmitMovImm8(long imm, Asm_Reg dst);
void Asm_EmitAddImm(long imm, Asm_Reg dst);
void Asm_EmitSubImm(long imm, Asm_Reg dst);

void Asm_EmitMovLoad(Asm_Reg base, int disp, Asm_Reg dst);
void Asm_EmitMovStore(Asm_Reg src, Asm_Reg base, int disp);
void Asm_EmitLea(Asm_Reg base, int disp, Asm_Reg dst);
void Asm_EmitLeaRip(Asm_Reg dst, const char *label, ...) __attribute__((format(printf, 2, 3)));

void Asm_EmitPush(Asm_Reg reg);
void Asm_EmitPop(Asm_Reg reg);

void Asm_EmitJmp(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_EmitJe(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_EmitJne(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_EmitCall(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_EmitRet(void);
void Asm_EmitSyscall(void);

// Walks the instruction list and writes AT&T-syntax assembly to out.
void Asm_PrintText(FILE *out);

#endif // ASM_H
