/*
 * Utility functions.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#endif

/*
 * Cross-platform wrapper around fopen. Use fopen_s on Windows, or fopen
 * otherwise.
 */
FILE *xfopen(const char *path, const char *mode);

/*
 * Compute the number of padding bytes required to align 'n' to the next
 * 4-byte boundary.
 */
uint32_t pad4(uint32_t n);

/*
 * strdup replacement using malloc.
 */
char *xstrdup(const char *s);

/*
 * Read an entire file into memory.
 */
uint8_t *read_file(const char *path, size_t *outSize);

/*
 * Write bytes to a file, creating or truncating it as needed.
 */
int write_file(const char *path, const uint8_t *data, size_t size);

/*
 * Create a directory if it does not already exist.
 */
int mkdir_dir(const char *path);

/*
 * Check whether a file extension matches one of the known Nintendo container
 * magic strings whose bytes appear reversed in the file header, or not.
 */
int is_invertible(const char *ext);

/*
 * Reverse a null-terminated string in place.
 */
void reverse_str_inplace(char *s);

/*
 * Attempt to derive a file extension from the leading bytes of data.
 */
const char *try_get_extension(const uint8_t *data, size_t size, int maxlength,
                              int minlength, const char *defaultExt,
                              char *outExt, size_t outExtSz);

/*
 * (Un)escape a set of characters for JSON output.
 */
char *escape_json_string(const char *s, size_t maxlen);
char *unescape_json_string(const char *start, size_t len);

/*
 * Parse/write a flat JSON object, mapping the literals true, false, and null to
 * the integers 1, 0, and -1 respectively.
 */
int read_json_file_states(const char *path, char ***outNames, int **outStates,
                          uint32_t *outCount);
int write_json_file_states(const char *path, char *const *names,
                           const int *states, uint32_t count);

/*
 * Free an array of strings.
 */
void free_string_array(char **strings, uint32_t count);

#endif /* UTILS_H */
