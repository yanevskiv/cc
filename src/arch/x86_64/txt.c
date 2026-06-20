#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "util/elf.h"
#include "util/str.h"
#include "arch/x86_64/asm.h"
#include "arch/x86_64/txt.h"

// 64-bit register names, indexed by Asm_x86_64_Reg.
static const char *Txt_x86_64_Reg64Name[16] = {
    "rax",
    "rcx",
    "rdx",
    "rbx",
    "rsp",
    "rbp",
    "rsi",
    "rdi",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15"
};

// 8-bit (low-byte) register names, indexed by Asm_x86_64_Reg.
static const char *Txt_x86_64_Reg8Name[16] = {
    "al",
    "cl",
    "dl",
    "bl",
    "spl",
    "bpl",
    "sil",
    "dil",
    "r8b",
    "r9b",
    "r10b",
    "r11b",
    "r12b",
    "r13b",
    "r14b",
    "r15b"
};

// Mnemonics, indexed by Asm_x86_64_Op.
static const char *Txt_x86_64_OpName[] = {
    [ASM_X86_64_OP_MOV]     = "mov",
    [ASM_X86_64_OP_LEA]     = "lea",
    [ASM_X86_64_OP_PUSH]    = "push",
    [ASM_X86_64_OP_POP]     = "pop",
    [ASM_X86_64_OP_ADD]     = "add",
    [ASM_X86_64_OP_SUB]     = "sub",
    [ASM_X86_64_OP_IMUL]    = "imul",
    [ASM_X86_64_OP_IDIV]    = "idiv",
    [ASM_X86_64_OP_CQO]     = "cqo",
    [ASM_X86_64_OP_NEG]     = "neg",
    [ASM_X86_64_OP_CMP]     = "cmp",
    [ASM_X86_64_OP_SETE]    = "sete",
    [ASM_X86_64_OP_SETNE]   = "setne",
    [ASM_X86_64_OP_SETL]    = "setl",
    [ASM_X86_64_OP_SETLE]   = "setle",
    [ASM_X86_64_OP_MOVZB]   = "movzb",
    [ASM_X86_64_OP_JMP]     = "jmp",
    [ASM_X86_64_OP_JE]      = "je",
    [ASM_X86_64_OP_JNE]     = "jne",
    [ASM_X86_64_OP_CALL]    = "call",
    [ASM_X86_64_OP_RET]     = "ret",
    [ASM_X86_64_OP_SYSCALL] = "syscall"
};

// Writes one operand in AT&T syntax.
void Txt_x86_64_Att_WriteOperand(FILE *out, const Asm_x86_64_Operand *op)
{
    switch (op->ao_kind) {
        case ASM_X86_64_OPERAND_REG: {
            const char *name = op->ao_width == 8 ? Txt_x86_64_Reg8Name[op->ao_reg]
                                                 : Txt_x86_64_Reg64Name[op->ao_reg];
            fprintf(out, "%%%s", name);
        } break;
        case ASM_X86_64_OPERAND_IMM: {
            fprintf(out, "$%ld", op->ao_imm);
        } break;
        case ASM_X86_64_OPERAND_MEM: {
            if (op->ao_disp) {
                fprintf(out, "%d(%%%s)", op->ao_disp, Txt_x86_64_Reg64Name[op->ao_reg]);
            } else {
                fprintf(out, "(%%%s)", Txt_x86_64_Reg64Name[op->ao_reg]);
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
void Txt_x86_64_Att_WriteInstr(FILE *out, const Asm_x86_64_Item *item)
{
    fprintf(out, "  %s", Txt_x86_64_OpName[item->ai_op]);

    int have_src = item->ai_src.ao_kind != ASM_X86_64_OPERAND_NONE;
    int have_dst = item->ai_dst.ao_kind != ASM_X86_64_OPERAND_NONE;

    if (have_src) {
        fputc(' ', out);
        Txt_x86_64_Att_WriteOperand(out, &item->ai_src);
    }
    if (have_dst) {
        fputs(have_src ? ", " : " ", out);
        Txt_x86_64_Att_WriteOperand(out, &item->ai_dst);
    }
    fputc('\n', out);
}

// Walks the instruction list and writes AT&T-syntax assembly to out.
void Txt_x86_64_Att_Write(FILE *out)
{
    for (Asm_x86_64_Item *item = Asm_x86_64_Items(); item; item = item->ai_next) {
        switch (item->ai_kind) {
            case ASM_X86_64_ITEM_INSTR: {
                Txt_x86_64_Att_WriteInstr(out, item);
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

// Returns the register index for an AT&T name like "rax"/"al", or -1; sets *width.
int Txt_x86_64_RegByName(const char *name, int *width)
{
    for (int i = 0; i < 16; i++) {
        if (strcmp(name, Txt_x86_64_Reg64Name[i]) == 0) {
            *width = 64;
            return i;
        }
        if (strcmp(name, Txt_x86_64_Reg8Name[i]) == 0) {
            *width = 8;
            return i;
        }
    }
    return -1;
}

// Returns the opcode for a mnemonic, or -1 if it names no instruction we encode.
int Txt_x86_64_OpByName(const char *name)
{
    int count = (int) (sizeof Txt_x86_64_OpName / sizeof Txt_x86_64_OpName[0]);
    for (int i = 0; i < count; i++) {
        if (Txt_x86_64_OpName[i] && strcmp(name, Txt_x86_64_OpName[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// Parses one AT&T operand into op; returns nonzero on success.
int Txt_x86_64_Att_ParseOperand(const char *text, Asm_x86_64_Operand *op)
{
    char *g[3];

    if (text[0] == '%' && Str_RegexExtract(text, "^%([A-Za-z][A-Za-z0-9]*)$", g, 1)) {
        int width;
        int reg = Txt_x86_64_RegByName(g[0], &width);
        free(g[0]);
        if (reg < 0) {
            return 0;
        }
        *op = width == 8 ? Asm_x86_64_Reg8(reg) : Asm_x86_64_Reg64(reg);
        return 1;
    }
    if (text[0] == '$' && Str_RegexExtract(text, "^[$](-?(0[xX][0-9A-Fa-f]+|[0-9]+))$", g, 1)) {
        long val = strtol(g[0], NULL, 0);
        free(g[0]);
        *op = Asm_x86_64_Imm(val);
        return 1;
    }
    if (Str_RegexExtract(text, "^([.A-Za-z0-9_$]+)[(]%rip[)]$", g, 1)) {
        *op = Asm_x86_64_Rip(strdup(g[0]));
        free(g[0]);
        return 1;
    }
    if (Str_RegexExtract(text, "^(-?(0[xX][0-9A-Fa-f]+|[0-9]+))?[(]%([A-Za-z][A-Za-z0-9]*)[)]$", g, 3)) {
        int width;
        int base = Txt_x86_64_RegByName(g[2], &width);
        int disp = g[0] ? (int) strtol(g[0], NULL, 0) : 0;
        free(g[0]);
        free(g[1]);
        free(g[2]);
        if (base < 0) {
            return 0;
        }
        *op = Asm_x86_64_Mem(base, disp);
        return 1;
    }
    if (Str_RegexMatch(text, "^[.A-Za-z0-9_$]+$")) {
        *op = Asm_x86_64_Target(strdup(text));
        return 1;
    }
    return 0;
}

// Emits a .byte/.word/.long/.quad list as width-byte little-endian values.
void Txt_x86_64_Att_EmitInts(const char *args, int width)
{
    Str_List parts = Str_Split(args, ",");
    for (int i = 0; i < parts.sl_count; i++) {
        char *text = Str_Trim(parts.sl_items[i]);
        if (! *text) {
            continue;
        }
        long val = strtol(text, NULL, 0);
        unsigned char bytes[8];
        for (int b = 0; b < width; b++) {
            bytes[b] = (val >> (8 * b)) & 0xFF;
        }
        Asm_x86_64_EmitBytes(bytes, width);
    }
    Str_ListFree(&parts);
}

// Emits the bytes of a quoted string, adding a NUL when terminate is set.
void Txt_x86_64_Att_EmitString(const char *args, int terminate)
{
    const char *p = strchr(args, '"');
    if (! p) {
        return;
    }
    p++;

    unsigned char *buf = malloc(strlen(p) + 2);
    size_t len = 0;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) {
            char esc = *p++;
            switch (esc) {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case 'r':  c = '\r'; break;
                case '0':  c = '\0'; break;
                case '\\': c = '\\'; break;
                case '"':  c = '"';  break;
                default:   c = esc;  break;
            }
        }
        buf[len++] = (unsigned char) c;
    }
    if (terminate) {
        buf[len++] = '\0';
    }
    Asm_x86_64_EmitBytes(buf, (int) len);
    free(buf);
}

// Parses one instruction line ("mnemonic [op[, op]]") into an instruction item.
void Txt_x86_64_Att_ParseInstr(const char *line)
{
    size_t mlen = strcspn(line, " \t");
    if (mlen == 0 || mlen >= 32) {
        Show_Error("as: bad mnemonic in '%s'", line);
    }
    char mnem[32];
    memcpy(mnem, line, mlen);
    mnem[mlen] = '\0';

    // Accept an AT&T size suffix (movq, pushq, movzbl) by retrying without it.
    int op = Txt_x86_64_OpByName(mnem);
    if (op < 0 && mlen >= 2 && strchr("bwlq", mnem[mlen - 1])) {
        mnem[mlen - 1] = '\0';
        op = Txt_x86_64_OpByName(mnem);
    }
    if (op < 0) {
        Show_Error("as: unknown mnemonic '%.*s'", (int) mlen, line);
    }

    Asm_x86_64_Operand ops[2];
    int nops = 0;
    const char *rest = line + mlen;
    while (*rest == ' ' || *rest == '\t') {
        rest++;
    }
    if (*rest) {
        Str_List parts = Str_Split(rest, ",");
        for (int i = 0; i < parts.sl_count; i++) {
            char *text = Str_Trim(parts.sl_items[i]);
            if (! *text) {
                continue;
            }
            if (nops >= 2) {
                Show_Error("as: too many operands in '%s'", line);
            }
            if (! Txt_x86_64_Att_ParseOperand(text, &ops[nops])) {
                Show_Error("as: bad operand '%s'", text);
            }
            nops++;
        }
        Str_ListFree(&parts);
    }

    Asm_x86_64_Item *item = Asm_x86_64_New(ASM_X86_64_ITEM_INSTR);
    item->ai_op = op;
    if (nops == 2) {
        item->ai_src = ops[0];
        item->ai_dst = ops[1];
    } else if (nops == 1) {
        item->ai_dst = ops[0];
    }
}

// Parses one directive line, lowering data directives to raw bytes.
void Txt_x86_64_Att_ParseDirective(const char *line)
{
    size_t nlen = strcspn(line, " \t");
    char name[32];
    if (nlen >= sizeof name) {
        nlen = sizeof name - 1;
    }
    memcpy(name, line, nlen);
    name[nlen] = '\0';

    const char *args = line + nlen;
    while (*args == ' ' || *args == '\t') {
        args++;
    }

    if (Str_Equals(name, ".text")) {
        Asm_x86_64_EmitSection(".text", ELF_SHT_PROGBITS, ELF_SHF_ALLOC | ELF_SHF_EXECINSTR);
    } else if (Str_Equals(name, ".data")) {
        Asm_x86_64_EmitSection(".data", ELF_SHT_PROGBITS, ELF_SHF_ALLOC | ELF_SHF_WRITE);
    } else if (Str_Equals(name, ".rodata")) {
        Asm_x86_64_EmitSection(".rodata", ELF_SHT_PROGBITS, ELF_SHF_ALLOC);
    } else if (Str_Equals(name, ".section")) {
        char    *secname = strndup(args, strcspn(args, " ,\t"));
        uint32_t type    = ELF_SHT_PROGBITS;
        uint64_t flags;
        const char *quote = strchr(args, '"');
        if (quote) {
            flags = 0;
            for (const char *c = quote + 1; *c && *c != '"'; c++) {
                if (*c == 'a') flags |= ELF_SHF_ALLOC;
                if (*c == 'w') flags |= ELF_SHF_WRITE;
                if (*c == 'x') flags |= ELF_SHF_EXECINSTR;
            }
            const char *at = strchr(args, '@');
            if (at && Str_StartsWith(at + 1, "nobits")) {
                type = ELF_SHT_NOBITS;
            }
        } else if (Str_Equals(secname, ".text")) {
            flags = ELF_SHF_ALLOC | ELF_SHF_EXECINSTR;
        } else if (Str_Equals(secname, ".data")) {
            flags = ELF_SHF_ALLOC | ELF_SHF_WRITE;
        } else {
            flags = ELF_SHF_ALLOC;
        }
        Asm_x86_64_EmitSection(secname, type, flags);
    } else if (Str_Equals(name, ".globl") || Str_Equals(name, ".global")) {
        char *sym = strndup(args, strcspn(args, " ,\t"));
        Asm_x86_64_EmitGlobl("%s", sym);
        free(sym);
    } else if (Str_Equals(name, ".byte")) {
        Txt_x86_64_Att_EmitInts(args, 1);
    } else if (Str_Equals(name, ".word") || Str_Equals(name, ".short") || Str_Equals(name, ".value")) {
        Txt_x86_64_Att_EmitInts(args, 2);
    } else if (Str_Equals(name, ".long") || Str_Equals(name, ".int")) {
        Txt_x86_64_Att_EmitInts(args, 4);
    } else if (Str_Equals(name, ".quad")) {
        Txt_x86_64_Att_EmitInts(args, 8);
    } else if (Str_Equals(name, ".string") || Str_Equals(name, ".asciz")) {
        Txt_x86_64_Att_EmitString(args, 1);
    } else if (Str_Equals(name, ".ascii")) {
        Txt_x86_64_Att_EmitString(args, 0);
    } else if (Str_Equals(name, ".skip") || Str_Equals(name, ".zero") || Str_Equals(name, ".space")) {
        long count = strtol(args, NULL, 0);
        unsigned char *zeros = calloc(count > 0 ? count : 1, 1);
        Asm_x86_64_EmitBytes(zeros, (int) count);
        free(zeros);
    } else {
        Asm_x86_64_EmitDirective("%s", line);
    }
}

// Parses one line: strips its comment, then dispatches by line shape.
void Txt_x86_64_Att_ParseLine(char *line)
{
    int inq = 0;
    for (char *c = line; *c; c++) {
        if (*c == '"') {
            inq = ! inq;
        } else if (*c == '#' && ! inq) {
            *c = '\0';
            break;
        }
    }

    char *text = Str_Trim(line);
    if (*text == '\0') {
        return;
    }

    char *g[2];
    if (Str_RegexExtract(text, "^([.A-Za-z_$][.A-Za-z0-9_$]*):[ \t]*(.*)$", g, 2)) {
        Asm_x86_64_EmitLabel("%s", g[0]);
        if (g[1] && *g[1]) {
            Txt_x86_64_Att_ParseLine(g[1]);
        }
        free(g[0]);
        free(g[1]);
    } else if (text[0] == '.') {
        Txt_x86_64_Att_ParseDirective(text);
    } else {
        Txt_x86_64_Att_ParseInstr(text);
    }
}

// Parses AT&T-syntax assembly text into the instruction list.
void Txt_x86_64_Att_Parse(const char *text)
{
    Asm_x86_64_Reset();
    Str_List lines = Str_Split(text, "\n");
    for (int i = 0; i < lines.sl_count; i++) {
        Txt_x86_64_Att_ParseLine(lines.sl_items[i]);
    }
    Str_ListFree(&lines);
}
