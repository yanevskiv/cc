#ifndef FILE_H
#define FILE_H

// Whole-file read and write
char *File_GetContent(const char *path, long *len);
int   File_PutContent(const char *path, const void *data, long len);

#endif // FILE_H
