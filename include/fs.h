/*
 * Filesystem and platform helpers.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef FS_H
#define FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#ifdef _WIN32
#define PATH_SEP  "\\"
#define IS_SEP(c) ((c) == '/' || (c) == '\\')
#else
#define PATH_SEP  "/"
#define IS_SEP(c) ((c) == '/')
#endif

#define PATH_SIZE 1024

/*
 * Name of the running program, printed before error messages.
 */
extern const char *programName;

/*
 * Open a file with fopen, or fopen_s on Windows.
 */
FILE *xfopen(const char *path, const char *mode);

/*
 * Read an entire file into memory. The buffer is null-terminated, the
 * terminator is not counted in 'outSize'.
 */
uint8_t *read_file(const char *path, size_t *outSize);

/*
 * Write bytes to a file, creating or truncating it as needed.
 */
bool write_file(const char *path, const uint8_t *data, size_t size);

/*
 * Create a directory if it does not already exist.
 */
bool mkdir_dir(const char *path);

/*
 * Check whether a path exists and whether it is a directory.
 */
bool path_exists(const char *path, bool *isDir);

/*
 * Return a pointer to the filename component of a path, skipping any leading
 * directory segments.
 */
const char *path_basename(const char *path);

/*
 * Join a directory and a name into a single path.
 */
bool join_path(char *dst, size_t dstSize, const char *dir, const char *name);

/*
 * Call 'fn' with the path of each entry of 'dir' ending with 'ext', ignoring
 * case. Return the number of matches, or -1 if the directory cannot be read.
 */
long for_each_file(const char *dir, const char *ext, void (*fn)(const char *));

/*
 * List the input operands, expanding '*' and '?' on Windows as the Microsoft C
 * runtime does. Return NULL if memory runs out after printing the reason.
 */
char **list_inputs(char *const *operands, size_t n, size_t *count);

/*
 * Check for a waiting key press without blocking.
 */
bool key_pressed(void);

/*
 * Read one key press without echo.
 */
int get_key(void);

#endif /* FS_H */
