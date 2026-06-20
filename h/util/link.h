#ifndef LINK_H
#define LINK_H

#include <stdint.h>
#include "util/elf.h"

// Most -place requests one link may carry.
#define LINK_MAX_PLACE 16

// One -place request: load the named section at a fixed address.
typedef struct {
    const char *lp_name;
    uint64_t    lp_addr;
} Link_Place;

// Options controlling a link.
typedef struct {
    const char *lo_entry;        // entry symbol (NULL selects _start)
    int         lo_relocatable;  // -r: merge into an ET_REL object, keep relocs
    Link_Place  lo_places[LINK_MAX_PLACE];
    int         lo_nplaces;
} Link_Options;

// Object indices and global lookup
long     Link_SectionIndex(const Elf *elf, const Elf_Sec *target);
long     Link_SymbolIndex(const Elf *elf, const Elf_Sym *target);
Elf_Sym *Link_FindGlobal(Elf *elf, const char *name);

// Merging objects
void Link_Merge(Elf *out, Elf *in);

// Placing sections and checking symbols
uint64_t Link_PlacedAddr(const Link_Options *opts, const char *name, int *placed);
void     Link_PlaceSections(Elf *elf, const Link_Options *opts);
void     Link_CheckDefined(Elf *elf);

// Linking
void Link_Exec(Elf *elf, const Link_Options *opts);
Elf *Link_Run(const char *const *paths, int npaths, const Link_Options *opts);

#endif // LINK_H
