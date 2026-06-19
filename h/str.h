#ifndef STR_H
#define STR_H

#include <stdarg.h>

// Returns a freshly allocated string formatted like printf(3).
char *Str_Format(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Returns a freshly allocated string formatted from a va_list.
char *Str_VFormat(const char *fmt, va_list ap) __attribute__((format(printf, 1, 0)));

// Changes or appends a file extension ('main.c' -> 'main.s').
char *Str_ChangeOrAppendExt(const char *input, const char *suffix);

#endif // STR_H
