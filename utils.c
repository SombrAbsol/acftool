// Copyright (c) 2026 SombrAbsol

#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// padding
size_t pad_size(size_t size, size_t align) {
    return (size + align - 1) & ~(align - 1);
}

// read entire file into memory
uint8_t *read_file(const char *path, size_t *outSize) {
    if (!path || !outSize) return NULL;

    struct stat st;
    if (stat(path, &st) != 0 || st.st_size < 0) return NULL;

    size_t size = (size_t)st.st_size;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    uint8_t *buf = malloc(size ? size : 1);
    if (!buf) { fclose(f); return NULL; }

    size_t got = fread(buf, 1, size, f);
    fclose(f);
    if (got != size) { free(buf); return NULL; }

    *outSize = size;
    return buf;
}

// write data to a file
int write_file(const char *path, const uint8_t *data, size_t size) {
    if (!path) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    if (size && data) {
        if (fwrite(data, 1, size, f) != size) { fclose(f); return -1; }
    }

    fclose(f);
    return EXIT_SUCCESS;
}

// check if extension needs reversing
int is_invertible(const char *ext) {
    const char *invertible[] = { "RGCN", "RLCN", "RECN", "RNAN", "RCSN", "RTFN" };
    for (size_t i = 0; i < sizeof(invertible)/sizeof(invertible[0]); ++i) {
        if (strcasecmp(ext, invertible[i]) == 0) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// reverse string in place
void reverse_str_inplace(char *s) {
    if (!s) return;
    size_t i = 0, j = strlen(s);
    if (j < 2) return;
    --j;
    while (i < j) {
        char t = s[i]; s[i++] = s[j]; s[j--] = t;
    }
}

// try to detect file extension from data
const char *try_get_extension(
    const uint8_t *data, size_t size,
    int maxlength, int minlength,
    const char *defaultExt, char *outExt, size_t outExtSz
) {
    if (!defaultExt || !outExt || outExtSz == 0) return defaultExt;
    if (!data || size == 0 || maxlength <= 0 || minlength < 0) {
        strncpy(outExt, defaultExt, outExtSz);
        outExt[outExtSz-1] = '\0';
        return defaultExt;
    }

    int n = 0;
    for (int i = 0; i < maxlength && (size_t)i < size; ++i) {
        unsigned char c = data[i];
        if (isalnum(c)) {
            if ((size_t)n + 1 < outExtSz) outExt[n++] = (char)c;
            else break;
        } else break;
    }

    outExt[n] = '\0';
    if (n <= minlength) return defaultExt;
    if (is_invertible(outExt)) reverse_str_inplace(outExt);
    return outExt;
}
