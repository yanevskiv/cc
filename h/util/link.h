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

// Linking
Elf *Link_Run(const char *const *paths, int npaths, const Link_Options *opts);
void Link_Exec(Elf *elf, const Link_Options *opts);

#endif // LINK_H
