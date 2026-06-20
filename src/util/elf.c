#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "util/elf.h"

// Initializes an empty byte buffer.
void Elf_BufInit(Elf_Buf *buf)
{
    buf->eb_data = NULL;
    buf->eb_len  = 0;
    buf->eb_cap  = 0;
}

// Frees a buffer's storage and clears it.
void Elf_BufFree(Elf_Buf *buf)
{
    free(buf->eb_data);
    buf->eb_data = NULL;
    buf->eb_len  = 0;
    buf->eb_cap  = 0;
}

// Grows a buffer so it can hold at least n more bytes.
void Elf_BufReserve(Elf_Buf *buf, size_t n)
{
    if (buf->eb_len + n <= buf->eb_cap) {
        return;
    }
    size_t cap = buf->eb_cap ? buf->eb_cap : 256;
    while (buf->eb_len + n > cap) {
        cap *= 2;
    }
    buf->eb_data = realloc(buf->eb_data, cap);
    buf->eb_cap  = cap;
}

// Returns a pointer to byte off within a buffer, for in-place patching.
void *Elf_BufAt(Elf_Buf *buf, size_t off)
{
    return buf->eb_data + off;
}

// Appends one byte, returning the offset it began at.
size_t Elf_BufByte(Elf_Buf *buf, uint8_t value)
{
    size_t off = buf->eb_len;
    Elf_BufReserve(buf, 1);
    buf->eb_data[buf->eb_len++] = value;
    return off;
}

// Appends n raw bytes, returning the offset they began at.
size_t Elf_BufData(Elf_Buf *buf, const void *data, size_t n)
{
    size_t off = buf->eb_len;
    Elf_BufReserve(buf, n);
    memcpy(buf->eb_data + buf->eb_len, data, n);
    buf->eb_len += n;
    return off;
}

// Appends a little-endian 16-bit value, returning its offset.
size_t Elf_BufU16(Elf_Buf *buf, uint16_t value)
{
    uint8_t bytes[2] = { value & 0xFF, (value >> 8) & 0xFF };
    return Elf_BufData(buf, bytes, 2);
}

// Appends a little-endian 32-bit value, returning its offset.
size_t Elf_BufU32(Elf_Buf *buf, uint32_t value)
{
    uint8_t bytes[4];
    for (int i = 0; i < 4; i++) {
        bytes[i] = (value >> (8 * i)) & 0xFF;
    }
    return Elf_BufData(buf, bytes, 4);
}

// Appends a little-endian 64-bit value, returning its offset.
size_t Elf_BufU64(Elf_Buf *buf, uint64_t value)
{
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (value >> (8 * i)) & 0xFF;
    }
    return Elf_BufData(buf, bytes, 8);
}

// Appends n zero bytes, returning the offset they began at.
size_t Elf_BufZero(Elf_Buf *buf, size_t n)
{
    size_t off = buf->eb_len;
    Elf_BufReserve(buf, n);
    memset(buf->eb_data + buf->eb_len, 0, n);
    buf->eb_len += n;
    return off;
}

// Pads the buffer with zeros up to a multiple of align, returning new length.
size_t Elf_BufAlign(Elf_Buf *buf, size_t align)
{
    if (align > 1) {
        while (buf->eb_len % align != 0) {
            Elf_BufByte(buf, 0);
        }
    }
    return buf->eb_len;
}

// Interns a name into the object's string pool, returning an owned copy.
const char *Elf_Intern(Elf *elf, const char *name)
{
    if (! name) {
        name = "";
    }
    char *copy = malloc(strlen(name) + 1);
    strcpy(copy, name);
    if (elf->elf_npool == elf->elf_cappool) {
        elf->elf_cappool = elf->elf_cappool ? elf->elf_cappool * 2 : 16;
        elf->elf_pool = realloc(elf->elf_pool, elf->elf_cappool * sizeof *elf->elf_pool);
    }
    elf->elf_pool[elf->elf_npool++] = copy;
    return copy;
}

// Creates an empty ELF object of the given type and machine.
Elf *Elf_New(uint16_t type, uint16_t machine)
{
    Elf *elf = calloc(1, sizeof *elf);
    elf->elf_type    = type;
    elf->elf_machine = machine;
    return elf;
}

// Frees an ELF object and everything it owns.
void Elf_Free(Elf *elf)
{
    if (! elf) {
        return;
    }
    for (size_t i = 0; i < elf->elf_nsecs; i++) {
        Elf_BufFree(&elf->elf_secs[i]->sec_data);
        free(elf->elf_secs[i]->sec_relas);
        free(elf->elf_secs[i]);
    }
    free(elf->elf_secs);
    for (size_t i = 0; i < elf->elf_nsyms; i++) {
        free(elf->elf_syms[i]);
    }
    free(elf->elf_syms);
    for (size_t i = 0; i < elf->elf_npool; i++) {
        free(elf->elf_pool[i]);
    }
    free(elf->elf_pool);
    free(elf);
}

// Sets the entry virtual address (ET_EXEC).
void Elf_SetEntry(Elf *elf, uint64_t vaddr)
{
    elf->elf_entry = vaddr;
}

// Sets the object type (ELF_ET_*).
void Elf_SetType(Elf *elf, uint16_t type)
{
    elf->elf_type = type;
}

// Returns the object type (ELF_ET_*).
uint16_t Elf_GetType(const Elf *elf)
{
    return elf->elf_type;
}

// Returns the last error message recorded on the object, or NULL.
const char *Elf_Error(const Elf *elf)
{
    return elf->elf_err;
}

// Appends a new section and returns it.
Elf_Sec *Elf_SectionAdd(Elf *elf, const char *name, uint32_t type, uint64_t flags)
{
    Elf_Sec *sec = calloc(1, sizeof *sec);
    sec->sec_name      = Elf_Intern(elf, name);
    sec->sec_type      = type;
    sec->sec_flags     = flags;
    sec->sec_addralign = 1;
    Elf_BufInit(&sec->sec_data);

    if (elf->elf_nsecs == elf->elf_capsecs) {
        elf->elf_capsecs = elf->elf_capsecs ? elf->elf_capsecs * 2 : 8;
        elf->elf_secs = realloc(elf->elf_secs, elf->elf_capsecs * sizeof *elf->elf_secs);
    }
    elf->elf_secs[elf->elf_nsecs++] = sec;
    return sec;
}

// Finds a section by name, or returns NULL.
Elf_Sec *Elf_SectionFind(Elf *elf, const char *name)
{
    for (size_t i = 0; i < elf->elf_nsecs; i++) {
        if (strcmp(elf->elf_secs[i]->sec_name, name) == 0) {
            return elf->elf_secs[i];
        }
    }
    return NULL;
}

// Finds a section by name, creating it with the given type and flags if absent.
Elf_Sec *Elf_SectionGet(Elf *elf, const char *name, uint32_t type, uint64_t flags)
{
    Elf_Sec *sec = Elf_SectionFind(elf, name);
    if (sec) {
        return sec;
    }
    return Elf_SectionAdd(elf, name, type, flags);
}

// Returns the number of sections.
size_t Elf_SectionCount(const Elf *elf)
{
    return elf->elf_nsecs;
}

// Returns section i.
Elf_Sec *Elf_SectionAt(const Elf *elf, size_t i)
{
    return elf->elf_secs[i];
}

// Returns the byte buffer a section's contents are appended to.
Elf_Buf *Elf_SectionData(Elf_Sec *sec)
{
    return &sec->sec_data;
}

// Places a section at a load address.
void Elf_SectionAddr(Elf_Sec *sec, uint64_t addr)
{
    sec->sec_addr = addr;
}

// Appends a symbol and returns it.  sec == NULL records an undefined reference.
Elf_Sym *Elf_SymbolAdd(Elf *elf, const char *name, Elf_Sec *sec,
                       uint64_t value, uint8_t bind, uint8_t type)
{
    Elf_Sym *sym = calloc(1, sizeof *sym);
    sym->sym_name  = Elf_Intern(elf, name);
    sym->sym_sec   = sec;
    sym->sym_value = value;
    sym->sym_bind  = bind;
    sym->sym_type  = type;

    if (elf->elf_nsyms == elf->elf_capsyms) {
        elf->elf_capsyms = elf->elf_capsyms ? elf->elf_capsyms * 2 : 16;
        elf->elf_syms = realloc(elf->elf_syms, elf->elf_capsyms * sizeof *elf->elf_syms);
    }
    elf->elf_syms[elf->elf_nsyms++] = sym;
    return sym;
}

// Finds a symbol by name, or returns NULL.
Elf_Sym *Elf_SymbolFind(Elf *elf, const char *name)
{
    for (size_t i = 0; i < elf->elf_nsyms; i++) {
        if (strcmp(elf->elf_syms[i]->sym_name, name) == 0) {
            return elf->elf_syms[i];
        }
    }
    return NULL;
}

// Returns the number of symbols.
size_t Elf_SymbolCount(const Elf *elf)
{
    return elf->elf_nsyms;
}

// Returns symbol i.
Elf_Sym *Elf_SymbolAt(const Elf *elf, size_t i)
{
    return elf->elf_syms[i];
}

// Appends a relocation to the section it patches and returns it.
Elf_Rela *Elf_RelaAdd(Elf_Sec *target, uint64_t offset, Elf_Sym *sym,
                      uint32_t type, int64_t addend)
{
    if (target->sec_nrelas == target->sec_caprelas) {
        target->sec_caprelas = target->sec_caprelas ? target->sec_caprelas * 2 : 8;
        target->sec_relas = realloc(target->sec_relas,
                                    target->sec_caprelas * sizeof *target->sec_relas);
    }
    Elf_Rela *rel = &target->sec_relas[target->sec_nrelas++];
    rel->rel_offset = offset;
    rel->rel_sym    = sym;
    rel->rel_type   = type;
    rel->rel_addend = addend;
    return rel;
}

// Returns the number of relocations patching a section.
size_t Elf_RelaCount(const Elf_Sec *target)
{
    return target->sec_nrelas;
}

// Returns relocation i of a section.
Elf_Rela *Elf_RelaAt(const Elf_Sec *target, size_t i)
{
    return (Elf_Rela *) &target->sec_relas[i];
}

// Appends name and a NUL to a string table, returning name's start offset.
uint32_t Elf_WriteStr(Elf_Buf *strtab, const char *name)
{
    uint32_t off = (uint32_t) strtab->eb_len;
    Elf_BufData(strtab, name, strlen(name) + 1);
    return off;
}

// Position of a section within the object, used to fill in section indices.
uint32_t Elf_SectionIndex(const Elf *elf, const Elf_Sec *sec, const uint32_t *secidx)
{
    for (size_t i = 0; i < elf->elf_nsecs; i++) {
        if (elf->elf_secs[i] == sec) {
            return secidx[i];
        }
    }
    return 0;
}

// Builds the .symtab and .strtab bodies, locals before globals, recording indices in slot[].
void Elf_WriteSymtab(const Elf *elf, const uint32_t *secidx, Elf_Buf *symtab,
                            Elf_Buf *strtab, uint32_t *slot, uint32_t *first_global)
{
    Elf64_Sym null = {0};
    Elf_BufData(symtab, &null, sizeof null);
    Elf_BufByte(strtab, 0);

    uint32_t si = 1;
    for (int pass = 0; pass < 2; pass++) {
        uint8_t want = pass == 0 ? ELF_BIND_LOCAL : ELF_BIND_GLOBAL;
        if (pass == 1) {
            *first_global = si;
        }
        for (size_t i = 0; i < elf->elf_nsyms; i++) {
            Elf_Sym *sym  = elf->elf_syms[i];
            uint8_t  bind = sym->sym_bind == ELF_BIND_LOCAL ? ELF_BIND_LOCAL
                                                            : ELF_BIND_GLOBAL;
            if (bind != want) {
                continue;
            }
            slot[i] = si++;
            Elf64_Sym out = {
                .st_name  = Elf_WriteStr(strtab, sym->sym_name),
                .st_info  = ELF_ST_INFO(sym->sym_bind, sym->sym_type),
                .st_other = sym->sym_other,
                .st_size  = sym->sym_size
            };
            if (sym->sym_sec) {
                out.st_shndx = Elf_SectionIndex(elf, sym->sym_sec, secidx);
                out.st_value = sym->sym_value;
            } else {
                out.st_shndx = ELF_SHN_UNDEF;
            }
            Elf_BufData(symtab, &out, sizeof out);
        }
    }
}

// Builds one .rela.* body from a section's relocations, using final indices.
void Elf_WriteRelas(const Elf_Sec *sec, const uint32_t *slot,
                           const Elf *elf, Elf_Buf *out)
{
    for (size_t r = 0; r < sec->sec_nrelas; r++) {
        Elf_Rela *rel = &sec->sec_relas[r];
        uint32_t  symi = 0;
        for (size_t i = 0; i < elf->elf_nsyms; i++) {
            if (elf->elf_syms[i] == rel->rel_sym) {
                symi = slot[i];
                break;
            }
        }
        Elf64_Rela disk = {
            .r_offset = rel->rel_offset,
            .r_info   = ELF_R_INFO(symi, rel->rel_type),
            .r_addend = rel->rel_addend
        };
        Elf_BufData(out, &disk, sizeof disk);
    }
}

// Serializes a relocatable object (ET_REL): sections, .symtab/.strtab, .rela.* and .shstrtab.
int Elf_WriteRel(const Elf *elf, FILE *out)
{
    size_t nuser = elf->elf_nsecs;

    // Phase: assign a section-header index to every output section.
    uint32_t *secidx = calloc(nuser ? nuser : 1, sizeof *secidx);
    uint32_t idx = 1;
    for (size_t i = 0; i < nuser; i++) {
        secidx[i] = idx++;
    }
    uint32_t idx_symtab = idx++;
    uint32_t idx_strtab = idx++;
    uint32_t *relaidx = calloc(nuser ? nuser : 1, sizeof *relaidx);
    for (size_t i = 0; i < nuser; i++) {
        if (elf->elf_secs[i]->sec_nrelas) {
            relaidx[i] = idx++;
        }
    }
    uint32_t idx_shstrtab = idx++;
    uint32_t shnum = idx;

    // Phase: build the symbol table and per-section relocation bodies.
    uint32_t *slot = calloc(elf->elf_nsyms ? elf->elf_nsyms : 1, sizeof *slot);
    uint32_t  first_global = 1;
    Elf_Buf   symtab, strtab, shstr;
    Elf_BufInit(&symtab);
    Elf_BufInit(&strtab);
    Elf_BufInit(&shstr);
    Elf_BufByte(&shstr, 0);
    Elf_WriteSymtab(elf, secidx, &symtab, &strtab, slot, &first_global);

    Elf_Buf *relas = calloc(nuser ? nuser : 1, sizeof *relas);
    for (size_t i = 0; i < nuser; i++) {
        Elf_BufInit(&relas[i]);
        if (relaidx[i]) {
            Elf_WriteRelas(elf->elf_secs[i], slot, elf, &relas[i]);
        }
    }

    // Phase: lay out section headers and their bodies.
    Elf64_Shdr *shdrs   = calloc(shnum, sizeof *shdrs);
    const void **bodies = calloc(shnum, sizeof *bodies);
    uint64_t    *sizes  = calloc(shnum, sizeof *sizes);

    for (size_t i = 0; i < nuser; i++) {
        Elf_Sec *sec = elf->elf_secs[i];
        shdrs[secidx[i]] = (Elf64_Shdr) {
            .sh_name      = Elf_WriteStr(&shstr, sec->sec_name),
            .sh_type      = sec->sec_type,
            .sh_flags     = sec->sec_flags,
            .sh_size      = sec->sec_data.eb_len,
            .sh_addralign = sec->sec_addralign,
            .sh_entsize   = sec->sec_entsize
        };
        bodies[secidx[i]] = sec->sec_data.eb_data;
        sizes[secidx[i]]  = sec->sec_data.eb_len;
    }

    shdrs[idx_symtab] = (Elf64_Shdr) {
        .sh_name      = Elf_WriteStr(&shstr, ".symtab"),
        .sh_type      = ELF_SHT_SYMTAB,
        .sh_size      = symtab.eb_len,
        .sh_link      = idx_strtab,
        .sh_info      = first_global,
        .sh_addralign = 8,
        .sh_entsize   = sizeof(Elf64_Sym)
    };
    bodies[idx_symtab] = symtab.eb_data;
    sizes[idx_symtab]  = symtab.eb_len;

    shdrs[idx_strtab] = (Elf64_Shdr) {
        .sh_name      = Elf_WriteStr(&shstr, ".strtab"),
        .sh_type      = ELF_SHT_STRTAB,
        .sh_size      = strtab.eb_len,
        .sh_addralign = 1
    };
    bodies[idx_strtab] = strtab.eb_data;
    sizes[idx_strtab]  = strtab.eb_len;

    for (size_t i = 0; i < nuser; i++) {
        if (! relaidx[i]) {
            continue;
        }
        char name[64];
        snprintf(name, sizeof name, ".rela%s", elf->elf_secs[i]->sec_name);
        shdrs[relaidx[i]] = (Elf64_Shdr) {
            .sh_name      = Elf_WriteStr(&shstr, name),
            .sh_type      = ELF_SHT_RELA,
            .sh_size      = relas[i].eb_len,
            .sh_link      = idx_symtab,
            .sh_info      = secidx[i],
            .sh_addralign = 8,
            .sh_entsize   = sizeof(Elf64_Rela)
        };
        bodies[relaidx[i]] = relas[i].eb_data;
        sizes[relaidx[i]]  = relas[i].eb_len;
    }

    shdrs[idx_shstrtab] = (Elf64_Shdr) {
        .sh_name      = Elf_WriteStr(&shstr, ".shstrtab"),
        .sh_type      = ELF_SHT_STRTAB,
        .sh_addralign = 1
    };
    bodies[idx_shstrtab] = shstr.eb_data;
    sizes[idx_shstrtab]  = shstr.eb_len;
    shdrs[idx_shstrtab].sh_size = shstr.eb_len;

    // Phase: assign file offsets, then write header, bodies and shdr table.
    uint64_t off = sizeof(Elf64_Ehdr);
    for (uint32_t i = 1; i < shnum; i++) {
        uint64_t align = shdrs[i].sh_addralign ? shdrs[i].sh_addralign : 1;
        off = (off + align - 1) / align * align;
        shdrs[i].sh_offset = off;
        off += sizes[i];
    }
    off = (off + 7) / 8 * 8;
    uint64_t shoff = off;

    Elf64_Ehdr ehdr = {
        .e_ident     = { 0x7F, 'E', 'L', 'F', ELF_CLASS64, ELF_DATA2LSB, ELF_VERSION },
        .e_type      = ELF_ET_REL,
        .e_machine   = elf->elf_machine,
        .e_version   = ELF_VERSION,
        .e_shoff     = shoff,
        .e_ehsize    = sizeof(Elf64_Ehdr),
        .e_shentsize = sizeof(Elf64_Shdr),
        .e_shnum     = (uint16_t) shnum,
        .e_shstrndx  = (uint16_t) idx_shstrtab
    };

    long pos = 0;
    fwrite(&ehdr, sizeof ehdr, 1, out);
    pos += sizeof ehdr;
    for (uint32_t i = 1; i < shnum; i++) {
        while (pos < (long) shdrs[i].sh_offset) {
            fputc(0, out);
            pos++;
        }
        if (sizes[i]) {
            fwrite(bodies[i], 1, sizes[i], out);
        }
        pos += sizes[i];
    }
    while (pos < (long) shoff) {
        fputc(0, out);
        pos++;
    }
    fwrite(shdrs, sizeof(Elf64_Shdr), shnum, out);

    Elf_BufFree(&symtab);
    Elf_BufFree(&strtab);
    Elf_BufFree(&shstr);
    for (size_t i = 0; i < nuser; i++) {
        Elf_BufFree(&relas[i]);
    }
    free(relas);
    free(secidx);
    free(relaidx);
    free(slot);
    free(shdrs);
    free(bodies);
    free(sizes);
    return 0;
}

// Smallest file offset >= pos that is page-congruent with vaddr, as PT_LOAD requires.
uint64_t Elf_PlaceOffset(uint64_t pos, uint64_t vaddr)
{
    return pos + (vaddr - pos) % ELF_PAGE;
}

// Serializes a static executable (ET_EXEC): one R+X PT_LOAD per placed section.
int Elf_WriteExec(const Elf *elf, FILE *out)
{
    // Phase: select the loadable sections.
    Elf_Sec **segs = calloc(elf->elf_nsecs ? elf->elf_nsecs : 1, sizeof *segs);
    uint64_t *offs = calloc(elf->elf_nsecs ? elf->elf_nsecs : 1, sizeof *offs);
    int nseg = 0;
    for (size_t i = 0; i < elf->elf_nsecs; i++) {
        Elf_Sec *sec = elf->elf_secs[i];
        if ((sec->sec_flags & ELF_SHF_ALLOC) && sec->sec_data.eb_len) {
            segs[nseg++] = sec;
        }
    }

    // Phase: assign page-congruent file offsets.
    uint64_t pos = sizeof(Elf64_Ehdr) + (uint64_t) nseg * sizeof(Elf64_Phdr);
    for (int i = 0; i < nseg; i++) {
        offs[i] = Elf_PlaceOffset(pos, segs[i]->sec_addr);
        pos = offs[i] + segs[i]->sec_data.eb_len;
    }

    Elf64_Ehdr ehdr = {
        .e_ident     = { 0x7F, 'E', 'L', 'F', ELF_CLASS64, ELF_DATA2LSB, ELF_VERSION },
        .e_type      = ELF_ET_EXEC,
        .e_machine   = elf->elf_machine,
        .e_version   = ELF_VERSION,
        .e_entry     = elf->elf_entry,
        .e_phoff     = sizeof(Elf64_Ehdr),
        .e_ehsize    = sizeof(Elf64_Ehdr),
        .e_phentsize = sizeof(Elf64_Phdr),
        .e_phnum     = (uint16_t) nseg
    };
    fwrite(&ehdr, sizeof ehdr, 1, out);
    for (int i = 0; i < nseg; i++) {
        Elf64_Phdr phdr = {
            .p_type   = ELF_PT_LOAD,
            .p_flags  = ELF_PF_R | ELF_PF_X,
            .p_offset = offs[i],
            .p_vaddr  = segs[i]->sec_addr,
            .p_paddr  = segs[i]->sec_addr,
            .p_filesz = segs[i]->sec_data.eb_len,
            .p_memsz  = segs[i]->sec_data.eb_len,
            .p_align  = ELF_PAGE
        };
        fwrite(&phdr, sizeof phdr, 1, out);
    }
    long pos2 = sizeof(Elf64_Ehdr) + (long) nseg * sizeof(Elf64_Phdr);
    for (int i = 0; i < nseg; i++) {
        while (pos2 < (long) offs[i]) {
            fputc(0, out);
            pos2++;
        }
        fwrite(segs[i]->sec_data.eb_data, 1, segs[i]->sec_data.eb_len, out);
        pos2 += segs[i]->sec_data.eb_len;
    }

    free(segs);
    free(offs);
    return 0;
}

// Serializes an object to an open stream, returning 0 on success or -1.
int Elf_WriteFile(const Elf *elf, FILE *out)
{
    if (elf->elf_type == ELF_ET_EXEC) {
        return Elf_WriteExec(elf, out);
    }
    return Elf_WriteRel(elf, out);
}

// Serializes an object to a file, returning 0 on success or -1 on error.
int Elf_Write(const Elf *elf, const char *path)
{
    FILE *out = fopen(path, "wb");
    if (! out) {
        return -1;
    }
    int rc = Elf_WriteFile(elf, out);
    fclose(out);
    return rc;
}

// Validates the file header and returns it, or NULL if it is not an ELF object.
const Elf64_Ehdr *Elf_ReadEhdr(const uint8_t *data, size_t n)
{
    if (n < sizeof(Elf64_Ehdr)) {
        return NULL;
    }
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *) data;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') {
        return NULL;
    }
    return eh;
}

// Parses ELF bytes into a new object, or returns NULL on error.
Elf *Elf_ReadMem(const void *buf, size_t n)
{
    const uint8_t    *data = buf;
    const Elf64_Ehdr *eh   = Elf_ReadEhdr(data, n);
    if (! eh) {
        return NULL;
    }
    const Elf64_Shdr *sh    = (const Elf64_Shdr *) (data + eh->e_shoff);
    int               shnum = eh->e_shnum;
    const char       *shstr = (const char *) (data + sh[eh->e_shstrndx].sh_offset);

    Elf *elf = Elf_New(eh->e_type, eh->e_machine);
    elf->elf_entry = eh->e_entry;

    // Phase: reconstruct the model's own sections (skip the synthesized ones).
    Elf_Sec **secmap = calloc(shnum ? shnum : 1, sizeof *secmap);
    for (int i = 1; i < shnum; i++) {
        if (sh[i].sh_type != ELF_SHT_PROGBITS) {
            continue;
        }
        const char *name = shstr + sh[i].sh_name;
        Elf_Sec *sec = Elf_SectionAdd(elf, name, sh[i].sh_type, sh[i].sh_flags);
        sec->sec_addr      = sh[i].sh_addr;
        sec->sec_addralign = sh[i].sh_addralign ? sh[i].sh_addralign : 1;
        sec->sec_entsize   = sh[i].sh_entsize;
        Elf_BufData(&sec->sec_data, data + sh[i].sh_offset, sh[i].sh_size);
        secmap[i] = sec;
    }

    // Phase: rebuild the symbol table, resolving names and defining sections.
    const Elf64_Sym *syms   = NULL;
    int              nsyms  = 0;
    const char      *symstr = NULL;
    for (int i = 0; i < shnum; i++) {
        if (sh[i].sh_type == ELF_SHT_SYMTAB) {
            syms   = (const Elf64_Sym *) (data + sh[i].sh_offset);
            nsyms  = sh[i].sh_size / sizeof(Elf64_Sym);
            symstr = (const char *) (data + sh[sh[i].sh_link].sh_offset);
            break;
        }
    }

    Elf_Sym **symmap = calloc(nsyms ? nsyms : 1, sizeof *symmap);
    for (int i = 1; i < nsyms; i++) {
        const Elf64_Sym *sym  = &syms[i];
        const char      *name = symstr + sym->st_name;
        Elf_Sec         *sec  = (sym->st_shndx != ELF_SHN_UNDEF && sym->st_shndx < shnum)
                                    ? secmap[sym->st_shndx] : NULL;
        symmap[i] = Elf_SymbolAdd(elf, name, sec, sym->st_value,
                                  ELF_ST_BIND(sym->st_info), ELF_ST_TYPE(sym->st_info));
        symmap[i]->sym_size  = sym->st_size;
        symmap[i]->sym_other = sym->st_other;
    }

    // Phase: rebuild relocations, attaching each to the section it patches.
    for (int i = 0; i < shnum; i++) {
        if (sh[i].sh_type != ELF_SHT_RELA) {
            continue;
        }
        Elf_Sec *target = (sh[i].sh_info < (uint32_t) shnum) ? secmap[sh[i].sh_info] : NULL;
        if (! target) {
            continue;
        }
        const Elf64_Rela *rela = (const Elf64_Rela *) (data + sh[i].sh_offset);
        int               nrel = sh[i].sh_size / sizeof(Elf64_Rela);
        for (int r = 0; r < nrel; r++) {
            uint32_t si  = ELF_R_SYM(rela[r].r_info);
            Elf_Sym *sym = (si < (uint32_t) nsyms) ? symmap[si] : NULL;
            Elf_RelaAdd(target, rela[r].r_offset, sym,
                        ELF_R_TYPE(rela[r].r_info), rela[r].r_addend);
        }
    }

    free(secmap);
    free(symmap);
    return elf;
}

// Parses an ELF file into a new object, or returns NULL on error.
Elf *Elf_Read(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (! file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *buf = malloc(size > 0 ? size : 1);
    if (fread(buf, 1, size, file) != (size_t) size) {
        free(buf);
        fclose(file);
        return NULL;
    }
    fclose(file);

    Elf *elf = Elf_ReadMem(buf, size);
    free(buf);
    return elf;
}
