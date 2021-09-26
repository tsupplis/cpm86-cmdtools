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
- Adjust exe2cmd to work with basic exes

## Build Environment
- Makefile compiles for a reasonable unix (linux/mac)
- Makefile.dos for DOS Aztec C compilers.  (May require adaptation. emu2 is used to run dos compiler on unix/mac)
- Makefile.cpm86 for DOS Aztec C cross compilers targetting CP/M-86.  (May require adaptation. emu2 is used to run dos compiler on unix/mac)

