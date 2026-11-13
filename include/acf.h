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
 * Extract all files from an ACF archive, Huffman-compressed or not, into a
 * directory, along with the filelist.json file listing them.
 */
bool acf_extract(const char *path, const char *outDir);

/*
 * Pack the contents of a directory into a new ACF archive, guided by the
 * filelist.json file found inside the directory, Huffman-compressed if asked
 * or if the file lists it so.
 */
bool acf_build(const char *directory, const char *outPath, bool huffman);

/*
 * Check whether the filelist.json file of a directory lists its archive as
 * Huffman-compressed. Return -1 if it is missing or invalid, after printing it.
 */
int acf_list_huffman(const char *dir);

/*
 * Check whether a file is compressed with Huffman coding, as the .acz map
 * archives of Guardian Signs outside Japan.
 */
bool acf_is_huffman(const char *path);

/*
 * Compress an ACF archive file with Huffman coding, as the game does. Return
 * false on failure, after printing the reason.
 */
bool acf_compress(const char *path, const char *outPath);

#endif /* ACF_H */
