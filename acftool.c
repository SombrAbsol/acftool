// Copyright (c) 2026 SombrAbsol

#include "acftool.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

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
    if (is_invertible(outExt)) reverse_str_inplace(outExt); // reverse if needed
    return outExt;
}

// lz10 decompression
uint8_t *lz10_decompress(const uint8_t *src, size_t srcSize, size_t *outSize) {
    if (!src || srcSize < 4 || !outSize) return NULL; // invalid input

    uint8_t method = src[0];
    if (method != 0x10) return NULL; // check compression type

    // read decompressed size from header
    uint32_t decSize = (uint32_t)src[1] | ((uint32_t)src[2] << 8) | ((uint32_t)src[3] << 16);
    if (decSize == 0) return NULL;

    uint8_t *dst = malloc(decSize); // allocate output buffer
    if (!dst) return NULL;

    const uint8_t *sp = src + 4;
    const uint8_t *send = src + srcSize;
    uint8_t *dp = dst;
    uint8_t *dend = dst + decSize;

    // main decompression loop
    while (dp < dend && sp < send) {
        uint8_t flags = *sp++;
        for (int bit = 0; bit < 8 && dp < dend; ++bit) {
            if ((flags & 0x80) == 0) { // literal byte
                if (sp >= send) { free(dst); return NULL; }
                *dp++ = *sp++;
            } else { // compressed block
                if (sp + 1 >= send) { free(dst); return NULL; }
                uint8_t b1 = *sp++;
                uint8_t b2 = *sp++;
                int length = (b1 >> 4) + 3; // length of copy
                size_t disp = (size_t)((((b1 & 0x0F) << 8) | b2) + 1); // displacement

                if ((size_t)(dp - dst) < disp) { free(dst); return NULL; }

                uint8_t *src_copy = dp - disp;
                for (int k = 0; k < length && dp < dend; ++k) {
                    *dp++ = *src_copy++;
                }
            }
            flags <<= 1;
        }
    }

    if (dp != dend) { free(dst); return NULL; }
    *outSize = decSize;
    return dst;
}

// lz10 compression
uint8_t *lz10_compress(const uint8_t *src, size_t srcSize, size_t *outSize) {
    if (!src || !outSize)
        return NULL;

    // worst-case size
    size_t maxSize = 4 + srcSize + ((srcSize + 7) >> 3);
    uint8_t *out = calloc(maxSize, 1);
    if (!out)
        return NULL;

    // header: 0x10 + 24-bit raw size
    out[0] = 0x10;
    out[1] = (uint8_t)(srcSize & 0xFF);
    out[2] = (uint8_t)((srcSize >> 8) & 0xFF);
    out[3] = (uint8_t)((srcSize >> 16) & 0xFF);

    const uint8_t *raw = src;
    const uint8_t *rawEnd = src + srcSize;
    uint8_t *pak = out + 4;

    uint8_t *flagp = NULL;
    uint8_t mask = 0;

    while (raw < rawEnd) {
        // start a new flag byte every 8 symbols
        if (!(mask >>= 1)) {
            flagp = pak++;
            *flagp = 0;
            mask = 0x80;
        }

        size_t bestLen = 2;
        size_t bestPos = 0;

        size_t maxPos = (size_t)(raw - src);
        if (maxPos > 0x1000)
            maxPos = 0x1000;

        size_t maxLen = (size_t)(rawEnd - raw);
        if (maxLen > 0x12)
            maxLen = 0x12;

        for (size_t p = maxPos; p > 1; --p) {
            if (raw[0] != raw[-(ptrdiff_t)p])
                continue;

            size_t l = 1;
            const uint8_t *a = raw + 1;
            const uint8_t *b = raw - p + 1;

            while (l < maxLen && *a == *b) {
                ++a;
                ++b;
                ++l;
            }

            if (l > bestLen) {
                bestLen = l;
                bestPos = p;
                if (l == maxLen)
                    break;
            }
        }

        if (bestLen > 2) {
            // compressed block
            *flagp |= mask;

            size_t lenField = bestLen - (2 + 1);
            size_t posField = bestPos - 1;

            *pak++ = (uint8_t)((lenField << 4) | (posField >> 8));
            *pak++ = (uint8_t)(posField & 0xFF);

            raw += bestLen;
        } else {
            *pak++ = *raw++; // literal byte
        }
    }

    *outSize = (size_t)(pak - out);
    return out;
}

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

    if (!strcmp(mode, "-x") || !strcmp(mode, "--extract")) { // extract mode
        struct stat st;
        if (stat(path, &st) != 0) {
            fprintf(stderr, "Invalid path: '%s'\n", path);
            return EXIT_FAILURE;
        }

        if (S_ISDIR(st.st_mode)) { // directory
            printf("Extracting all ACFs in directory: %s\n", path);
            process_directory(path);
        } else { // single file
            extract_acf(path);
        }
    } else if (!strcmp(mode, "-b") || !strcmp(mode, "--build")) { // build mode
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
