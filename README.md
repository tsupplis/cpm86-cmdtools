# CP/M-86 CMD Tools

## Synopsis

Simple tools collected to faciliate the cross development of executables running on CP/M-86

- cmdinfo Details the structure of a .CMD file
- bin2cmd Converts a .COM/.BIN file (DOS .COM binary) to a .CMD
- exe2cmd Converts a .EXE file (DOS .EXE binary) to a .CMD (Small Model)

bin2cmd and exe2cmd originate from John Elliot Code...

## TODOs

- Examples with MASM, RASM86, MS LINK, DR LINKEXE
- Provide real DOS makefile
- Regress test the submit script
- Adjust exe2cmd to work with basic exes

## Build Environment
- Makefile compiles for a reasonable unix (linux/mac)
- Makefile.dos for DOS Aztec C compilers.  (May require adaptation. emu2 is used to run dos compiler on unix/mac)
- Makefile.cpm86 for DOS Aztec C cross compilers targetting CP/M-86.  (May require adaptation. emu2 is used to run dos compiler on unix/mac)
- aztec c compiler version 3.4
- rasm86/linkcmd, DOS version from Digital Research (http://www.cpm.z80.de/binary.html)

```
LINK86 Linkage Editor  02/Feb/87      Version 2.02
Cross Linker: DOS -> CPM-86 & CDOS-286      1/2/86
```
```
RASM-86 Assembler   12-Mar-87  PC-DOS Version 1.4a
```
- The Super Cool emu2 DOS emulator to run the DR tools on macOS and Linux (https://github.com/dmsc/emu2). This is an incredible way to bring dos command line development tools to a modern and up to date shell/make/whatever based dev environment. Another stunning emulator. Emu2 and PCE are an incredible pair.

A rudimentary development environment can be assembled by using the following steps:
```
(cd devtools;./fetch)
export PATH=`pwd`/devtools/bin
# now you use make
make clean all
```
it pulls the following:
- aztec 3.4 c compiler  (https://www.aztecmuseum.ca/az8634b.zip)
- linkcmd and rasm86 (http://www.cpm.z80.de/download/tools86.zip)
- emu2 (https://github.com/dmsc/emu2)

## Test Environment
- CP/M-86 1.1 for IBM PC XT. I recompile cpm.sys from patched sources from (http://www.cpm.z80.de/source.html). The simple way to start however is probably (http://www.cpm.z80.de/download/144cpm86.zip)
  - CP/M-86 1.1, CCP/M-86 3.1 and PCP/M-2.0 can be found on (http://www.cpm.z80.de)
  - DOS Plus 1.2 and Patched kernel can be found on (https://www.seasip.info/Cpm/dosplus.html)
- The Excellent PCE emulator (http://www.hampa.ch/pce/pce-ibmpc.html)
- A sample directoy is added to play around
- mtools 4 and cpmtools 2.20



