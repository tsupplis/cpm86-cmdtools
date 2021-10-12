cc -D__MSDOS__ cmdinfo.c
sqz cmdinfo.o
ln -o cmdinfo.com cmdinfo.o -lm -lc
cc -D__MSDOS__ bin2cmd.c
sqz bin2cmd.o
ln -o bin2cmd.com bin2cmd.o -lm -lc
cc -D__MSDOS__ exe2cmd.c
sqz exe2cmd.o
ln -o exe2cmd.com exe2cmd.o -lm -lc
