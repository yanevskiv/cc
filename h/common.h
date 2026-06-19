#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

// Prints a diagnostic and exits; shared by the lexer, parser and back end.
#define error(...)                      \
    do {                                \
        fprintf(stderr, "cc: error: "); \
        fprintf(stderr, __VA_ARGS__);   \
        fprintf(stderr, "\n");          \
        exit(1);                        \
    } while (0)

#endif // COMMON_H
