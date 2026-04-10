// Copyright (c) 2026 SombrAbsol

#include <ctype.h>
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

#include "utils.h"

FILE *xfopen(const char *path, const char *mode) {
    #ifdef _WIN32
    FILE *f = NULL;
    return (fopen_s(&f, path, mode) == 0) ? f : NULL;
    #else
    return fopen(path, mode);
    #endif
}

// padding
size_t pad_size(size_t size, size_t align) {
    return (size + align - 1) & ~(align - 1);
}

// strdup implementation
char *xstrdup(const char *s) {
    if (!s) return NULL;

    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

// read entire file into memory
uint8_t *read_file(const char *path, size_t *outSize) {
    if (!path || !outSize) return NULL;

    struct stat st;
    if (stat(path, &st) != 0 || st.st_size < 0) return NULL;

    size_t size = (size_t)st.st_size;
    FILE *f = xfopen(path, "rb");
    if (!f) return NULL;

    uint8_t *buf = malloc(size ? size : 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t got = fread(buf, 1, size, f);
    fclose(f);

    if (got != size) {
        free(buf);
        return NULL;
    }

    *outSize = size;
    return buf;
}

// write data to a file
int write_file(const char *path, const uint8_t *data, size_t size) {
    if (!path) return -1;

    FILE *f = xfopen(path, "wb");
    if (!f) return -1;

    if (size && data && fwrite(data, 1, size, f) != size) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return EXIT_SUCCESS;
}

// create directory if needed
int mkdir_dir(const char *path) {
    #ifdef _WIN32
    if (_mkdir(path) == 0) return 0;
    #else
    if (mkdir(path, 0755) == 0) return 0;
    #endif

    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : -1;
}

// check if extension needs reversing
int is_invertible(const char *ext) {
    static const char *const invertible[] = {
        "RGCN", "RLCN", "RECN", "RNAN", "RCSN", "RTFN"
    };

    for (size_t i = 0; i < sizeof(invertible) / sizeof(invertible[0]); ++i) {
        if (strcasecmp(ext, invertible[i]) == 0)
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// reverse string in place
void reverse_str_inplace(char *s) {
    if (!s) return;

    size_t i = 0;
    size_t j = strlen(s);
    if (j < 2) return;

    --j;
    while (i < j) {
        char t = s[i];
        s[i++] = s[j];
        s[j--] = t;
    }
}

// try to detect file extension from data
const char *try_get_extension(
    const uint8_t *data, size_t size,
    int maxlength, int minlength,
    const char *defaultExt, char *outExt, size_t outExtSz
) {
    if (!defaultExt || !outExt || outExtSz == 0)
        return defaultExt;

    if (!data || size == 0 || maxlength <= 0 || minlength < 0) {
        snprintf(outExt, outExtSz, "%s", defaultExt);
        return defaultExt;
    }

    int n = 0;
    for (int i = 0; i < maxlength && (size_t)i < size; ++i) {
        unsigned char c = data[i];
        if (!isalnum(c))
            break;

        if ((size_t)n + 1 < outExtSz)
            outExt[n++] = (char)c;
        else
            break;
    }

    outExt[n] = '\0';

    if (n <= minlength)
        return defaultExt;

    if (is_invertible(outExt))
        reverse_str_inplace(outExt);

    return outExt;
}

// escape minimal set of characters
char *escape_json_string(const char *s, size_t maxlen) {
    char *esc = malloc(maxlen * 2 + 1);
    if (!esc) return NULL;

    char *d = esc;
    for (size_t i = 0; i < maxlen && s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];

        if (c == '"' || c == '\\') {
            *d++ = '\\';
            *d++ = (char)c;
        } else if (c == '\n') {
            *d++ = '\\';
            *d++ = 'n';
        } else if (c == '\r') {
            *d++ = '\\';
            *d++ = 'r';
        } else {
            *d++ = (char)c;
        }
    }

    *d = '\0';
    return esc;
}

// undo escaping
char *unescape_json_string(const char *start, size_t len) {
    char *out = malloc(len + 1);
    if (!out) return NULL;

    char *d = out;
    for (size_t i = 0; i < len; ++i) {
        if (start[i] == '\\' && i + 1 < len) {
            ++i;
            if (start[i] == 'n') *d++ = '\n';
            else if (start[i] == 'r') *d++ = '\r';
            else *d++ = start[i];
        } else {
            *d++ = start[i];
        }
    }

    *d = '\0';
    return out;
}

static void skip_ws(const char **p) {
    while (**p && isspace((unsigned char)**p))
        ++(*p);
}

int read_json_file_states(
    const char *path,
    char ***outNames,
    int **outStates,
    uint32_t *outCount
) {
    if (!path || !outNames || !outStates || !outCount)
        return EXIT_FAILURE;

    *outNames = NULL;
    *outStates = NULL;
    *outCount = 0;

    size_t size = 0;
    uint8_t *buf = read_file(path, &size);
    if (!buf) return EXIT_FAILURE;

    char *json = malloc(size + 1);
    if (!json) {
        free(buf);
        return EXIT_FAILURE;
    }

    memcpy(json, buf, size);
    json[size] = '\0';
    free(buf);

    const char *p = json;
    uint32_t capacity = 16;
    uint32_t count = 0;

    char **names = malloc(capacity * sizeof(*names));
    int *states = malloc(capacity * sizeof(*states));
    if (!names || !states) {
        free(names);
        free(states);
        free(json);
        return EXIT_FAILURE;
    }

    skip_ws(&p);
    if (*p != '{') goto error;
    ++p;

    for (;;) {
        skip_ws(&p);

        if (*p == '}') {
            ++p;
            break;
        }

        if (*p != '"')
            goto error;
        ++p;

        const char *start = p;
        while (*p && !(*p == '"' && p[-1] != '\\'))
            ++p;
        if (!*p)
            goto error;

        size_t len = (size_t)(p - start);
        char *name = unescape_json_string(start, len);
        if (!name)
            goto error;
        ++p;

        skip_ws(&p);
        if (*p != ':') {
            free(name);
            goto error;
        }
        ++p;

        skip_ws(&p);

        int state = -2;
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
            free(name);
            goto error;
        }

        if (count == capacity) {
            capacity *= 2;

            char **tmpNames = realloc(names, capacity * sizeof(*names));
            if (!tmpNames) {
                free(name);
                goto error;
            }
            names = tmpNames;

            int *tmpStates = realloc(states, capacity * sizeof(*states));
            if (!tmpStates) {
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
        goto error;
    }

    skip_ws(&p);
    if (*p != '\0')
        goto error;

    free(json);
    *outNames = names;
    *outStates = states;
    *outCount = count;
    return EXIT_SUCCESS;

    error:
    for (uint32_t i = 0; i < count; ++i)
        free(names[i]);
    free(names);
    free(states);
    free(json);
    return EXIT_FAILURE;
}

int write_json_file_states(
    const char *path,
    char *const *names,
    const int *states,
    uint32_t count
) {
    if (!path || (!names && count) || (!states && count))
        return EXIT_FAILURE;

    FILE *f = xfopen(path, "wb");
    if (!f) return EXIT_FAILURE;

    fputs("{\n", f);

    for (uint32_t i = 0; i < count; ++i) {
        if (!names[i]) {
            fclose(f);
            return EXIT_FAILURE;
        }

        char *esc = escape_json_string(names[i], strlen(names[i]));
        if (!esc) {
            fclose(f);
            return EXIT_FAILURE;
        }

        const char *value = NULL;
        if (states[i] == -1) value = "null";
        else if (states[i] == 0) value = "false";
        else if (states[i] == 1) value = "true";
        else {
            free(esc);
            fclose(f);
            return EXIT_FAILURE;
        }

        fprintf(f, "  \"%s\": %s%s\n", esc, value, (i + 1 < count) ? "," : "");
        free(esc);
    }

    fputs("}\n", f);
    fclose(f);
    return EXIT_SUCCESS;
}

void free_string_array(char **strings, uint32_t count) {
    if (!strings) return;

    for (uint32_t i = 0; i < count; ++i)
        free(strings[i]);

    free(strings);
}
