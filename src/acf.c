/*
 * ACF archive extraction and building.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#include "acf.h"

#include "filelist.h"
#include "fs.h"
#include "lz10.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE    0x20 // the FAT immediately follows the header
#define FAT_ENTRY_SIZE 12 // relative offset, output size, input size
#define UNUSED_ENTRY   0xFFFFFFFFu // relative offset of an absent entry
#define NAME_SIZE      16 // "NNNN.EXT" entry name, null terminator included

/*
 * Read a 32-bit little-endian integer.
 */
static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16
        | (uint32_t)p[3] << 24;
}

/*
 * Write a 32-bit little-endian integer.
 */
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/*
 * Compute the number of padding bytes required to align 'n' to the next
 * 4-byte boundary.
 */
static size_t pad4(size_t n)
{
    return (4 - (n & 3)) & 3;
}

/*
 * Check whether data is a Guardian Signs MES text file, as ra3mes reads it:
 * its size, a string count, then offsets to null-terminated strings in order.
 */
static bool is_mes(const uint8_t *data, size_t size)
{
    if (size < 8 || get_u32(data) != size) {
        return false;
    }

    // the offset table must fit within the file
    uint32_t count = get_u32(data + 4);
    if (count > (size - 8) / 4) {
        return false;
    }
    size_t tableEnd = 8 + (size_t)count * 4;

    // strings follow the table in order, each null-terminated before the next
    for (uint32_t i = 0; i < count; ++i) {
        size_t off = get_u32(data + 8 + (size_t)i * 4);
        size_t end = i + 1 < count ? get_u32(data + 12 + (size_t)i * 4) : size;
        if (off < tableEnd || end <= off || end > size
            || !memchr(data + off, 0, end - off)) {
            return false;
        }
    }
    return true;
}

/*
 * Derive a file extension from the leading bytes of data, or "bin".
 */
static void get_extension(const uint8_t *data, size_t size, char ext[5])
{
    // known Nintendo container magic strings whose bytes appear reversed
    static const char *const reversed[]
        = { "RGCN", "RLCN", "RECN", "RNAN", "RCSN", "RTFN" };

    // MES text files have no magic string, but a recognizable structure
    if (is_mes(data, size)) {
        memcpy(ext, "mes", 4);
        return;
    }

    // stop at the first non-alphanumeric byte
    size_t n = 0;
    while (n < 4 && n < size && isalnum(data[n])) {
        ext[n] = (char)data[n];
        ++n;
    }
    ext[n] = '\0';

    if (n <= 2) { // too few bytes to form a useful extension
        memcpy(ext, "bin", 4);
        return;
    }

    for (size_t i = 0; i < sizeof(reversed) / sizeof(reversed[0]); ++i) {
        if (strcasecmp(ext, reversed[i]) == 0) {
            char t = ext[0];
            ext[0] = ext[3];
            ext[3] = t;
            t = ext[1];
            ext[1] = ext[2];
            ext[2] = t;
            return;
        }
    }
}

/*
 * Print progress every 32 entries and on the last one, in normal mode.
 */
static void print_progress(
    const char *path, const char *verb, uint32_t i, uint32_t count)
{
    if (verbose == 1 && ((i & 31u) == 31u || i + 1 == count)) {
        printf("\r  %s: %s %u/%u", path_basename(path), verb, i + 1, count);
        fflush(stdout);
    }
}

/*
 * Extract all files from an ACF archive into a directory, along with the
 * filelist.json file listing them.
 */
bool acf_extract(const char *path, const char *outdir)
{
    size_t fileSize = 0;
    uint8_t *fileData = read_file(path, &fileSize);
    if (!fileData) {
        return false;
    }

    if (fileSize < HEADER_SIZE || memcmp(fileData, "acf", 3) != 0) {
        fprintf(stderr, "%s: '%s' is not an ACF archive\n", programName, path);
        free(fileData);
        return false;
    }

    uint32_t fatOffset = get_u32(fileData + 0x04); // header size
    uint32_t dataStart = get_u32(fileData + 0x08);
    uint32_t numFiles = get_u32(fileData + 0x0C);

    if (fatOffset > fileSize
        || numFiles > (fileSize - fatOffset) / FAT_ENTRY_SIZE) {
        fprintf(stderr,
            "%s: FAT table in '%s' exceeds file size\n",
            programName,
            path);
        free(fileData);
        return false;
    }

    // entry names are stored right after the entries
    FileState *entries
        = malloc((size_t)numFiles * (sizeof(*entries) + NAME_SIZE) + 1);
    char outname[PATH_SIZE];
    if (!entries || !mkdir_dir(outdir)) {
        free(entries);
        free(fileData);
        return false;
    }
    char *names = (char *)(entries + numFiles);

    for (uint32_t i = 0; i < numFiles; ++i) {
        const uint8_t *e = fileData + fatOffset + (size_t)i * FAT_ENTRY_SIZE;
        uint32_t relativeOffset = get_u32(e);
        uint32_t outputSize = get_u32(e + 4);
        uint32_t inputSize = get_u32(e + 8); // 0 if not compressed

        uint64_t dataOffset = (uint64_t)dataStart + relativeOffset;
        uint32_t storedSize = inputSize ? inputSize : outputSize;

        const uint8_t *data = NULL; // entry contents, NULL if absent
        size_t size = 0;
        uint8_t *dec = NULL;

        // sentinel value marks an absent entry
        if (relativeOffset != UNUSED_ENTRY) {
            if (dataOffset + storedSize > fileSize) {
                fprintf(stderr,
                    "%s: '%s': entry %u: data exceeds file size\n",
                    programName,
                    path,
                    i);
            } else {
                data = fileData + (size_t)dataOffset;
                size = storedSize;
            }
        }

        if (data && inputSize && data[0] == 0x10) { // LZ10 compression type
            dec = lz10_decompress(data, size, &size);
            if (dec) {
                data = dec;
            } else {
                fprintf(stderr,
                    "%s: '%s': entry %u: decompression failed, saved "
                    "raw\n",
                    programName,
                    path,
                    i);
            }
        }

        char ext[5] = "bin";
        entries[i].name = names + (size_t)i * NAME_SIZE;
        entries[i].state = -1;
        if (data) {
            get_extension(data, size, ext);
            entries[i].state = dec != NULL;
        }
        snprintf(entries[i].name, NAME_SIZE, "%04u.%s", i, ext);

        if (data
            && join_path(outname, sizeof(outname), outdir, entries[i].name)) {
            (void)write_file(outname, data, size); // reports its own errors
        }

        free(dec);
        print_progress(path, "extracted", i, numFiles);
    }

    if (verbose == 1 && numFiles) {
        printf("\n");
    }

    bool ok = join_path(outname, sizeof(outname), outdir, "filelist.json")
        && filelist_write(outname, entries, numFiles);

    free(entries);
    free(fileData);
    return ok;
}

/*
 * Pack the contents of a directory into a new ACF archive, guided by the
 * filelist.json file found inside the directory.
 */
bool acf_build(const char *dir, const char *outname)
{
    char path[PATH_SIZE];
    char *text = NULL;
    FileState *entries = NULL;
    uint32_t count = 0;
    uint8_t *fat = NULL;
    FILE *out = NULL;
    bool ok = false;

    if (!join_path(path, sizeof(path), dir, "filelist.json")
        || !filelist_read(path, &text, &entries, &count)) {
        fprintf(stderr, "%s: '%s' is missing or invalid\n", programName, path);
        return false;
    }

    if (count == 0) {
        fprintf(stderr, "%s: '%s': no files to pack\n", programName, path);
        goto done;
    }

    // filelist validation: entries must be contiguous and ordered
    for (uint32_t i = 0; i < count; ++i) {
        const char *name = entries[i].name;
        if (strrchr(name, '.') != name + 4 || name[5] == '\0'
            || !isdigit((unsigned char)name[0])
            || !isdigit((unsigned char)name[1])
            || !isdigit((unsigned char)name[2])
            || !isdigit((unsigned char)name[3])
            || strtoul(name, NULL, 10) != i) {
            fprintf(stderr,
                "%s: '%s': expected key %04u.EXT, got '%s'\n",
                programName,
                path,
                i,
                name);
            goto done;
        }
    }

    size_t fatSize = (size_t)count * FAT_ENTRY_SIZE;
    fat = calloc(count, FAT_ENTRY_SIZE);
    if (!fat) {
        fprintf(stderr, "%s: memory allocation failed\n", programName);
        goto done;
    }

    out = xfopen(outname, "wb");
    if (!out) {
        fprintf(stderr, "%s: cannot create '%s'\n", programName, outname);
        goto done;
    }

    uint8_t header[HEADER_SIZE] = { 'a', 'c', 'f', '\0' };
    put_u32(header + 0x04, HEADER_SIZE);
    put_u32(header + 0x08, (uint32_t)(HEADER_SIZE + fatSize));
    put_u32(header + 0x0C, count);
    put_u32(header + 0x10, 1); // unknown1
    put_u32(header + 0x14, 0x32); // unknown2

    // placeholder FAT, patched below after data is written
    if (fwrite(header, 1, sizeof(header), out) != sizeof(header)
        || fwrite(fat, 1, fatSize, out) != fatSize) {
        fprintf(stderr, "%s: failed to write '%s'\n", programName, outname);
        goto done;
    }

    static const uint8_t zeroPad[4] = { 0 };

    uint32_t offset = 0; // running byte offset into the data region
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t *e = fat + (size_t)i * FAT_ENTRY_SIZE;

        if (entries[i].state < 0) { // null state: absent entry
            put_u32(e, UNUSED_ENTRY);
        } else {
            uint8_t *buf = NULL;
            size_t sz = 0;
            if (join_path(path, sizeof(path), dir, entries[i].name)) {
                buf = read_file(path, &sz);
            }
            if (!buf) {
                fprintf(stderr,
                    "%s: '%s' is listed in filelist.json but cannot be "
                    "read\n",
                    programName,
                    path);
                goto done;
            }

            uint8_t *comp = NULL;
            size_t compSize = 0;
            if (entries[i].state) {
                comp = lz10_compress(buf, sz, &compSize);
                if (!comp) {
                    fprintf(stderr,
                        "%s: compression failed for '%s'\n",
                        programName,
                        path);
                    free(buf);
                    goto done;
                }
            }

            const uint8_t *data = comp ? comp : buf;
            size_t len = comp ? compSize : sz;
            size_t pad = pad4(len); // pad to 4-byte boundary
            bool written = fwrite(data, 1, len, out) == len
                && fwrite(zeroPad, 1, pad, out) == pad;

            put_u32(e, offset);
            put_u32(e + 4, (uint32_t)(sz + pad4(sz))); // padded output size
            put_u32(e + 8, comp ? (uint32_t)(len + pad) : 0); // padded input
            offset += (uint32_t)(len + pad);

            free(comp);
            free(buf);
            if (!written) {
                fprintf(
                    stderr, "%s: failed to write '%s'\n", programName, outname);
                goto done;
            }
        }

        print_progress(dir, "packed", i, count);
    }

    if (verbose == 1) {
        printf("\n");
    }

    // patch the FAT with the computed offsets and sizes
    if (fseek(out, HEADER_SIZE, SEEK_SET) != 0
        || fwrite(fat, 1, fatSize, out) != fatSize) {
        fprintf(stderr, "%s: failed to write '%s'\n", programName, outname);
        goto done;
    }

    ok = true;

done:
    if (out && fclose(out) != 0 && ok) {
        fprintf(stderr, "%s: failed to write '%s'\n", programName, outname);
        ok = false;
    }
    free(fat);
    free(entries);
    free(text);
    return ok;
}
