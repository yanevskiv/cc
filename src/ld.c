#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "util/elf.h"

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
        "  -place=SEC@ADDR    place output section SEC (text|data) at ADDR\n",
        prog);
    exit(1);
}

// Parses a -place=SEC@ADDR argument into opts, or aborts on a malformed value.
static void Ld_ParsePlace(const char *spec, Elf_LinkOptions *opts)
{
    const char *at = strchr(spec, '@');
    if (! at) {
        Show_Error("malformed -place (expected SEC@ADDR): '%s'", spec);
    }
    uint64_t addr = strtoull(at + 1, NULL, 0);
    int      len  = (int) (at - spec);

    if (strncmp(spec, "text", len) == 0 && len == 4) {
        opts->eo_place_text = 1;
        opts->eo_text_addr  = addr;
    } else if ((strncmp(spec, "data", len) == 0 || strncmp(spec, "rodata", len) == 0)
               && (len == 4 || len == 6)) {
        opts->eo_place_data = 1;
        opts->eo_data_addr  = addr;
    } else {
        Show_Error("unknown -place section (expected text|data): '%s'", spec);
    }
}

// Main function
int main(int argc, char **argv)
{
    const char     *output = LD_DEFAULT_OUTPUT;
    Elf_LinkOptions opts   = {0};

    // ld's flags (-r, -place=) use the single-dash forms its roadmap spells
    // out, so the arguments are walked by hand rather than through getopt.
    const char **objs = calloc(argc, sizeof *objs);
    int          nobjs = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(arg, "-e") == 0 && i + 1 < argc) {
            opts.eo_entry = argv[++i];
        } else if (strcmp(arg, "-r") == 0) {
            opts.eo_relocatable = 1;
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

    // The link core is short enough to drive inline: open the output, hand the
    // objects to Elf_*, then mark executables runnable (-r leaves an object).
    FILE *out = fopen(output, "wb");
    if (! out) {
        perror(output);
        return 1;
    }
    Elf_Link((const char *const *) objs, nobjs, &opts, out);
    fclose(out);
    if (! opts.eo_relocatable) {
        chmod(output, LD_MODE);
    }

    free(objs);
    return 0;
}
