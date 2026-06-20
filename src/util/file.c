#include <stdio.h>
#include <stdlib.h>
#include "util/file.h"

// Reads the whole file into a malloc'd, NUL-terminated buffer; len is optional.
char *File_GetContent(const char *path, long *len)
{
    FILE *file = fopen(path, "rb");
    if (! file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (fread(buf, 1, size, file) != (size_t) size) {
        free(buf);
        fclose(file);
        return NULL;
    }
    fclose(file);

    buf[size] = '\0';
    if (len) {
        *len = size;
    }
    return buf;
}

// Writes len bytes to path, replacing it; returns 0 on success or -1 on error.
int File_PutContent(const char *path, const void *data, long len)
{
    FILE *file = fopen(path, "wb");
    if (! file) {
        return -1;
    }
    int ok = fwrite(data, 1, len, file) == (size_t) len;
    fclose(file);
    return ok ? 0 : -1;
}
