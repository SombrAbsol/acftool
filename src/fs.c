/*
 * Filesystem and platform helpers.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE   200809L // ftello, also in strict ISO C modes
#define _FILE_OFFSET_BITS 64 // files past 2 GB on 32-bit systems
#endif

#include "fs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <conio.h>
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define mkdir(path, mode) _mkdir(path)
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#endif

/*
 * Growable list of file names.
 */
typedef struct {
    char **names;
    size_t count;
    size_t capacity;
} NameList;

const char *programName;

#ifndef _WIN32
static struct termios savedTio;
static bool rawMode;

/*
 * Restore the terminal settings saved by raw_enable.
 */
static void raw_disable(void)
{
    tcsetattr(0, TCSANOW, &savedTio);
}

/*
 * Put the terminal in non-canonical mode without echo, like _getch.
 */
static void raw_enable(void)
{
    if (rawMode || !isatty(0)) {
        return;
    }
    tcgetattr(0, &savedTio);
    struct termios t = savedTio;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &t);
    atexit(raw_disable);
    rawMode = true;
}
#endif

/*
 * Open a file with fopen, or fopen_s on Windows.
 */
FILE *xfopen(const char *path, const char *mode)
{
#ifdef _WIN32
    FILE *f = NULL;
    return fopen_s(&f, path, mode) == 0 ? f : NULL;
#else
    return fopen(path, mode);
#endif
}

/*
 * Read an entire file into memory. The buffer is null-terminated, the
 * terminator is not counted in 'outSize'.
 */
uint8_t *read_file(const char *path, size_t *outSize)
{
    uint8_t *buf = NULL;
    FILE *f = xfopen(path, "rb");
    if (!f) {
        fprintf(stderr, "%s: cannot open '%s'\n", programName, path);
        return NULL;
    }

#ifdef _WIN32
    // 64-bit position to support files > 2 GB
    long long sz = fseek(f, 0, SEEK_END) ? -1 : _ftelli64(f);
#else
    long long sz = fseek(f, 0, SEEK_END) ? -1 : (long long)ftello(f);
#endif

    // guard against truncation on 32-bit platforms, then seek back to the start
    if (sz < 0 || (unsigned long long)sz >= SIZE_MAX
        || fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: invalid file size for '%s'\n", programName, path);
        goto done;
    }

    size_t size = (size_t)sz;
    buf = malloc(size + 1); // +1 for the null terminator
    if (!buf) {
        fprintf(stderr,
            "%s: memory allocation failed (%zu bytes)\n",
            programName,
            size);
    } else if (fread(buf, 1, size, f) != size) {
        fprintf(
            stderr, "%s: failed to read entire file '%s'\n", programName, path);
        free(buf);
        buf = NULL;
    } else {
        buf[size] = '\0';
        if (outSize) {
            *outSize = size;
        }
    }

done:
    fclose(f);
    return buf;
}

/*
 * Write bytes to a file, creating or truncating it as needed.
 */
bool write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *f = xfopen(path, "wb");
    if (!f) {
        fprintf(stderr, "%s: cannot open '%s'\n", programName, path);
        return false;
    }

    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        fprintf(stderr, "%s: failed to write '%s'\n", programName, path);
    }
    return ok;
}

/*
 * Create a directory if it does not already exist.
 */
bool mkdir_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return true;
    }
    fprintf(stderr, "%s: failed to create '%s'\n", programName, path);
    return false;
}

/*
 * Check whether a path exists and whether it is a directory.
 */
bool path_exists(const char *path, bool *isDir)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    *isDir = S_ISDIR(st.st_mode);
    return true;
}

/*
 * Return a pointer to the filename component of a path, skipping any leading
 * directory segments.
 */
const char *path_basename(const char *path)
{
    const char *base = path;
    for (const char *p = path; *p; ++p) {
        // on Windows, "C:name" is a name in the current directory of drive C
        if (IS_SEP(*p) || (PATH_SEP[0] == '\\' && *p == ':')) {
            base = p + 1;
        }
    }
    return base;
}

/*
 * Join a directory and a name into a single path.
 */
bool join_path(char *dst, size_t dstSize, const char *dir, const char *name)
{
    size_t len = strlen(dir);
    const char *sep = len == 0 || IS_SEP(dir[len - 1]) ? "" : PATH_SEP;
    if ((size_t)snprintf(dst, dstSize, "%s%s%s", dir, sep, name) < dstSize) {
        return true;
    }
    fprintf(
        stderr, "%s: path too long: '%s%s%s'\n", programName, dir, sep, name);
    return false;
}

/*
 * Append a copy of a path to a list. Return false if memory runs out.
 */
static bool add_path(NameList *l, const char *path)
{
    if (l->count == l->capacity) {
        l->capacity = l->capacity ? l->capacity * 2 : 64;
        char **tmp = realloc(l->names, l->capacity * sizeof(*tmp));
        if (!tmp) {
            return false;
        }
        l->names = tmp;
    }
    size_t size = strlen(path) + 1;
    char *copy = malloc(size);
    if (!copy) {
        return false;
    }
    memcpy(copy, path, size);
    l->names[l->count++] = copy;
    return true;
}

/*
 * List the input operands, expanding '*' and '?' on Windows as the Microsoft C
 * runtime does. Return NULL if memory runs out after printing the reason.
 */
char **list_inputs(char *const *operands, size_t n, size_t *count)
{
    NameList list = { 0 };
    bool ok = true;
    for (size_t i = 0; ok && i < n; ++i) {
        size_t before = list.count;
#ifdef _WIN32
        const char *in = operands[i];
        int dirLen = (int)(path_basename(in) - in); // kept in the paths
        struct _finddata_t file;
        intptr_t handle = strpbrk(in, "*?") ? _findfirst(in, &file) : -1;
        if (handle != -1) {
            do {
                char path[PATH_SIZE];
                if (!strcmp(file.name, ".") || !strcmp(file.name, "..")) {
                    continue;
                }
                if ((size_t)snprintf(
                        path, sizeof(path), "%.*s%s", dirLen, in, file.name)
                    >= sizeof(path)) {
                    fprintf(stderr,
                        "%s: path too long: '%.*s%s'\n",
                        programName,
                        dirLen,
                        in,
                        file.name);
                    continue;
                }
                ok = add_path(&list, path);
            } while (ok && _findnext(handle, &file) == 0);
            _findclose(handle);
        }
#endif
        // an operand without wildcards, or matching nothing, is kept as is
        if (ok && list.count == before) {
            ok = add_path(&list, operands[i]);
        }
    }

    if (!ok) {
        fprintf(stderr, "%s: memory allocation failed\n", programName);
        for (size_t i = 0; i < list.count; ++i) {
            free(list.names[i]);
        }
        free(list.names);
        return NULL;
    }
    *count = list.count;
    return list.names;
}

/*
 * Call 'fn' with 'name' joined to 'dir' if 'name' ends with 'ext'.
 */
static bool visit(const char *dir,
    const char *name,
    const char *ext,
    void (*fn)(const char *))
{
    size_t len = strlen(name);
    size_t extLen = strlen(ext);
    char path[PATH_SIZE];
    if (len < extLen || strcasecmp(name + len - extLen, ext) != 0
        || !join_path(path, sizeof(path), dir, name)) {
        return false;
    }
    fn(path);
    return true;
}

/*
 * Call 'fn' with the path of each entry of 'dir' ending with 'ext', ignoring
 * case. Return the number of matches, or -1 if the directory cannot be read.
 */
#ifdef _WIN32
long for_each_file(const char *dir, const char *ext, void (*fn)(const char *))
{
    char pattern[PATH_SIZE];
    if (!join_path(pattern, sizeof(pattern), dir, "*")) {
        return -1;
    }

    struct _finddata_t file;
    intptr_t handle = _findfirst(pattern, &file);
    if (handle == -1) {
        return -1;
    }

    long found = 0;
    do {
        found += visit(dir, file.name, ext, fn);
    } while (_findnext(handle, &file) == 0);

    _findclose(handle);
    return found;
}
#else
long for_each_file(const char *dir, const char *ext, void (*fn)(const char *))
{
    DIR *d = opendir(dir);
    if (!d) {
        return -1;
    }

    long found = 0;
    for (struct dirent *entry; (entry = readdir(d)) != NULL;) {
        found += visit(dir, entry->d_name, ext, fn);
    }

    closedir(d);
    return found;
}
#endif

/*
 * Check for a waiting key press without blocking.
 */
bool key_pressed(void)
{
#ifdef _WIN32
    return _kbhit() != 0;
#else
    raw_enable();
    fd_set s;
    FD_ZERO(&s);
    FD_SET(0, &s);
    struct timeval tv = { 0, 0 };
    return rawMode && select(1, &s, NULL, NULL, &tv) > 0;
#endif
}

/*
 * Read one key press without echo.
 */
int get_key(void)
{
#ifdef _WIN32
    return _getch();
#else
    raw_enable();
    unsigned char c;
    // without a terminal, or at its end, answer escape
    return rawMode && read(0, &c, 1) == 1 ? c : 0x1b;
#endif
}
