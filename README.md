<!--
SPDX-FileCopyrightText: 2026 SombrAbsol

SPDX-License-Identifier: MIT
-->

# acftool
<a href="https://github.com/SombrAbsol/acftool/actions/workflows/ci.yml"><img src="https://github.com/SombrAbsol/acftool/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
<a href="https://opensource.org/license/mit"><img src="https://img.shields.io/badge/license-MIT-blue" alt="License: MIT (Expat)"></a>

ACF archive utility for *Pokémon Ranger: Guardian Signs*. Based on [a fork of acfdump](https://github.com/SombrAbsol/acfdump), an ACF extraction tool originally written by Barubary in Java and ported here to C.

ACF archives are used in *Pokémon Ranger: Guardian Signs* data. This tool allows you to extract the files contained in ACF archives, automatically handling decompression when needed, and to rebuild these archives, including the Huffman-compressed ones (ACZ). You can [download the latest build](#download) or [build the program from source](#building).

For more information about the ACF format, see [the documentation](/docs/acf.md).

## Download
|        | Linux | macOS | Windows |
| ------ | ----- | ----- | ------- |
| Latest | [Download](https://github.com/SombrAbsol/acftool/releases/download/latest/acftool-linux.zip) | [Download](https://github.com/SombrAbsol/acftool/releases/download/latest/acftool-macos.zip) | [Download](https://github.com/SombrAbsol/acftool/releases/download/latest/acftool-windows.zip) |

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
6. Go to your output directory. ACF archives are located in the `data` directory, and the map archives in its `map` (Japanese release) or `mapz` (other releases) directory

### Running acftool
> [!IMPORTANT]
> acftool is a command-line program and must be run in a terminal.

#### Extracting archives
* To extract the files of an ACF archive, run `acftool <in.acf>`, or `acftool <in.acz>` for a Huffman-compressed one
* To extract the files of every ACF archive in a directory, run `acftool <indir>`
* To handle several archives or directories at once, list them, for instance `acftool a.acf b.acf` or `acftool *.acf`. Wildcards such as `*` and `?` also work on Windows

The files of an archive are extracted, along with a `filelist.json` file listing them, to a directory with the same name as the archive, without its extension.

#### Building archives
To build an ACF archive, run `acftool <indir>`, where `<indir>` contains a `filelist.json` file listing the files and their state (null: set file entry as unused; false: do not compress; true: compress), for example:
```json
{
  "0000.RTCA": false,
  "0001.NCLR": true,
  "0002.NCGR": true,
  "0003.NCER": true,
  "0004.CANR": true
}
```

The directory is built into an archive with the same name, followed by the `.acf` extension. A directory holding a `filelist.json` file is built by default. To extract the ACF archives it contains instead, run `acftool -x <indir>` or `acftool --extract <indir>`.

For [map archives](/docs/acf.md#map-archives), `filelist.json` starts with a `"signature"` and a `"huffman"` key, which must be kept: a directory extracted from an `.acz` file is built back into one, unless `"huffman"` is set to `false`.

* To build a Huffman-compressed archive from any directory, run `acftool -z <indir>` or `acftool --huffman <indir>`
* To compress an existing archive, run `acftool -z <in.acf>`

#### Outputs
By default, the outputs are written next to their input. However, you can optionally specify the output directory or archive of a single input with `-o` or `--output`, or a directory to write the outputs in with `--output-dir`, for instance:
* `acftool in.acf -o outdir` will extract the files to `outdir`
* `acftool indir -o out.acf` will build the archive `out.acf`
* `acftool indir --output-dir=outdir`, `indir` holding ACF archives, will extract them to directories in `outdir`, created if missing
* `acftool *.acf --output-dir=outdir` will extract each archive to a directory in `outdir`, created if missing

If an output already exists, you will be asked whether to replace it. Run `acftool -y <in>` or `acftool --yes <in>` (with `<in>` being an input archive or directory) to replace existing outputs without asking.

Run `acftool -h` or `acftool --help` to list all the options.

## Building
### Dependencies
* `clang` or `gcc`
* `make`

### Steps
1. If you don't already have them, install the dependencies
2. Clone this repository by running `git clone https://github.com/SombrAbsol/acftool`, or [download the ZIP archive](https://github.com/SombrAbsol/acftool/archive/refs/heads/main.zip) and extract it
3. Go to the repository directory and build the program by running `make`

> [!TIP]
> Running `make` or `make release` will generate a release build. The downloadable releases are static builds generated using this recipe. If you want to generate a native or a debug build, run `make native` or `make debug`. Native builds are optimized for your specific CPU for better performance but may not be compatible with other systems, while debug builds include debugging symbols that help diagnose issues but run slower.
>
> Unix-like operating systems (such as Linux and macOS) can run `sudo make install` to install acftool system-wide. It installs the program already built, preferably a release or native build, or generates a release build first if there is none. Use `sudo make uninstall` to remove it.
>
> If you need to rebuild the program, run `make clean` or delete the `build` directory.

## TODO
* The extraction process now supports ACF/ACZ map archives: what to do with the extracted files to ensure full support?

## Credits
* acftool by [SombrAbsol](https://github.com/SombrAbsol)
* acfdump and ACF documentation by [Barubary](https://github.com/Barubary) (the original acfdump is available on [his website](http://www.propl.nl/javaprogs/acfdump.jar))

## License
acftool is free software. You can redistribute it and/or modify it under the [terms of the Expat License](/LICENSE) as published by the Massachusetts Institute of Technology.
