#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/elf.h"
#include "util/str.h"
#include "arch/x86_64/asm.h"

// The kind of one item in the instruction list.
typedef enum {
    ASM_X86_64_ITEM_INSTR,     // ai_op, ai_dst, ai_src
    ASM_X86_64_ITEM_LABEL,     // ai_label defined here
    ASM_X86_64_ITEM_GLOBL,     // ai_label marked global
    ASM_X86_64_ITEM_SECTION,   // switch to ai_section
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
    Asm_x86_64_Section   ai_section;  // SECTION
    unsigned char *ai_bytes;   // BYTES
    int           ai_nbytes;   // BYTES
};

// Head and tail of the instruction list being built.
static Asm_x86_64_Item *Asm_x86_64_Head;
static Asm_x86_64_Item *Asm_x86_64_Tail;

// 64-bit register names, indexed by Asm_x86_64_Reg.
static const char *Asm_x86_64_Reg64Name[16] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
};

// 8-bit (low-byte) register names, indexed by Asm_x86_64_Reg.
static const char *Asm_x86_64_Reg8Name[16] = {
    "al",  "cl",  "dl",   "bl",   "spl",  "bpl",  "sil",  "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
};

// Mnemonics, indexed by Asm_x86_64_Op.
static const char *Asm_x86_64_OpName[] = {
    [ASM_X86_64_OP_MOV] = "mov",  [ASM_X86_64_OP_LEA] = "lea",  [ASM_X86_64_OP_PUSH] = "push",
    [ASM_X86_64_OP_POP] = "pop",  [ASM_X86_64_OP_ADD] = "add",  [ASM_X86_64_OP_SUB] = "sub",
    [ASM_X86_64_OP_IMUL] = "imul", [ASM_X86_64_OP_IDIV] = "idiv", [ASM_X86_64_OP_CQO] = "cqo",
    [ASM_X86_64_OP_NEG] = "neg",  [ASM_X86_64_OP_CMP] = "cmp",  [ASM_X86_64_OP_SETE] = "sete",
    [ASM_X86_64_OP_SETNE] = "setne", [ASM_X86_64_OP_SETL] = "setl", [ASM_X86_64_OP_SETLE] = "setle",
    [ASM_X86_64_OP_MOVZB] = "movzb", [ASM_X86_64_OP_JMP] = "jmp", [ASM_X86_64_OP_JE] = "je",
    [ASM_X86_64_OP_JNE] = "jne",  [ASM_X86_64_OP_CALL] = "call", [ASM_X86_64_OP_RET] = "ret",
    [ASM_X86_64_OP_SYSCALL] = "syscall"
};

// Makes a 64-bit register operand (%rax .. %r15).
static Asm_x86_64_Operand Asm_x86_64_Reg64(Asm_x86_64_Reg reg)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_REG,
        .ao_reg = reg,
        .ao_width = 64
    };
}

// Makes an 8-bit low-byte register operand (%al .. %r15b).
static Asm_x86_64_Operand Asm_x86_64_Reg8(Asm_x86_64_Reg reg)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_REG,
        .ao_reg = reg,
        .ao_width = 8
    };
}

// Makes an immediate operand ($val).
static Asm_x86_64_Operand Asm_x86_64_Imm(long val)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_IMM,
        .ao_imm = val
    };
}

// Makes a base-plus-displacement memory operand (disp(%base)).
static Asm_x86_64_Operand Asm_x86_64_Mem(Asm_x86_64_Reg base, int disp)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_MEM,
        .ao_reg = base,
        .ao_disp = disp
    };
}

// Makes a RIP-relative operand naming a label (label(%rip)).
static Asm_x86_64_Operand Asm_x86_64_Rip(const char *label)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_RIP,
        .ao_label = label
    };
}

// Makes a jump or call target operand.
static Asm_x86_64_Operand Asm_x86_64_Target(const char *label)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_LABEL,
        .ao_label = label
    };
}

// Appends a fresh item of the given kind to the list and returns it.
static Asm_x86_64_Item *Asm_x86_64_New(Asm_x86_64_ItemKind kind)
{
    Asm_x86_64_Item *item = calloc(1, sizeof *item);
    item->ai_kind = kind;
    if (Asm_x86_64_Tail) {
        Asm_x86_64_Tail->ai_next = item;
    } else {
        Asm_x86_64_Head = item;
    }
    Asm_x86_64_Tail = item;
    return item;
}

// Clears the instruction list before a fresh translation unit.
void Asm_x86_64_Reset(void)
{
    Asm_x86_64_Item *item = Asm_x86_64_Head;
    while (item) {
        Asm_x86_64_Item *next = item->ai_next;
        free((char *) item->ai_label);
        free((char *) item->ai_text);
        free((char *) item->ai_dst.ao_label);
        free((char *) item->ai_src.ao_label);
        free(item->ai_bytes);
        free(item);
        item = next;
    }
    Asm_x86_64_Head = NULL;
    Asm_x86_64_Tail = NULL;
}

// Emits a label definition, named by a printf-style format.
void Asm_x86_64_EmitLabel(const char *name, ...)
{
    va_list ap;
    va_start(ap, name);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_LABEL);
    item->ai_label = Str_VFormat(name, ap);
    va_end(ap);
}

// Emits a switch to the given output section.
void Asm_x86_64_EmitSection(Asm_x86_64_Section sec)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_SECTION);
    item->ai_section = sec;
}

// Marks a symbol global, named by a printf-style format.
void Asm_x86_64_EmitGlobl(const char *name, ...)
{
    va_list ap;
    va_start(ap, name);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_GLOBL);
    item->ai_label = Str_VFormat(name, ap);
    va_end(ap);
}

// Emits a run of raw data bytes.
void Asm_x86_64_EmitBytes(const void *data, int len)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_BYTES);
    item->ai_bytes = malloc(len);
    memcpy(item->ai_bytes, data, len);
    item->ai_nbytes = len;
}

// Emits a raw assembler line from a printf-style format, written with indent.
void Asm_x86_64_EmitDirective(const char *text, ...)
{
    va_list ap;
    va_start(ap, text);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_DIRECTIVE);
    item->ai_text = Str_VFormat(text, ap);
    va_end(ap);
}

// Emits `add %src, %dst`.
void Asm_x86_64_EmitAdd(Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_ADD;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Reg64(src);
}

// Emits `sub %src, %dst`.
void Asm_x86_64_EmitSub(Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_SUB;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Reg64(src);
}

// Emits `imul %src, %dst`.
void Asm_x86_64_EmitImul(Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_IMUL;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Reg64(src);
}

// Emits `cmp %src, %dst`.
void Asm_x86_64_EmitCmp(Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_CMP;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Reg64(src);
}

// Emits `mov %src, %dst` between two registers.
void Asm_x86_64_EmitMovRR(Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_MOV;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Reg64(src);
}

// Emits `idiv %reg`.
void Asm_x86_64_EmitIdiv(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_IDIV;
    item->ai_dst = Asm_x86_64_Reg64(reg);
}

// Emits `neg %reg`.
void Asm_x86_64_EmitNeg(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_NEG;
    item->ai_dst = Asm_x86_64_Reg64(reg);
}

// Emits `cqo` (sign-extend %rax into %rdx:%rax).
void Asm_x86_64_EmitCqo(void)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op = ASM_X86_64_OP_CQO;
}

// Emits `sete %reg`.
void Asm_x86_64_EmitSete(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_SETE;
    item->ai_dst = Asm_x86_64_Reg8(reg);
}

// Emits `setne %reg`.
void Asm_x86_64_EmitSetne(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_SETNE;
    item->ai_dst = Asm_x86_64_Reg8(reg);
}

// Emits `setl %reg`.
void Asm_x86_64_EmitSetl(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_SETL;
    item->ai_dst = Asm_x86_64_Reg8(reg);
}

// Emits `setle %reg`.
void Asm_x86_64_EmitSetle(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_SETLE;
    item->ai_dst = Asm_x86_64_Reg8(reg);
}

// Emits `movzb %src, %dst` (zero-extend a byte into a 64-bit register).
void Asm_x86_64_EmitMovzb(Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_MOVZB;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Reg8(src);
}

// Emits `cmp $imm, %dst`.
void Asm_x86_64_EmitCmpImm(long imm, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_CMP;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Imm(imm);
}

// Emits `mov $imm, %dst` into a 64-bit register.
void Asm_x86_64_EmitMovImm(long imm, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_MOV;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Imm(imm);
}

// Emits `mov $imm, %dst` into an 8-bit register.
void Asm_x86_64_EmitMovImm8(long imm, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_MOV;
    item->ai_dst = Asm_x86_64_Reg8(dst);
    item->ai_src = Asm_x86_64_Imm(imm);
}

// Emits `add $imm, %dst`.
void Asm_x86_64_EmitAddImm(long imm, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_ADD;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Imm(imm);
}

// Emits `sub $imm, %dst`.
void Asm_x86_64_EmitSubImm(long imm, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_SUB;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Imm(imm);
}

// Emits `mov disp(%base), %dst` (load from memory).
void Asm_x86_64_EmitMovLoad(Asm_x86_64_Reg base, int disp, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_MOV;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Mem(base, disp);
}

// Emits `mov %src, disp(%base)` (store to memory).
void Asm_x86_64_EmitMovStore(Asm_x86_64_Reg src, Asm_x86_64_Reg base, int disp)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_MOV;
    item->ai_dst = Asm_x86_64_Mem(base, disp);
    item->ai_src = Asm_x86_64_Reg64(src);
}

// Emits `lea disp(%base), %dst`.
void Asm_x86_64_EmitLea(Asm_x86_64_Reg base, int disp, Asm_x86_64_Reg dst)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_LEA;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Mem(base, disp);
}

// Emits `lea label(%rip), %dst`, with label from a printf-style format.
void Asm_x86_64_EmitLeaRip(Asm_x86_64_Reg dst, const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_LEA;
    item->ai_dst = Asm_x86_64_Reg64(dst);
    item->ai_src = Asm_x86_64_Rip(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `push %reg`.
void Asm_x86_64_EmitPush(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_PUSH;
    item->ai_dst = Asm_x86_64_Reg64(reg);
}

// Emits `pop %reg`.
void Asm_x86_64_EmitPop(Asm_x86_64_Reg reg)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_POP;
    item->ai_dst = Asm_x86_64_Reg64(reg);
}

// Emits `jmp label`, with label from a printf-style format.
void Asm_x86_64_EmitJmp(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_JMP;
    item->ai_dst = Asm_x86_64_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `je label`, with label from a printf-style format.
void Asm_x86_64_EmitJe(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_JE;
    item->ai_dst = Asm_x86_64_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `jne label`, with label from a printf-style format.
void Asm_x86_64_EmitJne(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_JNE;
    item->ai_dst = Asm_x86_64_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `call label`, with label from a printf-style format.
void Asm_x86_64_EmitCall(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op  = ASM_X86_64_OP_CALL;
    item->ai_dst = Asm_x86_64_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `ret`.
void Asm_x86_64_EmitRet(void)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op = ASM_X86_64_OP_RET;
}

// Emits `syscall`.
void Asm_x86_64_EmitSyscall(void)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op = ASM_X86_64_OP_SYSCALL;
}

// Writes one operand in AT&T syntax.
static void Asm_x86_64_PrintOperand(FILE *out, const Asm_x86_64_Operand *op)
{
    switch (op->ao_kind) {
        case ASM_X86_64_OPERAND_REG: {
            const char *name = op->ao_width == 8 ? Asm_x86_64_Reg8Name[op->ao_reg]
                                                 : Asm_x86_64_Reg64Name[op->ao_reg];
            fprintf(out, "%%%s", name);
        } break;
        case ASM_X86_64_OPERAND_IMM: {
            fprintf(out, "$%ld", op->ao_imm);
        } break;
        case ASM_X86_64_OPERAND_MEM: {
            if (op->ao_disp) {
                fprintf(out, "%d(%%%s)", op->ao_disp, Asm_x86_64_Reg64Name[op->ao_reg]);
            } else {
                fprintf(out, "(%%%s)", Asm_x86_64_Reg64Name[op->ao_reg]);
            }
        } break;
        case ASM_X86_64_OPERAND_RIP: {
            fprintf(out, "%s(%%rip)", op->ao_label);
        } break;
        case ASM_X86_64_OPERAND_LABEL: {
            fprintf(out, "%s", op->ao_label);
        } break;
        case ASM_X86_64_OPERAND_NONE: {
            // nothing to print
        } break;
    }
}

// Writes one instruction: mnemonic plus operands in AT&T order.
static void Asm_x86_64_PrintInstr(FILE *out, const Asm_x86_64_Item *item)
{
    fprintf(out, "  %s", Asm_x86_64_OpName[item->ai_op]);

    int have_src = item->ai_src.ao_kind != ASM_X86_64_OPERAND_NONE;
    int have_dst = item->ai_dst.ao_kind != ASM_X86_64_OPERAND_NONE;

    if (have_src) {
        fputc(' ', out);
        Asm_x86_64_PrintOperand(out, &item->ai_src);
    }
    if (have_dst) {
        fputs(have_src ? ", " : " ", out);
        Asm_x86_64_PrintOperand(out, &item->ai_dst);
    }
    fputc('\n', out);
}

// Walks the instruction list and writes AT&T-syntax assembly to out.
void Asm_x86_64_PrintText(FILE *out)
{
    for (Asm_x86_64_Item *item = Asm_x86_64_Head; item; item = item->ai_next) {
        switch (item->ai_kind) {
            case ASM_X86_64_ITEM_INSTR: {
                Asm_x86_64_PrintInstr(out, item);
            } break;
            case ASM_X86_64_ITEM_LABEL: {
                fprintf(out, "%s:\n", item->ai_label);
            } break;
            case ASM_X86_64_ITEM_GLOBL: {
                fprintf(out, "  .globl %s\n", item->ai_label);
            } break;
            case ASM_X86_64_ITEM_SECTION: {
                fprintf(out, "%s\n", item->ai_section == ASM_X86_64_SECTION_TEXT
                                         ? "  .text" : "  .section .rodata");
            } break;
            case ASM_X86_64_ITEM_BYTES: {
                for (int i = 0; i < item->ai_nbytes; i++) {
                    fprintf(out, "  .byte %d\n", item->ai_bytes[i]);
                }
            } break;
            case ASM_X86_64_ITEM_DIRECTIVE: {
                fprintf(out, "  %s\n", item->ai_text);
            } break;
        }
    }
}

// REX prefix bits: 64-bit operand (W) and high-register extensions (R/B).
#define REX_BASE 0x40
#define REX_W    0x08
#define REX_R    0x04
#define REX_B    0x01

// ModRM mod field for a register-direct operand (mod = 11).
#define MODRM_DIRECT 0xC0

// SIB byte selecting %rsp as base with no index.
#define SIB_BASE_RSP 0x24

// Opcode-group /digit extensions for the 0x81 and 0xF7 instruction groups.
#define GRP_ADD  0
#define GRP_SUB  5
#define GRP_CMP  7
#define GRP_NEG  3
#define GRP_IDIV 7

// Returns the high bit of a register number, extending ModRM.reg or .rm.
static int Asm_x86_64_RegHigh(Asm_x86_64_Reg reg)
{
    return reg >> 3;
}

// Emits a REX.W prefix with the given reg- and rm-field extension bits.
static void Asm_x86_64_EncRexW(int regHigh, int rmHigh)
{
    Elf_WriteByte(REX_BASE | REX_W | (regHigh ? REX_R : 0) | (rmHigh ? REX_B : 0));
}

// Emits a register-direct ModRM byte pairing reg with rm.
static void Asm_x86_64_EncModRR(int reg, Asm_x86_64_Reg rm)
{
    Elf_WriteByte(MODRM_DIRECT | ((reg & 7) << 3) | (rm & 7));
}

// Emits the ModRM, optional SIB and displacement for disp(%base).
static void Asm_x86_64_EncMem(int reg, Asm_x86_64_Reg base, int disp)
{
    int rm  = base & 7;
    int mod;
    if (disp == 0 && rm != (ASM_X86_64_REG_RBP & 7)) {
        mod = 0;
    } else if (disp >= -128 && disp <= 127) {
        mod = 1;
    } else {
        mod = 2;
    }

    Elf_WriteByte((mod << 6) | ((reg & 7) << 3) | rm);
    if (rm == (ASM_X86_64_REG_RSP & 7)) {
        Elf_WriteByte(SIB_BASE_RSP);
    }
    if (mod == 1) {
        Elf_WriteByte(disp & 0xFF);
    } else if (mod == 2) {
        Elf_Write32((unsigned int) disp);
    }
}

// Emits `<opcode> %src, %dst` for a register-to-register operation.
static void Asm_x86_64_EncRR(int opcode, Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Asm_x86_64_EncRexW(Asm_x86_64_RegHigh(src), Asm_x86_64_RegHigh(dst));
    Elf_WriteByte(opcode);
    Asm_x86_64_EncModRR(src, dst);
}

// Emits a 0x81-group `<grp> $imm, %dst` with a 32-bit immediate.
static void Asm_x86_64_EncGrpImm(int grp, long imm, Asm_x86_64_Reg dst)
{
    Asm_x86_64_EncRexW(0, Asm_x86_64_RegHigh(dst));
    Elf_WriteByte(0x81);
    Asm_x86_64_EncModRR(grp, dst);
    Elf_Write32((unsigned int) imm);
}

// Emits `mov $imm, %dst` into a 64-bit register.
static void Asm_x86_64_EncMovImm(long imm, Asm_x86_64_Reg dst)
{
    if (imm >= INT32_MIN && imm <= INT32_MAX) {
        Asm_x86_64_EncRexW(0, Asm_x86_64_RegHigh(dst));
        Elf_WriteByte(0xC7);
        Asm_x86_64_EncModRR(0, dst);
        Elf_Write32((unsigned int) imm);
    } else {
        Asm_x86_64_EncRexW(0, Asm_x86_64_RegHigh(dst));
        Elf_WriteByte(0xB8 + (dst & 7));
        Elf_Write64((unsigned long long) imm);
    }
}

// Emits `mov $imm, %dst` into an 8-bit register.
static void Asm_x86_64_EncMovImm8(long imm, Asm_x86_64_Reg dst)
{
    if (dst >= ASM_X86_64_REG_R8) {
        Elf_WriteByte(REX_BASE | REX_B);
    } else if (dst >= ASM_X86_64_REG_RSP) {
        Elf_WriteByte(REX_BASE);
    }
    Elf_WriteByte(0xB0 + (dst & 7));
    Elf_WriteByte(imm & 0xFF);
}

// Emits `<opcode> disp(%base), %reg` (or the reverse for a store).
static void Asm_x86_64_EncMemForm(int opcode, Asm_x86_64_Reg reg, Asm_x86_64_Reg base, int disp)
{
    Asm_x86_64_EncRexW(Asm_x86_64_RegHigh(reg), Asm_x86_64_RegHigh(base));
    Elf_WriteByte(opcode);
    Asm_x86_64_EncMem(reg, base, disp);
}

// Emits `lea label(%rip), %dst` with a rel32 fixup to label.
static void Asm_x86_64_EncLeaRip(Asm_x86_64_Reg dst, const char *label)
{
    Asm_x86_64_EncRexW(Asm_x86_64_RegHigh(dst), 0);
    Elf_WriteByte(0x8D);
    Elf_WriteByte(((dst & 7) << 3) | 5);
    Elf_AddRel32(label);
    Elf_Write32(0);
}

// Emits a 0xF7-group unary instruction `<grp> %reg`.
static void Asm_x86_64_EncGrpUnary(int grp, Asm_x86_64_Reg reg)
{
    Asm_x86_64_EncRexW(0, Asm_x86_64_RegHigh(reg));
    Elf_WriteByte(0xF7);
    Asm_x86_64_EncModRR(grp, reg);
}

// Emits a `setcc %reg` byte-setting instruction.
static void Asm_x86_64_EncSetcc(int opcode, Asm_x86_64_Reg reg)
{
    if (reg >= ASM_X86_64_REG_R8) {
        Elf_WriteByte(REX_BASE | REX_B);
    } else if (reg >= ASM_X86_64_REG_RSP) {
        Elf_WriteByte(REX_BASE);
    }
    Elf_WriteByte(0x0F);
    Elf_WriteByte(opcode);
    Asm_x86_64_EncModRR(0, reg);
}

// Emits a rel32 control-transfer instruction with a fixup to its target.
static void Asm_x86_64_EncBranch(const Asm_x86_64_Item *item)
{
    switch (item->ai_op) {
        case ASM_X86_64_OP_JMP:  Elf_WriteByte(0xE9);                      break;
        case ASM_X86_64_OP_CALL: Elf_WriteByte(0xE8);                      break;
        case ASM_X86_64_OP_JE:   Elf_WriteByte(0x0F); Elf_WriteByte(0x84); break;
        case ASM_X86_64_OP_JNE:  Elf_WriteByte(0x0F); Elf_WriteByte(0x85); break;
        default: break;
    }
    Elf_AddRel32(item->ai_dst.ao_label);
    Elf_Write32(0);
}

// Emits a `mov` in whichever of its forms the operands select.
static void Asm_x86_64_EncMov(const Asm_x86_64_Item *item)
{
    Asm_x86_64_Reg dst = item->ai_dst.ao_reg;
    Asm_x86_64_Reg src = item->ai_src.ao_reg;

    switch (item->ai_src.ao_kind) {
        case ASM_X86_64_OPERAND_IMM: {
            if (item->ai_dst.ao_width == 8) {
                Asm_x86_64_EncMovImm8(item->ai_src.ao_imm, dst);
            } else {
                Asm_x86_64_EncMovImm(item->ai_src.ao_imm, dst);
            }
        } break;
        case ASM_X86_64_OPERAND_MEM: {
            Asm_x86_64_EncMemForm(0x8B, dst, item->ai_src.ao_reg, item->ai_src.ao_disp);
        } break;
        case ASM_X86_64_OPERAND_REG: {
            if (item->ai_dst.ao_kind == ASM_X86_64_OPERAND_MEM) {
                Asm_x86_64_EncMemForm(0x89, src, item->ai_dst.ao_reg, item->ai_dst.ao_disp);
            } else {
                Asm_x86_64_EncRR(0x89, src, dst);
            }
        } break;
        default: break;
    }
}

// Encodes one instruction item into the current section.
static void Asm_x86_64_EncodeInstr(const Asm_x86_64_Item *item)
{
    Asm_x86_64_Reg dst = item->ai_dst.ao_reg;
    Asm_x86_64_Reg src = item->ai_src.ao_reg;
    int     imm = item->ai_src.ao_kind == ASM_X86_64_OPERAND_IMM;

    switch (item->ai_op) {
        case ASM_X86_64_OP_ADD: {
            if (imm) {
                Asm_x86_64_EncGrpImm(GRP_ADD, item->ai_src.ao_imm, dst);
            } else {
                Asm_x86_64_EncRR(0x01, src, dst);
            }
        } break;
        case ASM_X86_64_OP_SUB: {
            if (imm) {
                Asm_x86_64_EncGrpImm(GRP_SUB, item->ai_src.ao_imm, dst);
            } else {
                Asm_x86_64_EncRR(0x29, src, dst);
            }
        } break;
        case ASM_X86_64_OP_CMP: {
            if (imm) {
                Asm_x86_64_EncGrpImm(GRP_CMP, item->ai_src.ao_imm, dst);
            } else {
                Asm_x86_64_EncRR(0x39, src, dst);
            }
        } break;
        case ASM_X86_64_OP_IMUL: {
            Asm_x86_64_EncRexW(Asm_x86_64_RegHigh(dst), Asm_x86_64_RegHigh(src));
            Elf_WriteByte(0x0F);
            Elf_WriteByte(0xAF);
            Asm_x86_64_EncModRR(dst, src);
        } break;
        case ASM_X86_64_OP_MOV: {
            Asm_x86_64_EncMov(item);
        } break;
        case ASM_X86_64_OP_LEA: {
            if (item->ai_src.ao_kind == ASM_X86_64_OPERAND_RIP) {
                Asm_x86_64_EncLeaRip(dst, item->ai_src.ao_label);
            } else {
                Asm_x86_64_EncMemForm(0x8D, dst, item->ai_src.ao_reg, item->ai_src.ao_disp);
            }
        } break;
        case ASM_X86_64_OP_IDIV: {
            Asm_x86_64_EncGrpUnary(GRP_IDIV, dst);
        } break;
        case ASM_X86_64_OP_NEG: {
            Asm_x86_64_EncGrpUnary(GRP_NEG, dst);
        } break;
        case ASM_X86_64_OP_CQO: {
            Elf_WriteByte(REX_BASE | REX_W);
            Elf_WriteByte(0x99);
        } break;
        case ASM_X86_64_OP_SETE:  { Asm_x86_64_EncSetcc(0x94, dst); } break;
        case ASM_X86_64_OP_SETNE: { Asm_x86_64_EncSetcc(0x95, dst); } break;
        case ASM_X86_64_OP_SETL:  { Asm_x86_64_EncSetcc(0x9C, dst); } break;
        case ASM_X86_64_OP_SETLE: { Asm_x86_64_EncSetcc(0x9E, dst); } break;
        case ASM_X86_64_OP_MOVZB: {
            Asm_x86_64_EncRexW(Asm_x86_64_RegHigh(dst), Asm_x86_64_RegHigh(src));
            Elf_WriteByte(0x0F);
            Elf_WriteByte(0xB6);
            Asm_x86_64_EncModRR(dst, src);
        } break;
        case ASM_X86_64_OP_PUSH: {
            if (dst >= ASM_X86_64_REG_R8) {
                Elf_WriteByte(REX_BASE | REX_B);
            }
            Elf_WriteByte(0x50 + (dst & 7));
        } break;
        case ASM_X86_64_OP_POP: {
            if (dst >= ASM_X86_64_REG_R8) {
                Elf_WriteByte(REX_BASE | REX_B);
            }
            Elf_WriteByte(0x58 + (dst & 7));
        } break;
        case ASM_X86_64_OP_JMP:
        case ASM_X86_64_OP_JE:
        case ASM_X86_64_OP_JNE:
        case ASM_X86_64_OP_CALL: {
            Asm_x86_64_EncBranch(item);
        } break;
        case ASM_X86_64_OP_RET: {
            Elf_WriteByte(0xC3);
        } break;
        case ASM_X86_64_OP_SYSCALL: {
            Elf_WriteByte(0x0F);
            Elf_WriteByte(0x05);
        } break;
    }
}

// Walks the instruction list and encodes machine code into the ELF image.
void Asm_x86_64_Encode(void)
{
    for (Asm_x86_64_Item *item = Asm_x86_64_Head; item; item = item->ai_next) {
        switch (item->ai_kind) {
            case ASM_X86_64_ITEM_INSTR: {
                Asm_x86_64_EncodeInstr(item);
            } break;
            case ASM_X86_64_ITEM_LABEL: {
                Elf_AddSymbol(item->ai_label);
            } break;
            case ASM_X86_64_ITEM_SECTION: {
                Elf_ChangeSection(item->ai_section == ASM_X86_64_SECTION_TEXT
                                      ? ELF_SECTION_TEXT : ELF_SECTION_RODATA);
            } break;
            case ASM_X86_64_ITEM_BYTES: {
                Elf_WriteBytes(item->ai_bytes, item->ai_nbytes);
            } break;
            case ASM_X86_64_ITEM_GLOBL:
            case ASM_X86_64_ITEM_DIRECTIVE: {
                // not represented in the executable image
            } break;
        }
    }
}
