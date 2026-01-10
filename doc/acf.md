Approximate documentation of the ACF file format. Written by [Barubary on GBATemp](https://gbatemp.net/threads/acf-files.211897/post-2638536).
```
{
    char[4] magicHeader; // 'acf'
    u32 headerSize; // should be 0x20
    u32 dataStart; // start of file data
    u32 numFiles; // amount of files inside
    u32 unknown1; // I've only seen 1 here
    u32 unknown2; // I've only seen 0x32 here
    u32[2] padding; // at least, I think it is
    FATENTRY[numFiles]; // first entry is dummy
} ACFHEADER

{
    u32 relativeOffset; // offset relative to dataStart (if 0xFFFFFFFF, ignore it)
    u32 outputSize; // Size of (decompressed) file. Might be a bit more then decompressed size in compression header; pad it with 0s
    u32 inputSize; // if file is LZSS-compressed, size of compressed data. 0 if not compressed.
} FATENTRY
```
