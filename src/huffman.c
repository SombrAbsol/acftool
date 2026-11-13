/*
 * Huffman compression and decompression.
 *
 * SPDX-FileCopyrightText: 2026 SombrAbsol
 *
 * SPDX-License-Identifier: MIT
 */

#include "huffman.h"

#include "fs.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define SYMBOLS     256 // 8-bit symbols
#define MAX_NODES   (2 * SYMBOLS - 1)
#define LEFT_LEAF   0x80 // tree node flag: its left child is a symbol
#define RIGHT_LEAF  0x40 // tree node flag: its right child is a symbol
#define NEXT_OFFSET 0x3F // tree node: offset to its child pair, minus one
#define REACH       64 // child pairs a node can reach after its own one

/*
 * Node of the Huffman tree, a leaf if 'left' is negative.
 */
typedef struct {
    uint32_t weight;
    int left, right;
    int symbol;
} Node;

/*
 * Bits written most significant first, in 32-bit little-endian words.
 */
typedef struct {
    uint8_t *pak; // next output word
    uint64_t bits;
    int count; // bits not written yet, fewer than 32
} BitWriter;

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
 * Append the 'count' low bits of 'value', 1 to 32 of them.
 */
static void put_bits(BitWriter *w, uint32_t value, int count)
{
    w->bits = w->bits << count | value;
    w->count += count;
    if (w->count >= 32) {
        w->count -= 32;
        put_u32(w->pak, (uint32_t)(w->bits >> w->count));
        w->pak += 4;
    }
}

/*
 * Decompress a Huffman buffer of 4-bit or 8-bit symbols. Return NULL without
 * printing anything if it is not a valid Huffman stream or memory runs out.
 */
uint8_t *huffman_decompress(const uint8_t *src, size_t srcSize, size_t *outSize)
{
    // compression type 0x24 or 0x28, for 4-bit or 8-bit symbols
    if (srcSize < 5 || (src[0] != 0x24 && src[0] != 0x28)) {
        return NULL;
    }

    size_t decSize
        = (size_t)src[1] | (size_t)src[2] << 8 | (size_t)src[3] << 16;
    int bits = src[0] & 0x0F; // bits per symbol

    // the tree, its size byte included, then the stream of codes
    const uint8_t *tree = src + 4;
    size_t treeSize = ((size_t)tree[0] + 1) * 2;
    if (srcSize - 4 < treeSize) {
        return NULL;
    }

    // zeroed as 4-bit symbols are ORed in; an empty stream gives an empty
    // buffer
    uint8_t *dst = calloc(decSize ? decSize : 1, 1);
    if (!dst) {
        return NULL;
    }

    const uint8_t *sp = tree + treeSize; // source pointer
    const uint8_t *send = src + srcSize; // source end
    uint8_t *dp = dst; // destination pointer
    uint8_t *dend = dst + decSize; // destination end
    size_t node = 1; // current tree node, starting from the root
    int shift = 0; // bits of the current output byte already written

    /*
     * Each bit goes left (0) or right (1) from the root. A node's children
     * follow its offset, and a leaf adds its symbol to the output, low bits
     * first.
     */
    while (dp < dend) {
        if (send - sp < 4) {
            goto invalid;
        }
        uint32_t word = get_u32(sp);
        sp += 4;

        // process 32 bits (MSB first)
        for (int b = 31; b >= 0 && dp < dend; --b) {
            unsigned bit = word >> b & 1;
            size_t child = (node & ~(size_t)1) + (tree[node] & NEXT_OFFSET) * 2
                + 2 + bit;
            if (child >= treeSize) {
                goto invalid; // beyond the end of the tree
            }
            if (!(tree[node] & (bit ? RIGHT_LEAF : LEFT_LEAF))) {
                node = child;
                continue;
            }
            *dp |= (uint8_t)((tree[child] & ((1u << bits) - 1)) << shift);
            shift = (shift + bits) & 7;
            dp += shift == 0;
            node = 1;
        }
    }

    *outSize = decSize;
    return dst;

invalid:
    free(dst);
    return NULL;
}

/*
 * Give each symbol below a node its code, the path from the root, 0 going
 * left. Codes fit in 64 bits, as 24-bit sizes keep them under 40 bits.
 */
static void assign_codes(const Node *t,
    int i,
    uint64_t code,
    int length,
    uint64_t *codes,
    int *lengths)
{
    if (t[i].left < 0) {
        codes[t[i].symbol] = code;
        lengths[t[i].symbol] = length;
        return;
    }
    assign_codes(t, t[i].left, code << 1, length + 1, codes, lengths);
    assign_codes(t, t[i].right, code << 1 | 1, length + 1, codes, lengths);
}

/*
 * Count the child pairs needed below a node, its own included, 0 for a leaf.
 */
static int count_pairs(const Node *t, int i, int *pairs)
{
    if (t[i].left < 0) {
        return pairs[i] = 0;
    }
    pairs[i] = 1 + count_pairs(t, t[i].left, pairs)
        + count_pairs(t, t[i].right, pairs);
    return pairs[i];
}

/*
 * Table being laid out: the index of each node placed, and the next free pair.
 */
typedef struct {
    const Node *t;
    uint8_t *table;
    int at[MAX_NODES];
    int next;
} Layout;

/*
 * Give a placed node the next free pair for its children, writing its offset,
 * its flags and its leaves. Return false if the pair is out of reach.
 */
static bool place(Layout *l, int x)
{
    const Node *t = l->t;
    int a = t[x].left, b = t[x].right;
    int offset = l->next - (l->at[x] >> 1) - 1;
    if (offset < 0 || offset > NEXT_OFFSET) {
        fprintf(stderr, "%s: Huffman tree too deep\n", programName);
        return false;
    }
    l->table[l->at[x]] = (uint8_t)(offset | (t[a].left < 0 ? LEFT_LEAF : 0)
        | (t[b].left < 0 ? RIGHT_LEAF : 0));
    l->at[a] = 2 * l->next;
    l->at[b] = 2 * l->next + 1;
    ++l->next;
    if (t[a].left < 0) {
        l->table[l->at[a]] = (uint8_t)t[a].symbol;
    }
    if (t[b].left < 0) {
        l->table[l->at[b]] = (uint8_t)t[b].symbol;
    }
    return true;
}

/*
 * Lay the tree out in its table as the game's own files are: a whole subtree
 * at once, breadth first, when the nodes left waiting can still be reached.
 */
static bool layout_tree(const Node *t, int root, uint8_t *table)
{
    Layout l = { t, table, { 0 }, 1 };
    int pairs[MAX_NODES], waiting[SYMBOLS], slack[SYMBOLS];
    int count = 0;
    l.at[root] = 1;
    waiting[count++] = root;
    count_pairs(t, root, pairs);

    while (count) {
        /*
         * A waiting node can get its pair up to REACH pairs after its own.
         * The waiting nodes are sorted by the pairs they can still wait, then
         * by their place in the table.
         */
        for (int i = 0; i < count; ++i) {
            int x = waiting[i], j = i;
            int s = (l.at[x] >> 1) + REACH - l.next;
            for (; j > 0
                && (slack[j - 1] > s
                    || (slack[j - 1] == s && l.at[waiting[j - 1]] > l.at[x]));
                --j) {
                waiting[j] = waiting[j - 1];
                slack[j] = slack[j - 1];
            }
            waiting[j] = x;
            slack[j] = s;
        }

        /*
         * The subtree of a node fits if it can wait for all its pairs, and if
         * every other node still gets its own pair in time after it, the node
         * itself counted ahead of them as one more pair. Of the subtrees that
         * fit, the one highest in the table is laid out.
         */
        int best = -1;
        for (int c = 0; c < count; ++c) {
            int need = pairs[waiting[c]], bound = REACH;
            for (int o = 0; o < count; ++o) {
                int rank = o + 1 + (o < c && slack[o] == slack[c]);
                if (o != c && slack[o] - rank + 1 < bound) {
                    bound = slack[o] - rank + 1;
                }
            }
            if (need <= slack[c] && need <= bound
                && (best < 0 || l.at[waiting[c]] > l.at[waiting[best]])) {
                best = c;
            }
        }
        if (best >= 0) {
            int queue[MAX_NODES], head = 0, tail = 0;
            queue[tail++] = waiting[best];
            waiting[best] = waiting[--count];
            while (head < tail) {
                int x = queue[head++];
                if (!place(&l, x)) {
                    return false;
                }
                if (t[t[x].left].left >= 0) {
                    queue[tail++] = t[x].left;
                }
                if (t[t[x].right].left >= 0) {
                    queue[tail++] = t[x].right;
                }
            }
            continue;
        }

        // otherwise the node that can wait the least, the larger first
        int u = 0;
        for (int i = 1; i < count && slack[i] == slack[0]; ++i) {
            if (pairs[waiting[i]] > pairs[waiting[u]]) {
                u = i;
            }
        }
        int x = waiting[u];
        waiting[u] = waiting[--count];
        if (!place(&l, x)) {
            return false;
        }
        if (t[t[x].left].left >= 0) {
            waiting[count++] = t[x].left;
        }
        if (t[t[x].right].left >= 0) {
            waiting[count++] = t[x].right;
        }
    }
    return true;
}

/*
 * Compress a buffer with Huffman, using 8-bit symbols and a tree laid out as in
 * the game's own files. Return NULL on failure after printing the reason.
 */
uint8_t *huffman_compress(const uint8_t *src, size_t srcSize, size_t *outSize)
{
    if (srcSize > 0xFFFFFF) {
        fprintf(stderr, "%s: file too large to compress\n", programName);
        return NULL;
    }

    // one leaf per symbol used, two at least for the decoder
    uint32_t freq[SYMBOLS] = { 0 };
    for (size_t i = 0; i < srcSize; ++i) {
        ++freq[src[i]];
    }
    Node t[MAX_NODES];
    int n = 0;
    for (int s = 0; s < SYMBOLS; ++s) {
        if (freq[s]) {
            t[n++] = (Node) { freq[s], -1, -1, s };
        }
    }
    for (int s = 0; n < 2; ++s) {
        if (!freq[s]) {
            t[n++] = (Node) { 1, -1, -1, s };
        }
    }

    // the two lightest nodes are merged until one is left, the first on ties
    int leaves = n;
    bool merged[MAX_NODES] = { false };
    while (n < 2 * leaves - 1) {
        int a = -1, b = -1;
        for (int i = 0; i < n; ++i) {
            if (merged[i]) {
                continue;
            }
            if (a < 0 || t[i].weight < t[a].weight) {
                b = a;
                a = i;
            } else if (b < 0 || t[i].weight < t[b].weight) {
                b = i;
            }
        }
        merged[a] = merged[b] = true;
        t[n++] = (Node) { t[a].weight + t[b].weight, a, b, 0 };
    }

    uint64_t codes[SYMBOLS];
    int lengths[SYMBOLS] = { 0 };
    uint64_t total = 0; // bits of the stream
    assign_codes(t, n - 1, 0, 0, codes, lengths);
    for (int s = 0; s < SYMBOLS; ++s) {
        total += (uint64_t)freq[s] * (uint64_t)lengths[s];
    }

    // the header, then the tree padded to 4 bytes, then the stream of codes
    size_t treeSize = ((size_t)(leaves + 1) & ~(size_t)1) * 2;
    size_t size = 4 + treeSize + (size_t)((total + 31) / 32) * 4;
    uint8_t *out = calloc(size, 1);
    if (!out) {
        fprintf(stderr, "%s: memory allocation failed\n", programName);
        return NULL;
    }

    // write header
    out[0] = 0x28;
    out[1] = (uint8_t)srcSize;
    out[2] = (uint8_t)(srcSize >> 8);
    out[3] = (uint8_t)(srcSize >> 16);
    out[4] = (uint8_t)(treeSize / 2 - 1); // tree size byte
    if (!layout_tree(t, n - 1, out + 4)) {
        free(out);
        return NULL;
    }

    // codes longer than 32 bits are written in two parts
    BitWriter w = { out + 4 + treeSize, 0, 0 };
    for (size_t i = 0; i < srcSize; ++i) {
        uint64_t code = codes[src[i]];
        int length = lengths[src[i]];
        if (length > 32) {
            put_bits(&w, (uint32_t)(code >> 32), length - 32);
            length = 32;
        }
        put_bits(&w, (uint32_t)code, length);
    }
    if (w.count) {
        put_bits(&w, 0, 32 - w.count);
    }
    *outSize = size;
    return out;
}
