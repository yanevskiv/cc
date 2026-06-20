#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "util/elf.h"
#include "util/link.h"
#include "arch/x86_64/rel.h"

// Virtual address an executable is loaded at, and the page segments align to.
#define LINK_BASE 0x400000
#define LINK_PAGE 0x1000

// Index of a section within an object, or -1 if it holds none.
long Link_SectionIndex(const Elf *elf, const Elf_Sec *target)
{
    for (size_t i = 0; i < Elf_SectionCount(elf); i++) {
        if (Elf_SectionAt(elf, i) == target) {
            return (long) i;
        }
    }
    return -1;
}

// Index of a symbol within an object, or -1 if it holds none.
long Link_SymbolIndex(const Elf *elf, const Elf_Sym *target)
{
    for (size_t i = 0; i < Elf_SymbolCount(elf); i++) {
        if (Elf_SymbolAt(elf, i) == target) {
            return (long) i;
        }
    }
    return -1;
}

// Finds an existing global symbol by name, or returns NULL.
Elf_Sym *Link_FindGlobal(Elf *elf, const char *name)
{
    for (size_t i = 0; i < Elf_SymbolCount(elf); i++) {
        Elf_Sym *sym = Elf_SymbolAt(elf, i);
        if (sym->sym_bind != ELF_BIND_LOCAL && strcmp(sym->sym_name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

// Merges one input object into the output, unifying globals and rebasing relocations.
void Link_Merge(Elf *out, Elf *in)
{
    size_t nsec = Elf_SectionCount(in);
    size_t nsym = Elf_SymbolCount(in);

    Elf_Sec **secmap  = calloc(nsec ? nsec : 1, sizeof *secmap);
    uint64_t *secbase = calloc(nsec ? nsec : 1, sizeof *secbase);
    Elf_Sym **symmap  = calloc(nsym ? nsym : 1, sizeof *symmap);

    // Phase: merge section bytes, recording each input section's new base.
    for (size_t i = 0; i < nsec; i++) {
        Elf_Sec *sec   = Elf_SectionAt(in, i);
        Elf_Sec *dst = Elf_SectionGet(out, sec->sec_name, sec->sec_type, sec->sec_flags);
        Elf_Buf *db  = Elf_SectionData(dst);
        if (sec->sec_addralign > dst->sec_addralign) {
            dst->sec_addralign = sec->sec_addralign;
        }
        Elf_BufAlign(db, sec->sec_addralign);
        secbase[i] = db->eb_len;
        Elf_BufData(db, sec->sec_data.eb_data, sec->sec_data.eb_len);
        secmap[i] = dst;
    }

    // Phase: copy symbols, unifying globals and resolving undefined references.
    for (size_t i = 0; i < nsym; i++) {
        Elf_Sym *sym   = Elf_SymbolAt(in, i);
        Elf_Sec *dsec  = NULL;
        uint64_t value = 0;
        if (sym->sym_sec) {
            long j = Link_SectionIndex(in, sym->sym_sec);
            dsec  = secmap[j];
            value = secbase[j] + sym->sym_value;
        }

        if (sym->sym_bind == ELF_BIND_LOCAL) {
            symmap[i] = Elf_SymbolAdd(out, sym->sym_name, dsec, value,
                                      ELF_BIND_LOCAL, sym->sym_type);
            continue;
        }

        Elf_Sym *existing = Link_FindGlobal(out, sym->sym_name);
        if (! existing) {
            symmap[i] = Elf_SymbolAdd(out, sym->sym_name, dsec, value,
                                      sym->sym_bind, sym->sym_type);
            continue;
        }
        if (dsec) {
            if (existing->sym_sec) {
                Show_Error("multiple definition of '%s'", sym->sym_name);
            }
            existing->sym_sec   = dsec;
            existing->sym_value = value;
            existing->sym_type  = sym->sym_type;
        }
        symmap[i] = existing;
    }

    // Phase: rebase each relocation onto the merged section and out symbol.
    for (size_t i = 0; i < nsec; i++) {
        Elf_Sec *sec = Elf_SectionAt(in, i);
        for (size_t r = 0; r < Elf_RelaCount(sec); r++) {
            Elf_Rela *rel = Elf_RelaAt(sec, r);
            long      k   = Link_SymbolIndex(in, rel->rel_sym);
            if (k < 0) {
                continue;
            }
            Elf_RelaAdd(secmap[i], secbase[i] + rel->rel_offset, symmap[k],
                        rel->rel_type, rel->rel_addend);
        }
    }

    free(secmap);
    free(secbase);
    free(symmap);
}

// Load address requested for a section by name, or 0 if it is unplaced.
uint64_t Link_PlacedAddr(const Link_Options *opts, const char *name, int *placed)
{
    for (int i = 0; i < opts->lo_nplaces; i++) {
        if (strcmp(opts->lo_places[i].lp_name, name) == 0) {
            *placed = 1;
            return opts->lo_places[i].lp_addr;
        }
    }
    *placed = 0;
    return 0;
}

// Assigns each allocatable section its -place address, else the next free page.
void Link_PlaceSections(Elf *elf, const Link_Options *opts)
{
    uint64_t next = LINK_BASE + LINK_PAGE;
    for (size_t i = 0; i < Elf_SectionCount(elf); i++) {
        Elf_Sec *sec = Elf_SectionAt(elf, i);
        if (! (sec->sec_flags & ELF_SHF_ALLOC)) {
            continue;
        }
        int      placed;
        uint64_t addr = Link_PlacedAddr(opts, sec->sec_name, &placed);
        if (! placed) {
            addr = next;
        }
        Elf_SectionAddr(sec, addr);
        uint64_t end = addr + sec->sec_data.eb_len;
        if (end > next) {
            next = (end + LINK_PAGE - 1) / LINK_PAGE * LINK_PAGE;
        }
    }
}

// Aborts if any relocation references a symbol that was never defined.
void Link_CheckDefined(Elf *elf)
{
    for (size_t i = 0; i < Elf_SectionCount(elf); i++) {
        Elf_Sec *sec = Elf_SectionAt(elf, i);
        for (size_t r = 0; r < Elf_RelaCount(sec); r++) {
            Elf_Sym *sym = Elf_RelaAt(sec, r)->rel_sym;
            if (! sym || ! sym->sym_sec) {
                Show_Error("undefined symbol '%s'", sym ? sym->sym_name : "?");
            }
        }
    }
}

// Finalizes an in-memory object into a static executable.
void Link_Exec(Elf *elf, const Link_Options *opts)
{
    const char *entry = opts->lo_entry ? opts->lo_entry : "_start";

    Link_PlaceSections(elf, opts);
    Link_CheckDefined(elf);

    Elf_Sym *sym = Elf_SymbolFind(elf, entry);
    if (! sym || ! sym->sym_sec) {
        Show_Error("undefined entry symbol '%s'", entry);
    }
    Elf_SetEntry(elf, sym->sym_sec->sec_addr + sym->sym_value);

    Rel_x86_64_Apply(elf);
    Elf_SetType(elf, ELF_ET_EXEC);
}

// Reads and links the given objects into one Elf.
Elf *Link_Run(const char *const *paths, int npaths, const Link_Options *opts)
{
    Elf *out = Elf_New(ELF_ET_REL, ELF_EM_X86_64);
    for (int i = 0; i < npaths; i++) {
        Elf *in = Elf_Read(paths[i]);
        if (! in) {
            Show_Error("cannot read object '%s'", paths[i]);
        }
        Link_Merge(out, in);
        Elf_Free(in);
    }

    if (! opts->lo_relocatable) {
        Link_Exec(out, opts);
    }
    return out;
}
