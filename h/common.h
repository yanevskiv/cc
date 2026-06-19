#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

// Prints a diagnostic and exits; shared by the lexer, parser and back end.
#define Show_Error(...)                 \
    do {                                \
        fprintf(stderr, "cc: error: "); \
        fprintf(stderr, __VA_ARGS__);   \
        fprintf(stderr, "\n");          \
        exit(1);                        \
    } while (0)

// Prints a warning diagnostic and continues; shared by the lexer, parser and back end.
#define Show_Warning(...)                 \
    do {                                  \
        fprintf(stderr, "cc: warning: "); \
        fprintf(stderr, __VA_ARGS__);     \
        fprintf(stderr, "\n");            \
    } while (0)

#endif // COMMON_H
