#include <stdint.h>
#include "common.h"
#include "util/elf.h"
#include "arch/x86_64/rel.h"

// Virtual address a symbol resolves to: its section's load address plus its offset.
uint64_t Rel_x86_64_SymbolAddr(const Elf_Sym *sym)
{
    if (sym->sym_sec) {
        return sym->sym_sec->sec_addr + sym->sym_value;
    }
    return sym->sym_value;
}

// Patches width little-endian bytes at a section offset with value.
void Rel_x86_64_PatchLE(Elf_Sec *sec, uint64_t offset, uint64_t value, int width)
{
    uint8_t *at = Elf_BufAt(Elf_SectionData(sec), offset);
    for (int i = 0; i < width; i++) {
        at[i] = (value >> (8 * i)) & 0xFF;
    }
}

// Applies one relocation, computing S (symbol), A (addend) and P (patch site).
void Rel_x86_64_One(Elf_Sec *sec, const Elf_Rela *rel)
{
    uint64_t S = Rel_x86_64_SymbolAddr(rel->rel_sym);
    int64_t  A = rel->rel_addend;
    uint64_t P = sec->sec_addr + rel->rel_offset;

    switch (rel->rel_type) {
        case R_X86_64_PC32:
        case R_X86_64_PLT32: {
            Rel_x86_64_PatchLE(sec, rel->rel_offset, (uint32_t) (int32_t) (S + A - P), 4);
        } break;
        case R_X86_64_32:
        case R_X86_64_32S: {
            Rel_x86_64_PatchLE(sec, rel->rel_offset, (uint32_t) (S + A), 4);
        } break;
        case R_X86_64_64: {
            Rel_x86_64_PatchLE(sec, rel->rel_offset, S + A, 8);
        } break;
        default: {
            Show_Error("unsupported relocation type %u", rel->rel_type);
        }
    }
}

// Applies every relocation in a placed object, patching each section's bytes.
void Rel_x86_64_Apply(Elf *elf)
{
    for (size_t i = 0; i < Elf_SectionCount(elf); i++) {
        Elf_Sec *sec = Elf_SectionAt(elf, i);
        for (size_t r = 0; r < Elf_RelaCount(sec); r++) {
            Rel_x86_64_One(sec, Elf_RelaAt(sec, r));
        }
    }
}
