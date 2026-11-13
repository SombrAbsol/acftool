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
  char[4] magic       // "acf\0"
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
