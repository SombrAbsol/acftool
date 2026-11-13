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
 * Parse a flat JSON object. Entry names point into '*outText', which must be
 * freed along with '*outEntries'.
 */
bool filelist_read(const char *path,
    char **outText,
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

        // unescape the key in place, up to the closing quote
        char *name = ++p;
        char *d = name;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) {
                ++p; // keep the escaped character as-is
            }
            *d++ = *p++;
        }
        if (!*p) {
            fprintf(stderr, "%s: unterminated string key\n", programName);
            goto error;
        }
        ++p;
        *d = '\0';

        p = skip_ws(p);
        if (*p != ':') {
            fprintf(stderr, "%s: expected ':' after key\n", programName);
            goto error;
        }
        p = skip_ws(p + 1);

        int state;
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
                "%s: expected true, false, or null as value\n",
                programName);
            goto error;
        }

        if (count == capacity) {
            capacity = capacity ? capacity * 2 : 16;
            FileState *tmp = realloc(entries, capacity * sizeof(*entries));
            if (!tmp) {
                fprintf(stderr, "%s: memory allocation failed\n", programName);
                goto error;
            }
            entries = tmp;
        }

        entries[count].name = name;
        entries[count].state = state;
        ++count;

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
 * Write a flat JSON object, one entry per line.
 */
bool filelist_write(const char *path, const FileState *entries, uint32_t count)
{
    static const char *const literals[] = { "null", "false", "true" };

    FILE *f = xfopen(path, "wb");
    if (!f) {
        fprintf(stderr, "%s: cannot open '%s'\n", programName, path);
        return false;
    }

    fputs("{\n", f);
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
