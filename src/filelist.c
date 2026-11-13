/*
 * Filelist handler.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#include "filelist.h"

#include "fs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Advance the input pointer past any leading whitespace characters.
 */
static char *skip_ws(char *p)
{
    while (isspace((unsigned char)*p)) {
        ++p;
    }
    return p;
}

/*
 * Unescape in place the string starting after the opening quote at 'p'. Return
 * the position after its closing quote, or NULL if it is unterminated.
 */
static char *read_string(char *p)
{
    char *d = p;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            ++p; // keep the escaped character as-is
        }
        *d++ = *p++;
    }
    if (!*p) {
        return NULL;
    }
    *d = '\0';
    return p + 1;
}

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
    uint32_t *outCount)
{
    char *text = (char *)read_file(path, NULL);
    if (!text) {
        return false;
    }

    FileState *entries = NULL;
    uint32_t capacity = 0; // array capacity; doubled on overflow
    uint32_t count = 0;
    *outSignature = NULL;
    *outHuffman = false;

    char *p = skip_ws(text);
    if (*p != '{') {
        fprintf(stderr, "%s: expected '{' at start of object\n", programName);
        goto error;
    }
    ++p;

    for (;;) {
        p = skip_ws(p);

        if (*p == '}') {
            ++p;
            break;
        }

        if (*p != '"') {
            fprintf(stderr, "%s: expected '\"' before key\n", programName);
            goto error;
        }

        char *name = p + 1;
        if (!(p = read_string(name))) {
            fprintf(stderr, "%s: unterminated string key\n", programName);
            goto error;
        }

        p = skip_ws(p);
        if (*p != ':') {
            fprintf(stderr, "%s: expected ':' after key\n", programName);
            goto error;
        }
        p = skip_ws(p + 1);

        // the signature of an archive other than ACF is a string
        bool isSignature = !strcmp(name, "signature") && *p == '"';
        bool isHuffman = !strcmp(name, "huffman");
        int state = 0;
        if (isSignature) {
            *outSignature = p + 1;
            if (!(p = read_string(p + 1))) {
                fprintf(stderr, "%s: unterminated signature\n", programName);
                goto error;
            }
        } else if (strncmp(p, "null", 4) == 0) {
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
                "%s: expected true, false, or null as value\n",
                programName);
            goto error;
        }

        if (isHuffman) {
            *outHuffman = state == 1;
        } else if (!isSignature) {
            if (count == capacity) {
                capacity = capacity ? capacity * 2 : 16;
                FileState *tmp = realloc(entries, capacity * sizeof(*entries));
                if (!tmp) {
                    fprintf(
                        stderr, "%s: memory allocation failed\n", programName);
                    goto error;
                }
                entries = tmp;
            }
            entries[count].name = name;
            entries[count].state = state;
            ++count;
        }

        p = skip_ws(p);
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p == '}') {
            ++p;
            break;
        }
        fprintf(stderr, "%s: expected ',' or '}' after value\n", programName);
        goto error;
    }

    if (*skip_ws(p) != '\0') { // trailing garbage after the closing brace
        fprintf(stderr, "%s: trailing data after '}'\n", programName);
        goto error;
    }

    *outText = text;
    *outEntries = entries;
    *outCount = count;
    return true;

error:
    free(entries);
    free(text);
    return false;
}

/*
 * Write a flat JSON object, one entry per line, starting with the "signature"
 * key if 'signature' is not NULL, then the "huffman" key if either is set.
 */
bool filelist_write(const char *path,
    const char *signature,
    bool huffman,
    const FileState *entries,
    uint32_t count)
{
    static const char *const literals[] = { "null", "false", "true" };

    FILE *f = xfopen(path, "wb");
    if (!f) {
        fprintf(stderr, "%s: cannot open '%s'\n", programName, path);
        return false;
    }

    fputs("{\n", f);
    // an MBG0 archive always tells whether it was Huffman-compressed
    if (signature) {
        fprintf(f, "  \"signature\": \"%s\",\n", signature);
    }
    if (signature || huffman) {
        fprintf(f,
            "  \"huffman\": %s%s\n",
            huffman ? "true" : "false",
            count ? "," : "");
    }
    for (uint32_t i = 0; i < count; ++i) {
        fputs("  \"", f);
        for (const char *s = entries[i].name; *s; ++s) {
            if (*s == '"' || *s == '\\') {
                fputc('\\', f);
            }
            fputc(*s, f);
        }
        // omit trailing comma on last entry
        fprintf(f,
            "\": %s%s\n",
            literals[entries[i].state + 1],
            i + 1 < count ? "," : "");
    }
    fputs("}\n", f);

    if (fclose(f) != 0) {
        fprintf(stderr, "%s: failed to write '%s'\n", programName, path);
        return false;
    }
    return true;
}
