#ifndef STR_H
#define STR_H

#include <stdarg.h>

// A list of owned strings, as produced by Str_Split.
typedef struct {
    char **sl_items;
    int    sl_count;
} Str_List;

// String utility functions
char *Str_Format(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *Str_VFormat(const char *fmt, va_list ap) __attribute__((format(printf, 1, 0)));
char *Str_ChangeOrAppendExt(const char *input, const char *suffix);
int Str_Equals(const char *a, const char *b);
int Str_StartsWith(const char *str, const char *prefix);
char *Str_Trim(char *str);

// Splitting a string into an owned list of pieces
Str_List Str_Split(const char *str, const char *sep);
void     Str_ListFree(Str_List *list);

// POSIX extended-regex matching over a whole string
int Str_RegexMatch(const char *str, const char *pattern);
int Str_RegexExtract(const char *str, const char *pattern, char **groups, int ngroups);

#endif // STR_H
