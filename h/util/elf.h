#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stdio.h>

// Output sections of the executable image.
typedef enum {
    ELF_SECTION_TEXT,
    ELF_SECTION_RODATA
} Elf_Section;

// Relocation kinds the back end records against a named symbol.
typedef enum {
    ELF_REL_PC32,   // R_X86_64_PC32   rip-relative data, jmp/jcc
    ELF_REL_PLT32   // R_X86_64_PLT32  call
} Elf_RelType;

// Clears the image before encoding a fresh program.
void Elf_Reset(void);

// Selects the section that following writes append to.
void Elf_ChangeSection(Elf_Section sec);

// Appends one byte to the current section.
void Elf_WriteByte(int byte);

// Appends a little-endian 32-bit value to the current section.
void Elf_Write32(unsigned int val);

// Appends a little-endian 64-bit value to the current section.
void Elf_Write64(unsigned long long val);

// Appends a run of raw bytes to the current section.
void Elf_WriteBytes(const void *data, int len);

// Records a symbol at the current position in the current section.
void Elf_AddSymbol(const char *name);

// Marks a defined symbol as STB_GLOBAL (from a .globl directive).
void Elf_MarkGlobal(const char *name);

// Reserves a rel32 at the current position, with a relocation of type type
// against target -- resolved internally by Elf_FinishExec, or emitted as a
// relocation entry by Elf_FinishRel.
void Elf_AddRel32(const char *target, Elf_RelType type);

// Resolves fixups and writes a finished static ELF executable (ET_EXEC),
// entered at entry.
void Elf_FinishExec(FILE *out, const char *entry);

// Writes a relocatable ELF object (ET_REL): .text/.rodata, a symbol table
// and .rela.* relocations for a linker to resolve later.
void Elf_FinishRel(FILE *out);

// Options controlling a link (Elf_Link).
typedef struct {
    const char *eo_entry;        // executable entry symbol (NULL selects _start)
    int         eo_relocatable;  // -r: merge into an ET_REL object, keep relocs
    int         eo_place_text;   // place .text at eo_text_addr instead of default
    uint64_t    eo_text_addr;
    int         eo_place_data;   // place .rodata/.data at eo_data_addr
    uint64_t    eo_data_addr;
} Elf_LinkOptions;

// Links npaths ET_REL objects, reading each back in and merging like sections.
// With opts->eo_relocatable it merges the symbol tables and relocations into a
// single ET_REL object; otherwise it resolves one global symbol table, applies
// relocations and writes a static ET_EXEC entered at opts->eo_entry, honoring
// any -place addresses.  This is the ld driver's link core; Elf_Reset and the
// writers above are not involved.
void Elf_Link(const char *const *paths, int npaths, const Elf_LinkOptions *opts, FILE *out);

#endif // ELF_H
