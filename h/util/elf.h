#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// Object file types (e_type).
#define ELF_ET_REL  1
#define ELF_ET_EXEC 2

// Target machine (e_machine).
#define ELF_EM_X86_64 62

// Section header types (sec_type).
#define ELF_SHT_PROGBITS 1
#define ELF_SHT_SYMTAB   2
#define ELF_SHT_STRTAB   3
#define ELF_SHT_RELA     4

// Section header flags (sec_flags).
#define ELF_SHF_WRITE     0x1
#define ELF_SHF_ALLOC     0x2
#define ELF_SHF_EXECINSTR 0x4

// Symbol binding (sym_bind).
#define ELF_BIND_LOCAL  0
#define ELF_BIND_GLOBAL 1
#define ELF_BIND_WEAK   2

// Symbol type (sym_type).
#define ELF_TYPE_NOTYPE 0
#define ELF_TYPE_OBJECT 1
#define ELF_TYPE_FUNC   2

// A growable byte buffer -- the only storage primitive, with no ELF knowledge.
typedef struct {
    uint8_t *eb_data;
    size_t   eb_len;
    size_t   eb_cap;
} Elf_Buf;

// Forward declaration: a symbol's defining section is a pointer to one of these.
typedef struct Elf_Sec Elf_Sec;

// One symbol; sym_sec == NULL means undefined (an external reference).
typedef struct {
    const char *sym_name;    // owned by the Elf string pool
    Elf_Sec    *sym_sec;     // defining section, or NULL
    uint64_t    sym_value;   // offset within sym_sec
    uint64_t    sym_size;
    uint8_t     sym_bind;    // ELF_BIND_*
    uint8_t     sym_type;    // ELF_TYPE_*
    uint8_t     sym_other;   // visibility
} Elf_Sym;

// One relocation; it lives in the section it patches and rel_type is opaque here.
typedef struct {
    uint64_t  rel_offset;    // within the patched section
    Elf_Sym  *rel_sym;       // referenced symbol
    uint32_t  rel_type;      // R_<machine>_* (opaque here)
    int64_t   rel_addend;
} Elf_Rela;

// One section; PROGBITS carry bytes in sec_data and may own their relocations.
struct Elf_Sec {
    const char *sec_name;    // owned by the string pool
    uint32_t    sec_type;    // ELF_SHT_*
    uint64_t    sec_flags;   // ELF_SHF_*
    uint64_t    sec_addr;    // load address, 0 = unplaced
    uint64_t    sec_addralign;
    uint64_t    sec_entsize;
    Elf_Buf     sec_data;    // raw contents (PROGBITS)
    Elf_Rela   *sec_relas;   // relocations patching THIS section
    size_t      sec_nrelas;
    size_t      sec_caprelas;
};

// An ELF object
typedef struct Elf Elf;

// Growable byte buffers
void   Elf_BufInit(Elf_Buf *buf);
void   Elf_BufFree(Elf_Buf *buf);
void  *Elf_BufAt(Elf_Buf *buf, size_t off);
size_t Elf_BufByte(Elf_Buf *buf, uint8_t value);
size_t Elf_BufData(Elf_Buf *buf, const void *data, size_t n);
size_t Elf_BufU16(Elf_Buf *buf, uint16_t value);
size_t Elf_BufU32(Elf_Buf *buf, uint32_t value);
size_t Elf_BufU64(Elf_Buf *buf, uint64_t value);
size_t Elf_BufZero(Elf_Buf *buf, size_t n);
size_t Elf_BufAlign(Elf_Buf *buf, size_t align);

// Object lifecycle and header fields
Elf        *Elf_New(uint16_t type, uint16_t machine);
void        Elf_Free(Elf *elf);
void        Elf_SetEntry(Elf *elf, uint64_t vaddr);
void        Elf_SetType(Elf *elf, uint16_t type);
uint16_t    Elf_GetType(const Elf *elf);
const char *Elf_Error(const Elf *elf);

// Sections
Elf_Sec *Elf_SectionAdd(Elf *elf, const char *name, uint32_t type, uint64_t flags);
Elf_Sec *Elf_SectionFind(Elf *elf, const char *name);
Elf_Sec *Elf_SectionGet(Elf *elf, const char *name, uint32_t type, uint64_t flags);
size_t   Elf_SectionCount(const Elf *elf);
Elf_Sec *Elf_SectionAt(const Elf *elf, size_t i);
Elf_Buf *Elf_SectionData(Elf_Sec *sec);
void     Elf_SectionAddr(Elf_Sec *sec, uint64_t addr);

// Symbols
Elf_Sym *Elf_SymbolAdd(Elf *elf, const char *name, Elf_Sec *sec, uint64_t value, uint8_t bind, uint8_t type);
Elf_Sym *Elf_SymbolFind(Elf *elf, const char *name);
size_t   Elf_SymbolCount(const Elf *elf);
Elf_Sym *Elf_SymbolAt(const Elf *elf, size_t i);

// Relocations
Elf_Rela *Elf_RelaAdd(Elf_Sec *target, uint64_t offset, Elf_Sym *sym, uint32_t type, int64_t addend);
size_t    Elf_RelaCount(const Elf_Sec *target);
Elf_Rela *Elf_RelaAt(const Elf_Sec *target, size_t i);

// Reading and writing ELF files
Elf *Elf_Read(const char *path);
Elf *Elf_ReadMem(const void *buf, size_t n);
int  Elf_Write(const Elf *elf, const char *path);
int  Elf_WriteFile(const Elf *elf, FILE *out);

#endif // ELF_H
