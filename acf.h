// Copyright (c) 2026 SombrAbsol

#ifndef ACF_H
#define ACF_H

#include <stdint.h>

typedef struct {
    char magic[4];       // "acf\0"
    uint32_t headerSize; // usually 0x20?
    uint32_t dataStart;
    uint32_t numFiles;
    uint32_t unknown1;   // always 1
    uint32_t unknown2;   // always 0x32
    uint32_t padding[2];
} ACFHeader;

typedef struct {
    uint32_t relativeOffset;
    uint32_t outputSize;
    uint32_t inputSize;
} FATEntry;

// acf operations
int extract_acf(const char *path);
void process_directory(const char *directory);
int build_acf(const char *directory);

#endif // ACF_H
