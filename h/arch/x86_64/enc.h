#ifndef ENC_X86_64_H
#define ENC_X86_64_H

#include <stdint.h>
#include "util/elf.h"
#include "arch/x86_64/asm.h"

// A label defined in the stream, awaiting its symbol-table entry.
typedef struct {
    const char *al_name;
    Elf_Sec    *al_sec;
    uint64_t    al_off;
} Enc_x86_64_Label;

// A pending rel32 fixup: a site in a section and the symbol name it targets.
typedef struct {
    Elf_Sec    *af_sec;
    uint64_t    af_off;
    const char *af_name;
    uint32_t    af_type;
} Enc_x86_64_Fix;

// Byte output
void Enc_x86_64_Emit8(int byte);
void Enc_x86_64_Emit32(uint32_t val);
void Enc_x86_64_Emit64(uint64_t val);
void Enc_x86_64_EmitRaw(const void *data, int len);

// Recording labels and fixups
void Enc_x86_64_RecordLabel(const char *name);
void Enc_x86_64_RecordGlobl(const char *name);
void Enc_x86_64_Fixup(const char *name, uint32_t type);

// REX and ModRM encoding
int  Enc_x86_64_RegHigh(Asm_x86_64_Reg reg);
void Enc_x86_64_RexW(int regHigh, int rmHigh);
void Enc_x86_64_ModRR(int reg, Asm_x86_64_Reg rm);
void Enc_x86_64_Mem(int reg, Asm_x86_64_Reg base, int disp);

// Instruction encoding
void Enc_x86_64_RR(int opcode, Asm_x86_64_Reg src, Asm_x86_64_Reg dst);
void Enc_x86_64_GrpImm(int grp, long imm, Asm_x86_64_Reg dst);
void Enc_x86_64_MovImm(long imm, Asm_x86_64_Reg dst);
void Enc_x86_64_MovImm8(long imm, Asm_x86_64_Reg dst);
void Enc_x86_64_MemForm(int opcode, Asm_x86_64_Reg reg, Asm_x86_64_Reg base, int disp);
void Enc_x86_64_LeaRip(Asm_x86_64_Reg dst, const char *label);
void Enc_x86_64_GrpUnary(int grp, Asm_x86_64_Reg reg);
void Enc_x86_64_Setcc(int opcode, Asm_x86_64_Reg reg);
void Enc_x86_64_Branch(const Asm_x86_64_Item *item);
void Enc_x86_64_Mov(const Asm_x86_64_Item *item);
void Enc_x86_64_Instr(const Asm_x86_64_Item *item);

// Symbols, sections and relocations
int  Enc_x86_64_IsGlobl(const char *name);
void Enc_x86_64_SelectSection(const char *name, uint32_t type, uint64_t flags);
void Enc_x86_64_BuildSymbols(void);
void Enc_x86_64_BuildRelocs(void);

// Encoding the instruction list to a relocatable ELF object
Elf *Enc_x86_64_Object(void);

#endif // ENC_X86_64_H
