#pragma author SombrAbsol
#pragma description ACF Archive (Pokémon Ranger: Guardian Signs)
#pragma magic [ 61 63 66 00 ] @ 0x00
#pragma loop_limit 10000

import std.io;
import type.magic;

enum ACFConst : u32 {
    ExpectedHeaderSize = 0x20
};

struct FatEntry {
    u32 relativeOffset [[color("00FFFF")]]; // relative to dataStart
    u32 outputSize     [[color("00FF00")]]; // decompressed size
    u32 inputSize      [[color("FFAA00")]]; // compressed size (0 = raw)
};

struct ACFHeader {
    type::Magic<"acf\0"> signature [[color("FF0000")]];
    u32 headerSize      [[color("00FF00")]];
    u32 dataStart       [[color("00FFFF")]];
    u32 numFiles        [[color("FFFF00")]];
    u32 unknown1        [[color("C0C0C0")]];
    u32 unknown2        [[color("C0C0C0")]];
    u32 padding_[2];

    FatEntry fat[numFiles];
};

ACFHeader acf @ 0x00;

std::print("Files: {}", acf.numFiles);
std::print("Data start: 0x{:08X}", acf.dataStart);

// header validation
if (acf.headerSize != ACFConst::ExpectedHeaderSize) {
    std::print("Unexpected header size: 0x{:X}", acf.headerSize);
}

for (u32 i = 0, i < acf.numFiles, i = i + 1) {

    if (acf.fat[i].relativeOffset == 0xFFFFFFFF) {
        std::print("FAT[{}] UNUSED", i);
        continue;
    }

    u32 absoluteOffset = acf.dataStart + acf.fat[i].relativeOffset;
    bool isCompressed  = (acf.fat[i].inputSize != 0);

    std::print(
        "FAT[{}] rel=0x{:08X} abs=0x{:08X} out={} in={} {}",
        i,
        acf.fat[i].relativeOffset,
        absoluteOffset,
        acf.fat[i].outputSize,
        acf.fat[i].inputSize,
        isCompressed ? "[LZ10]" : "[RAW]"
    );
}
