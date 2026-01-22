# acftool
<a href="https://github.com/SombrAbsol/acftool/actions/workflows/build-linux.yml"><img src="https://github.com/SombrAbsol/acftool/actions/workflows/build-linux.yml/badge.svg" alt="Linux Nightly"></a>
<a href="https://github.com/SombrAbsol/acftool/actions/workflows/build-windows.yml"><img src="https://github.com/SombrAbsol/acftool/actions/workflows/build-windows.yml/badge.svg" alt="Windows Nightly"></a>
<a href="https://opensource.org/license/mit"><img src="https://img.shields.io/badge/license-MIT-blue" alt="License: MIT (Expat)"></a>

ACF extraction and building tool. Based on [a fork of acfdump](https://github.com/SombrAbsol/acfdump), an ACF extraction tool originally created by Barubary in Java and ported here to C.

ACF files are used in *Pokémon Ranger: Guardian Signs* data. This tool allows you to extract the files contained in ACF archives, automatically handling decompression when needed, and to rebuild ACF files. You can [download the latest nightly](#download) or [build the program from source](#building).

For more information on the ACF format, see [the documentation](/doc/acf.md).

Please note that the ACF building process is currently insufficient. File extraction works fine on original and reconstructed ACF files, but reconstructed files do not match their originals.

## Download
|         | Linux | Windows |
| ------- | ----- | ------- |
| Release |       |         |
| Nightly | [Download](https://nightly.link/SombrAbsol/acftool/workflows/build-linux/main/acftool-linux.zip) | [Download](https://nightly.link/SombrAbsol/acftool/workflows/build-windows/main/acftool-windows.zip) |

## Usage
### Dumping the ROM
You can dump your own *Pokémon Ranger: Guardian Signs* ROM from:
* [a console from the Nintendo DS or Nintendo 3DS family](https://dumping.guide/carts/nintendo/ds) (Game Card release)
* [a Wii U](https://wiki.hacks.guide/wiki/Wii_U:VC_Extract) (Virtual Console release)

### Getting the ACF files
1. Download, extract and launch [NDSFactory](https://github.com/Luca1991/NDSFactory/releases/latest)
2. Open the program, load your ROM, then press the `Extract Everything` button and choose where to save your files
3. Once the process is complete, go to the `Fat Tools` tab, fill in the first three fields with the requested files you just extracted (`fat_data.bin`, `fnt.bin` and `fat.bin`), then press the `Extract FAT Data!` button and choose where to save your files
4. Go to your output directory. ACF files are located in the `data` directory

### Running acftool
#### File extraction
* To extract a specific ACF file, run `acftool e <file>.acf`
* To extract every ACF files in a directory, run `acftool e <directory>`

The output files will be located in a directory with the same name as the input ACF file.

#### ACF building
To build an ACF file, run `acftool b <directory>`. Please note that the target directory must contain a `filelist.txt` file listing the files to be compressed and a Boolean compression value (0: do not compress; 1: compress), for example:
```
0000.RTCA 0
0001.NCLR 1
0002.NCGR 1
0003.NCER 1
0004.CANR 1
```

## Building
Dependencies: `gcc`, `make` (optional)
1. Clone this repository by running `git clone https://github.com/SombrAbsol/acftool`, or [download the ZIP archive](https://github.com/SombrAbsol/acftool/archive/refs/heads/main.zip) and extract it
2. Go to the repository directory and build the project. You can either run `gcc -O3 -Wall -Wextra -Werror -o acftool` or `make` if you have it installed

## Credits
* acftool by [SombrAbsol](https://github.com/SombrAbsol)
* acfdump and ACF documentation by [Barubary](https://github.com/Barubary) (the original acfdump is available on [his website](http://www.propl.nl/javaprogs/acfdump.jar))

## License
acftool is free software. You can redistribute it and/or modify it under the [terms of the Expat License](/LICENSE) as published by the Massachusetts Institute of Technology.
