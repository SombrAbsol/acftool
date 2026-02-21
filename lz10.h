// Copyright (c) 2026 SombrAbsol

#ifndef LZ10_H
#define LZ10_H

#include <stddef.h>
#include <stdint.h>

// lz10 (de)compression
uint8_t *lz10_decompress(const uint8_t *src, size_t srcSize, size_t *outSize);
uint8_t *lz10_compress(const uint8_t *src, size_t srcSize, size_t *outSize);

#endif // LZ10_H
