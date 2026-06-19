#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/elf.h"

// Virtual address the executable is loaded at; file offset 0 maps here.
#define ELF_BASE 0x400000

// Page size used to align the single loadable segment.
#define ELF_PAGE 0x1000

// Largest number of symbols and fixups one program may define.
#define ELF_MAX_SYMBOLS 4096
#define ELF_MAX_RELS    4096
#define ELF_MAX_GLOBALS 4096

// ELF identification bytes: magic, 64-bit, little-endian, version 1.
#define ELF_CLASS64 2
#define ELF_DATA2LSB 1
#define ELF_VERSION 1

// Object types and target machine.
#define ELF_TYPE_REL  1
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_X86_64 62

// Loadable segment, readable and executable.
#define ELF_PT_LOAD 1
#define ELF_PF_R 4
#define ELF_PF_X 1

// Section header types and flags.
#define ELF_SHT_PROGBITS 1
#define ELF_SHT_SYMTAB   2
#define ELF_SHT_STRTAB   3
#define ELF_SHT_RELA     4
#define ELF_SHF_WRITE     0x1
#define ELF_SHF_ALLOC     0x2
#define ELF_SHF_EXECINSTR 0x4
#define ELF_SHF_INFO_LINK 0x40
#define ELF_SHN_UNDEF 0

// Symbol binding (high nibble) and type (low nibble) of st_info.
#define ELF_STB_LOCAL  0
#define ELF_STB_GLOBAL 1
#define ELF_STT_NOTYPE 0
#define ELF_STT_OBJECT 1
#define ELF_STT_FUNC   2
#define ELF_ST_INFO(bind, type) (((bind) << 4) | ((type) & 0xF))
#define ELF_ST_BIND(info) ((info) >> 4)
#define ELF_ST_TYPE(info) ((info) & 0xF)

// Relocation types (low 32 bits of r_info; high 32 bits are the symbol index).
#define ELF_R_X86_64_64    1
#define ELF_R_X86_64_PC32  2
#define ELF_R_X86_64_PLT32 4
#define ELF_R_X86_64_32    10
#define ELF_R_X86_64_32S   11
#define ELF_R_INFO(sym, type) (((uint64_t) (sym) << 32) | (uint32_t) (type))
#define ELF_R_SYM(info)  ((int) ((info) >> 32))
#define ELF_R_TYPE(info) ((int) ((info) & 0xFFFFFFFF))

// rel32 fixups sit 4 bytes before the next instruction, so a PC-relative
// reference to the exact target carries an addend of -4.
#define ELF_REL_ADDEND (-4)

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

// One section header in the section header table (ET_REL output).
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
} Elf_Shdr;

// One entry in .symtab.
typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf_Sym;

// One entry in a .rela.* section.
typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf_Rela;

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
    Elf_RelType er_type;
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

// Names declared global via .globl; matched against defined symbols at finish.
static const char *Elf_Globals[ELF_MAX_GLOBALS];
static int         Elf_NumGlobals;

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
    Elf_NumGlobals = 0;
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

// Marks a defined symbol as STB_GLOBAL (from a .globl directive).
void Elf_MarkGlobal(const char *name)
{
    if (Elf_NumGlobals >= ELF_MAX_GLOBALS) {
        Elf_Fail("too many globals (max %d)", ELF_MAX_GLOBALS);
    }
    Elf_Globals[Elf_NumGlobals++] = name;
}

// Reserves a rel32 at the current position, resolved to target by
// Elf_FinishExec or emitted as a type relocation by Elf_FinishRel.
void Elf_AddRel32(const char *target, Elf_RelType type)
{
    if (Elf_NumRels >= ELF_MAX_RELS) {
        Elf_Fail("too many relocations (max %d)", ELF_MAX_RELS);
    }
    Elf_Rels[Elf_NumRels++] = (Elf_Rel) {
        .er_section = Elf_Current,
        .er_offset  = Elf_SectionBuf(Elf_Current)->eb_len,
        .er_target  = target,
        .er_type    = type
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
void Elf_FinishExec(FILE *out, const char *entry)
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

// --- ET_REL writer ------------------------------------------------------

// Section indices in the object's section header table.  The .text and
// .rodata bytes always get a header; symbol references resolve to these.
#define ELF_SHNDX_TEXT   1
#define ELF_SHNDX_RODATA 2

// One row destined for .symtab, before it is split into local/global order.
typedef struct {
    const char *fs_name;
    int         fs_defined;   // 1 if defined in this object
    Elf_Section fs_section;   // where defined
    int         fs_value;     // offset within its section
    int         fs_bind;      // ELF_STB_*
    int         fs_type;      // ELF_STT_*
    int         fs_index;     // assigned .symtab index
} Elf_FinalSym;

// Appends name and a NUL to a string table, returning name's start offset.
static uint32_t Elf_BufStr(Elf_Buf *buf, const char *name)
{
    uint32_t off = buf->eb_len;
    for (const char *p = name; *p; p++) {
        Elf_BufByte(buf, (unsigned char) *p);
    }
    Elf_BufByte(buf, 0);
    return off;
}

// True if name was declared via .globl.
static int Elf_IsGlobal(const char *name)
{
    for (int i = 0; i < Elf_NumGlobals; i++) {
        if (strcmp(Elf_Globals[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Index of name within the final-symbol list, or -1 if absent.
static int Elf_FindFinal(const Elf_FinalSym *fs, int n, const char *name)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(fs[i].fs_name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Builds the .symtab rows: every defined symbol, plus an SHN_UNDEF entry for
// each relocation target with no local definition (e.g. a call to printf).
static int Elf_BuildFinalSyms(Elf_FinalSym *fs)
{
    int n = 0;

    for (int i = 0; i < Elf_NumSymbols; i++) {
        const Elf_Symbol *sym = &Elf_Symbols[i];
        int global = Elf_IsGlobal(sym->es_name);
        int type   = ELF_STT_NOTYPE;
        if (global) {
            type = sym->es_section == ELF_SECTION_TEXT ? ELF_STT_FUNC
                                                       : ELF_STT_OBJECT;
        }
        fs[n++] = (Elf_FinalSym) {
            .fs_name    = sym->es_name,
            .fs_defined = 1,
            .fs_section = sym->es_section,
            .fs_value   = sym->es_offset,
            .fs_bind    = global ? ELF_STB_GLOBAL : ELF_STB_LOCAL,
            .fs_type    = type
        };
    }

    for (int i = 0; i < Elf_NumRels; i++) {
        const char *name = Elf_Rels[i].er_target;
        if (Elf_FindFinal(fs, n, name) < 0) {
            fs[n++] = (Elf_FinalSym) {
                .fs_name = name,
                .fs_bind = ELF_STB_GLOBAL,
                .fs_type = ELF_STT_NOTYPE
            };
        }
    }
    return n;
}

// Orders fs[] with all locals before all globals (ELF requires it), assigns
// each row its final .symtab index, and builds the symbol table (*syms_out,
// *nsym_out), its string table (*strtab_out) and the first-global index.
// Index 0 is the reserved null entry.
static void Elf_EmitSymtab(Elf_FinalSym *fs, int nfs, Elf_Sym **syms_out,
                           int *nsym_out, Elf_Buf *strtab_out, int *first_global_out)
{
    int      nsym = nfs + 1;
    Elf_Sym *syms = calloc(nsym, sizeof *syms);
    Elf_Buf  strtab = {0};
    Elf_BufByte(&strtab, 0);

    int si = 1;
    for (int pass = 0; pass < 2; pass++) {
        int want = pass == 0 ? ELF_STB_LOCAL : ELF_STB_GLOBAL;
        for (int i = 0; i < nfs; i++) {
            if (fs[i].fs_bind != want) {
                continue;
            }
            fs[i].fs_index = si;
            syms[si].st_name  = Elf_BufStr(&strtab, fs[i].fs_name);
            syms[si].st_info  = ELF_ST_INFO(fs[i].fs_bind, fs[i].fs_type);
            if (fs[i].fs_defined) {
                syms[si].st_shndx = fs[i].fs_section == ELF_SECTION_TEXT
                                        ? ELF_SHNDX_TEXT : ELF_SHNDX_RODATA;
                syms[si].st_value = fs[i].fs_value;
            } else {
                syms[si].st_shndx = ELF_SHN_UNDEF;
            }
            si++;
        }
    }
    int first_global = si;
    for (int i = 0; i < nfs; i++) {
        if (fs[i].fs_bind == ELF_STB_GLOBAL) {
            first_global = fs[i].fs_index;
            break;
        }
    }

    *syms_out         = syms;
    *nsym_out         = nsym;
    *strtab_out       = strtab;
    *first_global_out = first_global;
}

// Writes a complete ET_REL file from already-built section contents: the
// .text/.rodata bytes, a symbol table (with its string table and first-global
// index) and the split .rela.text/.rela.rodata tables.  Empty .rela.* sections
// are omitted; .text and .rodata keep fixed indices 1 and 2.
static void Elf_WriteRelFile(FILE *out, const Elf_Buf *text, const Elf_Buf *rodata,
                             const Elf_Sym *syms, int nsym, int first_global,
                             const Elf_Buf *strtab, const Elf_Rela *relatext, int ntr,
                             const Elf_Rela *relarodata, int nrr)
{
    Elf_Shdr shdrs[8] = {0};
    const void *secdata[8] = {0};
    Elf_Buf shstr = {0};
    Elf_BufByte(&shstr, 0);

    int idx_relatext   = ntr ? 5 : 0;
    int idx_relarodata = nrr ? (idx_relatext ? idx_relatext + 1 : 5) : 0;
    int idx_shstrtab   = 5 + (ntr ? 1 : 0) + (nrr ? 1 : 0);
    int shnum          = idx_shstrtab + 1;

    shdrs[ELF_SHNDX_TEXT] = (Elf_Shdr) {
        .sh_name = Elf_BufStr(&shstr, ".text"),
        .sh_type = ELF_SHT_PROGBITS,
        .sh_flags = ELF_SHF_ALLOC | ELF_SHF_EXECINSTR,
        .sh_size = text->eb_len,
        .sh_addralign = 1
    };
    secdata[ELF_SHNDX_TEXT] = text->eb_data;

    shdrs[ELF_SHNDX_RODATA] = (Elf_Shdr) {
        .sh_name = Elf_BufStr(&shstr, ".rodata"),
        .sh_type = ELF_SHT_PROGBITS,
        .sh_flags = ELF_SHF_ALLOC,
        .sh_size = rodata->eb_len,
        .sh_addralign = 1
    };
    secdata[ELF_SHNDX_RODATA] = rodata->eb_data;

    int idx_symtab = 3, idx_strtab = 4;
    shdrs[idx_symtab] = (Elf_Shdr) {
        .sh_name = Elf_BufStr(&shstr, ".symtab"),
        .sh_type = ELF_SHT_SYMTAB,
        .sh_size = (uint64_t) nsym * sizeof(Elf_Sym),
        .sh_link = idx_strtab,
        .sh_info = first_global,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf_Sym)
    };
    secdata[idx_symtab] = syms;

    shdrs[idx_strtab] = (Elf_Shdr) {
        .sh_name = Elf_BufStr(&shstr, ".strtab"),
        .sh_type = ELF_SHT_STRTAB,
        .sh_size = strtab->eb_len,
        .sh_addralign = 1
    };
    secdata[idx_strtab] = strtab->eb_data;

    if (idx_relatext) {
        shdrs[idx_relatext] = (Elf_Shdr) {
            .sh_name = Elf_BufStr(&shstr, ".rela.text"),
            .sh_type = ELF_SHT_RELA,
            .sh_flags = ELF_SHF_INFO_LINK,
            .sh_size = (uint64_t) ntr * sizeof(Elf_Rela),
            .sh_link = idx_symtab,
            .sh_info = ELF_SHNDX_TEXT,
            .sh_addralign = 8,
            .sh_entsize = sizeof(Elf_Rela)
        };
        secdata[idx_relatext] = relatext;
    }
    if (idx_relarodata) {
        shdrs[idx_relarodata] = (Elf_Shdr) {
            .sh_name = Elf_BufStr(&shstr, ".rela.rodata"),
            .sh_type = ELF_SHT_RELA,
            .sh_flags = ELF_SHF_INFO_LINK,
            .sh_size = (uint64_t) nrr * sizeof(Elf_Rela),
            .sh_link = idx_symtab,
            .sh_info = ELF_SHNDX_RODATA,
            .sh_addralign = 8,
            .sh_entsize = sizeof(Elf_Rela)
        };
        secdata[idx_relarodata] = relarodata;
    }

    shdrs[idx_shstrtab] = (Elf_Shdr) {
        .sh_name = Elf_BufStr(&shstr, ".shstrtab"),
        .sh_type = ELF_SHT_STRTAB,
        .sh_size = shstr.eb_len,
        .sh_addralign = 1
    };
    secdata[idx_shstrtab] = shstr.eb_data;

    // Assign file offsets to each section, then the header table (8-aligned).
    uint64_t off = sizeof(Elf_Ehdr);
    for (int i = 1; i < shnum; i++) {
        uint64_t align = shdrs[i].sh_addralign ? shdrs[i].sh_addralign : 1;
        off = (off + align - 1) / align * align;
        shdrs[i].sh_offset = off;
        off += shdrs[i].sh_size;
    }
    off = (off + 7) / 8 * 8;
    uint64_t shoff = off;

    Elf_Ehdr ehdr = {
        .e_ident     = { 0x7F, 'E', 'L', 'F', ELF_CLASS64, ELF_DATA2LSB, ELF_VERSION },
        .e_type      = ELF_TYPE_REL,
        .e_machine   = ELF_MACHINE_X86_64,
        .e_version   = ELF_VERSION,
        .e_shoff     = shoff,
        .e_ehsize    = sizeof(Elf_Ehdr),
        .e_shentsize = sizeof(Elf_Shdr),
        .e_shnum     = shnum,
        .e_shstrndx  = idx_shstrtab
    };

    // Emit: header, section bytes (padded to their offsets), header table.
    long pos = 0;
    fwrite(&ehdr, sizeof(ehdr), 1, out);
    pos += sizeof(ehdr);
    for (int i = 1; i < shnum; i++) {
        while (pos < (long) shdrs[i].sh_offset) {
            fputc(0, out);
            pos++;
        }
        fwrite(secdata[i], 1, shdrs[i].sh_size, out);
        pos += shdrs[i].sh_size;
    }
    while (pos < (long) shoff) {
        fputc(0, out);
        pos++;
    }
    fwrite(shdrs, sizeof(Elf_Shdr), shnum, out);

    free(shstr.eb_data);
}

// Writes a relocatable ELF object (ET_REL) from the encoder's current state.
void Elf_FinishRel(FILE *out)
{
    // Gather symbols (defined + undefined references) and emit the symbol table.
    Elf_FinalSym *fs = calloc(Elf_NumSymbols + Elf_NumRels + 1, sizeof *fs);
    int nfs = Elf_BuildFinalSyms(fs);

    Elf_Sym *syms;
    int      nsym, first_global;
    Elf_Buf  strtab;
    Elf_EmitSymtab(fs, nfs, &syms, &nsym, &strtab, &first_global);

    // Split relocations into per-section .rela.* tables, by target name.
    Elf_Rela *relatext   = calloc(Elf_NumRels + 1, sizeof *relatext);
    Elf_Rela *relarodata = calloc(Elf_NumRels + 1, sizeof *relarodata);
    int ntr = 0, nrr = 0;
    for (int i = 0; i < Elf_NumRels; i++) {
        Elf_Rel *rel = &Elf_Rels[i];
        int fi  = Elf_FindFinal(fs, nfs, rel->er_target);
        int rt  = rel->er_type == ELF_REL_PLT32 ? ELF_R_X86_64_PLT32
                                                : ELF_R_X86_64_PC32;
        Elf_Rela e = {
            .r_offset = rel->er_offset,
            .r_info   = ELF_R_INFO(fs[fi].fs_index, rt),
            .r_addend = ELF_REL_ADDEND
        };
        if (rel->er_section == ELF_SECTION_TEXT) {
            relatext[ntr++] = e;
        } else {
            relarodata[nrr++] = e;
        }
    }

    Elf_WriteRelFile(out, &Elf_Text, &Elf_Rodata, syms, nsym, first_global,
                     &strtab, relatext, ntr, relarodata, nrr);

    free(fs);
    free(syms);
    free(strtab.eb_data);
    free(relatext);
    free(relarodata);
}

// --- ET_REL reader / link core ------------------------------------------

// One relocatable object loaded into memory for linking.  lo_outsec/lo_base
// record where each input section landed in the merged output: which output
// section (ELF_SECTION_* or -1 if not allocated) and at what offset within it.
typedef struct {
    const char    *lo_path;     // input path, for diagnostics
    unsigned char *lo_data;     // whole file image, owned
    Elf_Ehdr      *lo_ehdr;     // -> lo_data
    Elf_Shdr      *lo_shdr;     // section header table (lo_ehdr->e_shnum rows)
    Elf_Sym       *lo_syms;     // .symtab rows
    int            lo_nsyms;
    const char    *lo_symstr;   // string table backing .symtab
    int           *lo_outsec;   // [e_shnum] ELF_SECTION_*, or -1
    long          *lo_base;     // [e_shnum] offset within that output section
} Elf_Obj;

// One defined global symbol in the combined program.
typedef struct {
    const char *lg_name;
    uint64_t    lg_vaddr;
} Elf_LinkGlobal;

// One loadable segment in a placed (-place) executable.
typedef struct {
    Elf_Buf *es_buf;
    uint64_t es_vaddr;
    uint64_t es_offset;
} Elf_Seg;

// Reads an entire file into a freshly allocated buffer.
static unsigned char *Elf_ReadFile(const char *path, long *size_out)
{
    FILE *f = fopen(path, "rb");
    if (! f) {
        Elf_Fail("cannot open '%s'", path);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(size);
    if (fread(buf, 1, size, f) != (size_t) size) {
        Elf_Fail("short read on '%s'", path);
    }
    fclose(f);
    *size_out = size;
    return buf;
}

// Loads and validates one ET_REL object, locating its section headers and
// symbol table.  Section-placement fields start unassigned (-1).
static void Elf_LoadObj(Elf_Obj *obj, const char *path)
{
    long size;
    unsigned char *data = Elf_ReadFile(path, &size);
    Elf_Ehdr *eh = (Elf_Ehdr *) data;

    if (size < (long) sizeof *eh || eh->e_ident[0] != 0x7F ||
        eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') {
        Elf_Fail("'%s' is not an ELF file", path);
    }
    if (eh->e_type != ELF_TYPE_REL) {
        Elf_Fail("'%s' is not a relocatable object (ET_REL)", path);
    }
    if (eh->e_machine != ELF_MACHINE_X86_64) {
        Elf_Fail("'%s' is not an x86-64 object", path);
    }

    Elf_Shdr *sh = (Elf_Shdr *) (data + eh->e_shoff);
    int shnum = eh->e_shnum;

    Elf_Sym    *syms   = NULL;
    int         nsyms  = 0;
    const char *symstr = NULL;
    for (int i = 0; i < shnum; i++) {
        if (sh[i].sh_type == ELF_SHT_SYMTAB) {
            syms   = (Elf_Sym *) (data + sh[i].sh_offset);
            nsyms  = sh[i].sh_size / sizeof(Elf_Sym);
            symstr = (const char *) (data + sh[sh[i].sh_link].sh_offset);
            break;
        }
    }
    if (! syms) {
        Elf_Fail("'%s' has no symbol table", path);
    }

    obj->lo_path   = path;
    obj->lo_data   = data;
    obj->lo_ehdr   = eh;
    obj->lo_shdr   = sh;
    obj->lo_syms   = syms;
    obj->lo_nsyms  = nsyms;
    obj->lo_symstr = symstr;
    obj->lo_outsec = malloc(shnum * sizeof *obj->lo_outsec);
    obj->lo_base   = calloc(shnum, sizeof *obj->lo_base);
    for (int i = 0; i < shnum; i++) {
        obj->lo_outsec[i] = -1;
    }
}

// Appends len bytes to a merged section buffer, padding to align first, and
// returns the offset the bytes start at.
static long Elf_BufAppend(Elf_Buf *buf, const unsigned char *src, long len, long align)
{
    if (align < 1) {
        align = 1;
    }
    while (buf->eb_len % align != 0) {
        Elf_BufByte(buf, 0);
    }
    long at = buf->eb_len;
    for (long i = 0; i < len; i++) {
        Elf_BufByte(buf, src[i]);
    }
    return at;
}

// Virtual address of an output section's base, given the merged text/rodata
// vaddrs computed once the text size is known.
static uint64_t Elf_OutVaddr(int outsec, uint64_t text_vaddr, uint64_t rodata_vaddr)
{
    return outsec == ELF_SECTION_TEXT ? text_vaddr : rodata_vaddr;
}

// Resolves a symbol reference (object o, symbol index) to a virtual address.
// A symbol defined in a merged section -- whether a file-local label or a
// global -- is placed from that object's section base; an undefined reference
// is matched by name against the program-wide global table.
static uint64_t Elf_ResolveSym(Elf_Obj *o, int symidx, uint64_t text_vaddr,
                               uint64_t rodata_vaddr, Elf_LinkGlobal *globals, int nglob)
{
    Elf_Sym    *sym  = &o->lo_syms[symidx];
    const char *name = o->lo_symstr + sym->st_name;

    if (sym->st_shndx != ELF_SHN_UNDEF && sym->st_shndx < o->lo_ehdr->e_shnum) {
        int outsec = o->lo_outsec[sym->st_shndx];
        if (outsec < 0) {
            Elf_Fail("'%s': reference to symbol '%s' in a non-loadable section",
                     o->lo_path, name);
        }
        return Elf_OutVaddr(outsec, text_vaddr, rodata_vaddr)
             + o->lo_base[sym->st_shndx] + sym->st_value;
    }

    for (int g = 0; g < nglob; g++) {
        if (strcmp(globals[g].lg_name, name) == 0) {
            return globals[g].lg_vaddr;
        }
    }
    Elf_Fail("undefined symbol '%s'", name);
}

// Patches one little-endian field of width bytes at at with value.
static void Elf_PatchLE(unsigned char *at, uint64_t value, int width)
{
    for (int b = 0; b < width; b++) {
        at[b] = (value >> (8 * b)) & 0xFF;
    }
}

// Merges every object's allocatable PROGBITS section into text (executable) or
// rodata (everything else), recording each input section's output placement.
static void Elf_MergeSections(Elf_Obj *objs, int npaths, Elf_Buf *text, Elf_Buf *rodata)
{
    for (int i = 0; i < npaths; i++) {
        Elf_Obj *o = &objs[i];
        for (int s = 0; s < o->lo_ehdr->e_shnum; s++) {
            Elf_Shdr *sec = &o->lo_shdr[s];
            if (sec->sh_type != ELF_SHT_PROGBITS || ! (sec->sh_flags & ELF_SHF_ALLOC)) {
                continue;
            }
            int outsec = (sec->sh_flags & ELF_SHF_EXECINSTR) ? ELF_SECTION_TEXT
                                                             : ELF_SECTION_RODATA;
            Elf_Buf *buf = outsec == ELF_SECTION_TEXT ? text : rodata;
            o->lo_outsec[s] = outsec;
            o->lo_base[s]   = Elf_BufAppend(buf, o->lo_data + sec->sh_offset,
                                            sec->sh_size, sec->sh_addralign);
        }
    }
}

// Merges the symbol tables and relocations of the loaded objects into a single
// ET_REL object: globals are unified by name (undefined references fold into a
// matching definition), locals are kept per object, and every relocation site
// and symbol reference is rebased into the merged sections.
static void Elf_LinkEmitRel(Elf_Obj *objs, int npaths, const Elf_Buf *text,
                            const Elf_Buf *rodata, FILE *out)
{
    int total_syms = 0, total_rels = 0;
    for (int i = 0; i < npaths; i++) {
        total_syms += objs[i].lo_nsyms;
        for (int s = 0; s < objs[i].lo_ehdr->e_shnum; s++) {
            Elf_Shdr *rs = &objs[i].lo_shdr[s];
            if (rs->sh_type == ELF_SHT_RELA) {
                total_rels += rs->sh_size / sizeof(Elf_Rela);
            }
        }
    }

    Elf_FinalSym *fs = calloc(total_syms + 1, sizeof *fs);
    int nfs = 0;

    // Per-object map from an input symbol index to its row in fs[] (-1 = drop).
    int **symmap = calloc(npaths, sizeof *symmap);
    for (int i = 0; i < npaths; i++) {
        Elf_Obj *o = &objs[i];
        symmap[i] = malloc(o->lo_nsyms * sizeof **symmap);
        for (int k = 0; k < o->lo_nsyms; k++) {
            symmap[i][k] = -1;
            if (k == 0) {
                continue;   // reserved null symbol
            }
            Elf_Sym    *sym  = &o->lo_syms[k];
            int         bind = ELF_ST_BIND(sym->st_info);
            const char *name = o->lo_symstr + sym->st_name;

            int         defined = 0;
            Elf_Section section = ELF_SECTION_TEXT;
            int         value   = 0;
            if (sym->st_shndx != ELF_SHN_UNDEF && sym->st_shndx < o->lo_ehdr->e_shnum
                && o->lo_outsec[sym->st_shndx] >= 0) {
                defined = 1;
                section = o->lo_outsec[sym->st_shndx];
                value   = o->lo_base[sym->st_shndx] + sym->st_value;
            }

            if (bind == ELF_STB_GLOBAL) {
                int fi = -1;
                for (int j = 0; j < nfs; j++) {
                    if (fs[j].fs_bind == ELF_STB_GLOBAL && strcmp(fs[j].fs_name, name) == 0) {
                        fi = j;
                        break;
                    }
                }
                if (fi >= 0) {
                    if (defined) {
                        if (fs[fi].fs_defined) {
                            Elf_Fail("multiple definition of '%s'", name);
                        }
                        fs[fi].fs_defined = 1;
                        fs[fi].fs_section = section;
                        fs[fi].fs_value   = value;
                        fs[fi].fs_type    = ELF_ST_TYPE(sym->st_info);
                    }
                    symmap[i][k] = fi;
                    continue;
                }
            }

            fs[nfs] = (Elf_FinalSym) {
                .fs_name    = name,
                .fs_defined = defined,
                .fs_section = section,
                .fs_value   = value,
                .fs_bind    = bind == ELF_STB_GLOBAL ? ELF_STB_GLOBAL : ELF_STB_LOCAL,
                .fs_type    = ELF_ST_TYPE(sym->st_info)
            };
            symmap[i][k] = nfs++;
        }
    }

    Elf_Sym *syms;
    int      nsym, first_global;
    Elf_Buf  strtab;
    Elf_EmitSymtab(fs, nfs, &syms, &nsym, &strtab, &first_global);

    // Rebase relocations into the merged sections, retargeting symbol indices.
    Elf_Rela *relatext   = calloc(total_rels + 1, sizeof *relatext);
    Elf_Rela *relarodata = calloc(total_rels + 1, sizeof *relarodata);
    int ntr = 0, nrr = 0;
    for (int i = 0; i < npaths; i++) {
        Elf_Obj *o = &objs[i];
        for (int s = 0; s < o->lo_ehdr->e_shnum; s++) {
            Elf_Shdr *rs = &o->lo_shdr[s];
            if (rs->sh_type != ELF_SHT_RELA) {
                continue;
            }
            int tsec = rs->sh_info;
            if (tsec >= o->lo_ehdr->e_shnum || o->lo_outsec[tsec] < 0) {
                continue;
            }
            Elf_Rela *rela = (Elf_Rela *) (o->lo_data + rs->sh_offset);
            int       nrel = rs->sh_size / sizeof(Elf_Rela);
            for (int r = 0; r < nrel; r++) {
                int oldsym = ELF_R_SYM(rela[r].r_info);
                int fi     = (oldsym >= 0 && oldsym < o->lo_nsyms) ? symmap[i][oldsym] : -1;
                if (fi < 0) {
                    Elf_Fail("'%s': relocation against an unsupported symbol", o->lo_path);
                }
                Elf_Rela e = {
                    .r_offset = o->lo_base[tsec] + rela[r].r_offset,
                    .r_info   = ELF_R_INFO(fs[fi].fs_index, ELF_R_TYPE(rela[r].r_info)),
                    .r_addend = rela[r].r_addend
                };
                if (o->lo_outsec[tsec] == ELF_SECTION_TEXT) {
                    relatext[ntr++] = e;
                } else {
                    relarodata[nrr++] = e;
                }
            }
        }
    }

    Elf_WriteRelFile(out, text, rodata, syms, nsym, first_global, &strtab,
                     relatext, ntr, relarodata, nrr);

    for (int i = 0; i < npaths; i++) {
        free(symmap[i]);
    }
    free(symmap);
    free(fs);
    free(syms);
    free(strtab.eb_data);
    free(relatext);
    free(relarodata);
}

// Smallest file offset >= pos that is page-congruent with vaddr, as a PT_LOAD
// mapping requires (p_offset == p_vaddr, modulo the page size).
static uint64_t Elf_PlaceOffset(uint64_t pos, uint64_t vaddr)
{
    return pos + (vaddr - pos) % ELF_PAGE;
}

// Resolves the entry symbol against the global table, or aborts if undefined.
static uint64_t Elf_EntryVaddr(const char *entry, Elf_LinkGlobal *globals, int nglob)
{
    for (int g = 0; g < nglob; g++) {
        if (strcmp(globals[g].lg_name, entry) == 0) {
            return globals[g].lg_vaddr;
        }
    }
    Elf_Fail("undefined entry symbol '%s'", entry);
}

// Resolves symbols, applies relocations and writes a static ET_EXEC.  Without
// placement the headers, text and rodata share one R+X segment based at
// ELF_BASE; with -place each output section gets its own segment at the
// requested (or defaulted) address.
static void Elf_LinkEmitExec(Elf_Obj *objs, int npaths, Elf_Buf *text, Elf_Buf *rodata,
                             const Elf_LinkOptions *opts, FILE *out)
{
    int placed = opts->eo_place_text || opts->eo_place_data;

    // Output section addresses.  Default: a single segment with the headers,
    // text at ELF_BASE + headers, rodata immediately after.  Placed: each at
    // its own address, defaulting to ELF_BASE / the page above text's end.
    uint64_t text_vaddr, rodata_vaddr;
    if (! placed) {
        text_vaddr   = ELF_BASE + sizeof(Elf_Ehdr) + sizeof(Elf_Phdr);
        rodata_vaddr = text_vaddr + text->eb_len;
    } else {
        text_vaddr   = opts->eo_place_text ? opts->eo_text_addr : ELF_BASE;
        rodata_vaddr = opts->eo_place_data ? opts->eo_data_addr
                     : (text_vaddr + text->eb_len + ELF_PAGE - 1) / ELF_PAGE * ELF_PAGE;
    }

    // Collect defined global symbols; diagnose multiple definitions.
    int total_syms = 0;
    for (int i = 0; i < npaths; i++) {
        total_syms += objs[i].lo_nsyms;
    }
    Elf_LinkGlobal *globals = calloc(total_syms ? total_syms : 1, sizeof *globals);
    int nglob = 0;
    for (int i = 0; i < npaths; i++) {
        Elf_Obj *o = &objs[i];
        for (int k = 0; k < o->lo_nsyms; k++) {
            Elf_Sym *sym = &o->lo_syms[k];
            if (ELF_ST_BIND(sym->st_info) != ELF_STB_GLOBAL) {
                continue;
            }
            if (sym->st_shndx == ELF_SHN_UNDEF || sym->st_shndx >= o->lo_ehdr->e_shnum) {
                continue;
            }
            int outsec = o->lo_outsec[sym->st_shndx];
            if (outsec < 0) {
                continue;
            }
            const char *name = o->lo_symstr + sym->st_name;
            for (int g = 0; g < nglob; g++) {
                if (strcmp(globals[g].lg_name, name) == 0) {
                    Elf_Fail("multiple definition of '%s'", name);
                }
            }
            globals[nglob++] = (Elf_LinkGlobal) {
                .lg_name  = name,
                .lg_vaddr = Elf_OutVaddr(outsec, text_vaddr, rodata_vaddr)
                          + o->lo_base[sym->st_shndx] + sym->st_value
            };
        }
    }

    // Apply each object's relocations into the merged buffers.
    for (int i = 0; i < npaths; i++) {
        Elf_Obj *o = &objs[i];
        for (int s = 0; s < o->lo_ehdr->e_shnum; s++) {
            Elf_Shdr *rs = &o->lo_shdr[s];
            if (rs->sh_type != ELF_SHT_RELA) {
                continue;
            }
            int tsec = rs->sh_info;     // input section the relocations patch
            if (tsec >= o->lo_ehdr->e_shnum || o->lo_outsec[tsec] < 0) {
                continue;
            }
            int       outsec  = o->lo_outsec[tsec];
            Elf_Buf  *buf     = outsec == ELF_SECTION_TEXT ? text : rodata;
            uint64_t  secbase = Elf_OutVaddr(outsec, text_vaddr, rodata_vaddr);
            Elf_Rela *rela    = (Elf_Rela *) (o->lo_data + rs->sh_offset);
            int       nrel    = rs->sh_size / sizeof(Elf_Rela);

            for (int r = 0; r < nrel; r++) {
                int            type     = ELF_R_TYPE(rela[r].r_info);
                long           site_off = o->lo_base[tsec] + rela[r].r_offset;
                uint64_t       P        = secbase + site_off;
                uint64_t       S        = Elf_ResolveSym(o, ELF_R_SYM(rela[r].r_info),
                                              text_vaddr, rodata_vaddr, globals, nglob);
                int64_t        A        = rela[r].r_addend;
                unsigned char *at       = buf->eb_data + site_off;

                switch (type) {
                    case ELF_R_X86_64_PC32:
                    case ELF_R_X86_64_PLT32: {
                        Elf_PatchLE(at, (uint32_t) (int32_t) (S + A - P), 4);
                    } break;
                    case ELF_R_X86_64_32:
                    case ELF_R_X86_64_32S: {
                        Elf_PatchLE(at, (uint32_t) (S + A), 4);
                    } break;
                    case ELF_R_X86_64_64: {
                        Elf_PatchLE(at, S + A, 8);
                    } break;
                    default: {
                        Elf_Fail("'%s': unsupported relocation type %d", o->lo_path, type);
                    }
                }
            }
        }
    }

    uint64_t entry_vaddr = Elf_EntryVaddr(opts->eo_entry, globals, nglob);
    free(globals);

    if (! placed) {
        // One R+X segment: headers, then text, then rodata, mapped at ELF_BASE.
        uint64_t filesz = sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) + text->eb_len + rodata->eb_len;
        Elf_Ehdr ehdr = {
            .e_ident     = { 0x7F, 'E', 'L', 'F', ELF_CLASS64, ELF_DATA2LSB, ELF_VERSION },
            .e_type      = ELF_TYPE_EXEC,
            .e_machine   = ELF_MACHINE_X86_64,
            .e_version   = ELF_VERSION,
            .e_entry     = entry_vaddr,
            .e_phoff     = sizeof(Elf_Ehdr),
            .e_ehsize    = sizeof(Elf_Ehdr),
            .e_phentsize = sizeof(Elf_Phdr),
            .e_phnum     = 1
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
        fwrite(text->eb_data, 1, text->eb_len, out);
        fwrite(rodata->eb_data, 1, rodata->eb_len, out);
        return;
    }

    // Placed: one R+X PT_LOAD per non-empty output section, at its address.
    Elf_Seg segs[2];
    int nseg = 0;
    if (text->eb_len) {
        segs[nseg++] = (Elf_Seg) { text, text_vaddr, 0 };
    }
    if (rodata->eb_len) {
        segs[nseg++] = (Elf_Seg) { rodata, rodata_vaddr, 0 };
    }

    uint64_t pos = sizeof(Elf_Ehdr) + (uint64_t) nseg * sizeof(Elf_Phdr);
    for (int i = 0; i < nseg; i++) {
        segs[i].es_offset = Elf_PlaceOffset(pos, segs[i].es_vaddr);
        pos = segs[i].es_offset + segs[i].es_buf->eb_len;
    }

    Elf_Ehdr ehdr = {
        .e_ident     = { 0x7F, 'E', 'L', 'F', ELF_CLASS64, ELF_DATA2LSB, ELF_VERSION },
        .e_type      = ELF_TYPE_EXEC,
        .e_machine   = ELF_MACHINE_X86_64,
        .e_version   = ELF_VERSION,
        .e_entry     = entry_vaddr,
        .e_phoff     = sizeof(Elf_Ehdr),
        .e_ehsize    = sizeof(Elf_Ehdr),
        .e_phentsize = sizeof(Elf_Phdr),
        .e_phnum     = nseg
    };
    fwrite(&ehdr, sizeof(ehdr), 1, out);
    for (int i = 0; i < nseg; i++) {
        Elf_Phdr phdr = {
            .p_type   = ELF_PT_LOAD,
            .p_flags  = ELF_PF_R | ELF_PF_X,
            .p_offset = segs[i].es_offset,
            .p_vaddr  = segs[i].es_vaddr,
            .p_paddr  = segs[i].es_vaddr,
            .p_filesz = segs[i].es_buf->eb_len,
            .p_memsz  = segs[i].es_buf->eb_len,
            .p_align  = ELF_PAGE
        };
        fwrite(&phdr, sizeof(phdr), 1, out);
    }
    long fpos = sizeof(Elf_Ehdr) + (long) nseg * sizeof(Elf_Phdr);
    for (int i = 0; i < nseg; i++) {
        while (fpos < (long) segs[i].es_offset) {
            fputc(0, out);
            fpos++;
        }
        fwrite(segs[i].es_buf->eb_data, 1, segs[i].es_buf->eb_len, out);
        fpos += segs[i].es_buf->eb_len;
    }
}

// Links npaths ET_REL objects into an executable or, with -r, a merged object.
void Elf_Link(const char *const *paths, int npaths, const Elf_LinkOptions *opts, FILE *out)
{
    Elf_Obj *objs = calloc(npaths, sizeof *objs);
    for (int i = 0; i < npaths; i++) {
        Elf_LoadObj(&objs[i], paths[i]);
    }

    Elf_Buf text = {0}, rodata = {0};
    Elf_MergeSections(objs, npaths, &text, &rodata);

    if (opts->eo_relocatable) {
        Elf_LinkEmitRel(objs, npaths, &text, &rodata, out);
    } else {
        Elf_LinkOptions resolved = *opts;
        if (! resolved.eo_entry) {
            resolved.eo_entry = "_start";
        }
        Elf_LinkEmitExec(objs, npaths, &text, &rodata, &resolved, out);
    }

    free(text.eb_data);
    free(rodata.eb_data);
    for (int i = 0; i < npaths; i++) {
        free(objs[i].lo_data);
        free(objs[i].lo_outsec);
        free(objs[i].lo_base);
    }
    free(objs);
}
