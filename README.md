# CP/M-86 CMD Tools

## Synopsis

Simple tools collected to faciliate the cross development of executables running on CP/M-86

- `cmdinfo` Details the structure of a .CMD file and can extract CODE, DATA and EXTRA segments
- `cmdmod` Creates, modifies or deletes a segment (group) of a .CMD file in place
- `bin2cmd` Converts a .COM/.BIN file (DOS .COM binary) to a .CMD
- `exe2cmd` Converts a .EXE file (DOS .EXE binary) to a .CMD (Small Model)

bin2cmd and exe2cmd originate from John Elliot Code...

## cmdinfo Usage

```
cmdinfo file.cmd [...]
     - displays CMD headers

cmdinfo -e file.cmd
     - extracts code segments  (c<index>-<base>.bin)
     - extracts data segments  (d<index>-<base>.bin)
     - extracts extra segments (e<index>-<base>.bin)
```

## cmdmod Usage

```
cmdmod [-s file.bin] [-t type] [-b basehex] [-n minhex] [-m maxhex] file.cmd index
     - modifies, creates or deletes group <index> (0-7) of file.cmd in place
     - <index> is the HDR(index) shown by cmdinfo
     - view the result with cmdinfo

cmdmod -d file.cmd index
     - deletes the group, later groups move down one index
```

| Option | Description | Default (existing group) | Default (new group) |
|--------|-------------|--------------------------|---------------------|
| `-s file.bin` | Replace the group content, padded to 16 bytes | unchanged | length 0 |
| `-t type` | Type 1-9, or the cmdinfo mnemonics `CODE` `DATA` `EXTRA` `STACK` `AUX1`-`AUX4` `SHARED` (case-insensitive) | unchanged | index+1 |
| `-b basehex` | Base in hex paragraphs, 0 = relocatable | unchanged | 0 |
| `-n minhex` | Min size in hex bytes (`-n 0` resets it to the group length) | unchanged | group length |
| `-m maxhex` | Max size in hex bytes, 0 = no constraint, clamped to 10000h | unchanged | 0 |
| `-d` | Delete the group (cannot be combined with other options) | | |

- A missing file is created (index must be 0), if the options define a valid group
- A new group without `-s` has length 0 and needs `-n`, e.g. `cmdmod -t 4 -n 400 prog.cmd 3` adds a 1K stack group with no content
- The min size is raised to the group length when smaller (warning if given with `-n`); an explicit max smaller than the length or min is an error
- Files with RSX or fixup records are refused, as is deleting the only group
- Warns (`WRN:`) when the resulting file is 8080 model and has groups the loader ignores, see below
- The file is rewritten through a temporary `.$$$` file, groups are padded to 16 bytes and the file to 512 bytes
- `make test-cmdmod` and `make test-bin2cmd` run regression scenarios in `./cmdmod-test` and `./bin2cmd-test`, using `test.bin` (a masm "hello" for CP/M-86); the result is run with `emu2` when available (`EMU=` to override)

## exe2cmd Usage

```
exe2cmd file.exe file.cmd [base=hex]
     - converts a DOS .EXE to a CP/M-86 .CMD
     - generates a code group (type 1) and a data group (type 2),
       both relocatable (base 0), the data group being a fixed 0F0h paragraphs
     - base=hex  hex paragraph value added to the segment fixups applied
                 to the data group (default 60)
```

Current limitations (see TODOs):

- Only accepts CP/M-86 BDOS-style images (32 header paragraphs, first relocation at address 6 or less); other .EXE files are rejected with `ERR: <file> is not a BDOS image`
- Output is padded to a 128-byte record boundary
- Progress and header details are reported on stderr (`INF:` lines)

## CP/M-86 executable format (.CMD)

> Source: this section summarises the CP/M-86 `.CMD` format description at <https://www.seasip.info/Cpm/cmdfile.html>.

The `.CMD` file is the general executable format of CP/M-86 and its derivatives. Besides standalone programs, it is also used for system files (e.g. DOS Plus `DOSPLUS.SYS`) and GSX-86 drivers (CP/M and DOS).

| Extension | Usage |
|-----------|-------|
| `.CMD` | Standalone program (usual file type) |
| `.STM` | Speedstart CP/M (cut-down, embedded CP/M-86); first group is obfuscated |

### Layout

| Part | Location | Description |
|------|----------|-------------|
| Header | Offset 0, 128 bytes | 8 group descriptors, then optional RSX/fixup/flags fields |
| Groups | After the header | Group data, in descriptor order |
| Fixups | Next 128-byte boundary | Sequence of 4-byte fixup records (optional) |
| RSX index | Record given by header offset 7Bh | 16-byte entries (optional) |

The first byte of a CMD file is always 1-9 (1 is by far the most common). A `.CMD` file starting with any other byte is most likely a Windows NT script.

### Header

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 72 bytes | 8 group descriptors | 9 bytes each, see below |
| 7Bh | DW | RSX index record | 0 if no RSX index (not in CP/M-86 1.x) |
| 7Dh | DW | 1st record with fixups | 0 if no fixups (not in CP/M-86 1.x) |
| 7Fh | DB | Flags | See below (not in CP/M-86 1.x) |

### Group descriptor (9 bytes)

| Field | Size | Description |
|-------|------|-------------|
| type | DB | Group type, see below |
| length | DW | Length in paragraphs |
| base | DW | Base in paragraphs; 0 if relocatable (normally nonzero only in OS files: `DOSPLUS.SYS`, `PCPM.SYS`, `CPM.SYS`) |
| minimum size | DW | In paragraphs |
| maximum size | DW | In paragraphs |

| Type | Group |
|------|-------|
| 1 | Code |
| 2 | Data |
| 3 | Extra |
| 4 | Stack |
| 5-8 | Aux1 - Aux4 |
| 9 | "Pure" code (shareable between processes) |

### Flags (offset 7Fh)

| Bit | Meaning |
|-----|---------|
| 4 | File is an RSX, not a CMD file |
| 5 | Allocate the 8087 to the program, only if one is present |
| 6 | Allocate the 8087 to the program, even if absent (imaginary resource) |
| 7 | Do segment fixups |

### Fixup record (4 bytes)

| Field | Size | Description |
|-------|------|-------------|
| `xyH` | DB | x = source group (1-8), y = destination group (1-8) |
| segment offset | DW | Offset to add to the source segment register |
| offset | DB | Offset to add to the source offset (0-15) |

The loader adds the destination group segment address to the word specified. CP/M-86 1.1 does not support fixups; a separate program (`R.CMD` or `RUN.CMD`) loads such files.

### RSX index entry (16 bytes)

| Field | Size | Description |
|-------|------|-------------|
| offset | DW | Offset of RSX from the end of the CMD header, in 128-byte records minus 1. `0000h`: dynamically linked, loaded from disc; `0FFFFh`: end of list |
| name | 8 bytes | RSX name (`'RSXNAME '`); filename on disc if dynamically linked |
| unused | 3 x DW | Unused |

`GENRSX` allows at most 7 RSXs per file (one 128-byte record, leaving room for the `0FFFFh` terminator).

### STM obfuscation

In `.STM` files (SpeedStart CP/M-86), the first group (usually code) has every word XORed with `0xA5B4`: even-numbered bytes with `0xB4`, odd-numbered bytes with `0xA5`.

## Loader notes: 8080 model (CP/M-86 1.1 kernel)

Reference: `bdos.a86` (function 59, load program) and `ccp.a86` of [cpm86-kernel](https://github.com/tsupplis/cpm86-kernel).

A file is **8080 model** when none of its 8 descriptors has type 2 (DATA). Otherwise it is not.

| | 8080 model (no DATA group) | With a DATA group |
|---|---|---|
| CS:IP | code base : `0100h` | code base : `0` |
| DS | code base (the code group starts with the 100h-byte base page) | DATA group base (which starts with the base page) |
| ES | = DS, even if an EXTRA group exists | = DS, or the EXTRA group base if one exists |
| SS:SP | not set, the CCP stack is used | not set, the CCP stack is used |
| Group bases | only the first descriptor gets a base | every relocatable group gets a base, in descriptor order |
| Base page group table | code and data entries both describe the first group; other entries stay 0 | one 6-byte entry per type 1-8 (length-1 and base) |
| Type 9 (shared code) | not converted to code, CS would be 0 | converted to code |
| Memory allocated | sum of all relocatable groups (max, or min if no max) | same |

The loader never sets SS:SP from a STACK group: the group is allocated and its base is recorded in the base page, the program has to load SS:SP itself.

`cmdinfo` prints `MODEL(8080)` for such files, and both `cmdinfo` and `cmdmod` warn (`WRN:`) about:
- groups other than the first one, which are ignored by the loader (STACK, EXTRA, AUX...)
- relocatable groups with content, which would be loaded at segment 0
- no CODE group (e.g. only type 9), so CS would be 0
- a code group shorter than the 100h-byte base page

## TODOs

- Examples with MASM, RASM86, MS LINK, DR LINKEXE
- Provide real DOS makefile
- Regress test the submit script
- Adjust exe2cmd to work with basic exes

## Build Environment

For this cross development environment, please use (https://github.com/tsupplis/cpm86-crossdev). It comes with wrappers for all the tools necesary.

## Test Environment
- CP/M-86 1.1 for IBM PC XT. I recompile cpm.sys from patched sources from (http://www.cpm.z80.de/source.html). The simple way to start however is probably (http://www.cpm.z80.de/download/144cpm86.zip)
  - CP/M-86 1.1, CCP/M-86 3.1 and PCP/M-2.0 can be found on (http://www.cpm.z80.de)
  - DOS Plus 1.2 and Patched kernel can be found on (https://www.seasip.info/Cpm/dosplus.html)
- The most excellent CLI emulator for DOS and CP/M-86 is available at https://github.com/johnsonjh/emu2-cpm86 and delivered as part of the crossdev project.
- The Excellent PCE emulator (http://www.hampa.ch/pce/pce-ibmpc.html)
- A sample directory is added to play around
- mtools 4 and cpmtools 2.20

--

## Companion projects

| Project | Description |
|---------|-------------|
| [cpm86-kernel](https://github.com/tsupplis/cpm86-kernel)     | CP/M-86 1.1 distribution rebuilt from patched and reconstituted sources |
| [ccpm86-y2k](https://github.com/tsupplis/ccpm86-y2k)         | CCP/M-86 3.1 distribution rebuilt from patched and reconstituted sources |
| [cpm86-crossdev](https://github.com/tsupplis/cpm86-crossdev) | Unix CP/M-86 cross development project (compilers, emulation and tools) |
| [cpm86-hacking](https://github.com/tsupplis/cpm86-hacking)   | CP/M-86 miscellaneous tools and PCE emulator helpers |
| [cpm86-cmdtools](https://github.com/tsupplis/cpm86-cmdtools) | CP/M-86 `.cmd` file manipulation tools |
| [cpm86-ports](https://github.com/tsupplis/cpm86-ports)       | CP/M-86 application ports in C and assembler |
| [cpm86-vi](https://github.com/tsupplis/cpm86-vi)             | STevie vi port for CP/M-86 and PC-DOS 1.1 |
| [pcdos11-hacking](https://github.com/tsupplis/pcdos11-hacking) | PC-DOS 1.1 distribution, tools and notes |


