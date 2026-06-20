#ifndef REL_X86_64_H
#define REL_X86_64_H

#include <stdint.h>
#include "util/elf.h"

// x86-64 relocation type numbers, stored opaquely as an Elf_Rela's rel_type.
#define R_X86_64_64    1
#define R_X86_64_PC32  2
#define R_X86_64_PLT32 4
#define R_X86_64_32    10
#define R_X86_64_32S   11

// Addresses and byte patching
uint64_t Rel_x86_64_SymbolAddr(const Elf_Sym *sym);
void     Rel_x86_64_PatchLE(Elf_Sec *sec, uint64_t offset, uint64_t value, int width);

// Applying relocations
void Rel_x86_64_One(Elf_Sec *sec, const Elf_Rela *rel);
void Rel_x86_64_Apply(Elf *elf);

#endif // REL_X86_64_H
