# CP/M-86 CMD Tools

## Synopsis

Simple tools collected to faciliate the cross development of executables running on CP/M-86

- `cmdinfo` Details the structure of a .CMD file and can extract CODE, DATA and EXTRA segments
- `cmdmod` Creates, modifies or deletes a segment (group) of a .CMD file in place
- `bin2cmd` Converts a .COM/.BIN file (DOS .COM binary) to a .CMD
- `h862bin` Converts Intel HEX / ASM-86 H86 to a flat binary image
- `bin2asm` Converts binary bytes to NASM or MASM/ASM86 data declarations
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

For a file without a DATA group, `cmdinfo` prints `MODEL(8080)` and warns (`WRN:`) about what the loader would ignore, see the loader notes below.

## bin2cmd Usage

```
bin2cmd [-n] [-m maxhex] file.bin file.cmd
     - converts a .COM/.BIN to a CP/M-86 .CMD file with one relocatable code group
     -n         do not insert the 256-byte zero page (base page) after the header,
                the input then provides it
     -m maxhex  max segment size in hex bytes (0 = any, clamped to 10000h = 64K),
                ignored if <= the code size
```

- By default the code group is preceded by a 256-byte zero page, so a program assembled with `org 100h` runs as 8080 model
- The group min size is the code group length (including the zero page unless `-n`), and the file is padded to 512 bytes
- To leave room after the image (for a stack, for example) use `-m`, or `cmdmod -n` on the result to raise the min size

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

## h862bin Usage

```
h862bin file.h86 file.bin
     - converts Intel HEX / ASM-86 H86 to a contiguous binary image
     - strips the origin; adds no base page, header or padding
```

| Input | Handling |
| --- | --- |
| Intel HEX data (`00`) | Included in the image |
| ASM-86 segment data (`81`–`84`) | Included at their declared offsets |
| Start address (`03`) | Validated, then ignored |
| End of file (`01`) | Stops reading; CP/M padding afterwards is ignored |
| Bad checksum/length, unsupported record, overlap, hole, missing EOF | Rejected with `ERR:` and a nonzero exit status |

Addresses must fit the 16-bit range. Records may be out of order, but must
form one contiguous image with no overlapping code/data offsets. This is a
flat-image converter, not a linker. Input is validated before output is written;
a temporary `.$$$` file replaces the destination on success. Input and output
must have different names. `INF:` progress and `ERR:` diagnostics go to stderr;
stdout is unused. On CP/M, the filesystem pads output to 128-byte records.

## bin2asm Usage

```
bin2asm [-m] [-p] [-s skiphex] [-o originhex] [-l lengthhex]
        [-f file.asm] [file.bin [file.asm]]
```

| Option | Description |
| --- | --- |
| `-m` | MASM/ASM86 literals (`0ffh`); default is NASM (`0xff`) |
| `-p` | Emit `label_XXXX:` before each row of up to eight bytes |
| `-s skiphex` | Skip input bytes; default 0 |
| `-o originhex` | Add an origin to label addresses; default 0 |
| `-l lengthhex` | Convert at most this many bytes; default through EOF |
| `-f file.asm` | Write to this file instead of stdout; a second filename is equivalent |
| `-h` | Display help |

Numbers are hexadecimal, as in `cmdmod`; `0x` prefixes and `h` suffixes are
also accepted. Options accept uppercase and lowercase. Omit the input or use
`-` for stdin; `--` ends option parsing. No arguments display usage and fail.
`-o` changes label addresses, without inserting an ORG directive or padding.
Labels address `origin + skip + byte offset`. Input is read in binary mode;
ASCII comments accompany each row. Diagnostics use `INF:` / `ERR:` on stderr,
while stdout contains only assembly text. On CP/M, input includes record
padding; use `-l` for a known exact length.

Examples:

```
bin2asm -m -f bootsect.inc bootsect.bin
bin2asm -p -s 100 -o 1000 -l 20 image.bin fragment.asm
bin2asm -m - < image.bin > image.inc
```

## exe2cmd Usage

```
exe2cmd file.exe file.cmd [base=hex]
     - converts a DOS .EXE to a CP/M-86 .CMD
     - generates a code group (type 1) and a data group (type 2),
       both relocatable (base 0), the data group being a fixed 0F0h paragraphs
     - base=hex  hex paragraph value added to the segment fixups applied
                 to the data group (default 60)
```

### exe2cmd Limitations

`exe2cmd` is purposely narrow: it was written to convert one specific binary (the Personal CP/M-86 BDOS compiled as a small-model EXE) and has never been generalised.

**Input constraints**

| Constraint | Detail |
|---|---|
| BDOS fingerprint required | The EXE header must have exactly **32 header paragraphs** (`peek(8) == 32`) and the first relocation entry must sit at or before offset 6 (`peek(32) <= 6`). Every other EXE is rejected with `ERR: <file> is not a BDOS image`. |
| EXE header buffer is 520 bytes | Only the first 520 bytes (`0x208`) of the EXE are loaded into `exe_header[]`. Relocation tables beyond that window are silently ignored. |
| At most 8 relocation entries processed | The fixup loop (line ~204) iterates exactly 8 times, patching segment words at hard-coded positions inside the data group. It does **not** walk the EXE relocation table; all other relocations are left un-patched. Slot 6 is skipped unconditionally (reserved for the DOS module address). |

**Output constraints**

| Constraint | Detail |
|---|---|
| Fixed data segment size | `DS_LEN` is hard-coded to `0xF0` paragraphs (3840 bytes). The EXE's own `minalloc`/`maxalloc` fields are ignored; the commented-out general logic was never enabled. |
| Two groups only | Exactly one Code group (type 1) and one Data group (type 2) are emitted. No Stack, Extra or Aux groups are generated regardless of what the EXE image contains. |
| Hard-coded `base=` default | The paragraph offset added to every intra-DS fixup defaults to `0x60`, which is the CP/M-86 BDOS load address. Passing a different `base=` value is necessary for any other target. |
| Padded to 128-byte record boundary | The output `.CMD` is zero-padded to the next 128-byte boundary, not to a full 512-byte sector. |
| No write-error reporting in `mvbytes` | Errors during the byte-copy phase are silently dropped (noted as `TODO` in the source). |

**Diagnostic output**

All progress (`INF:`) and error (`ERR:`) messages go to **stderr**. The tool produces no output on stdout.

**Summary**: `exe2cmd` cannot be used as a general-purpose EXE-to-CMD converter. Adapting it for arbitrary small-model EXEs would require at minimum: removing the BDOS fingerprint check, replacing the hard-coded data-segment size with values derived from the EXE header, and replacing the fixed 8-entry fixup loop with a proper walk of the EXE relocation table.

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

The maximum memory size, is generally used when additional free memory may be needed for such purposes as I/O 
buffers or symbol tables.  If the data area size is fixed, then the maximum memory need not be included. 
In this case, the maximum memory is assumed to be the same as the minimum memory. The value FFFF allocates the
largest memory region available but, if used, the transient program must be aware that a three-byte length field is
produced in the base page for this group where the high order byte may be non-zero. Programs converted directly
from CP/M-80 or programs that use a 2-byte pointer to address buffers should restrict this value to FFF or less,  
producing a maximum allocation length of OFFFOH bytes.

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

The 8080 keyword is included whenever ASM-86 is used in the conversion of 8080 programs to the 8086/8088
environment when code and data are intermixed within a single 64K segment, regardless of the use of CSEG and
DSEG directives in the source program.

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

more info on segment programing at (Segment Patterns)[https://github.com/tsupplis/cpm86-crossdev/blob/main/docs/asm/asm-tutorial.md#3-segment-patterns].
The loader never sets SS:SP from a STACK group: the group is allocated and its base is recorded in the base page, the program has to load SS:SP itself.

`cmdinfo` prints `MODEL(8080)` for such files, and both `cmdinfo` and `cmdmod` warn (`WRN:`) about:
- groups other than the first one, which are ignored by the loader (STACK, EXTRA, AUX...)
- relocatable groups with content, which would be loaded at segment 0
- no CODE group (e.g. only type 9), so CS would be 0
- a code group shorter than the 100h-byte base page
- (`cmdmod` only) an 8080-model code group with no room beyond the image (min = length, no max): the stack must then be inside the image, or the min raised with `cmdmod -n`

## Tests

Native (build with `make`), each test works in its own directory (removed by `make clean`):

| Target | Directory | Checks |
|--------|-----------|--------|
| `make test-cmdinfo` | `cmdinfo-test` | group types, base/min/max, 8080 model warnings, extraction, errors |
| `make test-cmdmod` | `cmdmod-test` | create, resize, zero-size group, delete, round trip of `test.bin` |
| `make test-h862bin` | `h862bin-test` | Intel/ASM-86 records, out-of-order data, 64K range, padding and validation failures |
| `make test-bin2asm` | temporary directory | binary ranges, 64K input, labels, errors, stdin and NASM round trips |
| `make test-bin2cmd` | `bin2cmd-test` | zero page, `-n`, `-m`, `test.bin` |

- `test.bin` is a masm "hello" for CP/M-86. When `emu2` is available (`EMU=` to override) the converted files are run and their output is checked
- H86 conversion under either emulator target: `make -f Makefile.cpm86 test-h862bin-emu2` or `make -f Makefile.dos test-h862bin-emu2`.
- BIN2ASM under either emulator target: `make -f Makefile.cpm86 test-bin2asm-emu2` or `make -f Makefile.dos test-bin2asm-emu2`.
- Cross-built tools (Aztec C) are tested with `make -f Makefile.cpm86 test-emu2` and `make -f Makefile.dos test`: they run under `emu2` (which needs a terminal), and their output is compared with the native tools in `emu-test`

## CP/M and DOS builds

- CP/M has no exact file length, files are padded to whole 128-byte records: `bin2cmd` and `cmdmod -s` size a binary input by reading it, so its length is rounded up to 128 bytes
- CP/M upper-cases the command line: options are accepted in lower or upper case (`-M 1000`)
- Aztec C `printf` rounds sizes in `cmdinfo` such as `0.25K` up, the native one to even

## Distribution

`make -f Makefile.cpm86 dist` creates `cpm86-cmdtools.zip` containing all six
CMD tools, including `bin2asm.cmd`. `make -f Makefile.dos dist` creates
`dos-cmdtools.zip` with the six COM tools, including `bin2asm.com`.
Archives are rebuilt only when their tools or packaging Makefile change.
Use the matching Makefile's `clean` before switching between the two Aztec
compilers: their intermediate `.o` files are incompatible. Native builds
compile directly from source and do not reuse those objects.

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
| [ccpm86-kernel](https://github.com/tsupplis/ccpm86-kernel)         | CCP/M-86 3.1 distribution rebuilt from patched and reconstituted sources |
| [cdos41-kernel](https://github.com/tsupplis/cdos41-kernel)         | Concurrent DOS 4.1 distribution rebuilt from patched and reconstituted sources |
| [cpm86-crossdev](https://github.com/tsupplis/cpm86-crossdev) | Unix CP/M-86 cross development project (compilers, emulation and tools) |
| [cpm86-hacking](https://github.com/tsupplis/cpm86-hacking)   | CP/M-86 miscellaneous tools and PCE emulator helpers |
| [cpm86-cmdtools](https://github.com/tsupplis/cpm86-cmdtools) | CP/M-86 `.cmd` file manipulation tools |
| [cpm86-ports](https://github.com/tsupplis/cpm86-ports)       | CP/M-86 application ports in C and assembler |
| [cpm86-vi](https://github.com/tsupplis/cpm86-vi)             | STevie vi port for CP/M-86 and PC-DOS |
| [cpm86-msbasic](https://github.com/tsupplis/cpm86-msbasic)   | A recreaction of msbasic-86 for CP/M-86 and PC-DOS 1.1 from gwbasic sources |
| [cpm86-menu](https://github.com/tsupplis/cpm86-menu)   | A submit replacement and a menu system for submit orchestration for CP/M-86 1.1 |
| [cpm86-make](https://github.com/tsupplis/cpm86-make)   | A simple submit based make tool for CP/M-86 |
| [pcdos11-hacking](https://github.com/tsupplis/pcdos11-hacking) | PC-DOS 1.1 distribution, tools and notes |

--

## License

The whole repository is under LGPL 2.1 [License](LICENSE.md).
