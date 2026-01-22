// Copyright (c) 2026 SombrAbsol

#ifndef ACFDUMP_H
#define ACFDUMP_H

#include <stdint.h>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #include <sys/types.h>
    #define mkdir_dir(name) _mkdir(name)
#else
    #include <dirent.h>
    #include <unistd.h>
    #define mkdir_dir(name) mkdir(name, 0755)
#endif

typedef struct {
    char magic[4];       // "acf\0"
    uint32_t headerSize; // usually 0x20
    uint32_t dataStart;
    uint32_t numFiles;
    uint32_t unknown1;   // always 1?
    uint32_t unknown2;   // always 0x32?
    uint32_t padding[2];
} ACFHeader;

typedef struct {
    uint32_t relativeOffset;
    uint32_t outputSize;
    uint32_t inputSize;
} FATEntry;

// padding
size_t pad_size(size_t size, size_t align);

// lz10 decompression
uint8_t *lz10_decompress(const uint8_t *src, size_t srcSize, size_t *outSize);

// file helpers
uint8_t *read_file(const char *path, size_t *outSize);
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

// acf operations
int extract_acf(const char *path);
void process_directory(const char *directory);
int build_acf(const char *directory);

#endif /* ACFDUMP_H */
