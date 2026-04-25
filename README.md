<!--
SPDX-FileCopyrightText: 2026 SombrAbsol

SPDX-License-Identifier: MIT
-->

# acftool
<a href="https://github.com/SombrAbsol/acftool/actions/workflows/build-latest.yml"><img src="https://github.com/SombrAbsol/acftool/actions/workflows/build-latest.yml/badge.svg" alt="Latest"></a>
<a href="https://opensource.org/license/mit"><img src="https://img.shields.io/badge/license-MIT-blue" alt="License: MIT (Expat)"></a>

ACF archive utility for *Pokémon Ranger: Guardian Signs*. Based on [a fork of acfdump](https://github.com/SombrAbsol/acfdump), an ACF extraction tool originally written by Barubary in Java and ported here to C.

ACF archives are used in *Pokémon Ranger: Guardian Signs* data. This tool allows you to extract the files contained in ACF archives, automatically handling decompression when needed, and to rebuild these archives. You can [download the latest nightly](#download) or [build the program from source](#building).

For more information on the ACF format, see [the documentation](/docs/acf.md).

## Download
|        | Linux | macOS | Windows |
| ------ | ----- | ----- | ------- |
| Latest | [Download](https://nightly.link/SombrAbsol/acftool/workflows/build-latest/main/acftool-linux.zip) | [Download](https://nightly.link/SombrAbsol/acftool/workflows/build-latest/main/acftool-macos.zip) | [Download](https://nightly.link/SombrAbsol/acftool/workflows/build-latest/main/acftool-windows.zip) |

## Usage
### Dumping the ROM
You can dump your own *Pokémon Ranger: Guardian Signs* ROM from:
* [a console from the Nintendo DS or Nintendo 3DS family](https://dumping.guide/carts/nintendo/ds) (Game Card release)
* [a Wii U](https://wiki.hacks.guide/wiki/Wii_U:VC_Extract) (Virtual Console release)

### Getting the ACF archives
To get the ACF archives, Windows users can run [TinkeDSi](https://github.com/R-YaTian/TinkeDSi), but [NDSFactory](https://github.com/Luca1991/NDSFactory) can also be run on macOS and Linux:
1. Download [the latest NDSFactory release](https://github.com/Luca1991/NDSFactory/releases/latest), then extract the archive and run the executable
2. Open the program, load your ROM, then scroll down until you see the `FAT Files Address` field. Take note or copy its value
3. Press the `Extract Everything` button and choose where to save your files
4. Once the process is complete, go to the `Fat Tools` tab, and fill in the first three fields with the requested files you just extracted (`fat_data.bin`, `fnt.bin` and `fat.bin`) and the fourth with the value from the previous `FAT Files Address` field
5. Press the `Extract FAT Data!` button and choose where to save your files
6. Go to your output directory. ACF archives are located in the `data` directory

### Running acftool
> [!IMPORTANT]
> acftool is a command-line program and must be run in a terminal.

#### ACF Extraction
* To extract files from an ACF archive, run `acftool -x <in.acf>` or `acftool --extract <in.acf>`
* To extract files from every ACF archives in a directory, run `acftool -x <indir>` or `acftool --extract <indir>`

The output files will be located in a directory with the same name as the input ACF archive.

#### ACF Building
To build an ACF archive, run `acftool -b <indir>` or `acftool --build <indir>`. Please note that the target directory must contain a `filelist.json` file listing the files and their state (null: set file entry as unused; false: do not compress; true: compress), for example:
```json
{
  "0000.RTCA": false,
  "0001.NCLR": true,
  "0002.NCGR": true,
  "0003.NCER": true,
  "0004.CANR": true
}
```

## Building
Dependencies: `clang` or `gcc`, and `make`
1. If you don't already have them, install the dependencies
2. Clone this repository by running `git clone https://github.com/SombrAbsol/acftool`, or [download the ZIP archive](https://github.com/SombrAbsol/acftool/archive/refs/heads/main.zip) and extract it
3. Go to the repository directory and build the executable by running `make`

Operating systems that use the Unix file system (such as Linux and macOS) can then run `sudo make install` to install acftool system-wide. `sudo make uninstall` removes it.

## TODO
* Add ACZ support
* Better ACF and ACZ documentation

## Credits
* acftool by [SombrAbsol](https://github.com/SombrAbsol)
* acfdump and ACF documentation by [Barubary](https://github.com/Barubary) (the original acfdump is available on [his website](http://www.propl.nl/javaprogs/acfdump.jar))

## License
acftool is free software. You can redistribute it and/or modify it under the [terms of the Expat License](/LICENSE) as published by the Massachusetts Institute of Technology.
