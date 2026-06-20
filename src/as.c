#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "util/elf.h"
#include "util/file.h"
#include "util/str.h"
#include "arch/x86_64/enc.h"
#include "arch/x86_64/txt.h"

// Shows usage information and exits.
static void As_Usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] INPUT.s\n"
        "  -o OUTPUT   write the object to OUTPUT (default: INPUT.o)\n",
        prog);
    exit(1);
}

// Reads AT&T assembly from input and writes a relocatable object to output.
static void As_Assemble(const char *input, const char *output)
{
    char *text = File_GetContent(input, NULL);
    if (! text) {
        perror(input);
        exit(1);
    }
    Txt_x86_64_Att_Parse(text);
    free(text);

    Elf *elf = Enc_x86_64_Object();
    if (Elf_Write(elf, output) != 0) {
        perror(output);
        exit(1);
    }
    Elf_Free(elf);
}

// Main function
int main(int argc, char **argv)
{
    const char *output = NULL;
    const char *input  = NULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-o") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (arg[0] == '-' && arg[1]) {
            As_Usage(argv[0]);
        } else {
            input = arg;
        }
    }

    if (! input) {
        As_Usage(argv[0]);
    }

    char *outbuf = NULL;
    if (! output) {
        output = outbuf = Str_ChangeOrAppendExt(input, ".o");
    }

    As_Assemble(input, output);
    free(outbuf);
    return 0;
}
