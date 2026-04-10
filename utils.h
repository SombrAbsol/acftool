// Copyright (c) 2026 SombrAbsol

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#endif

// use fopen or fopen_s
FILE *xfopen(const char *path, const char *mode);

// padding
size_t pad_size(size_t size, size_t align);

// strdup implementation
char *xstrdup(const char *s);

// read an entire file into memory
uint8_t *read_file(const char *path, size_t *outSize);

// write data to a file
int write_file(const char *path, const uint8_t *data, size_t size);

// create a directory
int mkdir_dir(const char *path);

// extension helpers
int is_invertible(const char *ext);
void reverse_str_inplace(char *s);
const char *try_get_extension(
    const uint8_t *data, size_t size,
    int maxlength, int minlength,
    const char *defaultExt, char *outExt, size_t outExtSz
);

// json helpers
char *escape_json_string(const char *s, size_t maxlen);
char *unescape_json_string(const char *start, size_t len);

// read/write flat json item
int read_json_file_states(
    const char *path,
    char ***outNames,
    int **outStates,
    uint32_t *outCount
);
int write_json_file_states(
    const char *path,
    char *const *names,
    const int *states,
    uint32_t count
);

// free an array of heap-allocated strings
void free_string_array(char **strings, uint32_t count);

#endif // UTILS_H
