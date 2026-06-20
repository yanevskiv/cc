#include <regex.h>
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

// Trims leading and trailing whitespace in place, returning the new start.
char *Str_Trim(char *str)
{
    while (*str && strchr(" \t\r\n\f\v", *str)) {
        str++;
    }
    char *end = str + strlen(str);
    while (end > str && strchr(" \t\r\n\f\v", end[-1])) {
        *--end = '\0';
    }
    return str;
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

// Splits str on each occurrence of sep into a list of owned pieces.
Str_List Str_Split(const char *str, const char *sep)
{
    Str_List list = { NULL, 0 };
    size_t seplen = strlen(sep);

    const char *start = str;
    for (;;) {
        const char *hit = strstr(start, sep);
        size_t len = hit ? (size_t) (hit - start) : strlen(start);

        list.sl_items = realloc(list.sl_items, (list.sl_count + 1) * sizeof *list.sl_items);
        list.sl_items[list.sl_count++] = strndup(start, len);

        if (! hit) {
            break;
        }
        start = hit + seplen;
    }
    return list;
}

// Frees every piece of a list and clears it.
void Str_ListFree(Str_List *list)
{
    for (int i = 0; i < list->sl_count; i++) {
        free(list->sl_items[i]);
    }
    free(list->sl_items);
    list->sl_items = NULL;
    list->sl_count = 0;
}

// Returns nonzero if the extended regex pattern matches anywhere in str.
int Str_RegexMatch(const char *str, const char *pattern)
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0) {
        return 0;
    }
    int ok = regexec(&re, str, 0, NULL, 0) == 0;
    regfree(&re);
    return ok;
}

// Matches pattern against str, filling groups[0..ngroups-1] with owned capture
// text (NULL where a group did not participate); returns nonzero on a match.
int Str_RegexExtract(const char *str, const char *pattern, char **groups, int ngroups)
{
    for (int i = 0; i < ngroups; i++) {
        groups[i] = NULL;
    }

    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED) != 0) {
        return 0;
    }

    regmatch_t *match = calloc(ngroups + 1, sizeof *match);
    int ok = regexec(&re, str, ngroups + 1, match, 0) == 0;
    if (ok) {
        for (int i = 0; i < ngroups; i++) {
            regmatch_t *m = &match[i + 1];
            if (m->rm_so >= 0) {
                groups[i] = strndup(str + m->rm_so, m->rm_eo - m->rm_so);
            }
        }
    }
    free(match);
    regfree(&re);
    return ok;
}
