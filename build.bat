cc -D__MSDOS__ cmdinfo.c
sqz cmdinfo.o
ln -o cmdinfo.com cmdinfo.o -lm -lc
cc -D__MSDOS__ cmdmod.c
sqz cmdmod.o
ln -o cmdmod.com cmdmod.o -lm -lc
cc -D__MSDOS__ bin2cmd.c
sqz bin2cmd.o
ln -o bin2cmd.com bin2cmd.o -lm -lc
cc -D__MSDOS__ exe2cmd.c
sqz exe2cmd.o
ln -o exe2cmd.com exe2cmd.o -lm -lc
cc -D__MSDOS__ h862bin.c
sqz h862bin.o
ln -o h862bin.com h862bin.o -lm -lc
cc -D__MSDOS__ h862cmd.c
sqz h862cmd.o
ln -o h862cmd.com h862cmd.o -lm -lc
cc -D__MSDOS__ bin2asm.c
sqz bin2asm.o
cc -D__MSDOS__ b2aconf.c
sqz b2aconf.o
ln -o bin2asm.com bin2asm.o b2aconf.o -lm -lc
