#ifndef ASM_X86_64_H
#define ASM_X86_64_H

#include <stdint.h>
#include <stdio.h>

// Registers
typedef enum {
    ASM_X86_64_REG_RAX = 0,
    ASM_X86_64_REG_RCX,
    ASM_X86_64_REG_RDX,
    ASM_X86_64_REG_RBX,
    ASM_X86_64_REG_RSP,
    ASM_X86_64_REG_RBP,
    ASM_X86_64_REG_RSI,
    ASM_X86_64_REG_RDI,
    ASM_X86_64_REG_R8,
    ASM_X86_64_REG_R9,
    ASM_X86_64_REG_R10,
    ASM_X86_64_REG_R11,
    ASM_X86_64_REG_R12,
    ASM_X86_64_REG_R13,
    ASM_X86_64_REG_R14,
    ASM_X86_64_REG_R15
} Asm_x86_64_Reg;

// Opcodes
typedef enum {
    ASM_X86_64_OP_MOV = 0,
    ASM_X86_64_OP_LEA,
    ASM_X86_64_OP_PUSH,
    ASM_X86_64_OP_POP,
    ASM_X86_64_OP_ADD,
    ASM_X86_64_OP_SUB,
    ASM_X86_64_OP_IMUL,
    ASM_X86_64_OP_IDIV,
    ASM_X86_64_OP_CQO,
    ASM_X86_64_OP_NEG,
    ASM_X86_64_OP_CMP,
    ASM_X86_64_OP_SETE,
    ASM_X86_64_OP_SETNE,
    ASM_X86_64_OP_SETL,
    ASM_X86_64_OP_SETLE,
    ASM_X86_64_OP_MOVZB,
    ASM_X86_64_OP_JMP,
    ASM_X86_64_OP_JE,
    ASM_X86_64_OP_JNE,
    ASM_X86_64_OP_CALL,
    ASM_X86_64_OP_RET,
    ASM_X86_64_OP_SYSCALL
} Asm_x86_64_Op;

// How an operand is addressed.
typedef enum {
    ASM_X86_64_OPERAND_NONE,
    ASM_X86_64_OPERAND_REG,   // ao_reg, ao_width      %rax / %al
    ASM_X86_64_OPERAND_IMM,   // ao_imm                $42
    ASM_X86_64_OPERAND_MEM,   // ao_reg (base), ao_disp   -8(%rbp)
    ASM_X86_64_OPERAND_RIP,   // ao_label              .Lstr0(%rip)
    ASM_X86_64_OPERAND_LABEL  // ao_label              jump / call target
} Asm_x86_64_OperandKind;

// A single instruction operand.
typedef struct {
    Asm_x86_64_OperandKind ao_kind;
    Asm_x86_64_Reg         ao_reg;    // REG, or base of MEM
    long                   ao_imm;    // IMM
    int                    ao_disp;   // MEM displacement
    const char            *ao_label;  // RIP / LABEL
    int                    ao_width;  // REG width in bits: 8 or 64
} Asm_x86_64_Operand;

// The kind of one item in the instruction list.
typedef enum {
    ASM_X86_64_ITEM_INSTR,     // ai_op, ai_dst, ai_src
    ASM_X86_64_ITEM_LABEL,     // ai_label defined here
    ASM_X86_64_ITEM_GLOBL,     // ai_label marked global
    ASM_X86_64_ITEM_SECTION,   // switch to ai_secname
    ASM_X86_64_ITEM_BYTES,     // ai_bytes / ai_nbytes raw data
    ASM_X86_64_ITEM_DIRECTIVE  // ai_text raw assembler line
} Asm_x86_64_ItemKind;

// One node in the ordered instruction list.
typedef struct Asm_x86_64_Item Asm_x86_64_Item;
struct Asm_x86_64_Item {
    Asm_x86_64_Item     *ai_next;
    Asm_x86_64_ItemKind  ai_kind;
    Asm_x86_64_Op        ai_op;       // INSTR
    Asm_x86_64_Operand   ai_dst;      // INSTR
    Asm_x86_64_Operand   ai_src;      // INSTR
    const char   *ai_label;    // LABEL / GLOBL
    const char   *ai_text;     // DIRECTIVE
    const char   *ai_secname;  // SECTION
    uint32_t      ai_sectype;  // SECTION
    uint64_t      ai_secflags; // SECTION
    unsigned char *ai_bytes;   // BYTES
    int           ai_nbytes;   // BYTES
};

// Operand constructors
Asm_x86_64_Operand Asm_x86_64_Reg64(Asm_x86_64_Reg reg);
Asm_x86_64_Operand Asm_x86_64_Reg8(Asm_x86_64_Reg reg);
Asm_x86_64_Operand Asm_x86_64_Imm(long val);
Asm_x86_64_Operand Asm_x86_64_Mem(Asm_x86_64_Reg base, int disp);
Asm_x86_64_Operand Asm_x86_64_Rip(const char *label);
Asm_x86_64_Operand Asm_x86_64_Target(const char *label);

// Instruction list
Asm_x86_64_Item *Asm_x86_64_New(Asm_x86_64_ItemKind kind);
void Asm_x86_64_Reset(void);
Asm_x86_64_Item *Asm_x86_64_Items(void);

// Non-instruction items
void Asm_x86_64_EmitLabel(const char *name, ...) __attribute__((format(printf, 1, 2)));
void Asm_x86_64_EmitSection(const char *name, uint32_t type, uint64_t flags);
void Asm_x86_64_EmitGlobl(const char *name, ...) __attribute__((format(printf, 1, 2)));
void Asm_x86_64_EmitBytes(const void *data, int len);
void Asm_x86_64_EmitDirective(const char *text, ...) __attribute__((format(printf, 1, 2)));

// Register-to-register
void Asm_x86_64_EmitAdd(Asm_x86_64_Reg src, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitSub(Asm_x86_64_Reg src, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitImul(Asm_x86_64_Reg src, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitCmp(Asm_x86_64_Reg src, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitMovRR(Asm_x86_64_Reg src, Asm_x86_64_Reg dst);

// Single register
void Asm_x86_64_EmitIdiv(Asm_x86_64_Reg reg);
void Asm_x86_64_EmitNeg(Asm_x86_64_Reg reg);
void Asm_x86_64_EmitCqo(void);

// Condition flags to a register
void Asm_x86_64_EmitSete(Asm_x86_64_Reg reg);
void Asm_x86_64_EmitSetne(Asm_x86_64_Reg reg);
void Asm_x86_64_EmitSetl(Asm_x86_64_Reg reg);
void Asm_x86_64_EmitSetle(Asm_x86_64_Reg reg);
void Asm_x86_64_EmitMovzb(Asm_x86_64_Reg src, Asm_x86_64_Reg dst);

// Immediate operands
void Asm_x86_64_EmitCmpImm(long imm, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitMovImm(long imm, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitMovImm8(long imm, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitAddImm(long imm, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitSubImm(long imm, Asm_x86_64_Reg dst);

// Memory loads, stores and addresses
void Asm_x86_64_EmitMovLoad(Asm_x86_64_Reg base, int disp, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitMovStore(Asm_x86_64_Reg src, Asm_x86_64_Reg base, int disp);
void Asm_x86_64_EmitLea(Asm_x86_64_Reg base, int disp, Asm_x86_64_Reg dst);
void Asm_x86_64_EmitLeaRip(Asm_x86_64_Reg dst, const char *label, ...) __attribute__((format(printf, 2, 3)));

// Stack
void Asm_x86_64_EmitPush(Asm_x86_64_Reg reg);
void Asm_x86_64_EmitPop(Asm_x86_64_Reg reg);

// Jumps, calls and returns
void Asm_x86_64_EmitJmp(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_x86_64_EmitJe(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_x86_64_EmitJne(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_x86_64_EmitCall(const char *label, ...) __attribute__((format(printf, 1, 2)));
void Asm_x86_64_EmitRet(void);
void Asm_x86_64_EmitSyscall(void);

// Text output
void Asm_x86_64_PrintOperand(FILE *out, const Asm_x86_64_Operand *op);
void Asm_x86_64_PrintInstr(FILE *out, const Asm_x86_64_Item *item);
void Asm_x86_64_PrintText(FILE *out);

#endif // ASM_X86_64_H
