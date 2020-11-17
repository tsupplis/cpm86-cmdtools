# cpm86-cmdtools

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

## Build Env

- Makefile compiles for a reasonable unix (OSX, Linux)
- Makefile.dos compilers for (DOS using Aztec C) (Will need to be adapted, I am using the emu2 emulator)
- Makefile.cpm86 compilers for (CP/M-86 using Aztec C)  (Will need to be adapted, I am using the emu2 emulator)

