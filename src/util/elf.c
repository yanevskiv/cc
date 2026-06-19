#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/elf.h"
#include "util/str.h"

// Virtual address the executable is loaded at; file offset 0 maps here.
#define ELF_BASE 0x400000

// Page size used to align the single loadable segment.
#define ELF_PAGE 0x1000

// Largest number of symbols and fixups one program may define.
#define ELF_MAX_SYMBOLS 4096
#define ELF_MAX_RELS    4096

// ELF identification bytes: magic, 64-bit, little-endian, version 1.
#define ELF_CLASS64 2
#define ELF_DATA2LSB 1
#define ELF_VERSION 1

// Executable type and target machine.
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_X86_64 62

// Loadable segment, readable and executable.
#define ELF_PT_LOAD 1
#define ELF_PF_R 4
#define ELF_PF_X 1

// Diagnoses an internal error in the writer and exits.
#define Elf_Fail(...)                   \
    do {                                \
        fprintf(stderr, "cc: error: "); \
        fprintf(stderr, __VA_ARGS__);   \
        fprintf(stderr, "\n");          \
        exit(1);                        \
    } while (0)

// The fixed-size ELF file header.
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
} Elf_Ehdr;

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
} Elf_Phdr;

// A growable buffer of encoded bytes for one section.
typedef struct {
    unsigned char *eb_data;
    int            eb_len;
    int            eb_cap;
} Elf_Buf;

// A named position recorded inside a section.
typedef struct {
    const char *es_name;
    Elf_Section es_section;
    int         es_offset;
} Elf_Symbol;

// A rel32 placeholder to be patched with the distance to a target symbol.
typedef struct {
    Elf_Section er_section;
    int         er_offset;
    const char *er_target;
} Elf_Rel;

// Encoded bytes of the .text and .rodata sections.
static Elf_Buf Elf_Text;
static Elf_Buf Elf_Rodata;

// Section that writes currently append to.
static Elf_Section Elf_Current;

// Symbols defined while encoding.
static Elf_Symbol Elf_Symbols[ELF_MAX_SYMBOLS];
static int        Elf_NumSymbols;

// Pending rel32 fixups.
static Elf_Rel Elf_Rels[ELF_MAX_RELS];
static int     Elf_NumRels;

// Returns the buffer backing the given section.
static Elf_Buf *Elf_SectionBuf(Elf_Section sec)
{
    return sec == ELF_SECTION_TEXT ? &Elf_Text : &Elf_Rodata;
}

// Appends one byte to a buffer, growing it as needed.
static void Elf_BufByte(Elf_Buf *buf, int byte)
{
    if (buf->eb_len == buf->eb_cap) {
        buf->eb_cap = buf->eb_cap ? buf->eb_cap * 2 : 256;
        buf->eb_data = realloc(buf->eb_data, buf->eb_cap);
    }
    buf->eb_data[buf->eb_len++] = (unsigned char) byte;
}

// Clears the image before encoding a fresh program.
void Elf_Reset(void)
{
    free(Elf_Text.eb_data);
    free(Elf_Rodata.eb_data);
    Elf_Text    = (Elf_Buf) {0};
    Elf_Rodata  = (Elf_Buf) {0};
    Elf_Current = ELF_SECTION_TEXT;
    Elf_NumSymbols = 0;
    Elf_NumRels    = 0;
}

// Selects the section that following writes append to.
void Elf_ChangeSection(Elf_Section sec)
{
    Elf_Current = sec;
}

// Appends one byte to the current section.
void Elf_WriteByte(int byte)
{
    Elf_BufByte(Elf_SectionBuf(Elf_Current), byte);
}

// Appends a little-endian 32-bit value to the current section.
void Elf_Write32(unsigned int val)
{
    for (int i = 0; i < 4; i++) {
        Elf_WriteByte((val >> (8 * i)) & 0xFF);
    }
}

// Appends a little-endian 64-bit value to the current section.
void Elf_Write64(unsigned long long val)
{
    for (int i = 0; i < 8; i++) {
        Elf_WriteByte((val >> (8 * i)) & 0xFF);
    }
}

// Appends a run of raw bytes to the current section.
void Elf_WriteBytes(const void *data, int len)
{
    const unsigned char *bytes = data;
    for (int i = 0; i < len; i++) {
        Elf_WriteByte(bytes[i]);
    }
}

// Records a symbol at the current position in the current section.
void Elf_AddSymbol(const char *name)
{
    if (Elf_NumSymbols >= ELF_MAX_SYMBOLS) {
        Elf_Fail("too many symbols (max %d)", ELF_MAX_SYMBOLS);
    }
    Elf_Symbols[Elf_NumSymbols++] = (Elf_Symbol) {
        .es_name    = name,
        .es_section = Elf_Current,
        .es_offset  = Elf_SectionBuf(Elf_Current)->eb_len
    };
}

// Reserves a rel32 at the current position, resolved to target by Elf_Finish.
void Elf_AddRel32(const char *target)
{
    if (Elf_NumRels >= ELF_MAX_RELS) {
        Elf_Fail("too many relocations (max %d)", ELF_MAX_RELS);
    }
    Elf_Rels[Elf_NumRels++] = (Elf_Rel) {
        .er_section = Elf_Current,
        .er_offset  = Elf_SectionBuf(Elf_Current)->eb_len,
        .er_target  = target
    };
}

// File offset at which the .text bytes begin.
static int Elf_TextOffset(void)
{
    return sizeof(Elf_Ehdr) + sizeof(Elf_Phdr);
}

// File offset at which the .rodata bytes begin.
static int Elf_RodataOffset(void)
{
    return Elf_TextOffset() + Elf_Text.eb_len;
}

// Virtual address of a position within a section.
static uint64_t Elf_Vaddr(Elf_Section sec, int offset)
{
    int base = sec == ELF_SECTION_TEXT ? Elf_TextOffset() : Elf_RodataOffset();
    return ELF_BASE + base + offset;
}

// Virtual address of a named symbol.
static uint64_t Elf_SymbolVaddr(const char *name)
{
    for (int i = 0; i < Elf_NumSymbols; i++) {
        if (strcmp(Elf_Symbols[i].es_name, name) == 0) {
            return Elf_Vaddr(Elf_Symbols[i].es_section, Elf_Symbols[i].es_offset);
        }
    }
    Elf_Fail("undefined symbol '%s'", name);
}

// Patches every rel32 with the signed distance from its site to its target.
static void Elf_ResolveRels(void)
{
    for (int i = 0; i < Elf_NumRels; i++) {
        Elf_Rel *rel = &Elf_Rels[i];
        uint64_t site   = Elf_Vaddr(rel->er_section, rel->er_offset);
        uint64_t target = Elf_SymbolVaddr(rel->er_target);
        int32_t  delta  = (int32_t) (target - (site + 4));

        unsigned char *at = Elf_SectionBuf(rel->er_section)->eb_data + rel->er_offset;
        for (int b = 0; b < 4; b++) {
            at[b] = (delta >> (8 * b)) & 0xFF;
        }
    }
}

// Resolves fixups and writes the finished ELF executable, entered at entry.
void Elf_Finish(FILE *out, const char *entry)
{
    Elf_ResolveRels();

    uint64_t filesz = Elf_RodataOffset() + Elf_Rodata.eb_len;

    Elf_Ehdr ehdr = {
        .e_ident    = { 0x7F, 'E', 'L', 'F', ELF_CLASS64, ELF_DATA2LSB, ELF_VERSION },
        .e_type     = ELF_TYPE_EXEC,
        .e_machine  = ELF_MACHINE_X86_64,
        .e_version  = ELF_VERSION,
        .e_entry    = Elf_SymbolVaddr(entry),
        .e_phoff    = sizeof(Elf_Ehdr),
        .e_ehsize   = sizeof(Elf_Ehdr),
        .e_phentsize = sizeof(Elf_Phdr),
        .e_phnum    = 1
    };

    Elf_Phdr phdr = {
        .p_type   = ELF_PT_LOAD,
        .p_flags  = ELF_PF_R | ELF_PF_X,
        .p_offset = 0,
        .p_vaddr  = ELF_BASE,
        .p_paddr  = ELF_BASE,
        .p_filesz = filesz,
        .p_memsz  = filesz,
        .p_align  = ELF_PAGE
    };

    fwrite(&ehdr, sizeof(ehdr), 1, out);
    fwrite(&phdr, sizeof(phdr), 1, out);
    fwrite(Elf_Text.eb_data, 1, Elf_Text.eb_len, out);
    fwrite(Elf_Rodata.eb_data, 1, Elf_Rodata.eb_len, out);
}
