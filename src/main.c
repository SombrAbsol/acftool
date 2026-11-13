/*
 * ACF archive utility for Pokémon Ranger: Guardian Signs.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#include "acf.h"
#include "fs.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_LIKE __attribute__((format(printf, 1, 2)))
#else
#define PRINTF_LIKE
#endif

// the OS built for, the UTC build date and time set by the Makefile, as
// yyyy.mm.dd-hh:mm:ss, and the Makefile recipe and static linking, if any
#if defined(_WIN32)
#define BUILD_OS "windows"
#elif defined(__APPLE__)
#define BUILD_OS "macos"
#elif defined(__linux__)
#define BUILD_OS "linux"
#else
#define BUILD_OS "unknown"
#endif
#define STRING(x)    #x
#define STRING_OF(x) STRING(x)
#ifdef BUILD_DATE
#define BUILD_UTC STRING_OF(BUILD_DATE)
#else
#define BUILD_UTC "unknown"
#endif
#ifdef MAKE_RECIPE
#define BUILD_RECIPE "-" STRING_OF(MAKE_RECIPE)
#else
#define BUILD_RECIPE ""
#endif
#ifdef STATIC_LINK
#define BUILD_STATIC "-static"
#else
#define BUILD_STATIC ""
#endif

int verbose = 1;
static bool yes;
static const char *outRoot; // directory of the extracted archives, or NULL
static bool failed; // an input or an archive found failed

/*
 * Print a command-line error and point to the help.
 */
PRINTF_LIKE static int usage_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", programName);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\nTry '%s --help' for more information.\n", programName);
    va_end(ap);
    return EXIT_FAILURE;
}

/*
 * Ask whether to replace an existing output: 'y' answers yes, any other key
 * no. Keys typed beforehand are dropped, so that they cannot answer.
 */
static bool confirm_overwrite(const char *name)
{
    while (key_pressed()) {
        (void)get_key();
    }
    printf("Replace '%s'? [y/N] ", name);
    fflush(stdout);
    bool replace = tolower(get_key()) == 'y';
    printf("%s\n", replace ? "yes" : "no");
    return replace;
}

/*
 * Check whether an output may be written: new, confirmed, or -y given.
 */
static bool may_write(const char *path)
{
    bool isDir;
    return yes || !path_exists(path, &isDir) || confirm_overwrite(path);
}

/*
 * Derive the output of an input from its path, trailing separators dropped:
 * its extension stripped if 'ext' is empty, or 'ext' appended otherwise.
 */
static bool default_output(
    char *dst, size_t dstSize, const char *path, const char *ext)
{
    if ((size_t)snprintf(dst, dstSize, "%s", path) >= dstSize) {
        fprintf(stderr, "%s: path too long: '%s'\n", programName, path);
        return false;
    }
    for (size_t n = strlen(dst); n > 1 && IS_SEP(dst[n - 1]);) {
        dst[--n] = '\0';
    }

    // find the last dot of the filename to locate the extension
    char *dot = strrchr(dst, '.');
    if (!*ext && dot && dot > path_basename(dst)) {
        *dot = '\0';
    }
    size_t len = strlen(dst);
    if ((size_t)snprintf(dst + len, dstSize - len, "%s", ext)
        >= dstSize - len) {
        fprintf(stderr, "%s: path too long: '%s%s'\n", programName, dst, ext);
        return false;
    }
    return true;
}

/*
 * Derive the output of an input written in the directory 'root', if any, or
 * next to it otherwise.
 */
static bool output_in(char *dst,
    size_t dstSize,
    const char *root,
    const char *path,
    const char *ext)
{
    char name[PATH_SIZE];
    return root ? default_output(name, sizeof(name), path_basename(path), ext)
            && join_path(dst, dstSize, root, name)
                : default_output(dst, dstSize, path, ext);
}

/*
 * Extract an archive found in a directory, into the output directory if any,
 * or next to it. An error does not stop the others, but fails the run.
 */
static void extract_found(const char *path)
{
    char outDir[PATH_SIZE];
    if (!output_in(outDir, sizeof(outDir), outRoot, path, "")) {
        failed = true;
    } else if (may_write(outDir)) {
        failed |= !acf_extract(path, outDir);
    }
}

/*
 * Print the usage and options.
 */
static void print_help(void)
{
    printf("Usage: %s [OPTION]... INPUT...\n", programName);
    printf(
        "Extract and build the ACF archives of Pokémon Ranger: Guardian "
        "Signs.\n"
        "\n"
        "INPUT is an ACF archive, extracted to a directory along with a\n"
        "filelist.json file listing its files, or a directory holding such a\n"
        "file, built into an archive. Any other directory has all its ACF\n"
        "archives extracted. The outputs are named after their input, its\n"
        "extension replaced, and written next to it by default.\n"
        "\n"
        "Options:\n"
        "  -h, --help             show this help and exit\n"
        "      --version          show the version and exit\n"
        "  -o, --output=OUTPUT    write the output of a single archive or "
        "directory to\n"
        "                         build to OUTPUT\n"
        "      --output-dir=DIR   write the outputs in DIR, created if "
        "missing\n"
        "  -x, --extract          extract the archives of INPUT, even if it "
        "holds a\n"
        "                         filelist.json file\n"
        "  -b, --build            build INPUT into an archive\n"
        "  -y, --yes              replace existing outputs without asking\n"
        "  -q, --quiet, --silent  show errors and warnings only\n");
}

/*
 * Print the version, the UTC build date and time followed by the OS built for,
 * and the Makefile recipe and static linking, if any, then the copyright.
 */
static void print_version(void)
{
    printf("%s %s-UTC-%s%s%s\n",
        programName,
        BUILD_UTC,
        BUILD_OS,
        BUILD_RECIPE,
        BUILD_STATIC);
    printf("Copyright (C) 2026 SombrAbsol\n"
           "License MIT (Expat)\n"
           "This is free software: you are free to change and redistribute "
           "it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n");
}

/*
 * Command-line interface.
 */
int main(int argc, char **argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // ensure UTF-8 output on Windows
#endif
    programName = "acftool";

    /*
     * Options as getopt_long: short ones grouped or with an attached argument,
     * long ones abbreviated or with "=arg", before "--". Help and version stop.
     */
    bool extract = false, build = false;
    static const struct {
        char name; // short name, or 0 for none
        const char *longName;
        bool hasArg;
    } options[] = {
        { 'o', "output", true },
        { 0, "output-dir", true },
        { 'x', "extract", false },
        { 'b', "build", false },
        { 'y', "yes", false },
        { 'q', "quiet", false },
        { 0, "silent", false },
        { 'h', "help", false },
        { 0, "version", false },
    };
    size_t known = sizeof(options) / sizeof(*options);
    const char *out = NULL, *outDir = NULL;
    bool operands = false; // past "--"
    int n = 0; // operands, moved to argv[1] onwards, never past argv[i]
    for (int i = 1; i < argc; ++i) {
        char *a = argv[i];
        if (operands || a[0] != '-' || a[1] == '\0') {
            argv[++n] = a;
            continue;
        }
        if (!strcmp(a, "--")) {
            operands = true;
            continue;
        }

        // a long option or its only abbreviation, or a group of short ones
        for (const char *p = a + 1; *p;) {
            size_t k = known, len = strcspn(a + 2, "=");
            const char *arg = NULL;
            char shown[3] = { '-', *p, 0 }; // the option named in errors
            if (a[1] == '-') {
                for (size_t j = 0, found = 0; j < known; ++j) {
                    if (!strncmp(options[j].longName, a + 2, len)) {
                        bool exact = !options[j].longName[len];
                        k = exact || !found++ ? j : known + 1;
                        if (exact) {
                            break;
                        }
                    }
                }
                if (k >= known) {
                    return usage_error(k == known ? "unrecognized option '%s'"
                                                  : "option '%s' is ambiguous",
                        a);
                }
                arg = a[2 + len] ? a + 3 + len : NULL;
                if (arg && !options[k].hasArg) {
                    return usage_error(
                        "option '--%s' doesn't allow an argument",
                        options[k].longName);
                }
                shown[1] = '-';
                p = "";
            } else {
                for (k = 0; k < known && options[k].name != *p; ++k) { }
                if (k == known) {
                    return usage_error("invalid option -- '%c'", *p);
                }
                arg = options[k].hasArg && p[1] ? p + 1 : NULL;
                p = arg ? "" : p + 1;
            }
            if (options[k].hasArg && !arg && (arg = argv[++i]) == NULL) {
                return usage_error("option '%s%s' requires an argument",
                    shown,
                    shown[1] == '-' ? options[k].longName : "");
            }

            const char *name = options[k].longName;
            if (!strcmp(name, "output")) {
                out = arg;
            } else if (!strcmp(name, "output-dir")) {
                outDir = arg;
            } else if (!strcmp(name, "extract")) {
                extract = true;
            } else if (!strcmp(name, "build")) {
                build = true;
            } else if (!strcmp(name, "yes")) {
                yes = true;
            } else if (!strcmp(name, "quiet") || !strcmp(name, "silent")) {
                verbose = 0;
            } else if (!strcmp(name, "version")) {
                print_version();
                return EXIT_SUCCESS;
            } else if (!strcmp(name, "help")) {
                print_help();
                return EXIT_SUCCESS;
            }
        }
    }

    // every operand is an input
    if (n == 0) {
        return usage_error("missing input operand");
    }
    if (extract && build) {
        return usage_error("options '--extract' and '--build' conflict");
    }
    if (out && outDir) {
        return usage_error("options '--output' and '--output-dir' conflict");
    }
    size_t count = 0;
    char **inputs = list_inputs(argv + 1, (size_t)n, &count);
    if (!inputs) {
        return EXIT_FAILURE;
    }

    /*
     * -o names the output of a single input, and --output-dir the directory to
     * write the outputs in, created if missing.
     */
    if (out && count > 1) {
        usage_error("option '--output' needs a single input");
        failed = true;
    }
    bool outIsDir = false;
    if (!failed && outDir && path_exists(outDir, &outIsDir) && !outIsDir) {
        fprintf(stderr, "%s: '%s' is not a directory\n", programName, outDir);
        failed = true;
    }
    if (failed || (outDir && !outIsDir && !mkdir_dir(outDir))) {
        for (size_t i = 0; i < count; ++i) {
            free(inputs[i]);
        }
        free(inputs);
        return EXIT_FAILURE;
    }

    /*
     * Build or extract each input in turn. An error does not stop the others
     * but fails the run, as does a single input not replacing its output.
     */
    int done = 0;
    for (size_t i = 0; i < count; ++i) {
        const char *in = inputs[i];
        bool isDir = false;
        if (!path_exists(in, &isDir)) {
            fprintf(stderr, "%s: cannot access '%s'\n", programName, in);
            failed = true;
            continue;
        }
        if (build && !isDir) {
            fprintf(stderr, "%s: '%s' is not a directory\n", programName, in);
            failed = true;
            continue;
        }

        // a directory holding a filelist.json file is built unless told
        // otherwise
        bool doBuild = build;
        char path[PATH_SIZE];
        bool listIsDir = false;
        if (isDir && !extract
            && join_path(path, sizeof(path), in, "filelist.json")
            && path_exists(path, &listIsDir)) {
            doBuild = true;
        }

        if (doBuild || !isDir) {
            if (out) {
                snprintf(path, sizeof(path), "%s", out);
            } else if (!output_in(path,
                           sizeof(path),
                           outDir,
                           in,
                           doBuild ? ".acf" : "")) {
                failed = true;
                continue;
            }
            if (may_write(path)) {
                failed
                    |= doBuild ? !acf_build(in, path) : !acf_extract(in, path);
                ++done;
            }
            continue;
        }

        // every archive of a directory, the output being their parent directory
        if (out) {
            usage_error("option '--output' needs an archive or a directory to "
                        "build");
            failed = true;
            continue;
        }
        outRoot = outDir;
        if (verbose != 0) {
            printf("Extracting all ACF archives in directory: '%s'\n", in);
        }
        long found = for_each_file(in, ".acf", extract_found);
        if (found < 0) {
            fprintf(
                stderr, "%s: cannot open directory '%s'\n", programName, in);
            failed = true;
        }
        if (found == 0 && verbose != 0) {
            printf("No ACF archives found in '%s'\n", in);
        }
        ++done;
    }

    for (size_t i = 0; i < count; ++i) {
        free(inputs[i]);
    }
    free(inputs);
    return !failed && done ? EXIT_SUCCESS : EXIT_FAILURE;
}
