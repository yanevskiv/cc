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
#define ELF_SHT_NOBITS   8

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

// ELF identification bytes: 64-bit, little-endian, version 1.
#define ELF_CLASS64  2
#define ELF_DATA2LSB 1
#define ELF_VERSION  1

// Virtual address an executable is loaded at, and the page size segments align.
#define ELF_BASE 0x400000
#define ELF_PAGE 0x1000

// Loadable program-header segment, readable and executable.
#define ELF_PT_LOAD 1
#define ELF_PF_R 4
#define ELF_PF_X 1

// The undefined section index used by external references.
#define ELF_SHN_UNDEF 0

// Packs and unpacks the symbol binding/type nibbles of st_info.
#define ELF_ST_INFO(bind, type) (((bind) << 4) | ((type) & 0xF))
#define ELF_ST_BIND(info) ((info) >> 4)
#define ELF_ST_TYPE(info) ((info) & 0xF)

// Packs and unpacks the symbol index and type of r_info.
#define ELF_R_INFO(sym, type) (((uint64_t) (sym) << 32) | (uint32_t) (type))
#define ELF_R_SYM(info)  ((uint32_t) ((info) >> 32))
#define ELF_R_TYPE(info) ((uint32_t) ((info) & 0xFFFFFFFF))

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

// The fixed-size ELF file header, on disk.
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

// One program header, describing a segment to load.
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

// One section header in the section header table.
typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

// One entry in an on-disk .symtab.
typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

// One entry in an on-disk .rela.* section.
typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

// An ELF object: header fields, sections, symbols and a name string pool.
struct Elf {
    uint16_t    elf_type;
    uint16_t    elf_machine;
    uint64_t    elf_entry;
    Elf_Sec   **elf_secs;
    size_t      elf_nsecs;
    size_t      elf_capsecs;
    Elf_Sym   **elf_syms;
    size_t      elf_nsyms;
    size_t      elf_capsyms;
    char      **elf_pool;
    size_t      elf_npool;
    size_t      elf_cappool;
    const char *elf_err;
};
typedef struct Elf Elf;

// Growable byte buffers
void   Elf_BufInit(Elf_Buf *buf);
void   Elf_BufFree(Elf_Buf *buf);
void   Elf_BufReserve(Elf_Buf *buf, size_t n);
void  *Elf_BufAt(Elf_Buf *buf, size_t off);
size_t Elf_BufByte(Elf_Buf *buf, uint8_t value);
size_t Elf_BufData(Elf_Buf *buf, const void *data, size_t n);
size_t Elf_BufU16(Elf_Buf *buf, uint16_t value);
size_t Elf_BufU32(Elf_Buf *buf, uint32_t value);
size_t Elf_BufU64(Elf_Buf *buf, uint64_t value);
size_t Elf_BufZero(Elf_Buf *buf, size_t n);
size_t Elf_BufAlign(Elf_Buf *buf, size_t align);

// Object lifecycle and header fields
const char *Elf_Intern(Elf *elf, const char *name);
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

// Writing ELF files
uint32_t Elf_WriteStr(Elf_Buf *strtab, const char *name);
uint32_t Elf_SectionIndex(const Elf *elf, const Elf_Sec *sec, const uint32_t *secidx);
void     Elf_WriteSymtab(const Elf *elf, const uint32_t *secidx, Elf_Buf *symtab, Elf_Buf *strtab, uint32_t *slot, uint32_t *first_global);
void     Elf_WriteRelas(const Elf_Sec *sec, const uint32_t *slot, const Elf *elf, Elf_Buf *out);
int      Elf_WriteRel(const Elf *elf, FILE *out);
uint64_t Elf_PlaceOffset(uint64_t pos, uint64_t vaddr);
int      Elf_WriteExec(const Elf *elf, FILE *out);
int      Elf_WriteFile(const Elf *elf, FILE *out);
int      Elf_Write(const Elf *elf, const char *path);

// Reading ELF files
const Elf64_Ehdr *Elf_ReadEhdr(const uint8_t *data, size_t n);
Elf *Elf_ReadMem(const void *buf, size_t n);
Elf *Elf_Read(const char *path);

#endif // ELF_H
