/*
 * Filelist handler.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef FILELIST_H
#define FILELIST_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Flat JSON object entry, mapping the literals true, false, and null to the
 * integers 1, 0, and -1 respectively.
 */
typedef struct {
    char *name;
    int state;
} FileState;

/*
 * Parse a flat JSON object, its optional "signature" and "huffman" keys landing
 * in '*outSignature' and '*outHuffman', NULL and false if absent. Names point
 * into '*outText', to be freed.
 */
bool filelist_read(const char *path,
    char **outText,
    const char **outSignature,
    bool *outHuffman,
    FileState **outEntries,
    uint32_t *outCount);

/*
 * Write a flat JSON object, one entry per line, starting with the "signature"
 * key if 'signature' is not NULL, then the "huffman" key if either is set.
 */
bool filelist_write(const char *path,
    const char *signature,
    bool huffman,
    const FileState *entries,
    uint32_t count);

#endif /* FILELIST_H */
