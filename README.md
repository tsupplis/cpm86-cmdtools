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
- The Super Excellent command line emulator for DOS/CP/M-86 
- The Excellent PCE emulator (http://www.hampa.ch/pce/pce-ibmpc.html)
- A sample directory is added to play around
- mtools 4 and cpmtools 2.20



