#ifndef STR_H
#define STR_H

#include <stdarg.h>

// String utility functions
char *Str_Format(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *Str_VFormat(const char *fmt, va_list ap) __attribute__((format(printf, 1, 0)));
char *Str_ChangeOrAppendExt(const char *input, const char *suffix);
int Str_Equals(const char *a, const char *b);
int Str_StartsWith(const char *str, const char *prefix);

#endif // STR_H
