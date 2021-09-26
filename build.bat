@echo off
cc cmdinfo.c
sqz cmdinfo.o
ln -o cmdinfo.cmd cmdinfo.o c:m.lib -lc
cc bin2cmd.c
sqz bin2cmd.o
ln -o bin2cmd.cmd bin2cmd.o -lm -lc
cc exe2cmd.c
sqz exe2cmd.o
ln -o exe2cmd.cmd exe2cmd.o c:m.lib -lc
del cmdinfo.o
del bin2cmd.o
del exe2cmd.o
