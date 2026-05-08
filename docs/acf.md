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
* The first FAT entry is a dummy and should be ignored
* Files may optionally be LZ10-compressed

### Header
```rust
{
  char[4] magic       // "acf\0"
  u32     header_size // 0x20
  u32     data_start  // absolute offset to the start of file data
  u32     num_files   // number of files, including the dummy entry
  u32     unknown1    // 0x01
  u32     unknown2    // 0x32
  u32[2]  padding     // zero padding
}
```

Immediately followed by `num_files` FAT entries, beginning at byte `0x20`.

### FAT entries
```rust
{
  u32 relative_offset // offset relative to data_start; if 0xFFFFFFFF, skip this entry
  u32 output_size     // size of the output (decompressed) data; pad the output with zeros to reach this size
  u32 input_size      // size of the compressed input data if LZ10-compressed; 0 if not compressed
}
```

### Compression
If `input_size` is non-zero, the file data is LZ10-compressed. In that case:
* Read `input_size` bytes from the archive starting at `data_start + relative_offset`
* Decompress the LZ10 data to obtain the file contents
* If the decompressed size is less than `output_size`, zero-pad the output up to `output_size`

If `input_size` is zero, the file data is stored uncompressed. Read `output_size` bytes directly from `data_start + relative_offset`.
