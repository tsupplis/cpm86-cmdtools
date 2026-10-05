CC=cc
CFLAGS=-O -c
STRIP=strip
LDFLAGS=
LD=cc
BINEXT=

TOOLS=cmdinfo$(BINEXT) cmdmod$(BINEXT) bin2cmd$(BINEXT)  exe2cmd$(BINEXT) 

all: $(TOOLS)

bin2cmd$(BINEXT): bin2cmd.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdinfo$(BINEXT): cmdinfo.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdmod$(BINEXT): cmdmod.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

exe2cmd$(BINEXT): exe2cmd.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

.c.o:
	$(CC) $(CFLAGS) $<

clean:
	$(RM) *.o $(TOOLS)
	$(RM) -r cmdmod-test bin2cmd-test

test-cmdinfo: $(TOOLS)
	./cmdinfo t0.cmd
	./cmdinfo t1.cmd
	./cmdinfo t2.cmd
	./cmdinfo t3.cmd

# Exercises cmdmod (create, resize, zero-size group, delete) in ./cmdmod-test
test-cmdmod: cmdmod cmdinfo
	$(RM) -r cmdmod-test && mkdir cmdmod-test
	head -c 300 cmdmod.c > cmdmod-test/a.bin
	head -c 50 cmdmod.c > cmdmod-test/b.bin
	cd cmdmod-test && set -e; \
	../cmdmod -s a.bin t.cmd 0; \
	../cmdmod -s b.bin t.cmd 1; \
	../cmdmod -t 3 -s a.bin t.cmd 2; \
	../cmdmod -t 4 -n 400 t.cmd 3; \
	../cmdmod -s a.bin t.cmd 1; \
	../cmdmod -s b.bin t.cmd 1; \
	../cmdmod -t 9 -b 40 t.cmd 0; \
	../cmdmod -d t.cmd 0; \
	../cmdinfo t.cmd; \
	../cmdinfo -e t.cmd; \
	head -c 50 d0-0000.bin | cmp - b.bin; \
	head -c 300 e1-0000.bin | cmp - a.bin; \
	if ../cmdmod t.cmd 9 2>/dev/null; then exit 1; fi; \
	if ../cmdmod -d t.cmd 7 2>/dev/null; then exit 1; fi

# Exercises bin2cmd (zero page, -n, -m) in ./bin2cmd-test
test-bin2cmd: bin2cmd cmdinfo
	$(RM) -r bin2cmd-test && mkdir bin2cmd-test
	head -c 57 bin2cmd.c > bin2cmd-test/a.bin
	cd bin2cmd-test && set -e; \
	../bin2cmd a.bin a.cmd; \
	../cmdinfo a.cmd | tee a.txt; \
	grep -q 'TYPE(01,CODE).*MIN(0.3k=320).*LEN(320)' a.txt; \
	test `wc -c < a.cmd` -eq 512; \
	../bin2cmd -n a.bin n.cmd; \
	../cmdinfo n.cmd | tee n.txt; \
	grep -q 'TYPE(01,CODE).*MIN(0.1k=64).*LEN(64)' n.txt; \
	test `wc -c < n.cmd` -eq 512; \
	tail -c +129 n.cmd | head -c 57 | cmp - a.bin; \
	../bin2cmd -m 1000 a.bin m.cmd; \
	../cmdinfo m.cmd | tee m.txt; \
	grep -q 'MAX(4.0k=4096)' m.txt; \
	../cmdinfo -e a.cmd; \
	tail -c +257 c0-0000.bin | head -c 57 | cmp - a.bin; \
	if ../bin2cmd a.bin 2>/dev/null; then exit 1; fi
