/* main.c - command line driver for cc.
 *
 * Pipeline:   source.c --[flex+bison]--> AST --[codegen]--> x86-64 asm
 *                       --[system assembler+linker]--> executable
 *
 * The lexer and parser do the real compiler work; an external assembler/linker
 * (gcc by default, overridable with $CC) turns our assembly into an ELF binary
 * and pulls in the C runtime.  This mirrors how small compilers such as tcc's
 * early versions or chibicc bootstrap themselves.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"
#include "codegen.h"

extern FILE *yyin;
int          yyparse(void);

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [-S] [-o OUTPUT] INPUT.c\n"
        "  -S          emit assembly only (do not assemble or link)\n"
        "  -o OUTPUT   write the result to OUTPUT (default: a.out / out.s)\n",
        prog);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *input    = NULL;
    const char *output   = NULL;
    int         asm_only = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-S") == 0) {
            asm_only = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) usage(argv[0]);
            output = argv[i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "cc: unknown option '%s'\n", argv[i]);
            usage(argv[0]);
        } else {
            input = argv[i];
        }
    }

    if (!input)
        usage(argv[0]);
    if (!output)
        output = asm_only ? "out.s" : "a.out";

    /* ---- front end: build the AST ---- */
    yyin = fopen(input, "r");
    if (!yyin) {
        perror(input);
        return 1;
    }
    yyparse();
    fclose(yyin);

    /* ---- back end: emit assembly ---- */
    char *asmpath;
    if (asm_only) {
        asmpath = strdup(output);
    } else {
        asmpath = malloc(strlen(output) + 4);
        sprintf(asmpath, "%s.s", output);
    }

    FILE *asmfile = fopen(asmpath, "w");
    if (!asmfile) {
        perror(asmpath);
        return 1;
    }
    codegen(asmfile, program);
    fclose(asmfile);

    if (asm_only)
        return 0;

    /* ---- assemble + link with the system C toolchain ---- */
    const char *driver = getenv("CC");
    if (!driver || !*driver)
        driver = "gcc";

    size_t cmdlen = strlen(driver) + strlen(asmpath) + strlen(output) + 32;
    char  *cmd    = malloc(cmdlen);
    snprintf(cmd, cmdlen, "%s \"%s\" -o \"%s\"", driver, asmpath, output);

    int rc = system(cmd);
    unlink(asmpath);
    free(cmd);
    free(asmpath);

    if (rc != 0) {
        fprintf(stderr, "cc: assembler/linker step failed\n");
        return 1;
    }
    return 0;
}
