// Copyright (c) 2026 SombrAbsol

#include "acf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv) {

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // force utf-8 on windows
#endif

    if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        printf("Copyright (c) 2026 SombrAbsol\n");
        printf("acftool - ACF archive utility for Pokémon Ranger: Guardian Signs\n\n");
        printf("Usage:\n");
        printf("  %s -x|--extract <in.acf|indir>  extract mode\n", argv[0]);
        printf("  %s -b|--build   <indir>         build mode\n", argv[0]);
        printf("  %s -h|--help                    show this help\n", argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc != 3) {
        fprintf(stderr, "Invalid arguments\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *mode = argv[1];
    const char *path = argv[2];

    if (!strcmp(mode, "-x") || !strcmp(mode, "--extract")) {
        struct stat st;
        if (stat(path, &st) != 0) {
            fprintf(stderr, "Invalid path: '%s'\n", path);
            return EXIT_FAILURE;
        }

        if (S_ISDIR(st.st_mode)) {
            printf("Extracting all ACFs in directory: %s\n", path);
            process_directory(path);
        } else {
            extract_acf(path);
        }
    } else if (!strcmp(mode, "-b") || !strcmp(mode, "--build")) {
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Invalid path: '%s'\n", path);
            return EXIT_FAILURE;
        }

        printf("Building ACF from directory: %s\n", path);
        build_acf(path);
    } else {
        fprintf(stderr, "Unknown option: %s\n", mode);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
