#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "ast/ast.h"
#include "util/str.h"
#include "arch/x86_64/gen.h"

// Permission bits for the executables cc writes (rwxr-xr-x).
#define ELF_MODE 0755

// Default output name for a freestanding executable.
#define DEFAULT_OUTPUT "a.out"

// Target architecture selected when no -march= is given.
#define DEFAULT_ARCH "x86_64"

// Machine-option prefix recognised inside -m (e.g. -march=x86_64).
#define MARCH_PREFIX "arch="

// Input stream read by the generated lexer.
extern FILE *yyin;

// Entry point of the generated parser; fills in Ast_Program.
int yyparse(void);

// Show usage information and exits.
static void Show_Usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] INPUT.c\n"
        "  -o OUTPUT   write output to OUTPUT (default: " DEFAULT_OUTPUT ")\n"
        "  -S          write assembly text instead of an executable\n"
        "  -march=ARCH target architecture (default: " DEFAULT_ARCH ")\n",
        prog);
    exit(1);
}

// Main function
int main(int argc, char **argv)
{
    const char *output = NULL;
    const char *arch = DEFAULT_ARCH;
    int emit_text = 0;

    int opt;
    while ((opt = getopt(argc, argv, "o:cESgI:D:U:l:L:W:f:m:O::")) != -1) {
        switch (opt) {
            case 'o': {
                output = optarg;
            } break;
            case 'S': {
                emit_text = 1;
            } break;
            case 'm': {
                // -m carries machine options; only -march=ARCH is recognised.
                if (Str_StartsWith(optarg, MARCH_PREFIX)) {
                    arch = optarg + strlen(MARCH_PREFIX);
                }
            } break;
            case 'c': case 'E': case 'g':
            case 'I': case 'D': case 'U': case 'l':
            case 'L': case 'W': case 'f':
            case 'O': {
                // Recognised compiler flag with no effect here; ignore it.
            } break;
        }
    }

    if (! Str_Equals(arch, DEFAULT_ARCH)) {
        Show_Error("unsupported architecture '%s' (only " DEFAULT_ARCH " is supported)", arch);
    }

    if (optind >= argc) {
        Show_Usage(argv[0]);
    }
    const char *input = argv[optind];

    char *outbuf = NULL;
    if (! output) {
        output = outbuf = emit_text ? Str_ChangeOrAppendExt(input, ".s")
                                    : strdup(DEFAULT_OUTPUT);
    }

    int result = 0;

    // Front end: build the AST
    yyin = fopen(input, "r");
    if (! yyin) {
        perror(input);
        result = 1;
        goto cleanup;
    }
    yyparse();
    fclose(yyin);

    // Back end: emit assembly text or a freestanding executable
    FILE *out = fopen(output, emit_text ? "w" : "wb");
    if (! out) {
        perror(output);
        result = 1;
        goto cleanup;
    }
    if (emit_text) {
        Gen_x86_64_CodegenAsm(out, Ast_Program);
    } else {
        Gen_x86_64_CodegenElf(out, Ast_Program);
    }
    fclose(out);

    if (! emit_text) {
        chmod(output, ELF_MODE);
    }

cleanup:
    free(outbuf);
    return result;
}
