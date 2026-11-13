<!--
SPDX-FileCopyrightText: 2026 SombrAbsol

SPDX-License-Identifier: MIT
-->

# ACF File Format
Used in *Pokémon Ranger: Guardian Signs* to store and optionally compress multiple files into a single archive.

## Format specifications
### Overview
* 32-bit little-endian integer values
* File data begins at the absolute offset stored in `data_start`
* Offsets in FAT entries are relative to `data_start`
* The first FAT entry is a regular entry; it may hold file data or be unused
* Files may optionally be LZ10-compressed
* File data is 4-byte aligned; each file is followed by 0 to 3 zero bytes

### Header
```rust
{
  char[4] magic       // "acf\0", or "MBG0" for map archives
  u32     header_size // 0x20
  u32     data_start  // absolute offset to the start of file data
  u32     num_files   // number of FAT entries, unused ones included
  u32     unknown1    // 0x01
  u32     unknown2    // 0x32
  u32[2]  padding     // zero padding
}
```

Immediately followed by `num_files` FAT entries, beginning at byte `header_size` (`0x20`).

### FAT entries
```rust
{
  u32 relative_offset // offset relative to data_start, 4-byte aligned; if 0xFFFFFFFF, skip this entry
  u32 output_size     // size of the output (decompressed) data, rounded up to a multiple of 4
  u32 input_size      // size of the compressed input data if LZ10-compressed, rounded up to a multiple of 4; 0 if not compressed
}
```

### Compression
If `input_size` is non-zero and the first data byte is `0x10`, the file data is LZ10-compressed. In that case:
* Read `input_size` bytes from the archive starting at `data_start + relative_offset`
* Decompress the LZ10 data to obtain the file contents
* The decompressed size is the one stored in the LZ10 header, not `output_size`; it may be less than `output_size` because of the 4-byte rounding, and the output is not padded

If `input_size` is non-zero but the first data byte is not `0x10`, read `input_size` bytes as-is.

If `input_size` is zero, the file data is stored uncompressed. Read `output_size` bytes directly from `data_start + relative_offset`.

## Map archives
The map files of the Japanese release (`data/map/*.acf`) are ACF archives with the `MBG0` magic, every other field being the same. The other releases store these map files as ACZ archives (`data/mapz/*.acz`): the same ACF archives compressed as a whole with Huffman coding.

An ACZ archive is laid out as follows:
```rust
{
  u8    type      // 0x28: Huffman coding of 8-bit symbols (0x24 for 4-bit ones, unused)
  u24   size      // size of the decompressed data
  u8    tree_size // the tree table takes (tree_size + 1) * 2 bytes, this byte included
  u8[]  tree      // the rest of the tree table
  u32[] stream    // the codes of the symbols
}
```

### Tree table
* Indices start at the `tree_size` byte (index 0), and the root node is at index 1
* A node byte holds an offset in its bits 0 to 5, sets bit 6 if its right child is a leaf, and bit 7 if its left child is a leaf
* The children of the node at index `i` are a pair: the left one at `(i & ~1) + offset * 2 + 2`, the right one right after it
* A leaf byte holds its symbol
* As the offset has 6 bits, the children of a node are at most 64 pairs after its own pair

### Stream
* 32-bit little-endian words, each read from its most significant bit
* From the root, a 0 bit goes to the left child and a 1 bit to the right one. Reaching a leaf outputs its symbol and goes back to the root
* The last word is padded with zero bits

### Game's encoder
The archives of the game are rebuilt identically by encoding them as follows:
* **Tree:** one leaf per symbol used, in symbol order, padded to two leaves with unused symbols of weight 1. The two lightest nodes not merged yet are merged until one is left, the first created on ties, the lightest becoming the left child
* **Table size:** with `leaves` symbols, `tree_size` is `((leaves + 1) & ~1) - 1`, the bytes left unused at the end of the table being zero
* **Table layout:** the root is placed at index 1, and each node placed waits for the pair of its children, given out in table order. A waiting node at index `i` can still wait `i / 2 + 64 - next` pairs (its slack), `next` being the next free pair, and the subtree below it needs one pair per node with children (its need). At each step:
  * The subtree of a waiting node fits if its need is at most its slack, and if every other waiting node still gets its pair in time once the subtree is laid out. For this, the waiting nodes are ranked from 1 by slack, then by index, the candidate coming first on ties, and the candidate's need must be at most each other node's slack minus its rank plus 1
  * If subtrees fit, the one whose node has the highest index is laid out at once, breadth first: each of its nodes takes the next free pair, and its children with children are queued left first
  * Otherwise, the waiting node with the least slack takes the next free pair (the one with the largest need on ties, then the lowest index), and its children with children start waiting
