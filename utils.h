// Copyright (c) 2026 SombrAbsol

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #include <sys/types.h>
    #define mkdir_dir(name) _mkdir(name)
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #define mkdir_dir(name) mkdir(name, 0755)
#endif

// padding
size_t pad_size(size_t size, size_t align);

// read entire file into memory
uint8_t *read_file(const char *path, size_t *outSize);

// write data to a file
int write_file(const char *path, const uint8_t *data, size_t size);

// extension helpers
int is_invertible(const char *ext);
void reverse_str_inplace(char *s);
const char *try_get_extension(
    const uint8_t *data,
    size_t size,
    int maxlength,
    int minlength,
    const char *defaultExt,
    char *outExt,
    size_t outExtSz
);

#endif // UTILS_H
