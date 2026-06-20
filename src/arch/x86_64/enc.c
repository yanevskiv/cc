#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/elf.h"
#include "arch/x86_64/asm.h"
#include "arch/x86_64/enc.h"
#include "arch/x86_64/rel.h"

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

// A rel32 fixup targets its exact site, so it carries an addend of -4.
#define ENC_X86_64_REL32_ADDEND (-4)

// The object being encoded.
static Elf *Enc_x86_64_Out;

// The section that following writes append to.
static Elf_Sec *Enc_x86_64_Cur;

// Labels collected while encoding.
static Enc_x86_64_Label *Enc_x86_64_Labels;
static size_t Enc_x86_64_NumLabels;
static size_t Enc_x86_64_CapLabels;

// Names declared via .globl while encoding.
static const char **Enc_x86_64_Globls;
static size_t Enc_x86_64_NumGlobls;
static size_t Enc_x86_64_CapGlobls;

// Pending rel32 fixups collected while encoding.
static Enc_x86_64_Fix *Enc_x86_64_Fixes;
static size_t Enc_x86_64_NumFixes;
static size_t Enc_x86_64_CapFixes;

// Appends one byte to the current section.
void Enc_x86_64_Emit8(int byte)
{
    Elf_BufByte(Elf_SectionData(Enc_x86_64_Cur), (uint8_t) byte);
}

// Appends a little-endian 32-bit value to the current section.
void Enc_x86_64_Emit32(uint32_t val)
{
    Elf_BufU32(Elf_SectionData(Enc_x86_64_Cur), val);
}

// Appends a little-endian 64-bit value to the current section.
void Enc_x86_64_Emit64(uint64_t val)
{
    Elf_BufU64(Elf_SectionData(Enc_x86_64_Cur), val);
}

// Appends a run of raw bytes to the current section.
void Enc_x86_64_EmitRaw(const void *data, int len)
{
    Elf_BufData(Elf_SectionData(Enc_x86_64_Cur), data, (size_t) len);
}

// Records a label at the current position in the current section.
void Enc_x86_64_RecordLabel(const char *name)
{
    if (Enc_x86_64_NumLabels == Enc_x86_64_CapLabels) {
        Enc_x86_64_CapLabels = Enc_x86_64_CapLabels ? Enc_x86_64_CapLabels * 2 : 64;
        Enc_x86_64_Labels = realloc(Enc_x86_64_Labels,
                                    Enc_x86_64_CapLabels * sizeof *Enc_x86_64_Labels);
    }
    Enc_x86_64_Labels[Enc_x86_64_NumLabels++] = (Enc_x86_64_Label) {
        .al_name = name,
        .al_sec  = Enc_x86_64_Cur,
        .al_off  = Elf_SectionData(Enc_x86_64_Cur)->eb_len
    };
}

// Records that name appeared in a .globl directive.
void Enc_x86_64_RecordGlobl(const char *name)
{
    if (Enc_x86_64_NumGlobls == Enc_x86_64_CapGlobls) {
        Enc_x86_64_CapGlobls = Enc_x86_64_CapGlobls ? Enc_x86_64_CapGlobls * 2 : 64;
        Enc_x86_64_Globls = realloc(Enc_x86_64_Globls,
                                    Enc_x86_64_CapGlobls * sizeof *Enc_x86_64_Globls);
    }
    Enc_x86_64_Globls[Enc_x86_64_NumGlobls++] = name;
}

// Records a rel32 fixup at the current site; the caller writes the placeholder bytes.
void Enc_x86_64_Fixup(const char *name, uint32_t type)
{
    if (Enc_x86_64_NumFixes == Enc_x86_64_CapFixes) {
        Enc_x86_64_CapFixes = Enc_x86_64_CapFixes ? Enc_x86_64_CapFixes * 2 : 64;
        Enc_x86_64_Fixes = realloc(Enc_x86_64_Fixes,
                                   Enc_x86_64_CapFixes * sizeof *Enc_x86_64_Fixes);
    }
    Enc_x86_64_Fixes[Enc_x86_64_NumFixes++] = (Enc_x86_64_Fix) {
        .af_sec  = Enc_x86_64_Cur,
        .af_off  = Elf_SectionData(Enc_x86_64_Cur)->eb_len,
        .af_name = name,
        .af_type = type
    };
}

// Returns the high bit of a register number, extending ModRM.reg or .rm.
int Enc_x86_64_RegHigh(Asm_x86_64_Reg reg)
{
    return reg >> 3;
}

// Emits a REX.W prefix with the given reg- and rm-field extension bits.
void Enc_x86_64_RexW(int regHigh, int rmHigh)
{
    Enc_x86_64_Emit8(REX_BASE | REX_W | (regHigh ? REX_R : 0) | (rmHigh ? REX_B : 0));
}

// Emits a register-direct ModRM byte pairing reg with rm.
void Enc_x86_64_ModRR(int reg, Asm_x86_64_Reg rm)
{
    Enc_x86_64_Emit8(MODRM_DIRECT | ((reg & 7) << 3) | (rm & 7));
}

// Emits the ModRM, optional SIB and displacement for disp(%base).
void Enc_x86_64_Mem(int reg, Asm_x86_64_Reg base, int disp)
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

    Enc_x86_64_Emit8((mod << 6) | ((reg & 7) << 3) | rm);
    if (rm == (ASM_X86_64_REG_RSP & 7)) {
        Enc_x86_64_Emit8(SIB_BASE_RSP);
    }
    if (mod == 1) {
        Enc_x86_64_Emit8(disp & 0xFF);
    } else if (mod == 2) {
        Enc_x86_64_Emit32((unsigned int) disp);
    }
}

// Emits `<opcode> %src, %dst` for a register-to-register operation.
void Enc_x86_64_RR(int opcode, Asm_x86_64_Reg src, Asm_x86_64_Reg dst)
{
    Enc_x86_64_RexW(Enc_x86_64_RegHigh(src), Enc_x86_64_RegHigh(dst));
    Enc_x86_64_Emit8(opcode);
    Enc_x86_64_ModRR(src, dst);
}

// Emits a 0x81-group `<grp> $imm, %dst` with a 32-bit immediate.
void Enc_x86_64_GrpImm(int grp, long imm, Asm_x86_64_Reg dst)
{
    Enc_x86_64_RexW(0, Enc_x86_64_RegHigh(dst));
    Enc_x86_64_Emit8(0x81);
    Enc_x86_64_ModRR(grp, dst);
    Enc_x86_64_Emit32((unsigned int) imm);
}

// Emits `mov $imm, %dst` into a 64-bit register.
void Enc_x86_64_MovImm(long imm, Asm_x86_64_Reg dst)
{
    if (imm >= INT32_MIN && imm <= INT32_MAX) {
        Enc_x86_64_RexW(0, Enc_x86_64_RegHigh(dst));
        Enc_x86_64_Emit8(0xC7);
        Enc_x86_64_ModRR(0, dst);
        Enc_x86_64_Emit32((unsigned int) imm);
    } else {
        Enc_x86_64_RexW(0, Enc_x86_64_RegHigh(dst));
        Enc_x86_64_Emit8(0xB8 + (dst & 7));
        Enc_x86_64_Emit64((unsigned long long) imm);
    }
}

// Emits `mov $imm, %dst` into an 8-bit register.
void Enc_x86_64_MovImm8(long imm, Asm_x86_64_Reg dst)
{
    if (dst >= ASM_X86_64_REG_R8) {
        Enc_x86_64_Emit8(REX_BASE | REX_B);
    } else if (dst >= ASM_X86_64_REG_RSP) {
        Enc_x86_64_Emit8(REX_BASE);
    }
    Enc_x86_64_Emit8(0xB0 + (dst & 7));
    Enc_x86_64_Emit8(imm & 0xFF);
}

// Emits `<opcode> disp(%base), %reg` (or the reverse for a store).
void Enc_x86_64_MemForm(int opcode, Asm_x86_64_Reg reg, Asm_x86_64_Reg base, int disp)
{
    Enc_x86_64_RexW(Enc_x86_64_RegHigh(reg), Enc_x86_64_RegHigh(base));
    Enc_x86_64_Emit8(opcode);
    Enc_x86_64_Mem(reg, base, disp);
}

// Emits `lea label(%rip), %dst` with a rel32 fixup to label.
void Enc_x86_64_LeaRip(Asm_x86_64_Reg dst, const char *label)
{
    Enc_x86_64_RexW(Enc_x86_64_RegHigh(dst), 0);
    Enc_x86_64_Emit8(0x8D);
    Enc_x86_64_Emit8(((dst & 7) << 3) | 5);
    Enc_x86_64_Fixup(label, R_X86_64_PC32);
    Enc_x86_64_Emit32(0);
}

// Emits a 0xF7-group unary instruction `<grp> %reg`.
void Enc_x86_64_GrpUnary(int grp, Asm_x86_64_Reg reg)
{
    Enc_x86_64_RexW(0, Enc_x86_64_RegHigh(reg));
    Enc_x86_64_Emit8(0xF7);
    Enc_x86_64_ModRR(grp, reg);
}

// Emits a `setcc %reg` byte-setting instruction.
void Enc_x86_64_Setcc(int opcode, Asm_x86_64_Reg reg)
{
    if (reg >= ASM_X86_64_REG_R8) {
        Enc_x86_64_Emit8(REX_BASE | REX_B);
    } else if (reg >= ASM_X86_64_REG_RSP) {
        Enc_x86_64_Emit8(REX_BASE);
    }
    Enc_x86_64_Emit8(0x0F);
    Enc_x86_64_Emit8(opcode);
    Enc_x86_64_ModRR(0, reg);
}

// Emits a rel32 control-transfer instruction with a fixup to its target.
void Enc_x86_64_Branch(const Asm_x86_64_Item *item)
{
    switch (item->ai_op) {
        case ASM_X86_64_OP_JMP:  Enc_x86_64_Emit8(0xE9);                      break;
        case ASM_X86_64_OP_CALL: Enc_x86_64_Emit8(0xE8);                      break;
        case ASM_X86_64_OP_JE:   Enc_x86_64_Emit8(0x0F); Enc_x86_64_Emit8(0x84); break;
        case ASM_X86_64_OP_JNE:  Enc_x86_64_Emit8(0x0F); Enc_x86_64_Emit8(0x85); break;
        default: break;
    }
    // A call may bind through the PLT; jmp/jcc are plain PC-relative.
    uint32_t type = item->ai_op == ASM_X86_64_OP_CALL ? R_X86_64_PLT32
                                                      : R_X86_64_PC32;
    Enc_x86_64_Fixup(item->ai_dst.ao_label, type);
    Enc_x86_64_Emit32(0);
}

// Emits a `mov` in whichever of its forms the operands select.
void Enc_x86_64_Mov(const Asm_x86_64_Item *item)
{
    Asm_x86_64_Reg dst = item->ai_dst.ao_reg;
    Asm_x86_64_Reg src = item->ai_src.ao_reg;

    switch (item->ai_src.ao_kind) {
        case ASM_X86_64_OPERAND_IMM: {
            if (item->ai_dst.ao_width == 8) {
                Enc_x86_64_MovImm8(item->ai_src.ao_imm, dst);
            } else {
                Enc_x86_64_MovImm(item->ai_src.ao_imm, dst);
            }
        } break;
        case ASM_X86_64_OPERAND_MEM: {
            Enc_x86_64_MemForm(0x8B, dst, item->ai_src.ao_reg, item->ai_src.ao_disp);
        } break;
        case ASM_X86_64_OPERAND_REG: {
            if (item->ai_dst.ao_kind == ASM_X86_64_OPERAND_MEM) {
                Enc_x86_64_MemForm(0x89, src, item->ai_dst.ao_reg, item->ai_dst.ao_disp);
            } else {
                Enc_x86_64_RR(0x89, src, dst);
            }
        } break;
        default: break;
    }
}

// Encodes one instruction item into the current section.
void Enc_x86_64_Instr(const Asm_x86_64_Item *item)
{
    Asm_x86_64_Reg dst = item->ai_dst.ao_reg;
    Asm_x86_64_Reg src = item->ai_src.ao_reg;
    int     imm = item->ai_src.ao_kind == ASM_X86_64_OPERAND_IMM;

    switch (item->ai_op) {
        case ASM_X86_64_OP_ADD: {
            if (imm) {
                Enc_x86_64_GrpImm(GRP_ADD, item->ai_src.ao_imm, dst);
            } else {
                Enc_x86_64_RR(0x01, src, dst);
            }
        } break;
        case ASM_X86_64_OP_SUB: {
            if (imm) {
                Enc_x86_64_GrpImm(GRP_SUB, item->ai_src.ao_imm, dst);
            } else {
                Enc_x86_64_RR(0x29, src, dst);
            }
        } break;
        case ASM_X86_64_OP_CMP: {
            if (imm) {
                Enc_x86_64_GrpImm(GRP_CMP, item->ai_src.ao_imm, dst);
            } else {
                Enc_x86_64_RR(0x39, src, dst);
            }
        } break;
        case ASM_X86_64_OP_IMUL: {
            Enc_x86_64_RexW(Enc_x86_64_RegHigh(dst), Enc_x86_64_RegHigh(src));
            Enc_x86_64_Emit8(0x0F);
            Enc_x86_64_Emit8(0xAF);
            Enc_x86_64_ModRR(dst, src);
        } break;
        case ASM_X86_64_OP_MOV: {
            Enc_x86_64_Mov(item);
        } break;
        case ASM_X86_64_OP_LEA: {
            if (item->ai_src.ao_kind == ASM_X86_64_OPERAND_RIP) {
                Enc_x86_64_LeaRip(dst, item->ai_src.ao_label);
            } else {
                Enc_x86_64_MemForm(0x8D, dst, item->ai_src.ao_reg, item->ai_src.ao_disp);
            }
        } break;
        case ASM_X86_64_OP_IDIV: {
            Enc_x86_64_GrpUnary(GRP_IDIV, dst);
        } break;
        case ASM_X86_64_OP_NEG: {
            Enc_x86_64_GrpUnary(GRP_NEG, dst);
        } break;
        case ASM_X86_64_OP_CQO: {
            Enc_x86_64_Emit8(REX_BASE | REX_W);
            Enc_x86_64_Emit8(0x99);
        } break;
        case ASM_X86_64_OP_SETE:  { Enc_x86_64_Setcc(0x94, dst); } break;
        case ASM_X86_64_OP_SETNE: { Enc_x86_64_Setcc(0x95, dst); } break;
        case ASM_X86_64_OP_SETL:  { Enc_x86_64_Setcc(0x9C, dst); } break;
        case ASM_X86_64_OP_SETLE: { Enc_x86_64_Setcc(0x9E, dst); } break;
        case ASM_X86_64_OP_MOVZB: {
            Enc_x86_64_RexW(Enc_x86_64_RegHigh(dst), Enc_x86_64_RegHigh(src));
            Enc_x86_64_Emit8(0x0F);
            Enc_x86_64_Emit8(0xB6);
            Enc_x86_64_ModRR(dst, src);
        } break;
        case ASM_X86_64_OP_PUSH: {
            if (dst >= ASM_X86_64_REG_R8) {
                Enc_x86_64_Emit8(REX_BASE | REX_B);
            }
            Enc_x86_64_Emit8(0x50 + (dst & 7));
        } break;
        case ASM_X86_64_OP_POP: {
            if (dst >= ASM_X86_64_REG_R8) {
                Enc_x86_64_Emit8(REX_BASE | REX_B);
            }
            Enc_x86_64_Emit8(0x58 + (dst & 7));
        } break;
        case ASM_X86_64_OP_JMP:
        case ASM_X86_64_OP_JE:
        case ASM_X86_64_OP_JNE:
        case ASM_X86_64_OP_CALL: {
            Enc_x86_64_Branch(item);
        } break;
        case ASM_X86_64_OP_RET: {
            Enc_x86_64_Emit8(0xC3);
        } break;
        case ASM_X86_64_OP_SYSCALL: {
            Enc_x86_64_Emit8(0x0F);
            Enc_x86_64_Emit8(0x05);
        } break;
    }
}

// True if name was declared via .globl.
int Enc_x86_64_IsGlobl(const char *name)
{
    for (size_t i = 0; i < Enc_x86_64_NumGlobls; i++) {
        if (strcmp(Enc_x86_64_Globls[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Switches the current section to the named one, creating it on first use.
void Enc_x86_64_SelectSection(const char *name, uint32_t type, uint64_t flags)
{
    Enc_x86_64_Cur = Elf_SectionGet(Enc_x86_64_Out, name, type, flags);
}

// Creates a symbol for every label, then an undefined symbol for each unresolved target.
void Enc_x86_64_BuildSymbols(void)
{
    for (size_t i = 0; i < Enc_x86_64_NumLabels; i++) {
        Enc_x86_64_Label *l = &Enc_x86_64_Labels[i];
        int     global = Enc_x86_64_IsGlobl(l->al_name);
        uint8_t bind   = global ? ELF_BIND_GLOBAL : ELF_BIND_LOCAL;
        uint8_t type   = ELF_TYPE_NOTYPE;
        if (global) {
            type = (l->al_sec->sec_flags & ELF_SHF_EXECINSTR) ? ELF_TYPE_FUNC
                                                              : ELF_TYPE_OBJECT;
        }
        Elf_SymbolAdd(Enc_x86_64_Out, l->al_name, l->al_sec, l->al_off, bind, type);
    }
    for (size_t i = 0; i < Enc_x86_64_NumFixes; i++) {
        const char *name = Enc_x86_64_Fixes[i].af_name;
        if (! Elf_SymbolFind(Enc_x86_64_Out, name)) {
            Elf_SymbolAdd(Enc_x86_64_Out, name, NULL, 0, ELF_BIND_GLOBAL, ELF_TYPE_NOTYPE);
        }
    }
}

// Turns each collected fixup into a relocation against its resolved symbol.
void Enc_x86_64_BuildRelocs(void)
{
    for (size_t i = 0; i < Enc_x86_64_NumFixes; i++) {
        Enc_x86_64_Fix *f   = &Enc_x86_64_Fixes[i];
        Elf_Sym        *sym = Elf_SymbolFind(Enc_x86_64_Out, f->af_name);
        Elf_RelaAdd(f->af_sec, f->af_off, sym, f->af_type, ENC_X86_64_REL32_ADDEND);
    }
}

// Walks the instruction list and builds a relocatable ELF object from it.
Elf *Enc_x86_64_Object(void)
{
    Elf *elf = Elf_New(ELF_ET_REL, ELF_EM_X86_64);
    Enc_x86_64_Out = elf;
    Enc_x86_64_NumLabels = 0;
    Enc_x86_64_NumGlobls = 0;
    Enc_x86_64_NumFixes  = 0;
    Enc_x86_64_SelectSection(".text", ELF_SHT_PROGBITS, ELF_SHF_ALLOC | ELF_SHF_EXECINSTR);

    for (Asm_x86_64_Item *item = Asm_x86_64_Items(); item; item = item->ai_next) {
        switch (item->ai_kind) {
            case ASM_X86_64_ITEM_INSTR: {
                Enc_x86_64_Instr(item);
            } break;
            case ASM_X86_64_ITEM_LABEL: {
                Enc_x86_64_RecordLabel(item->ai_label);
            } break;
            case ASM_X86_64_ITEM_SECTION: {
                Enc_x86_64_SelectSection(item->ai_secname, item->ai_sectype, item->ai_secflags);
            } break;
            case ASM_X86_64_ITEM_BYTES: {
                Enc_x86_64_EmitRaw(item->ai_bytes, item->ai_nbytes);
            } break;
            case ASM_X86_64_ITEM_GLOBL: {
                Enc_x86_64_RecordGlobl(item->ai_label);
            } break;
            case ASM_X86_64_ITEM_DIRECTIVE: {
                // raw assembler text, not represented in the encoded image
            } break;
        }
    }

    Enc_x86_64_BuildSymbols();
    Enc_x86_64_BuildRelocs();
    return elf;
}
