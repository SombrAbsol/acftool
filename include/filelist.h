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
 * Parse a flat JSON object. Entry names point into '*outText', which must be
 * freed along with '*outEntries'.
 */
bool filelist_read(const char *path,
    char **outText,
    FileState **outEntries,
    uint32_t *outCount);

/*
 * Write a flat JSON object, one entry per line.
 */
bool filelist_write(const char *path, const FileState *entries, uint32_t count);

#endif /* FILELIST_H */
