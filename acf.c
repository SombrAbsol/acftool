// Copyright (c) 2026 SombrAbsol

#include "acf.h"
#include "utils.h"
#include "lz10.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

// extract files from acf archive
int extract_acf(const char *path) {
    if (!path) return EXIT_FAILURE;

    size_t fileSize;
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

    // read header
    ACFHeader hdr;
    memcpy(&hdr, fileData, sizeof(hdr));

    if (memcmp(hdr.magic, "acf", 3) != 0) {
        fprintf(stderr, "%s doesn't have an 'acf\\0' header\n", path);
        free(fileData);
        return EXIT_FAILURE;
    }

    size_t fatOffset = hdr.headerSize;
    if (hdr.numFiles * sizeof(FATEntry) > fileSize - fatOffset) {
        fprintf(stderr, "%s: FAT table exceeds file size\n", path);
        free(fileData);
        return EXIT_FAILURE;
    }

    FATEntry *entries = (FATEntry*)(fileData + fatOffset);

    // create output directory
    char outdir[512];
    strncpy(outdir, path, sizeof(outdir)-1);
    outdir[sizeof(outdir)-1] = '\0';
    char *dot = strrchr(outdir, '.'); if (dot) *dot = '\0';
    (void)mkdir_dir(outdir);

    // create metadata file
    char metafile[768];
    snprintf(metafile, sizeof(metafile), "%s/filelist.txt", outdir);
    FILE *mf = fopen(metafile, "w");
    if (!mf) {
        fprintf(stderr, "Cannot create metadata file\n");
        free(fileData);
        return EXIT_FAILURE;
    }

    char extBuf[16];
    for (uint32_t i = 0; i < hdr.numFiles; ++i) {
        FATEntry e = entries[i];
        if (e.relativeOffset == 0xFFFFFFFFu) { // unused entry
            fprintf(mf, "%04u -1\n", i);
            continue;
        }

        size_t dataOffset = (size_t)hdr.dataStart + (size_t)e.relativeOffset;
        if (dataOffset >= fileSize) {
            fprintf(stderr, "Entry %u: offset out of range\n", i);
            continue;
        }

        const uint8_t *src = fileData + dataOffset;
        uint8_t *outBuf = NULL;
        size_t outSize = 0;
        int compressed = 0;

        // decompress if needed
        if (e.inputSize > 0 && src[0] == 0x10) {
            outBuf = lz10_decompress(src, (size_t)e.inputSize, &outSize);
            if (!outBuf) {
                fprintf(stderr, "Decompression failed for entry %u, saving raw\n", i);
                outSize = (size_t)e.inputSize;
                outBuf = malloc(outSize ? outSize : 1);
                if (!outBuf) {
                    fprintf(stderr, "OOME\n");
                    free(fileData);
                    fclose(mf);
                    return EXIT_FAILURE;
                }
                memcpy(outBuf, src, outSize);
            } else {
                compressed = 1;
            }
        } else { // copy raw data
            outSize = (size_t)e.outputSize;
            outBuf = malloc(outSize ? outSize : 1);
            if (!outBuf) {
                fprintf(stderr, "OOME\n");
                free(fileData);
                fclose(mf);
                return EXIT_FAILURE;
            }
            memcpy(outBuf, src, outSize);
        }

        // detect file extension
        const char *ext = try_get_extension(outBuf, outSize, 4, 2, "bin", extBuf, sizeof(extBuf));

        // write file
        char outname[768];
        snprintf(outname, sizeof(outname), "%s/%04u.%s", outdir, i, ext);
        if (write_file(outname, outBuf, outSize) != 0)
            fprintf(stderr, "Failed writing %s\n", outname);

        fprintf(mf, "%s %d\n", outname + strlen(outdir) + 1, compressed);
        free(outBuf);

        // progress display
        if ((i & 31) == 31 || i == hdr.numFiles - 1) {
            printf("\r  extracted %u/%u", i + 1, hdr.numFiles);
            fflush(stdout);
        }
    }

    printf("\n");
    fclose(mf);
    free(fileData);
    return EXIT_SUCCESS;
}

// process all acf archives in a directory
#ifdef _WIN32
void process_directory(const char *directory) {
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
        snprintf(fullPath, sizeof(fullPath), "%s\\%s", directory, file.name);
        extract_acf(fullPath);
    } while (_findnext(hFile, &file) == 0);

    _findclose(hFile);
}
#else
void process_directory(const char *directory) {
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
            snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, name);
            extract_acf(fullPath);
        }
    }

    closedir(dir);
}
#endif

// build an acf archive from a directory
int build_acf(const char *directory) {
    if (!directory) return EXIT_FAILURE;

    // read metadata from filelist.txt
    char metafile[512];
    snprintf(metafile, sizeof(metafile), "%s/filelist.txt", directory);
    FILE *mf = fopen(metafile, "r");
    char **files = NULL;
    int *compressFlags = NULL; // -1 = unused; 0 = raw; 1 = compressed
    size_t numFiles = 0;

    if (mf) {
        char line[1024];
        while (fgets(line, sizeof(line), mf)) {
            if (line[0] == '#' || strlen(line) < 3) continue;

            char id[32];
            int flag;
            if (sscanf(line, "%31s %d", id, &flag) != 2) continue;

            // determine the index from the id
            int index = atoi(id);
            if ((size_t)(index + 1) > numFiles) {
                // expand arrays if needed
                char **tmpFiles = realloc(files, (index + 1) * sizeof(char*));
                if (!tmpFiles) {
                    for (size_t j = 0; j < numFiles; j++) free(files[j]);
                    free(files);
                    free(compressFlags);
                    fclose(mf);
                    fprintf(stderr, "Memory allocation failed\n");
                    return EXIT_FAILURE;
                }
                files = tmpFiles;

                int *tmpFlags = realloc(compressFlags, (index + 1) * sizeof(int));
                if (!tmpFlags) {
                    for (size_t j = 0; j < numFiles; j++) free(files[j]);
                    free(files);
                    free(compressFlags);
                    fclose(mf);
                    fprintf(stderr, "Memory allocation failed\n");
                    return EXIT_FAILURE;
                }
                compressFlags = tmpFlags;

                // initialize new entries
                for (size_t j = numFiles; j <= (size_t)index; j++) {
                    files[j] = NULL;
                    compressFlags[j] = -1; // default to unused
                }

                numFiles = index + 1;
            }

            // store file path if not unused
            if (flag == -1) {
                files[index] = NULL; // mark unused
                compressFlags[index] = -1;
            } else {
                size_t pathlen = strlen(directory) + strlen(id) + 8; // allow for extension
                files[index] = malloc(pathlen);
                if (!files[index]) {
                    for (size_t j = 0; j < numFiles; j++) free(files[j]);
                    free(files);
                    free(compressFlags);
                    fclose(mf);
                    fprintf(stderr, "Memory allocation failed\n");
                    return EXIT_FAILURE;
                }
                snprintf(files[index], pathlen, "%s/%s", directory, id);
                compressFlags[index] = flag;
            }
        }
        fclose(mf);
    } else {
        fprintf(stderr, "Metadata file not found in %s\n", directory);
        return EXIT_FAILURE;
    }

    if (numFiles == 0) { fprintf(stderr, "No files to pack\n"); return EXIT_FAILURE; }

    // create output acf archive
    char outname[512];
    snprintf(outname, sizeof(outname), "%s.acf", directory);
    FILE *out = fopen(outname, "wb");
    if (!out) {
        fprintf(stderr, "Cannot create %s\n", outname);
        for (size_t i = 0; i < numFiles; i++) free(files[i]);
        free(files);
        free(compressFlags);
        return EXIT_FAILURE;
    }

    // initialize header
    ACFHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, "acf", 4);
    hdr.headerSize = sizeof(ACFHeader);
    hdr.numFiles = (uint32_t)numFiles;
    hdr.dataStart = 0;
    hdr.unknown1 = 1;
    hdr.unknown2 = 0x32;
    hdr.padding[0] = hdr.padding[1] = 0;

    uint8_t *headerBuf = calloc(1, hdr.headerSize);
    if (!headerBuf) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(out);
        for (size_t i = 0; i < numFiles; i++) free(files[i]);
        free(files);
        free(compressFlags);
        return EXIT_FAILURE;
    }

    memcpy(headerBuf, &hdr, sizeof(hdr));
    fwrite(headerBuf, 1, hdr.headerSize, out);
    free(headerBuf);

    // write empty fat table
    FATEntry *fat = calloc(numFiles, sizeof(FATEntry));
    if (!fat) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(out);
        for (size_t i = 0; i < numFiles; i++) free(files[i]);
        free(files);
        free(compressFlags);
        return EXIT_FAILURE;
    }

    // initialize FAT to unused
    for (size_t i = 0; i < numFiles; i++) {
        fat[i].relativeOffset = 0xFFFFFFFFu;
        fat[i].inputSize = 0;
        fat[i].outputSize = 0;
    }

    fwrite(fat, sizeof(FATEntry), numFiles, out);

    // write files
    size_t offset = 0;
    for (size_t i = 0; i < numFiles; i++) {
        // skip unused entries
        if (compressFlags[i] == -1 || files[i] == NULL) {
            fat[i].relativeOffset = 0xFFFFFFFFu;
            fat[i].inputSize = 0;
            fat[i].outputSize = 0;
            continue;
        }

        size_t sz = 0;
        uint8_t *buf = read_file(files[i], &sz);
        if (!buf) {
            fprintf(stderr, "Skipping %s\n", files[i]);
            fat[i].relativeOffset = 0xFFFFFFFFu;
            fat[i].inputSize = 0;
            fat[i].outputSize = 0;
            continue;
        }

        int doCompress = compressFlags[i];
        if (i == 0) doCompress = 0; // first file always raw

        fat[i].relativeOffset = (uint32_t)offset;

        if (doCompress) {
            size_t compSize = 0;
            uint8_t *comp = lz10_compress(buf, sz, &compSize);

            size_t paddedComp = pad_size(compSize, 4);
            fwrite(comp, 1, compSize, out);

            // padding
            for (size_t p = compSize; p < paddedComp; p++)
                fputc(0x00, out);

            fat[i].inputSize = (uint32_t)paddedComp;
            fat[i].outputSize = (uint32_t)pad_size(sz, 4);

            offset += paddedComp;
            free(comp);
        } else {
            size_t padded = pad_size(sz, 4);
            fwrite(buf, 1, sz, out);

            // padding
            for (size_t p = sz; p < padded; p++)
                fputc(0x00, out);

            fat[i].inputSize = 0;
            fat[i].outputSize = (uint32_t)padded;
            offset += padded;
        }

        free(buf);

        if ((i & 31) == 31 || i == numFiles - 1) {
            printf("\r  packed %zu/%zu", i + 1, numFiles);
            fflush(stdout);
        }
    }

    printf("\n");

    // update header and fat table
    hdr.dataStart = (uint32_t)(hdr.headerSize + numFiles * sizeof(FATEntry));
    fseek(out, 0, SEEK_SET);
    fwrite(&hdr, 1, sizeof(hdr), out);

    fseek(out, hdr.headerSize, SEEK_SET);
    fwrite(fat, sizeof(FATEntry), numFiles, out);

    fclose(out);

    // free memory
    free(fat);
    for (size_t i = 0; i < numFiles; i++) free(files[i]);
    free(files);
    free(compressFlags);

    printf("Built %s with %zu files. headerSize=0x%X dataStart=0x%X\n",
           outname, numFiles, (unsigned)hdr.headerSize, (unsigned)hdr.dataStart);

    return EXIT_SUCCESS;
}
