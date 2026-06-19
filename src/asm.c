#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "asm.h"
#include "str.h"

// The kind of one item in the instruction list.
typedef enum {
    ASM_ITEM_INSTR,     // ai_op, ai_dst, ai_src
    ASM_ITEM_LABEL,     // ai_label defined here
    ASM_ITEM_GLOBL,     // ai_label marked global
    ASM_ITEM_SECTION,   // switch to ai_section
    ASM_ITEM_BYTES,     // ai_bytes / ai_nbytes raw data
    ASM_ITEM_DIRECTIVE  // ai_text raw assembler line
} Asm_ItemKind;

// One node in the ordered instruction list.
typedef struct Asm_Item Asm_Item;
struct Asm_Item {
    Asm_Item     *ai_next;
    Asm_ItemKind  ai_kind;
    Asm_Op        ai_op;       // INSTR
    Asm_Operand   ai_dst;      // INSTR
    Asm_Operand   ai_src;      // INSTR
    const char   *ai_label;    // LABEL / GLOBL
    const char   *ai_text;     // DIRECTIVE
    Asm_Section   ai_section;  // SECTION
    unsigned char *ai_bytes;   // BYTES
    int           ai_nbytes;   // BYTES
};

// Head and tail of the instruction list being built.
static Asm_Item *Asm_Head;
static Asm_Item *Asm_Tail;

// 64-bit register names, indexed by Asm_Reg.
static const char *Asm_Reg64Name[16] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
};

// 8-bit (low-byte) register names, indexed by Asm_Reg.
static const char *Asm_Reg8Name[16] = {
    "al",  "cl",  "dl",   "bl",   "spl",  "bpl",  "sil",  "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
};

// Mnemonics, indexed by Asm_Op.
static const char *Asm_OpName[] = {
    [ASM_OP_MOV] = "mov",  [ASM_OP_LEA] = "lea",  [ASM_OP_PUSH] = "push",
    [ASM_OP_POP] = "pop",  [ASM_OP_ADD] = "add",  [ASM_OP_SUB] = "sub",
    [ASM_OP_IMUL] = "imul", [ASM_OP_IDIV] = "idiv", [ASM_OP_CQO] = "cqo",
    [ASM_OP_NEG] = "neg",  [ASM_OP_CMP] = "cmp",  [ASM_OP_SETE] = "sete",
    [ASM_OP_SETNE] = "setne", [ASM_OP_SETL] = "setl", [ASM_OP_SETLE] = "setle",
    [ASM_OP_MOVZB] = "movzb", [ASM_OP_JMP] = "jmp", [ASM_OP_JE] = "je",
    [ASM_OP_JNE] = "jne",  [ASM_OP_CALL] = "call", [ASM_OP_RET] = "ret",
    [ASM_OP_SYSCALL] = "syscall"
};

// Makes a 64-bit register operand (%rax .. %r15).
static Asm_Operand Asm_Reg64(Asm_Reg reg)
{
    return (Asm_Operand) {
        .ao_kind = ASM_OPERAND_REG,
        .ao_reg = reg,
        .ao_width = 64
    };
}

// Makes an 8-bit low-byte register operand (%al .. %r15b).
static Asm_Operand Asm_Reg8(Asm_Reg reg)
{
    return (Asm_Operand) {
        .ao_kind = ASM_OPERAND_REG,
        .ao_reg = reg,
        .ao_width = 8
    };
}

// Makes an immediate operand ($val).
static Asm_Operand Asm_Imm(long val)
{
    return (Asm_Operand) {
        .ao_kind = ASM_OPERAND_IMM,
        .ao_imm = val
    };
}

// Makes a base-plus-displacement memory operand (disp(%base)).
static Asm_Operand Asm_Mem(Asm_Reg base, int disp)
{
    return (Asm_Operand) {
        .ao_kind = ASM_OPERAND_MEM,
        .ao_reg = base,
        .ao_disp = disp
    };
}

// Makes a RIP-relative operand naming a label (label(%rip)).
static Asm_Operand Asm_Rip(const char *label)
{
    return (Asm_Operand) {
        .ao_kind = ASM_OPERAND_RIP,
        .ao_label = label
    };
}

// Makes a jump or call target operand.
static Asm_Operand Asm_Target(const char *label)
{
    return (Asm_Operand) {
        .ao_kind = ASM_OPERAND_LABEL,
        .ao_label = label
    };
}

// Appends a fresh item of the given kind to the list and returns it.
static Asm_Item *Asm_New(Asm_ItemKind kind)
{
    Asm_Item *item = calloc(1, sizeof *item);
    item->ai_kind = kind;
    if (Asm_Tail) {
        Asm_Tail->ai_next = item;
    } else {
        Asm_Head = item;
    }
    Asm_Tail = item;
    return item;
}

// Clears the instruction list before a fresh translation unit.
void Asm_Reset(void)
{
    Asm_Item *item = Asm_Head;
    while (item) {
        Asm_Item *next = item->ai_next;
        free((char *) item->ai_label);
        free((char *) item->ai_text);
        free((char *) item->ai_dst.ao_label);
        free((char *) item->ai_src.ao_label);
        free(item->ai_bytes);
        free(item);
        item = next;
    }
    Asm_Head = NULL;
    Asm_Tail = NULL;
}

// Emits a label definition, named by a printf-style format.
void Asm_EmitLabel(const char *name, ...)
{
    va_list ap;
    va_start(ap, name);
    Asm_Item *item = Asm_New(ASM_ITEM_LABEL);
    item->ai_label = Str_VFormat(name, ap);
    va_end(ap);
}

// Emits a switch to the given output section.
void Asm_EmitSection(Asm_Section sec)
{
    Asm_Item *item = Asm_New(ASM_ITEM_SECTION);
    item->ai_section = sec;
}

// Marks a symbol global, named by a printf-style format.
void Asm_EmitGlobl(const char *name, ...)
{
    va_list ap;
    va_start(ap, name);
    Asm_Item *item = Asm_New(ASM_ITEM_GLOBL);
    item->ai_label = Str_VFormat(name, ap);
    va_end(ap);
}

// Emits a run of raw data bytes.
void Asm_EmitBytes(const void *data, int len)
{
    Asm_Item *item = Asm_New(ASM_ITEM_BYTES);
    item->ai_bytes = malloc(len);
    memcpy(item->ai_bytes, data, len);
    item->ai_nbytes = len;
}

// Emits a raw assembler line from a printf-style format, written with indent.
void Asm_EmitDirective(const char *text, ...)
{
    va_list ap;
    va_start(ap, text);
    Asm_Item *item = Asm_New(ASM_ITEM_DIRECTIVE);
    item->ai_text = Str_VFormat(text, ap);
    va_end(ap);
}

// Emits `add %src, %dst`.
void Asm_EmitAdd(Asm_Reg src, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_ADD;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Reg64(src);
}

// Emits `sub %src, %dst`.
void Asm_EmitSub(Asm_Reg src, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_SUB;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Reg64(src);
}

// Emits `imul %src, %dst`.
void Asm_EmitImul(Asm_Reg src, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_IMUL;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Reg64(src);
}

// Emits `cmp %src, %dst`.
void Asm_EmitCmp(Asm_Reg src, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_CMP;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Reg64(src);
}

// Emits `mov %src, %dst` between two registers.
void Asm_EmitMovRR(Asm_Reg src, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_MOV;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Reg64(src);
}

// Emits `idiv %reg`.
void Asm_EmitIdiv(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_IDIV;
    item->ai_dst = Asm_Reg64(reg);
}

// Emits `neg %reg`.
void Asm_EmitNeg(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_NEG;
    item->ai_dst = Asm_Reg64(reg);
}

// Emits `cqo` (sign-extend %rax into %rdx:%rax).
void Asm_EmitCqo(void)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op = ASM_OP_CQO;
}

// Emits `sete %reg`.
void Asm_EmitSete(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_SETE;
    item->ai_dst = Asm_Reg8(reg);
}

// Emits `setne %reg`.
void Asm_EmitSetne(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_SETNE;
    item->ai_dst = Asm_Reg8(reg);
}

// Emits `setl %reg`.
void Asm_EmitSetl(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_SETL;
    item->ai_dst = Asm_Reg8(reg);
}

// Emits `setle %reg`.
void Asm_EmitSetle(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_SETLE;
    item->ai_dst = Asm_Reg8(reg);
}

// Emits `movzb %src, %dst` (zero-extend a byte into a 64-bit register).
void Asm_EmitMovzb(Asm_Reg src, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_MOVZB;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Reg8(src);
}

// Emits `cmp $imm, %dst`.
void Asm_EmitCmpImm(long imm, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_CMP;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Imm(imm);
}

// Emits `mov $imm, %dst` into a 64-bit register.
void Asm_EmitMovImm(long imm, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_MOV;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Imm(imm);
}

// Emits `mov $imm, %dst` into an 8-bit register.
void Asm_EmitMovImm8(long imm, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_MOV;
    item->ai_dst = Asm_Reg8(dst);
    item->ai_src = Asm_Imm(imm);
}

// Emits `add $imm, %dst`.
void Asm_EmitAddImm(long imm, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_ADD;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Imm(imm);
}

// Emits `sub $imm, %dst`.
void Asm_EmitSubImm(long imm, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_SUB;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Imm(imm);
}

// Emits `mov disp(%base), %dst` (load from memory).
void Asm_EmitMovLoad(Asm_Reg base, int disp, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_MOV;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Mem(base, disp);
}

// Emits `mov %src, disp(%base)` (store to memory).
void Asm_EmitMovStore(Asm_Reg src, Asm_Reg base, int disp)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_MOV;
    item->ai_dst = Asm_Mem(base, disp);
    item->ai_src = Asm_Reg64(src);
}

// Emits `lea disp(%base), %dst`.
void Asm_EmitLea(Asm_Reg base, int disp, Asm_Reg dst)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_LEA;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Mem(base, disp);
}

// Emits `lea label(%rip), %dst`, with label from a printf-style format.
void Asm_EmitLeaRip(Asm_Reg dst, const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_LEA;
    item->ai_dst = Asm_Reg64(dst);
    item->ai_src = Asm_Rip(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `push %reg`.
void Asm_EmitPush(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_PUSH;
    item->ai_dst = Asm_Reg64(reg);
}

// Emits `pop %reg`.
void Asm_EmitPop(Asm_Reg reg)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_POP;
    item->ai_dst = Asm_Reg64(reg);
}

// Emits `jmp label`, with label from a printf-style format.
void Asm_EmitJmp(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_JMP;
    item->ai_dst = Asm_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `je label`, with label from a printf-style format.
void Asm_EmitJe(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_JE;
    item->ai_dst = Asm_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `jne label`, with label from a printf-style format.
void Asm_EmitJne(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_JNE;
    item->ai_dst = Asm_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `call label`, with label from a printf-style format.
void Asm_EmitCall(const char *label, ...)
{
    va_list ap;
    va_start(ap, label);
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op  = ASM_OP_CALL;
    item->ai_dst = Asm_Target(Str_VFormat(label, ap));
    va_end(ap);
}

// Emits `ret`.
void Asm_EmitRet(void)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op = ASM_OP_RET;
}

// Emits `syscall`.
void Asm_EmitSyscall(void)
{
    Asm_Item *item = Asm_New(ASM_ITEM_INSTR);
    item->ai_op = ASM_OP_SYSCALL;
}

// Writes one operand in AT&T syntax.
static void Asm_PrintOperand(FILE *out, const Asm_Operand *op)
{
    switch (op->ao_kind) {
        case ASM_OPERAND_REG: {
            const char *name = op->ao_width == 8 ? Asm_Reg8Name[op->ao_reg]
                                                 : Asm_Reg64Name[op->ao_reg];
            fprintf(out, "%%%s", name);
        } break;
        case ASM_OPERAND_IMM: {
            fprintf(out, "$%ld", op->ao_imm);
        } break;
        case ASM_OPERAND_MEM: {
            if (op->ao_disp) {
                fprintf(out, "%d(%%%s)", op->ao_disp, Asm_Reg64Name[op->ao_reg]);
            } else {
                fprintf(out, "(%%%s)", Asm_Reg64Name[op->ao_reg]);
            }
        } break;
        case ASM_OPERAND_RIP: {
            fprintf(out, "%s(%%rip)", op->ao_label);
        } break;
        case ASM_OPERAND_LABEL: {
            fprintf(out, "%s", op->ao_label);
        } break;
        case ASM_OPERAND_NONE: {
            // nothing to print
        } break;
    }
}

// Writes one instruction: mnemonic plus operands in AT&T order.
static void Asm_PrintInstr(FILE *out, const Asm_Item *item)
{
    fprintf(out, "  %s", Asm_OpName[item->ai_op]);

    int have_src = item->ai_src.ao_kind != ASM_OPERAND_NONE;
    int have_dst = item->ai_dst.ao_kind != ASM_OPERAND_NONE;

    if (have_src) {
        fputc(' ', out);
        Asm_PrintOperand(out, &item->ai_src);
    }
    if (have_dst) {
        fputs(have_src ? ", " : " ", out);
        Asm_PrintOperand(out, &item->ai_dst);
    }
    fputc('\n', out);
}

// Walks the instruction list and writes AT&T-syntax assembly to out.
void Asm_PrintText(FILE *out)
{
    for (Asm_Item *item = Asm_Head; item; item = item->ai_next) {
        switch (item->ai_kind) {
            case ASM_ITEM_INSTR: {
                Asm_PrintInstr(out, item);
            } break;
            case ASM_ITEM_LABEL: {
                fprintf(out, "%s:\n", item->ai_label);
            } break;
            case ASM_ITEM_GLOBL: {
                fprintf(out, "  .globl %s\n", item->ai_label);
            } break;
            case ASM_ITEM_SECTION: {
                fprintf(out, "%s\n", item->ai_section == ASM_SECTION_TEXT
                                         ? "  .text" : "  .section .rodata");
            } break;
            case ASM_ITEM_BYTES: {
                for (int i = 0; i < item->ai_nbytes; i++) {
                    fprintf(out, "  .byte %d\n", item->ai_bytes[i]);
                }
            } break;
            case ASM_ITEM_DIRECTIVE: {
                fprintf(out, "  %s\n", item->ai_text);
            } break;
        }
    }
}
