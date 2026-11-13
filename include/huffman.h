/*
 * Huffman compression and decompression.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

/*
 * Decompress a Huffman buffer of 4-bit or 8-bit symbols. Return NULL without
 * printing anything if it is not a valid Huffman stream or memory runs out.
 */
uint8_t *huffman_decompress(
    const uint8_t *src, size_t srcSize, size_t *outSize);

/*
 * Compress a buffer with Huffman, using 8-bit symbols and a tree laid out as in
 * the game's own files. Return NULL on failure after printing the reason.
 */
uint8_t *huffman_compress(const uint8_t *src, size_t srcSize, size_t *outSize);

#endif /* HUFFMAN_H */
