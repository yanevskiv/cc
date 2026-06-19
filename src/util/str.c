#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/str.h"

// Returns a freshly allocated string formatted from a va_list.
char *Str_VFormat(const char *fmt, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);

    int len = vsnprintf(NULL, 0, fmt, ap);
    char *out = malloc(len + 1);
    vsnprintf(out, len + 1, fmt, ap2);
    va_end(ap2);
    return out;
}

// Returns a freshly allocated string formatted like printf(3).
char *Str_Format(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *out = Str_VFormat(fmt, ap);
    va_end(ap);
    return out;
}

// Returns nonzero if the two strings are equal.
int Str_Equals(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

// Returns nonzero if str begins with prefix.
int Str_StartsWith(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Changes or appends a file extension ('main.c' -> 'main.s').
char *Str_ChangeOrAppendExt(const char *input, const char *suffix)
{
    const char *slash = strrchr(input, '/');
    const char *dot   = strrchr(input, '.');

    if (!dot || (slash && dot < slash)) {
        dot = NULL;
    }

    size_t stem = dot ? (size_t)(dot - input) : strlen(input);
    size_t slen = strlen(suffix);
    char  *out  = malloc(stem + slen + 1);
    memcpy(out, input, stem);
    memcpy(out + stem, suffix, slen + 1);
    return out;
}
