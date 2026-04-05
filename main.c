// Copyright (c) 2026 SombrAbsol

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <strings.h>
#include <unistd.h>
#endif

#include "lz10.h"
#include "utils.h"

typedef struct {
    char magic[4];       // "acf\0"
    uint32_t headerSize; // usually 0x20
    uint32_t dataStart;
    uint32_t numFiles;
    uint32_t unknown1;   // always 1
    uint32_t unknown2;   // always 0x32
    uint32_t padding[2];
} ACFHeader;

typedef struct {
    uint32_t relativeOffset;
    uint32_t outputSize;
    uint32_t inputSize;
} FATEntry;

static void make_index_name(char *dst, size_t dstSize, uint32_t index, const char *ext) {
    snprintf(dst, dstSize, "%04u.%s", index, ext ? ext : "bin");
}

static void make_outdir(char *dst, size_t dstSize, const char *path) {
    strncpy(dst, path, dstSize - 1);
    dst[dstSize - 1] = '\0';

    char *dot = strrchr(dst, '.');
    if (dot) *dot = '\0';
}

static void join_path(char *dst, size_t dstSize, const char *dir, const char *name) {
    #ifdef _WIN32
    char sep = '\\';
    if (!dir || !*dir) {
        snprintf(dst, dstSize, "%s", name);
        return;
    }
    size_t len = strlen(dir);
    if (dir[len - 1] == '\\' || dir[len - 1] == '/')
        snprintf(dst, dstSize, "%s%s", dir, name);
    else
        snprintf(dst, dstSize, "%s\\%s", dir, name);
    #else
    if (!dir || !*dir) {
        snprintf(dst, dstSize, "%s", name);
        return;
    }
    size_t len = strlen(dir);
    if (dir[len - 1] == '/')
        snprintf(dst, dstSize, "%s%s", dir, name);
    else
        snprintf(dst, dstSize, "%s/%s", dir, name);
    #endif
}

static int set_meta_name(char **metaNames, uint32_t count, uint32_t index, const char *name) {
    (void)count;
    metaNames[index] = xstrdup(name);
    return metaNames[index] ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int set_meta_bin_name(char **metaNames, uint32_t count, uint32_t index) {
    char name[32];
    make_index_name(name, sizeof(name), index, "bin");
    return set_meta_name(metaNames, count, index, name);
}

static void cleanup_extract(uint8_t *fileData, char **metaNames, int *metaStates, uint32_t numFiles) {
    free_string_array(metaNames, numFiles);
    free(metaStates);
    free(fileData);
}

static void cleanup_build(
    FILE *out,
    FATEntry *fat,
    char **files,
    int *compressFlags,
    size_t numFiles,
    char **jsonNames,
    int *jsonStates,
    uint32_t jsonCount
) {
    if (out) fclose(out);
    free(fat);

    if (files) {
        for (size_t i = 0; i < numFiles; ++i)
            free(files[i]);
    }
    free(files);
    free(compressFlags);
    free_string_array(jsonNames, jsonCount);
    free(jsonStates);
}

// extract files from acf archive
static int extract_acf(const char *path) {
    if (!path) return EXIT_FAILURE;

    size_t fileSize = 0;
    uint8_t *fileData = read_file(path, &fileSize);
    if (!fileData) {
        fprintf(stderr, "Can't read %s\n", path);
        return EXIT_FAILURE;
    }

    if (fileSize < sizeof(ACFHeader)) {
        fprintf(stderr, "%s: too small to be an ACF\n", path);
        free(fileData);
        return EXIT_FAILURE;
    }

    ACFHeader hdr;
    memcpy(&hdr, fileData, sizeof(hdr));

    if (memcmp(hdr.magic, "acf", 3) != 0) {
        fprintf(stderr, "%s doesn't have an 'acf\\0' header\n", path);
        free(fileData);
        return EXIT_FAILURE;
    }

    size_t fatOffset = hdr.headerSize;
    if (fatOffset > fileSize ||
        hdr.numFiles > (fileSize - fatOffset) / sizeof(FATEntry)) {
        fprintf(stderr, "%s: FAT table exceeds file size\n", path);
    free(fileData);
    return EXIT_FAILURE;
        }

        const FATEntry *entries = (const FATEntry *)(fileData + fatOffset);

        char outdir[512];
        make_outdir(outdir, sizeof(outdir), path);
        (void)mkdir_dir(outdir);

        char **metaNames = calloc(hdr.numFiles ? hdr.numFiles : 1, sizeof(*metaNames));
        int *metaStates = calloc(hdr.numFiles ? hdr.numFiles : 1, sizeof(*metaStates));
        if (!metaNames || !metaStates) {
            fprintf(stderr, "Memory allocation failed\n");
            cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
            return EXIT_FAILURE;
        }

        char extBuf[16];

        for (uint32_t i = 0; i < hdr.numFiles; ++i) {
            const FATEntry e = entries[i];

            if (e.relativeOffset == 0xFFFFFFFFu) {
                if (set_meta_bin_name(metaNames, hdr.numFiles, i) != EXIT_SUCCESS) {
                    fprintf(stderr, "Memory allocation failed\n");
                    cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                    return EXIT_FAILURE;
                }
                metaStates[i] = -1;
                continue;
            }

            size_t dataOffset = (size_t)hdr.dataStart + (size_t)e.relativeOffset;
            if (dataOffset >= fileSize) {
                fprintf(stderr, "Entry %u: offset out of range\n", i);
                if (set_meta_bin_name(metaNames, hdr.numFiles, i) != EXIT_SUCCESS) {
                    fprintf(stderr, "Memory allocation failed\n");
                    cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                    return EXIT_FAILURE;
                }
                metaStates[i] = -1;
                continue;
            }

            const uint8_t *src = fileData + dataOffset;
            uint8_t *outBuf = NULL;
            size_t outSize = 0;
            int compressed = 0;

            if (e.inputSize > 0) {
                if (dataOffset + (size_t)e.inputSize > fileSize) {
                    fprintf(stderr, "Entry %u: compressed data exceeds file size\n", i);
                    if (set_meta_bin_name(metaNames, hdr.numFiles, i) != EXIT_SUCCESS) {
                        fprintf(stderr, "Memory allocation failed\n");
                        cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                        return EXIT_FAILURE;
                    }
                    metaStates[i] = -1;
                    continue;
                }

                if (src[0] == 0x10) {
                    outBuf = lz10_decompress(src, (size_t)e.inputSize, &outSize);
                    if (outBuf) {
                        compressed = 1;
                    } else {
                        fprintf(stderr, "Decompression failed for entry %u, saving raw\n", i);
                        outSize = (size_t)e.inputSize;
                        outBuf = malloc(outSize ? outSize : 1);
                        if (!outBuf) {
                            fprintf(stderr, "Memory allocation failed\n");
                            cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                            return EXIT_FAILURE;
                        }
                        memcpy(outBuf, src, outSize);
                    }
                } else {
                    outSize = (size_t)e.inputSize;
                    outBuf = malloc(outSize ? outSize : 1);
                    if (!outBuf) {
                        fprintf(stderr, "Memory allocation failed\n");
                        cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                        return EXIT_FAILURE;
                    }
                    memcpy(outBuf, src, outSize);
                }
            } else {
                if (dataOffset + (size_t)e.outputSize > fileSize) {
                    fprintf(stderr, "Entry %u: raw data exceeds file size\n", i);
                    if (set_meta_bin_name(metaNames, hdr.numFiles, i) != EXIT_SUCCESS) {
                        fprintf(stderr, "Memory allocation failed\n");
                        cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                        return EXIT_FAILURE;
                    }
                    metaStates[i] = -1;
                    continue;
                }

                outSize = (size_t)e.outputSize;
                outBuf = malloc(outSize ? outSize : 1);
                if (!outBuf) {
                    fprintf(stderr, "Memory allocation failed\n");
                    cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                    return EXIT_FAILURE;
                }
                memcpy(outBuf, src, outSize);
            }

            const char *ext = try_get_extension(outBuf, outSize, 4, 2, "bin", extBuf, sizeof(extBuf));

            char relname[64];
            make_index_name(relname, sizeof(relname), i, ext);

            char outname[768];
            join_path(outname, sizeof(outname), outdir, relname);

            if (write_file(outname, outBuf, outSize) != 0)
                fprintf(stderr, "Failed writing %s\n", outname);

            if (set_meta_name(metaNames, hdr.numFiles, i, relname) != EXIT_SUCCESS) {
                fprintf(stderr, "Memory allocation failed\n");
                free(outBuf);
                cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
                return EXIT_FAILURE;
            }

            metaStates[i] = compressed ? 1 : 0;
            free(outBuf);

            if ((i & 31u) == 31u || i == hdr.numFiles - 1) {
                printf("\r  %s: extracted %u/%u", path, i + 1, hdr.numFiles);
                fflush(stdout);
            }
        }

        printf("\n");

        char metafile[768];
        join_path(metafile, sizeof(metafile), outdir, "filelist.json");

        if (write_json_file_states(metafile, metaNames, metaStates, hdr.numFiles) != 0) {
            fprintf(stderr, "Cannot create metadata file %s\n", metafile);
            cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
            return EXIT_FAILURE;
        }

        cleanup_extract(fileData, metaNames, metaStates, hdr.numFiles);
        return EXIT_SUCCESS;
}

// process all acf archives in a directory
#ifdef _WIN32
static void process_directory(const char *directory) {
    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "%s\\*.acf", directory);

    struct _finddata_t file;
    intptr_t hFile = _findfirst(searchPath, &file);
    if (hFile == -1L) {
        printf("No acf archives found in %s\n", directory);
        return;
    }

    do {
        char fullPath[512];
        join_path(fullPath, sizeof(fullPath), directory, file.name);
        (void)extract_acf(fullPath);
    } while (_findnext(hFile, &file) == 0);

    _findclose(hFile);
}
#else
static void process_directory(const char *directory) {
    DIR *dir = opendir(directory);
    if (!dir) {
        printf("Can't open directory %s\n", directory);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        const char *ext = strrchr(name, '.');

        if (ext && strcasecmp(ext, ".acf") == 0) {
            char fullPath[512];
            join_path(fullPath, sizeof(fullPath), directory, name);
            (void)extract_acf(fullPath);
        }
    }

    closedir(dir);
}
#endif

// build an acf archive from a directory
static int build_acf(const char *directory) {
    if (!directory) return EXIT_FAILURE;

    char metafile[512];
    join_path(metafile, sizeof(metafile), directory, "filelist.json");

    char **jsonNames = NULL;
    int *jsonStates = NULL; // -1 = null; 0 = false; 1 = true
    uint32_t jsonCount = 0;

    if (read_json_file_states(metafile, &jsonNames, &jsonStates, &jsonCount) != 0) {
        fprintf(stderr, "Metadata file not found or invalid: %s\n", metafile);
        return EXIT_FAILURE;
    }

    if (jsonCount == 0) {
        fprintf(stderr, "No files to pack\n");
        free_string_array(jsonNames, jsonCount);
        free(jsonStates);
        return EXIT_FAILURE;
    }

    size_t numFiles = jsonCount;
    char **files = calloc(numFiles, sizeof(*files));
    int *compressFlags = calloc(numFiles, sizeof(*compressFlags));
    FATEntry *fat = NULL;
    FILE *out = NULL;

    if (!files || !compressFlags) {
        fprintf(stderr, "Memory allocation failed\n");
        cleanup_build(NULL, NULL, files, compressFlags, numFiles, jsonNames, jsonStates, jsonCount);
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < numFiles; ++i)
        compressFlags[i] = -1;

    // filelist validation
    for (uint32_t i = 0; i < jsonCount; ++i) {
        const char *name = jsonNames[i];
        const int state = jsonStates[i];

        if (!name) {
            fprintf(stderr, "Invalid metadata entry at index %u\n", i);
            goto fail;
        }

        const char *dot = strrchr(name, '.');
        if (!dot || dot == name || dot[1] == '\0') {
            fprintf(stderr, "Invalid metadata key: expected NNNN.EXT, got %s\n", name);
            goto fail;
        }

        if ((size_t)(dot - name) != 4 ||
            !isdigit((unsigned char)name[0]) ||
            !isdigit((unsigned char)name[1]) ||
            !isdigit((unsigned char)name[2]) ||
            !isdigit((unsigned char)name[3])) {
            fprintf(stderr, "Invalid metadata key: expected 4-digit prefix, got %s\n", name);
        goto fail;
            }

            long index = strtol(name, NULL, 10);
            if (index < 0 || (uint32_t)index != i) {
                fprintf(stderr,
                        "Metadata entries must be contiguous and ordered: expected index %04u, got %s\n",
                        i, name);
                goto fail;
            }

            if (state == -1) {
                files[i] = NULL;
                compressFlags[i] = -1;
                continue;
            }

            if (state != 0 && state != 1) {
                fprintf(stderr, "Invalid metadata state for %s\n", name);
                goto fail;
            }

            size_t pathlen = strlen(directory) + 1 + strlen(name) + 1;
            files[i] = malloc(pathlen);
            if (!files[i]) {
                fprintf(stderr, "Memory allocation failed\n");
                goto fail;
            }

            join_path(files[i], pathlen, directory, name);
            compressFlags[i] = state;
    }

    char outname[512];
    snprintf(outname, sizeof(outname), "%s.acf", directory);

    out = fopen(outname, "wb");
    if (!out) {
        fprintf(stderr, "Cannot create %s\n", outname);
        goto fail;
    }

    ACFHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, "acf", 4);
    hdr.headerSize = sizeof(ACFHeader);
    hdr.dataStart = 0; // filled after data has been written
    hdr.numFiles = (uint32_t)numFiles;
    hdr.unknown1 = 1;
    hdr.unknown2 = 0x32;

    uint8_t *headerBuf = calloc(1, hdr.headerSize);
    if (!headerBuf) {
        fprintf(stderr, "Memory allocation failed\n");
        goto fail;
    }

    memcpy(headerBuf, &hdr, sizeof(hdr));
    fwrite(headerBuf, 1, hdr.headerSize, out);
    free(headerBuf);

    fat = calloc(numFiles ? numFiles : 1, sizeof(*fat));
    if (!fat) {
        fprintf(stderr, "Memory allocation failed\n");
        goto fail;
    }

    for (size_t i = 0; i < numFiles; ++i)
        fat[i].relativeOffset = 0xFFFFFFFFu;

    fwrite(fat, sizeof(*fat), numFiles, out);

    size_t offset = 0;
    for (size_t i = 0; i < numFiles; ++i) {
        if (compressFlags[i] == -1 || !files[i]) {
            fat[i].relativeOffset = 0xFFFFFFFFu;
            fat[i].inputSize = 0;
            fat[i].outputSize = 0;
            continue;
        }

        size_t sz = 0;
        uint8_t *buf = read_file(files[i], &sz);
        if (!buf) {
            fprintf(stderr, "Missing file referenced by JSON: %s\n", files[i]);
            goto fail;
        }

        int doCompress = compressFlags[i];
        if (i == 0) doCompress = 0; // first file always raw

        fat[i].relativeOffset = (uint32_t)offset;

        if (doCompress) {
            size_t compSize = 0;
            uint8_t *comp = lz10_compress(buf, sz, &compSize);
            if (!comp) {
                fprintf(stderr, "Compression failed for %s\n", files[i]);
                free(buf);
                goto fail;
            }

            size_t paddedComp = pad_size(compSize, 4);
            fwrite(comp, 1, compSize, out);

            for (size_t p = compSize; p < paddedComp; ++p)
                fputc(0x00, out);

            fat[i].inputSize = (uint32_t)paddedComp;
            fat[i].outputSize = (uint32_t)pad_size(sz, 4);
            offset += paddedComp;

            free(comp);
        } else {
            size_t padded = pad_size(sz, 4);
            fwrite(buf, 1, sz, out);

            for (size_t p = sz; p < padded; ++p)
                fputc(0x00, out);

            fat[i].inputSize = 0;
            fat[i].outputSize = (uint32_t)padded;
            offset += padded;
        }

        free(buf);

        if ((i & 31u) == 31u || i == numFiles - 1) {
            printf("\r  packed %zu/%zu", i + 1, numFiles);
            fflush(stdout);
        }
    }

    printf("\n");

    hdr.dataStart = (uint32_t)(hdr.headerSize + numFiles * sizeof(FATEntry));

    fseek(out, 0, SEEK_SET);
    fwrite(&hdr, 1, sizeof(hdr), out);

    fseek(out, (long)hdr.headerSize, SEEK_SET);
    fwrite(fat, sizeof(*fat), numFiles, out);

    cleanup_build(out, fat, files, compressFlags, numFiles, jsonNames, jsonStates, jsonCount);

    return EXIT_SUCCESS;

    fail:
    cleanup_build(out, fat, files, compressFlags, numFiles, jsonNames, jsonStates, jsonCount);
    return EXIT_FAILURE;
}

int main(int argc, char **argv) {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
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
            return extract_acf(path);
        }
    } else if (!strcmp(mode, "-b") || !strcmp(mode, "--build")) {
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Invalid path: '%s'\n", path);
            return EXIT_FAILURE;
        }

        printf("Building ACF from directory: %s\n", path);
        return build_acf(path);
    } else {
        fprintf(stderr, "Unknown option: %s\n", mode);
        fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
