#ifndef ELF_H
#define ELF_H

#include <stdio.h>

// Output sections of the executable image.
typedef enum {
    ELF_SECTION_TEXT,
    ELF_SECTION_RODATA
} Elf_Section;

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

// Reserves a rel32 at the current position, resolved to target by Elf_Finish.
void Elf_AddRel32(const char *target);

// Resolves fixups and writes the finished ELF executable, entered at entry.
void Elf_Finish(FILE *out, const char *entry);

#endif // ELF_H
