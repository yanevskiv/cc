#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "util/elf.h"
#include "util/link.h"

// Permission bits for the executable ld writes (rwxr-xr-x).
#define LD_MODE 0755

// Default output name when no -o is given.
#define LD_DEFAULT_OUTPUT "a.out"

// Shows usage information and exits.
static void Ld_Usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] INPUT.o...\n"
        "  -o OUTPUT          write the output to OUTPUT (default: " LD_DEFAULT_OUTPUT ")\n"
        "  -e ENTRY           set the entry symbol (default: _start)\n"
        "  -r                 merge the inputs into one relocatable object\n"
        "  -place=SEC@ADDR    place output section SEC at ADDR (text/data name a\n"
        "                     default section; any other name is used verbatim)\n",
        prog);
    exit(1);
}

// Maps a -place section name to its ELF section: text -> .text, data/rodata ->
// .rodata, anything else is taken as a literal section name.
static char *Ld_PlaceName(const char *spec, int len)
{
    char *name = strndup(spec, len);
    if (strcmp(name, "text") == 0) {
        free(name);
        return strdup(".text");
    }
    if (strcmp(name, "data") == 0 || strcmp(name, "rodata") == 0) {
        free(name);
        return strdup(".rodata");
    }
    return name;
}

// Parses a -place=SEC@ADDR argument into opts, or aborts on a malformed value.
static void Ld_ParsePlace(const char *spec, Link_Options *opts)
{
    const char *at = strchr(spec, '@');
    if (! at) {
        Show_Error("malformed -place (expected SEC@ADDR): '%s'", spec);
    }
    if (opts->lo_nplaces >= LINK_MAX_PLACE) {
        Show_Error("too many -place options (max %d)", LINK_MAX_PLACE);
    }
    opts->lo_places[opts->lo_nplaces].lp_name = Ld_PlaceName(spec, (int) (at - spec));
    opts->lo_places[opts->lo_nplaces].lp_addr = strtoull(at + 1, NULL, 0);
    opts->lo_nplaces++;
}

// Main function
int main(int argc, char **argv)
{
    const char  *output = LD_DEFAULT_OUTPUT;
    Link_Options opts    = {0};

    // ld's flags (-r, -place=) use the single-dash forms its roadmap spells
    // out, so the arguments are walked by hand rather than through getopt.
    const char **objs = calloc(argc, sizeof *objs);
    int          nobjs = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(arg, "-e") == 0 && i + 1 < argc) {
            opts.lo_entry = argv[++i];
        } else if (strcmp(arg, "-r") == 0) {
            opts.lo_relocatable = 1;
        } else if (strncmp(arg, "-place=", 7) == 0) {
            Ld_ParsePlace(arg + 7, &opts);
        } else if (arg[0] == '-') {
            Ld_Usage(argv[0]);
        } else {
            objs[nobjs++] = arg;
        }
    }

    if (nobjs == 0) {
        Ld_Usage(argv[0]);
    }

    // Link the inputs into one Elf, write it, then mark executables runnable
    // (-r leaves a relocatable object, which stays non-executable).
    Elf *e = Link_Run((const char *const *) objs, nobjs, &opts);
    if (Elf_Write(e, output) != 0) {
        perror(output);
        return 1;
    }
    Elf_Free(e);
    if (! opts.lo_relocatable) {
        chmod(output, LD_MODE);
    }

    free(objs);
    return 0;
}
