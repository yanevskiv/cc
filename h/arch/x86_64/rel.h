#ifndef REL_X86_64_H
#define REL_X86_64_H

#include "util/elf.h"

// x86-64 relocation type numbers, stored opaquely as an Elf_Rela's rel_type.
#define R_X86_64_64    1
#define R_X86_64_PC32  2
#define R_X86_64_PLT32 4
#define R_X86_64_32    10
#define R_X86_64_32S   11

// Applying relocations
void Rel_x86_64_Apply(Elf *elf);

#endif // REL_X86_64_H
