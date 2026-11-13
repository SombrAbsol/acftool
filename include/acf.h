/*
 * ACF archive extraction and building.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ACF_H
#define ACF_H

#include <stdbool.h>

/*
 * Verbosity level: 0 quiet, 1 normal.
 */
extern int verbose;

/*
 * Extract all files from an ACF archive into a directory, along with the
 * filelist.json file listing them.
 */
bool acf_extract(const char *path, const char *outDir);

/*
 * Pack the contents of a directory into a new ACF archive, guided by the
 * filelist.json file found inside the directory.
 */
bool acf_build(const char *directory, const char *outPath);

#endif /* ACF_H */
