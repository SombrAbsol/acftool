/*
 * Utility functions.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#include "utils.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define strcasecmp _stricmp
#endif

/*
 * Cross-platform wrapper around fopen. Use fopen_s on Windows, or fopen
 * otherwise.
 */
FILE *xfopen(const char *path, const char *mode)
{
#ifdef _WIN32
    FILE *f = NULL;
    if (fopen_s(&f, path, mode) != 0) {
        fprintf(
            stderr, "xfopen: failed to open '%s' with mode '%s'\n", path, mode);
        return NULL;
    }
    return f;
#else
    FILE *f = fopen(path, mode);
    if (!f) {
        fprintf(
            stderr, "xfopen: failed to open '%s' with mode '%s'\n", path, mode);
    }
    return f;
#endif
}

/*
 * Compute the number of padding bytes required to align 'n' to the next
 * 4-byte boundary.
 */
uint32_t pad4(uint32_t n)
{
    return (4 - (n & 3)) & 3;
}

/*
 * strdup replacement using malloc.
 */
char *xstrdup(const char *s)
{
    if (!s) {
        return NULL;
    }

    size_t len = strlen(s) + 1; // +1 for null terminator
    char *p = malloc(len);
    if (p) {
        memcpy(p, s, len);
    } else {
        fprintf(stderr, "xstrdup: memory allocation failed\n");
    }

    return p;
}

/*
 * Read an entire file into memory.
 */
unsigned char *read_file(const char *path, size_t *out_size)
{
    FILE *f = NULL;
    unsigned char *buf = NULL;

    f = xfopen(path, "rb");
    if (!f) {
        fprintf(stderr, "read_file: cannot open '%s'\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "read_file: fseek failed for '%s'\n", path);
        goto error;
    }

#if defined(_WIN32)
    long long sz = _ftelli64(f); // 64-bit position to support files > 2 GB
#else
    long long sz = ftello(f);
#endif

    if (sz < 0) {
        fprintf(stderr, "read_file: invalid file size for '%s'\n", path);
        goto error;
    }

    if ((unsigned long long)sz
        > SIZE_MAX) { // guard against truncation on 32-bit platforms
        fprintf(stderr, "read_file: file too large for memory allocation\n");
        goto error;
    }

    size_t size = (size_t)sz;

    rewind(f); // seek back to the start before reading

    buf = malloc(size);
    if (!buf) {
        fprintf(
            stderr, "read_file: memory allocation failed (%zu bytes)\n", size);
        goto error;
    }

    if (fread(buf, 1, size, f) != size) {
        fprintf(stderr, "read_file: failed to read entire file '%s'\n", path);
        goto error;
    }

    fclose(f);

    if (out_size) {
        *out_size = size;
    }
    return buf;

error:
    if (f) {
        fclose(f);
    }
    free(buf);
    return NULL;
}

/*
 * Write bytes to a file, creating or truncating it as needed.
 */
bool write_file(const char *path, const uint8_t *data, size_t size)
{
    if (!path) {
        return false;
    }

    FILE *f = xfopen(path, "wb");
    if (!f) {
        return false;
    }

    if (size && data
        && fwrite(data, 1, size, f)
            != size) { // skip fwrite if there is nothing to write
        fprintf(stderr, "write_file: failed to write '%s'\n", path);
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

/*
 * Create a directory if it does not already exist.
 */
bool mkdir_dir(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0) {
        return true;
    }
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#endif
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    fprintf(stderr, "mkdir_dir: failed to create '%s'\n", path);
    return false;
}

/*
 * Check whether a file extension matches one of the known Nintendo container
 * magic strings whose bytes appear reversed in the file header, or not.
 */
bool is_invertible(const char *ext)
{
    static const char *const invertible[]
        = { "RGCN", "RLCN", "RECN", "RNAN", "RCSN", "RTFN" };

    for (size_t i = 0; i < sizeof(invertible) / sizeof(invertible[0]); ++i) {
        if (strcasecmp(ext, invertible[i]) == 0) {
            return true;
        }
    }

    return false;
}

/*
 * Reverse a null-terminated string in place.
 */
void reverse_str_inplace(char *s)
{
    if (!s) {
        return;
    }

    size_t i = 0;
    size_t j = strlen(s);
    if (j < 2) { // nothing to reverse
        return;
    }

    --j; // convert length to index of last character
    while (i < j) {
        char t = s[i];
        s[i++] = s[j];
        s[j--] = t;
    }
}

/*
 * Attempt to derive a file extension from the leading bytes of data.
 */
const char *try_get_extension(const uint8_t *data,
    size_t size,
    int maxlength,
    int minlength,
    const char *defaultExt,
    char *outExt,
    size_t outExtSz)
{
    if (!defaultExt || !outExt || outExtSz == 0) {
        return defaultExt;
    }

    if (!data || size == 0 || maxlength <= 0 || minlength < 0) {
        snprintf(outExt, outExtSz, "%s", defaultExt);
        return defaultExt;
    }

    int n = 0;
    for (int i = 0; i < maxlength && (size_t)i < size; ++i) {
        unsigned char c = data[i];
        if (!isalnum(c)) { // stop at the first non-alphanumeric byte
            break;
        }

        if ((size_t)n + 1 < outExtSz) {
            outExt[n++] = (char)c;
        } else {
            break; // outExt buffer is full
        }
    }

    outExt[n] = '\0';

    if (n <= minlength) { // too few bytes to form a useful extension
        return defaultExt;
    }

    if (is_invertible(outExt)) {
        reverse_str_inplace(outExt);
    }

    return outExt;
}

/*
 * Escape a set of characters for JSON output.
 */
char *escape_json_string(const char *s, size_t len)
{
    char *esc = malloc(len * 6 + 1); // worst case
    if (!esc) {
        fprintf(stderr, "escape_json_string: memory allocation failed\n");
        return NULL;
    }

    char *d = esc;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        if (c == '"' || c == '\\') {
            *d++ = '\\';
            *d++ = c;
        } else if (c == '\n') {
            *d++ = '\\';
            *d++ = 'n';
        } else if (c == '\r') {
            *d++ = '\\';
            *d++ = 'r';
        } else {
            *d++ = c;
        }
    }

    *d = '\0';
    return esc;
}

/*
 * Reverse escape_json_string for supported sequences.
 */
char *unescape_json_string(const char *start, size_t len)
{
    char *out = malloc(
        len + 1); // output is at most as long as the input plus null terminator
    if (!out) {
        fprintf(stderr, "unescape_json_string: memory allocation failed\n");
        return NULL;
    }

    char *d = out;

    for (size_t i = 0; i < len; i++) {
        if (start[i] == '\\' && i + 1 < len) {
            i++; // consume the backslash and handle the next character
            if (start[i] == 'n') {
                *d++ = '\n';
            } else if (start[i] == 'r') {
                *d++ = '\r';
            } else {
                *d++ = start[i]; // pass through other escape sequences as-is
            }
        } else {
            *d++ = start[i];
        }
    }

    *d = '\0';
    return out;
}

/*
 * Advance the input pointer past any leading whitespace characters.
 */
static void skip_ws(const char **p)
{
    while (**p && isspace((unsigned char)**p)) {
        ++(*p);
    }
}

/*
 * Parse a flat JSON object, mapping the literals true, false, and null to
 * the integers 1, 0, and -1 respectively.
 */
bool read_json_file_states(
    const char *path, char ***outNames, int **outStates, uint32_t *outCount)
{
    if (!path || !outNames || !outStates || !outCount) {
        return false;
    }

    *outNames = NULL;
    *outStates = NULL;
    *outCount = 0;

    size_t size = 0;
    uint8_t *buf = read_file(path, &size);
    if (!buf) {
        return false;
    }

    char *json = malloc(size + 1); // +1 for the null terminator added below
    if (!json) {
        free(buf);
        return false;
    }

    memcpy(json, buf, size);
    json[size] = '\0';
    free(buf);

    const char *p = json;
    uint32_t capacity = 16; // initial array capacity; doubled on overflow
    uint32_t count = 0;

    char **names = malloc(capacity * sizeof(*names));
    int *states = malloc(capacity * sizeof(*states));
    if (!names || !states) {
        free(names);
        free(states);
        free(json);
        return false;
    }

    skip_ws(&p);
    if (*p != '{') {
        fprintf(
            stderr, "read_json_file_states: expected '{' at start of object\n");
        goto error;
    }
    ++p;

    for (;;) {
        skip_ws(&p);

        if (*p == '}') {
            ++p;
            break;
        }

        if (*p != '"') {
            fprintf(
                stderr, "read_json_file_states: expected '\"' before key\n");
            goto error;
        }
        ++p;

        const char *start = p;
        while (
            *p && !(*p == '"' && p[-1] != '\\')) { // scan to the closing quote,
                                                   // treating \" as escaped
            ++p;
        }
        if (!*p) {
            fprintf(stderr, "read_json_file_states: unterminated string key\n");
            goto error;
        }

        size_t len = (size_t)(p - start);
        char *name = unescape_json_string(start, len);
        if (!name) {
            goto error;
        }
        ++p;

        skip_ws(&p);
        if (*p != ':') {
            fprintf(stderr, "read_json_file_states: expected ':' after key\n");
            free(name);
            goto error;
        }
        ++p;

        skip_ws(&p);

        int state = -2; // sentinel: -2 = not yet parsed
        if (strncmp(p, "null", 4) == 0) {
            state = -1;
            p += 4;
        } else if (strncmp(p, "false", 5) == 0) {
            state = 0;
            p += 5;
        } else if (strncmp(p, "true", 4) == 0) {
            state = 1;
            p += 4;
        } else {
            fprintf(stderr,
                "read_json_file_states: expected true, false, or "
                "null as value\n");
            free(name);
            goto error;
        }

        if (count == capacity) {
            capacity *= 2; // double the array capacity

            char **tmpNames = realloc(names, capacity * sizeof(*names));
            if (!tmpNames) {
                fprintf(stderr,
                    "read_json_file_states: memory allocation failed\n");
                free(name);
                goto error;
            }
            names = tmpNames;

            int *tmpStates = realloc(states, capacity * sizeof(*states));
            if (!tmpStates) {
                fprintf(stderr,
                    "read_json_file_states: memory allocation failed\n");
                free(name);
                goto error;
            }
            states = tmpStates;
        }

        names[count] = name;
        states[count] = state;
        ++count;

        skip_ws(&p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            ++p;
            break;
        }
        fprintf(
            stderr, "read_json_file_states: expected ',' or '}' after value\n");
        goto error;
    }

    skip_ws(&p);
    if (*p != '\0') { // trailing garbage after the closing brace
        fprintf(stderr, "read_json_file_states: trailing data after '}'\n");
        goto error;
    }

    free(json);
    *outNames = names;
    *outStates = states;
    *outCount = count;
    return true;

error:
    for (uint32_t i = 0; i < count; ++i) {
        free(names[i]);
    }
    free(names);
    free(states);
    free(json);
    return false;
}

/*
 * Write a flat JSON object, mapping the literals true, false, and null to the
 * integers 1, 0, and -1 respectively.
 */
bool write_json_file_states(
    const char *path, char *const *names, const int *states, uint32_t count)
{
    if (!path || (!names && count) || (!states && count)) {
        return false;
    }

    FILE *f = xfopen(path, "wb");
    if (!f) {
        return false;
    }

    fputs("{\n", f);

    for (uint32_t i = 0; i < count; ++i) {
        if (!names[i]) {
            fclose(f);
            return false;
        }

        char *esc = escape_json_string(names[i], strlen(names[i]));
        if (!esc) {
            fclose(f);
            return false;
        }

        // map integer state back to its JSON literal
        const char *value = NULL;
        if (states[i] == -1) {
            value = "null";
        } else if (states[i] == 0) {
            value = "false";
        } else if (states[i] == 1) {
            value = "true";
        } else {
            free(esc);
            fclose(f);
            return false;
        }

        fprintf(f,
            "  \"%s\": %s%s\n",
            esc,
            value,
            (i + 1 < count) ? "," : ""); // omit trailing comma on last entry
        free(esc);
    }

    fputs("}\n", f);
    fclose(f);
    return true;
}

/*
 * Free an array of strings.
 */
void free_string_array(char **strings, uint32_t count)
{
    if (!strings) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        free(strings[i]);
    }

    free(strings);
}
