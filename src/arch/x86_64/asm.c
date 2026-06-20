#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/str.h"
#include "arch/x86_64/asm.h"

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
Asm_x86_64_Operand Asm_x86_64_Reg64(Asm_x86_64_Reg reg)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_REG,
        .ao_reg = reg,
        .ao_width = 64
    };
}

// Makes an 8-bit low-byte register operand (%al .. %r15b).
Asm_x86_64_Operand Asm_x86_64_Reg8(Asm_x86_64_Reg reg)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_REG,
        .ao_reg = reg,
        .ao_width = 8
    };
}

// Makes an immediate operand ($val).
Asm_x86_64_Operand Asm_x86_64_Imm(long val)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_IMM,
        .ao_imm = val
    };
}

// Makes a base-plus-displacement memory operand (disp(%base)).
Asm_x86_64_Operand Asm_x86_64_Mem(Asm_x86_64_Reg base, int disp)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_MEM,
        .ao_reg = base,
        .ao_disp = disp
    };
}

// Makes a RIP-relative operand naming a label (label(%rip)).
Asm_x86_64_Operand Asm_x86_64_Rip(const char *label)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_RIP,
        .ao_label = label
    };
}

// Makes a jump or call target operand.
Asm_x86_64_Operand Asm_x86_64_Target(const char *label)
{
    return (Asm_x86_64_Operand) {
        .ao_kind = ASM_X86_64_OPERAND_LABEL,
        .ao_label = label
    };
}

// Appends a fresh item of the given kind to the list and returns it.
Asm_x86_64_Item *Asm_x86_64_New(Asm_x86_64_ItemKind kind)
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

// Returns the head of the instruction list for a back end to walk.
Asm_x86_64_Item *Asm_x86_64_Items(void)
{
    return Asm_x86_64_Head;
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

// Emits a switch to the named output section, created with type and flags.
void Asm_x86_64_EmitSection(const char *name, uint32_t type, uint64_t flags)
{
    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_SECTION);
    item->ai_secname  = name;
    item->ai_sectype  = type;
    item->ai_secflags = flags;
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
void Asm_x86_64_PrintOperand(FILE *out, const Asm_x86_64_Operand *op)
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
void Asm_x86_64_PrintInstr(FILE *out, const Asm_x86_64_Item *item)
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
                if (strcmp(item->ai_secname, ".text") == 0) {
                    fprintf(out, "  .text\n");
                } else {
                    fprintf(out, "  .section %s\n", item->ai_secname);
                }
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
