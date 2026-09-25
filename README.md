# CP/M-86 CMD Tools

## Synopsis

Simple tools collected to faciliate the cross development of executables running on CP/M-86

- `cmdinfo` Details the structure of a .CMD file and can extract CODE, DATA and EXTRA segments
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


